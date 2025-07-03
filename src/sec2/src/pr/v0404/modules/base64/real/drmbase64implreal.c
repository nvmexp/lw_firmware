/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMBASE64IMPLREAL_C
#include <drmbase64.h>
#include <drmerr.h>
#include <drmconstants.h>
#include <drmbytemanip.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#define CCH_B64_IN_QUARTET  4
#define CB_B64_OUT_TRIO     3

#ifdef NONE
static DRM_GLOBAL_CONST DRM_WCHAR  s_wchEqual = DRM_WCHAR_CAST( '=' );
#endif

#if defined (SEC_COMPILE)
static DRM_GLOBAL_CONST DRM_BYTE   g_rgbBase64Decode [] PR_ATTR_DATA_OVLY(_g_rgbBase64Decode) =
{
    /* Note we also accept ! and + interchangably. */
    /* Note we also accept * and / interchangably. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*   0 -   7 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*   8 -  15 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  16 -  23 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  24 -  31 */
    0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  32 -  39 */
    0x00, 0x00, 0x3f, 0x3e, 0x00, 0x00, 0x00, 0x3f, /*  40 -  47 */
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, /*  48 -  55 */
    0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  56 -  63 */
    0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, /*  64 -  71 */
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, /*  72 -  79 */
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, /*  80 -  87 */
    0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, /*  88 -  95 */
    0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, /*  96 - 103 */
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, /* 104 - 111 */
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, /* 112 - 119 */
    0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00  /* 120 - 127 */
};
#endif

#ifdef NONE
/*
** Character 65 ('A') is the legitimate zero. All other indices into the above
** array that have the value of zero are invalid Base64 characters.
*/
static DRM_GLOBAL_CONST DRM_DWORD g_iBase64ZeroChar = 65;

static DRM_GLOBAL_CONST DRM_DWORD g_cbBase64Decode = sizeof( g_rgbBase64Decode );

/* when decode in-place, the decoded string will be the start of the input buffer */

static DRM_GLOBAL_CONST DRM_DWORD g_fDecodeAllowed = DRM_BASE64_DECODE_IN_PLACE; /* OR in any new supported flags */
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_B64_DecodeW(
    __in                            const DRM_CONST_STRING *f_pdstrSource,
    __inout                               DRM_DWORD        *f_pcbDestination,
    __out_bcount_opt( *f_pcbDestination ) DRM_BYTE         *f_pbDestination,
    __in                                  DRM_DWORD         f_fFlags)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  cbDecode  = 0;
    DRM_DWORD  ichSource = 0;
    DRM_DWORD  ibInsert  = 0;
    DRM_DWORD  ibDest    = 0;

    ChkDRMString(f_pdstrSource);
    ChkArg     ((f_pdstrSource->cchString  % CCH_B64_IN_QUARTET) == 0);     /* Effectively ensures f_pdstrSource->cchString >= CCH_B64_IN_QUARTET because f_pdstrSource->cchString > 0 */
    ChkArg      (f_pcbDestination != NULL);
    ChkArg     ((f_fFlags & ~g_fDecodeAllowed) == 0);

    /* Compute maximum buffer size needed. */
    ChkDR( DRM_DWordAdd( f_pdstrSource->cchString, CCH_B64_IN_QUARTET - 1, &cbDecode ) );
    cbDecode /= CCH_B64_IN_QUARTET;
    ChkDR( DRM_DWordMult( cbDecode, CB_B64_OUT_TRIO, &cbDecode ) );

    if( f_pdstrSource->pwszString[ f_pdstrSource->cchString - 1 ] == s_wchEqual )   /* can't underflow: already confirmed that f_pdstrSource->cchString > 0 using ChkDRMString */
    {
        cbDecode--;         /* can't underflow: math above ensures that at this point cbDecode >= CB_B64_OUT_TRIO == 3 */
        if( f_pdstrSource->cchString >= 2 && f_pdstrSource->pwszString[ f_pdstrSource->cchString - 2 ] == s_wchEqual )
        {
            cbDecode--;     /* can't underflow: math above ensures that at this point cbDecode >= CB_B64_OUT_TRIO - 1 == 2 */
        }
    }

    if (((cbDecode > *f_pcbDestination)
       || f_pbDestination == NULL)
    && (f_fFlags & DRM_BASE64_DECODE_IN_PLACE) == 0)
    {
        *f_pcbDestination = cbDecode;
        ChkDR(DRM_E_BUFFERTOOSMALL);
    }

    if (f_fFlags & DRM_BASE64_DECODE_IN_PLACE )
    {
        f_pbDestination = (DRM_BYTE *) f_pdstrSource->pwszString;
        ibInsert        = (f_pdstrSource->cchString * sizeof(DRM_WCHAR)) - cbDecode;    /* Can't underflow: f_pdstrSource->cchString >= 4 && cbDecode == f_pdstrSource->cchString * 3/4 */
    }

    *f_pcbDestination = cbDecode;

    for (ichSource  = f_pdstrSource->cchString;
         ichSource  > 0;
         ichSource -= CCH_B64_IN_QUARTET)           /* Can't underflow: f_pdstrSource->cchString % CCH_B64_IN_QUARTET == 0 */
    {
        DRM_BYTE  rgbOutput [CB_B64_OUT_TRIO + 1] = { 0 };
        DRM_DWORD iBase64Decode                   = 0;

        for (ibDest = 0;
             ibDest < CB_B64_OUT_TRIO + 1;
             ibDest++ )
        {
            if( f_pdstrSource->pwszString[ ( ichSource - CCH_B64_IN_QUARTET ) + ibDest ] == s_wchEqual )    /* Can't over/underflow: ichSource >= CCH_B64_IN_QUARTET and ibDest <= CCH_B64_IN_QUARTET */
            {
                if( ibDest     < 2
                 || ichSource != f_pdstrSource->cchString )
                {
                    ChkDR( DRM_E_ILWALID_BASE64 );
                }
                break;
            }

            iBase64Decode = (DRM_DWORD)DRM_NATIVE_WCHAR( f_pdstrSource->pwszString[ ( ( ichSource - CCH_B64_IN_QUARTET ) + ibDest ) ] );    /* Can't over/underflow: ichSource >= CCH_B64_IN_QUARTET and ibDest <= CCH_B64_IN_QUARTET */

            if ( iBase64Decode >= g_cbBase64Decode )
            {
                /*
                ** The index is larger than the size of g_rgbBase64Decode and
                ** should not be used as an index into g_rgbBase64Decode.  The
                ** string may have been base64Encoded incorrectly.
                */
                ChkDR (DRM_E_ILWALID_BASE64);
            }

            if ( 0 == g_rgbBase64Decode[ iBase64Decode ] && iBase64Decode != g_iBase64ZeroChar )
            {
                /*
                ** The value of this index is zero and it isn't the legitimate
                ** zero char, so this character is not actually a valid Base64
                ** character
                */
                ChkDR( DRM_E_ILWALID_BASE64 );
            }

            rgbOutput[ ibDest ] = g_rgbBase64Decode[ iBase64Decode ];
        }

        switch (ibDest)
        {
        default:
            f_pbDestination[ ibInsert  + --cbDecode ] = (DRM_BYTE)( ((rgbOutput[ 2 ] & 0x03) << 6) |   rgbOutput[ 3 ] );                /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
            __fallthrough;
        case 3:
            f_pbDestination[ ibInsert  + --cbDecode ] = (DRM_BYTE)( ((rgbOutput[ 1 ] & 0x0F) << 4) | ((rgbOutput[ 2 ] & 0x3C) >> 2) );  /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
            __fallthrough;
        case 2:
            f_pbDestination[ ibInsert  + --cbDecode ] = (DRM_BYTE)( (rgbOutput[ 0 ]          << 2) | ((rgbOutput[ 1 ] & 0x30) >> 4) );  /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
        }
    }

    /* move the buffer to start of input buffer */
    if (f_fFlags & DRM_BASE64_DECODE_IN_PLACE)
    {
        (DRM_VOID)DRM_BYT_MoveBytes((DRM_BYTE *) f_pdstrSource->pwszString,
                           0,
                           f_pbDestination,
                           ibInsert,
                          *f_pcbDestination);
    }

    dr = DRM_SUCCESS;

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_B64_DecodeA(
    __in_ecount( f_pdasstrSource->m_ich + f_pdasstrSource->m_cch ) const DRM_CHAR        *f_pszBase,
    __in                                                           const DRM_SUBSTRING   *f_pdasstrSource,
    __inout                                                              DRM_DWORD       *f_pcbDestination,
    __out_bcount_opt( ( f_fFlags & DRM_BASE64_DECODE_IN_PLACE ) == 0 ? *f_pcbDestination : f_pdasstrSource->m_ich + *f_pcbDestination ) DRM_BYTE *f_pbDestination,
    __in                                                                 DRM_DWORD        f_fFlags)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  cbDecode  = 0;
    DRM_DWORD  ichSource = 0;
    DRM_DWORD  ibInsert  = 0;
    DRM_DWORD  ibDest    = 0;

    ChkArg(f_pdasstrSource  != NULL);
    ChkArg(f_pcbDestination != NULL);
    ChkArg((f_pdasstrSource->m_cch % CCH_B64_IN_QUARTET) == 0);
    ChkArg((f_fFlags & ~g_fDecodeAllowed) == 0);
    ChkArg( f_pdasstrSource->m_cch > 0 );                       /* Effectively ensures f_pdasstrSource->m_cch >= CCH_B64_IN_QUARTET because f_pdasstrSource->m_cch % CCH_B64_IN_QUARTET == 0 */

    /* Compute maximum buffer size needed. */
    ChkDR( DRM_DWordAdd( f_pdasstrSource->m_cch, CCH_B64_IN_QUARTET - 1, &cbDecode ) );
    cbDecode /= CCH_B64_IN_QUARTET;
    ChkDR( DRM_DWordMult( cbDecode, CB_B64_OUT_TRIO, &cbDecode ) );

    if( ( (DRM_BYTE*)f_pszBase )[ f_pdasstrSource->m_ich + f_pdasstrSource->m_cch - 1 ] == '=' )   /* can't underflow: already confirmed that f_pdstrSource->cchString > 0 using ChkDRMString */
    {
        cbDecode--;         /* can't underflow: math above ensures that at this point cbDecode >= CB_B64_OUT_TRIO == 3 */
        if( ( (DRM_BYTE*)f_pszBase )[ f_pdasstrSource->m_ich + f_pdasstrSource->m_cch - 2 ] == '=' )
        {
            cbDecode--;     /* can't underflow: math above ensures that at this point cbDecode >= CB_B64_OUT_TRIO - 1 == 2 */
        }
    }

    if (((cbDecode > *f_pcbDestination)
       || f_pbDestination == NULL)
    && (f_fFlags & DRM_BASE64_DECODE_IN_PLACE) == 0)
    {
        *f_pcbDestination = cbDecode;
        ChkDR (DRM_E_BUFFERTOOSMALL);
    }

    if (f_fFlags & DRM_BASE64_DECODE_IN_PLACE )
    {
        ibInsert = f_pdasstrSource->m_ich + f_pdasstrSource->m_cch - cbDecode;   /* Can't underflow: cbDecode == f_pdasstrSource->m_cch * 3/4 */

        f_pbDestination = (DRM_BYTE *) f_pszBase;
    }

    *f_pcbDestination = cbDecode;

    for (ichSource  = f_pdasstrSource->m_cch;
         ichSource  > 0;
         ichSource -= CCH_B64_IN_QUARTET)   /* Can't underflow: f_pdasstrSource->m_cch % CCH_B64_IN_QUARTET == 0 */
    {
        DRM_BYTE  rgbOutput [CB_B64_OUT_TRIO + 1] = { 0 };
        DRM_DWORD iBase64Decode                   = 0;

        for (ibDest = 0;
             ibDest < CB_B64_OUT_TRIO + 1;
             ibDest++ )
        {
            DRM_DWORD ichGet = f_pdasstrSource->m_ich + ichSource + ibDest - CCH_B64_IN_QUARTET;    /* Can't over/underflow: 4 <= ichSource <= f_pdasstrSource->m_cch, ibDest < 4, CCH_B64_IN_QUARTET == 4 */
            if (((DRM_BYTE*)f_pszBase)[ ichGet ] == '=')
            {
                if (ibDest     < 2
                ||  ichSource != f_pdasstrSource->m_cch)
                {
                    ChkDR( DRM_E_ILWALID_BASE64 );
                }
                break;
            }

            iBase64Decode = (DRM_DWORD) ((DRM_BYTE*)f_pszBase)[ ichGet ];
            if ( iBase64Decode >= g_cbBase64Decode )
            {
                /*
                ** The index is larger than the size of g_rgbBase64Decode and
                ** should not be used as an index into g_rgbBase64Decode.  The
                ** string may have been base64Encoded incorrectly.
                */
                ChkDR( DRM_E_ILWALID_BASE64 );
            }

            if ( 0 == g_rgbBase64Decode[ iBase64Decode ] && iBase64Decode != g_iBase64ZeroChar )
            {
                /*
                ** The value of this index is zero and it isn't the legitimate
                ** zero char, so this character is not actually a valid Base64
                ** character
                */
                ChkDR( DRM_E_ILWALID_BASE64 );
            }

            rgbOutput[ ibDest ] = g_rgbBase64Decode[ iBase64Decode ];
        }

        ChkArg(f_pbDestination != NULL);

        switch (ibDest)
        {
        default:
            f_pbDestination[ ibInsert + --cbDecode ] = (DRM_BYTE)( ((rgbOutput[ 2 ] & 0x03) << 6) |   rgbOutput[ 3 ]);                   /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
            __fallthrough;
        case 3:
            f_pbDestination[ ibInsert + --cbDecode ] = (DRM_BYTE)( ((rgbOutput[ 1 ] & 0x0F) << 4) | ((rgbOutput[ 2 ] &0x3C) >> 2) );     /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
            __fallthrough;
        case 2:
            f_pbDestination[ ibInsert + --cbDecode ] = (DRM_BYTE)( (rgbOutput[ 0 ]          << 2) | ((rgbOutput[ 1 ] & 0x30) >> 4) );    /* Can't over/underflow: Computation of ibInsert/cbDecode ensures it */
        }
    }

    /* move the buffer to start of input buffer */
    if( f_fFlags & DRM_BASE64_DECODE_IN_PLACE )
    {
        ChkBOOL( f_pdasstrSource->m_cch >= *f_pcbDestination, DRM_E_ILWALIDARG );

        (DRM_VOID)DRM_BYT_MoveBytes(f_pbDestination, f_pdasstrSource->m_ich, f_pbDestination, ibInsert, *f_pcbDestination);
    }

    dr = DRM_SUCCESS;

ErrorExit:
    return dr;
}
#endif

#if defined (SEC_COMPILE)
static DRM_GLOBAL_CONST DRM_CHAR g_rgchBase64EncodingStandard[] PR_ATTR_DATA_OVLY(_g_rgchBase64EncodingStandard) = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#endif

#if defined (SEC_COMPILE)
/*
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_B64_EncodeA(
    __in_bcount( f_cbSource ) const    DRM_BYTE  *f_pbSource,    /* input buffer */
    __in                               DRM_DWORD  f_cbSource,    /* input len */
    __out_ecount_opt( *f_pcchEncoded ) DRM_CHAR  *f_pszB64,      /* output char */
    __inout                            DRM_DWORD *f_pcchEncoded, /* output ch len */
    __in                               DRM_DWORD  f_fFlags )
{
    DRM_DWORD cchRequired = 0;
    DRM_DWORD iInput      = 0;
    DRM_DWORD iOutput     = 0;
    DRM_RESULT dr         = DRM_SUCCESS;
    const DRM_CHAR *pszMapping = NULL;

    /*
    ** Null input buffer, null output size pointer, and a nonzero
    ** encoded size with a null output buffer are all invalid
    ** parameters.
    **
    ** Check the size first so that caller can allocate enough output buffer before generating
    ** the input buffer.
    */

    ChkArg(f_cbSource     > 0
        && f_pbSource    != NULL
        && f_pcchEncoded != NULL);

    if (f_fFlags == 0)
    {
        pszMapping = g_rgchBase64EncodingStandard;
    }
    else
    {
        ChkDR(DRM_E_ILWALIDARG);
    }

    cchRequired = CCH_BASE64_EQUIV(f_cbSource);     /* Effectively ensures cchRequired >= CCH_B64_IN_QUARTET == 4 because f_cbSource > 0 */

    /* if the buffer is too small or both buffers are NULL, we return the required buffer size */

    if ((*f_pcchEncoded < cchRequired)
    ||    f_pszB64     == NULL)
    {
        *f_pcchEncoded = cchRequired;
        ChkDR(DRM_E_BUFFERTOOSMALL);
    }

    *f_pcchEncoded = cchRequired;

    /* encoding starts from end of string */

    /*
    ** Colwert the input buffer bytes through the encoding table and
    ** out into the output buffer.
    */
    iInput  = (cchRequired / CCH_B64_IN_QUARTET) * CB_B64_OUT_TRIO - CB_B64_OUT_TRIO;   /* Can't underflow: cchRequired >= CCH_B64_IN_QUARTET */
    iOutput =  cchRequired - CCH_B64_IN_QUARTET;                                        /* Can't underflow: cchRequired >= CCH_B64_IN_QUARTET */

    DRM_FOR_INFINITE
    {
        const DRM_BYTE uc0 =                                               f_pbSource[ iInput ];
        const DRM_BYTE uc1 = (DRM_BYTE)( ( ( iInput + 1 ) < f_cbSource ) ? f_pbSource[ iInput + 1 ] : 0 );
        const DRM_BYTE uc2 = (DRM_BYTE)( ( ( iInput + 2 ) < f_cbSource ) ? f_pbSource[ iInput + 2 ] : 0 );

        ( (DRM_BYTE*)f_pszB64 )[ iOutput ]     = ( (DRM_BYTE*)pszMapping )[ uc0 >> 2 ];                                             /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        ( (DRM_BYTE*)f_pszB64 )[ iOutput + 1 ] = ( (DRM_BYTE*)pszMapping )[ ( ( uc0 << 4 ) & 0x30 ) | ( ( uc1 >> 4 ) & 0xf ) ];     /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        ( (DRM_BYTE*)f_pszB64 )[ iOutput + 2 ] = ( (DRM_BYTE*)pszMapping )[ ( ( uc1 << 2 ) & 0x3c ) | ( ( uc2 >> 6 ) & 0x3 ) ];     /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        ( (DRM_BYTE*)f_pszB64 )[ iOutput + 3 ] = ( (DRM_BYTE*)pszMapping )[ uc2 & 0x3f ];                                           /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */

        if (iInput ==0)
        {
            break;
        }

        iOutput -= CCH_B64_IN_QUARTET;  /* Can't underflow: Computation of iOutput ensures it */
        iInput  -= CB_B64_OUT_TRIO;     /* Can't underflow: Computation of iInput ensures it */
    }

    /*
    ** Fill in leftover bytes at the end
    */
    switch (f_cbSource % CB_B64_OUT_TRIO)
    {
        /*
        ** One byte out of three, add padding and fall through
        */
        case 1:
            ((DRM_BYTE*)f_pszB64)[ cchRequired - 2 ] = '=';     /* Can't underflow: cchRequired >= 4 */
            __fallthrough;
        /*
        ** Two bytes out of three, add padding.
        */
        case 2:
            ((DRM_BYTE*)f_pszB64)[ cchRequired - 1 ] = '=';     /* Can't underflow: cchRequired >= 4 */
            break;
        case 0:
        default:
            break;
    }

    dr = DRM_SUCCESS;

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_B64_EncodeW(
    __in_bcount( f_cbSource ) const    DRM_BYTE   *f_pbSource,    /* input buffer */
    __in                               DRM_DWORD   f_cbSource,    /* input len */
    __out_ecount_opt( *f_pcchEncoded ) DRM_WCHAR  *f_pwszEncoded, /* output WCHAR */
    __inout                            DRM_DWORD  *f_pcchEncoded, /* output ch len */
    __in                               DRM_DWORD   f_dwFlags )
{
    DRM_DWORD cchRequired = 0;
    DRM_DWORD iInput      = 0;
    DRM_DWORD iOutput     = 0;
    DRM_RESULT dr         = DRM_SUCCESS;
    const DRM_CHAR *pszMapping = NULL;

    /*
    ** Null input buffer, null output size pointer, and a nonzero
    ** encoded size with a null output buffer are all invalid
    ** parameters.
    **
    ** Check the size first so that caller can allocate enough output buffer before generating
    ** the input buffer.
    */

    ChkArg(f_cbSource     > 0
        && f_pbSource    != NULL
        && f_pcchEncoded != NULL);

    if (f_dwFlags == 0)
    {
        pszMapping = g_rgchBase64EncodingStandard;
    }
    else
    {
        ChkDR(DRM_E_ILWALIDARG);
    }

    cchRequired = CCH_BASE64_EQUIV(f_cbSource);     /* Effectively ensures cchRequired >= CCH_B64_IN_QUARTET == 4 because f_cbSource > 0 */

    /* if the buffer is too small or both buffers are NULL, we return the required buffer size */

    if ((*f_pcchEncoded < cchRequired)
    ||    f_pwszEncoded == NULL)
    {
        *f_pcchEncoded = cchRequired;
        ChkDR(DRM_E_BUFFERTOOSMALL);
    }

    *f_pcchEncoded = cchRequired;

    /* encoding starts from end of string */

    /*
    ** Colwert the input buffer bytes through the encoding table and
    ** out into the output buffer.
    */
    iInput  = (cchRequired / CCH_B64_IN_QUARTET) * CB_B64_OUT_TRIO - CB_B64_OUT_TRIO;   /* Can't underflow: cchRequired >= CCH_B64_IN_QUARTET */
    iOutput =  cchRequired - CCH_B64_IN_QUARTET;                                        /* Can't underflow: cchRequired >= CCH_B64_IN_QUARTET */

    DRM_FOR_INFINITE
    {
        const DRM_BYTE uc0 =                                               f_pbSource[ iInput ];
        const DRM_BYTE uc1 = (DRM_BYTE)( ( ( iInput + 1 ) < f_cbSource ) ? f_pbSource[ iInput + 1 ] : 0 );
        const DRM_BYTE uc2 = (DRM_BYTE)( ( ( iInput + 2 ) < f_cbSource ) ? f_pbSource[ iInput + 2 ] : 0 );

        f_pwszEncoded[ iOutput ]     = (DRM_WCHAR)DRM_WCHAR_CAST( ( (DRM_BYTE*)pszMapping )[ uc0 >> 2 ] );                                          /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        f_pwszEncoded[ iOutput + 1 ] = (DRM_WCHAR)DRM_WCHAR_CAST( ( (DRM_BYTE*)pszMapping )[ ( ( uc0 << 4 ) & 0x30 ) | ( ( uc1 >> 4 ) & 0xf ) ] );  /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        f_pwszEncoded[ iOutput + 2 ] = (DRM_WCHAR)DRM_WCHAR_CAST( ( (DRM_BYTE*)pszMapping )[ ( ( uc1 << 2 ) & 0x3c ) | ( ( uc2 >> 6 ) & 0x3 ) ] );  /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */
        f_pwszEncoded[ iOutput + 3 ] = (DRM_WCHAR)DRM_WCHAR_CAST( ( (DRM_BYTE*)pszMapping )[ uc2 & 0x3f ] );                                        /* Can't overflow: iOutput <= cchRequired - 4 && cchRequired >= 4 */

        if (iInput ==0)
        {
            break;
        }

        iOutput -= CCH_B64_IN_QUARTET;  /* Can't underflow: Computation of iOutput ensures it */
        iInput  -= CB_B64_OUT_TRIO;     /* Can't underflow: Computation of iInput ensures it */
    }

    /*
    ** Fill in leftover bytes at the end
    */
    switch (f_cbSource % CB_B64_OUT_TRIO)
    {
        /*
        ** One byte out of three, add padding and fall through
        */
        case 1:
            f_pwszEncoded[cchRequired - 2] = s_wchEqual;        /* Can't underflow: cchRequired >= 4 */
            __fallthrough;
        /*
        ** Two bytes out of three, add padding.
        */
        case 2:
            f_pwszEncoded[cchRequired - 1] = s_wchEqual;        /* Can't underflow: cchRequired >= 4 */
            break;
        case 0:
            __fallthrough;
        default:
            break;
    }

    dr = DRM_SUCCESS;

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

