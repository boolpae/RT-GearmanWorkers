/** @file	Laser.h
 * 	@brief	Multi-stage decoder.
 *
 * 	Written by HOON CHUNG (hchung@etri.re.kr) and jgp@etri.re.kr
 */

#ifndef	_Laser_H
#define	_Laser_H

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

typedef		void	Laser;
typedef		int		boolType;
typedef		void	GrammerBuilder;


//////////////////////////////////////////////////////////////////////
//	SISD Laser related functions
//////////////////////////////////////////////////////////////////////
APITYPE	Laser		 	*	createALaser( char *a_aAm, char *a_aLat, char *a_aSym, char *a_iSym );
APITYPE	void		 		freeALaser( Laser *a_laserP );
APITYPE	int				 	resetALaser( Laser *a_laserP );
APITYPE	int					stepFrameALaser( Laser *a_laserP, int a_t, float *a_Ot );

APITYPE	char			*	getResultALaser( Laser *a_laserP, int a_t, int a_N, boolType a_showOsyms );
APITYPE	int					unloadFsmALaser( Laser *a_laserP );
APITYPE	int					loadFsmALaser( Laser *a_laserP, char *a_latFn, char *a_symFn );

APITYPE	int					readALaserConfig( Laser *a_laserP, char * );
APITYPE void				setALaserConfig( Laser *a_laserP, char *, char * );
APITYPE void				getALaserConfig( Laser *a_laserP, char *, char * );

// Multi alternative hypotheses frame-sync decoding related functions.
APITYPE	void			**	enableALaserMultiAltHypos( Laser *a_laserP, int a_hypoN, char **a_amP,
								char **a_agmmP, int a_scoreMode );
APITYPE	void				disableALaserMultiAltHypos( Laser *a_laserP, int a_hypoN, void **a_agsetP );
APITYPE	void			*	getALaserMultAltHypos( Laser *a_laserP, int a_t );

// Slot grammar support
APITYPE	GrammerBuilder	*	createALaserGrammerBuilder( char *a_psetFn, char *a_csetFn );
APITYPE	void				freeALaserGrammerBuilder( GrammerBuilder *a_bldP );
APITYPE int					attachALaserWord( Laser *a_laserP, GrammerBuilder *a_bldP,
								char *a_slotName, boolType a_first,
								char *a_wordName, int a_phnN, char **a_phnP );
APITYPE int                 attachALaserWordExt( Laser *a_laserP, GrammerBuilder *a_bldP,
								char *a_slotName, boolType a_first,
								char *a_wordName, int a_phnN, int *a_phnP );
APITYPE int					detachALaserWord( Laser *a_laserP,
								char *a_slotName, char *a_wordName );
APITYPE void				removeWordFromSymbolSet( Laser *a_laserP, void *a_wordName );
APITYPE	void				setLaserErrorHandleProc( Laser *a_laserP, void *a_proc );
APITYPE int					mt_resetSlot ( void *a_laserP, char *sname );

#if 1
//////////////////////////////////////////////////////////////////////
//	SIMD Laser related functions
//////////////////////////////////////////////////////////////////////
APITYPE	Laser		 	*	createSLaser( char *a_aAm, char *a_aLat, char *a_aSym );
APITYPE	void		 		freeSLaser( Laser *a_laserP );

APITYPE	Laser		 	*	createMasterSLaser( char *a_aAm, char *a_aLat, char *a_aSym );
APITYPE	void				freeMasterSLaser( Laser *a_laserP );
APITYPE void				setSLaserLBCores( int a_cores );

APITYPE	Laser		 	*	createChildSLaser( Laser *a_laserP );
APITYPE	void				freeChildSLaser( Laser *a_laserP );
APITYPE Laser			*   createChildSLaserExt( Laser *a_laserP, char *a_amFn );
APITYPE void				freeChildSLaserExt( Laser *a_laserP );


APITYPE	int				 	stepSARecFrameExt( Laser *a_laserP, int a_t, int featureDim, float *a_Ot );
APITYPE	int				 	resetSLaser( Laser *a_laserP );
APITYPE	int					stepFrameSLaser( Laser *a_laserP, int a_t, float *a_Ot );
APITYPE	char			*	getWBAdjustedResultSLaser( Laser *a_laserP, int a_t, int a_N, boolType a_showOsyms );
APITYPE	char			*	getResultSLaser( Laser *a_laserP, int a_t, int a_N, boolType a_showOsyms );

APITYPE	int					unloadFsmSLaser( Laser *a_laserP );
APITYPE	int					loadFsmSLaser( Laser *a_laserP, char *a_latFn, char *a_symFn );

APITYPE void				setSLaserConfig( Laser *a_laserP, char *, char * );
APITYPE void				getSLaserConfig( Laser *a_laserP, char *, char * );
APITYPE	int					readSLaserConfig( Laser *a_laserP, char * );
APITYPE int                 reallocSLaser( Laser *a_laserP );

#endif

#ifdef __cplusplus
}
#endif

#endif
