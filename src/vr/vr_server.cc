/**
 * @file	vr_server.cc
 * @brief	VR Server
 * @details	
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 06. 17. 17:32:24
 * @see		
 */
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "ETRIPP.h"
#include "vr.hpp"
#include "restapi.hpp"

#include <iostream>
#include <fstream>

using namespace itfact::vr::node;

static log4cpp::Category *job_log = NULL;
static std::string tmp_path;

static gearman_return_t job_stt(gearman_job_st *job, void *context);
static gearman_return_t job_rt_stt(gearman_job_st *job, void *context);
static gearman_return_t job_unsegment(gearman_job_st *job, void *context);
static gearman_return_t job_unsegment_with_time(gearman_job_st *job, void *context);
static gearman_return_t job_ssp(gearman_job_st *job, void *context);

// 디코딩을 위한 함수 
extern int ITFACT_ETC_Init_Convert_RECFile(void);
extern int ITFACT_ETC_Convert_RECFile(char *ORGFile, char *TARFile, int WAVHEADER);

static struct {
	std::string laser_config;
	std::string frontend_config;
	std::string sil_dnn;
	std::string am_filename;	// amFn
	std::string fsm_filename;	// fsmFn
	std::string sym_filename;	// symFn
	std::string dnn_filename;	// dnnFn
	std::string prior_filename;	// priFn
	std::string norm_filename;	// normFn
	std::string chunking_filename;
	std::string tagging_filename;
	std::string user_dic;
	std::string tmp_path;
	std::string fail_nofile;
	std::string fail_download;
	std::string fail_decoding;
} default_config = {
	.laser_config = "config/stt_laser.cfg",
	.frontend_config = "config/frontend_dnn.cfg",
	.sil_dnn = "config/sil_dnn.data",
	.am_filename = "stt_release.sam.bin",
	.fsm_filename = "stt_release.sfsm.bin",
	.sym_filename = "stt_release.sym.bin",
	.dnn_filename = "final.dnn.adapt",
	.prior_filename = "final.dnn.prior.bin",
	.norm_filename = "final.dnn.lda.bin",
	.chunking_filename = "chunking.release.bin",
	.tagging_filename = "tagging.release.bin",
	.user_dic = "user_dic.txt",
	.tmp_path = "/dev/shm/smart-vr",
	.fail_nofile = "E10100",
	.fail_download = "E10200",
	.fail_decoding = "E20400",
};

/**
 */
int main(const int argc, char const *argv[]) {
	try {
		VRServer server(argc, argv);
		server.initialize();
	} catch (std::exception &e) {
		perror(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

VRServer::~VRServer() {
	if (masterLaserP)
		unload_laser_module();
}

/**
 * @brief		초기화 후 실행 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 27. 10:12:37
 * @param[in]	argc	인수의 개수 
 * @param[in]	argc	인수 배열 
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a negative error code is returned indicating what went wrong.
 */
int VRServer::initialize() {
	const itfact::common::Configuration *config = getConfig();
	std::string image_path = "./";
	image_path = config->getConfig("stt.image_path", image_path.c_str());
	if (image_path.at(image_path.size() - 1) != '/')
		image_path.push_back('/');

	job_log = getLogger();
	job_log->info("Smart VR Server v1.1 Build-Date(%s)", __DATE__);
	job_log->debug("=========================================");
	job_log->debug("========== Check configuration ==========");
	job_log->debug("=========================================");

	job_log->debug("stt.worker: %d", getTotalWorkers("stt"));
	job_log->debug("unsegment.worker: %d", getTotalWorkers("unsegment"));
	job_log->debug("ssp.worker: %d", getTotalWorkers("ssp"));
	job_log->debug("realtime.worker: %d, startnum(%d)", getTotalWorkers("realtime"), config->getConfig("realtime.startnum", 1));

	// 설정값 로드 
	mfcc_size = config->getConfig("stt.mfcc_size", mfcc_size);
	mini_batch = config->getConfig("stt.mini_batch", mini_batch);
	prior_weight = config->getConfig("stt.prior_weight", prior_weight);
	useGPU = config->getConfig("stt.useGPU", useGPU);
	idGPU = config->getConfig("stt.idGPU", idGPU);
	if (mini_batch > MAX_MINIBATCH)
		mini_batch = MAX_MINIBATCH;
	feature_dim = mfcc_size * mini_batch;

	job_log->debug("stt.mfcc_size: %d, stt.mini_batch: %d, stt.prior_weight: %f",
					mfcc_size, mini_batch, prior_weight);

	job_log->debug("stt.useGPU: %s, stt.idGPU: %d", (useGPU) ? "true" : "false" , idGPU);

	std::string laser_config = config->getConfig("stt.laser_config", default_config.laser_config.c_str());
	frontend_config = config->getConfig("stt.frontend_config", default_config.frontend_config.c_str());
	std::string sil_dnn = config->getConfig("stt.sil_dnn", default_config.sil_dnn.c_str());

	job_log->debug("stt.laser_config: %s", laser_config.c_str());
	job_log->debug("stt.frontend_config: %s", frontend_config.c_str());
	job_log->debug("stt.sil_dnn: %s", sil_dnn.c_str());

	am_file = std::string(image_path).
			append(config->getConfig("stt.am_filename", default_config.am_filename.c_str()));
	fsm_file = std::string(image_path).
			append(config->getConfig("stt.fsm_filename", default_config.fsm_filename.c_str()));
	sym_file = std::string(image_path).
			append(config->getConfig("stt.sym_filename", default_config.sym_filename.c_str()));
	dnn_file = std::string(image_path).
			append(config->getConfig("stt.dnn_filename", default_config.dnn_filename.c_str()));
	prior_file = std::string(image_path).
			append(config->getConfig("stt.prior_filename", default_config.prior_filename.c_str()));
	norm_file = std::string(image_path).
			append(config->getConfig("stt.norm_filename", default_config.norm_filename.c_str()));

	std::string chunking_file = std::string(image_path).
			append(config->getConfig("stt.chunking_filename", default_config.chunking_filename.c_str()));
	std::string tagging_file = std::string(image_path).
			append(config->getConfig("stt.tagging_filename", default_config.tagging_filename.c_str()));
	std::string user_dic_file = std::string(image_path).
			append(config->getConfig("stt.user_dic", default_config.user_dic.c_str()));

	tmp_path = config->getConfig("master.tmp_path", default_config.tmp_path.c_str());
	if (tmp_path.at(tmp_path.size() - 1) != '/')
		tmp_path.push_back('/');
	itfact::common::checkPath(tmp_path, true);

	job_log->debug("stt.am_filename: %s", am_file.c_str());
	job_log->debug("stt.fsm_filename: %s", fsm_file.c_str());
	job_log->debug("stt.sym_filename: %s", sym_file.c_str());
	job_log->debug("stt.dnn_filename: %s", dnn_file.c_str());
	job_log->debug("stt.prior_filename: %s", prior_file.c_str());
	job_log->debug("stt.norm_filename: %s", norm_file.c_str());
	job_log->debug("stt.chunking_filename: %s", chunking_file.c_str());
	job_log->debug("stt.tagging_filename: %s", tagging_file.c_str());
	job_log->debug("stt.user_dic: %s", user_dic_file.c_str());
	job_log->debug("=========================================");

	//FIXME: License 체크 

	// module 초기화 
	if (!load_laser_module())
		return EXIT_FAILURE;

	// Controller 실행 
	RestApi api(config, job_log);
	api.start();

	job_log->info("Connect to Master server(%s:%d)", config->getHost().c_str(), config->getPort());
	run("vr_stt", this, getTotalWorkers("stt"), job_stt);
	run("vr_text_only", this, getTotalWorkers("unsegment"), job_unsegment);
	run("vr_text", this, getTotalWorkers("unsegment"), job_unsegment_with_time);
	run("vr_ssp", this, getTotalWorkers("ssp"), job_ssp);

	run("vr_realtime", this, getTotalWorkers("realtime"), job_rt_stt);

	job_log->info("Done");
	join();

	// module 종료 
	job_log->info("Release server");
	unload_laser_module();

	return EXIT_SUCCESS;
}

/**
 * @brief		파일 저장 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 10. 14. 17:55:25
 * @param[in]	job_name	Job name
 * @return		Upon successful completion, a TRUE is returned.\n
 				Otherwise, a FALSE is returned.
 * @see			job_stt()
 */
static inline bool
__store_file(gearman_job_st *job, const char *job_name, const bool is_wave,
			 const short *data, const size_t size,
			 const char *workload, const size_t workload_size,
			 const enum PROTOCOL protocol, std::string &input_file) {
	if (protocol == PROTOCOL_FILE) {
		std::string::size_type idx = std::string(workload, 10).find("://");
		if (idx == std::string::npos) {
			job_log->error("[%s] Cannot decoding: %s", job_name, workload);
			return false;
		}

		input_file = std::string(workload, workload_size).substr(idx + 3);
	} else {
		/*
		// 로컬 파일이 이니므로 파일 저장 
		input_file = tmp_path;
		input_file.push_back('/');
		input_file.append(gearman_job_handle(job));
		input_file.push_back('.');
		if (is_wave)
			input_file.append("wav");
		else
			input_file.append("mp3");


		job_log->debug("[%s] Write to %s", job_name, input_file.c_str());
		std::FILE *fp = std::fopen(input_file.c_str(), "wb");
		if (!fp) {
			job_log->error("[%s] Cannot write: %s", job_name, std::strerror(errno));
			return false;
		}
		std::shared_ptr<std::FILE> fd(fp, std::fclose);

		for (size_t i = 0; i < size; ++i) {
			if (std::fwrite((char *) &data[i], sizeof(short), 1, fd.get()) == 0)
				job_log->warn("[%s] Write size is zero: %s", job_name, std::strerror(errno));
		}
		*/

		// 기존 쓰레드값으로 다운로드 파일을 저장하던 부분을 다운로드 실제 파일명으로 저장하도록 변경
		job_log->debug("[%s] workload: %s", job_name, workload);
		std::string down_fn;
		std::string tmp_fn = std::string(workload, workload_size);
		std::vector<std::string> tmp_vec;
		boost::split(tmp_vec, tmp_fn, boost::is_any_of("/"));
		down_fn = tmp_vec[tmp_vec.size() - 1];
		int nFind = down_fn.rfind('.');
		down_fn = down_fn.substr(0, nFind);

		input_file = tmp_path;
		//input_file.append(gearman_job_handle(job));
		input_file.append(down_fn);
		input_file.push_back('.');
		if (is_wave)
			input_file.append("wav");
		else
			input_file.append("mp3");


		job_log->debug("[%s] Write to %s", job_name, input_file.c_str());
		std::FILE *fp = std::fopen(input_file.c_str(), "wb");
		if (!fp) {
			job_log->error("[%s] Cannot write: %s", job_name, std::strerror(errno));
			return false;
		}
		std::shared_ptr<std::FILE> fd(fp, std::fclose);

		for (size_t i = 0; i < size; ++i) {
			if (std::fwrite((char *)&data[i], sizeof(short), 1, fd.get()) == 0)
				job_log->warn("[%s] Write size is zero: %s", job_name, std::strerror(errno));
		}
	}

	return true;
}

/**
 * @brief		파일 로드  
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 10. 14. 17:57:51
 * @param[in]	job_name	Job name
 * @return		Upon successful completion, a TRUE is returned.\n
 				Otherwise, a FALSE is returned.
 * @see			job_stt()
 */
static inline bool __load_file(const std::string &output_file, std::vector<short> &buffer) {
	std::FILE *fp = std::fopen(output_file.c_str(), "rb");
	if (!fp)
		return false;
	std::shared_ptr<std::FILE> fd(fp, std::fclose);

	short sData;
	while (std::fread(&sData, sizeof(short), 1, fd.get()) > 0)
		buffer.push_back(sData);

	return true;
}

/**
 * @brief		STT 수행
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 10. 14. 17:53:49
 * @param[in]	server	 VR 인스턴스 
 * @return		Upon successful completion, a TRUE is returned.\n
 				Otherwise, a FALSE is returned.
 * @see			job_stt()
 */
static inline bool __job_stt(VRServer *server, short *data, size_t size, std::string &cell_data) {
	try {
		int rc = server->stt(data, size, cell_data);
		if (rc)
			return false;

		return true;
	} catch (std::exception &e) {
		return false;
	} catch (std::exception *e) {
		return false;
	}
}

/**
 * @brief		STT 요청 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 27. 13:39:27
 * @return		Upon successful completion, a GEARMAN_SUCCESS is returned.\n
 				Otherwise, a GEARMAN_ERROR is returned.
 * @see			job_unsegment()
 */
static gearman_return_t job_stt(gearman_job_st *job, void *context) {
	const char *workload = (const char *) gearman_job_workload(job);
	const size_t workload_size = gearman_job_workload_size(job);
	std::string __job_name(COLOR_BLACK_BOLD);
	__job_name.append("STT:");
	__job_name.append(gearman_job_handle(job));
	__job_name.append(COLOR_NC);
	const char *job_name = __job_name.c_str();
	VRServer *server = (VRServer *) context;

	job_log->info("[%s] Recieved %d bytes", job_name, workload_size);
	if (workload_size < 10) {
		job_log->error("[%s] The file size is too small (< 10 bytes)", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 프로토콜 확인 
	short *data;
	size_t size;
	std::vector<short> buffer;
	enum PROTOCOL protocol;

	// 프로토콜로 오는 경우 파일패스에 쓰레기 값이 붙는 현상이 있어서 데이터 처리
	std::string get_file_nm("");
	get_file_nm = workload;
	get_file_nm = get_file_nm.substr(0, workload_size);
	job_log->debug("[%s] workload ==> %s", job_name, get_file_nm.c_str());

	try {
		//protocol = WorkerDaemon::downloadData(server->getConfig(), workload, workload_size, buffer);
		protocol = WorkerDaemon::downloadData(server->getConfig(), get_file_nm.c_str() , workload_size, buffer);
	} catch (std::exception &e) {
		// 처리 불가 
		job_log->error("[%s] Fail to download. %s", job_name, e.what());
		gearman_job_send_warning(job, default_config.fail_download.c_str(),
									  default_config.fail_download.size());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	if (protocol == PROTOCOL_NONE) {
		// Stream data
		data = (short *) workload;
		size = workload_size / sizeof(short);
	} else {
		data = (short *) &*buffer.begin();
		size = buffer.size();
	}

	// Check WAVE format
	int metadata_size;
	bool is_wave = false;
	std::string input_file, output_file;
	switch (VRServer::check_wave_format(data, size)) {
	default:
		// 분석 불가능한 포멧 
		job_log->error("[%s] Unsupported format", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;

	case UNKNOWN_FORMAT:
		job_log->warn("[%s] Input data like RAW PCM", job_name);
		// PCM_WAVE 타입일 수도 있으니 일단 분석 시도 
		break;

	case STANDARD_WAVE:
		job_log->info("[%s] Input data is Standard WAVE format", job_name);
		metadata_size = 44 / sizeof(short);
		data += metadata_size;
		size -= metadata_size;
		break;

	case WAVE:
		is_wave = true;

	case MPEG:
		if (is_wave)
			job_log->info("[%s] Input data is WAVE format", job_name);
		else
			job_log->info("[%s] Input data is MPEG-3 format", job_name);

		// 로컬 파일이 아닌 경우 저장 시도 
		//if (!__store_file(job, job_name, is_wave, data, size, workload, workload_size, protocol, input_file)) {
		if (!__store_file(job, job_name, is_wave, data, size, get_file_nm.c_str(), workload_size, protocol, input_file)) {
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		// 기존 버퍼 삭제 
		buffer.clear();

		// 바이너리 호출로 대체 
		if (server->getConfig()->isSet("stt.decoder")) {
			std::string cmd(server->getConfig()->getConfig("stt.decoder"));
			cmd.push_back(' ');
			cmd.append(input_file.c_str());
			job_log->debug("[%s] %s", job_name, cmd.c_str());
			if (std::system(cmd.c_str())) {
				job_log->error("[%s] Fail to decoding: %s", job_name, input_file.c_str());
				gearman_job_send_warning(job, default_config.fail_decoding.c_str(),
					default_config.fail_decoding.size());
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
			}
		}
		else {
			job_log->error("[%s] Cannot decoding: %s", job_name, input_file.c_str());
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		output_file = std::string(input_file.c_str(), input_file.size() - 3);
		output_file.append("pcm");

		// 파일 다시 로드 
		if (!__load_file(output_file, buffer)) {
			job_log->error("[%s] Cannot decoding: %s", job_name, std::strerror(errno));
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		data = (short *) &*buffer.begin();
		size = buffer.size();

		// 임시 파일 삭제 
		std::remove(output_file.c_str());
		if (protocol != PROTOCOL_FILE)
			std::remove(input_file.c_str());
		break;


	case WAVE_2CH:	// 일반 디코딩 후 처리 하도록 수정(AIA 용)
		// 로컬 파일이 아닌 경우 저장 시도 
		//if (!__store_file(job, job_name, is_wave, data, size, workload, workload_size, protocol, input_file)) {
		if (!__store_file(job, job_name, is_wave, data, size, get_file_nm.c_str(), workload_size, protocol, input_file)) {
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		// 기존 버퍼 삭제 
		buffer.clear();

		// 바이너리 호출로 대체 
		if (server->getConfig()->isSet("stt.decoder")) {
			std::string cmd(server->getConfig()->getConfig("stt.decoder"));
			cmd.push_back(' ');
			cmd.append(input_file.c_str());
			job_log->debug("[%s] %s", job_name, cmd.c_str());
			if (std::system(cmd.c_str())) {
				job_log->error("[%s] Fail to decoding: %s", job_name, input_file.c_str());
				gearman_job_send_warning(job, default_config.fail_decoding.c_str(),
					default_config.fail_decoding.size());
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
			}
		}
		else {
			job_log->error("[%s] Cannot decoding: %s", job_name, input_file.c_str());
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		output_file = std::string(input_file.c_str(), input_file.size() - 3);
		output_file.append("pcm");

		// 파일 다시 로드 
		if (!__load_file(output_file, buffer)) {
			job_log->error("[%s] Cannot decoding: %s", job_name, std::strerror(errno));
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		data = (short *) &*buffer.begin();
		size = buffer.size();

		// 임시 파일 삭제 
		std::remove(output_file.c_str());
		if (protocol != PROTOCOL_FILE)
			std::remove(input_file.c_str());
	}
		



	// 아래 주석 부분을 AIA 처리 부분 때문에 주석으로 처리함. 다른 사이트는 적용시 주석 해제(20171206)
	/*
	case WAVE_2CH:
		job_log->info("[%s] Input data is 2CH WAVE", job_name);
		// 로컬 파일이 아닌 경우 저장 시도 
		if (!__store_file(job, job_name, true, data, size, workload, workload_size, protocol, input_file)) {
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
		}

		// 바이너리 호출로 대체 
		if (server->getConfig()->isSet("stt.separator")) {
			std::string cmd(server->getConfig()->getConfig("stt.separator"));
			cmd.push_back(' ');
			cmd.append(input_file.c_str());
			cmd.push_back(' ');
			cmd.append(input_file.substr(0, input_file.rfind("/")));
			job_log->debug("[%s] %s", job_name, cmd.c_str());
			if (std::system(cmd.c_str())) {
				job_log->error("[%s] Fail to separation: %s", job_name, input_file.c_str());
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
			}
		} else {
			job_log->error("[%s] Cannot separation: %s", job_name, input_file.c_str());
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		if (protocol != PROTOCOL_FILE)
			std::remove(input_file.c_str());

		// 분리된 파일 처리 
		std::string part_data[2];
		for (int ch_idx = 0; ch_idx < 2; ++ch_idx) {
			output_file = std::string(input_file.c_str(), input_file.size() - 4);
			output_file.push_back('_');
			output_file.append(ch_idx == 0 ? "left" : "right");
			output_file.append(".pcm");

			// 기존 버퍼 삭제 
			buffer.clear();

			// 파일 다시 로드 
			if (!__load_file(output_file, buffer)) {
				job_log->error("[%s] Cannot decoding: %s", job_name, std::strerror(errno));
				gearman_job_send_warning(job, default_config.fail_decoding.c_str(),
											  default_config.fail_decoding.size());
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
			}

			data = (short *) &*buffer.begin();
			size = buffer.size();

			if (!__job_stt(server, data, size, part_data[ch_idx])) {
				job_log->error("[%s] Fail to stt", job_name);
				gearman_job_send_fail(job);
				return GEARMAN_ERROR;
			}

			std::remove(output_file.c_str());
		}

		// 결과 전송
		std::string merge_data(boost::lexical_cast<std::string>(size * sizeof(short)));
		merge_data.push_back('\n');
 		merge_data.append(part_data[0]);
 		merge_data.append("||");
 		merge_data.append(part_data[1]);

		job_log->debug("[%s] Done: %d bytes", job_name, merge_data.size());
		if (gearman_failed(gearman_job_send_complete(job, merge_data.c_str(), merge_data.size()))) {
			job_log->error("[%s] Fail to send result", job_name);
			return GEARMAN_ERROR;
		}

		return GEARMAN_SUCCESS;
	}
	*/

	// STT
	std::string cell_data(boost::lexical_cast<std::string>(size * sizeof(short)));
	cell_data.push_back('\n');
	if (!__job_stt(server, data, size, cell_data)) {
		job_log->error("[%s] Fail to stt", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 결과 전송 
	job_log->debug("[%s] Done: %d bytes", job_name, cell_data.size());
	gearman_return_t ret = gearman_job_send_complete(job, cell_data.c_str(), cell_data.size());
	if (gearman_failed(ret)) {
		job_log->error("[%s] Fail to send result", job_name);
		return GEARMAN_ERROR;
	}

	return GEARMAN_SUCCESS;
}

/**
 * @brief		save_data
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 5. 12. 오전 11:27
 * @return		Upon successful completion, a EXIT_SUCCESS is returned.\n
 				Otherwise,
 				a positive error code is returned indicating what went wrong.
 */
static int save_data(const std::string &filename, const size_t data_size, const char *data) {
	std::ofstream ofs(filename);
	try {
		ofs.write(data, data_size);
	} catch (std::exception &e) {
		ofs.close();
		return errno;
	}
	ofs.flush();
	ofs.close();
	return EXIT_SUCCESS;
}

/**
 * @brief		Unsegment 요청
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 30. 13:17:55
 * @return		Upon successful completion, a GEARMAN_SUCCESS is returned.\n
 				Otherwise, a GEARMAN_ERROR is returned.
 * @see			job_stt()
 */
static gearman_return_t job_unsegment(gearman_job_st *job, void *context) {
	const char *workload = (const char *) gearman_job_workload(job);
	const size_t workload_size = gearman_job_workload_size(job);
	std::string __job_name(COLOR_BLACK_BOLD);
	__job_name.append("Unsegment:");
	__job_name.append(gearman_job_handle(job));
	__job_name.append(COLOR_NC);
	const char *job_name = __job_name.c_str();
	// const char *job_name = (const char *) gearman_job_handle(job);
	VRServer *server = (VRServer *) context;

	job_log->info("[%s] Recieved %d bytes", job_name, workload_size);
	if (workload_size < 10) {
		job_log->error("[%s] The file size is too small (< 10 bytes)", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// Unsegment 
	std::string cell_data(workload, workload_size);
	std::string text;
	try {
		if (server->unsegment(cell_data, text)) {
			job_log->error("[%s] Fail to unsegment", job_name);
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}
	} catch(std::exception &e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e.what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	} catch(std::exception *e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e->what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 결과 전송 
	job_log->debug("[%s] Done: %s", job_name, text.c_str());
	job_log->debug("[%s] Done: %d bytes", job_name, text.size());
	gearman_return_t ret = gearman_job_send_complete(job, text.c_str(), text.size());
	if (gearman_failed(ret)) {
		job_log->error("[%s] Fail to send result", job_name);
		return GEARMAN_ERROR;
	}

	return GEARMAN_SUCCESS;
}

/**
 * @brief		Unsegment 요청
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 06. 30. 13:17:55
 * @return		Upon successful completion, a GEARMAN_SUCCESS is returned.\n
 				Otherwise, a GEARMAN_ERROR is returned.
 * @see			job_stt()
 */
static gearman_return_t job_unsegment_with_time(gearman_job_st *job, void *context) {
	const char *workload = (const char *) gearman_job_workload(job);
	const size_t workload_size = gearman_job_workload_size(job);
	std::string __job_name(COLOR_BLACK_BOLD);
	__job_name.append("Unsegment:");
	__job_name.append(gearman_job_handle(job));
	__job_name.append(COLOR_NC);
	const char *job_name = __job_name.c_str();
	// const char *job_name = (const char *) gearman_job_handle(job);
	VRServer *server = (VRServer *) context;

	job_log->info("[%s] Recieved %d bytes", job_name, workload_size);
	if (workload_size < 10) {
		job_log->error("[%s] The file size is too small (< 10 bytes)", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// FIXME: 임시로 파일 저장 
	std::string mlf_pathname(tmp_path);
	mlf_pathname.append(gearman_job_handle(job));
	std::string text_pathname(mlf_pathname);
	text_pathname.append(".txt");
	mlf_pathname.append(".mlf");
	if (save_data(mlf_pathname, workload_size, workload)) {
		job_log->error("[%s] Fail to recieve data", job_name);
		return GEARMAN_ERROR;
	}

	// Unsegment 
	std::string text = "";
	try {
		if (server->unsegment_with_time(mlf_pathname, text_pathname)) {
			job_log->error("[%s] Fail to unsegment", job_name);
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}

		// FIXME: 임시로 파일 리드 
		std::ifstream text_file(text_pathname);
		if (text_file.is_open()) {
			for (std::string line; std::getline(text_file, line); ) {
				text.append(line);
				text.push_back('\n');
			}
			text_file.close();
		} else
			throw std::runtime_error("Cannot load data");

		std::remove(mlf_pathname.c_str());
		std::remove(text_pathname.c_str());
	} catch(std::exception &e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e.what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	} catch(std::exception *e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e->what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 결과 전송 
	job_log->debug("[%s] Done: %d bytes", job_name, text.size());
	gearman_return_t ret = gearman_job_send_complete(job, text.c_str(), text.size());
	if (gearman_failed(ret)) {
		job_log->error("[%s] Fail to send result", job_name);
		return GEARMAN_ERROR;
	}

	return GEARMAN_SUCCESS;
}

/**
 * @brief		출금 동의 구간 검출 요청 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2016. 08. 03. 19:02:18
 * @return		Upon successful completion, a GEARMAN_SUCCESS is returned.\n
 				Otherwise, a GEARMAN_ERROR is returned.
 */
static gearman_return_t job_ssp(gearman_job_st *job, void *context) {
	const char *workload = (const char *) gearman_job_workload(job);
	const size_t workload_size = gearman_job_workload_size(job);
	std::string __job_name(COLOR_BLACK_BOLD);
	__job_name.append("SSP:");
	__job_name.append(gearman_job_handle(job));
	__job_name.append(COLOR_NC);
	const char *job_name = __job_name.c_str();
	VRServer *server = (VRServer *) context;

	job_log->info("[%s] Recieved %d bytes", job_name, workload_size);
	if (workload_size < 10) {
		job_log->error("[%s] The file size is too small (< 10 bytes)", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// <s> </s> 태그를 제외해야 하며 like 정보도 무시해야 함 
	std::string mlf_data;
	std::string recv_data(workload, workload_size);
	std::vector<std::string> lines;
	boost::split(lines, recv_data, boost::is_any_of("\n"));
	bool useUtil = server->getConfig()->isSet("ssp.util"); 
	for (size_t i = 0; i < lines.size(); ++i) {
		std::vector<std::string> line;
		boost::split(line, lines[i], boost::is_any_of("\t"));
		if (line.size() < 3)
			continue;
		if (!useUtil && (line[2].compare("<s>") == 0 || line[2].compare("</s>") == 0))
			continue;

		char *value = const_cast<char *>(line[2].c_str());
		if (value[0] == '#')
			++value;

		mlf_data.append(line[0]);
		mlf_data.push_back('\t');
		mlf_data.append(line[1]);
		mlf_data.push_back('\t');
		mlf_data.append(line[2]);
		if (useUtil) {
			mlf_data.push_back('\t');
			mlf_data.append(line[3]);
		}
		mlf_data.push_back('\n');
	}

	// FIXME: 임시로 파일 저장 
	std::string pathname(tmp_path);
	pathname.append(gearman_job_handle(job));
	pathname.append(".mlf");
	if (save_data(pathname, mlf_data.size(), mlf_data.c_str())) {
		job_log->error("[%s] Fail to recieve data", job_name);
		return GEARMAN_ERROR;
	}

	std::string result;
	try {
		if (server->ssp(pathname, result)) {
			job_log->error("[%s] Fail to ssp", job_name);
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}
		std::remove(pathname.c_str());
	} catch(std::exception &e) {
		job_log->error("[%s] Fail to ssp, %s", job_name, e.what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	} catch(std::exception *e) {
		job_log->error("[%s] Fail to ssp, %s", job_name, e->what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 결과 전송 
	job_log->debug("[%s] Done: %d bytes", job_name, result.size());
	gearman_return_t ret = gearman_job_send_complete(job, result.c_str(), result.size());
	if (gearman_failed(ret)) {
		job_log->error("[%s] Fail to send result", job_name);
		return GEARMAN_ERROR;
	}

	return GEARMAN_SUCCESS;
}

/**
 * @brief		Realtime STT 요청 
 * @author		Kijeong Khil (kjkhil@itfact.co.kr)
 * @date		2017. 03. 14. 10:15:34
 * @return		Upon successful completion, a GEARMAN_SUCCESS is returned.\n
 				Otherwise, a GEARMAN_ERROR is returned.
 * @see			job_unsegment()
 */
static gearman_return_t job_rt_stt(gearman_job_st *job, void *context) {
	const char *workload = (const char *) gearman_job_workload(job);
	const size_t workload_size = gearman_job_workload_size(job);
	std::string __job_name(COLOR_BLACK_BOLD);
	__job_name.append("RT:");
	__job_name.append(gearman_job_handle(job));
	__job_name.append(COLOR_NC);
	const char *job_name = __job_name.c_str();
	VRServer *server = (VRServer *) context;

	job_log->info("[%s] Recieved %d bytes", job_name, workload_size);
	if (workload_size < 10) {
		job_log->error("[%s] The file size is too small (< 5 bytes)", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 명령어 해석 
	// CALL_ID | CMD | DATA
	char *tmp = const_cast<char *>(workload);
	tmp = strchr(tmp, '|');
	if (tmp == NULL) {
		job_log->error("[%s] Cannot find Call ID", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	int idx_call_id = tmp - workload;
	std::string call_id(workload, idx_call_id);

	tmp = strchr(tmp + 1, '|');
	if (tmp == NULL) {
		job_log->error("[%s] Invalid argument", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	int idx_cmd = tmp - workload;
	std::string cmd(workload + idx_call_id + 1, idx_cmd - idx_call_id - 1);
	char state = 1;

	if (cmd.find("FIRS") == 0) state = 0;
	else if (cmd.find("LAST") == 0) state = 2;

	short *data = (short *) (tmp + 1);
	size_t size = (workload_size - idx_cmd - 1) / sizeof(short);

	// DEBUG, fvad를 이용하여 음성 데이터 처리 확인
	if (0) {
		struct timeval tv;
		char dist[32];

		gettimeofday(&tv, NULL);
        sprintf(dist, "%ld.%ld", tv.tv_sec, tv.tv_usec);
		std::string filename = call_id + std::string("_") + std::string(dist) + std::string(".pcm");
		std::ofstream pcmFile;

		pcmFile.open(filename, std::ofstream::out | std::ofstream::binary);
		if (pcmFile.is_open()) {
			pcmFile.write((const char*)data, size * sizeof(short));
			pcmFile.close();
		}
	}
#if 0
	// FIXME: 바이트 배열 변경 
	for (size_t i = 0; i < size; ++i)
		data[i] = ntohs(data[i]);
#endif
	job_log->debug("[%s] Call ID: %s[%s], length: %lu, state(%d)", job_name, call_id.c_str(), cmd.c_str(), size, state);
	// STT
	std::string cell_data = "";
	if (server->stt(call_id, data, size, (const char)state, cell_data) == EXIT_FAILURE) {
		job_log->error("[%s] Fail to stt", job_name);
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}

	// 결과 전송 
	job_log->debug("[%s] Done: %d bytes", job_name, cell_data.size());
#if 1
	std::string text;
	try {
		if (server->unsegment(cell_data, text)) {
			job_log->error("[%s] Fail to unsegment", job_name);
			gearman_job_send_fail(job);
			return GEARMAN_ERROR;
		}
	} catch(std::exception &e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e.what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	} catch(std::exception *e) {
		job_log->error("[%s] Fail to unsegment, %s", job_name, e->what());
		gearman_job_send_fail(job);
		return GEARMAN_ERROR;
	}
	gearman_return_t ret = gearman_job_send_complete(job, text.c_str(), text.size());
#else
	gearman_return_t ret = gearman_job_send_complete(job, cell_data.c_str(), cell_data.size());
#endif

	if (gearman_failed(ret)) {
		job_log->error("[%s] Fail to send result", job_name);
		return GEARMAN_ERROR;
	}

	return GEARMAN_SUCCESS;
}
