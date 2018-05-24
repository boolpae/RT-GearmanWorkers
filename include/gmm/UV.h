/** @file	UV.h
 *
 * 	Utterance verification (UV) APIs
 *
 * 	Written by HOON CHUNG (hchung@etri.re.kr)
 *  Updated by JEOMJA KANG (jjkang@etri.re.kr)
 *   - MergeUVAlign(), 2011.07.13
 */

#ifndef	_UV_H
#define	_UV_H

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

#define		UV_OK					0
#define		UV_FAIL					-1

#define		UV_WORD_LEVEL			1
#define		UV_PHONE_LEVEL			2

#define		UV_ACCEPTED				0x00
#define		UV_REJECTED				0x01

typedef     void    				UV;
typedef 	struct _UVAlign   		UVAlign;
typedef 	struct _UVResult		UVResult;

/**
 * 	Alignment data 
 *
 */
#pragma pack(push, 1)
struct _UVAlign
{
	char    *			m_name;			/**< Segment name */
	int					m_st;			/**< Start frame number  */
	int					m_et;			/**< End frame number */
	int					m_N;			/**< Num. of alternative Hs */
	float 	*			m_H;			/**< Array of alternative Hs */
	struct  _UVAlign *	m_next;			/**< Pointer of next segment */
};
#pragma pack(pop)

#pragma pack(push, 1)
struct _UVResult
{
	char    *			m_name;			/**< Segment name */
	int					m_st;			/**< Start frame number  */
	int					m_et;			/**< End frame number */
	int					m_decision;		/**< Decision result */
	float 				m_confidence;	/**< Confidence score */
	struct _UVResult *	m_next;			/**< Pointer of next result */
};
#pragma pack(pop)

#define MAX_STR             256
#define MAX_MULTI_PRON      10
typedef struct __voc_str {
	char    name[MAX_STR];
	int     npron;
	int     pronNum[MAX_MULTI_PRON];
	int     pronIdx[MAX_MULTI_PRON][64];
} UVVOC;

APITYPE	UV *			CreateUV( char *szBaseDir );
APITYPE	void 			FreeUV( UV *pxUV );
APITYPE	UVResult *		GetUVResult( UV *pxUV, UVAlign *pxAlign, int iLevel, float *sentenceUVScore );
APITYPE	void			FreeUVResult( UVResult *pxResult );
APITYPE	void			ShowUVResult( FILE *, UVResult *pxResult );

APITYPE	void			FreeUVAlign( UVAlign *pxAlign );
APITYPE	void			ShowUVAlign( FILE *, UVAlign *pxAlign );

APITYPE int     		SetUVConfig( UV *pxUV, char *szKey, char *szVal );
APITYPE char *     		GetUVConfig( UV *pxUV, char *szKey );
APITYPE int     		ReadUVConfig( UV *pxUV, char *szCfgFn );
APITYPE void     		ShowUVConfig( FILE *, UV *pxUV );

APITYPE UVAlign *  		MergeUVAlign( UVAlign *pxAlign  );

APITYPE UVVOC *         CreateUVVOC(char *vocFName, int *numVoc, char *pset, char *cset, int numKey, int ascVoc);
APITYPE void            FreeUVVOC(UVVOC* uv_voc);


#ifdef __cplusplus
}
#endif

#endif
