/**
 * @headerfile	vr.hpp "vr.hpp"
 * @file	vr.hpp
 * @brief	VR Server
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 06. 17. 17:35:46
 * @see		
 */

#ifndef __ITFACT_VR_INOTIFY_H__
#define __ITFACT_VR_INOTIFY_H__

#include <curl/curl.h>
#include "configuration.hpp"

namespace itfact {
	namespace vr {
		namespace node {
			class VRInotify
			{
			private:
				itfact::common::Configuration config;
				CURLM *multi_handle;
				int handle_count;

			public:
				VRInotify(const int argc, const char *argv[]);
				~VRInotify();
				int monitoring();
				static int runJob(const std::shared_ptr<std::string> path,
								  const std::shared_ptr<std::string> filename,
								  const itfact::common::Configuration *config);
				static std::thread::id getFinishedJob();

			private:
				VRInotify();
				//static void waitForFinish(const int max_worker, const int seconds, const int increment = 0);
				static void waitForFinish(const int max_worker, const int seconds, const int increment = 0, const char* filename="none", std::set<std::thread::id> *list = NULL);
				static int processRequest(
					const itfact::common::Configuration *config,
					const char *apiserver_uri, const char *pathname,
					const char *download_path, const std::string &format_string,
					const std::string data, const char *output = NULL, std::set<std::thread::id> *list = NULL);
				static int sendRequest(
					const std::string &id,
					const itfact::common::Configuration *config,
					const char *apiserver_uri,
					const std::string &call_id,
					const std::string &body,
					const std::string &download_uri,
					const char *output_path = NULL, std::set<std::thread::id> *list = NULL);
			};
		}
	}
}

#endif /* __ITFACT_VR_INOTIFY_H__ */
