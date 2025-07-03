/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMBASE64_H__
#define __DRMBASE64_H__

#include <drmmathsafe.h>

ENTER_PK_NAMESPACE;

/*
   Using decode in place will cause the decoder to place the decoded output in the
   last *pcbDestination bytes of pwszSource->pwszString
*/
#define DRM_BASE64_DECODE_NO_FLAGS 0
#define DRM_BASE64_DECODE_IN_PLACE 1

/* No encoding flags are lwrrently supported */
#define DRM_BASE64_ENCODE_NO_FLAGS 0

/* characters required for a binary of size cb expressed as base64*/
#define CCH_BASE64_EQUIV(cb)          ((((cb)/3)+(((cb)%3)?1:0))*4)
/* SAL does not accept modulo% operand, introducing another macro for SAL */
#define CCH_BASE64_EQUIV_SAL(cb)      ((((cb)/3)+(((cb)-((cb)/3)*3)?1:0))*4)
#define CB_BASE64_DECODE(cch)         (((cch)*3)/4)

#define CCH_BASE64_EQUIV_SAFE(__cb,__cch) DRM_DO {                  \
    __cch = ( ( ( __cb ) / 3 ) + ( ( ( __cb ) % 3 ) ? 1 : 0 ) );    \
    ChkDR( DRM_DWordMult( ( __cch ), 4, &( __cch ) ) );             \
} DRM_WHILE_FALSE

#define CB_BASE64_DECODE_SAFE(__cch,__cb) DRM_DO {      \
    ChkDR( DRM_DWordMult( ( __cch ), 3, &( __cb ) ) );  \
    ( __cb ) /= 4;                                      \
} DRM_WHILE_FALSE

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER, "f_pbDestination length defined by f_pcbDestination and f_pdasstrSource" )
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER_25057, "f_pbDestination length defined by f_pcbDestination and f_pdasstrSource" )
DRM_API DRM_RESULT DRM_CALL DRM_B64_DecodeA(
    __in_ecount( f_pdasstrSource->m_ich + f_pdasstrSource->m_cch ) const DRM_CHAR *f_pszBase,
    __in                          const DRM_SUBSTRING  *f_pdasstrSource,
    __inout                             DRM_DWORD      *pcbDestination,
    __out_bcount_opt( ( dwFlags & DRM_BASE64_DECODE_IN_PLACE ) == 0 ? *pcbDestination : f_pdasstrSource->m_ich + *pcbDestination ) DRM_BYTE *pbDestination,
    __in                                DRM_DWORD       dwFlags);
PREFAST_POP
PREFAST_POP

DRM_API DRM_RESULT DRM_CALL DRM_B64_DecodeW(
    __in                          const DRM_CONST_STRING *pdstrSource,
    __inout                             DRM_DWORD        *pcbDestination,
    __out_bcount_opt( *pcbDestination ) DRM_BYTE         *pbDestination,
    __in                                DRM_DWORD         dwFlags);

DRM_API DRM_RESULT DRM_CALL DRM_B64_EncodeA(
    __in_bcount( cbBuffer )    const DRM_BYTE  *pvBuffer,    /* input buffer */
    __in                             DRM_DWORD  cbBuffer,    /* input len */
    __out_ecount_opt( *pcchEncoded ) DRM_CHAR  *pszEncoded,  /* output char */
    __inout                          DRM_DWORD *pcchEncoded, /* output ch len */
    __in                             DRM_DWORD  dwFlags );

DRM_API DRM_RESULT DRM_CALL DRM_B64_EncodeW(
    __in_bcount( f_cbSource )    const DRM_BYTE  *f_pbSource,    /* input buffer */
    __in                               DRM_DWORD  f_cbSource,    /* input len */
    __out_ecount_opt( *f_pcchEncoded ) DRM_WCHAR *f_pwszEncoded, /* output WCHAR */
    __inout                            DRM_DWORD *f_pcchEncoded, /* output ch len */
    __in                               DRM_DWORD  f_dwFlags );

#define DRM_B64_DECODEA_SIMPLE( __cchB64, __pszB64, __cb, __pb, __cbL ) DRM_DO {        \
    DRM_SUBSTRING __drmsubstr = DRM_EMPTY_DRM_SUBSTRING;                                \
    __cbL = __cb;                                                                       \
    __drmsubstr.m_cch = (DRM_DWORD)__cchB64;                                            \
    ChkDR( DRM_B64_DecodeA( __pszB64, &__drmsubstr, &__cbL, __pb, 0 ) );                \
} DRM_WHILE_FALSE

EXIT_PK_NAMESPACE;

#endif /* __DRMBASE64_H__ */

