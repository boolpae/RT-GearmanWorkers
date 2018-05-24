// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.
/**
 * @headerfile	uos-spk-recog.h "uos-spk-recog.h"
 * @file		uos-spk-recog.h
 * @brief		
 * @author		Hee-Soo Heo, Il-Ho Yang, Myung-Jae Kim, Sung-Hyun Yoon, Ha-Jin Yu
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 3. 14. 오후 5:16
 */
#include <stdexcept>
#include <array>
#include <memory>

#include <boost/noncopyable.hpp>

#include "feat/wave-reader.h"
#include "feat/feature-mfcc.h"
#include "ivector/ivector-extractor.h"
#include "gmm/diag-gmm.h"
#include "gmm/full-gmm.h"
#include "ivector/plda.h"

#ifndef UOS_SPK_RECOG_H
#define UOS_SPK_RECOG_H

namespace uos {

enum GENDER
{
	NONE,
	FEMALE,
	MALE
};

enum FILE_TYPE
{
	PCM,
	MFCC,
	SPK_FEAT
};

class BackgroundModel : private boost::noncopyable 
{
public:
	static const unsigned int GENDER_SIZE = 3;
public:
	BackgroundModel(const std::string &path);

	const kaldi::DiagGmm *getDiagGmm(enum GENDER gender = NONE);
	const kaldi::FullGmm *getFullGmm(enum GENDER gender = NONE);
	const kaldi::IvectorExtractor *getIvectorExtractor(enum GENDER gender = NONE);
	const kaldi::Matrix<kaldi::BaseFloat> *getLda(enum GENDER gender = NONE);
	const kaldi::Plda *getPlda(enum GENDER gender = NONE);
	const kaldi::DiagGmm *getDigit(unsigned int index);

	const kaldi::DiagGmm *getDiagGmm(enum GENDER gender = NONE) const;
	const kaldi::FullGmm *getFullGmm(enum GENDER gender = NONE) const;
	const kaldi::IvectorExtractor *getIvectorExtractor(enum GENDER gender = NONE) const;
	const kaldi::Matrix<kaldi::BaseFloat> *getLda(enum GENDER gender = NONE) const;
	const kaldi::Plda *getPlda(enum GENDER gender = NONE) const;
	const kaldi::DiagGmm *getDigit(unsigned int index) const;

	const bool checkModel();
	const bool checkModel() const;

private:
	std::array<kaldi::DiagGmm, GENDER_SIZE> dubm;
	std::array<kaldi::FullGmm, GENDER_SIZE> fubm;
	std::array<kaldi::IvectorExtractor, GENDER_SIZE> ie;
	std::array<kaldi::Matrix<kaldi::BaseFloat>, GENDER_SIZE> lda;
	std::array<kaldi::Plda, GENDER_SIZE> plda;
	std::array<kaldi::DiagGmm, 10>  digit;

	BackgroundModel();
	void validate();
};

class UOSpkFeat
{
public: // Return value
	static const int ERROR_FREQUENCY 	= 2;	// The sample frequency of file is not 8000 Hz
	static const int ERROR_SHROT_LENGTH	= 3;	// The file is too short (< 0.025 sec)
	static const int ERROR_NOT_MONO		= 4;	// The number of channels of file is not 1(mono)

public:
	UOSpkFeat(const std::string &pathname,
			  const enum GENDER gender = NONE,
			  const enum FILE_TYPE = PCM);
	UOSpkFeat(const BackgroundModel *bgm,
			  const std::string &pathname,
			  const enum GENDER gender = NONE,
			  const enum FILE_TYPE = PCM);
	UOSpkFeat(const kaldi::Matrix<kaldi::BaseFloat> &mfcc,
			  const enum GENDER gender = NONE);
	UOSpkFeat(const BackgroundModel *bgm,
			  const kaldi::Matrix<kaldi::BaseFloat> &mfcc,
			  const enum GENDER gender = NONE);
	UOSpkFeat(const kaldi::Vector<kaldi::BaseFloat> &spk_feat,
			  const enum GENDER gender = NONE) : feat_data(spk_feat),
			  									 spk_gender(gender),
			  									 file_type(SPK_FEAT),
			  									 preprocess(true) {}

	const enum GENDER getGender() {return spk_gender;}
	const enum GENDER getGender() const {return spk_gender;}
	const enum FILE_TYPE getType() {return file_type;}
	const enum FILE_TYPE getType() const {return file_type;}
	const bool isPreprocess() {return preprocess;}
	const bool isPreprocess() const {return preprocess;}
	const kaldi::Matrix<kaldi::BaseFloat> *getMfccData() {return &mfcc_data;}
	const kaldi::Matrix<kaldi::BaseFloat> *getMfccData() const {return &mfcc_data;}
	const kaldi::Vector<kaldi::BaseFloat> *getFeatData() {return &feat_data;}
	const kaldi::Vector<kaldi::BaseFloat> *getFeatData() const {return &feat_data;}

	void setGender(const enum GENDER gender) {spk_gender = gender;};

	static int computeMfcc(const std::string wave_pathname, const std::string mfcc_pathname);
	template <bool verification = true>
	void featPreProcess();
	void extractSpkFeat(const BackgroundModel *bgm);
	const enum GENDER checkGender(const BackgroundModel *bgm);

	template <typename T, bool is_model = false>
	static const int readFile(const std::string &pathname, T &buf) {
		std::ifstream ifs(pathname.c_str(), std::ios_base::binary);
		int rc = initFileStream<is_model>(pathname, ifs);
		if (rc) {
			ifs.close();
			return rc;
		}
		try {
			buf.Read(ifs, true);
		} catch (std::exception &e) {
			ifs.close();
			throw;
		}
		ifs.close();
		return EXIT_SUCCESS;
	}
	template <typename T>
	static const int writeFile(const std::string &pathname, const T &buf) {
		std::ofstream ofs(pathname.c_str(), std::ios_base::binary);;
		int rc = initFileStream(pathname, ofs);
		if (rc) {
			ofs.close();
			return rc;
		}
		try {
			buf.Write(ofs, true);
		} catch (std::exception &e) {
			ofs.close();
			throw;
		}
		ofs.flush();
		ofs.close();
		return EXIT_SUCCESS;
	}

private:
	kaldi::Vector<kaldi::BaseFloat> feat_data;
	kaldi::Matrix<kaldi::BaseFloat> mfcc_data;
	enum GENDER spk_gender;
	enum FILE_TYPE file_type;
	bool preprocess;

	UOSpkFeat();
	void init(const std::string &pathname, const enum FILE_TYPE type);
	void computeMfcc(const kaldi::WaveData &wave);
	void featPreProcess(int cmn_window, bool normalize_variance);

	template <bool is_model = false>
	static const int initFileStream(const std::string &pathname, std::ifstream &fs);
	static const int initFileStream(const std::string &pathname, std::ofstream &fs);
};

class UOSpkRecog
{
public:
	UOSpkRecog(const BackgroundModel *bgm);
	UOSpkRecog(const std::string &path);

	const BackgroundModel *getModel() {return model;}
	const BackgroundModel *getModel() const {return model;}
	void setModel(const BackgroundModel *bgm) {
		if (bgm && bgm->checkModel())
			model = const_cast<BackgroundModel *>(bgm);
	}

	std::shared_ptr<UOSpkFeat> trainSpkModel(std::vector<UOSpkFeat> &spk_mfcc_list);
	void trainSpkModel(std::vector<UOSpkFeat> &spk_mfcc_list,
					   kaldi::Vector<kaldi::BaseFloat> &spk_model);
	void trainSpkModel(const std::vector<std::string> &file_list,
					   const std::string &spk_model_path,
					   const enum GENDER gender = NONE,
					   const enum FILE_TYPE = PCM);
	const float checkPrompt(UOSpkFeat test, const std::vector<int> prompt);
	const float checkPrompt(const std::string &test_mfcc_file,
							const std::vector<int> prompt,
							enum FILE_TYPE type = PCM);
	const float verification(const UOSpkFeat &target_model, UOSpkFeat test);
	const float verification(const std::string &target_model_path,
							 const std::string &test_path,
							 const enum GENDER gender = NONE,
							 const enum FILE_TYPE type = PCM);
	
private:
	std::shared_ptr<BackgroundModel> backgroundModel;
	BackgroundModel *model;

	UOSpkRecog();
	const float __verification(const UOSpkFeat &target_model, UOSpkFeat &test);
	const float __checkPrompt(UOSpkFeat &test, const std::vector<int> prompt);
};

}

#endif /* UOS_SPK_RECOG_H */
