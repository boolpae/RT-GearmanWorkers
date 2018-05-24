/**
 * @headerfile	restapi.hpp "restapi.hpp"
 * @file	restapi.hpp
 * @brief	RESP API 
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 08. 17:23:23
 * @see		
 */

#include <string>
#include <map>
#include <memory>

#include <microhttpd.h>
#include <log4cpp/Category.hh>
#include <boost/noncopyable.hpp>

#include "configuration.hpp"

#ifndef ITFACT_RESTAPI_HPP
#define ITFACT_RESTAPI_HPP

namespace itfact {
	namespace vr {
		namespace node {
			enum HTTP_METHOD
			{
				HTTP_GET,
				HTTP_POST,
				HTTP_PUT,
				HTTP_PATCH,
				HTTP_DELETE
			};

			class Version : private boost::noncopyable
			{
			protected: // member
				log4cpp::Category *req_logger;

			public:
				Version(log4cpp::Category *logger) {req_logger = logger;};

				virtual int
				handleRequest(const char *job_name,
							  struct MHD_Connection *connection,
							  const std::string *resource,
							  const std::string *id,
							  const enum HTTP_METHOD method,
							  const char *upload_data,
							  size_t *upload_data_size,
							  void **con_cls) = 0;

			private:
				Version();

			};

			class RestApi : private boost::noncopyable
			{
			private: // Member
				struct MHD_Daemon *descriptor = NULL;
				// std::shared_ptr<struct MHD_Daemon> daemon;
				std::string service_name;
				const itfact::common::Configuration *config;
				std::map<std::string, std::shared_ptr<Version>> versions;

			public:
				RestApi(const itfact::common::Configuration *server_config, log4cpp::Category *logger);
				~RestApi();
				int start();
				void stop();

				static int
				request_handler(void *cls,
								struct MHD_Connection *connection,
								const char *url,
								const char *method,
								const char *version,
								const char *upload_data,
								size_t *upload_data_size,
								void **con_cls);

				std::shared_ptr<Version> getVersion(const std::string &Version) const;
				std::shared_ptr<Version> getVersion(const std::string &Version);
				std::string getServiceName() const {return service_name;};
				std::string getServiceName() {return service_name;};

				static bool response(struct MHD_Connection *connection,
									const char *body, size_t body_size);

				static bool sendBadRequest( struct MHD_Connection *connection,
											const std::string &detail_message);
				static bool sendNotFound(struct MHD_Connection *connection,
										const std::string &detail_message);
				static bool sendUnsupportedMethod(struct MHD_Connection *connection,
												 const std::string &detail_message);
				static bool sendInternalServerError( struct MHD_Connection *connection,
													const std::string &detail_message);

			private:
				RestApi();

			};

			class Request
			{
			protected:
				const char *job_name;
				struct MHD_Connection *connection;
				const enum HTTP_METHOD method;
				log4cpp::Category *logger;

			public:
				Request(const char *job_name,
						struct MHD_Connection *connection,
						const enum HTTP_METHOD method,
						log4cpp::Category *logger) : method(method) {
					this->job_name = job_name;
					this->connection = connection;
					this->logger = logger;
				};

				virtual int request(const std::string *id,
									const char *upload_data,
									size_t *upload_data_size,
									void **con_cls) = 0;

				static bool equals(const std::string *resource) {return false;};

			private:
				Request();

			};
		}
	}
}
#endif /* ITFACT_RESTAPI_HPP */
