/**
 * @file	worker.cc
 * @brief	Worker
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 06. 07. 10:48:56
 * @see		feat_worker.cc
 */
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <mutex>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include "worker.hpp"

using namespace itfact::worker;

static std::mutex curl_lock;

/**
 * @brief		CURL 초기화 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 14. 09:38:57
 */
static inline void init_curl() {
	CURLcode rc = curl_global_init(CURL_GLOBAL_ALL);
	if (rc) {
		std::perror(curl_easy_strerror(rc));
		throw std::runtime_error(curl_easy_strerror(rc));
	}
}

WorkerDaemon::WorkerDaemon() {
	init_curl();
}

WorkerDaemon::WorkerDaemon(const int argc, const char *argv[]) : config(argc, argv) {
	init_curl();
	logger = config.getLogger();
}

WorkerDaemon::~WorkerDaemon() {
	curl_global_cleanup();
}

/**
 * @brief		초기화 후 실행 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 27. 10:12:37
 * @param[in]	argc	인수의 개수 
 * @param[in]	argc	인수 배열 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int WorkerDaemon::initialize(const int argc, const char *argv[]) {
	int rc = config.configure(argc, argv);
	if (rc)
		return rc;
	logger = config.getLogger();
	return initialize();
}

/**
 * @brief		워커를 실행시키기 위한 쓰래드 함수 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 07. 10:59:39
 * @param[in]	host	Connect to the host
 * @param[in]	port	Port number use for connection
 * @param[in]	timeout	Timeout in milliseconds
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise, a error code is returned indicating what went wrong.
 */
static int
worker_thread(const std::string name, const char *host, const int port, const int timeout,
			  WorkerDaemon *daemon, log4cpp::Category *logger, void *context,
			  gearman_return_t (*fn)(gearman_job_st *, void *)) {
	gearman_worker_st worker_st;
	if (gearman_worker_create(&worker_st) == NULL) {
		logger->fatal("[%s] Memory allocation failure on worker creation", name.c_str());
		return EXIT_FAILURE;
	}
	std::shared_ptr<gearman_worker_st> worker(&worker_st, gearman_worker_free);

	gearman_return_t ret = gearman_worker_add_server(worker.get(), host, port);
	if (ret != GEARMAN_SUCCESS) {
		logger->error("[%s] %s", name.c_str(), gearman_worker_error(worker.get()));
		return EXIT_FAILURE;
	}

	ret = gearman_worker_define_function(worker.get(),
										 name.c_str(), name.size(),
										 gearman_function_create(fn),
										 timeout,
										 context);
	if (gearman_failed(ret)) {
		std::cout << gearman_worker_error(worker.get()) << std::endl;
		logger->error("[%s] %s", name.c_str(), gearman_worker_error(worker.get()));
		return EXIT_FAILURE;
	}

	while (daemon->isRunning()) {
		try {
			ret = gearman_worker_work(worker.get());
			if (gearman_failed(ret)) {
				logger->error("[%s] %s", name.c_str(), gearman_worker_error(worker.get()));
				std::this_thread::sleep_for(std::chrono::seconds(10));
			}
		} catch (std::exception &e) {
			logger->error("[%s] Error detected. %s", name.c_str(), e.what());
			std::this_thread::sleep_for(std::chrono::seconds(10));
		}
	}

	return EXIT_SUCCESS;
}

/**
 * @brief		로딩할 워커 개수 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 24. 12:43:06
 * @param[in]	name	워커명
 */
unsigned long WorkerDaemon::getTotalWorkers(const std::string &name) {
	if (name.empty())
		return config.getThreads();

	std::string worker_env(name.c_str());
	worker_env.append(".worker");
	try {
		long nr_workers = static_cast<long>(config.getThreads());
		return static_cast<unsigned long>(config.getConfig<long>(worker_env, nr_workers));
	} catch (std::exception &e) {
		return config.getThreads();
	}
}

/**
 * @brief		워커 실행 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 20. 16:29:10
 */
void WorkerDaemon::run(const std::string name, void *context, unsigned int count,
						gearman_return_t (*fn)(gearman_job_st *, void *)) {
	logger->info("Initialize %s", name.c_str());
	//logger->info("Initialize count %d", count);
	is_running = true;
	if ( !name.compare("vr_realtime")) {
		unsigned int sNum = config.getConfig("realtime.startnum", 0);
		for (unsigned int i = sNum; i < count+sNum; ++i) {
			std::string sNewFname = name + "_" + std::to_string(i);
			workers.push_back(std::thread(worker_thread, sNewFname,
					config.getHost().c_str(), config.getPort(), config.getTimeout(),
					this, logger, context, fn));
		}
	}
	else {
		for (unsigned int i = 0; i < count; ++i) {
			workers.push_back(std::thread(worker_thread, name,
						config.getHost().c_str(), config.getPort(), config.getTimeout(),
						this, logger, context, fn));
		}
	}
}

/**
 * @brief		워커 종료 대기 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 20. 16:38:01
 */
void WorkerDaemon::join() {
	if (is_running) {
		for (std::vector<std::thread>::iterator iter = workers.begin(); iter != workers.end(); ++iter)
			(*iter).join();
	}
}

/**
 * @brief		워커 종료 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 23. 13:03:41
 * @see			run()
 */
void WorkerDaemon::stop() {
	is_running = false;

	/// todo 응답을 대기하고 있는 워커 강제 종료 
}

//--------------------------------------------------------------------------------
/**
 * @brief		데이터 다운로드 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 28. 22:59:42
 * @param[in]	source		받은 데이터 
 * @param[in]	size		메모리 블럭 크기 
 * @param[in]	nmemb		메모리 블럭 수 
 * @param[in]	userData	받은 데이터 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
static size_t getData(void *source , size_t size , size_t nmemb , void *userData) {
	const int total_size = size * nmemb;
	const int length = total_size / sizeof(short);
	short *data = (short *) source;
	std::vector<short> *buffer = (std::vector<short> *) userData;

	for (int i = 0; i < length; ++i) {
		buffer->push_back(data[i]);
	}

	return total_size;
}

static const std::map<std::string, enum PROTOCOL>
protocol_list =  {{"file", PROTOCOL_FILE},
				  {"mount", PROTOCOL_MOUNT},
				  {"http", PROTOCOL_HTTP},
				  {"https", PROTOCOL_HTTPS},
				  {"ftp", PROTOCOL_FTP},
				  {"ftps", PROTOCOL_FTPS},
				  {"sftp", PROTOCOL_SFTP},
				  {"scp", PROTOCOL_SFTP},
				  {"ssh", PROTOCOL_SFTP}};
/**
 * @brief		download data
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 17. 16:27:10
 * @return		프로토콜 타입 반환 
 * @retval		PROTOCOL_NONE	스트리밍 데이터
 * @exception	domain_error	지원하지 않거나 알 수 없는 프로토콜 
 * @exception	runtime_error	다운로드 실패 
 */
enum PROTOCOL
WorkerDaemon::downloadData(
	const common::Configuration *config,	//< 설정
	const char *workload,					//< 수신된 데이터 
	const size_t workload_size,				//< 수신된 크기
	std::vector<short> &buffer,				//< 수신할 버퍼
	const std::string *account,				//< 계정 정보 
	log4cpp::Category *logger				//< 로거 
) {
	std::string metadata(std::string(workload, 10));
	std::string::size_type pos = metadata.find("://");
	if (pos == std::string::npos)
		return PROTOCOL_NONE;

	enum PROTOCOL protocol;
	try {
		protocol = protocol_list.at(metadata.substr(0, pos));
	} catch (std::out_of_range &e) {
		// 스트리밍 데이터 | 미지원 프로토콜 
		logger->warn("Unsupported protocol: %s", metadata.substr(0, pos).c_str());
		return PROTOCOL_NONE;
	}

	short sData;
	std::string filename;
	if (protocol == PROTOCOL_FILE) { // 파일 리드 
		filename = std::string(workload, workload_size).substr(7);
		logger->debug("Read file: %s", filename.c_str());

		std::FILE *fp = std::fopen(filename.c_str(), "rb");
		if (!fp) {
			int ret = errno;
			logger->error("%s: %s", std::strerror(ret), filename.c_str());
			throw std::runtime_error(std::strerror(ret));
		}
		std::shared_ptr<std::FILE> fd(fp, std::fclose);

		// read 
		while (std::fread(&sData, sizeof(short), 1, fd.get()) > 0)
			buffer.push_back(sData);

		return protocol;
	}

	if (protocol == PROTOCOL_MOUNT) {
		filename = std::string(workload, workload_size).substr(8);
		logger->debug("[0x%X] Read file: %s", THREAD_ID, filename.c_str());

		if (!boost::filesystem::exists(filename)) {
			int ret = errno;
			logger->error("%s: %s", std::strerror(ret), filename.c_str());
			throw std::runtime_error(std::strerror(ret));
		}


		std::FILE *fp = std::fopen(filename.c_str(), "rb");
		if (!fp) {
			int ret = errno;
			logger->error("%s: %s", std::strerror(ret), filename.c_str());
			throw std::runtime_error(std::strerror(ret));
		}
		std::shared_ptr<std::FILE> fd(fp, std::fclose);

		// read 
		while (std::fread(&sData, sizeof(short), 1, fd.get()) > 0)
			buffer.push_back(sData);

		return protocol;
	}

	if (protocol == PROTOCOL_FTP && config->getConfig<bool>("master.use_ftp_ssl", false))
		protocol = PROTOCOL_FTPS;

	std::lock_guard<std::mutex> guard(curl_lock);
	CURLcode response_code;
	CURL *_curl = curl_easy_init();
	if (!_curl)
		throw std::runtime_error("Cannot allocation CURL");
	std::shared_ptr<CURL> ctx(_curl, curl_easy_cleanup);

	switch (protocol) {
		case PROTOCOL_FTPS:	// FTPS 프로토콜
			curl_easy_setopt(ctx.get(), CURLOPT_USE_SSL, CURLUSESSL_ALL);
			// CURLFTPAUTH_TLS // TLS first

		case PROTOCOL_SFTP:	// SFTP 프로토콜
		case PROTOCOL_FTP:	// FTP 프로토콜
			if (account && !account->empty()) {
				logger->debug("Account %s", account->c_str());
				curl_easy_setopt(ctx.get(), CURLOPT_USERPWD, account->c_str()); // User Password 설정 
			}

		case PROTOCOL_HTTPS:	// HTTPS 프로토콜
			if (config->getConfig<bool>("master.ssl_insecure", false))
				curl_easy_setopt(ctx.get(), CURLOPT_SSL_VERIFYPEER, 0);
		case PROTOCOL_HTTP:		// HTTP 프로토콜
			filename = std::string(workload, workload_size);
			logger->debug("Download %s", filename.c_str());

			// curl_easy_setopt(ctx.get(), CURLOPT_WRITEHEADER, stderr); // 헤더 출력 설정 
			curl_easy_setopt(ctx.get(), CURLOPT_URL, filename.c_str());
			curl_easy_setopt(ctx.get(), CURLOPT_WRITEFUNCTION, getData); // Write function 설정 
			curl_easy_setopt(ctx.get(), CURLOPT_WRITEDATA, (void *) &buffer); // 바디 출력 설정 
			curl_easy_setopt(ctx.get(), CURLOPT_NOPROGRESS, true);
			// curl_easy_setopt(ctx.get(), CURLOPT_VERBOSE, true);

			response_code = curl_easy_perform(ctx.get());
			if (response_code != CURLE_OK) {
				logger->alert("%s (%d)", curl_easy_strerror(response_code), response_code);
				throw std::runtime_error(curl_easy_strerror(response_code));
			}

			break;

		default:	// 알 수 없는 프로토콜 
			logger->alert("Not implemented yet");
			throw std::domain_error("not implemented yet");
			break;
	}

	return protocol;
}

/**
 * @brief		upload data
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 03. 14:40:36
 * @param[in]	url			업로드 URL
 * @param[in]	pathname	업로드  파일 
 * @param[in]	account		계정 정보 
 * @return		Upon successful completion, a CURL_OK is returned.\n
 				Otherwise,
 				a error code is returned indicating what went wrong.
 * @see			Worker::downloadData()
 */
CURLcode
WorkerDaemon::uploadData(
	const common::Configuration *config,	//< 설정
	const std::string &uri,					//< 업로드 URL
	const std::string &pathname,			//< 업로드  파일 
	const std::string *account,				//< 계정 정보 
	log4cpp::Category *logger				//< 로거
) {
	std::string::size_type pos = uri.find("://");
	if (pos == std::string::npos)
		return CURLE_URL_MALFORMAT;

	enum PROTOCOL protocol;
	try {
		protocol = protocol_list.at(uri.substr(0, pos));
	} catch (std::out_of_range &e) {
		// 스트리밍 데이터 | 미지원 프로토콜 
		logger->error("Unsupported protocol: %s", uri.substr(0, pos).c_str());
		return CURLE_UNSUPPORTED_PROTOCOL;
	}

	std::FILE *fp = std::fopen(pathname.c_str(), "rb");
	if (!fp) {
		logger->debug("Cannot open: %s", pathname.c_str());
		return CURLE_UPLOAD_FAILED;
	}
	std::shared_ptr<std::FILE> fd(fp, std::fclose);

	std::lock_guard<std::mutex> guard(curl_lock);
	// curl_lock.lock();
	CURL *_curl = curl_easy_init();
	if (!_curl) {
		logger->error("Cannot allocation CURL");
		return CURLE_FAILED_INIT;
	}
	std::shared_ptr<CURL> ctx(_curl, curl_easy_cleanup);

	// FTPs 설정 
	if (protocol == PROTOCOL_FTP && config->getConfig<bool>("use_ftp_ssl", false)) {
		protocol = PROTOCOL_FTPS;
		curl_easy_setopt(ctx.get(), CURLOPT_USE_SSL, CURLUSESSL_ALL);
	}

	curl_easy_setopt(ctx.get(), CURLOPT_URL, uri.c_str());
	switch (protocol) {
		case PROTOCOL_SFTP:		// SFTP 프로토콜
			curl_easy_setopt(ctx.get(), CURLOPT_PROTOCOLS, CURLPROTO_SFTP);

		case PROTOCOL_FTPS:	// FTPS 프로토콜
		case PROTOCOL_FTP:	// FTP 프로토콜
			if (account && !account->empty())
				curl_easy_setopt(ctx.get(), CURLOPT_USERPWD, account->c_str()); // User Password 설정 

			curl_easy_setopt(ctx.get(), CURLOPT_UPLOAD, true);
			curl_easy_setopt(ctx.get(), CURLOPT_READDATA, fd.get()); // 업로드 파일 디스크립터 
			// curl_easy_setopt(ctx.get(), CURLOPT_INFILESIZE_LARGE, 00); // 업로드 사이즈 
			if (config->getConfig<bool>("ssl_insecure", false))
				curl_easy_setopt(ctx.get(), CURLOPT_SSL_VERIFYPEER, 0);
			break;

		default:	// 알 수 없는 프로토콜 
			logger->alert("Not implemented yet");
			return CURLE_UNSUPPORTED_PROTOCOL;
	}

	logger->debug("Upload file: %s", pathname.c_str());
	CURLcode response_code = curl_easy_perform(ctx.get());
	if (response_code != CURLE_OK)
		logger->warn("Cannot upload, %s", curl_easy_strerror(response_code));
	else {
		double speed_upload, total_time;
		curl_easy_getinfo(ctx.get(), CURLINFO_SPEED_UPLOAD, &speed_upload);
		curl_easy_getinfo(ctx.get(), CURLINFO_TOTAL_TIME, &total_time);
		logger->debug("Done: %.3f bytes/sec during %.3f seconds", speed_upload, total_time);
	}

	return response_code;
}
