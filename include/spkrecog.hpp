/**
 * @headerfile	spkrecog.hpp "spkrecog.hpp"
 * @file	spkrecog.hpp
 * @brief	Summary
 * @details	Description
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 4. 25. 오후 5:29
 * @see		See Also
 */

#ifndef __SPK_RECOG_H
#define __SPK_RECOG_H

#include <cstdlib>

static char char_gender[3] = {'N', 'F', 'M'};

// --------------------------------------------------
// Request
// --------------------------------------------------
typedef struct req_verification_s {
	char gender;			// 성별 
	uint32_t prompt_len;	// 프롬프트 길이 
	char prompt[0];			// 프롬프트 
	uint32_t feat_len;		// 화자 특징 파일 크기 
	char spk_feat[0];		// 화자 특징 파일 
	char wav_data[0];		// WAVE 데이터 
} req_verification_t;

typedef struct req_extract_s {
	char gender;		// 성별 
	uint32_t no_train;	// 이전 학습 횟수. 이전 학습 횟수가 0이면 특징 파일의 크기는 0
	uint32_t feat_len;	// 이전 특징 파일 크기. 특징 파일 크기가 0이면 특징 파일은 존재하지 않음 
	char spk_feat[0];	// 이전 특징 파일 
	char wav_data[0];	// WAVE 데이터 
} req_extract_t;
// --------------------------------------------------

// --------------------------------------------------
// Response
// --------------------------------------------------
typedef struct res_verification_s {
	char gender;		// 확인된 성별 
	float distance;
	float score;
	float coalescence_score;
	float checked_score;
} res_verification_t;

typedef struct res_extract_s {
	char gender;		// 확인된 성별 
	char spk_feat[0];
} res_extract_t;
// --------------------------------------------------

#endif /* __SPK_RECOG_H */
