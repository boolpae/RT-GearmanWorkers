/**
 * @file	system_info.cc
 * @brief	서버 정보 
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 25. 19:58:26
 * @see		
 */
#include <cctype>
#include <cstring>
#include <cmath>
#include <exception>
#include <fstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "system_info.hpp"

using namespace itfact::common;

NetworkTraffic NetworkTraffic::operator+(const NetworkTraffic &rhs) {
	NetworkTraffic result(rx + rhs.rx, tx + rhs.tx);
	return result;
}

NetworkTraffic &NetworkTraffic::operator+=(const NetworkTraffic &rhs) {
	rx += rhs.rx;
	tx += rhs.tx;
	return *this;
}

NetworkTraffic NetworkTraffic::operator-(const NetworkTraffic &rhs) {
	NetworkTraffic result(rx - rhs.rx, tx - rhs.tx);
	return result;
}

NetworkTraffic &NetworkTraffic::operator-=(const NetworkTraffic &rhs) {
	rx -= rhs.rx;
	tx -= rhs.tx;
	return *this;
}

NetworkTraffic NetworkTraffic::operator*(const NetworkTraffic &rhs) {
	NetworkTraffic result(rx * rhs.rx, tx * rhs.tx);
	return result;
}

NetworkTraffic &NetworkTraffic::operator*=(const NetworkTraffic &rhs) {
	rx *= rhs.rx;
	tx *= rhs.tx;
	return *this;
}

NetworkTraffic NetworkTraffic::operator/(const NetworkTraffic &rhs) {
	NetworkTraffic result(rx / rhs.rx, tx / rhs.tx);
	return result;
}

NetworkTraffic &NetworkTraffic::operator/=(const NetworkTraffic &rhs) {
	rx /= rhs.rx;
	tx /= rhs.tx;
	return *this;
}


NetworkTraffic NetworkTraffic::operator*(unsigned long long ratio) {
	NetworkTraffic result(rx * ratio, tx * ratio);
	return result;
}

NetworkTraffic &NetworkTraffic::operator*=(unsigned long long ratio) {
	rx *= ratio;
	tx *= ratio;
	return *this;
}

NetworkTraffic NetworkTraffic::operator/(unsigned long long ratio) {
	NetworkTraffic result(rx / ratio, tx / ratio);
	return result;
}

NetworkTraffic &NetworkTraffic::operator/=(unsigned long long ratio) {
	rx /= ratio;
	tx /= ratio;
	return *this;
}

/**
 * @brief		CPU 정보 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 25. 20:00:41
 * @return		Map<std::string, double> 형태의 CPU 사용율 (%단위)
 * @see			SystemInfo::getCpuInfo()
 * @see			SystemInfo::getMemoryInfo()
 * @see			SystemInfo::getDiskInfo()
 * @see			SystemInfo::getNetworkInfo()
 */
std::shared_ptr<std::map<std::string, unsigned long long>>
SystemInfo::getCpuInfo() {
	std::shared_ptr<std::map<std::string, unsigned long long>> result
		= std::make_shared<std::map<std::string, unsigned long long>>();
	std::ifstream stat_file("/proc/stat");
	if (stat_file.is_open()) {
		for (std::string line; std::getline(stat_file, line); ) {
			std::vector<std::string> v;
			boost::split(v, line, boost::is_any_of("\t "), boost::token_compress_on);
			if (v.size() < 5 || v[0].find("cpu") != 0)
				continue;

			std::string name(v[0]);
			unsigned long long user = stoull(v[1]) + stoull(v[2]);
			unsigned long long sys = stoull(v[3]);
			unsigned long long idle = stoull(v[4]);

			(*result.get())[name + "_user"] = user;
			(*result.get())[name + "_system"] = sys;
			(*result.get())[name + "_idle"] = idle;
		}
	} else
		throw std::runtime_error("Cannot read CPU information");
	stat_file.close();

	return result;
}

/**
 * @brief		입력된 값을 KiB 단위로 반환 
 * @details		입력된 값을 KiB 단위로 반환하는 메소드로,
 				입력받은 단위가 없으면 기본 단위인 KiB로 인식한다.
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 25. 20:07:14
 * @exception	invalid_argument	잘못된 매개변수 
 */
static inline unsigned long long
string2ull(std::string &value, const char *unit = NULL) {
	// KiB KB
	// GiB GB

	long double result = stold(value);
	if (unit) {
		size_t unit_size = std::strlen(unit);
		if (unit_size > 3)
			throw std::invalid_argument("Invalid argument: argument 'unit' is too long");

		switch (std::tolower(unit[0])) {
			case 'b': // Bytes
				result /= 1024;
				break;
			case 't':
				result *= 1024;
			case 'g':
				result *= 1024;
			case 'k':
				// if (std::tolower(unit[1]) != 'b') {
				// 	if (std::tolower(unit[1]) != 'i' || std::tolower(unit[2]) != 'b')
				// 		throw std::invalid_argument("Invalid argument: argument 'unit' is wrong");
				// }
				break;

			default:
				throw std::invalid_argument("Invalid argument: argument 'unit' is wrong");
		}
	}

	return static_cast<unsigned long long>(std::llround(result));
}

/**
 * @brief		메모리 정보 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 25. 20:07:14
 * @return		Map<std::string, unsigned long long> 형태의 메모리 정보 (KiB 단위)
 * @see			SystemInfo::getCpuInfo()
 * @see			SystemInfo::getMemoryInfo()
 * @see			SystemInfo::getDiskInfo()
 * @see			SystemInfo::getNetworkInfo()
 */
std::shared_ptr<std::map<std::string, unsigned long long>>
SystemInfo::getMemoryInfo() {
	std::shared_ptr<std::map<std::string, unsigned long long>> result
		= std::make_shared<std::map<std::string, unsigned long long>>();
	std::ifstream stat_file("/proc/meminfo");
	if (stat_file.is_open()) {
		for (std::string line; std::getline(stat_file, line); ) {
			std::vector<std::string> v;
			boost::split(v, line, boost::is_any_of(":\t "), boost::token_compress_on);
			if (v.size() < 2)
				continue;

			if (v[0].find("MemTotal") == 0)
				(*result.get())["total"] = string2ull(v[1], (v.size() < 3 ? NULL : v[2].c_str()));
			else if (v[0].find("MemFree") == 0)
				(*result.get())["free"] = string2ull(v[1], (v.size() < 3 ? NULL : v[2].c_str()));
			else if (v[0].find("Active") == 0)
				(*result.get())["active"] = string2ull(v[1], (v.size() < 3 ? NULL : v[2].c_str()));
			else if (v[0].find("Inactive") == 0)
				(*result.get())["inactive"] = string2ull(v[1], (v.size() < 3 ? NULL : v[2].c_str()));
		}
	} else
		throw std::runtime_error("Cannot read Memory information");
	stat_file.close();

	return result;
}

/**
 * @brief		디스크 사용량 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 26. 16:11:42
 * @return		Map<std::string, unsigned long long> 형태의 사용량 정보 (KB/s 단위)
 * @see			SystemInfo::getCpuInfo()
 * @see			SystemInfo::getMemoryInfo()
 * @see			SystemInfo::getDiskInfo()
 * @see			SystemInfo::getNetworkInfo()
 */
std::shared_ptr<std::map<std::string, unsigned long long>>
SystemInfo::getDiskInfo() {
	std::shared_ptr<std::map<std::string, unsigned long long>> result
		= std::make_shared<std::map<std::string, unsigned long long>>();
	// FIXME: df 외에는 방법이 없는가?
	char buffer[128];
	std::string disk_info = "";
	std::shared_ptr<FILE> pipe(popen("/bin/df -k . | /usr/bin/tail -1", "r"), pclose);
	if (!pipe)
		throw std::runtime_error("Cannot read Disk information");

	while (!std::feof(pipe.get())) {
		if (std::fgets(buffer, 128, pipe.get()) != NULL)
			disk_info += buffer;
	}

	std::vector<std::string> v;
	boost::split(v, disk_info, boost::is_any_of(":\t "), boost::token_compress_on);
	if (v.size() < 2)
		throw std::runtime_error("Cannot read Disk information");

	int idx = 1;

	(*result.get())["total"] = std::stoull(v[idx]);
	(*result.get())["used"] = std::stoull(v[idx + 1]);

	return result;
}

/**
 * @brief		네트워크 트래픽 정보 반환 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 26. 16:11:42
 */
static inline std::shared_ptr<std::map<std::string, NetworkTraffic>> __getNetworkInfo() {
	std::shared_ptr<std::map<std::string, NetworkTraffic>> result
		= std::make_shared<std::map<std::string, NetworkTraffic>>();
	std::ifstream stat_file("/proc/net/dev");
	if (stat_file.is_open()) {
		for (std::string line; std::getline(stat_file, line); ) {
			std::vector<std::string> v;
			boost::split(v, line, boost::is_any_of("\t "), boost::token_compress_on);
			if (v.size() < 2)
				continue;

			int idx = 0;
			if (v[idx].size() < 2)
				idx = 1;

			if (v[idx].find(":") == std::string::npos || v[idx].compare("lo:") == 0)
				continue;

			std::string name = v[idx].substr(0, v[idx].size() - 1);
			try {
				unsigned long long rx = std::stoull(v[idx + 1]) / 1024;
				unsigned long long tx = std::stoull(v[idx + 9]) / 1024;
				(*result.get())[name] = NetworkTraffic(rx, tx);
			} catch(std::exception &e) {
				// FIXME: 무시해야 하는지?
				(*result.get())[name] = NetworkTraffic();
			}
		}
	} else
		throw std::runtime_error("Cannot read Network information");
	stat_file.close();

	return result;
}

/**
 * @brief		네트워크 트래픽 반환 
 * @details		이전 호출부터 현재까지의 송/수신 바이트로 시간당 전송량을 얻기 위해서는 시간으로 나누어야 한다.
 				최초 호출할 때에는 현재까지의 총 바이트가 표시되므로 무시해도 상관 없다.
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 26. 16:11:42
 * @return		Map<std::string, NetworkTraffic> 형태의 네트워크 트래픽 정보 (KB/s 단위)
 * @see			SystemInfo::getCpuInfo()
 * @see			SystemInfo::getMemoryInfo()
 * @see			SystemInfo::getDiskInfo()
 * @see			SystemInfo::getNetworkInfo()
 */
std::shared_ptr<std::map<std::string, NetworkTraffic>>
SystemInfo::getNetworkInfo(const std::map<std::string, NetworkTraffic> *old_info) {
	std::shared_ptr<std::map<std::string, NetworkTraffic>> result = __getNetworkInfo();
	if (old_info) {
		for (auto net : *result.get()) {
			auto search = old_info->find(net.first);
			if (search != old_info->end()) {
				net.second.rx -= search->second.rx;
				net.second.tx -= search->second.tx;
			} else {
				net.second.rx = 0;
				net.second.tx = 0;
			}
		}

		// FIXME: 없는 정보는 이전 데이터 사용해야 하는지?
	}

	return result;
}
