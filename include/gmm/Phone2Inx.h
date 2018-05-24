/** @file	Phone2Inx.h
 * 	@brief	Phone2Inx module.
 *
 * 	Written by HOON CHUNG (hchung@etri.re.kr)
 */

#ifndef	_Phone2Inx_H
#define	_Phone2Inx_H

#ifdef _cplusplus
extern "C" {
#endif

#if defined( _WIN32 ) || defined( _WIN32_CE )
#ifdef	_USRDLL
#define APITYPE __declspec (dllexport)
#else
#define APITYPE __declspec (dllimport)
#endif
#else
#define APITYPE
#endif

typedef	struct _Phone2Inx		Phone2Inx;

struct _Phone2Inx
{
	void			*	m_psetP;	/**< Pointer of PhoneSet */
	void			*	m_csetP;	/**< Pointer of ContextSet */
};

Phone2Inx *createPhone2Inx( char *a_psetFn, char *a_csetFn );

int freePhone2Inx( Phone2Inx *a_bldP );

int runPhone2Inx( Phone2Inx *a_bldP, int a_hmmbase, int a_phnN, char **a_phnP, int *a_osym  );

#ifdef _cplusplus
}
#endif

#endif
