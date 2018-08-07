/**
 * @file	inotify.cc
 * @brief	디렉터리 감시 
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 08. 08. 10:11:12
 * @see		
 */
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <ctime>
#include <regex>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <sys/inotify.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "inotify.hpp"

#define BUF_LEN (10 * (sizeof(struct inotify_event) + FILENAME_MAX + 1))

using namespace itfact::vr::node;

static log4cpp::Category *logger = NULL;
static std::string token_header;
static std::mutex job_lock;
static std::mutex req_lock;
static std::atomic<int> working_job(0);
static std::atomic<int> available_workers(10);
static std::atomic<int> running_workers(0);
static std::atomic<int> list_count(0);

static struct {
	std::string rec_ext;
	std::string index_type;
	std::string watch;
	std::string api_service;
	std::string apiserver_url;
	std::string apiserver_version;
	std::string api_key;
	bool delete_on_success;
	bool daily_output;
	bool unique_output;
} default_config = {
	.rec_ext = "wav",
	.index_type = "filename",
	.watch = "pcm",
	.api_service = "vr",
	.apiserver_url = "https://localhost:3000",
	.apiserver_version = "v1.0",
	.api_key = "vr_server",
	.delete_on_success = false,
	.daily_output = false,
	.unique_output = false,
};

/**
 */
int main(const int argc, char const *argv[]) {
	try {
		VRInotify server(argc, argv);
		server.monitoring();
	} catch (std::exception &e) {
		perror(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

VRInotify::VRInotify(const int argc, const char *argv[]) : config(argc, argv) {
	CURLcode rc = curl_global_init(CURL_GLOBAL_ALL);
	if (rc)
		throw std::runtime_error(curl_easy_strerror(rc));

	handle_count = 0;
	multi_handle = curl_multi_init();
	if (multi_handle == NULL)
		throw std::runtime_error("Cannot allocation CURL-multi");

	logger = config.getLogger();
}

VRInotify::~VRInotify() {
	std::vector<std::shared_ptr<CURL>> curl_handles;
	while (curl_handles.size() > 0) {
		curl_multi_remove_handle(multi_handle, curl_handles.back().get());
		curl_handles.pop_back();
	}

	curl_multi_cleanup(multi_handle);
	curl_global_cleanup();
}

/**
 * @brief		작업 종료 큐에 등록 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 11. 04. 13:34:38
 * @param[in]	threadId	ThreadId
 */
static inline void finishJob(std::thread::id threadId, std::map<std::thread::id,std::string> *list) {
	std::lock_guard<std::mutex> guard(job_lock);
	working_job.fetch_sub(1);
	list_count.fetch_sub(1);
	if (list)
		list->erase(std::this_thread::get_id());
}

static inline void addListCount(int cnt) {
	std::lock_guard<std::mutex> guard(job_lock);
	list_count.fetch_add(cnt);
}
#if 0
static inline void subListCount() {
	std::lock_guard<std::mutex> guard(list_lock);
	list_count.fetch_sub(1);
}
#endif

/**
 * @brief		데이터 다운로드 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 28. 22:59:42
 * @param[in/out]	Name	Param description
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
static size_t getData(void *source , size_t size , size_t nmemb , void *userData) {
	const int total_size = size * nmemb;
	const int length = total_size / sizeof(char);
	char *data = (char *) source;
	std::string *buffer = (std::string *) userData;

	for (int i = 0; i < length; ++i)
		buffer->push_back(data[i]);

	return total_size;
}

/**
 * @brief		사용 가능한 워커 총 수 확인 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 08. 08. 19:08:14
 */
static void getTotalWorkers() {
	char buffer[128];
	bool isLoaded = false;
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(isLoaded ? 300 : 15));
		std::memset(buffer, '\0', 128);
		std::string worker_info = "";
		FILE *_pipe = popen("/usr/local/bin/gearadmin --status | /bin/grep vr_stt", "r");
		if (!_pipe)
			continue;
		std::shared_ptr<FILE> pipe(_pipe, pclose);

		while (!std::feof(pipe.get())) {
			if (std::fgets(buffer, 128, pipe.get()) != NULL)
				worker_info += buffer;
		}

		std::vector<std::string> v;
		boost::split(v, worker_info, boost::is_any_of("\t "), boost::token_compress_on);
		if (v.size() < 2)
			continue;

		try {
			int total_workers = std::stoi(v[3]);
			// 동작 중인 worker 수
			int current_running_workers = std::stoi(v[2]);

			if (total_workers > 0) {
				running_workers = current_running_workers;
				available_workers = total_workers;
			}
		} catch (std::exception &e) {
			logger->warn("Cannot read information");
		}

	}
}

/**
 * @brief		인증 토큰 요청 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 08. 18. 10:13:19
 * @param[in]	config			서버 설정 
 * @param[in]	curl_headers	CURL 헤더 
 */
static std::string
__get_token(const char *id,
			const itfact::common::Configuration *config,
			std::shared_ptr<struct curl_slist> curl_headers) {
	CURL *curl = curl_easy_init();
	if (!curl) {
		logger->error("[%s] Cannot allocation CURL", id);
		return "";
	}

	std::shared_ptr<CURL> ctx(curl, curl_easy_cleanup);
	std::string uri(config->getConfig("api.url", default_config.apiserver_url.c_str()));
	uri.append("/login");

	std::string auth_body("{\"username\": \"vr_server\", \"password\": \"");
	auth_body.append(config->getConfig("api.api_key", default_config.api_key.c_str()));
	auth_body.append("\"}");

	curl_easy_setopt(ctx.get(), CURLOPT_NOSIGNAL, 1); // longjmp causes uninitialized stack frame 
	curl_easy_setopt(ctx.get(), CURLOPT_URL, uri.c_str()); // URL 설정 
	curl_easy_setopt(ctx.get(), CURLOPT_POST, true);
	curl_easy_setopt(ctx.get(), CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(ctx.get(), CURLOPT_HTTPHEADER, curl_headers.get());
	curl_easy_setopt(ctx.get(), CURLOPT_NOPROGRESS, true);
	curl_easy_setopt(ctx.get(), CURLOPT_TIMEOUT, 3L);
	curl_easy_setopt(ctx.get(), CURLOPT_POSTFIELDS, auth_body.c_str());
	curl_easy_setopt(ctx.get(), CURLOPT_WRITEFUNCTION, getData);
	std::string token("");
	curl_easy_setopt(ctx.get(), CURLOPT_WRITEDATA, (void *) &token); // 바디 출력 설정 

	// 인증 요청 
	std::string auth_token("authorization: ");
	try {
		CURLcode response_code = curl_easy_perform(ctx.get());
		if (response_code == CURLE_OK) {
			long http_code = 0;
			curl_easy_getinfo(ctx.get(), CURLINFO_RESPONSE_CODE, &http_code);
			if (http_code == 200) {
				auto pos = token.find("access_token");
				if (pos == std::string::npos) 
					throw std::invalid_argument("Login fail");

				auto t_pos = token.find(":", pos);
				if (t_pos == std::string::npos)
					throw std::invalid_argument("Login fail");

				auto s_pos = token.find("\"", t_pos);
				if (s_pos == std::string::npos)
					throw std::invalid_argument("Login fail");
				else
					++s_pos;

				auto e_pos = token.find("\"", s_pos);
				if (e_pos == std::string::npos) 
					throw std::invalid_argument("Login fail");

				auth_token.append(token.substr(s_pos, e_pos - s_pos));
				logger->debug("[%s] %s", id, auth_token.c_str());
			} else {
				logger->error("[%s] Access denied(%d)", id, http_code);
			}
		} else {
			logger->error("[%s] cURL error: %s(%d)", id, curl_easy_strerror(response_code), response_code);
		}
	} catch (std::exception &e) {
		// 인증 실패 
		logger->error("[%s] Error occurred: %s", id, e.what());
	}

	return auth_token;
}

/**
 * @brief		STT 요청 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 10. 07. 14:56:47
 * @param[in]	metadata	Metadata
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int VRInotify::sendRequest(
		const std::string &id,
		const itfact::common::Configuration *config,
		const char *apiserver_uri,
		const std::string &call_id,
		const std::string &body,
		const std::string &download_uri,
		const char *output_path, std::map<std::thread::id,std::string> *list) {
	std::lock_guard<std::mutex> guard(req_lock);

	// HTTP 헤더 생성 
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	if (!headers) {
		logger->error("[%s] Cannot allocation Headers", id.c_str());
		finishJob(std::this_thread::get_id(), list);
		return EXIT_FAILURE;
	}
	std::shared_ptr<struct curl_slist> curl_headers(headers, curl_slist_free_all);
	curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");

	// 미인증 상태라면 인증 요청 
	if (token_header.empty() || token_header.size() < 20) {
		token_header = __get_token(id.c_str(), config, curl_headers);
		if (token_header.empty() || token_header.size() < 20) {
			finishJob(std::this_thread::get_id(), list);
			return EXIT_FAILURE;
		}
	}
	curl_slist_append(headers, token_header.c_str());

	// STT 요청 
	CURL *curl = curl_easy_init();
	if (!curl) {
		logger->error("[%s] Cannot allocation CURL", id.c_str());
		finishJob(std::this_thread::get_id(), list);
		return EXIT_FAILURE;
	}

	std::shared_ptr<CURL> ctx(curl, curl_easy_cleanup);
	curl_easy_setopt(ctx.get(), CURLOPT_URL, apiserver_uri); // URL 설정 
	curl_easy_setopt(ctx.get(), CURLOPT_POST, true);
	curl_easy_setopt(ctx.get(), CURLOPT_SSL_VERIFYPEER, 0L); // 인증서 무시 
	curl_easy_setopt(ctx.get(), CURLOPT_HTTPHEADER, curl_headers.get());
	curl_easy_setopt(ctx.get(), CURLOPT_NOPROGRESS, true);
	// curl_easy_setopt(ctx.get(), CURLOPT_VERBOSE, true);
	curl_easy_setopt(ctx.get(), CURLOPT_TIMEOUT, 0L);

	curl_easy_setopt(ctx.get(), CURLOPT_POSTFIELDS, body.c_str());
	// curl_easy_setopt(ctx.get(), CURLOPT_WRITEHEADER, stdout);
	curl_easy_setopt(ctx.get(), CURLOPT_WRITEFUNCTION, getData);
	std::string response_text("");
	curl_easy_setopt(ctx.get(), CURLOPT_WRITEDATA, (void *) &response_text); // 바디 출력 설정 

	// CURLMcode curl_multi_add_handle(CURLM *multi_handle, CURL *easy_handle); // FIXME:
	req_lock.unlock();
	logger->debug("[%s] POST %s HTTP/1.1\n%s", id.c_str(), apiserver_uri, body.c_str());
	try {
		CURLcode response_code = curl_easy_perform(ctx.get());
		if (response_code != CURLE_OK) {
			logger->error("[%s] Fail to request: (%d) %s", id.c_str(),
						  response_code, curl_easy_strerror(response_code));
			finishJob(std::this_thread::get_id(), list);
			return EXIT_FAILURE;
		}
	} catch (std::exception &e) {
		logger->error("[%s] Error %s", id.c_str(), e.what());
		finishJob(std::this_thread::get_id(), list);
		return EXIT_FAILURE;
	}

	req_lock.lock();
	long http_code = 0;
	curl_easy_getinfo(ctx.get(), CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code == 200 || http_code == 201) {
		logger->info("[%s] " COLOR_GREEN "Success" COLOR_NC, id.c_str());
		bool delete_on_success =
			const_cast<itfact::common::Configuration *>(config)->getConfig<bool>(
				"inotify.delete_on_success", &default_config.delete_on_success);

		// 성공이면 설정에 따라 삭제 
		if (delete_on_success) {
			std::string::size_type idx = download_uri.find("://");
			if (idx == std::string::npos || download_uri.find("file://") != std::string::npos) {
				std::string remove_pathname(idx == std::string::npos ? download_uri : download_uri.substr(idx + 3));
				try {
					std::remove(download_uri.c_str());
				} catch (std::exception &e) {
					logger->warn("[%s] Cannot remove: %s", id.c_str(), download_uri.c_str());
				}
			}
		}
	} else if (http_code == 401) {
		logger->notice("[%s] " COLOR_RED "Failure" COLOR_NC " (HTTP Response code: " COLOR_YELLOW "%d" COLOR_NC ")",
						id.c_str(), http_code);
		token_header = "";
		// 인증 실패의 경우 재시도 
		req_lock.unlock();
		logger->info("[%s] Retry job", id.c_str());
		return sendRequest(id, config, apiserver_uri, call_id, body, download_uri, output_path, list);
	} else {
		logger->warn("[%s] " COLOR_RED "Failure" COLOR_NC " (HTTP Response code: %s%d" COLOR_NC ")\n%s",
					  id.c_str(), (http_code >= 500 ? COLOR_RED : COLOR_YELLOW), http_code, response_text.c_str());
	}

	finishJob(std::this_thread::get_id(), list);
	return EXIT_SUCCESS;
}

/**
 * @brief		다운로드 경로 설정 
 * @details
 * 	download_path 우선순위:
 * 	1. index file
 * 	2. configuration file
 * 	3. input directory
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 10. 10. 22:16:27
 * @param[in]	config	환경 설정 
 */
static inline std::string
getDownloadPath(const std::string &filename,
				const std::shared_ptr<std::map<std::string, std::string>> metadata,
				const char *download_path = NULL) {
	std::string download_uri("");
	auto down_path = metadata->find("download_path");
	if (down_path != metadata->end()) {
		logger->debug("Download path from metadata: %s", down_path->second.c_str());
		download_uri.append(down_path->second);
	} else 	if (download_path != NULL)
		download_uri.append(download_path);

	if (download_uri.size() > 0 && download_uri[download_uri.size() - 1] != '/')
		download_uri.push_back('/');

	download_uri.append(filename);
	return download_uri;
}

/**
 * @brief		Event 처리 
 * @details		iNotify에 의해 확인된 녹취 파일을 분석 처리 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 19. 09:39:12
 * @param[in]	path		파일 위치 
 * @param[in]	filename	파일명 
 * @param[in]	config		서버 설정 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise, a EXIT_FAILURE is returned.
 */
int VRInotify::processRequest(
	const itfact::common::Configuration *config,
	const char *apiserver_uri, const char *pathname,
	const char *download_path, const std::string &format_string,
	const std::string data, const char *output, std::map<std::thread::id,std::string> *list) {
	std::string id(COLOR_BLACK_BOLD);
	id.append("job:");
	id.append(boost::lexical_cast<std::string>(std::this_thread::get_id()));
	id.push_back(' ');
	id.append(data.c_str());
	id.append(COLOR_NC);

	//working_job.fetch_add(1);

	if (pathname)
		logger->info("[%s] Request STT: %s", id.c_str(), pathname);

	std::shared_ptr<std::map<std::string, std::string>> metadata;
	try {
		metadata = config->parsingConfig(format_string, data);
	} catch (std::exception &e) {
		logger->error("[%s] Cannot parsing. %s", id.c_str(), e.what());
		finishJob(std::this_thread::get_id(), list);
		return EXIT_FAILURE;
	}

	// 다운로드 경로가 없는 경우 입력 경로로 설정 
	std::string tmp_input_path;
	if (download_path == NULL) {
		tmp_input_path = config->getConfig("inotify.input_path");
		download_path = const_cast<char *>(tmp_input_path.c_str());
	}

	std::string download_uri("");
	if (pathname != NULL) {
		// filename이 따로 설정된 경우 
		auto filename = metadata->find("filename");
		if (filename != metadata->end())
			download_uri = getDownloadPath(filename->second, metadata, download_path);
		else
			download_uri.append(pathname);
	} else {
		auto filename = metadata->find("filename");
		if (filename != metadata->end())
			download_uri = getDownloadPath(filename->second, metadata, download_path);
		else {
			logger->error("[%s] Invalid index_format: need to filename", id.c_str());
			finishJob(std::this_thread::get_id(), list);
			return EXIT_FAILURE;
		}
	}
	logger->info("[%s] Process %s", id.c_str(), download_uri.c_str());

	std::string post_data("{\"uri\": \"");
	post_data.append(download_uri);

	std::string call_id("");
	for (auto cur : *metadata.get()) {
		if (cur.first.compare("uri") == 0 ||
			cur.first.compare("filename") == 0 ||
			cur.first.compare("download_path") == 0 ||
			cur.first.compare("rec_time") == 0 ||
			cur.first.compare("output") == 0 ||
			cur.first.compare("silence") == 0) {
			continue;
		} else if (cur.first.compare("call_id") == 0) {
			call_id = cur.second;
		}

		post_data.append("\", \"");
		post_data.append(cur.first);
		post_data.append("\": \"");
		post_data.append(cur.second);

		if (cur.first.compare("rec_date") == 0) {
			auto search = metadata->find("rec_time");
			if (search != metadata->end())
				post_data.append(search->second);
		}
	}

	if (output) {
		post_data.append("\", \"output\": \"");
		post_data.append(output);
	}

	post_data.append("\", \"silence\": \"yes\"}");

	return sendRequest(id, config, apiserver_uri, call_id, post_data, download_uri, output, list);
}

/**
 * @brief		Wait for finish
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 09. 21. 22:32:19
 * @param[in]	max_worker	최대 제한 
 * @param[in]	seconds		슬립 시간 
 * @param[in]	increment	슬립 시간 증가값 
 */
#if 0
void VRInotify::waitForFinish(const int max_worker, const int seconds, const int increment ) {
	int wait_time = 0;
	while (true) {
		if (max_worker <= working_job.load()) {
			std::string thrdId = boost::lexical_cast<std::string>(std::this_thread::get_id());
			logger->debug("[job: %s] Sleep for %ds (running: %d, Max: %d)", thrdId.c_str(), wait_time, working_job.load(), max_worker);
			std::this_thread::sleep_for(std::chrono::seconds(seconds));
			wait_time += increment;
			if (wait_time > 300)
			{
				logger->warn("[job: %s] wait_time exceed 300s)", thrdId.c_str());
			}
		} else
			break;
	}
}
#endif
void VRInotify::waitForFinish(const int max_worker, const int seconds, const int increment, const char *filename, std::map<std::thread::id,std::string> *list ) {
	int wait_time = 0;
	std::map<std::thread::id,std::string>::iterator iter;
	while (true) {
		if (max_worker <= working_job.load()) {
			std::string thrdId = boost::lexical_cast<std::string>(std::this_thread::get_id());
			std::string pids;

			if (list && list->size()) {
				pids.append("SubThreads[ ");
				for(iter = list->begin(); iter != list->end(); iter++) {
					char spid[64];
					sprintf(spid, "%s ", (iter->second).c_str());
					pids.append(spid);
				}
				pids.push_back(']');
				//logger->debug("%s", pids.c_str());
				logger->debug("[job: %s] Sleep for %ds (running: %d, Max: %d, Filename: %s) %s", thrdId.c_str(), wait_time, working_job.load(), max_worker, filename, pids.c_str());
			}
			std::this_thread::sleep_for(std::chrono::seconds(seconds));
			wait_time += increment;
			if (wait_time > 300)
			{
				logger->warn("[job: %s] wait_time exceed 300s, File: %s)", thrdId.c_str(), filename);
			}
		} else
			break;
	}
}

/**
 * @brief		Event 처리 
 * @details		iNotify에 의해 확인된 녹취 파일을 분석 처리 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 05. 10:55:57
 * @param[in]	path		파일 위치 
 * @param[in]	filename	파일명 
 * @param[in]	config		서버 설정 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise, a EXIT_FAILURE is returned.
 */
int VRInotify::runJob(const std::shared_ptr<std::string> path,
					  const std::shared_ptr<std::string> filename,
					  const itfact::common::Configuration *config) {
	int rc;
	std::string name(filename->c_str(), filename->rfind("."));
	char *output_path = NULL;
	std::string output_pathname;
	if (config->isSet("inotify.output_path")) {
		output_pathname = config->getConfig("inotify.output_path");
		const bool unique = config->getConfig<bool>("inotify.unique_output", default_config.unique_output);
		if (config->getConfig<bool>("inotify.daily_output", default_config.daily_output) || unique) {
			time_t now = time(0);
			tm *ltm = localtime(&now);
			if (output_pathname[output_pathname.size() - 1] != '/')
				output_pathname.push_back('/');
			output_pathname.append(boost::lexical_cast<std::string>(1900 + ltm->tm_year)); // Year
			output_pathname.push_back('/');
			output_pathname.append(boost::lexical_cast<std::string>(1 + ltm->tm_mon)); // Month
			output_pathname.push_back('/');
			output_pathname.append(boost::lexical_cast<std::string>(ltm->tm_mday)); // Day

			if (unique) {
				output_pathname.push_back('/');
				if (ltm->tm_hour < 10)
					output_pathname.push_back('0');
				output_pathname.append(boost::lexical_cast<std::string>(ltm->tm_hour)); // Hour
				if (ltm->tm_min < 10)
					output_pathname.push_back('0');
				output_pathname.append(boost::lexical_cast<std::string>(ltm->tm_min)); // Minute
				if (ltm->tm_sec < 10)
					output_pathname.push_back('0');
				output_pathname.append(boost::lexical_cast<std::string>(ltm->tm_sec)); // Second
				output_pathname.push_back('_');
				output_pathname.append(boost::lexical_cast<std::string>(std::clock() & (CLOCKS_PER_SEC - 1)));
				// output_pathname.append(boost::lexical_cast<std::string>(std::clock()));
			}
		}

		itfact::common::checkPath(output_pathname, true);
		output_path = const_cast<char *>(output_pathname.c_str());
	}

	std::string apiserver(config->getConfig("api.url", default_config.apiserver_url.c_str()));
	if (apiserver[apiserver.size() - 1] != '/')
		apiserver.push_back('/');
	apiserver.append(config->getConfig("api.service", default_config.api_service.c_str()));
	if (apiserver[apiserver.size() - 1] != '/')
		apiserver.push_back('/');
	apiserver.append(config->getConfig("api.version", default_config.apiserver_version.c_str()));
	apiserver.append("/jobs");

	// 분석할 파일 
	std::string pathname(path->c_str());
	if (pathname[pathname.size() - 1] != '/')
		pathname.push_back('/');
	pathname.append(filename->c_str());

	char *download_path = NULL;
	if (config->isSet("inotify.download_path"))
		download_path = const_cast<char *>(config->getConfig("inotify.download_path").c_str());

	std::string format_string;
	try {
		format_string = config->getConfig("inotify.index_format");
		logger->debug("index_format: %s", format_string.c_str());
	} catch (std::exception &e) {
		logger->error("Please check inotify.index_type and inotify.index_format");
		return EXIT_FAILURE;
	}

	// preprocess
	if (config->isSet("inotify.preprocess")) {
		std::string cmd(config->getConfig("inotify.preprocess"));
		const char *proc = config->getConfig("inotify.preprocess").c_str();
		cmd.push_back(' ');
		cmd.append(pathname.c_str());
		cmd.push_back(' ');

		// new pathname 
		pathname = "/tmp/";
		pathname.append(boost::lexical_cast<std::string>(std::this_thread::get_id()));
		pathname.append(".txt");

		cmd.append(pathname.c_str());
		logger->debug(cmd.c_str());
		rc = std::system(cmd.c_str());
		if (rc) {
			logger->error("Error(%d) occurred during execution '%s'", rc, proc);
			return EXIT_FAILURE;
		}
	}

	std::shared_ptr<std::map<std::string, std::string>> metadata;
	if (config->getConfig("inotify.index_type").compare("filename") == 0) {
		VRInotify::waitForFinish(available_workers.load(), 5, 5);
		std::string tmp_input_path = config->getConfig("inotify.input_path");
		download_path = const_cast<char *>(tmp_input_path.c_str());
		working_job.fetch_add(1);
		rc = VRInotify::processRequest( config, apiserver.c_str(), pathname.c_str(), download_path,
										format_string, *filename.get(), output_path);
	} else if (config->getConfig("inotify.index_type").compare("file") == 0) {
		VRInotify::waitForFinish(available_workers.load(), 5, 5);
		std::ifstream index_file(pathname);
		if (index_file.is_open()) {
			std::string::size_type idx = pathname.rfind(".");
			std::string wav_pathname(pathname.c_str(), (idx != std::string::npos ? idx : pathname.size()));
			wav_pathname.push_back('.');
			wav_pathname.append(config->getConfig("inotify.rec_ext", default_config.rec_ext.c_str()));
			std::string line;
			std::getline(index_file, line);
			working_job.fetch_add(1);
			rc = VRInotify::processRequest(config, apiserver.c_str(), wav_pathname.c_str(), download_path,
										   format_string, line, output_path);
		}
		index_file.close();
	} else if (config->getConfig("inotify.index_type").compare("pair") == 0) {
		// FIXME: 채널이 분리된 경우의 처리 
	} else if (config->getConfig("inotify.index_type").compare("list") == 0) {
		std::ifstream index_file(pathname);
		if (index_file.is_open()) {
			logger->info("Request STT with list '%s'", filename->c_str());
			std::thread *job;
			//std::set<std::thread::id> jobs;
			std::map<std::thread::id,std::string> jobs;
			std::vector<std::thread> jobList;
			std::set<std::thread::id>::iterator iter;

			int listCount=0;
			for(std::string line; std::getline(index_file, line); ) {
				if (line.empty() || line.size() < 5 )
					continue;
				listCount++;
			}
			addListCount(listCount);
			index_file.clear();
			index_file.seekg(0);

			for (std::string line; std::getline(index_file, line); ) {
				if (line.empty() || line.size() < 5)
					continue;

				VRInotify::waitForFinish(available_workers.load(), 1, 1, line.c_str(), &jobs);
#if 0	// disable code block by boolpae
				// FIXME: 동시 처리 문제를 확인하기 위한 코드 
				while (jobs.size() > 10000) {
					logger->warn("Waiting for end (# of running, remain: %lu)", working_job.load(), jobs.size());
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
#endif
				try {
					std::lock_guard<std::mutex> guard(job_lock);
					working_job.fetch_add(1);
					/*
					jobList.push_back( std::thread(VRInotify::processRequest, config, apiserver.c_str(),
									(const char *) NULL, download_path,
									format_string, line, output_path, nullptr) );
					*/
					job = new std::thread(VRInotify::processRequest, config, apiserver.c_str(),
									(const char *) NULL, download_path,
									format_string, line, output_path, &jobs);
					jobs.insert(make_pair(job->get_id(),line));
					job->detach();
				} catch (std::exception &e) {
					logger->warn("Job error: %s(%d) (#job: %d, running: %d)",
								 e.what(), errno, jobs.size(), working_job.load());
					break;
				}
			}
			index_file.close();
#if 1
			while (jobs.size() > 0) {
				#if 0
				std::string pids;

				pids.append("Running Threads[");
				pids.append(boost::lexical_cast<std::string>(std::this_thread::get_id()));
				pids.append("] - SubThreads[ ");
				for(iter = jobs.begin(); iter != jobs.end(); iter++) {
					char spid[64];
					sprintf(spid, "%ld ", (*iter));
					pids.append(spid);
				}
				pids.push_back(']');
				logger->debug("%s", pids.c_str());
				#endif
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
#else
			for(iter = jobList.begin(); iter != jobList.end(); iter++) {
				(*iter).join();
				//delete (*iter);
			}

			jobList.clear();
#endif
			rc = EXIT_SUCCESS;
			logger->info("Done: %s", filename->c_str());

			// 리스트 파일 삭제 설정이 되어 있으면 삭제 처리
			bool delete_on_list = const_cast<itfact::common::Configuration *>(config)->getConfig<bool>("inotify.delete_on_list");
			if (delete_on_list)
				std::remove(filename->c_str());
		} else {
			rc = errno;
			logger->error("Cannot read '%s'", pathname.c_str());
			return EXIT_FAILURE;
		}
	}

	// postprocess
	if (rc == EXIT_SUCCESS && config->isSet("inotify.postprocess")) {
		std::string cmd(config->getConfig("inotify.postprocess"));
		const char *proc = config->getConfig("inotify.postprocess").c_str();
		cmd.push_back(' ');
		cmd.append(pathname);
		cmd.push_back(' ');
		cmd.append(output_pathname);
		// for KT-DS
		cmd.push_back(' ');
		cmd.append(path->c_str());
		cmd.push_back('/');
		cmd.append(filename->c_str());

		logger->debug(cmd.c_str());
		rc = std::system(cmd.c_str());
		if (rc) {
			logger->error("Error(%d) occurred during execution '%s'", rc, proc);
			return EXIT_FAILURE;
		}
	}

	return rc;
}

/**
 * @brief		iNotify 초기화
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 30. 17:33:12
 * @param[in]	path	모니터링할 경로
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a positive error code is returned indicating what went wrong.
 */
int VRInotify::monitoring() {
	char buf[BUF_LEN] __attribute__ ((aligned(8)));
	memset(buf, 0x00, BUF_LEN);

	if (!config.isSet("inotify.input_path")) {
		logger->fatal("Not set inotify.input_path");
		return EXIT_FAILURE;
	}

	std::shared_ptr<std::string> path = std::make_shared<std::string>(config.getConfig("inotify.input_path"));
	logger->info("Initialize monitoring module to watch %s", path->c_str());
	if (!itfact::common::checkPath(*path.get(), true)) {
		logger->error("Cannot create directory '%s' with error %s", path->c_str(), std::strerror(errno));
		return EXIT_FAILURE;
	}

	int inotify = inotify_init();
	if (inotify < 0) {
		int rc = errno;
		logger->error("Cannot initialization iNotify '%s'", std::strerror(rc));
		return rc;
	}

	int wd = inotify_add_watch(inotify, path->c_str(), IN_CLOSE_WRITE);
	if (wd < 0) {
		int rc = errno;
		logger->error("Cannot watch '%s' with error %s", path->c_str(), std::strerror(rc));
		return rc;
	}

	// 가용한 워커 수 확인 (설정된 값을 우선시)
	if (config.isSet("inotify.maximum_jobs")) {
		available_workers = config.getConfig<unsigned long>("inotify.maximum_jobs", available_workers.load());
	} else {
		std::thread nr_worker(getTotalWorkers);
		nr_worker.detach();
	}

	std::string watch_ext = config.getConfig("inotify.watch", default_config.watch.c_str());
	while (true) {
		ssize_t numRead = read(inotify, buf, BUF_LEN);
		if (numRead <= 0) {
			int rc = errno;
			if (rc == EINTR)
				continue;

			logger->warn("Error occurred: (%d), %s", rc, std::strerror(rc));
			return rc;
		}

		/* Process IN_MOVED_TO event in buffer returned by read() */
		struct inotify_event *event = NULL;
		for (char *p = buf; p < buf + numRead; p += sizeof(struct inotify_event) + event->len) {
			event = (struct inotify_event *) p;
			if (!(event->mask & IN_ISDIR)) {
				// Call Job
				std::shared_ptr<std::string> filename = std::make_shared<std::string>(event->name, event->len);
				std::string file_ext = filename->substr(filename->rfind(".") + 1);
				if (filename->at(0) != '.' && file_ext.find(watch_ext) == 0 &&
					(file_ext.size() == watch_ext.size() || file_ext.at(watch_ext.size()) == '\0' )) {
					try {
						//VRInotify::waitForFinish(available_workers.load(), 1, 1);
						while(available_workers.load() < list_count.load()) {
							logger->debug("List Count(%d)", list_count.load());
							std::this_thread::sleep_for(std::chrono::seconds(1));
						}
						std::thread job(VRInotify::runJob, path, filename, &config);
						job.detach();
						std::this_thread::sleep_for(std::chrono::seconds(1));
					} catch (std::exception &e) {
						logger->warn("%s: %s", e.what(), filename->c_str());
					}
				} else {
					logger->debug("Ignore %s (Watch: '%s', ext: '%s') ListCount(%d)", filename->c_str(),
									watch_ext.c_str(), file_ext.c_str(), list_count.load());
				}
			}
		}
	}

	inotify_rm_watch(inotify, wd);
	return EXIT_SUCCESS;
}
