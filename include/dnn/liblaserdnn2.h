/** @file	liblaserdnn.h
 * 	@brief	ESTkDNN LASER API.
 *
 * 	Written by HOON CHUNG (hchung@etri.re.kr)
 */

#ifndef	_LIB_LASER_DNN2_H
#define	_LIB_LASER_DNN2_H

#ifdef __cplusplus
extern "C" {
#endif

void *createDNN2( int useGPU, int idGPU, char *dnndefFn, char *normFn, char *priorFn, float prWgt );
void *createDNN2Ext( int useGPU, int idGPU, char *dnndefFn, char *normFn, char *priorFn, float prWgt, int miniBatch );
void freeDNN2( void *p );
void calcDNN2SetLog( void *p, int a_dim, float *a_Ot, int a_N, float *a_bjotP );
void calcDNN2SetLogExt( int t, void *p, int a_dim, float *a_Ot, int a_N, float *a_bjotP );

void *createDNN2ExtChild( void *, int useGPU, int idGPU, char *dnndefFn, char *normFn, char *priorFn, float prWgt, int miniBatch );
void freeDNN2Child( void *p );

#ifdef __cplusplus
}
#endif

#endif
