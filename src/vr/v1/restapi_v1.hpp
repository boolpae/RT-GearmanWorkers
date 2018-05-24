/**
 * @headerfile	version1.hpp "version1.hpp
 * @file	version1.hpp
 * @brief	REST API v1.0
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 11. 15:10:39
 * @see		
 */

#ifndef ITFACT_RESTAPI_V1_0_HPP
#define ITFACT_RESTAPI_V1_0_HPP

#include "../restapi.hpp"

namespace itfact {
	namespace vr {
		namespace node {
			namespace v1 {
				class RestApiV1 : public itfact::vr::node::Version
				{
				public:
					RestApiV1(log4cpp::Category *logger) : itfact::vr::node::Version(logger) {};

					virtual int
					handleRequest(const char *job_name,
								  struct MHD_Connection *connection,
								  const std::string *resource,
								  const std::string *id,
								  const enum HTTP_METHOD method,
								  const char *upload_data,
								  size_t *upload_data_size,
								  void **con_cls) override;

				};

				class Servers : public Request
				{
				private: // member
					static const std::string resource_name;

				public:
					Servers(const char *job_name, struct MHD_Connection *connection,
							const enum HTTP_METHOD method, log4cpp::Category *logger)
						: Request(job_name, connection, method, logger) {};
					int request(const std::string *id,
								const char *upload_data, size_t *upload_data_size, void **con_cls);
					static bool equals (const std::string *resource);

				private:
					int getServerState(const std::string *id);
					std::string getCpuInfo(const char *value = NULL);
					std::string getMemoryInfo();
					std::string getDiskInfo();
					std::string getNetworkInfo(const char *value = NULL);

				};

				class Waves : public Request
				{
				private: // member
					static const std::string resource_name;

				public:
					Waves(const char *job_name, struct MHD_Connection *connection,
						  const enum HTTP_METHOD method, log4cpp::Category *logger)
						: Request(job_name, connection, method, logger) {};
					int request(const std::string *id,
								const char *upload_data, size_t *upload_data_size, void **con_cls);
					static bool equals(const std::string *resource);

				private:
					// int getWave(const std::string *id, void **con_cls);
				};
			}
		}
	}
}

#endif /* ITFACT_RESTAPI_V1_0_HPP */
