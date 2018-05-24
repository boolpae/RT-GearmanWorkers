/**
 * @file	restapi.cc
 * @brief	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 08. 17:21:03
 * @see		
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <thread>
#include <exception>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "restapi.hpp"
#include "v1/restapi_v1.hpp"

using namespace itfact::vr::node;

static log4cpp::Category *req_logger;

static struct {
	std::string service_name;
	long port;
	long connection_limits;
	long connection_timeout;
} default_config = {
	.service_name = "vr",
	.port = 3001,
	.connection_limits = 1,
	.connection_timeout = 10000
};

RestApi::RestApi(const itfact::common::Configuration *server_config, log4cpp::Category *logger) {
	config = server_config;
	req_logger = logger;
	service_name = config->getConfig("api.service", default_config.service_name.c_str());
	req_logger->info("Initialize service: %s", service_name.c_str());

	// 각 버전 등록 
	try {
		versions["v1.0"] = std::make_shared<v1::RestApiV1>(logger);
	} catch (std::exception &e) {
		req_logger->warn("Cannot register: v1.0: %s", e.what());
	}
}

RestApi::~RestApi() {
	stop();
}

/**
 * @brief		REST API 데몬 시작 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 08. 17:31:45
 * @param[in]	port	포트 번호
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 * @see			RestApi::stop()
 */
int RestApi::start() {
	long port = config->getConfig("api.port", default_config.port);
	req_logger->info("HTTP server listening on port %d", port);
	long connection_limits = config->getConfig("api.limits", default_config.connection_limits);
	long connection_timeout = config->getConfig("api.timeout", default_config.connection_timeout);
	req_logger->debug("Connection limits: %d, timeout: %d", connection_limits, connection_timeout);
	descriptor = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, // MHD_USE_SSL
								  port,
								  NULL, this,
								  &request_handler, this,
								  // MHD_OPTION_HTTPS_MEM_KEY, , NULL,
								  // MHD_OPTION_HTTPS_MEM_CERT, , NULL,
								  MHD_OPTION_CONNECTION_LIMIT, connection_limits, NULL,
								  MHD_OPTION_CONNECTION_TIMEOUT, connection_timeout, NULL,
								  MHD_OPTION_END);
	if (descriptor == NULL) {
		req_logger->error("Fail to start HTTP server on port %d", port);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/**
 * @brief		REST API 데몬 종료 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 11. 11:43:38
 */
void RestApi::stop() {
	if (descriptor)
		MHD_stop_daemon(descriptor);
}

static const std::map<std::string, enum HTTP_METHOD>
method_list =  {{MHD_HTTP_METHOD_GET, HTTP_GET},
				{MHD_HTTP_METHOD_POST, HTTP_POST},
				{MHD_HTTP_METHOD_PUT, HTTP_PUT},
				{"PATCH", HTTP_PATCH},
				{"patch", HTTP_PATCH},
				// {MHD_HTTP_METHOD_PATCH, HTTP_PATCH},
				{MHD_HTTP_METHOD_DELETE, HTTP_DELETE}};

/**
 * @brief		Request handler
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 11. 10:41:55
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int RestApi::request_handler(void *cls,
							 struct MHD_Connection *connection,
							 const char *url,
							 const char *method,
							 const char *version,
							 const char *upload_data,
							 size_t *upload_data_size,
							 void **con_cls) {
	RestApi *daemon = (RestApi *) cls;
	int rc = EXIT_SUCCESS;
	std::string name(COLOR_BLACK_BOLD);
	name.append("request:");
	name.append(boost::lexical_cast<std::string>(std::hash<std::thread::id>()(std::this_thread::get_id())));
	name.append(COLOR_NC);

	req_logger->info("[%s] %s %s %s", name.c_str(), method, url, version);
	if (*upload_data_size > 0)
		req_logger->debug("[%s] %s", name.c_str(), std::string(upload_data, *upload_data_size).c_str());

	if (std::strlen(url) <= 1) {
		RestApi::sendBadRequest(connection, "잘못된 요청입니다.");
		req_logger->error("[%s] Bad request", name.c_str());
		return MHD_NO;
	}

	auto search = method_list.find(method);
	if (search == method_list.end()) {
		RestApi::sendUnsupportedMethod(connection, "허용되지 않은 메소드입니다.");
		return MHD_NO;
	}

	std::vector<std::string> url_parse;
	boost::split(url_parse, url, boost::is_any_of("/"));
	if (url_parse.size() <= 3) {
		req_logger->error("[%s] Bad request: Cannot find resource", name.c_str());
		RestApi::sendBadRequest(connection, "잘못된 요청입니다.");
		return MHD_NO;
	}

	if (daemon->getServiceName().compare(url_parse[1]) != 0) {
		req_logger->error("[%s] Invalid service: %s", name.c_str(), url_parse[1].c_str());
		RestApi::sendBadRequest(connection, "지원하지 않는 서비스입니다.");
		return MHD_NO;
	}

	std::shared_ptr<Version> api = daemon->getVersion(url_parse[2]);
	if (!api) {
		req_logger->error("[%s] Unsupported version: %s", name.c_str(), url_parse[2].c_str());
		RestApi::sendBadRequest(connection, "지원하지 않는 버전입니다.");
		return MHD_NO;
	}

	// /vr/{version}/{resource}
	rc = api->handleRequest(name.c_str(), connection, &url_parse[3],
							(url_parse.size() > 4 ? &url_parse[4] : NULL),
							search->second, upload_data, upload_data_size, con_cls);
	if (rc == EXIT_SUCCESS)
		return MHD_YES;
	return MHD_NO;
}

/**
 * @brief		Response 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 20:15:16
 * @param[in]	connection	연결 정보 
 * @param[in]	body		응답 메시지 
 * @param[in]	body_size	응답 메시지 크기 
 * @retval		true	전송 성공 
 * @retval		false	전송 실패 
* @see			
 */
bool RestApi::response(struct MHD_Connection *connection,
					  const char *body, size_t body_size) {
	req_logger->debug("Response < %s", body);
	struct MHD_Response *resp =
		MHD_create_response_from_buffer(body_size, (void *) body, MHD_RESPMEM_MUST_COPY);
	if (!resp)
		return false;

	std::shared_ptr<struct MHD_Response> res(resp, MHD_destroy_response);
	if (MHD_add_response_header(res.get(), "Content-Type", "application/json; charset=utf-8") != MHD_YES)
		return false;

	int rc = MHD_queue_response(connection, MHD_HTTP_OK, res.get());
	if (rc == MHD_YES)
		return true;
	return false;
}

/**
 * @brief		Send bad request message
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 14:26:20
 * @param[in]	connection		연결 정보 
 * @param[in]	detail_message	오류 내용 
 * @retval		true	전송 성공 
 * @retval		false	전송 실패 
 * @see			
 */
bool RestApi::sendBadRequest(struct MHD_Connection *connection,
							 const std::string &detail_message) {
	std::string json("{\"message\": \"");
	json.append("Bad Request");
	json.append("\", \"detail\": \"");
	json.append(detail_message);
	json.append("\"}");

	struct MHD_Response *resp =
		MHD_create_response_from_buffer(json.size(), (void *) json.c_str(), MHD_RESPMEM_MUST_COPY);
	if (!resp)
		return false;

	std::shared_ptr<struct MHD_Response> res(resp, MHD_destroy_response);
	if (MHD_add_response_header(res.get(), "Content-Type", "application/json; charset=utf-8") != MHD_YES)
		return false;

	int rc = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, res.get());
	if (rc == MHD_YES)
		return true;
	return false;
}

/**
 * @brief		Send not found message
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 14:34:34
 * @param[in]	connection		연결 정보 
 * @param[in]	detail_message	오류 내용 
 * @retval		true	전송 성공 
 * @retval		false	전송 실패 
 * @see			
 */
bool RestApi::sendNotFound(struct MHD_Connection *connection,
						  const std::string &detail_message) {
	std::string json("{\"message\": \"");
	json.append("Not Found");
	json.append("\", \"detail\": \"");
	json.append(detail_message);
	json.append("\"}");

	struct MHD_Response *resp =
		MHD_create_response_from_buffer(json.size(), (void *) json.c_str(), MHD_RESPMEM_MUST_COPY);
	if (!resp)
		return false;

	std::shared_ptr<struct MHD_Response> res(resp, MHD_destroy_response);
	if (MHD_add_response_header(res.get(), "Content-Type", "application/json; charset=utf-8") != MHD_YES)
		return false;

	int rc = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, res.get());
	if (rc == MHD_YES)
		return true;
	return false;
}

/**
 * @brief		Send unsupported HTTP method
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 15:47:08
 * @param[in]	connection		연결 정보 
 * @param[in]	detail_message	오류 내용 
 * @retval		true	전송 성공 
 * @retval		false	전송 실패 
 * @see			
 */
bool RestApi::sendUnsupportedMethod( struct MHD_Connection *connection,
									const std::string &detail_message) {
	std::string json("{\"message\": \"");
	json.append("Not Acceptable");
	json.append("\", \"detail\": \"");
	json.append(detail_message);
	json.append("\"}");

	struct MHD_Response *resp =
		MHD_create_response_from_buffer(json.size(), (void *) json.c_str(), MHD_RESPMEM_MUST_COPY);
	if (!resp)
		return false;

	std::shared_ptr<struct MHD_Response> res(resp, MHD_destroy_response);
	if (MHD_add_response_header(res.get(), "Content-Type", "application/json; charset=utf-8") != MHD_YES)
		return false;

	int rc = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ACCEPTABLE, res.get()); // MHD_HTTP_METHOD_NOT_ALLOWED
	if (rc == MHD_YES)
		return true;
	return false;
}

/**
 * @brief		Send internal server error
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 21. 14:36:30
 * @param[in]	connection		연결 정보 
 * @param[in]	detail_message	오류 내용 
 * @retval		true	전송 성공 
 * @retval		false	전송 실패 
 * @see			
 */
bool RestApi::sendInternalServerError(struct MHD_Connection *connection,
									 const std::string &detail_message) {
	std::string json("{\"message\": \"");
	json.append("Internal Server Error");
	json.append("\", \"detail\": \"");
	json.append(detail_message);
	json.append("\"}");

	struct MHD_Response *resp =
		MHD_create_response_from_buffer(json.size(), (void *) json.c_str(), MHD_RESPMEM_MUST_COPY);
	if (!resp)
		return false;

	std::shared_ptr<struct MHD_Response> res(resp, MHD_destroy_response);
	if (MHD_add_response_header(res.get(), "Content-Type", "application/json; charset=utf-8") != MHD_YES)
		return false;

	int rc = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, res.get());
	if (rc == MHD_YES)
		return true;
	return false;
}

/**
 * @brief		get API
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 11. 17:17:52
 * @param[in]	version	버전
 * @return		
 * @retval		NULL	일치하는 버전이 없음 
 */
std::shared_ptr<Version> RestApi::getVersion(const std::string &version) const {
	auto search = versions.find(version);
	if (search != versions.end())
		return search->second;

	return NULL;
}
std::shared_ptr<Version> RestApi::getVersion(const std::string &version) {
	return (static_cast<const RestApi *>(this))->getVersion(version);
}
