/**
 * @headerfile	configuration.hpp "configuration.hpp"
 * @file	configuration.hpp
 * @brief	설정 API 
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 12. 16:59:56
 * @see		
 */
#ifndef ITFACT_COMMON_CONFIGRATION_HPP
#define ITFACT_COMMON_CONFIGRATION_HPP

#include <exception>
#include <map>
#include <memory>
#include <string>

#include <log4cpp/Appender.hh>
#include <log4cpp/Category.hh>

#define COLOR_NC			"\033[00m"

#define COLOR_BLACK			"\033[00;30m"
#define COLOR_RED			"\033[00;31m"
#define COLOR_GREEN			"\033[00;32m"
#define COLOR_YELLOW		"\033[00;33m"
#define COLOR_BLUE			"\033[00;34m"
#define COLOR_MAGENTA		"\033[00;35m"
#define COLOR_CYAN			"\033[00;36m"
#define COLOR_WHITE			"\033[00;37m"

#define COLOR_BLACK_BOLD	"\033[01;30m"
#define COLOR_RED_BOLD		"\033[01;31m"
#define COLOR_GREEN_BOLD	"\033[01;32m"
#define COLOR_YELLOW_BOLD	"\033[01;33m"
#define COLOR_BLUE_BOLD		"\033[01;34m"
#define COLOR_MAGENTA_BOLD	"\033[01;35m"
#define COLOR_CYAN_BOLD		"\033[01;36m"
#define COLOR_WHITE_BOLD	"\033[01;37m"

#define COLOR_BLACK_LIGHT	"\033[02;30m"
#define COLOR_RED_LIGHT		"\033[02;31m"
#define COLOR_GREEN_LIGHT	"\033[02;32m"
#define COLOR_YELLOW_LIGHT	"\033[02;33m"
#define COLOR_BLUE_LIGHT	"\033[02;34m"
#define COLOR_MAGENTA_LIGHT	"\033[02;35m"
#define COLOR_CYAN_LIGHT	"\033[02;36m"
#define COLOR_WHITE_LIGHT	"\033[02;37m"

#define COLOR_BLACK_UNDER	"\033[04;30m"
#define COLOR_RED_UNDER		"\033[04;31m"
#define COLOR_GREEN_UNDER	"\033[04;32m"
#define COLOR_YELLOW_UNDER	"\033[04;33m"
#define COLOR_BLUE_UNDER	"\033[04;34m"
#define COLOR_MAGENTA_UNDER	"\033[04;35m"
#define COLOR_CYAN_UNDER	"\033[04;36m"
#define COLOR_WHITE_UNDER	"\033[04;37m"

#define COLOR_BLACK_BLINK	"\033[05;30m"
#define COLOR_RED__BLINK	"\033[05;31m"
#define COLOR_GREEN_BLINK	"\033[05;32m"
#define COLOR_YELLOW_BLINK	"\033[05;33m"
#define COLOR_BLUE_BLINK	"\033[05;34m"
#define COLOR_MAGENTA_BLINK	"\033[05;35m"
#define COLOR_CYAN_BLINK	"\033[05;36m"
#define COLOR_WHITE_BLINK	"\033[05;37m"

#define THREAD_ID	std::this_thread::get_id()
#define LOG_INFO	__FILE__, __FUNCTION__, __LINE__
#define LOG_FMT		" [at %s (%s:%d)]"

#ifndef likely
#define likely(x)	__builtin_expect((x), 1)
#endif
#ifndef unlikely
#define unlikely(x)	__builtin_expect((x), 0)
#endif

namespace itfact {
	namespace common {
		unsigned long convertUnit(const std::string &value);
		bool checkPath(const std::string &path, const bool create = false);

		class Configuration
		{
		private: // Member
			std::map<std::string, std::string> config;
			std::string host = "localhost";
			unsigned long port = 4730;
			long timeout = -1;
			unsigned long threads = 10;
			log4cpp::Category &logger = log4cpp::Category::getRoot();
			// std::shared_ptr<log4cpp::Appender> appender;
			log4cpp::Appender *appender = NULL;
			log4cpp::Layout *layout = NULL;

		public:
			Configuration();
			Configuration(const int argc, const char *argv[]);
			~Configuration();
			int configure(const int argc, const char *argv[]);

			bool isSet(const std::string &key);
			bool isSet(const std::string &key) const;
			std::string getConfig(const std::string &key, const char *default_value = NULL);
			std::string getConfig(const std::string &key, const char *default_value = NULL) const;

			template <typename T>
			T getConfig(const std::string &key, const T default_value) const {
				auto search = config.find(key);
				if (search != config.end()) {
					unsigned long unit = itfact::common::convertUnit(search->second);
					if (typeid(T) == typeid(int))
						return std::stoi(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(long))
						return std::stol(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(unsigned long))
						return std::stoul(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(long long))
						return std::stoll(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(unsigned long long))
						return std::stoull(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(double))
						return std::stod(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(long double))
						return std::stold(search->second.c_str()) * static_cast<T>(unit);
					else if (typeid(T) == typeid(float))
						return std::stof(search->second.c_str()) * static_cast<T>(unit);
					else
						throw std::bad_cast();
				} else
					return default_value;
			}

			template <typename T>
			T getConfig(const std::string &key, const T default_value) {
				return (static_cast<const Configuration *>(this))->getConfig<T>(key, static_cast<T>(default_value));
			}

			template <typename T>
			T getConfig(const std::string &key, const T *default_value = NULL) const {
				T value = default_value ? *default_value : static_cast<T>(0);
				return (static_cast<const Configuration *>(this))->getConfig<T>(key, value);
			}

			template <typename T>
			T getConfig(const std::string &key, const T *default_value = NULL) {
				return (static_cast<const Configuration *>(this))->getConfig<T>(key, default_value);
			}

			static std::shared_ptr<std::map<std::string, std::string>>
			parsingConfig(const std::string &pattern, const std::string &text);

			log4cpp::Category *getLogger() {return &logger;};
			log4cpp::Category *getLogger() const {return &logger;};
			std::string getHost() {return host;};
			std::string getHost() const {return host;};
			unsigned long getPort() {return port;};
			unsigned long getPort() const {return port;};
			long getTimeout() {return timeout;};
			long getTimeout() const {return timeout;};
			unsigned long getThreads() {return threads;};
			unsigned long getThreads() const {return threads;};

			void verbose();
			void verbose() const;

		private:
			void initializeLog( const std::string &level, const std::string *pathname = NULL,
								const int max_size = 0, const int max_backup = 0);
		};
	}
}

#endif /* ITFACT_COMMON_CONFIGRATION_HPP */
