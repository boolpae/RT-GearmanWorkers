/**
 * @file	restapi_v1.cc
 * @brief	REST API v1.0
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 12. 09:23:19
 * @see		waves.cc
 */

#include "restapi_v1.hpp"

using namespace itfact::vr::node::v1;

/**
 * @brief		Request 처리 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 12. 09:56:29
 * @param[in]	connection			Request 커넥션 디스크립터 
 * @param[in]	resource			리소스 명 
 * @param[in]	id					ID
 * @param[in]	method				HTTP 메소드 
 * @param[in]	upload_data			업로드 데이터 
 * @param[in]	upload_data_size	업로드 데이터 크기 
 * @param[out]	con_cls				전달할 데이터 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int
RestApiV1::handleRequest(const char *job_name,
						 struct MHD_Connection *connection,
						 const std::string *resource,
						 const std::string *id,
						 const enum HTTP_METHOD method,
						 const char *upload_data,
						 size_t *upload_data_size,
						 void **con_cls) {
	if (!resource) {
		RestApi::sendBadRequest(connection, "리소스 정보가 없습니다.");
		return EXIT_FAILURE;
	}

	req_logger->debug("[%s] Check resource: %s", job_name, resource->c_str());
	std::shared_ptr<Request> request;
	if (resource->compare("command") == 0) {
		req_logger->debug("[%s] Command", job_name);
		return EXIT_FAILURE;
	} else if(Servers::equals(resource)) {
		request = std::make_shared<Servers>(job_name, connection, method, req_logger);
	} else if (Waves::equals(resource)) {
		request = std::make_shared<Waves>(job_name, connection, method, req_logger);
	} else {
		RestApi::sendNotFound(connection, "해당하는 리소스가 없습니다.");
		return EXIT_FAILURE;
	}

	return request->request(id, upload_data, upload_data_size, con_cls);
}
