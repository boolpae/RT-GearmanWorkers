/**
 * @headerfile	license.hpp "license.hpp"
 * @file	license.hpp
 * @brief	라이선스 API
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 07. 28. 15:22:19
 * @see		
 */

#ifndef ITFACT_LICENSE_HPP
#define ITFACT_LICENSE_HPP

#include <fstream>
#include <string>

#include <openssl/rsa.h>

namespace itfact {
	namespace license {
		class License
		{
		private:

		public:
			License(const std::ifstream &file);
			License(const std::string &license);

			static void makeLicense(const std::string &policy);
			static void updateLicense(const std::string &policy);

			bool isValid();
			bool isValid() const;

		private:
			License();
			static RSA *generateKey(const std::string &path);
			static RSA *loadKey(const std::string &pathanme);
			void checkLicense(const std::string &license);
		};
	}
}

#endif /* ITFACT_LICENSE_HPP */
