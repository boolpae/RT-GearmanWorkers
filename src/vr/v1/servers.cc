/**
 * @file	servers.cc
 * @brief	Request Servers v1.0
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 12. 14:10:59
 * @see		restapi_v1.cc
 */
#include <cstring>
#include <exception>
#include <fstream>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "system_info.hpp"
#include "restapi_v1.hpp"

using namespace itfact::vr::node::v1;

const std::string Servers::resource_name = "servers";

bool Servers::equals(const std::string *resource) {
	if (resource->compare(resource_name) == 0)
		return true;
	return false;
}

/**
 * @brief		Request 처리 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 12. 14:11:30
 * @param[in]	Name	Param description
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int Servers::request(const std::string *id,
			const char *upload_data, size_t *upload_data_size, void **con_cls) {
	logger->debug("[%s] in Servers resource", job_name);

	if (method != HTTP_POST && !id) {
		logger->error("[%s] Bad request: Cannot find server name", job_name);
		RestApi::sendBadRequest(connection, "잘못된 요청입니다.");
		return EXIT_FAILURE;
	} else {
		// 마스터로부터 요청이 온 상태이므로 ID가 호스트네임이나 IP주소와 일치하는지를 확인 
		char hostname[256];
		int rc = gethostname(hostname, 256);
		if (rc) {
			logger->error("[%s] Cannot read hostname: %s", job_name, strerror(rc));
			RestApi::sendInternalServerError(connection, "요청하는 서버의 Hostname 설정을 확인해주시기 바랍니다.");
			return EXIT_FAILURE;
		}

		logger->debug("[%s] Input: %s, Hostname: %s", job_name, id->c_str(), hostname);
		if (id->compare(hostname) != 0) {
			logger->warn("[%s] Bad request: hostname mismatch(%s)", job_name, id->c_str());
			RestApi::sendBadRequest(connection, "잘못된 요청입니다.");
			return EXIT_FAILURE;
		}
	}

	switch (method) {
	case HTTP_GET:
		getServerState(id);
		break;
	case HTTP_PUT:
	case HTTP_PATCH:
	case HTTP_DELETE:
		// break;
	case HTTP_POST:
	default:
		logger->error("[%s] Unsupported method", job_name);
		RestApi::sendUnsupportedMethod(connection, "허용되지 않은 메소드입니다.");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/**
 * @brief		CPU 정보를 JSON 형태로 반환 
 * @details		공용 시스템 정보 함수의 정보 외에 코어별 사용률을 얻기 위해 별도로 구현 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 25. 12:28:13
 * @return		JSON 형태의 CPU 정보 
 * @see			Servers::getCpuInfo()
 * @see			Servers::getMemoryInfo()
 * @see			Servers::getDiskInfo()
 * @see			Servers::getNetworkInfo()
 */
std::string Servers::getCpuInfo(const char *value) {
	std::string result("\"cpu\": {");
	std::map<std::string, std::string> cores;
	logger->debug("[%s] Check CPU Information", job_name);
	std::shared_ptr<std::map<std::string, unsigned long long>> cpu_info
		= itfact::common::SystemInfo::getCpuInfo();

	if (cpu_info && !cpu_info->empty()) {
		bool is_first = true;
		for (auto cur : *cpu_info.get()) {
			if (!is_first)
				result.append(", ");
			std::vector<std::string> name;
			boost::split(name, cur.first, boost::is_any_of("_"), boost::token_compress_on);

			if (name[0].compare("cpu") == 0) {
				result.append("\"");
				result.append(name[1]);
				result.append("\": ");
				result.append(boost::lexical_cast<std::string>(cur.second));
				is_first = false;
			} else {
				if (cores.find(name[0]) == cores.end()) {
					cores[name[0]] = "\"id\": ";
					cores[name[0]].append("\"");
					cores[name[0]].append(name[0]);
					cores[name[0]].append("\"");
				}

				cores[name[0]].append(", \"");
				cores[name[0]].append(name[1]);
				cores[name[0]].append("\": ");
				cores[name[0]].append(boost::lexical_cast<std::string>(cur.second));
			}
		}
	}

	result.append(", \"cores\": [");
	if (!cores.empty()) {
		bool is_first = true;
		for (auto core : cores) {
			if (!is_first)
				result.append(", ");
			result.push_back('{');
			result.append(core.second);
			result.push_back('}');
			is_first = false;
		}
	}

	result.append("]}");
	return result;
}
// std::string Servers::getCpuInfo() {
// 	std::string result("\"cpu\": {");
// 	logger->debug("[%s] Check CPU Information", job_name);
// 	std::ifstream stat_file("/proc/stat");
// 	if (stat_file.is_open()) {
// 		for (std::string line; std::getline(stat_file, line); ) {
// 			std::vector<std::string> v;
// 			boost::split(v, line, boost::is_any_of("\t "), boost::token_compress_on);
// 			if (v.size() < 5 || v[0].find("cpu") != 0)
// 				continue;

// 			unsigned long long user = stoull(v[1]);
// 			// unsigned long long nice = stoull(v[2]);
// 			unsigned long long sys = stoull(v[3]);
// 			// unsigned long long idle = stoull(v[4]);

// 			unsigned long long total = user + sys + stoull(v[2]) + stoull(v[4]);

// 			double user_usage = 100.00 * user / total;
// 			double sys_usage = 100.00 * sys / total;

// 			logger->debug("[%s] %s usage: user %f%%, system %f%%", job_name, v[0].c_str(), user_usage, sys_usage);

// 			if (v[0].size() > 3) {
// 				int id = stoi(v[0].substr(3));
// 				if (id > 0)
// 					result.append(", ");

// 				result.append("{\"id\": ");
// 				result.append(v[0].substr(3));
// 				result.append(", ");
// 			}

// 			char usage[6];
// 			result.append("\"user\": ");
// 			sprintf(usage, "%.02f", user_usage);
// 			result.append(usage);
// 			result.append(", \"system\": ");
// 			sprintf(usage, "%.02f", sys_usage);
// 			result.append(usage);

// 			if (v[0].compare("cpu") == 0)
// 				result.append(", \"cores\": [");

// 			if (v[0].size() > 3)
// 				result.push_back('}');

// 		}
// 		result.append("]}");
// 		stat_file.close();
// 	} else
// 		throw std::runtime_error("Cannot read CPU information"); //?

// 	return result;
// }

/**
 * @brief		메모리 정보를 JSON 형태로 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 25. 15:04:45
 * @return		JSON 형태의 메모리 정보 
 * @see			Servers::getCpuInfo()
 * @see			Servers::getMemoryInfo()
 * @see			Servers::getDiskInfo()
 * @see			Servers::getNetworkInfo()
 */
std::string Servers::getMemoryInfo() {
	logger->debug("[%s] Check Memory Information", job_name);
	std::string result("\"memory\": {");
	std::shared_ptr<std::map<std::string, unsigned long long>> mem_info
		= itfact::common::SystemInfo::getMemoryInfo();
	if (mem_info && !mem_info->empty()) {
		bool is_first = true;
		for (auto cur : *mem_info.get()) {
			if (!is_first)
				result.append(", ");
			result.append("\"");
			result.append(cur.first);
			result.append("\": ");
			result.append(boost::lexical_cast<std::string>(cur.second));
			is_first = false;
		}
	}

	result.push_back('}');
	return result;
}

/**
 * @brief		디스크 정보를 JSON 형태로 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 27. 09:51:58
 * @return		JSON 형태의 디스크 정보 
 * @see			Servers::getCpuInfo()
 * @see			Servers::getMemoryInfo()
 * @see			Servers::getDiskInfo()
 * @see			Servers::getNetworkInfo()
 */
std::string Servers::getDiskInfo() {
	logger->debug("[%s] Check Disk Information", job_name);
	std::string result("\"disk\": {");
	std::shared_ptr<std::map<std::string, unsigned long long>> disk_info
		= itfact::common::SystemInfo::getDiskInfo();
	if (disk_info && !disk_info->empty()) {
		bool is_first = true;
		for (auto cur : *disk_info.get()) {
			if (!is_first)
				result.append(", ");
			result.append("\"");
			result.append(cur.first);
			result.append("\": ");
			result.append(boost::lexical_cast<std::string>(cur.second));
			is_first = false;
		}
	}

	result.push_back('}');
	return result;
}

/**
 * @brief		네트워크 트래픽 정보를 JSON 형태로 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 27. 09:51:58
 * @return		JSON 형태의 네트워크 트래픽 정보 
 * @see			Servers::getCpuInfo()
 * @see			Servers::getMemoryInfo()
 * @see			Servers::getDiskInfo()
 * @see			Servers::getNetworkInfo()
 */
std::string Servers::getNetworkInfo(const char *value) {
	logger->debug("[%s] Check Network Information", job_name);
	std::string result("\"network\": [");
	std::shared_ptr<std::map<std::string, itfact::common::NetworkTraffic>> net_info
		= itfact::common::SystemInfo::getNetworkInfo();

	unsigned long long pre_tx = 0;
	unsigned long long pre_rx = 0;
	if (value) {
		try {
			std::vector<std::string> v;
			boost::split(v, value, boost::is_any_of(","), boost::token_compress_on);
			if (v.size() == 2) {
				pre_rx = std::stoull(v[0]);
				pre_tx = std::stoull(v[1]);
			}
		} catch (std::exception &e) {
			pre_rx = pre_tx = 0;
		}
	}

	if (net_info && !net_info->empty()) {
		bool is_first = true;
		for (auto cur : *net_info.get()) {
			if (!is_first)
				result.append(", ");
			result.append("{\"name\": \"");
			result.append(cur.first);
			result.append("\", \"rx\": ");
			result.append(boost::lexical_cast<std::string>(cur.second.rx - pre_rx));
			result.append(", \"tx\": ");
			result.append(boost::lexical_cast<std::string>(cur.second.tx - pre_tx));
			result.push_back('}');
			is_first = false;
		}
	}

		result.append("]");
	return result;
}

/**
 * @brief		서버 정보 요청 처리 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 16:49:04
 * @param[in]	id	요청된 서버명 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int Servers::getServerState(const std::string *id) {
	logger->debug("[%s] Request Server Info", job_name);
	const char *query = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "q");
	if (query)
		logger->debug("[%s] Query: %s", job_name, query);

	const char *value = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "v");
	if (value)
		logger->debug("[%s] Value: %s", job_name, value);

	std::string response("{");
	if (!query) {
		response.append(getCpuInfo(value));
		response.append(", ");
		response.append(getMemoryInfo());
		response.append(", ");
		response.append(getDiskInfo());
		response.append(", ");
		response.append(getNetworkInfo(value));
	} else if (std::strlen(query) == 3 && std::strncmp(query, "cpu", 3) == 0) {
		response.append(getCpuInfo(value));
	} else if (std::strlen(query) == 6 && std::strncmp(query, "memory", 6) == 0) {
		response.append(getMemoryInfo());
	} else if (std::strlen(query) == 4 && std::strncmp(query, "disk", 4) == 0) {
		response.append(getDiskInfo());
	} else if (std::strlen(query) == 7 && std::strncmp(query, "network", 7) == 0) {
		response.append(getNetworkInfo(value));
	} else {
		logger->error("[%s] Bad request: Cannot find server name", job_name);
		RestApi::sendBadRequest(connection, "잘못된 요청입니다.");
		return EXIT_FAILURE;
	}

	response.push_back('}');
	if (RestApi::response(connection, response.c_str(), response.size()))
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}
