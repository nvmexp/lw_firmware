/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMECCP256IMPL_C
#include <drmerr.h>
#include <drmprofile.h>
#include <bigdefs.h>
#include <bignum.h>
#include <field.h>
#include <fieldpriv.h>
#include <elwrve.h>
#include <mprand.h>
#include <oemeccp256impl.h>
#include <oemsha256.h>
#include <oem.h>
#include <oembyteorder.h>
#include <drmlastinclude.h>
#include "seapi.h"

ENTER_PK_NAMESPACE_CODE;

#define P256_KEY_GEN_TRIES ( 1000 )

#if DRM_SUPPORT_PRECOMPUTE_GTABLE

#define ECC_P256_TABLE_SPACING 5
#define ECC_P256_TABLE_LAST 256

extern const digit_t ECC_P256_GENERATOR_MULTPLE_TABLE[ ( ECC_P256_TABLE_LAST + 1 ) * ECC_P256_POINT_SIZE_IN_ECAFFINES ];

#endif /* DRM_SUPPORT_PRECOMPUTE_GTABLE */

//
// LWE (kwilson) - We will use these values directly to prevent the need to declare another lwrve.
// Once all ECC functionality has been ported to HW SE, then there will only be a need for one lwrve
// and these can be removed.
//
extern DRM_GLOBAL_CONST digit_t g_rgdConstA_P256_PreM[ECC_P256_INTEGER_SIZE_IN_DIGITS];
extern DRM_GLOBAL_CONST digit_t g_rgdConstB_P256_PreM[ECC_P256_INTEGER_SIZE_IN_DIGITS];
extern DRM_GLOBAL_CONST digit_t g_rgecConstG_P256_PreM[ECC_P256_POINT_SIZE_IN_ECAFFINES];

/*
** Function Implementations
*/

/*************************************************************************************************
**
** Function: _Colwert_P256_BigEndianBytesToModular
**
** Description: Colwerts big endian bytes into modular formatted
**
** Args:
**    [f_rgbInBytes]:  The bytes to be colwerted into modular digits ( ecaffines )
**    [f_pElwrve ]:    EC that we are working on
**    [f_rgecModular]: The colwerted modular format integer output
**    [f_pBigCtx]:     Bignum context pointer
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT _Colwert_P256_BigEndianBytesToModular(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )              const  DRM_BYTE  f_rgbInBytes[],
    __in_ecount( 1 )                                           const  elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_ECAFFINES )                digit_t   f_rgecModular[],
    __inout                                                    struct bigctx_t *f_pBigCtx );
static DRM_NO_INLINE DRM_RESULT _Colwert_P256_BigEndianBytesToModular(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )              const  DRM_BYTE  f_rgbInBytes[],
    __in_ecount( 1 )                                           const  elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_ECAFFINES )                digit_t   f_rgecModular[],
    __inout                                                    struct bigctx_t *f_pBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdTemp[ ECC_P256_POINT_SIZE_IN_DIGITS / 2 ];

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_ECC_P256, PERF_FUNC_Colwert_P256_BigEndianBytesToModular );
#endif

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_rgbInBytes );
    ChkArg( NULL != f_pElwrve );
    ChkArg( NULL != f_rgecModular );
    ChkArg( NULL != f_pBigCtx );
    ChkArg( NULL != f_pElwrve->fdesc );
    ChkArg( NULL != f_pElwrve->fdesc->modulo );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_DIGITS == f_pElwrve->fdesc->modulo->length );

    /*
    ** First colwert to digits
    */
    ChkDR( Colwert_P256_BigEndianBytesToDigitsImpl( f_rgbInBytes, rgdTemp ) );

    /*
    ** Now Colwert to modular format
    */
    ChkBOOL( to_modular( rgdTemp,
                         ECC_P256_INTEGER_SIZE_IN_DIGITS,
                         f_rgecModular,
                         f_pElwrve->fdesc->modulo,
                         f_pBigCtx ),
             DRM_E_P256_COLWERSION_FAILURE );

ErrorExit:

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_LEAVE_SCOPE;
#endif

    return dr;
} /* end _Colwert_P256_BigEndianBytesToModular */
#endif
/************************************************************************************************
**
** Function:  OEM_ECC_MapX2PointP256Impl
**
** Synopsis:  Given X coordinate of a point tries to callwlate Y coordinate. Such that the
**            resulting point is on the P256 lwrve. Fails if it is not possible.
**
** Arguments:
**    [f_pX]:               X Coordinate
**    [f_pY]:               Output Y Coordinate
**    [f_rgdSuppliedTemps]: Optional temps Array for math functions
**    [f_pBigCtx]:          Bignum context pointer, assumes initialized one.
**
** Returns: DRM_RESULT indicating success or the corresponding error code,
**          DRM_E_P256_PLAINTEXT_MAPPING_FAILURE is returned if mapping was not possible.
**          f_pY is filled in case of success.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_MapX2PointP256Impl(
    __in_ecount(ECC_P256_INTEGER_SIZE_IN_DIGITS)   const digit_t  *f_pX,
    __out_ecount(ECC_P256_INTEGER_SIZE_IN_DIGITS)        digit_t  *f_pY,
    __inout_opt                                          digit_t   f_rgdSuppliedTemps[],
    __inout                                       struct bigctx_t *f_pBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BOOL   fSquare = FALSE;
    digit_t    rgdYSqrd[ ECC_P256_INTEGER_SIZE_IN_DIGITS * 2 ]; /* big enough for x^3 + ax + b */

    ChkArg( f_pX != NULL );
    ChkArg( f_pY != NULL );
    ChkArg( f_pBigCtx != NULL );

    OEM_SELWRE_DIGITZEROMEMORY( rgdYSqrd, DRM_NO_OF(rgdYSqrd) );

    /*
    ** Compute the right-hand size of the EC function in two steps:
    ** rgdYSqrd = x^2 + a
    ** rgdYSqrd = x^3 + ax + b
    **
    ** FYI: EC function is defined as y^2 = x^3 + ax + b
    */
    ChkBOOL( Kmuladd( f_pX,
                      f_pX,
                      g_Elwrve.a,
                      rgdYSqrd,
                      g_Elwrve.fdesc,
                      f_rgdSuppliedTemps,
                      f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    ChkBOOL( Kmuladd( f_pX,
                      rgdYSqrd,
                      g_Elwrve.b,
                      rgdYSqrd,
                      g_Elwrve.fdesc,
                      f_rgdSuppliedTemps,
                      f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Callwlate the square root of our x^3 + ax + b, this solves for y,
    ** b/c the equation is: y^2 = x^3 + ax + b, if this function succeeds in
    ** producing a root then we have successfully mapped the array to a point!
    */
    ChkBOOL( Kprime_sqrter( rgdYSqrd,
                            f_pY,
                            1,
                            g_Elwrve.fdesc,
                           &fSquare,
                            f_pBigCtx ), DRM_E_P256_PKCRYPTO_FAILURE);

    /*
    ** if we could not square root the value, then return the mapping error
    */
    ChkBOOL( fSquare, DRM_E_P256_PLAINTEXT_MAPPING_FAILURE ); /* Is x^3 + ax + b  a square? */

ErrorExit:
    return dr;
}
#endif

/************************************************************************************************
**
** Function:  Colwert_P256_PlaintextToPoint
**
** Synopsis:  Maps a plaintext array into an EC point, this can fail if the plaintext
**            is not a valid x coord on the EC lwrve. Colwersion expands the data by 100%
**
** Arguments:
**    [f_pPlaintext]:       Input Plaintext data array to be colwerted into a point
**    [f_pElwrve ]:         EC that we are working on
**    [f_rgdSuppliedTemps]: Optional temps array that the math functions can use.
**    [f_rgptOutPoint]:     Output plaintext in modular point format ( ecaffines )
**    [f_pBigCtx]:          Bignum context pointer
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:   This function is very similar to HMAC callwlation function.
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_RESULT Colwert_P256_PlaintextToPoint(
    __in_ecount( 1 )                                 const  PLAINTEXT_P256 *f_pPlaintext,
    __in_ecount( 1 )                                 const  elwrve_t       *f_pElwrve,
    __out                                                   digit_t         f_rgdSuppliedTemps[],
    __out_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES )        digit_t         f_rgptOutPoint[],
    __inout                                          struct bigctx_t       *f_pBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT dr      = DRM_SUCCESS;

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_ECC_P256, PERF_FUNC_Colwert_P256_PlaintextToPoint );
#endif

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPlaintext );
    ChkArg( NULL != f_pElwrve );
    ChkArg( NULL != f_rgptOutPoint );
    ChkArg( NULL != f_pBigCtx );


    /*
    ** Colwert to the array to modular format
    */
    ChkDR( _Colwert_P256_BigEndianBytesToModular( f_pPlaintext->m_rgbPlaintext,
                                                 f_pElwrve,
                                                 f_rgptOutPoint,
                                                 f_pBigCtx ) );

    ChkDR( OEM_ECC_MapX2PointP256Impl( f_rgptOutPoint,
                                  &f_rgptOutPoint[ ECC_P256_POINT_SIZE_IN_ECAFFINES / 2 ] ,
                                   f_rgdSuppliedTemps,
                                   f_pBigCtx ) );

ErrorExit:

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_LEAVE_SCOPE;
#endif

    return dr;
} /* end Colwert_P256_PlaintextToPoint */
#endif

/*************************************************************************************************
**
** Function: Colwert_P256_PointToBigEndianBytes
**
** Description: Colwerts a modular EC point into big endian bytes format
**
** Args:
**    [f_rgptPoint]:  EC point to colwert
**    [f_pElwrve ]:   EC that we are working on
**    [f_pPlaintext]: Colwert plaintext output
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Colwert_P256_PointToBigEndianBytes(
    __in_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES )           const digit_t   f_rgptInPoint[],
    __in_ecount( 1 )                                          const elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_POINT_SIZE_IN_BYTES )                    DRM_BYTE  f_rgbOutBytes[] )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdTemp[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_ECC_P256, PERF_FUNC_Colwert_P256_PointToBigEndianBytes );
#endif

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_rgptInPoint );
    ChkArg( NULL != f_pElwrve );
    ChkArg( NULL != f_rgbOutBytes );
    ChkArg( NULL != f_pElwrve->fdesc );
    ChkArg( NULL != f_pElwrve->fdesc->modulo );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_DIGITS == f_pElwrve->fdesc->modulo->length );

    /*
    ** Colwert the x-coord out of modular format ( into digits ) then into bytes
    */
    ChkBOOL( from_modular( f_rgptInPoint,
                           rgdTemp,
                           f_pElwrve->fdesc->modulo ),
             DRM_E_P256_COLWERSION_FAILURE );
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdTemp, f_rgbOutBytes ) );

    /*
    ** Colwert the y-coord out of modular format ( into digits ) then into bytes
    */
    ChkBOOL( from_modular( f_rgptInPoint + ECC_P256_POINT_SIZE_IN_ECAFFINES / 2,
                           rgdTemp,
                           f_pElwrve->fdesc->modulo ),
             DRM_E_P256_COLWERSION_FAILURE );
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdTemp,
                                               &f_rgbOutBytes[ ECC_P256_POINT_SIZE_IN_BYTES / 2 ] ) );

ErrorExit:

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_LEAVE_SCOPE;
#endif

    return dr;
} /* end Colwert_P256_PointToBigEndianBytes */
#endif

/*************************************************************************************************
**
** Function: Colwert_P256_PointToPlaintextImpl
**
** Description: Colwerts points into plaintext, assuming a lossless integer-to-point formula
**              see OEM_ECC_Encrpyt_P256 for details
**
** Arguements:
**    [f_rgptPoint]:  EC point to colwert
**    [f_pElwrve ]:   EC that we are working on
**    [f_pPlaintext]: Colwert plaintext output
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Colwert_P256_PointToPlaintextImpl(
    __in_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES ) const digit_t         f_rgptPoint[],
    __in_ecount( 1 )                                const elwrve_t       *f_pElwrve,
    __out_ecount( 1 )                                     PLAINTEXT_P256 *f_pPlaintext )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdTemp[ ECC_P256_INTEGER_SIZE_IN_DIGITS ] = { 0 };

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPlaintext );
    ChkArg( NULL != f_rgptPoint );
    ChkArg( NULL != f_pElwrve );
    ChkArg( NULL != f_pElwrve->fdesc );
    ChkArg( FIELD_Q_MP == f_pElwrve->fdesc->ftype );
    ChkArg( NULL != f_pElwrve->fdesc->modulo );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_DIGITS == f_pElwrve->fdesc->modulo->length );

    /*
    ** Colwert the x coord of the point from modular format to digits
    */
    ChkBOOL( from_modular( f_rgptPoint,
                           rgdTemp,
                           f_pElwrve->fdesc->modulo ),
             DRM_E_P256_COLWERSION_FAILURE );

    /*
    ** Now colwert the digits into the DRM_BYTE array output
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdTemp, f_pPlaintext->m_rgbPlaintext ) );

ErrorExit:
    return dr;
} /* end Colwert_P256_PointToPlaintextImpl */

/*************************************************************************************************
**
** Function:    OEM_ECC_GenerateHMACKey_P256Impl
**
** Description: This function assumes that PLAINTEXT_P256 stucture can contain 2 128bit AES keys.
**              It will, given a PLAINTEXT_P256 struct with the 2nd key set, generate the 1st key
**              in the struct (used for XMR HMACing ) such that will have a 100% chance of being
**              Mapped to an EC point without losing any information. (See OEM_ECC_Encrypt_P256Impl
**              function header for more details ).
**
** Arguements:
**    [f_pKeys]   : A struct containing the ALREADY SET 2nd key (content key ) for XMR lics.
**    [f_pBigCtx] : Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**    [f_cbBigCtx]: size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes: This function uses the P256 bit lwrve.
**        ( !!!) There is a very small chance that this function may fail to find a HMAC key
**        that, coupled with the content key will map to an EC Point. While the probability
**        is very low (way less then .01%) the caller should check if the DRM_RESULT returned
**        is DRM_E_P256_HMAC_KEYGEN_FAILURE. In which case calling the function another time will
**        probably solve the issue.
**
*************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenerateHMACKey_P256Impl(
    __inout                     PLAINTEXT_P256  *f_pKeys,
    __inout_bcount(f_cbBigCtx)  struct bigctx_t *f_pBigCtx,
    __in                        DRM_DWORD        f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT dr       = DRM_SUCCESS;
    DRM_DWORD  i        = 0;
    digit_t    rgdRandNum[ ECC_P256_POINT_SIZE_IN_DIGITS ];

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pKeys );
    ChkArg( NULL != f_pBigCtx );

    OEM_SELWRE_DIGITZEROMEMORY( rgdRandNum, DRM_NO_OF(rgdRandNum) );

    /*
    ** Init big num context
    */
    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    /*
    ** Zero the HMAC key that was given
    */
    OEM_SELWRE_MEMSET( f_pKeys, 0, ECC_P256_INTEGER_SIZE_IN_BYTES / 2 );

    /*
    ** Colwert the first key to digits
    */
    ChkDR( Colwert_P256_BigEndianBytesToDigitsImpl( f_pKeys->m_rgbPlaintext, rgdRandNum ) );

    /*
    ** Now do a loop where I attempt to generate an AES key such that the two
    ** keys will successfully map into an EC point
    */
    for ( i = 0; i < P256_KEY_GEN_TRIES; ++i )
    {
        /*
        ** Generate the digits
        */
        ChkBOOL( random_mod_nonzero( g_Elwrve.gorder + ECC_P256_INTEGER_SIZE_IN_DIGITS / 2,
                                     rgdRandNum + ECC_P256_INTEGER_SIZE_IN_DIGITS / 2,
                                     ECC_P256_INTEGER_SIZE_IN_DIGITS / 2,
                                     f_pBigCtx ),
                 DRM_E_P256_PKCRYPTO_FAILURE );


        /*
        ** Check if it there is a solution to the EC equation for this x value,
        ** if there is then we have generated a good key.
        */
        dr = OEM_ECC_CanMapToPoint_P256Impl( rgdRandNum, f_pBigCtx );

        if ( DRM_SUCCESS == dr )
        {
            /*
            ** SUCCESS: Now lets covert the digits to bytes
            */
            ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdRandNum, f_pKeys->m_rgbPlaintext ) );
            break;
        } /* end if */
        else if ( DRM_S_FALSE == dr )
        {
            /*
            ** The number we produced does not have a solution to the EC equation, try again.
            */
            continue;
        } /* end else if */
        else
        {
            /*
            ** Some error oclwred in the Mapping test function, bail out of this function.
            */
            ChkDR( dr );
        }
    } /* end for */

    /*
    ** Check we ran out of generation tries, if so then return a corresponding error.
    */
    if ( i >= P256_KEY_GEN_TRIES )
    {
        ChkDR( DRM_E_P256_HMAC_KEYGEN_FAILURE );
    } /* end if */
ErrorExit:
    return dr;
}
#endif



/************************************************************************************************
**
** Function:  OEM_ECC_CanMapToPoint_P256Impl
**
** Synopsis:  This function test to see if a number can be mapped into an EC point, to do this
**            the function checks if the number has a solution to the equation y^2 = x^3 + ax + b,
**            where the passed in number is the 'x' value
**
** Arguments:
**    [f_rgdNumber]: Number to test if it can map to an EC point
**    [f_pBigCtx]  : Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations, assumes initialized struct
**
** Returns: DRM_SUCCESS if the number can be mapped, DRM_S_FALSE if it can not, and returns
**          an error code if there has an error while exelwting the function
**
** Notes:
**
*************************************************************************************************/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_CanMapToPoint_P256Impl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS ) const  digit_t   f_rgdNumber[],
    __inout                                        struct bigctx_t *f_pBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT dr      = DRM_SUCCESS;
    digit_t    rgptTemp[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];

    OEM_SELWRE_MEMSET( rgptTemp, 0, sizeof( rgptTemp ) );

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_rgdNumber );
    ChkArg( NULL != f_pBigCtx );
    ChkArg( NULL != g_Elwrve.fdesc->modulo );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_DIGITS == g_Elwrve.fdesc->modulo->length );


    /*
    ** Colwert to the array to modular format
    */
    ChkBOOL( to_modular( f_rgdNumber,
                         ECC_P256_INTEGER_SIZE_IN_DIGITS,
                         rgptTemp,
                         g_Elwrve.fdesc->modulo,
                         NULL ),
             DRM_E_P256_COLWERSION_FAILURE );

    dr = OEM_ECC_MapX2PointP256Impl(
        rgptTemp,
       &rgptTemp[ ECC_P256_POINT_SIZE_IN_ECAFFINES / 2 ] ,
        NULL,
        f_pBigCtx );

    if ( DRM_E_P256_PLAINTEXT_MAPPING_FAILURE == dr )
    {
        dr = DRM_S_FALSE;
    }

ErrorExit:
    return dr;
} /* end OEM_ECC_CanMapToPoint_P256Impl */
#endif
#ifdef NONE
DRM_API_VOID DRM_VOID DRM_CALL OEM_ECC_ZeroPrivateKey_P256Impl(
    __inout        PRIVKEY_P256    *f_pPrivkey )
{
    if( NULL != f_pPrivkey )
    {
        OEM_SELWRE_ZERO_MEMORY( f_pPrivkey, sizeof( *f_pPrivkey ) );
    }
}
#endif

/************************************************************************************************
**
** Function: OEM_ECC_Decrypt_P256Impl
**
** Synopsis: Performs EC El-Gamal P256 decryption on a buffer. The equation is:
**           P = C2 - k*C1, where ( C1, C2 ) is the ciphertext, P is the plaintext
**           and k is the private key.
**
** Arguments:
**    [f_pprivkey] -- Private key to decrypt with
**    [f_pCiphertext]    -- Encrypted bytes that are to be decrypted
**    [f_pPlaintext]   -- Clear text result
**    [f_pBigCtx]  -- Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**    [f_cbBigCtx]  -- size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes: It is put in the [f_pPlaintext]. This function uses
**        the private key, be sure to protect/obfuscate/encrypt this function if the private key
**        is an asset. Also refer to the OEM_ECC_Decrypt_P256Impl function comments for more details
**        about the exact protocol for encyption and plaintext colwesion.
**
*************************************************************************************************/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_Decrypt_P256Impl(
    __in    const  PRIVKEY_P256    *f_pPrivkey,
    __in    const  CIPHERTEXT_P256 *f_pCiphertext,
    __out          PLAINTEXT_P256  *f_pPlaintext,
    __inout struct bigctx_t        *f_pBigCtx,
    __in           DRM_DWORD        f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdTemps[ ECC_P256_DECRYPT_TEMPS ];
    digit_t    rgdPrivkey[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t rgptC1[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    digit_t rgptC2[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    digit_t rgptResult[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    const CIPHERTEXT_P256_2POINTS *pCtPoints = ( const CIPHERTEXT_P256_2POINTS* )f_pCiphertext;

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPrivkey );
    ChkArg( NULL != f_pCiphertext );
    ChkArg( NULL != f_pPlaintext );
    ChkArg( NULL != f_pBigCtx );

    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    /*
    ** Colwert priv key from big endian bytes to digits
    */
    ChkDR( Colwert_P256_BigEndianBytesToDigitsImpl( f_pPrivkey->m_rgbPrivkey, rgdPrivkey ) );

    /*
    ** Colwert the ciphertext byte stream to two modular format points
    */
    ChkDR( Colwert_P256_BigEndianBytesToPointImpl( pCtPoints->m_rgbC1,
                                              &g_Elwrve,
                                               rgptC1,
                                               f_pBigCtx ) );
    ChkDR( Colwert_P256_BigEndianBytesToPointImpl( pCtPoints->m_rgbC2,
                                              &g_Elwrve,
                                               rgptC2,
                                               f_pBigCtx ) );

    /*
    ** Calc: result := k*C1, where k is the private key
    */
    ChkBOOL( ecaffine_exponentiation( rgptC1,
                                      rgdPrivkey,
                                      ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                      rgptResult,
                                     &g_Elwrve,
                                      f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

    /*
    ** Calc: plaintext point == result := C2 - k*C1, where k*C1
    ** was callwlated in the previous step
    */
    ChkBOOL( ecaffine_addition( rgptC2,
                                rgptResult,
                                rgptResult,
                                ECC_POINT_SUBTRACTION,
                               &g_Elwrve,
                                rgdTemps,
                                f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

    ChkDR( Colwert_P256_PointToPlaintextImpl( rgptResult,
                                        &g_Elwrve,
                                         f_pPlaintext ) );

ErrorExit:
    OEM_SELWRE_ZERO_MEMORY( rgdPrivkey, ECC_P256_INTEGER_SIZE_IN_DIGITS  * sizeof( digit_t ) );
    OEM_SELWRE_ZERO_MEMORY( rgptResult, ECC_P256_POINT_SIZE_IN_ECAFFINES * sizeof( digit_t ) );

    return dr;
}/* end OEM_ECC_Decrypt_P256Impl */


/************************************************************************************************
**
** Function:  OEM_ECC_Encrypt_P256CoreImpl
**
** Synopsis:  Encrpyts a block of plaintext, given a public key. The algorithm is EC El-Gamal:
**            C1 = r*G, C2 = P + r*K, where ( C1, C2 ) is the ciphertext, r is a runtime generated
**            random integer (256 bit) K is the public key, G is the generator or basepoint for
**            the elliptic lwrve and P is the plaintext.
**
** Arguments:
**    [f_pPubkey]:     Public key used to encrypt the plaintext
**    [f_ptPlaintext]: To-point mapped plaintext data to be encrypted.
**                     This function does not check that it is on the lwrve.
**    [f_pCiphertext]: Ciphertext result, this will be output if function succeeds
**    [f_pdTemps]:     space for some temporary variables.
**    [f_pBigCtx]:     Pointer to initialized DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:  Data is not encrpyted in place.  It is put in the f_pCiphertext. Not all plaintext
**         maps to ciphertext. There are two methods in which to deal with this:
**         1) Shorten the plaintext buffer such that the left over space can be arbirarily changed.
**            At decryption time we will not be able to retrieve those spare bytes.
**         2) Choose the ciphertext such that we know that it will be able to map to ciphertext.
**            In this way we can use all 32 bytes in the plaintext buffer, but if someone does not
**            choose a good plaintext then there is a chance that encryption will fail!
**         This function implements the 2nd of the two options, so your plaintext must be good!
**
*************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_Encrypt_P256CoreImpl(
    __in                                            const  PUBKEY_P256     *f_pPubkey,
    __in_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES ) const  digit_t         *f_ptPlaintext,
    __out                                                  CIPHERTEXT_P256 *f_pCiphertext,
    __out_ecount( ECC_P256_ENCRYPT_TEMPS )                 digit_t         *f_pdTemps,
    __inout                                         struct bigctx_t        *f_pBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdR[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    CIPHERTEXT_P256_2POINTS *pCtPoints = ( CIPHERTEXT_P256_2POINTS* )f_pCiphertext;
    digit_t    rgptPubkey[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    /*
        these are initialized during callwlation
    */
    digit_t    rgptC1[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    digit_t    rgptC2[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];

    ChkArg( f_pPubkey != NULL );
    ChkArg( f_ptPlaintext != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pdTemps != NULL );
    ChkArg( f_pBigCtx != NULL );

    OEM_SELWRE_MEMSET( rgdR, 0, sizeof( rgdR ) );

    /*
    ** Colwert the public key and generator point ( aka base point ) into point format
    */
    ChkDR( Colwert_P256_BigEndianBytesToPointImpl( f_pPubkey->m_rgbPubkey,
                                              &g_Elwrve,
                                               rgptPubkey,
                                               f_pBigCtx ) );

    /*
    ** Verify the (now colwerted) point is a valid lwrve point
    */
    ChkBOOL( ecaffine_on_lwrve( rgptPubkey,
                               &g_Elwrve,
                                NULL,
                                f_pdTemps,
                                f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

    /*
    ** Verify the point is not infinite
    */
    ChkBOOL( !ecaffine_is_infinite( rgptPubkey, &g_Elwrve, f_pBigCtx ), DRM_E_PKCRYPTO_FAILURE );

    /*
    ** Generate our random integer for encryption, r ( it must be smaller than our lwrve's order )
    */
    ChkBOOL( random_mod_nonzero( g_Elwrve.gorder,
                                 rgdR,
                                 ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                 f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

    /*
    ** Callwlate C1 = rG
    */
#if DRM_SUPPORT_PRECOMPUTE_GTABLE

    ChkBOOL( ecaffine_exponentiation_tabular( ECC_P256_GENERATOR_MULTPLE_TABLE,
                                              ECC_P256_TABLE_SPACING,
                                              ECC_P256_TABLE_LAST,
                                              rgdR,
                                              ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                              rgptC1,
                                             &g_Elwrve,
                                              f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );
#else /* DRM_SUPPORT_PRECOMPUTE_GTABLE */
    ChkBOOL( ecaffine_exponentiation( g_Elwrve.generator,
                                      rgdR,
                                      ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                      rgptC1,
                                     &g_Elwrve,
                                      f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );
#endif /* DRM_SUPPORT_PRECOMPUTE_GTABLE */
    /*
    ** Callwlate C2 = P + rK where P is the plaintext point and K is the pub key point
    */
    ChkBOOL( ecaffine_exponentiation( rgptPubkey,
                                      rgdR,
                                      ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                      rgptC2,
                                     &g_Elwrve,
                                      f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

    ChkBOOL( ecaffine_addition( f_ptPlaintext,
                                rgptC2,
                                rgptC2,
                                ECC_POINT_ADDITION,
                               &g_Elwrve,
                                f_pdTemps,
                                f_pBigCtx ),
             DRM_E_PKCRYPTO_FAILURE );

     /*
     ** We now have our two ciphertext points, now colwert them to one big
     ** ciphertext byte array ( in big endian bytes format )
     */
    ChkDR( Colwert_P256_PointToBigEndianBytes( rgptC1,
                                              &g_Elwrve,
                                               pCtPoints->m_rgbC1 ) );
    ChkDR( Colwert_P256_PointToBigEndianBytes( rgptC2,
                                              &g_Elwrve,
                                               pCtPoints->m_rgbC2 ) );

ErrorExit:
    OEM_SELWRE_ZERO_MEMORY( rgdR, ECC_P256_INTEGER_SIZE_IN_DIGITS * sizeof( digit_t ) ) ;

    return dr;
}


/************************************************************************************************
**
** Function:  OEM_ECC_Encrypt_P256Impl
**
** Synopsis:  Encrpyts a block of plaintext, given a public key. The algorithm is EC El-Gamal:
**            C1 = r*G, C2 = P + r*K, where ( C1, C2 ) is the ciphertext, r is a runtime generated
**            random integer (256 bit) K is the public key, G is the generator or basepoint for
**            the elliptic lwrve and P is the plaintext.
**
** Arguments:
**    [f_pPubkey]:     Public key used to encrypt the plaintext
**    [f_pPlaintext]:  Plaintext data to be encrypted. Not all plaintext can map to Ciphertext!
**    [f_pCiphertext]: Ciphertext result, this will be output if function succeeds
**    [f_pBigCtx]:     Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations;
**    [f_cbBigCtx]:    size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:  Data is not encrpyted in place.  It is put in the f_pCiphertext. Not all plaintext
**         maps to ciphertext. There are two methods in which to deal with this:
**         1) Shorten the plaintext buffer such that the left over space can be arbirarily changed.
**            At decryption time we will not be able to retrieve those spare bytes.
**         2) Choose the ciphertext such that we know that it will be able to map to ciphertext.
**            In this way we can use all 32 bytes in the plaintext buffer, but if someone does not
**            choose a good plaintext then there is a chance that encryption will fail!
**         This function implements the 2nd of the two options, so your plaintext must be good!
**
*************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_Encrypt_P256Impl(
    __in    const  PUBKEY_P256     *f_pPubkey,
    __in    const  PLAINTEXT_P256  *f_pPlaintext,
    __out          CIPHERTEXT_P256 *f_pCiphertext,
    __inout struct bigctx_t        *f_pBigCtx,
    __in           DRM_DWORD        f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdTemps[ ECC_P256_ENCRYPT_TEMPS ];
    digit_t    rgptPlaintext[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];

    OEM_SELWRE_MEMSET( rgptPlaintext, 0, sizeof( rgptPlaintext ) );

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPubkey );
    ChkArg( NULL != f_pPlaintext );
    ChkArg( NULL != f_pCiphertext );
    ChkArg( NULL != f_pBigCtx );

    /*
    ** Ensure that BigNum context is initialized
    */
    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    /*
    ** Now map the plaintext byte array to a plaintext point
    */
    ChkDR( Colwert_P256_PlaintextToPoint( f_pPlaintext,
                                        &g_Elwrve,
                                         rgdTemps,
                                         rgptPlaintext,
                                         f_pBigCtx ) );

    ChkDR( OEM_ECC_Encrypt_P256CoreImpl( f_pPubkey, rgptPlaintext, f_pCiphertext, rgdTemps, f_pBigCtx ) );

ErrorExit:
    OEM_SELWRE_ZERO_MEMORY( rgptPlaintext, ECC_P256_POINT_SIZE_IN_ECAFFINES * sizeof( digit_t ) );

    return dr;
} /* end OEM_ECC_Encrypt_P256Impl */

/************************************************************************************************
**
** Function:  OEM_ECC_GenKeyPair_P256Impl
**
** Synopsis:  Generates a public-private key pair for the P256 FIPS Elliptic Lwrve
**
** Arguments:
**    [f_pPubKey]:  A pointer to the structure that will be populated with the new public key
**    [f_pPrivKey]: A pointer to the structure that will be populated with the new public key
**    [f_pBigCtx]:  Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**    [f_cbBigCtx]:    size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes: Generates a private key, this function should be protected the key is deemed as an asset
**
*************************************************************************************************/
DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPair_P256Impl(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT dr = DRM_SUCCESS;
    digit_t    rgdPriv[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t    rgptPub[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPrivKey );
    ChkArg( NULL != f_pPubKey );
    ChkArg( NULL != f_pBigCtx );

    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    /*
    ** Create the private key such that it is any random number < lwrve order
    */
    ChkBOOL( random_mod_nonzero( g_Elwrve.gorder,
                                 rgdPriv,
                                 ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                 f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Compute the Public key ( privKey * G )
    */
    ChkBOOL( ecaffine_exponentiation( g_Elwrve.generator,
                                      rgdPriv,
                                      ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                      rgptPub,
                                     &g_Elwrve,
                                      f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Just to be safe lets make sure the pubkey is on the lwrve
    */
    ChkBOOL( ecaffine_on_lwrve( rgptPub,
                               &g_Elwrve,
                                NULL,
                                NULL,
                                f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Now Colwert the priv and pub keys into big-endian DRM-BYTES
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdPriv, f_pPrivKey->m_rgbPrivkey ) );
    ChkDR( Colwert_P256_PointToBigEndianBytes( rgptPub,
                                              &g_Elwrve,
                                               f_pPubKey->m_rgbPubkey ) );
ErrorExit:
    return dr;
} /* end OEM_ECC_GenKeyPair_P256Impl */

/************************************************************************************************
**
** Function:  OEM_ECC_GenKeyPair_P256Impl_LWIDIA
**
** Synopsis:  Generates a public-private key pair for the P256 FIPS Elliptic Lwrve using Lwpu
**            HW accelerated Security Engine crypto functions
**
** Arguments:
**    [f_pPubKey]:  A pointer to the structure that will be populated with the new public key
**    [f_pPrivKey]: A pointer to the structure that will be populated with the new public key
**    [f_pBigCtx]:  Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**    [f_cbBigCtx]:    size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes: Generates a private key, this function should be protected the key is deemed as an asset
**
*************************************************************************************************/
DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPair_P256Impl_LWIDIA(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT  dr    = DRM_SUCCESS;
    DRM_RESULT  drTmp = DRM_SUCCESS;
    digit_t     rgdPriv[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdR[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdS[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPrivKey );
    ChkArg( NULL != f_pPubKey );
    ChkArg( NULL != f_pBigCtx );

    ChkDR(OEM_ECC_InitializeBignumStackImpl(f_pBigCtx, f_cbBigCtx));

    /*
    ** Create the private key such that it is any random number < lwrve order
    */
    ChkBOOL(random_mod_nonzero(g_Elwrve.gorder,
                                 rgdPriv,
                                 ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                 f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );
#ifdef SELWRITY_ENGINE
    /*
    ** Compute the Public key ( privKey * G )
    ** We will use the pre-Montgomery colwersion parameters directly, rather than declare a new lwrve.
    */
    if (seECPointMult((LwU32 *)g_Elwrve.fdesc->modulo->modulus, (LwU32 *)&g_rgdConstA_P256_PreM[0],
                      (LwU32 *)&g_rgecConstG_P256_PreM[0],      (LwU32 *)&g_rgecConstG_P256_PreM[8],
                                rgdPriv,                                  rgdR,
                                rgdS,                                     SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        dr = DRM_E_P256_PKCRYPTO_FAILURE;
        goto ErrorExit;
    }

    /*
    ** Just to be safe lets make sure the pubkey is on the lwrve
    */
    if (seECPointVerification((LwU32 *)g_Elwrve.fdesc->modulo->modulus, (LwU32 *)&g_rgdConstA_P256_PreM[0],
                              (LwU32 *)&g_rgdConstB_P256_PreM[0],                 rgdR,
                                        rgdS,                                     SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        dr = DRM_E_SE_CRYPTO_POINT_NOT_ON_LWRVE;
        goto ErrorExit;
    }
#endif
    /*
    ** Now Colwert the priv and pub keys into big-endian DRM-BYTES
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl(rgdPriv, f_pPrivKey->m_rgbPrivkey ));
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl(rgdR, f_pPubKey->m_rgbPubkey));
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl(rgdS, &f_pPubKey->m_rgbPubkey[ECC_P256_INTEGER_SIZE_IN_BYTES]));

ErrorExit:
    if (dr == DRM_SUCCESS)
    {
        dr = drTmp;
    }
    return dr;
} /* end OEM_ECC_GenKeyPair_P256Impl_LWIDIA */
#endif
/************************************************************************************************
**
** Function:  OEM_ECC_GenKeyPairRestrictedPriv_P256Impl
**
** Synopsis:  Generates a public-private key pair for the P256 FIPS Elliptic Lwrve
**
** Arguments:
**    [f_pPubKey]:  A pointer to the structure that will be populated with the new public key
**    [f_pPrivKey]: A pointer to the structure that will be populated with the new public key
**    [f_pBigCtx]:  Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**    [f_cbBigCtx]:    size of allocated f_pBigCtx
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes: Generates a private key, this function should be protected the key is deemed as an asset
**
*************************************************************************************************/
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPairRestrictedPriv_P256Impl(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** decls
    */
    DRM_RESULT dr         = DRM_SUCCESS;
    DRM_DWORD  dwNumTries = 0;
    /* All values initialized later */
    digit_t    rgdPriv[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t    rgptPub[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pPrivKey );
    ChkArg( NULL != f_pPubKey );
    ChkArg( NULL != f_pBigCtx );

    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    /*
    ** Create the private key such that it is any random number < lwrve order
    */
    do
    {
        /*
        ** Make sure we have not tried too many times (prevent infinite loop)
        */
        if ( P256_KEY_GEN_TRIES < dwNumTries )
        {
            ChkDR( DRM_E_P256_PLAINTEXT_MAPPING_FAILURE );
        } /* end if */

        /*
        ** Generate random key, test to see if it is a valid key
        */
        ChkBOOL( random_mod_nonzero( g_Elwrve.gorder,
                                     rgdPriv,
                                     ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                     f_pBigCtx ),
                 DRM_E_P256_PKCRYPTO_FAILURE );

        ChkDR( OEM_ECC_CanMapToPoint_P256Impl(rgdPriv, f_pBigCtx) );

        ++dwNumTries;
    } /* end do/while */
    while  ( DRM_SUCCESS != dr );

    /*
    ** Compute the Public key ( privKey * G )
    */
    ChkBOOL( ecaffine_exponentiation( g_Elwrve.generator,
                                      rgdPriv,
                                      ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                      rgptPub,
                                     &g_Elwrve,
                                      f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Just to be safe lets make sure the pubkey is on the lwrve
    */
    ChkBOOL( ecaffine_on_lwrve( rgptPub,
                               &g_Elwrve,
                                NULL,
                                NULL,
                                f_pBigCtx ),
             DRM_E_P256_PKCRYPTO_FAILURE );

    /*
    ** Now Colwert the priv and pub keys into big-endian DRM-BYTES
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdPriv, f_pPrivKey->m_rgbPrivkey ) );
    ChkDR( Colwert_P256_PointToBigEndianBytes( rgptPub,
                                              &g_Elwrve,
                                               f_pPubKey->m_rgbPubkey ) );
ErrorExit:
    return dr;
} /* end OEM_ECC_GenKeyPairEncryptablePriv_P256 */
#endif

/*************************************************************************************************
**
**  Function: OEM_ECDSA_SignHash_P256Impl
**
**  Synopsis: Creates an ECDSA P256 signature for a hash of a message, given a private keys
**
**  Algorithm: x = SHA256 hash, P = public key, G = generator pt,
**             ( s, t ) = signature, q = field order and the ECDSA modulus
**             w = ilwerse( t ) MOD q
**             i = w * x MOD q
**             j = w * s MOD q
**             ( u, v ) = iG + jP
**             Verify( x, ( s, t ) ) := u MOQ q == s
**
**  Arguments:
**   IN   [f_rgbHash]:       Hash to create signature on
**   IN   [f_cbHash]:        Hash length in bytes
**   IN   [f_pPrivkey]:      Pointer to ECC Priv Key struct containing the privkey
**   OUT  [f_pSignature ]:   Pointer to the signature for the message
**   INOUT[f_pBigCtx] :      Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**   IN   [f_cbBigCtx]:      size of allocated f_pBigCtx
**
**  Notes: This function deals with the private key thus it must be protected/obfuscated/encrypted
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_SignHash_P256Impl(
    __in_ecount( f_cbHash )       const  DRM_BYTE        f_rgbHash[],
    __in                          const  DRM_DWORD       f_cbHash,
    __in                          const  PRIVKEY_P256   *f_pPrivkey,
    __out                                SIGNATURE_P256 *f_pSignature,
    __inout                       struct bigctx_t       *f_pBigCtx,
    __in                                 DRM_DWORD       f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT  dr            = DRM_SUCCESS;
    DRM_DWORD   dwNumTry      = 0;
    DRM_DWORD   cdGCDLen      = 0;
    digit_t     rgptRandPoint[ ECC_P256_POINT_SIZE_IN_ECAFFINES ];
    digit_t     rgdRandNum[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdRandNumIlwerse[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdSigElement[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdIntermediateVal1[ 2 * ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdIntermediateVal2[ 2 * ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdMessageHash[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];

    /*
    ** Arg Checks
    */
    ChkArg( NULL !=  f_rgbHash );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_BYTES == f_cbHash );
    ChkArg( NULL !=  f_pPrivkey );
    ChkArg( NULL !=  f_pSignature );
    ChkArg( NULL !=  f_pBigCtx );

    /*
    ** Re-Init big num context if it was not inialized or erased.
    */
    ChkDR( OEM_ECC_InitializeBignumStackImpl( f_pBigCtx, f_cbBigCtx ) );

    ChkDR( Colwert_P256_BigEndianBytesToDigitsModOrderImpl( f_rgbHash,
                                                           &g_Elwrve,
                                                            rgdMessageHash ) );
    /*
    ** Attempt the rest of the algorithm, if something is wrong, like we hit the pt at infinity,
    ** if something goes wrong try again, until it succeeds or our loop ctr expires
    */
    for( dwNumTry = 0; dwNumTry < 1000; ++dwNumTry )
    {
        /*
        ** Generate r, the random number
        */
        ChkBOOL( random_mod_nonzero( g_Elwrve.gorder,
                                     rgdRandNum,
                                     ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                     f_pBigCtx ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );

        /*
        ** Callwlate r*G, where G is the generator point
        */
#if DRM_SUPPORT_PRECOMPUTE_GTABLE


        ChkBOOL( ecaffine_exponentiation_tabular( ECC_P256_GENERATOR_MULTPLE_TABLE,
                                                  ECC_P256_TABLE_SPACING,
                                                  ECC_P256_TABLE_LAST,
                                                  rgdRandNum,
                                                  ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                                  rgptRandPoint,
                                                 &g_Elwrve,
                                                  f_pBigCtx ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );
#else /* DRM_SUPPORT_PRECOMPUTE_GTABLE */
        ChkBOOL( ecaffine_exponentiation( g_Elwrve.generator,
                                          rgdRandNum,
                                          ECC_P256_INTEGER_SIZE_IN_DIGITS,
                                          rgptRandPoint,
                                         &g_Elwrve,
                                          f_pBigCtx ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );
#endif /* DRM_SUPPORT_PRECOMPUTE_GTABLE */

        /*
        ** where r*G = (u, v) grab u, then callwlate s = u MOD q
        */
        ChkDR( Colwert_P256_ModularIntToDigitsModOrderImpl( rgptRandPoint,
                                                       &g_Elwrve,
                                                        rgdSigElement ) );

        /*
        ** Chk to make sure that our signature element is not zero, if it is try again!
        ** If != 0, top the first part of the sig. out to the output buffer in big endian bytes
        */
        if ( significant_digit_count( rgdSigElement,  ECC_P256_INTEGER_SIZE_IN_DIGITS ) == 0 )
        {
            continue;
        } /* end if */
        ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdSigElement, f_pSignature->m_rgbSignature ) );

        /*
        ** Callwlate ilwerse( r )
        */
        {
            ChkBOOL( mp_gcdex( rgdRandNum,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           g_Elwrve.gorder,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           rgdRandNumIlwerse,
                           NULL,
                           rgdIntermediateVal1,
                           NULL,
                          &cdGCDLen,
                           NULL,
                           f_pBigCtx ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );
        }
        /*
        ** Make sure that the rand num and the modulus are relative primes, (GDC must == 1 too!)
        ** if not then there can be no ilwerse, try the whole process again!
        */
        if (   1 != cdGCDLen
            || 1 != rgdIntermediateVal1[ 0 ] )
        {
            continue;
        } /* end if */

        /*
        ** Colwert the priv key into digit format
        */
        ChkDR( Colwert_P256_BigEndianBytesToDigitsImpl( f_pPrivkey->m_rgbPrivkey, rgdRandNum ) );

        /*
        ** Callwlate k*s and zero the priv key afterwards regardless of success or failure
        ** Note: I am reusing the randnum buffer b/c it does not need to be use anymore
        */
        ChkBOOL( multiply( rgdSigElement,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           rgdRandNum,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           rgdIntermediateVal1 ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );

        OEM_SELWRE_ZERO_MEMORY( rgdRandNum, sizeof( rgdRandNum ) );

        /*
        ** calc: k*s mod q,
        */
        ChkBOOL( divide( rgdIntermediateVal1,
                          2 * ECC_P256_INTEGER_SIZE_IN_DIGITS,
                          g_Elwrve.gorder,
                          ECC_P256_INTEGER_SIZE_IN_DIGITS,
                          NULL,
                          NULL,
                          rgdIntermediateVal2 ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );

        /*
        ** Callwlate x + (k*s MOD q) MOD q, where x is the message hash
        */
        ChkBOOL( add_mod( rgdIntermediateVal2,
                           rgdMessageHash,
                           rgdRandNum,
                           g_Elwrve.gorder,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );

        /*
        ** Callwlate (x + (k*s MOD q) MOD q)/r mod q
        */
        ChkBOOL( multiply( rgdRandNumIlwerse,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           rgdRandNum,
                           ECC_P256_INTEGER_SIZE_IN_DIGITS,
                           rgdIntermediateVal1 ),
                DRM_E_P256_ECDSA_SIGNING_ERROR );
        ChkBOOL( divide( rgdIntermediateVal1,
                         2 * ECC_P256_INTEGER_SIZE_IN_DIGITS,
                         g_Elwrve.gorder,
                         ECC_P256_INTEGER_SIZE_IN_DIGITS,
                         NULL,
                         NULL,
                         rgdSigElement ),
                 DRM_E_P256_ECDSA_SIGNING_ERROR );

        /*
        ** Chk to make sure that our signature element is not zero, if it is try again!
        */
        if( significant_digit_count( rgdSigElement,  ECC_P256_INTEGER_SIZE_IN_DIGITS ) == 0 )
        {
            continue;
        } /* end if */

        /*
        ** We must have created a good signature at this point, so lets end the loop
        */
        break;
    }/* end for */

    /*
    ** Pop out the second signature element into the output buff in BE Bytes
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdSigElement,
                                               &f_pSignature->m_rgbSignature[ ECC_P256_INTEGER_SIZE_IN_BYTES ] ) );

ErrorExit:

    /*
    ** Be safe and zero some critical buffers
    */
    OEM_SELWRE_DIGITZEROMEMORY( rgdRandNum, DRM_NO_OF(rgdRandNum) );
    OEM_SELWRE_DIGITZEROMEMORY( rgdRandNumIlwerse, DRM_NO_OF(rgdRandNumIlwerse) );
    OEM_SELWRE_DIGITZEROMEMORY( rgdIntermediateVal1, DRM_NO_OF(rgdIntermediateVal1) );
    OEM_SELWRE_DIGITZEROMEMORY( rgdIntermediateVal2, DRM_NO_OF(rgdIntermediateVal2) );

    return dr;
} /* end OEM_ECDSA_SignHash_P256Impl */

#define ECDSA_RETRY_LOOP  1000
/*************************************************************************************************
**
**  Function: OEM_ECDSA_SignHash_P256Impl_LWIDIA
**
**  Synopsis: Creates an ECDSA P256 signature for a hash of a message, given a private keys.
**            The function uses HW accelerated crypto functions from Lwpu's Security Engine
**
**  Arguments:
**   IN   [f_rgbHash]:       Hash to create signature on
**   IN   [f_cbHash]:        Hash length in bytes
**   IN   [f_pPrivkey]:      Pointer to ECC Priv Key struct containing the privkey
**   OUT  [f_pSignature ]:   Pointer to the signature for the message
**   INOUT[f_pBigCtx] :      Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**   IN   [f_cbBigCtx]:      size of allocated f_pBigCtx
**
**  Notes: This function deals with the private key thus it must be protected/obfuscated/encrypted
**
*************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_SignHash_P256Impl_LWIDIA(
    __in_ecount( f_cbHash )       const  DRM_BYTE        f_rgbHash[],
    __in                          const  DRM_DWORD       f_cbHash,
    __in                          const  PRIVKEY_P256   *f_pPrivkey,
    __out                                SIGNATURE_P256 *f_pSignature,
    __inout                       struct bigctx_t       *f_pBigCtx,
    __in                                 DRM_DWORD       f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT  dr            = DRM_SUCCESS;
    digit_t     rgdSigElement0[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdSigElement1[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     rgdMessageHash[ ECC_P256_INTEGER_SIZE_IN_DIGITS ];
    digit_t     privKeyToDigits[ECC_P256_INTEGER_SIZE_IN_DIGITS];

    /*
    ** Arg Checks
    */
    ChkArg( NULL !=  f_rgbHash );
    ChkArg( ECC_P256_INTEGER_SIZE_IN_BYTES == f_cbHash );
    ChkArg( NULL !=  f_pPrivkey );
    ChkArg( NULL !=  f_pSignature );
    ChkArg( NULL !=  f_pBigCtx );

    /*
    ** Re-Init big num context if it was not inialized or erased.
    */
    ChkDR(OEM_ECC_InitializeBignumStackImpl(f_pBigCtx, f_cbBigCtx));

    /*
    ** Colwert msg hash to required form
    */
    ChkDR( Colwert_P256_BigEndianBytesToDigitsModOrderImpl( f_rgbHash,
                                                           &g_Elwrve,
                                                            rgdMessageHash ) );

    /*
    ** Colwert priv key to form needed for HW crypto functions
    */
    ChkDR(Colwert_P256_BigEndianBytesToDigitsImpl(f_pPrivkey->m_rgbPrivkey, privKeyToDigits));

#ifdef SELWRITY_ENGINE
    /*
    ** Call ECDSA HW function.  The HW function will check for errors with the the signature,
    ** like we hit the pt at infinity, and loop until it succeeds or the loop counter expires.
    ** The loop counter is passed in by the calling function.
    ** Compute the ECDSA Signature
    ** We will use the pre-Montgomery colwersion parameters directly, rather than declare a new lwrve.
    */
    if (seECDSASignHash((LwU32 *)g_Elwrve.fdesc->modulo->modulus, (LwU32 *)g_Elwrve.gorder,
                        (LwU32 *)&g_rgdConstA_P256_PreM[0],         (LwU32 *)&g_rgdConstB_P256_PreM[0],
                        (LwU32 *)&g_rgecConstG_P256_PreM[0],        (LwU32 *)&g_rgecConstG_P256_PreM[8],
                                  rgdMessageHash,                             privKeyToDigits,
                        (LwU32)  ECDSA_RETRY_LOOP,                            rgdSigElement0,
                                 rgdSigElement1,                              SE_ECC_KEY_SIZE_256) != SE_OK)
    {
        dr = DRM_E_P256_ECDSA_SIGNING_ERROR;
        goto ErrorExit;
    }
#endif

    /*
    ** Pop out the signature elements into the output buff in BE Bytes
    */
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdSigElement0, f_pSignature->m_rgbSignature) );
    ChkDR( Colwert_P256_DigitsToBigEndianBytesImpl( rgdSigElement1, &f_pSignature->m_rgbSignature[ ECC_P256_INTEGER_SIZE_IN_BYTES ] ) );

ErrorExit:

    return dr;
} /* end OEM_ECDSA_SignHash_P256Impl */
#endif

#ifdef NONE
/*************************************************************************************************
**
**  Function: OEM_ECDSA_Sign_P256Impl
**
**  Synopsis: Creates an ECDSA P256 signature for a message, given a private keys
**
**  Algorithm: x = message, P = public key, G = generator pt,
**             ( s, t ) = signature, q = field order and the ECDSA modulus
**             w = ilwerse( t ) MOD q
**             i = w * SHA256( x ) MOD q
**             j = w * s MOD q
**             ( u, v ) = iG + jP
**             Verify( x, ( s, t ) ) := u MOQ q == s
**
**  Arguments:
**   IN   [f_rgbMessage ]:  Message to check signature
**   IN   [f_cbMessageLen]: Message length in bytes
**   IN   [f_pPrivkey]:     Pointer to ECC Priv Key struct containing the privkey
**   OUT  [f_pSignature ]:  Pointer to the signature for the message
**   INOUT[f_pBigCtx] :     Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**   IN   [f_cbBigCtx]:     size of allocated f_pBigCtx
**
**  Notes: This function deals with the private key thus it must be protected/obfuscated/encrypted
**
*************************************************************************************************/
DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_Sign_P256Impl(
    __in_ecount( f_cbMessageLen ) const  DRM_BYTE        f_rgbMessage[],
    __in                          const  DRM_DWORD       f_cbMessageLen,
    __in                          const  PRIVKEY_P256   *f_pPrivkey,
    __out                                SIGNATURE_P256 *f_pSignature,
    __inout                       struct bigctx_t       *f_pBigCtx,
    __in                                 DRM_DWORD       f_cbBigCtx )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Decls
    */
    DRM_RESULT  dr            = DRM_SUCCESS;
    DRM_SHA256_Digest  shaDigest;
    DRM_SHA256_CONTEXT shaData;

    /*
    ** Arg Checks
    */

    ChkArg( NULL    !=  f_rgbMessage );
    ChkArg( 0       !=  f_cbMessageLen );

    /*
    ** Hash the message
    */
    ChkDR( DRM_SHA256_Init( &shaData ) );
    ChkDR( DRM_SHA256_Update( &shaData,
                               f_rgbMessage,
                               f_cbMessageLen ) );
    ChkDR( DRM_SHA256_Finalize( &shaData, &shaDigest ) );
    ChkDR( OEM_ECDSA_SignHash_P256Impl( shaDigest.m_rgbDigest,
                                        sizeof(shaDigest.m_rgbDigest),
                                        f_pPrivkey,
                                        f_pSignature,
                                        f_pBigCtx,
                                        f_cbBigCtx ) );

ErrorExit:
    return dr;
} /* end OEM_ECDSA_Sign_P256Impl */
#endif
EXIT_PK_NAMESPACE_CODE;

