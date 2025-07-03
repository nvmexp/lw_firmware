/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_CRTIMPLREAL_C
#include <drmcrt.h>
#include <byteorder.h>
#include <drmconstants.h>
#include <drmdebug.h>
#include <oemcommon.h>
#include <drmlastinclude.h>

#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1
#ifdef DRMCRT_memcpy
#undef DRMCRT_memcpy
#endif
#ifdef DRMCRT_memset
#undef DRMCRT_memset
#endif
#ifdef DRMCRT_memcmp
#undef DRMCRT_memcmp
#endif

#endif /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 */



ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
static const    DRM_WCHAR    s_wchNull              = DRM_WCHAR_CAST('\0');
static const    DRM_WCHAR    s_wch0                 = DRM_WCHAR_CAST('0');
static const    DRM_WCHAR    s_wch9                 = DRM_WCHAR_CAST('9');
static const    DRM_WCHAR    s_wcha                 = DRM_WCHAR_CAST('a');
static const    DRM_WCHAR    s_wchf                 = DRM_WCHAR_CAST('f');
static const    DRM_WCHAR    s_wchz                 = DRM_WCHAR_CAST('z');
static const    DRM_WCHAR    s_wchA                 = DRM_WCHAR_CAST('A');
static const    DRM_WCHAR    s_wchF                 = DRM_WCHAR_CAST('F');
static const    DRM_WCHAR    s_wchZ                 = DRM_WCHAR_CAST('Z');
static const    DRM_WCHAR    s_wchSpace             = DRM_WCHAR_CAST(' ');
static const    DRM_WCHAR    s_wchTab               = DRM_WCHAR_CAST('\x9');
static const    DRM_WCHAR    s_wchLineFeed          = DRM_WCHAR_CAST('\xA');
static const    DRM_WCHAR    s_wchVerticalTab       = DRM_WCHAR_CAST('\xB');
static const    DRM_WCHAR    s_wchFormFeed          = DRM_WCHAR_CAST('\xC');
static const    DRM_WCHAR    s_wchCarriageReturn    = DRM_WCHAR_CAST('\xD');
static const    DRM_CHAR     s_ch0                  = '0';
static const    DRM_CHAR     s_ch9                  = '9';

/* String CRT functions */

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsncmp(
    __in_ecount( count ) const DRM_WCHAR  *first,
    __in_ecount( count ) const DRM_WCHAR  *last,
    __in                       DRM_SIZE_T  count )
{
    if (!count)
    {
        return(0);
    }

    while (--count && *first && *first == *last)
    {
        first++;
        last++;
    }

    return((DRM_LONG)(*first - *last));
}

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsicmp(
    __in_z const DRM_WCHAR *first,
    __in_z const DRM_WCHAR *last )
{
    while( ( *first != s_wchNull )
        && ( DRMCRT_towlower(*first) == DRMCRT_towlower(*last) ) )
    {
        first++;
        last++;
    }

    return( (DRM_LONG)( DRMCRT_towlower(*first) - DRMCRT_towlower(*last) ) );
}

DRM_API DRM_LONG DRM_CALL DRMCRT_wcsnicmp(
    __in_ecount( count ) const DRM_WCHAR  *first,
    __in_ecount( count ) const DRM_WCHAR  *last,
    __in                       DRM_SIZE_T  count )
{
    if( count == 0 )
    {
        return(0);
    }

    while( ( --count > 0 )
        && ( *first != s_wchNull )
        && ( DRMCRT_towlower(*first) == DRMCRT_towlower(*last) ) )
    {
        first++;
        last++;
    }

    return( (DRM_LONG)( DRMCRT_towlower(*first) - DRMCRT_towlower(*last) ) );
}

DRM_API DRM_SIZE_T DRM_CALL DRMCRT_wcslen(
    __in_z const DRM_WCHAR *wsz )
{
    const DRM_WCHAR *eos = wsz;
    while( *eos )
    {
        eos++;
    }
    return( (DRM_SIZE_T)(eos - wsz) );
}

DRM_API DRM_LONG DRM_CALL DRMCRT_strncmp(
    __in_ecount( count ) const DRM_CHAR   *first,
    __in_ecount( count ) const DRM_CHAR   *last,
    __in                       DRM_SIZE_T  count )
{
    if (!count)
    {
        return(0);
    }

    while (--count && *first && *first == *last)
    {
        first++;
        last++;
    }

    return((DRM_LONG)(*first - *last));
}


DRM_API DRM_BOOL DRM_CALL DRMCRT_iswxdigit (DRM_WCHAR f_wch)
{
    DRM_WCHAR wch = DRM_NATIVE_WCHAR( f_wch );
    return ((wch >= DRM_NATIVE_WCHAR(s_wch0) && wch <= DRM_NATIVE_WCHAR(s_wch9))   /* A digit */
        ||  (wch >= DRM_NATIVE_WCHAR(s_wchA) && wch <= DRM_NATIVE_WCHAR(s_wchF))   /* Upper case hex char */
        ||  (wch >= DRM_NATIVE_WCHAR(s_wcha) && wch <= DRM_NATIVE_WCHAR(s_wchf))); /* lower case hex char */
}

DRM_API DRM_BOOL DRM_CALL DRMCRT_iswdigit( DRM_WCHAR wch )
{
    return ( DRM_NATIVE_WCHAR(wch) >= DRM_NATIVE_WCHAR(s_wch0)
          && DRM_NATIVE_WCHAR(wch) <= DRM_NATIVE_WCHAR(s_wch9) ); /* A digit */
}

DRM_API DRM_BOOL DRM_CALL DRMCRT_isdigit( DRM_CHAR ch )
{
    return ( ch >= s_ch0 && ch <= s_ch9 ); /* A digit */
}

DRM_API DRM_BOOL DRM_CALL DRMCRT_iswalpha (DRM_WCHAR wch)
{
    return ((DRM_NATIVE_WCHAR(wch) >= DRM_NATIVE_WCHAR(s_wchA) && DRM_NATIVE_WCHAR(wch) <= DRM_NATIVE_WCHAR(s_wchZ))   /* Upper case char */
         || (DRM_NATIVE_WCHAR(wch) >= DRM_NATIVE_WCHAR(s_wcha) && DRM_NATIVE_WCHAR(wch) <= DRM_NATIVE_WCHAR(s_wchz))); /* lower case char */
}

DRM_API DRM_WCHAR DRM_CALL DRMCRT_towlower (DRM_WCHAR wch)
{
    if (DRMCRT_iswalpha (wch)
    &&   DRM_NATIVE_WCHAR(wch) < DRM_NATIVE_WCHAR(s_wcha))
    {
        DRM_WCHAR wchTemp = (DRM_WCHAR)( DRM_NATIVE_WCHAR(s_wcha) - DRM_NATIVE_WCHAR(s_wchA) );
        wch = (DRM_WCHAR)( DRM_NATIVE_WCHAR(wch) + wchTemp );
        wch = (DRM_WCHAR)DRM_WCHAR_CAST( wch );
    }

    return wch;
}

DRM_API DRM_BOOL DRM_CALL DRMCRT_iswspace (DRM_WCHAR wch)
{
    return (DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchTab)
         || DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchLineFeed)
         || DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchVerticalTab)
         || DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchFormFeed)
         || DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchCarriageReturn)
         || DRM_NATIVE_WCHAR(wch) == DRM_NATIVE_WCHAR(s_wchSpace));
}

DRM_API DRM_SIZE_T DRM_CALL DRMCRT_strlen(
    __in_z const DRM_CHAR *sz )
{
    const DRM_CHAR *eos = sz;
    while( *eos )
    {
        eos++;
    }
    return( (DRM_SIZE_T)(eos - sz) );
}

/*******************************************************************************
*memmove - Copy source buffer to destination buffer
*
*Purpose:
*       memmove() copies a source memory buffer to a destination memory buffer.
*       This routine recognize overlapping buffers to avoid propogation.
*       For cases where propogation is not a problem, memcpy() can be used.
*
*Entry:
*       void *dst = pointer to destination buffer
*       const void *src = pointer to source buffer
*       size_t count = number of bytes to copy
*
*Exit:
*       Returns a pointer to the destination buffer
*
*Exceptions:
*******************************************************************************/

DRM_API_VOID DRM_VOID * DRM_CALL DRMCRT_memmove(
    __out_bcount( count )       DRM_VOID  *dst,
    __in_bcount( count )  const DRM_VOID  *src,
    __in                        DRM_SIZE_T count )
{
    DRM_VOID * ret = dst;

    if (dst <= src || (DRM_CHAR *)dst >= ((DRM_CHAR *)src + count))
    {
            /*
             * Buffers don't overlap in a way that would cause problems
             * copy from lower addresses to higher addresses
             */
            while (count--)
            {
                    *(DRM_CHAR *)dst = *(const DRM_CHAR *)src;
                    dst = (DRM_CHAR *)dst + 1;
                    src = (DRM_CHAR *)src + 1;
            }
    }
    else
    {
            /*
             * Overlapping Buffers
             * copy from higher addresses to lower addresses
             */
            dst = (DRM_CHAR *)dst + count - 1;
            src = (DRM_CHAR *)src + count - 1;

            while (count--)
            {
                    *(DRM_CHAR *)dst = *(const DRM_CHAR *)src;
                    dst = (DRM_CHAR *)dst - 1;
                    src = (DRM_CHAR *)src - 1;
            }
    }

    return ret;
}
#endif
#if defined(SEC_COMPILE)
DRM_API_VOID DRM_VOID DRM_CALL DRMCRT_memcpy(
    __out_bcount( count )       DRM_VOID   *dst,
    __in_bcount( count )  const DRM_VOID   *src,
    __in                        DRM_SIZE_T  count )
{
    while( count > 0 )
    {
        *(DRM_BYTE*)dst = *(const DRM_BYTE*)src;
        dst = (DRM_CHAR *)dst + 1;
        src = (DRM_CHAR *)src + 1;
        count--;
    }
}
#endif

#if defined(SEC_COMPILE)
DRM_API_VOID DRM_VOID DRM_CALL DRMCRT_memset(
    __out_bcount( count ) DRM_VOID   *dst,
    __in                  DRM_DWORD   b,
    __in                  DRM_SIZE_T  count )
{
    /*
    ** b is a DRM_DWORD to be consistent with standard memset
    ** but we only use the low order byte
    */
    DRM_BYTE by = b & 0xFF;

    DRMASSERT( b <= 0xFF );

    while( count > 0 )
    {
        *(DRM_BYTE*)dst = by;
        dst = (DRM_CHAR *)dst + 1;
        count--;
    }
}
#endif
#if defined(SEC_COMPILE)
DRM_API DRM_LONG DRM_CALL DRMCRT_memcmp(
    __in_bcount( count ) const DRM_VOID   *src1,
    __in_bcount( count ) const DRM_VOID   *src2,
    __in                       DRM_SIZE_T  count )
{
    while( count > 0 )
    {
        if( *(const DRM_BYTE*)src1 > *(const DRM_BYTE*)src2 )
        {
            return 1;
        }
        else if( *(const DRM_BYTE*)src1 < *(const DRM_BYTE*)src2 )
        {
            return -1;
        }
        src1 = (DRM_CHAR *)src1 + 1;
        src2 = (DRM_CHAR *)src2 + 1;
        count--;
    }
    return 0;
}
#endif
#ifdef NONE
DRM_API DRM_LONG DRM_CALL DRMCRT_abs( DRM_LONG number )
{
    return (number>=0 ? number : -number);
}
#endif
EXIT_PK_NAMESPACE_CODE;

