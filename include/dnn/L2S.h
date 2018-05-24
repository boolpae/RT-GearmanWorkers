#ifndef    _ETRI_EK_G2P_H
#define    _ETRI_EK_G2P_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined( _WIN32 ) || defined( _WIN32_CE )
#ifdef  _USRDLL
#define APITYPE __declspec (dllexport)
#else
#define APITYPE
#endif
#else
#define APITYPE
#endif


#define MAX_G2P_PRON_LEN   128
#define MAX_G2P_RESULT     32

#pragma pack(push, 1)
typedef struct {
	char w[MAX_G2P_RESULT][MAX_G2P_PRON_LEN];
	char p[MAX_G2P_RESULT][MAX_G2P_PRON_LEN];
	int  sz;
} G2Pret;
#pragma pack(pop)

typedef std::map<std::string, int> freq_map;

APITYPE bool G2PInit(const char*);
APITYPE void G2PDo(const char*, G2Pret*);
APITYPE void G2PDoFile(FILE *f, freq_map fm);
APITYPE void G2Pclear();


#ifdef __cplusplus
}
#endif
#endif

