/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMH264_H__
#define __DRMH264_H__

#include <drmteetypes.h>

ENTER_PK_NAMESPACE;

#define DRM_MAP_H264_ERROR( drOrig ) DRM_DO {   \
    DRM_RESULT _drTemp = (drOrig);              \
    if( _drTemp >= DRM_E_H264_MINIMUM           \
     && _drTemp <= DRM_E_H264_MAXIMUM )         \
    {                                           \
        dr = DRM_E_H264_PARSING_FAILED;         \
    }                                           \
    else                                        \
    {                                           \
        dr = _drTemp;                           \
    }                                           \
} DRM_WHILE_FALSE

/*
** Note: The functions declared in this file do NOT support exelwtion on a Big-Endian CPU.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_H264_FindSliceHeaderData(
    __in                                                DRM_DWORD         f_cbEncrypted,
    __in_bcount( f_cbEncrypted )                  const DRM_BYTE         *f_pbEncrypted,
    __in                                                DRM_DWORD         f_cEncryptedRegionMapping,
    __in_ecount( f_cEncryptedRegionMapping )      const DRM_DWORD        *f_pdwEncryptedRegionMapping,
    __out                                               DRM_DWORD        *f_pcbNaluLengthInfo,
    __deref_out_bcount( *f_pcbNaluLengthInfo )          DRM_BYTE        **f_ppbNaluLengthInfo,
    __out                                               DRM_DWORD        *f_pibToRemove,
    __out                                               DRM_DWORD        *f_pcbNaluTypes,
    __deref_out_bcount( *f_pcbNaluTypes )               DRM_BYTE        **f_ppbNaluTypes,
    __out                                               DRM_DWORD        *f_pcbSPSPPSData,
    __deref_out_bcount( *f_pcbSPSPPSData )              DRM_BYTE        **f_ppbSPSPPSData,
    __out                                               DRM_DWORD        *f_pcbUsedPortion,
    __deref_out_bcount( *f_pcbUsedPortion )             DRM_BYTE        **f_ppbUsedPortion,
    __out                                               DRM_DWORD        *f_pcdwOffsetData,
    __deref_out_ecount( *f_pcdwOffsetData )             DRM_DWORD       **f_ppdwOffsetData,
    __out                                               DRM_DWORD        *f_pcbSEIData,
    __deref_out_bcount( *f_pcbSEIData )                 DRM_BYTE        **f_ppbSEIData );

typedef DRM_API DRM_RESULT( DRM_CALL *LPFN_H264_MEM_ALLOCATE )(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __in                                            DRM_DWORD                   f_cbSize,
    __deref_out_bcount( f_cbSize )                  DRM_VOID                  **f_ppv );

typedef DRM_API_VOID DRM_VOID( DRM_CALL *LPFN_H264_MEM_FREE )(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __inout                                         DRM_VOID                  **f_ppv );

typedef DRM_API DRM_RESULT( DRM_CALL *LPFN_H264_MEM_GET_TIME )(
    __inout_opt                                     DRM_VOID                   *f_pCallbackContextAllowNULL,
    __out                                           DRMFILETIME                *f_pftSystemTime );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_H264_LocateAndVerifySliceHeaders(
    __in                                                      LPFN_H264_MEM_ALLOCATE  f_pfnMemAllocate,
    __in                                                      LPFN_H264_MEM_FREE      f_pfnMemFree,
    __in_opt                                                  LPFN_H264_MEM_GET_TIME  f_pfnGetTime,
    __inout_opt                                               DRM_VOID               *f_pCallbackContextAllowNULL,
    __in                                                      DRM_DWORD               f_cbFrame,
    __in_bcount( f_cbFrame )                            const DRM_BYTE               *f_pbFrame,
    __in                                                      DRM_DWORD               f_cbSPSPPSDictionary,
    __in_bcount_opt( f_cbSPSPPSDictionary )             const DRM_BYTE               *f_pbSPSPPSDictionary,
    __out                                                     DRM_DWORD              *f_pcbSPSPPSDictionaryUpdated,
    __deref_out_bcount_opt( *f_pcbSPSPPSDictionaryUpdated )   DRM_BYTE              **f_ppbSPSPPSDictionaryUpdated,
    __out                                                     DRM_DWORD              *f_pcbSliceHeaders,
    __deref_out_bcount_opt( *f_pcbSliceHeaders )              DRM_BYTE              **f_ppbSliceHeaders );

#define DRM_H264_CheckErrorWindow_IMPLEMENTED 1

#if DRM_H264_CheckErrorWindow_IMPLEMENTED
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_H264_CheckErrorWindow(
    __in    const DRMFILETIME  *f_pftLwrrentTime,
    __in          DRM_BOOL      f_fOutsideTEE );

#define DRM_H264_ERROR_WINDOW_IN_SECONDS    60

/*
** For Perf, DRM_H264_ERROR_WINDOW_IN_TICS was pre-callwlated.
** If either value is changed, make sure this expression is also updated.
**
** DRM_H264_ERROR_WINDOW_IN_TICS =
**              DRM_H264_ERROR_WINDOW_IN_SECONDS * C_TICS_PER_SECOND
*/
#define DRM_H264_ERROR_WINDOW_IN_TICS       600000000

#define DRM_H264_MAX_ERRORS_IN_WINDOW       600
#define DRM_H264_ERROR_USERMODE_FACTOR      0.85
#else   /* DRM_H264_CheckErrorWindow_IMPLEMENTED */
#define DRM_H264_CheckErrorWindow(x,y) DRM_SUCCESS
#endif  /* DRM_H264_CheckErrorWindow_IMPLEMENTED */

EXIT_PK_NAMESPACE;

#endif /* __DRMH264_H__ */

