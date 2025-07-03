/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMCRT_H__
#define __DRMCRT_H__

#include <drmstrsafe.h>

#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC
#ifndef DRM_BUILDING_CRTIMPLREAL_C
#include <oemcommonmem.h>
#endif  /* DRM_BUILDING_CRTIMPLREAL_C */
#endif  /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC */

ENTER_PK_NAMESPACE;

/* String CRT functions */

DRM_API DRM_BOOL  DRM_CALL DRMCRT_iswspace  (DRM_WCHAR wch);
DRM_API DRM_BOOL  DRM_CALL DRMCRT_iswxdigit (DRM_WCHAR wch);
DRM_API DRM_BOOL  DRM_CALL DRMCRT_iswdigit  (DRM_WCHAR wch);
DRM_API DRM_BOOL  DRM_CALL DRMCRT_iswalpha  (DRM_WCHAR wch);
DRM_API DRM_WCHAR DRM_CALL DRMCRT_towlower  (DRM_WCHAR wch);

DRM_API DRM_BOOL  DRM_CALL DRMCRT_isdigit   (DRM_CHAR ch);

#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC

#ifndef DRM_BUILDING_CRTIMPLREAL_C
#ifdef DRMCRT_memcpy
#undef DRMCRT_memcpy
#endif
#ifdef DRMCRT_memset
#undef DRMCRT_memset
#endif
#ifdef DRMCRT_memcmp
#undef DRMCRT_memcmp
#endif

#if defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR)
DRM_ALWAYS_INLINE int DRM_CALL DRMCRT_LocalMemcmp(
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pLHS,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pRHS,
    __in                            DRM_DWORD  f_cbCount );

DRM_ALWAYS_INLINE int DRM_CALL DRMCRT_LocalMemcmp(
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pLHS,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pRHS,
    __in                            DRM_DWORD  f_cbCount )
{
    /* Without volatile, some compilers can figure out this is an implementation of a memcmp */
    volatile const char * pRHS = (const char *) f_pRHS;
    const char * pLHS = (const char *) f_pLHS;
    while (f_cbCount--)
    {
        if(*pLHS < *pRHS)
        {
            return -1;
        }
        else if(*pLHS > *pRHS)
        {
            return 1;
        }
        pLHS++;
        pRHS++;
    }
    return 0;
}

#else   /* defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR) */
#error "Unexpected Error: In the profile lwrrently being compiled, either DRM_MSC_VER or DRM_GNUC_MAJOR *should* be defined."
#endif  /* defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR) */

#define DRMCRT_memcpy OEM_SELWRE_MEMCPY
#define DRMCRT_memset OEM_SELWRE_MEMSET
#define DRMCRT_memcmp DRMCRT_LocalMemcmp

#endif  /* DRM_BUILDING_CRTIMPLREAL_C */

#else /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_ANDROID || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_MAC */

DRM_API DRM_LONG DRM_CALL DRMCRT_memcmp(
    __in_bcount( count ) const DRM_VOID   *src1,
    __in_bcount( count ) const DRM_VOID   *src2,
    __in                       DRM_SIZE_T  count );

DRM_API_VOID DRM_VOID DRM_CALL DRMCRT_memset(
    __out_bcount( count ) DRM_VOID   *dst,
    __in                  DRM_DWORD   b,
    __in                  DRM_SIZE_T  count );

DRM_API_VOID DRM_VOID DRM_CALL DRMCRT_memcpy(
    __out_bcount( count )       DRM_VOID   *dst,
    __in_bcount( count )  const DRM_VOID   *src,
    __in                        DRM_SIZE_T  count );

#endif /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 || DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS*/

DRM_API_VOID DRM_VOID * DRM_CALL DRMCRT_memmove(
    __out_bcount( count )       DRM_VOID  *dst,
    __in_bcount( count )  const DRM_VOID  *src,
    __in                        DRM_SIZE_T count );

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsncmp(
    __in_ecount( count ) const DRM_WCHAR  *first,
    __in_ecount( count ) const DRM_WCHAR  *last,
    __in                       DRM_SIZE_T  count );

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsnicmp(
    __in_ecount( count ) const DRM_WCHAR    *first,
    __in_ecount( count ) const DRM_WCHAR    *last,
    __in                       DRM_SIZE_T    count );

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsicmp(
    __in_z const DRM_WCHAR *first,
    __in_z const DRM_WCHAR *last );

DRM_API _Post_equal_to_( _String_length_( wsz ) ) DRM_SIZE_T DRM_CALL DRMCRT_wcslen(
    __in_z const DRM_WCHAR *wsz );

DRM_API _Post_equal_to_( _String_length_( sz ) ) DRM_SIZE_T DRM_CALL DRMCRT_strlen(
    __in_z const DRM_CHAR *sz );

DRM_API DRM_LONG DRM_CALL DRMCRT_strncmp(
    _In_reads_or_z_( count ) const DRM_CHAR   *first,
    _In_reads_or_z_( count ) const DRM_CHAR   *last,
    __in                           DRM_SIZE_T  count );

DRM_API DRM_LONG DRM_CALL DRMCRT_abs( DRM_LONG number );

/**********************************************************************
**
** Set of DRM specific functions that complement C run time.
*
***********************************************************************/

DRM_API DRM_RESULT DRM_CALL DRMCRT_AtoDWORD(
    __in_ecount( f_cchStringInput ) const DRM_CHAR  *f_pszStringInput,
    __in                                  DRM_DWORD  f_cchStringInput,
    __in                                  DRM_DWORD  f_base,
    __out                                 DRM_DWORD *f_pdwValue );

DRM_API DRM_RESULT DRM_CALL DRMCRT_WtoDWORD(
    __in_ecount( f_cchStringInput ) const DRM_WCHAR *pwszStringInput,
    __in                                  DRM_DWORD  f_cchStringInput,
    __in                                  DRM_DWORD  f_base,
    __out                                 DRM_DWORD *f_pdwValue,
    __out_opt                             DRM_DWORD *f_pcchValue );

DRM_API DRM_RESULT DRM_CALL DRMCRT_strntol(
    __in_ecount( cchStringInput ) const DRM_CHAR *pszStringInput,
    __in                                DRM_DWORD  cchStringInput,
    __out                               DRM_LONG  *plValue );

DRM_API DRM_RESULT DRM_CALL DRMCRT_wcsntol(
    __in_ecount( cchStringInput ) const DRM_WCHAR *pwszStringInput,
    __in                                DRM_DWORD  cchStringInput,
    __out                               DRM_LONG  *plValue );

DRM_API _Post_maybenull_ _When_( _String_length_( pszStr ) <= cchMaxStr, _Null_terminated_ ) DRM_CHAR* DRM_CALL DRMCRT_strnstr(
    _In_reads_or_z_( cchMaxStr )       const DRM_CHAR  *pszStr,
   __in                                      DRM_DWORD  cchMaxStr,
    _In_reads_or_z_( cchMaxStrSearch ) const DRM_CHAR  *pszStrSearch,
   __in                                      DRM_DWORD  cchMaxStrSearch );

DRM_API _Post_maybenull_ _When_( _String_length_( pwszStr ) <= cchMaxStr, _Null_terminated_ ) DRM_WCHAR* DRM_CALL DRMCRT_wcsnstr(
    _In_reads_or_z_( cchMaxStr )       const DRM_WCHAR *pwszStr,
   __in                                      DRM_DWORD  cchMaxStr,
    _In_reads_or_z_( cchMaxStrSearch ) const DRM_WCHAR *pwszStrSearch,
   __in                                      DRM_DWORD  cchMaxStrSearch );

EXIT_PK_NAMESPACE;

#endif /* __DRMCRT_H__ */

