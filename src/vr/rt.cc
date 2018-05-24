/**
 * @file	rt.cc
 * @brief	Real-time STT(Speech to text)
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2017. 02. 20. 17:41:47
 * @see		vr.cc
 */
 
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <system_error>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>
 
#include "vr.hpp"

using namespace itfact::vr::node;

#define THREAD_ID	std::this_thread::get_id()
#define LOG_INFO	__FILE__, __FUNCTION__, __LINE__
#define LOG_FMT		" [at %s (%s:%d)]"

/**
 * @brief		RealtimeSTT 객체 초기화
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 06. 18:19:19
 * @param[in]	logger		Logger
 * @param[in]	buffer		내부 버퍼
 * @param[in]	frontend	LFrontEnd 객체
 * @param[in]	child_laser	Laser 객체
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 * @retval		RealtimeSTT	Value description
 * @see			See Also
 */
RealtimeSTT::RealtimeSTT(
	std::shared_ptr<float> buffer,
	std::shared_ptr<LFrontEnd> frontend,
	std::shared_ptr<Laser> child_laser,
	const std::size_t a_mfcc_size,
	const std::size_t a_mini_batch,
	float *a_sil,
	log4cpp::Category *logger
) : mfcc_size(a_mfcc_size), mini_batch(a_mini_batch) {
	job_log = logger;
	feature_vector = buffer;
	front = frontend;
	laser = child_laser;

	sil = a_sil;
	minimum_size = mfcc_size * mini_batch;//80 * mini_batch;

	temp_buffer = (float *) malloc(sizeof(float) * minimum_size);//(short *) malloc(sizeof(short) * minimum_size);
	if (temp_buffer == NULL)
		throw std::system_error(ENOMEM, std::system_category());
}

/**
 * @brief		RealtimeSTT 객체 해제
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 06. 18:26:10
 */
RealtimeSTT::~RealtimeSTT() {
	if (temp_buffer)
		free(temp_buffer);
}

/**
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 10. 23:44:16
 */
void RealtimeSTT::set_reset_period(const std::size_t period) {
	reset_period = period;
}

/**
 * @brief		Speech to text
 * @author		Youngsoo Min (ysmin@itfact.co.kr)
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 06. 16:41:03
 * @param[in]	buffer		녹취 데이터 
 * @param[in]	buffer_len	녹취 데이터 길이 
 * @param[out]	result		STT 결과
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 * @see			RealtimeSTT::unsegment()
 */
int RealtimeSTT::stt(
	const short *buffer,
	const std::size_t buffer_len,
	std::string &result
) {
#if 0
	std::size_t i;
	int rc;
	int reset_flag=0;

	// FIXME: 데이터 푸시
	// CASE 1) temp_buffer_len == 0 && (buffer_len + temp_buffer_len) >= minimum_size
	// 분석 시도하고 남은 버퍼를 임시 버퍼에 보관 

	// CASE 2) temp_buffer_len > 0 && (buffer_len + temp_buffer_len) >= minimum_size
	// 버퍼와 합친 후 분석 시도하고 남은 버퍼를 임시 버퍼에 보관 

	// CASE 3) (temp_buffer_len > 0 || temp_buffer_len == 0) && (buffer_len + temp_buffer_len) < minimum_size
	// 임시 버퍼에 보관 

	std::size_t remain_size = buffer_len;
	std::size_t buffer_index = 0;
	// std::size_t offset = 0;
	while (remain_size > 0) {
		if ((remain_size + temp_buffer_len) < minimum_size) {
			memcpy(temp_buffer + temp_buffer_len, buffer + buffer_index, sizeof(short) * remain_size);
			temp_buffer_len += remain_size;
			break;
			return EXIT_SUCCESS;
		}

		short *data;
		std::size_t data_size = 0;
		if (temp_buffer_len > 0) {
			std::size_t copy_size = minimum_size - temp_buffer_len;
			memcpy(temp_buffer + temp_buffer_len, buffer + buffer_index, sizeof(short) * copy_size);
			data = temp_buffer;
			buffer_index += copy_size;
			remain_size -= copy_size;
		} else {
			data = const_cast<short *>(buffer + buffer_index);
			buffer_index += minimum_size;
			remain_size -= minimum_size;
		}

		data_size = minimum_size;
		if (remain_size < 0)
			data_size += remain_size;
		temp_buffer_len = 0;

		int fsize = 0;
		if (likely(running++)) {
			//result.clear();
			rc = stepFrameLFrontEnd(front.get(), data_size, data, &fsize, feature_vector.get());
			// if (rc)
				job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), fsize: %d" LOG_FMT, THREAD_ID, rc, fsize, LOG_INFO);
		} else {
			float *_temp_vector = (float *) malloc(sizeof(float) * mfcc_size * mini_batch);
			if (_temp_vector == NULL)
				continue;
			std::shared_ptr<float> temp_vector(_temp_vector, free);

			rc = stepFrameLFrontEnd(front.get(), data_size, data, &fsize, temp_vector.get());
			job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), fsize: %d" LOG_FMT, THREAD_ID, rc, fsize, LOG_INFO);

			for (i = 0; i < VRServer::LDA_LEN_FRAMESTACK; ++i)
				memcpy(feature_vector.get() + i * mfcc_size, temp_vector.get(), sizeof(float) * mfcc_size);
			memcpy(feature_vector.get() + i * mfcc_size, temp_vector.get(), sizeof(float) * fsize);
			fsize = fsize + i * mfcc_size;
		}

		if (fsize <= 0)
			continue;

		std::size_t nf = fsize / mfcc_size;
		if (nf < mini_batch)
			memcpy(feature_vector.get() + nf * mfcc_size, sil, sizeof(float) * mfcc_size * (mini_batch - nf));

		for (i = 0; i < nf; ++i) {
			// 특징 벡터의 차원 값을 추가적으로 사용하여 프레임 기반의 탐색을 수행 (feature_dim = 128 * 600)
			if (stepSARecFrameExt(laser.get(), index + i, feature_dim, feature_vector.get() + i * mfcc_size) != EXIT_SUCCESS)
				return EXIT_FAILURE;
		}

		index += nf;
		rc = get_intermediate_results(laser.get(), index, skip_position, last_position, reset_period, result);
		if (rc == EXIT_SUCCESS && index > reset_period) {
			// reallocSLaser(laser.get());
			if (resetSLaser(laser.get())) {
				job_log->error("[0x%X] Fail to resetSLaser" LOG_FMT, THREAD_ID, LOG_INFO);
				return EXIT_FAILURE;
			}

			index = 0;
			last_position += skip_position;
			skip_position = 0;

			// after resetSLaser(), copy buffer to temp_buffer
			reset_flag = 1;
		}
	}

	// after resetSLaser(), copy buffer to temp_buffer
	if (reset_flag) {
		if ((buffer_len - minimum_size)>0) {
			memcpy(temp_buffer, buffer+(buffer_len - minimum_size), sizeof(short) * minimum_size);
			temp_buffer_len = minimum_size;
		}
		else {
			memset(temp_buffer, 0, (sizeof(short) * minimum_size));
			memcpy(temp_buffer, buffer, sizeof(short) * buffer_len);
			temp_buffer_len = buffer_len;
		}
		reset_flag = 0;
	}
#else
	resetSLaser(laser.get()); 
	resetLFrontEnd(front.get());	

	// 녹취 파일을 읽어가며 처리
	std::size_t read_size = 80 * mini_batch;
	std::size_t offset = 0;
	std::size_t index = 0;
	unsigned long i;
	int rc;
	for (; offset < buffer_len; offset += read_size) {
		int fsize = 0;
		std::size_t rsize = read_size;
		std::size_t remain = buffer_len - offset;
		if (rsize > remain)
			rsize = remain;

		rc = stepFrameLFrontEnd(front.get(), rsize, const_cast<short *>(&buffer[offset]), &fsize, temp_buffer);
		job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), read: %d, fsize: %d" LOG_FMT,
						THREAD_ID, rc, rsize, fsize, LOG_INFO);
		if (fsize <= 0)
			continue;

		for (i = 0; i < 15; ++i)
			memcpy(feature_vector.get() + i * mfcc_size, temp_buffer, sizeof(float) * mfcc_size);
		memcpy(feature_vector.get() + i * mfcc_size, temp_buffer, sizeof(float) * fsize);
		fsize = fsize + i * mfcc_size;

		std::size_t nf = fsize / mfcc_size;
		if (nf < mini_batch)
			memcpy(feature_vector.get() + nf * mfcc_size, sil, sizeof(float) * mfcc_size * (mini_batch - nf));

		for (i = 0; i < nf; ++i) {
			// 특징 벡터의 차원 값을 추가적으로 사용하여 프레임 기반의 탐색을 수행 (feature_dim = 128 * 600)
			if (stepSARecFrameExt(laser.get(), index + i, feature_dim, feature_vector.get() + i * mfcc_size) != EXIT_SUCCESS)
				return EXIT_FAILURE;
		}

		index += nf;
		break;
	}

	offset += read_size;
	job_log->debug("[0x%X] index: %d, offset: %d" LOG_FMT, THREAD_ID, index, offset, LOG_INFO);
	std::size_t last_position = 0;
	int fsize = 0;
	for (; offset < buffer_len; offset += read_size) {
		std::size_t rsize = read_size;
		std::size_t remain = buffer_len - offset;
		if (rsize > remain)
			rsize = remain;

		// 음성 신호로부터 특징 벡터 출력
		rc = stepFrameLFrontEnd(front.get(), rsize, const_cast<short *>(&buffer[offset]), &fsize,feature_vector.get());
		if (rc) job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), read: %d, fsize: %d" LOG_FMT,
								THREAD_ID, rc, rsize, fsize, LOG_INFO);
		if (fsize <= 0)
			continue;

		std::size_t nf = fsize / mfcc_size;
		if (nf < mini_batch)
			memcpy(feature_vector.get() + nf * mfcc_size, sil, sizeof(float) * mfcc_size * (mini_batch - nf));

		for (i = 0; i < nf; ++i) {
			// 특징 벡터의 차원 값을 추가적으로 사용하여 프레임 기반의 탐색을 수행 (feature_dim = 128 * 600)
			if (stepSARecFrameExt(laser.get(), index + i, feature_dim, feature_vector.get() + i * mfcc_size) != EXIT_SUCCESS)
				return EXIT_FAILURE;
		}
		index += nf;

		if (index > reset_period) {
			if (get_final_result(laser.get(), index, last_position, feature_dim, mfcc_size, sil, result) != EXIT_SUCCESS)
				continue;

			// reallocSLaser(laser.get());
			if (resetSLaser(laser.get())) {
				job_log->error("[0x%X] Fail to resetSLaser" LOG_FMT, THREAD_ID, LOG_INFO);
				return EXIT_FAILURE;
			}

			index = 0;
		}
	}

	// flush internal buffer (static + zero padding -> dynamic feat)
	rc = stepFrameLFrontEnd(front.get(), 0, NULL, &fsize, feature_vector.get());
	job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), fsize: %d" LOG_FMT, THREAD_ID, rc, fsize, LOG_INFO);
	std::size_t nf = fsize / mfcc_size;
	for (i = 0; i < nf; ++i) {
		if (stepSARecFrameExt(laser.get(), index + i, feature_dim, feature_vector.get() + i * mfcc_size) != EXIT_SUCCESS)
			return EXIT_FAILURE;
	}
	index += nf;

	if (index > 0) {
		job_log->debug("[0x%X] partial backtracking size: %d" LOG_FMT, THREAD_ID, index * mfcc_size, LOG_INFO);
		if (get_final_result(laser.get(), index, last_position, feature_dim, mfcc_size, sil, result) != EXIT_SUCCESS)
			return EXIT_FAILURE;
	}

#endif
	return EXIT_SUCCESS;
}

/**
 * @brief		버퍼에 남은 녹취 데이터를 분석 시도
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 14. 10:58:54
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int RealtimeSTT::free_buffer(std::string &result) {
	int fsize = 0;
	int rc = stepFrameLFrontEnd(front.get(), 0, NULL, &fsize, feature_vector.get());
	job_log->debug("[0x%X] stepFrameLFrontEnd(0x%x), fsize: %d" LOG_FMT, THREAD_ID, rc, fsize, LOG_INFO);
	std::size_t nf = fsize / mfcc_size;
	for (std::size_t i = 0; i < nf; ++i) {
		if (stepSARecFrameExt(laser.get(), index + i, feature_dim, feature_vector.get() + i * mfcc_size) != EXIT_SUCCESS)
			return EXIT_FAILURE;
	}
	index += nf;

	if (index)
		return get_intermediate_results(laser.get(), index, skip_position, last_position, reset_period, result);
		// return get_final_result(laser.get(), index, feature_dim, mfcc_size, sil, result);
	return EXIT_SUCCESS;
}
