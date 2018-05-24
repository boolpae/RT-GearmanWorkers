/**
 * @file	comfiguration.cc
 * @brief	설정 API
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 12. 17:03:16
 * @see		
 */

#include <exception>
#include <fstream>
// #include <regex>
#include <regex.h>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>

#include "configuration.hpp"

using namespace itfact::common;

Configuration::Configuration() {
}

Configuration::Configuration(const int argc, const char *argv[]) : Configuration() {
	if (configure(argc, argv)) 
		throw std::invalid_argument("Invalid argument");
}

Configuration::~Configuration() {
	// log4cpp::Appender *tmp_appender = NULL;
	// while ((tmp_appender = logger.getAppender()) != NULL) {
	//  	logger.removeAppender(tmp_appender);
	// 	tmp_appender->close();
	// 	delete tmp_appender;
	// }

	// delete layout;
	// delete appender;
}

/**
 * @brief		로그 설정 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 20. 16:05:41
 * @param[in]	level		Log level
 * @param[in]	pathname	Log file including path
 * @param[in]	max_size	max file size
 * @param[in]	max_backup	max number of backup
 */
void
Configuration::initializeLog(const std::string &level, const std::string *pathname,
							 const int max_size, const int max_backup) {
	using namespace log4cpp;
	std::map<std::string, int> mode;
	mode["FATAL"] = Priority::FATAL;
	mode["ALERT"] = Priority::ALERT;
	mode["CRITICAL"] = Priority::CRIT;
	mode["ERROR"] = Priority::ERROR;
	mode["WARNING"] = Priority::WARN;
	mode["NOTICE"] = Priority::NOTICE;
	mode["INFO"] = Priority::INFO;
	mode["DEBUG"] = Priority::DEBUG;

	if (pathname && !pathname->empty() && pathname->compare("stdout") != 0) {
		if (pathname->rfind("/") != std::string::npos) {
			std::string path(pathname->substr(0, pathname->rfind("/")));
			if (!checkPath(path, true))
				return;

			if (path.at(path.size() - 1) != '/')
				path.push_back('/');
		}

		// appender = std::make_shared<RollingFileAppender>(std::string("file"), *pathname, max_size, max_backup);
		appender = new RollingFileAppender(std::string("file"), *pathname, max_size, max_backup);
	} else
		// appender = std::make_shared<OstreamAppender>(std::string("console"), &std::cout);
		appender = new OstreamAppender(std::string("console"), &std::cout);

	layout = new PatternLayout();
	try {
		((PatternLayout *) layout)->setConversionPattern("[%d{%Y-%m-%d %H:%M:%S.%l}] %-6p %m%n");
		appender->setLayout(layout);
	} catch (ConfigureFailure &e) {
		if (layout)
			delete layout;
		layout = new BasicLayout();
		appender->setLayout(layout);
	}

	auto search = mode.find(level);
    if(search != mode.end())
		logger.setPriority(search->second);
    else
		logger.setPriority(Priority::ERROR);
	logger.setAppender(appender);
	// logger.setAppender(appender.get());
	mode.clear();
}

/**
 * @brief		데몬 환경 설정
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 20. 9:46:14
 * @param[in]	argc	인수의 개수 
 * @param[in]	argc	인수 배열 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int Configuration::configure(const int argc, const char *argv[]) {
	using namespace boost::program_options;
	std::string verbose;
	std::string config_file;
	std::string log_file;

	// 파라미터 분석 
	options_description desc("Options");
	desc.add_options()
		("help", "Options related to the program.")
		("daemon,d", "Daemonize, detach and run in the background")
		("host,h", value<std::string>(&host)->default_value(host),
			"Master server hostname")
		("port,p", value<unsigned long>(&port)->default_value(port),
			"Master server port")
		("timeout,u", value<long>(&timeout)->default_value(timeout),
			"Timeout in milliseconds")
		("thread,n", value<unsigned long>(&threads)->default_value(threads),
			"Number of threads to use")
		("config-file,i", value<std::string>(&config_file),
			"Configuration file, Read a file to configure")
		("log-file,l", value<std::string>(&log_file),
			"Log file to write errors and information. "
			"If this parameter is not setting(including configuration file if you set), "
			"then output will go to 'stderr' or 'stdout'(aka consol). "
			"If the syslog parameter is set, then this parameter is ignored.")
		("syslog", "Use syslog, not implemented yet") // FIXME: 미구현 
		("verbose", value<std::string>(&verbose)->default_value("INFO"),
		 "Set verbose level (FATAL, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG)")
		;
	variables_map vm;
	try {
		store(parse_command_line(argc, const_cast<char **>(argv), desc), vm);
		notify(vm);
	} catch(std::exception &e) {
		std::cout << desc << std::endl;
		return EXIT_FAILURE;
	}

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		exit(EXIT_SUCCESS);
	}

	// 설정 파일 분석 
	int max_size = -1, max_backup = 0;
	if (vm.count("config-file")) {
		std::string log_max;
		options_description file_desc("Options");
		file_desc.add_options()
			("master.host", "Master server host")
			("master.port", "Master server port")
			("master.timeout", "Timeout in milliseconds")
			("log.logfile", "Log file")
			("log.max_size", value<std::string>(&log_max)->default_value("1MiB"), "Max size")
			("log.max_backup", value<int>(&max_backup)->default_value(5), "Max number of bcakup")
			;

		try {
			std::fstream fs(config_file);
			auto parsed_options = parse_config_file(fs, file_desc, true);
			fs.close();
			store(parsed_options, vm);
			notify(vm);

			max_size = std::stoul(log_max.c_str()) * itfact::common::convertUnit(log_max);
			for (const auto& o : parsed_options.options) {
				if (file_desc.find_nothrow(o.string_key, true) != NULL) {
					if (o.string_key.find("master.") == 0 || o.string_key.find("log.") == 0) {
						std::string item(o.string_key.substr(o.string_key.rfind(".") + 1));
						if (vm[item].defaulted()) {
							if (item.compare("host") == 0)
								host = std::string(o.value[0]);
						} else if (item.compare("logfile") == 0) {
							if (log_file.empty())
								log_file = std::string(o.value[0]);
						}
					}
					continue;
				}

				if (vm.find(o.string_key) == vm.end())
					config[o.string_key] = std::string(o.value[0]);
			}
		} catch(std::exception &e) {
			std::cout << e.what() << std::endl;
			throw std::invalid_argument("Cannot read config file");
		}
	}

	if (!log_file.empty())
		initializeLog(verbose, &log_file, max_size, max_backup);
	else
		initializeLog(verbose);

	if (vm.count("daemon")) {
		// Daemonize
		logger.info("Detach and run in the background");
		int pid = fork();
		if (likely(pid > 0)) {
			exit(EXIT_SUCCESS);
		} else if (unlikely(pid < 0)) {
			pid = -errno;
			fprintf(stderr, "ERROR[%d]: fork()\n", -pid);
			return pid;
		}

		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
		setsid();
	}

	return EXIT_SUCCESS;
}

/**
 * @brief		설정값 파싱 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 14. 10:13:04
 * @param[in]	Name	Param description
 * @return		Key, Value 형태의 맵 리턴 
 * @exception	regex_error 구문 오류 
 */
std::shared_ptr<std::map<std::string, std::string>>
Configuration::parsingConfig(const std::string &pattern, const std::string &text) {
	std::shared_ptr<std::map<std::string, std::string>> result =
		std::make_shared<std::map<std::string, std::string>>();
	size_t patten_idx = 0;
	size_t text_idx = 0;
	size_t idx;

	while (patten_idx < pattern.size()) {
		switch (pattern[patten_idx]) {
		case '}':
			// ERROR
			throw std::invalid_argument("the expression contains mismatched curly braces ('{' and '}')");
			// throw std::regex_error(std::regex_constants::error_brace);

		// case ']':
		// 	// ERROR
		// 	throw std::regex_error(std::regex_constants::error_brack);

		case '*':
			do {
				if (++patten_idx >= pattern.size())
					return result;
			} while (pattern[patten_idx] == '*');

			idx = text.find(pattern[patten_idx], text_idx);
			if (idx == std::string::npos)
				throw std::invalid_argument("the expression contains an invalid character class name");
				// throw std::regex_error(std::regex_constants::error_ctype);
			text_idx = idx;
			break;

		default:
			if (text_idx >= text.size())
				throw std::invalid_argument("the expression contains an invalid character class name");

			// 문자열 일치 
			if (pattern[patten_idx++] != text[text_idx++])
				throw std::invalid_argument("the expression contains an invalid character class name");
				// throw std::regex_error(std::regex_constants::error_ctype);
			break;

		// case '[':
		// 	// 생략 가능한 변수 
		// 	int end_idx = pattern.find("]", ++patten_idx);
		// 	if (end_idx == std::string::npos)
		// 		throw std::regex_error(std::regex_constants::error_brack);

		// 	std::string var_name = pattern.substr(patten_idx, end_idx - patten_idx);
		// 	patten_idx = end_idx + 1;

		// 	idx = var_name.find(":");
		// 	int size = 0;
		// 	if (idx != std::string::npos) {
		// 		size = std::stoi(var_name.substr(idx + 1));
		// 		var_name = var_name.substr(0, idx);
		// 	} else if (patten_idx >= pattern.size()) {
		// 		size = text.size() - text_idx;
		// 	} else {
		// 		if (pattern[patten_idx] == '[')
		// 			throw std::regex_error(std::regex_constants::error_complexity);

		// 		idx = text.find(pattern[patten_idx], text_idx);
		// 		if (idx == std::string::npos)
		// 			throw std::regex_error(std::regex_constants::error_ctype);

		// 		size = idx - text_idx;
		// 	}

		// 	std::string value = text.substr(text_idx, size);
		// 	text_idx += size;

		// 	(*result.get())[var_name] = value;
		// 	break;

		case '{':
			std::string::size_type end_idx = pattern.find("}", ++patten_idx);
			if (end_idx == std::string::npos)
				throw std::invalid_argument("the expression contains mismatched curly braces ('{' and '}')");
				// throw std::regex_error(std::regex_constants::error_brace);

			std::string var_name = pattern.substr(patten_idx, end_idx - patten_idx);
			patten_idx = end_idx + 1;

			idx = var_name.find(":");
			int size = 0;
			if (idx != std::string::npos) {
				size = std::stoi(var_name.substr(idx + 1));
				var_name = var_name.substr(0, idx);
			} else if (patten_idx >= pattern.size()) {
				size = text.size() - text_idx;
			} else {
				if (pattern[patten_idx] == '{')
					throw std::invalid_argument("the complexity of an attempted match exceeded a predefined level");
					// throw std::regex_error(std::regex_constants::error_complexity);

				idx = text.find(pattern[patten_idx], text_idx);
				if (idx == std::string::npos)
				throw std::invalid_argument("the expression contains an invalid character class name");
					// throw std::regex_error(std::regex_constants::error_ctype);

				size = idx - text_idx;
			}

			std::string value = text.substr(text_idx, size);
			text_idx += size;

			(*result.get())[var_name] = value;
			break;
		}
	}
	return result;
}

/**
 * @brief		디렉토리의 존재 유무를 검사 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 22. 13:15:18
 * @param[in]	pathname	경로명
 * @retval		true	존재하는 디렉토리 
 * @retval		false	존재하지 않는 디렉토리 
 */
bool itfact::common::checkPath(const std::string &path, const bool create) {
	bool exists = true;
	if (!path.empty()) {
		try {
			if (!boost::filesystem::exists(path)) {
				if (create)
					boost::filesystem::create_directories(path);
				else
					exists = false;
			}
		} catch (std::exception &e) {
			return false;
		}
	}
	return exists;
}

static const char *pattern = "([0-9]+)";
static std::map<std::string, unsigned long>
units = {{"GiB", 1024 * 1024 * 1024},
		 {"MiB", 1024 * 1024},
		 {"KiB", 1024},
		 {"B", 1},
		 {"Y", 365},
		 {"M", 30},
		 {"D", 1},
		 {"h", 60 * 60 * 1000},
		 {"m", 60 * 1000},
		 {"s", 1000},
		 {"ms", 1}};
/**
 * @brief		기본 단위로 환산 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 13. 10:08:34
 * @param[in]	value	단위를 포함한 값 
 * @return		기본 단위로 변환된 값 
 * @see			SeeConfiguration::getConfig()
 * @exception	regex_error	값의 형식이 맞지 않은 경우 발생 
 */
unsigned long itfact::common::convertUnit(const std::string &value) {
	regex_t _reg;
	regmatch_t matches;

	int rc = regcomp(&_reg, pattern, REG_EXTENDED);
	if (rc != 0) {
		char error_str[1024];
		regerror(rc, &_reg, error_str, 1024);
		throw std::runtime_error(std::string(error_str));
	}
	std::shared_ptr<regex_t> reg(&_reg, regfree);

	// std::string number(value);
	rc = regexec(reg.get(), value.c_str(), 1, &matches, 0);
	if (rc == 0) {
		if (static_cast<long>(value.size()) > matches.rm_eo) {
			// number = value.substr(matches.rm_so, matches.rm_eo);
			std::string unit = value.substr(matches.rm_eo);
			auto search = units.find(unit);
			if (search != units.end())
				return static_cast<unsigned long>(search->second);
		}
	}

	return 1;
}

/**
 * @brief		설정 여부 확인 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 07. 14. 12:51:29
 * @param[in]	key	설정 항목 
 * @retval		true	설정된 항목 
 * @retval		true	설정되지 않은 항목 
 */
bool Configuration::isSet(const std::string &key) {
	return (static_cast<const Configuration *>(this))->isSet(key);
}
bool Configuration::isSet(const std::string &key) const {
	auto search = config.find(key);
	if (search != config.end())
		return true;
	return false;
}

/**
 * @brief		환경 설정값 로드 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 07. 13:08:55
 * @param[in]	key				설정 항목 
 * @param[in]	default_value	기본값 
 * @return		해당하는 설정이 존재하면 설정 값을 반환하고,
 				해당하는 설정이 존재하지 않은 경우 default_value값을 반환하며,
 				default_value값이 없는 경우 빈 문자열을 반환한다.
 * @exception	regex_error	값의 형식이 맞지 않은 경우 발생 
 * @exception	bad_cast	지원하지 않는 타입인 경우 발생 
 * @exception	logic_error	해당하는 설정이 없으며 기본값이 지정되지 않은 경우 발생 
 */
namespace itfact {
	namespace common {
		template <>
		bool Configuration::getConfig<bool>(const std::string &key, const bool default_value) const {
			auto search = config.find(key);
			if (search != config.end()) {
				if (search->second.compare("true") == 0)
					return true;
				else
					return false;
			} else
				return default_value;
		}
	}
}

std::string Configuration::getConfig(const std::string &key, const char *default_value) const {
	auto search = config.find(key);
	if (search != config.end()) {
		// if (search->second[0] == '@')
		// 	search->second
		return search->second;
	} else if (default_value)
		return std::string(default_value);
	else
		throw std::logic_error(std::string("Cannot find key: ").append(key));
}
std::string Configuration::getConfig(const std::string &key, const char *default_value) {
	return (static_cast<const Configuration *>(this))->getConfig(key, default_value);
}

void Configuration::verbose() const {
	for (auto cur : config)
		logger.debug("%s: %s", cur.first.c_str(), cur.second.c_str());
}
void Configuration::verbose() {
	return (static_cast<const Configuration *>(this))->verbose();
}