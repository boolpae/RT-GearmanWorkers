/*! \file frontend_api.h
 * \brief LFrontEnd용 사용자 헤더파일.

 * ETRI LASER 음성 인식기에 사용되는 프론트엔드 모듈인 LFrontEnd를
 * 사용하기 위한 사용제 헤더파일이다.
 * LFrontEnd를 사용하기 위해서는 아래와 같은 방식으로 필요한 함수를 호출할 수 있다.
 * \verbatim
    LFrontEnd* pFront;
    pFront = createLFrontEnd();
    if ( pFront == NULL )
        return -1; // 생성 실패 오류
    if ( readOptionLFrontEnd( pFront, "frontend.cfg" ) )
        return -1; // frontend.cfg 에서 옵션을 정상적으로 읽을 수 없음

    while( end_of_program )
    {
        resetLFrontEnd( pFront );
        while( end_of_utterance )
        {
            n = read_samples_from_audio_device( data ); // n은 읽어온 샘플의 개수
            ret = stepFrameLFrontEnd( pFront, n, data, &m, out );
            send_extracted_feature_to_recognize( out, m );
        }
    } \endverbatim
 *
 * 상세한 내용은 사용자 설명서를 참고하도록 한다.
 */

#ifndef __FRONTEND_API_H_
#define __FRONTEND_API_H_
#ifdef __cplusplus
extern "C" {
#endif

#define FRONTEND_OPTION_8KHZFRONTEND    0x0001

/// LFrontEnd 객체 정의.
/// 전처리에 필요한 메모리 등을 객체별로 저장하고 있다.
/// 하나의 프로세스에서 여러개의 객체를 서로 다른 옵션으로 동작시키는 것도 가능하며
/// 각 객체는 독립적으로 동작한다.
typedef void LFrontEnd;

/// stepFrameLFrontEnd 함수에서 반환하는 값
typedef     enum { 
	noise = 0x00,  //!< 현재 입력된 프레임이 잡음 구간임
	detecting = 0x01, //!< 현재 입력된 프레임이 음성 구간임
	detected = 0x02, //!< 현재 입력된 프레임이 음성의 종료 구간임
	reset = 0x04,  //!< LFrontEnd가 reset되었음. 검출된 음성구간이 너무 짧아서 음성이 아닌 것으로 간주할 때에 해당됨.
    onset = 0x08, //!< 음성신호가 시작되었음. 실제시작점은 아니나, 디코더에 시작점이 될 수 있는 que를 제공.
                  //!< 이 상태에서는 내부에 버퍼링된 이전 프레임에 해당하는 특징벡터도 같이 출력된다.
                  //!< 그 길이는 옵션에 따라 달라지며 대략 수십 프레임정도에 해당하는 특징벡터가 출력된다.
    offset = 0x10, //!< 음성신호가 종료되었음. 디코더에 끝점이 될 수 있는 que를 제공
    timeout = 0x20,
    restart = 0x40
} FSTATUS;

#define FRONTEND_COPYFEATUREVECTOR_ASIS           0
#define FRONTEND_COPYFEATUREVECTOR_MEANSUBTRACTED 1
#define FRONTEND_COPYFEATUREVECTOR_STATICONLY     2
#define FRONTEND_COPYFEATUREVECTOR_MFCC39         3
#define FRONTEND_COPYFEATUREVECTOR_MFCC52         4
#define FRONTEND_COPYFEATUREVECTOR_MFCC53         5


#ifdef _USRDLL
#define APITYPE __declspec (dllexport)
#else
#define APITYPE
#endif

/// LFrontEnd 객체를 생성하고, 동작에 필요한 메모리를 할당한다.
///
/// @return 생성된 객체의 포인터를 반환한다. 에러가 있으면 NULL을 반환
///
APITYPE LFrontEnd	* 	createLFrontEnd( );


/// LFrontEnd 객체를 생성하고, 동작에 필요한 메모리를 할당한다.
/// createLFrontEnd 함수와 동일한 기능을 하나 옵션을 줄 수 있다.
/// 현재 구현된 옵션은 아래와 같다.
///     if ( a_opt & FRONTEND_OPTION_8KHZFRONTEND ) 이면 8k 프론트엔드를 생성하여 반환한다.
///
/// @return 생성된 객체의 포인터를 반환한다. 에러가 있으면 NULL을 반환
///
APITYPE LFrontEnd	* 	createLFrontEndExt( int a_opt );

/// 발화단위의 초기화를 수행한다.
///
/// 설정된 옵션에 따라 동작에 필요한 변수들의 초기값 설정 및 버퍼 초기화 수행한다.
/// 매 발화마다 호출해야 한다.
APITYPE int 	resetLFrontEnd( LFrontEnd* a_pFront );

/// 발화단위의 초기화를 수행한다. lookback 옵션을 줄 수 있다.
///
/// resetLFrontEnd함수와 기본적인 기능은 동일하며
/// 특별히 lookback 옵션을 줄 수 있다.
/// lookback 파라미터가 0보다 크게 주어진 경우, 끝점이 검출된 이후 다음 세션을 진행할때,
/// 이번에 검출된 발화의 끝부분 중 이 숫자만큼의 frame을 재사용하게된다.
/// 현재 EPD status가 detected인경우에만 적용된다. 그렇지 않은 경우 resetLFrontEnd와 동일
APITYPE int 	resetLFrontEndExt( LFrontEnd* a_pFront, int a_framelen_lookback );

/// 주어진 입력신호를 처리하여 특징벡터를 출력한다.
///
/// @param a_pFront 생성된 LFrontEnd 객체의 포인터
/// @param a_ilen 입력신호의 길이. 0보다 같거나 큰 임의의 정수. 
///		(a_sig == NULL인 경우에는 0이어야한다. ) 
/// @param a_sig 입력신호 배열의 포인터. NULL은 신호의 끝을 의미.
/// @param a_olen 출력벡터의 길이. 39차 배열이 10개 인경우에는 390을 출력
/// @param a_out 출력벡터를 담을 배열의 포인터. 미리 할당 되어있어야 한다.
///		내부에서 메모리 overflow는 check하지 않으므로 충분히 할당 되어있어야 한다
/// @return FSTATUS값 중 하나 이상이 or되어 출력된다.
///     입력신호가 1 frame (160samples)인 경우 noise, detecting,
//      detected, reset, onset, offset 중 하나의 신호가 나오며
//      각 결과에 TIMEOUT 옵션에 따라 timeout이 or되어 출력될 수 있다.
//      입력신호가 2 frame이상인 경우 각 프레임의 결과값이 모두 or되어 출력되고,
//      각 event의 순서는 알 수 없다. 따라서 이후 과정은
//      출력되는 특징벡터의 길이 정보에 기초해서 처리해야한다.
//
APITYPE int 	stepFrameLFrontEnd( LFrontEnd* a_pFront, 
					int a_ilen, short* a_sig, int* a_olen, float* a_out );

/// 생성된 메모리를 해제한다.
///
APITYPE void 	closeLFrontEnd( LFrontEnd* a_pFront );

/// LFrontEnd의 동작방식을 설정한다.
/// 허용가능한 옵션 및 디폴트 값은 사용자 설명서를 참고한다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_key 설정하고자 하는 옵션의 이름.
/// @param a_val 설정하고자 하는 옵션의 값.
/// @return 성공하면 0을, 실패하면 -1을 반환한다. 옵션의 이름이 잘 못 전달된 경우, 
/// 또는 잘못된 값이 전달된 경우에 -1을 반환한다.
///
APITYPE int 	setOptionLFrontEnd( LFrontEnd* a_pFront, char* a_key, char* a_val );

/// LFrontEnd의 현재 설정된 옵션을 가져온다.
/// setOptionLFrontEnd() 함수에 사용하는 키값을 사용할 수 있다.
///
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_key 가져오가자 하는 옵션
/// @param a_val 옵션값을 담아올 문자열의 포인터. 메모리는 충분히 할당되어있다고 가정하고
/// 별도의 체크는 하지 않는다. 호출하는 쪽에서 반환되는 값을 예상하여 충분한 메모리를 잡도록 한다.
/// @return 성공하면 0, 그렇지 않으면 -1을 반환한다. 
///
APITYPE int 	getOptionLFrontEnd( LFrontEnd* a_pFront, char* a_key, char* a_val );

/// LFrontEnd의 내부의 파라미터값을 가져올 때 사용한다.
///
/// 현재 FRONTEND_OUTDIM 을 사용하여 현재 설정에서 반환되는 특징벡터의 차수를 알 수 있다.
/// 현재 가져올 수 있는 파라미터로는 FRONTEND_OUTDIM, EPD_PEAKVALUE 등이 있다.
///
/// @param a_pFront LFrontEnd 객체의 포인터
/// @param a_key 가져오고자하는 파라미터의 키값
/// @param a_val 파라미터값을 담을 정수형 변수의 포인터
/// @return 성공하면 0, 실패하면 -1을 반환한다. 실패하는 경우는 적당한 키값이 주어지지 않은 경우이다.
APITYPE int 	getIntValueLFrontEnd( LFrontEnd* a_pFront, char* a_key, int* a_val );

/// 파일로부터 옵션을 읽어온다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_fname 파일명.
/// @return 성공하면 0, 실패하면 0이 아닌값을 반환한다.
APITYPE int 	readOptionLFrontEnd ( LFrontEnd* a_pFront, char* a_fname );

/// 프레임 에너지를 가져온다.
/// 직전에 호출된 stepFrameLFrontEnd 에 넘겨진 음성 신호의 피크값을 추정한다.
/// 음성신호의 피크값은 단순 추정이며 일반적으로 음성신호의 envelope을 기준으로 최대값보다
/// 약간 낮은 값으로 추정된다. 따라서 가능한 값의 범위는 0 -- 32767 이며 
/// 32767보다 큰 경우에는 32767 (2byte short형 정수형 최대값) 으로 제한된다.
/// @return 추정된 피크값
APITYPE int 	getLFrameEnergy( LFrontEnd* a_pFront );

/// @return LFrontEnd의 버전 문자열을 반환한다. 버전 문자열은 컴파일시에 정해지며,
/// 컴파일 옵션에 따라 설정되어있지 않을 수도 있다.
APITYPE char*	getLFrontEndVersion();

/// 리셋이후부터 현재까지 검출된 음성 신호 버퍼의 포인터를 반환한다.
/// 시작점부터 현재 입력까지의 모든 신호를 저장하고 있는 버퍼의 포인터를 반환한다.
/// 연속해서 호출할 경우 계속해서 처음부터 현재까지의 신호를 가져오게 된다.
/// 즉, 매 프레임 호출할 경우, 매 프레임에 해당하는 검출된 신호를 가져오는 것이 아니라,
/// 항상 음성의 시작점부터 현재까지의 신호를 반환한다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_len 반환된 입력신호의 길이
/// @return 음성신호의 포인터
APITYPE short* getLDetectedSignal( LFrontEnd* a_pFront, unsigned int* a_len );

/// 리셋이후부터 현재까지 검출된 음성 신호를 복사하여 반환한다.
/// 연속해서 호출할 경우 계속해서 처음부터 현재까지의 신호를 가져오게 된다.
/// 즉, 매 프레임 호출할 경우, 매 프레임에 해당하는 검출된 신호를 가져오는 것이 아니라,
/// 항상 음성의 시작점부터 현재까지의 신호를 반환한다.
/// a_maxolen 이 반환할 신호의 길이보다 작은 경우 a_maxolen만큼만 반환한다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_osig 신호를 저장하여 가져올 포인터
/// @param a_maxolen a_osig의 최대 길이
/// @return 반환된 음성신호의 길이
APITYPE unsigned int getLDetectedSignalExt( LFrontEnd* a_pFront, short* a_osig, unsigned int a_maxolen );

/// 리셋이후부터 현재까지 검출된 음성 신호의 길이를 반환한다.
APITYPE unsigned int getLDetectedSignalLength( LFrontEnd* a_pFront );

/// 리셋 이후부터 현재까지 추출된 피쳐 벡터의 길이를 반환한다.
/// 프레임길이가 아니라, 실제 데이터의 길이이다. 즉 39차 벡터가 10프레임인 경우
/// 390을 반환한다.
APITYPE unsigned int getLFeatureLength( LFrontEnd* a_pFront );

/// 리셋 이후부터 현재까지 추출된 피쳐 벡터를 복사하여 반환한다.
/// 연속해서 호출할 경우 계속해서 처음부터 현재까지의 피쳐 벡터를 모두 
/// 복사하여 반환한다.
/// 즉, 매 프레임 호출할 경우, 매 프레임에 해당하는 추출된 특징벡터를 가져오는 것이 아니라,
/// 항상 지난번 리셋이후부터 현재까지 추출된 특징벡터를 반환한다.
/// a_maxolen 이 반환할 신호의 길이보다 작은 경우 a_maxolen만큼만 반환한다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param a_ofeat 신호를 저장하여 가져올 포인터
/// @param a_maxolen a_osig의 최대 길이
/// @return 반환된 피쳐벡터의 길이
APITYPE unsigned int getLFeature( LFrontEnd* a_pFront, float* a_ofeat, unsigned int a_maxolen );

/// 리셋 이후부터 현재까지 추출된 피쳐 벡터를 저장하는 버퍼의 포인터를 반환한다.
/// MFCC_OPTION_STOREMFCCINMEMORY 옵션이 활성화되어 있어야 한다. 
/// 반환된 피쳐 벡터의 길이는 getLFeatureLength 함수로 알 수 있다. 
/// @return 피쳐벡터 버퍼의 포인터
APITYPE float* getLFeatureBuffer( LFrontEnd* a_pFront );

/// 리셋이후부터 현재까지 검출된 음성 신호 버퍼의 포인터를 반환한다.
/// getLDetectedSignal() 함수와 동일하나, 이름만 직관적으로 변경하였다.
APITYPE short* getLDetectedSignalBuffer( LFrontEnd* a_pFront, unsigned int* a_len );

/// 리셋이후부터 현재까지 입력된 음성 신호 버퍼의 포인터를 반환한다.
APITYPE short* getLInputSignalBuffer( LFrontEnd* a_pFront, unsigned int* a_len );

/// 리셋이후부터 현재까지 입력된 음성 신호를 복사하여 반환한다.
/// @return 반환된 음성신호의 길이
APITYPE unsigned int getLInputSignalExt( LFrontEnd* a_pFront, short* a_osig, unsigned int a_maxolen );

/// 리셋이후부터 현재까지 입력된 음성 신호의 길이를 반환한다.
APITYPE unsigned int getLInputSignalLength( LFrontEnd* a_pFront );

/// 내부 버퍼에 있는 입력신호 중 최근의 a_len 샘플만큼을 반환한다.
/// @return 반환된 음성신호의 길이
APITYPE unsigned int getLLastInputSignal( LFrontEnd* a_pFront, unsigned int a_len, short* a_osig );

/// 끝점검출된 신호의 시작점과 끝점을 프레임 단위로 반환한다.
/// 끝점이 검출되지 않은 상태에서 호출할 경우 마지막 입력된 신호의 시간을 반환한다.
/// 출력되는 끝점 값은 trailing silence를 고려하여 적당한 음성의 끝점이 반환된다.
/// 주로 파일 입력을 사용한 배치 테스트에서 끝점 추출기의 성능을 평가하기 위해서 사용한다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param b 검출된 신호의 시작점이 담길 정수 포인터
/// @param e 검출된 신호의 끝점이 담길 정수 포인터
/// @return 항상 0을 반환한다. 
APITYPE int getLSpeechInterval( LFrontEnd* a_pFront, int* b, int* e );

/// 끝점검출된 신호의 시작점과 끝점을 프레임 단위로 반환한다.
/// 끝점이 검출되지 않은 상태에서 호출할 경우 마지막 입력된 신호의 시간을 반환한다.
/// 출력되는 끝점 값은 trailing silence를 고려하여 적당한 음성의 끝점이 반환된다.
/// @param a_pFront 프론트엔드 객체의 포인터
/// @param b 검출된 신호의 시작점이 담길 정수 포인터
/// @param e 검출된 신호의 끝점이 담길 정수 포인터
/// @param makeup_end 1인 경우 끝점을 trailing silence를 고려하여 가공한다.
/// @return 항상 0을 반환한다. 
APITYPE int getLSpeechIntervalExt( LFrontEnd* a_pFront, int* b, int* e, int makeup_end );

/// LFrontEnd의 모든 옵션리스트 및 각각의 값을 stdout에 출력한다.
/// a_pFront 가 NULL인 경우 각 옵션의 디폴트 값을
/// NULL이 아닌 경우 현재 설정된 값을 출력한다.
/// 오류가 있는 경우 stderr에 출력한다.
APITYPE void getAllOptionsLFrontEnd( LFrontEnd* a_pFront );

/// 현재 버퍼에 저장된 특징벡터를 복사하여 반환한다.
/// float array의 포인터를 반환하며 반환된 array는 caller가 free해 주어야 한다.
/// 모드에 따라 아래와 같은 형식으로 반환한다.
///   - mode = FRONTEND_COPYFEATUREVECTOR_ASIS: 버퍼에 있는 형태 그대로 반환 (eg. 39차 online cms)
///   - mode = FRONTEND_COPYFEATUREVECTOR_MEANSUBTRACTED: 
///      mode0에서 static 부분의 mean을 제거하여 반환 (eg. 39차 gcms와 유사)
///   - mode = FRONTEND_COPYFEATUREVECTOR_STATICONLY:
///      cms하기 이전의 static feature만 반환 (eg. 13차 static, no cms)
///   - mode = FRONTEND_COPYFEATUREVECTOR_MFCC39: 39차 global cms를 적용하여 반환
///     (static mfcc -> global cms -> delta 재계산)
///   - mode = FRONTEND_COPYFEATUREVECTOR_MFCC52:
///      52차 global cms를 적용하여 반환 (static mfcc -> global cms -> dynamic 재계산)
///   - mode = FRONTEND_COPYFEATUREVECTOR_MFCC53:
///      53차 global cms를 적용하여 반환 (static mfcc -> global cms -> dynamic 재계산)
/// 필요한 설정이 세팅되지 않아 특징 벡터를 계산할 수 없는 경우,
///      *a_len을 0으로 하고 NULL을 리턴한다. 
/// mfcc는 online 시 계산된 것을 이용하며 cms, delta/dynamics는 재계산하게 된다. 
/// 이때  가능한 기본 옵션에 따라 계산된다. ( POLEFILTERING 사용하지 않음 주의 )
/// 일반적인 경우에 mode0를 이용하여 전체 특징벡터를 얻은 이후에
/// static 부분의 mean을 계산하여 빼는 것으로 대신할 수 있다.
/// @param a_pFront LFrontEnd 객체 포인터
/// @param mode 출력형태의 모드
/// @param a_len 출력버퍼의 길이( dimension * framelength )
/// @return 동적할당된 float array 포인터 (caller가 free해야 함)
APITYPE float* copyLFeatureVector( LFrontEnd* a_pFront, int mode, unsigned int* a_len );
#ifdef __cplusplus
}
#endif
#endif
