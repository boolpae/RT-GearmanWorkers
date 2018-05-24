/**
 * @headerfile	worker "worker.hpp"
 * @file	worker
 * @brief	Worker API
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 06. 07. 10:22:45
 * @see		
 */

#ifndef ITF_WORKER_H
#define ITF_WORKER_H

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <boost/noncopyable.hpp>
#include <log4cpp/Category.hh>
#include <libgearman/gearman.h>

#include "configuration.hpp"

namespace itfact {
	namespace worker {

		enum PROTOCOL {
			PROTOCOL_FILE,
			PROTOCOL_MOUNT,	///< 마운트된(로컬파일인식) 파일을 VR서버로 복사하기 위한 프로토콜
			PROTOCOL_HTTP,
			PROTOCOL_HTTPS,
			PROTOCOL_FTP,
			PROTOCOL_FTPS,
			PROTOCOL_SFTP,
			PROTOCOL_NONE
		};

		class WorkerDaemon : private boost::noncopyable
		{
		private: // Member
			std::vector<std::thread> workers;
			bool is_running = false;
			common::Configuration config;
			log4cpp::Category *logger;

		public:
			WorkerDaemon();
			WorkerDaemon(const int argc, const char *argv[]);
			~WorkerDaemon();
			int initialize(const int argc, const char *argv[]);
			virtual int initialize() = 0;
			void stop();

			const common::Configuration *getConfig() {return &config;};
			const common::Configuration *getConfig() const {return &config;};
			log4cpp::Category *getLogger() {return logger;};
			log4cpp::Category *getLogger() const {return logger;};

			unsigned long getTotalWorkers(const std::string &name);
			bool isRunning() {return is_running;};
			bool isRunning() const {return is_running;};
			std::string getHost() {return config.getHost();};
			std::string getHost() const {return config.getHost();};
			in_port_t getPort() {return static_cast<in_port_t>(config.getPort());};
			in_port_t getPort() const {return static_cast<in_port_t>(config.getPort());};
			long getTimeout() {return config.getTimeout();};
			long getTimeout() const {return config.getTimeout();};

			static enum PROTOCOL
			downloadData(const common::Configuration *config,
						 const char *workload, const size_t workload_size,
						 std::vector<short> &buffer, const std::string *account = NULL,
						 log4cpp::Category *logger = &log4cpp::Category::getRoot());
			static CURLcode uploadData( const common::Configuration *config,
										const std::string &uri, const std::string &pathname,
										const std::string *account = NULL,
										log4cpp::Category *logger = &log4cpp::Category::getRoot());

		protected:
			void run(const std::string name, void *context, unsigned int count,
					 gearman_return_t (*fn)(gearman_job_st *, void *));
			void join();

		};
	}
}

#endif /* ITF_WORKER_H */
