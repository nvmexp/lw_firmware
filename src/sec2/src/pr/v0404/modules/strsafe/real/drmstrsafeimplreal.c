/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMSTRSAFEIMPLREAL_C
#include <drmutilitieslite.h>
#include <drmstrsafe.h>
#include <drmconstants.h>
#include <drmbytemanip.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#ifdef NONE
static DRM_GLOBAL_CONST    DRM_WCHAR    s_wchNull = DRM_WCHAR_CAST('\0');

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_USE_WIDE_API, "Ignore prefast warning about ANSI functions for this entire file.")

static DRM_RESULT DRM_CALL _DrmStringLengthWorkerW(
    _In_reads_or_z_( f_cchMax ) const DRM_WCHAR *f_pwsz,
    __in                              DRM_DWORD  f_cchMax,
    __out_opt                         DRM_DWORD *f_pcchLength )
{
    DRM_RESULT dr           = DRM_SUCCESS;
    DRM_DWORD  cchMaxPrev   = f_cchMax;

    while ((f_cchMax != 0) && (*f_pwsz != s_wchNull))
    {
        f_pwsz++;
        f_cchMax--;
    }

    /* Check if the string is longer than cchMax */
    ChkArg( f_cchMax != 0 );

ErrorExit:

    if (f_pcchLength != NULL)
    {
        if (DRM_SUCCEEDED(dr))
        {
            *f_pcchLength = cchMaxPrev - f_cchMax;
        }
        else
        {
            *f_pcchLength = 0;
        }
    }

    return dr;
}
#endif

#if defined (SEC_COMPILE)
static DRM_RESULT DRM_CALL _DrmStringLengthWorkerA(
    _In_reads_or_z_( f_cchMax ) const DRM_CHAR  *f_psz,
    __in                              DRM_DWORD  f_cchMax,
    __out_opt                         DRM_DWORD *f_pcchLength)
{
    DRM_RESULT dr           = DRM_SUCCESS;
    DRM_DWORD  cchMaxPrev   = f_cchMax;
    DRM_DWORD  ib           = 0;

    while (f_cchMax && (((DRM_BYTE*)f_psz)[ ib ] != '\0'))
    {
        ib++;
        f_cchMax--;
    }

    /* Check if the string is longer than cchMax */
    ChkArg( f_cchMax != 0 );

ErrorExit:

    if (f_pcchLength != NULL)
    {
        if (DRM_SUCCEEDED(dr))
        {
            *f_pcchLength = cchMaxPrev - f_cchMax;
        }
        else
        {
            *f_pcchLength = 0;
        }
    }

    return dr;
}
#endif

#ifdef NONE
static DRM_NO_INLINE DRM_RESULT DRM_CALL _DrmStringCopyNWorkerW(
    __out_ecount_z( f_cchDest )          DRM_WCHAR *f_pwszDest,
    __in                                 DRM_DWORD  f_cchDest,
    _In_reads_or_z_( f_cchToCopy ) const DRM_WCHAR *f_pwszSrc,
    __in                                 DRM_DWORD  f_cchToCopy )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pwszSrc != NULL );
    ChkArg( f_pwszDest != NULL );
    ChkArg( f_cchDest != 0 );

    while (f_cchDest && f_cchToCopy && (*f_pwszSrc != s_wchNull))
    {
        *f_pwszDest++ = *f_pwszSrc++;
        f_cchDest--;
        f_cchToCopy--;
    }

    if (f_cchDest == 0)
    {
        /* We are going to truncate f_pwszDest */
        f_pwszDest--;
        dr = DRM_E_BUFFERTOOSMALL;
    }

    *f_pwszDest = s_wchNull;

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _DrmStringCopyWorkerW(
    __out_ecount_z( f_cchDest )        DRM_WCHAR *f_pwszDest,
    __in                               DRM_DWORD  f_cchDest,
    _In_reads_or_z_( f_cchDest ) const DRM_WCHAR *f_pwszSrc)
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_cchDest != 0 );

    while (f_cchDest && (*f_pwszSrc != s_wchNull))
    {
        *f_pwszDest++ = *f_pwszSrc++;
        f_cchDest--;
    }

    if (f_cchDest == 0)
    {
        /* we are going to truncate f_pwszDest */
        f_pwszDest--;
        dr = DRM_E_BUFFERTOOSMALL;
    }

    *f_pwszDest = s_wchNull;

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
static DRM_RESULT DRM_CALL _DrmStringCopyWorkerA(
    __out_ecount_z( f_cchDest )        DRM_CHAR *f_pszDest,
    __in                               DRM_DWORD f_cchDest,
    _In_reads_or_z_( f_cchDest ) const DRM_CHAR *f_pszSrc)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  ib = 0;

    ChkArg(f_cchDest != 0);

    while (f_cchDest && (((DRM_BYTE*)f_pszSrc)[ ib ] != '\0'))
    {
        ((DRM_BYTE*)f_pszDest)[ ib ] = ((DRM_BYTE*)f_pszSrc)[ ib ];
        ib++;
        f_cchDest--;
    }

    if (f_cchDest == 0)
    {
        /* we are going to truncate f_pszDest */
        ib--;
        dr = DRM_E_BUFFERTOOSMALL;
    }

    ((DRM_BYTE*)f_pszDest)[ ib ] = '\0';

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
static DRM_RESULT DRM_CALL _DrmStringCopyNWorkerA(
    __out_ecount_z( f_cchDest )       DRM_CHAR *f_pszDest,
    __in                              DRM_DWORD f_cchDest,
    _In_reads_or_z_( f_cchSrc ) const DRM_CHAR *f_pszSrc,
    __in                              DRM_DWORD f_cchSrc )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  ib = 0;

    ChkArg(f_cchDest != 0);

    while (f_cchDest && f_cchSrc && (((DRM_BYTE*)f_pszSrc)[ ib ] != '\0'))
    {
        ((DRM_BYTE*)f_pszDest)[ ib ] = ((DRM_BYTE*)f_pszSrc)[ ib ];
        ib++;
        f_cchDest--;
        f_cchSrc--;
    }

    if (f_cchDest == 0)
    {
        /* we are going to truncate f_pszDest */
        ib--;
        dr = DRM_E_BUFFERTOOSMALL;
    }

    ((DRM_BYTE*)f_pszDest)[ ib ] = '\0';

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _DrmStringCatNWorkerW(
    __inout_ecount_z(f_cchDest)       DRM_WCHAR *f_pwszDest,
    __in                              DRM_DWORD  f_cchDest,
    __in_ecount(f_cchToAppend) const  DRM_WCHAR *f_pwszSrc,
    __in                              DRM_DWORD  f_cchToAppend)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  cchDestLength = 0;

    ChkDR( _DrmStringLengthWorkerW(f_pwszDest, f_cchDest, &cchDestLength) );

    ChkDR( _DrmStringCopyNWorkerW(f_pwszDest + cchDestLength,
                        f_cchDest - cchDestLength,
                        f_pwszSrc,
                        f_cchToAppend) );

ErrorExit:
    return dr;
}

static DRM_RESULT DRM_CALL _DrmStringCatWorkerW(
    __inout_ecount_z(f_cchDest) DRM_WCHAR *f_pwszDest,
    __in                        DRM_DWORD  f_cchDest,
    __in_z                const DRM_WCHAR *f_pwszSrc)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  cchDestLength = 0;

    ChkDR( _DrmStringLengthWorkerW(f_pwszDest, f_cchDest, &cchDestLength) );

    ChkDR( _DrmStringCopyWorkerW(f_pwszDest + cchDestLength,
                          f_cchDest - cchDestLength,
                          f_pwszSrc) );
ErrorExit:
   return dr;
}
#endif

#if defined(SEC_COMPILE)
static DRM_RESULT DRM_CALL _DrmStringCatWorkerA(
    __inout_ecount_z(f_cchDest) DRM_CHAR *f_pszDest,
    __in                        DRM_DWORD f_cchDest,
    __in_z                const DRM_CHAR *f_pszSrc)
{
    DRM_RESULT dr;
    DRM_DWORD  cchDestLength;

    ChkDR( _DrmStringLengthWorkerA( f_pszDest, f_cchDest, &cchDestLength) );

    ChkDR( DRM_DWordSub( f_cchDest, cchDestLength, &f_cchDest ) );
    ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&f_pszDest, cchDestLength ) );

    ChkDR( _DrmStringCopyWorkerA( f_pszDest, f_cchDest, f_pszSrc ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
static DRM_RESULT DRM_CALL _DrmStringCatNWorkerA(
    __inout_ecount_z(f_cchDest)       DRM_CHAR *f_pszDest,
    __in                              DRM_DWORD f_cchDest,
    __in_ecount(f_cchToAppend) const  DRM_CHAR *f_pszSrc,
    __in                              DRM_DWORD f_cchToAppend)
{
    DRM_RESULT dr;
    DRM_DWORD  cchDestLength;

    ChkDR( _DrmStringLengthWorkerA(f_pszDest, f_cchDest, &cchDestLength) );

    ChkDR( DRM_DWordSub( f_cchDest, cchDestLength, &f_cchDest ) );
    ChkDR( DRM_DWordPtrAddSame( (DRM_SIZE_T*)&f_pszDest, cchDestLength ) );

    ChkDR( _DrmStringCopyNWorkerA( f_pszDest, f_cchDest, f_pszSrc, f_cchToAppend ) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchLengthW
**
** Synopsis:    This function is a replacement for strlen.
**
**              It is used to ensure that a string is not larger than a
**              given length, in characters. If that condition is met,
**              StringCchLength returns the current length of the string
**              in characters, not including the terminating null character.
**
** Arguments:   [f_pwsz]    Pointer to a buffer containing the string whose
**                          length is being checked.
**              [f_cchMax]  The maximum number of characters allowed in f_pwsz,
**                          including the terminating null character.
**                          This value cannot exceed DRM_STRSAFE_MAX_CCH.
**              [f_pcch]    Pointer to a variable of type DRM_DWORD
**                          containing the number of characters in f_pwsz,
**                          excluding the terminating null character.
**                          This value is valid only if f_pcch is not null
**                          and the function succeeds.
**
** Returns:     DRM_SUCCESS
**                  The string at f_pwsz was not null, and the length of the
**                  string (including the terminating null character) is less
**                  than or equal to f_cchMax characters.
**              DRM_E_ILWALIDARG
**                  The value in f_pwsz is NULL, f_cchMax is larger than
**                  DRM_STRSAFE_MAX_CCH, or f_pwsz is longer than cchMax.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchLengthW(
    _In_reads_or_z_( f_cchMax ) const DRM_WCHAR *f_pwsz,
    __in                              DRM_DWORD  f_cchMax,
    __out_opt                         DRM_DWORD *f_pcchLength )
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkArg( (f_pwsz != NULL) && (f_cchMax <= DRM_STRSAFE_MAX_CCH) );

    ChkDR( _DrmStringLengthWorkerW(f_pwsz, f_cchMax, f_pcchLength) );

ErrorExit:
    if (DRM_FAILED(dr) && f_pcchLength != NULL)
    {
        *f_pcchLength = 0;
    }
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchLengthA
**
** Synopsis:    This function is a replacement for strlen.
**
**              It is used to ensure that a string is not larger than a
**              given length, in characters. If that condition is met,
**              StringCchLength returns the current length of the string
**              in characters, not including the terminating null character.
**
** Arguments:   [f_psz]     Pointer to a buffer containing the string whose
**                          length is being checked.
**              [f_cchMax]  The maximum number of characters allowed in f_pwsz,
**                          including the terminating null character.
**                          This value cannot exceed DRM_STRSAFE_MAX_CCH.
**              [f_pcch]    Pointer to a variable of type DRM_DWORD
**                          containing the number of characters in f_pwsz,
**                          excluding the terminating null character.
**                          This value is valid only if f_pcch is not null
**                          and the function succeeds.
**
** Returns:     DRM_SUCCESS
**                  The string at f_pwsz was not null, and the length of the
**                  string (including the terminating null character) is less
**                  than or equal to f_cchMax characters.
**              DRM_E_ILWALIDARG
**                  The value in f_pwsz is NULL, f_cchMax is larger than
**                  DRM_STRSAFE_MAX_CCH, or f_pwsz is longer than f_cchMax.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchLengthA(
    __in_z              const DRM_CHAR  *f_psz,
    __in                      DRM_DWORD  f_cchMax,
    __out_opt                 DRM_DWORD *f_pcchLength)
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkArg( (f_psz != NULL) && (f_cchMax <= DRM_STRSAFE_MAX_CCH) );

    ChkDR( _DrmStringLengthWorkerA(f_psz, f_cchMax, f_pcchLength) );

ErrorExit:
    if (DRM_FAILED(dr) && f_pcchLength != NULL)
    {
        *f_pcchLength = 0;
    }

    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCopyNW
**
** Synopsis:    Copies a null-terminated wide character buffer from source
**              to destination. If the source buffer is longer than the
**              destination buffer, sets the last byte of the destination
**              buffer to 0 and returns DRM_E_BUFFERTOOSMALL
**
** Arguments:   [f_pwszDest] The destination buffer
**              [f_cchDest]  The character count of the destination buffer
**              [f_pwszSrc]  The source buffer
**              [f_cchSrc]   The character count of the source buffer
**
** Returns:     DRM_SUCCESS
**                  Success
**              DRM_E_ILWALIDARG
**                  One of the byte counts is too large, or either the source
**                  or destination buffer is NULL, or the destination byte count
**                  is 0.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCopyNW(
    __out_ecount_z( f_cchDest )       DRM_WCHAR  *f_pwszDest,
    __in                              DRM_DWORD   f_cchDest,
    _In_reads_or_z_( f_cchSrc ) const DRM_WCHAR  *f_pwszSrc,
    __in                              DRM_DWORD   f_cchSrc )
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkArg( (f_cchDest <= DRM_STRSAFE_MAX_CCH) && (f_cchSrc <= DRM_STRSAFE_MAX_CCH) );

    ChkArg( f_pwszDest != NULL
         && f_pwszSrc  != NULL );

    ChkDR( _DrmStringCopyNWorkerW(f_pwszDest, f_cchDest, f_pwszSrc, f_cchSrc) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCopyNA
**
** Synopsis:    Copies a null-terminated character buffer from source
**              to destination. If the source buffer is longer than the
**              destination buffer, sets the last byte of the destination
**              buffer to 0 and returns DRM_E_BUFFERTOOSMALL
**
** Arguments:   [f_pszDest]  The destination buffer
**              [f_cchDest]  The byte count of the destination buffer
**              [f_pszSrc]   The source buffer
**              [f_cchSrc]   The byte count of the source buffer
**
** Returns:     DRM_SUCCESS
**                  Success
**              DRM_E_ILWALIDARG
**                  One of the byte counts is too large, or either the source
**                  or destination buffer is NULL, or the destination byte count
**                  is 0.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCopyNA(
    __out_ecount_z( f_cchDest )          DRM_CHAR  *f_pszDest,
    __in                                 DRM_DWORD  f_cchDest,
    _In_reads_or_z_( f_cchToCopy ) const DRM_CHAR  *f_pszSrc,
    __in                                 DRM_DWORD  f_cchToCopy )
{
    DRM_RESULT dr;

    ChkArg( (f_cchDest <= DRM_STRSAFE_MAX_CCH) );
    ChkArg( (f_cchToCopy <= DRM_STRSAFE_MAX_CCH) );

    ChkArg( f_pszDest != NULL
         && f_pszSrc  != NULL );

    ChkDR( _DrmStringCopyNWorkerA(f_pszDest, f_cchDest, f_pszSrc, f_cchToCopy) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCopyW
**
** Synopsis:    Copies a null-terminated wide character buffer from source
**              to destination. If the source buffer is longer than the
**              destination buffer, sets the last byte of the destination
**              buffer to 0 and returns DRM_E_BUFFERTOOSMALL
**
** Arguments:   [f_pwszDest] The destination buffer
**              [f_cchDest]  The byte count of the destination buffer
**              [f_pwszSrc]  The NULL terminated source buffer
**
** Returns:     DRM_SUCCESS
**                  Success
**              DRM_E_ILWALIDARG
**                  One of the byte counts is too large, or either the source
**                  or destination buffer is NULL, or the destination byte count
**                  is 0.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCopyW(
    __out_ecount_z( f_cchDest )        DRM_WCHAR  *f_pwszDest,
    __in                               DRM_DWORD   f_cchDest,
    _In_reads_or_z_( f_cchDest ) const DRM_WCHAR  *f_pwszSrc )
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkArg( (f_cchDest <= DRM_STRSAFE_MAX_CCH) );

    ChkArg( f_pwszDest != NULL
         && f_pwszSrc  != NULL );

    ChkDR( _DrmStringCopyWorkerW(f_pwszDest, f_cchDest, f_pwszSrc) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCopyA
**
** Synopsis:    Copies a null-terminated character buffer from source
**              to destination. If the source buffer is longer than the
**              destination buffer, sets the last byte of the destination
**              buffer to 0 and returns DRM_E_BUFFERTOOSMALL
**
** Arguments:   [f_pszDest]  The destination buffer
**              [f_cchDest]  The byte count of the destination buffer
**              [f_pszSrc]   The NULL terminated source buffer
**
** Returns:     DRM_SUCCESS
**                  Success
**              DRM_E_ILWALIDARG
**                  One of the byte counts is too large, or either the source
**                  or destination buffer is NULL, or the destination byte count
**                  is 0.
**              DRM_E_BUFFERTOOSMALL
**                  f_cchSrc is larger than f_cchDest
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCopyA(
    __out_ecount_z( f_cchDest )        DRM_CHAR *f_pszDest,
    __in                               DRM_DWORD f_cchDest,
    _In_reads_or_z_( f_cchDest ) const DRM_CHAR *f_pszSrc )
{
    DRM_RESULT dr;

    ChkArg( (f_cchDest <= DRM_STRSAFE_MAX_CCH) );

    ChkArg( f_pszDest != NULL
         && f_pszSrc  != NULL );

    ChkDR( _DrmStringCopyWorkerA(f_pszDest, f_cchDest, f_pszSrc) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCatW
**
** Synopsis:    This function is a replacement for strcat. The size, in
**              characters, of the destination buffer is provided to the
**              function to ensure that StringCchCat does not write past
**              the end of this buffer.
**
** Arguments:   [f_pwszDest] Pointer to a buffer containing the string
**                           that f_pwszSrc is concatenated to, and which
**                           contains the entire resultant string. The
**                           string at f_pwszSrc is added to the end of the
**                           string at f_pwszDest.
**              [f_cchDest]  Size of the destination buffer, in characters.
**                           This value must equal the length of f_pwszSrc
**                           plus the length of f_pwszDest plus 1 to account
**                           for both strings and the terminating null character.
**                           The maximum number of characters allowed is
**                           DRM_STRSAFE_MAX_CCH.
**              [f_pwszSrc]  Pointer to a buffer containing the source string
**                           that is concatenated to the end of f_pwszDest. This
**                           source string must be null-terminated.
**
** Returns:     DRM_SUCCESS
**                  Source data was present, the strings were fully concatenated
**                  without truncation, and the resultant destination buffer is
**                  null-terminated.
**              DRM_E_ILWALIDARG
**                  The value in f_cchDest is 0 or larger than STRSAFE_MAX_CCH, or
**                  the destination buffer is already full.
**              DRM_E_BUFFERTOOSMALL
**                  The concatenation operation failed due to insufficient buffer
**                  space. The destination buffer contains a truncated,
**                  null-terminated version of the intended result. Where
**                  truncation is acceptable, this is not necessarily a failure
**                  condition.
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCatW(
    __inout_ecount_z( f_cchDest ) DRM_WCHAR *f_pwszDest,
    __in                          DRM_DWORD  f_cchDest,
    __in_z                  const DRM_WCHAR *f_pwszSrc )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pwszDest != NULL && f_cchDest <= DRM_STRSAFE_MAX_CCH && f_pwszSrc != NULL );

    ChkDR( _DrmStringCatWorkerW(f_pwszDest, f_cchDest, f_pwszSrc) );

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/**********************************************************************
** Function:    DRM_STR_StringCchCatA
**
** Synopsis:    This function is a replacement for strcat. The size, in
**              characters, of the destination buffer is provided to the
**              function to ensure that StringCchCat does not write past
**              the end of this buffer.
**
** Arguments:   [f_pszDest] Pointer to a buffer containing the string
**                          that f_pszSrc is concatenated to, and which
**                          contains the entire resultant string. The
**                          string at f_pszSrc is added to the end of the
**                          string at f_pszDest.
**              [f_cchDest] Size of the destination buffer, in characters.
**                          This value must equal the length of f_pszSrc
**                          plus the length of f_pszDest plus 1 to account
**                          for both strings and the terminating null character.
**                          The maximum number of characters allowed is
**                          DRM_STRSAFE_MAX_CCH.
**              [f_pszSrc]  Pointer to a buffer containing the source string
**                          that is concatenated to the end of f_pszDest. This
**                          source string must be null-terminated.
**
** Returns:     DRM_SUCCESS
**                  Source data was present, the strings were fully concatenated
**                  without truncation, and the resultant destination buffer is
**                  null-terminated.
**              DRM_E_ILWALIDARG
**                  The value in f_cchDest is 0 or larger than STRSAFE_MAX_CCH, or
**                  the destination buffer is already full.
**              DRM_E_BUFFERTOOSMALL
**                  The concatenation operation failed due to insufficient buffer
**                  space. The destination buffer contains a truncated,
**                  null-terminated version of the intended result. Where
**                  truncation is acceptable, this is not necessarily a failure
**                  condition.
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCatA(
    __inout_ecount_z( f_cchDest ) DRM_CHAR  *f_pszDest,
    __in                          DRM_DWORD  f_cchDest,
    __in_z                  const DRM_CHAR  *f_pszSrc )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pszDest != NULL && f_cchDest <= DRM_STRSAFE_MAX_CCH && f_pszSrc != NULL );

    ChkDR( _DrmStringCatWorkerA(f_pszDest, f_cchDest, f_pszSrc) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/**********************************************************************
** Function:    DRM_STR_StringCchCatNW
**
** Synopsis:    This function is a replacement for strcat. The size, in
**              characters, of the destination buffer is provided to the
**              function to ensure that StringCchCat does not write past
**              the end of this buffer.
**
** Arguments:   [f_pwszDest] Pointer to a buffer containing the string
**                           that f_pwszSrc is concatenated to, and which
**                           contains the entire resultant string. The
**                           string at f_pwszSrc is added to the end of the
**                           string at f_pwszDest.
**              [f_cchDest]  Size of the destination buffer, in characters.
**                           This value must equal the length of f_pwszSrc
**                           plus the length of f_pwszDest plus 1 to account
**                           for both strings and the terminating null character.
**                           The maximum number of characters allowed is
**                           DRM_STRSAFE_MAX_CCH.
**              [f_pwszSrc]  Pointer to a buffer containing the source string
**                           that is concatenated to the end of f_pwszDest. This
**                           source string must be null-terminated.
**              [f_cchToAppend] The maximum number of characters to append to f_pwszDest.
**
** Returns:     DRM_SUCCESS
**                  Source data was present, the strings were fully concatenated
**                  without truncation, and the resultant destination buffer is
**                  null-terminated.
**              DRM_E_ILWALIDARG
**                  The value in f_cchDest is 0 or larger than STRSAFE_MAX_CCH, or
**                  the destination buffer is already full.
**              DRM_E_BUFFERTOOSMALL
**                  The concatenation operation failed due to insufficient buffer
**                  space. The destination buffer contains a truncated,
**                  null-terminated version of the intended result. Where
**                  truncation is acceptable, this is not necessarily a failure
**                  condition.
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCatNW(
    __inout_ecount_z( f_cchDest )        DRM_WCHAR *f_pwszDest,
    __in                                 DRM_DWORD  f_cchDest,
    __in_ecount( f_cchToAppend )   const DRM_WCHAR *f_pwszSrc,
    __in                                 DRM_DWORD  f_cchToAppend )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pwszDest != NULL && f_cchDest <= DRM_STRSAFE_MAX_CCH && f_pwszSrc != NULL );

    ChkDR( _DrmStringCatNWorkerW(f_pwszDest, f_cchDest, f_pwszSrc, f_cchToAppend) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchCatNA
**
** Synopsis:    This function is a replacement for strcat. The size, in
**              characters, of the destination buffer is provided to the
**              function to ensure that StringCchCat does not write past
**              the end of this buffer.
**
** Arguments:   [f_pszDest] Pointer to a buffer containing the string
**                          that f_pszSrc is concatenated to, and which
**                          contains the entire resultant string. The
**                          string at f_pszSrc is added to the end of the
**                          string at f_pszDest.
**              [f_cchDest] Size of the destination buffer, in characters.
**                          This value must equal the length of f_pszSrc
**                          plus the length of f_pszDest plus 1 to account
**                          for both strings and the terminating null character.
**                          The maximum number of characters allowed is
**                          DRM_STRSAFE_MAX_CCH.
**              [f_pszSrc]  Pointer to a buffer containing the source string
**                          that is concatenated to the end of f_pszDest. This
**                          source string must be null-terminated.
**              [f_cchToAppend] The maximum number of characters to append
**                              to f_pwszDest.
**
** Returns:     DRM_SUCCESS
**                  Source data was present, the strings were fully concatenated
**                  without truncation, and the resultant destination buffer is
**                  null-terminated.
**              DRM_E_ILWALIDARG
**                  The value in f_cchDest is 0 or larger than STRSAFE_MAX_CCH, or
**                  the destination buffer is already full.
**              DRM_E_BUFFERTOOSMALL
**                  The concatenation operation failed due to insufficient buffer
**                  space. The destination buffer contains a truncated,
**                  null-terminated version of the intended result. Where
**                  truncation is acceptable, this is not necessarily a failure
**                  condition.
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchCatNA(
    __inout_ecount_z( f_cchDest )      DRM_CHAR  *f_pszDest,
    __in                               DRM_DWORD  f_cchDest,
    __in_ecount( f_cchToAppend ) const DRM_CHAR  *f_pszSrc,
    __in                               DRM_DWORD  f_cchToAppend )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pszDest != NULL && f_cchDest <= DRM_STRSAFE_MAX_CCH && f_pszSrc != NULL );

    ChkDR( _DrmStringCatNWorkerA(f_pszDest, f_cchDest, f_pszSrc, f_cchToAppend) );

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_StringCchToLower
**
** Synopsis:    Colwerts DRM string to lower case DRM string
**
** Arguments:   [f_pwszToLower] -- Pointer to a buffer that contains
**                                 wide character string to colwert to lowercase
**              [f_cchToLower]  -- Size of string to colwert
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_StringCchToLower(
    __inout_ecount_nz(f_cchToLower) DRM_WCHAR *f_prgwchToLower,
    __in                            DRM_DWORD  f_cchToLower )
{
    DRM_RESULT dr          = DRM_SUCCESS;
    DRM_DWORD  dwCount     = 0;
    DRM_WCHAR *pwchLwrrent = f_prgwchToLower;

    ChkArg( f_prgwchToLower!=NULL );

    for ( dwCount = 0; dwCount<f_cchToLower; dwCount++ )
    {
        *pwchLwrrent = DRMCRT_towlower( *pwchLwrrent );
        pwchLwrrent++;
    }

ErrorExit:
    return dr;
}

/**********************************************************************
** Function:    DRM_STR_DstrTowLower
**
** Synopsis:    Colwerts DRM string to lower case DRM string
**
** Arguments:   [f_pdstrToLower] -- DRM string to colwert
**
***********************************************************************/
DRM_API DRM_RESULT DRM_CALL DRM_STR_DstrToLower(
    __inout DRM_STRING *f_pdstrToLower )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkDRMString(f_pdstrToLower);

    ChkDR( DRM_STR_StringCchToLower( f_pdstrToLower->pwszString,
                                     f_pdstrToLower->cchString ) );

ErrorExit:
    return dr;
}
#endif

PREFAST_POP /* ignore prefast warning about ANSI functions for this entire file  */

EXIT_PK_NAMESPACE_CODE;

