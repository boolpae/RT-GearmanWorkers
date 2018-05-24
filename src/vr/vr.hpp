/**
 * @headerfile	vr.hpp "vr.hpp"
 * @file	vr.hpp
 * @brief	VR Server
 * @author	Kijeong Khil (kjkhil@itfact.co.kr)
 * @date	2016. 06. 17. 17:35:46
 * @see		
 */

#ifndef __ITFACT_VR_SERVER_H__
#define __ITFACT_VR_SERVER_H__

#include "worker.hpp"
#include "frontend_api.h"
#include "Laser.h"

using namespace itfact::worker;

namespace itfact {
	namespace vr {
		namespace node {
			enum WAVE_FORMAT {
				STANDARD_WAVE,
				WAVE_2CH,
				WAVE,
				MPEG,
				UNKNOWN_FORMAT
			};
			class RealtimeSTT;

			int get_final_result(Laser *slaserP, std::size_t index, std::size_t &last_position,
				const std::size_t feature_dim, const std::size_t mfcc_size, float * const sil,
				std::string &buffer);
			int get_intermediate_results(Laser *laser, std::size_t index,
				std::size_t &skip_position, std::size_t last_position, std::size_t reset_period, std::string &buffer);

			class VRServer : public WorkerDaemon
			{
			public: // const
				static const unsigned long MAX_MINIBATCH = 1024;
				static const unsigned long DEFAULT_ENGINE_CORE = 10;
				static const unsigned long LDA_LEN_FRAMESTACK = 15;

			private: // Member
				std::shared_ptr<std::thread> monitoring_thread;
				Laser *masterLaserP = NULL;
				float *sil = NULL;
				std::map<std::string, std::shared_ptr<RealtimeSTT>> channel;

				// ----------
				std::size_t mfcc_size = 600;
				std::size_t mini_batch = 128;
				double prior_weight = 0.8L;
				bool useGPU = true;
				long idGPU = 0;
				std::size_t feature_dim;
				std::size_t read_size;

				std::string frontend_config;
				std::string am_file;
				std::string fsm_file;
				std::string sym_file;
				std::string dnn_file;
				std::string prior_file;
				std::string norm_file;

			public:
				VRServer() : WorkerDaemon() {};
				VRServer(const int argc, const char *argv[]) : WorkerDaemon(argc, argv) {};
				~VRServer();
				virtual int initialize() override;
				int stt(const short *buffer, const std::size_t bufferLen, std::string &result);
				int unsegment(const std::string &data, std::string &result);
				int unsegment_with_time(const std::string &mlf_file, const std::string &unseg_file);
				int ssp(const std::string &mlf_file, std::string &buf);
				static enum WAVE_FORMAT check_wave_format(const short *data, const size_t data_size);

				// For Real-time
				int stt(const std::string &call_id, const short *buffer, const std::size_t bufferLen,
						const char state, std::string &result);

			private:
				int monitoring(std::shared_ptr<std::string> path);
				bool load_laser_module();
				void unload_laser_module();

				// For Real-time
				int create_channel(const std::string &call_id);
				int close_channel(const std::string &call_id);
			};

			class RealtimeSTT
			{
			private:
				log4cpp::Category *job_log = NULL;
				std::shared_ptr<float> feature_vector;
				std::shared_ptr<LFrontEnd> front;
				std::shared_ptr<Laser> laser;
				float *temp_buffer = NULL;//short *temp_buffer = NULL;
				std::size_t temp_buffer_len = 0;
				std::size_t running = 0;
				std::size_t reset_period;
				std::size_t index = 0;
				std::size_t skip_position = 0;
				std::size_t last_position = 0;

				// ----------
				std::size_t mfcc_size = 600;
				std::size_t mini_batch = 256;
				std::size_t minimum_size;
				std::size_t feature_dim;
				float *sil = NULL;

			public:
				RealtimeSTT(std::shared_ptr<float> buffer,
							std::shared_ptr<LFrontEnd> frontend,
							std::shared_ptr<Laser> child_laser,
							const std::size_t a_mfcc_size,
							const std::size_t a_mini_batch,
							float *a_sil,
							log4cpp::Category *logger = &log4cpp::Category::getRoot());
				~RealtimeSTT();

				void set_reset_period(const std::size_t period);
				int stt(const short *buffer, const std::size_t buffer_len, std::string &result);
				int free_buffer(std::string &result);

			private:
				RealtimeSTT();
			};
		}
	}
}

#endif /* __ITFACT_VR_SERVER_H__ */
