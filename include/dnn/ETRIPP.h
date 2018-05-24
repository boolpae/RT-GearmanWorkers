#ifndef		_ETRIPP_H
#define		_ETRIPP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined( _WIN32 ) || defined( _WIN32_CE )
#ifdef	_USRDLL
#define APITYPE __declspec (dllexport)
#else
#define APITYPE
#endif
#else
#define APITYPE
#endif

APITYPE	int SPLPostProc ( char *instr, char *outstr );
APITYPE	int SPLPostProcMLF ( char *in_mlf_fn, char *out_fn );
APITYPE int SPLPostProcSentenceSegment ( char *instr, char *outstr );
APITYPE int SPLPostProcSentenceSegment_POS ( char *instr, char *outstr );
APITYPE	int createSPLPostProc ( const char *, const char *, const char * );
APITYPE int Lat2cnWordNbestOutInit (const char *, const char *, const char *, int );
APITYPE	void closeSPLPostProc();

#ifdef __cplusplus
}
#endif

#endif
