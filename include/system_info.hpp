/**
 * @headerfile	system_info.hpp "system_info.hpp"
 * @file	system_info.hpp
 * @brief	서버 정보 반환 
 * @details	서버의 CPU 사용률, 메모리 사용량, 디스크 사용량, 네트워크 트래픽 정보를 반환한다.
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 26. 16:16:11
 * @see		
 */
#include <memory>
#include <map>

#ifndef ITFACT_SYSTEM_INFO_HPP
#define ITFACT_SYSTEM_INFO_HPP

namespace itfact {
	namespace common {
		class NetworkTraffic
		{
		public:
			NetworkTraffic(unsigned long long rx, unsigned long long tx) : rx(rx), tx(tx) {};
			NetworkTraffic() : NetworkTraffic(0, 0) {};
			unsigned long long rx;
			unsigned long long tx;

			NetworkTraffic operator+(const NetworkTraffic &rhs);
			NetworkTraffic &operator+=(const NetworkTraffic &rhs);

			NetworkTraffic operator-(const NetworkTraffic &rhs);
			NetworkTraffic &operator-=(const NetworkTraffic &rhs);

			NetworkTraffic operator*(const NetworkTraffic &rhs);
			NetworkTraffic &operator*=(const NetworkTraffic &rhs);

			NetworkTraffic operator/(const NetworkTraffic &rhs);
			NetworkTraffic &operator/=(const NetworkTraffic &rhs);

			NetworkTraffic operator*(unsigned long long ratio);
			NetworkTraffic &operator*=(unsigned long long ratio);

			NetworkTraffic operator/(unsigned long long ratio);
			NetworkTraffic &operator/=(unsigned long long ratio);
		};

		class SystemInfo
		{
		public:
			static std::shared_ptr<std::map<std::string, unsigned long long>> getCpuInfo();
			static std::shared_ptr<std::map<std::string, unsigned long long>> getMemoryInfo();
			static std::shared_ptr<std::map<std::string, unsigned long long>> getDiskInfo();
			static std::shared_ptr<std::map<std::string, NetworkTraffic>>
			getNetworkInfo(const std::map<std::string, NetworkTraffic> *old_info = NULL);
		};
	}
}
#endif /* ITFACT_SYSTEM_INFO_HPP */
