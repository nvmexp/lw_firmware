/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** drmaes.c
**
** Summary:
**  This file implements AES signing and encryption of variable length data.
**  It assumes the existence of routines to encrypt a block sized buffer
*/

#define DRM_BUILDING_OEMAESMULTI_C
#include <drmutilitieslite.h>
#include <drmcrt.h>
#include <drmtypes.h>
#include <drmmathsafe.h>
#include <oembyteorder.h>
#include <oem.h>
#include <oembroker.h>
#include <drmprofile.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*********************************************************************
**
**  Function:  _LShift
**
**  Synopsis:  Left-shift on a byte buffer (not done in place)
**
**  Arguments:
**     f_pbIn  :  Input buffer
**     f_pbOut :  Output (result) buffer
**     f_cb    :  Byte count to shift
**
**  Returns: DRM_SUCCESS
**              Success
**           DRM_E_ILWALIDARG
**              One of the pointer arguments was NULL
*********************************************************************/
#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT DRM_CALL _LShift(
    __in_bcount( f_cb )  const DRM_BYTE  *f_pbIn,
    __out_ecount( f_cb )       DRM_BYTE  *f_pbOut,
    __in                       DRM_DWORD  f_cb );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _LShift(
    __in_bcount( f_cb )  const DRM_BYTE  *f_pbIn,
    __out_ecount( f_cb )       DRM_BYTE  *f_pbOut,
    __in                       DRM_DWORD  f_cb )
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  i  = 0;
    DRM_BYTE   b  = 0;

    ChkArg( f_pbIn != NULL && f_pbOut != NULL );

    for( i = 0; i < f_cb; i++ )
    {
        b = (DRM_BYTE)( f_pbIn[ i ] << 1 );

        if( i + 1 < f_cb )
        {
            f_pbOut[ i ] = (DRM_BYTE)( b | ( f_pbIn[ i + 1 ] >> 7 ) );
        }
        else
        {
            f_pbOut[ i ] = b;
        }
    }

ErrorExit:
    return dr;
}
#endif
#ifdef NONE
/*********************************************************************************************
** Function:  _XOR
**
** Synopsis:  Byte-by-byte XOR of two buffers using two indices.
**
** Arguments: [f_pbLHS]  : Pointer to the left-hand side
**            [f_ibLHS]  : Index into the LHS to start XOR at
**            [f_pbRHS]  : Pointer to the right-hand side
**            [f_ibRHS]  : Index into the RHS to start XOR at
**            [f_cb]     : Byte count to XOR
**
** Returns:   DRM_VOID
**********************************************************************************************/
PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER_25057, "f_pbLHS length defined by f_ibLHS and f_cb" )
static DRM_ALWAYS_INLINE DRM_VOID _XOR(
    __inout_bcount( f_ibLHS + f_cb )    DRM_BYTE   *f_pbLHS,
    __in                                DRM_DWORD   f_ibLHS,
    __in_bcount( f_ibRHS + f_cb ) const DRM_BYTE   *f_pbRHS,
    __in                                DRM_DWORD   f_ibRHS,
    __in                                DRM_DWORD   f_cb )
PREFAST_POP
{
    DRM_DWORD i = 0;

    for(  ; i < f_cb; i++ )
    {
        f_pbLHS[ i + f_ibLHS ] = (DRM_BYTE)( f_pbLHS[ i + f_ibLHS ] ^ f_pbRHS[ i + f_ibRHS ] );
    }
}


/*********************************************************************************************
** Function:  Oem_Aes_CtrProcessData
**
** Synopsis:  Does AES Counter-Mode encryption or decryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to encrypt or decrypt buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The buffer to encrypt or decrypt ( in place )
**            [f_cbData]    : The number of bytes to encrypt or decrypt
**            [f_pCtrContext] : Contains the initialization vector and offset data. Will be updated
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**              One of the pointers was NULL, or the byte count is 0, or neither the
**              block ID or offset are 0 ( one must == 0 ).
**            DRM_E_CRYPTO_FAILED
**              The encrypt/decrypt operation failed
**********************************************************************************************/
DRM_API DRM_RESULT DRM_CALL Oem_Aes_CtrProcessData(
    __in_ecount( 1 )            const OEM_AES_KEY_CONTEXT          *f_pKey,
    __inout_bcount( f_cbData )        DRM_BYTE                     *f_pbData,
    __in                              DRM_DWORD                     f_cbData,
    __inout_ecount( 1 )               DRM_AES_COUNTER_MODE_CONTEXT *f_pCtrContext )
{
#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1
    CLAW_AUTO_RANDOM_CIPHER
#endif /* DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_WP8_1 */
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_UINT64 qwIVCorrectEndianness;
    DRM_UINT64 rllDataOut[ DRM_AES_BLOCKLEN /sizeof(DRM_UINT64) ];
    DRM_DWORD  cbDataLeft   = f_cbData;
    DRM_DWORD  ibDataOutLwr = 0;
    DRM_DWORD  cbDataToUse  = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_CtrProcessData );

    ChkArg( f_pbData      != NULL );
    ChkArg( f_cbData      >  0 );
    ChkArg( f_pCtrContext != NULL );
    ChkArg( f_pKey        != NULL );
    ChkArg( f_pCtrContext->bByteOffset <= DRM_AES_BLOCKLEN );

    qwIVCorrectEndianness = f_pCtrContext->qwInitializatiolwector;
    ChkVOID( FORMAT_QWORD_AS_BIG_ENDIAN_BYTES_INPLACE( qwIVCorrectEndianness ) );

    if( f_pCtrContext->bByteOffset > 0 )
    {
        /*
        ** The data is in the middle of a block.  Handle the special case first
        */
        cbDataToUse = (DRM_DWORD)( DRM_AES_BLOCKLEN - f_pCtrContext->bByteOffset );
        cbDataToUse = min( cbDataLeft, cbDataToUse );

        rllDataOut[1] = f_pCtrContext->qwBlockOffset;
        ChkVOID( FORMAT_QWORD_AS_BIG_ENDIAN_BYTES_INPLACE( rllDataOut[ 1 ] ) );
        rllDataOut[ 0 ] = qwIVCorrectEndianness;

        dr = Oem_Broker_Aes_EncryptOneBlock( f_pKey, (DRM_BYTE*) rllDataOut );
        ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );

        _XOR( f_pbData, ibDataOutLwr, (DRM_BYTE*)rllDataOut, f_pCtrContext->bByteOffset, cbDataToUse );

        ibDataOutLwr += cbDataToUse;

        ChkDR( DRM_DWordSub( cbDataLeft, cbDataToUse, &cbDataLeft ) );

        /*
        ** If we used all of the bytes in the current block, then the block offset needs to be increased by one.
        */
        if( f_pCtrContext->bByteOffset + cbDataToUse == DRM_AES_BLOCKLEN )
        {
            /*
            ** Overflow is expected/required for CTR.  The block offset is used as the counter
            ** value, which is allowed to reset to zero.
            */
            f_pCtrContext->qwBlockOffset = DRM_UI64Add( f_pCtrContext->qwBlockOffset, DRM_UI64( 1 ) );
        }
    }

    while( cbDataLeft >= DRM_AES_BLOCKLEN )
    {
        rllDataOut[ 1 ] = f_pCtrContext->qwBlockOffset;
        ChkVOID( FORMAT_QWORD_AS_BIG_ENDIAN_BYTES_INPLACE( rllDataOut[ 1 ] ) );
        rllDataOut[ 0 ] = qwIVCorrectEndianness;

        dr = Oem_Broker_Aes_EncryptOneBlock( f_pKey, (DRM_BYTE*)rllDataOut );
        ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_WRITE_OVERRUN_6386, "Writing to f_pbData cant overrun because ibDataOutLwr + DRM_AES_BLOCKLEN is ensured to be less than f_cbData" )
        _XOR( f_pbData, ibDataOutLwr, (DRM_BYTE*)rllDataOut, 0, DRM_AES_BLOCKLEN );
PREFAST_POP /* __WARNING_WRITE_OVERRUN_6386 */

        ibDataOutLwr += DRM_AES_BLOCKLEN;
        cbDataLeft   -= DRM_AES_BLOCKLEN;

        /*
        ** Overflow is expected/required for CTR.  The block offset is used as the counter
        ** value, which is allowed to reset to zero.
        */
        f_pCtrContext->qwBlockOffset = DRM_UI64Add( f_pCtrContext->qwBlockOffset, DRM_UI64( 1 ) );
    }

    if( cbDataLeft > 0 ) /* at this point it is strictly less than DRM_AES_BLOCKLEN */
    {
        cbDataToUse = cbDataLeft;

        rllDataOut[ 1 ] = f_pCtrContext->qwBlockOffset;
        ChkVOID( FORMAT_QWORD_AS_BIG_ENDIAN_BYTES_INPLACE( rllDataOut[ 1 ] ) );
        rllDataOut[ 0 ] = qwIVCorrectEndianness;

        dr = Oem_Broker_Aes_EncryptOneBlock( f_pKey, (DRM_BYTE*)rllDataOut );
        ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );

        _XOR( f_pbData, ibDataOutLwr, (DRM_BYTE*)rllDataOut, 0, cbDataToUse );
    }

    f_pCtrContext->bByteOffset = ( f_pCtrContext->bByteOffset + f_cbData ) % DRM_AES_BLOCKLEN;
ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/*********************************************************************************************
** Function:  Oem_Aes_CbcEncryptData
**
** Synopsis:  Does AES CBC-Mode encryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to encrypt the buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The buffer to encrypt ( in place )
**            [f_cbData]    : The number of bytes to encrypt
**            [f_rgbIV]     : The initialization vector to use for encryption
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**            DRM_E_CRYPTO_FAILED
**              The encrypt operation failed
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_CbcEncryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN ) const DRM_BYTE               f_rgbIV[ DRM_AES_BLOCKLEN ] );
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_CbcEncryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN ) const DRM_BYTE               f_rgbIV[ DRM_AES_BLOCKLEN ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr           =  DRM_SUCCESS;
    DRM_DWORD  cbDataLeft   = f_cbData;
    DRM_DWORD  ibDataOutLwr = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_CbcEncryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData >= DRM_AES_BLOCKLEN );
    ChkArg( f_cbData % DRM_AES_BLOCKLEN ==  0 );

    /*
    ** The first block is a special case: Use the IV to XOR
    */
    _XOR( f_pbData, 0, f_rgbIV, 0, DRM_AES_BLOCKLEN );
    dr = Oem_Broker_Aes_EncryptOneBlock( f_pKey, f_pbData );
    ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );
    cbDataLeft -= DRM_AES_BLOCKLEN;

    while( cbDataLeft > 0 )
    {
        __analysis_assume( ibDataOutLwr + DRM_AES_BLOCKLEN + DRM_AES_BLOCKLEN < f_cbData );
        _XOR( f_pbData, ibDataOutLwr + DRM_AES_BLOCKLEN, f_pbData, ibDataOutLwr, DRM_AES_BLOCKLEN );
        ibDataOutLwr += DRM_AES_BLOCKLEN;
        dr = Oem_Broker_Aes_EncryptOneBlock( f_pKey, &f_pbData[ibDataOutLwr] );
        ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );
        cbDataLeft -= DRM_AES_BLOCKLEN;
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/*********************************************************************************************
** Function:  Oem_Aes_CbcDecryptData
**
** Synopsis:  Does AES CBC-Mode decryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to decrypt the buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The buffer to decrypt ( in place )
**            [f_cbData]    : The number of bytes to decrypt
**            [f_rgbIV]     : The initialization vector to use for decryption
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**            DRM_E_CRYPTO_FAILED
**              The decrypt operation failed
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_CbcDecryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN ) const DRM_BYTE               f_rgbIV[ DRM_AES_BLOCKLEN ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr           =  DRM_SUCCESS;
    DRM_DWORD  ibDataOutLwr = 0;
    DRM_DWORD  i            = 0;
    DRM_BYTE   rgbTemp1[ DRM_AES_BLOCKLEN ];
    DRM_BYTE   rgbTemp2[ DRM_AES_BLOCKLEN ];

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_CbcDecryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData >= DRM_AES_BLOCKLEN );
    ChkArg( f_cbData % DRM_AES_BLOCKLEN ==  0 );

    /*
    ** The first block is a special case: Use the IV to XOR
    */
    OEM_SELWRE_MEMCPY( rgbTemp1, f_pbData, DRM_AES_BLOCKLEN );
    dr = Oem_Broker_Aes_DecryptOneBlock( f_pKey, f_pbData );
    _XOR( f_pbData, 0, f_rgbIV, 0, DRM_AES_BLOCKLEN );
    ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );
    ibDataOutLwr += DRM_AES_BLOCKLEN;

    for( ; ibDataOutLwr < f_cbData; i++, ibDataOutLwr += DRM_AES_BLOCKLEN )
    {
        OEM_SELWRE_MEMCPY( ( i % 2 == 0? rgbTemp2 : rgbTemp1 ), f_pbData + ibDataOutLwr, DRM_AES_BLOCKLEN );
        dr = Oem_Broker_Aes_DecryptOneBlock( f_pKey, &f_pbData[ibDataOutLwr] );
        ChkBOOL( DRM_SUCCEEDED( dr ), DRM_E_CRYPTO_FAILED );

        __analysis_assume( ibDataOutLwr + DRM_AES_BLOCKLEN < f_cbData );
        _XOR( f_pbData, ibDataOutLwr, ( i % 2 == 0? rgbTemp1 : rgbTemp2 ), 0, DRM_AES_BLOCKLEN );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

/*********************************************************************************************
**
** Function:  Oem_Aes_EcbEncryptData
**
** Synopsis:  Does AES ECB-Mode encryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to encrypt  buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The buffer to encrypt ( in place )
**            [f_cbData]    : The number of bytes to encrypt
**
** Returns:   DRM_SUCCESS
**               Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**********************************************************************************************/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbEncryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData );
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbEncryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  ibData = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_EcbEncryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData != 0 );
    ChkArg( f_pKey   != NULL );

    ChkArg( f_cbData % DRM_AES_BLOCKLEN == 0 );

    for ( ; ibData < f_cbData; ibData += DRM_AES_BLOCKLEN )
    {
        ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, &( f_pbData[ibData] ) ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

/*********************************************************************************************
**
** Function:  Oem_Aes_EcbDecryptData
**
** Synopsis:  Does AES ECB-Mode decryption on a buffer of data
**
** Arguments: [f_pKey]      : The AES secret key used to decrypt buffer
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The buffer to decrypt ( in place )
**            [f_cbData]    : The number of bytes to decrypt
**
** Returns:   DRM_SUCCESS
**               Success
**            DRM_E_ILWALIDARG
**               One of the pointers was NULL, or the byte count is 0
**               or not a multiple of DRM_AES_BLOCKLEN
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbDecryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData );
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EcbDecryptData(
    __in_ecount( 1 )                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __inout_bcount( f_cbData )            DRM_BYTE              *f_pbData,
    __in                                  DRM_DWORD              f_cbData )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr     = DRM_SUCCESS;
    DRM_DWORD  ibData = 0;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Aes_EcbDecryptData );

    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData != 0 );
    ChkArg( f_pKey   != NULL );

    ChkArg( f_cbData % DRM_AES_BLOCKLEN == 0 );

    for ( ; ibData < f_cbData; ibData += DRM_AES_BLOCKLEN )
    {
        ChkDR( Oem_Broker_Aes_DecryptOneBlock( f_pKey, &( f_pbData[ibData] ) ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif
/*********************************************************************************************
**
** Function:  _Omac1_GenerateSignTag
**
** Synopsis:  Computes the OMAC1 sign tag
**
** Arguments: [f_pKey]      : The AES secret key
**            [f_pbData]    : The data to sign
**            [f_cbData]    : The number of bytes to sign
**            [f_rgbLU]     : Computed by _Omac1_GenerateSignInfo
**            [f_rgbLU_1]   : Computed by _Omac1_GenerateSignInfo
**            [f_rgbTag]    : The generated OMAC1 tag
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**              One of the pointers was NULL, or the byte count is 0
**********************************************************************************************/
#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT DRM_CALL _Omac1_GenerateSignTag(
    __in_ecount( 1 )                 const OEM_AES_KEY_CONTEXT  *f_pKey,
    __in_bcount( f_ibData+f_cbData ) const DRM_BYTE             *f_pbData,
    __in                                   DRM_DWORD             f_ibData,
    __in                                   DRM_DWORD             f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )  const DRM_BYTE              f_rgbLU  [ DRM_AES_BLOCKLEN ],
    __in_bcount( DRM_AES_BLOCKLEN )  const DRM_BYTE              f_rgbLU_1[ DRM_AES_BLOCKLEN ],
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbTag [ DRM_AES_BLOCKLEN ] );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _Omac1_GenerateSignTag(
    __in_ecount( 1 )                 const OEM_AES_KEY_CONTEXT  *f_pKey,
    __in_bcount( f_ibData+f_cbData ) const DRM_BYTE             *f_pbData,
    __in                                   DRM_DWORD             f_ibData,
    __in                                   DRM_DWORD             f_cbData,
    __in_bcount( DRM_AES_BLOCKLEN )  const DRM_BYTE              f_rgbLU  [ DRM_AES_BLOCKLEN ],
    __in_bcount( DRM_AES_BLOCKLEN )  const DRM_BYTE              f_rgbLU_1[ DRM_AES_BLOCKLEN ],
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbTag [ DRM_AES_BLOCKLEN ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr           = DRM_SUCCESS;
    DRM_DWORD  ibDataInLwr  = 0;
    DRM_BYTE   rgbDataBlock    [ DRM_AES_BLOCKLEN ] = { 0 };
    DRM_BYTE   rgbLastDataBlock[ DRM_AES_BLOCKLEN ] = { 0 };

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Omac1_GenerateSignTag );

    ChkArg( f_pKey   != NULL );
    ChkArg( f_pbData != NULL );
    ChkArg( f_cbData  > 0 );

    do
    {
        if( f_cbData > DRM_AES_BLOCKLEN )
        {
            OEM_SELWRE_MEMCPY_IDX(rgbDataBlock, 0, f_pbData, f_ibData + ibDataInLwr, DRM_AES_BLOCKLEN);
            DRM_XOR( rgbDataBlock, rgbLastDataBlock, DRM_AES_BLOCKLEN );
            OEM_SELWRE_MEMCPY( rgbLastDataBlock, rgbDataBlock, DRM_AES_BLOCKLEN );

            ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, rgbLastDataBlock ) );

            f_cbData -= DRM_AES_BLOCKLEN;
            ibDataInLwr += DRM_AES_BLOCKLEN;
        }
        else
        {
            if( f_cbData == DRM_AES_BLOCKLEN )
            {
                OEM_SELWRE_MEMCPY_IDX(rgbDataBlock, 0, f_pbData, f_ibData + ibDataInLwr, DRM_AES_BLOCKLEN);
                DRM_XOR( rgbDataBlock, rgbLastDataBlock, DRM_AES_BLOCKLEN );
                DRM_XOR( rgbDataBlock, f_rgbLU, DRM_AES_BLOCKLEN );
            }
            else
            {
                OEM_SELWRE_ZERO_MEMORY( rgbDataBlock, DRM_AES_BLOCKLEN );

                OEM_SELWRE_MEMCPY_IDX(rgbDataBlock, 0, f_pbData, f_ibData + ibDataInLwr, f_cbData);
                rgbDataBlock[ f_cbData ] = 0x80;

                DRM_XOR( rgbDataBlock, rgbLastDataBlock, DRM_AES_BLOCKLEN );
                DRM_XOR( rgbDataBlock, f_rgbLU_1, DRM_AES_BLOCKLEN );
            }

            ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, rgbDataBlock ) );

            f_cbData = 0;
        }

    } while( f_cbData > 0 );

    OEM_SELWRE_MEMCPY( f_rgbTag, rgbDataBlock, DRM_AES_BLOCKLEN );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

#endif

/*********************************************************************************************
**
** Function:  _Omac1_GenerateSignInfo
**
** Synopsis:  Computes the OMAC1 sign info
**
** Arguments: [f_pKey]      : The AES secret key
**            [f_rgbLU]     : Computed L.u from the OMAC algorithm
**            [f_rgbLU_1]   : Computed L.u2 from the OMAC algorithm
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**              The key pointer was NULL
**********************************************************************************************/
#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT DRM_CALL _Omac1_GenerateSignInfo(
    __in_ecount( 1 )                 const OEM_AES_KEY_CONTEXT  *f_pKey,
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbLU  [ DRM_AES_BLOCKLEN ],
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbLU_1[ DRM_AES_BLOCKLEN ] );
static DRM_NO_INLINE DRM_RESULT DRM_CALL _Omac1_GenerateSignInfo(
    __in_ecount( 1 )                 const OEM_AES_KEY_CONTEXT  *f_pKey,
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbLU  [ DRM_AES_BLOCKLEN ],
    __out_bcount( DRM_AES_BLOCKLEN )       DRM_BYTE              f_rgbLU_1[ DRM_AES_BLOCKLEN ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr                                       = DRM_SUCCESS;
    DRM_BYTE   rgbBuffer[ DRM_AES_BLOCKLEN ]            = { 0 };
    const DRM_BYTE bLU_ComputationConstant              = 0x87;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Omac1_GenerateSignInfo );

    ChkArg( f_pKey != NULL );

    ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, rgbBuffer ) );

    /*
    ** Compute L.u from the OMAC algorithm
    */
    ChkDR( _LShift( rgbBuffer, f_rgbLU, DRM_AES_BLOCKLEN ) );

    if( rgbBuffer[ 0 ] & 0x80 )
    {
        f_rgbLU[ DRM_AES_BLOCKLEN - 1 ] = (DRM_BYTE)( f_rgbLU[ DRM_AES_BLOCKLEN - 1 ] ^ bLU_ComputationConstant );
    }

    /*
    ** Compute L.u2 from the OMAC algorithm ( OMAC1 veriant )
    */
    ChkDR( _LShift( f_rgbLU, f_rgbLU_1, DRM_AES_BLOCKLEN ) );

    if( f_rgbLU[ 0 ] & 0x80 )
    {
        f_rgbLU_1[ DRM_AES_BLOCKLEN - 1 ] = (DRM_BYTE)( f_rgbLU_1[ DRM_AES_BLOCKLEN - 1 ] ^ bLU_ComputationConstant );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

/*********************************************************************************************
**
** Function:  Oem_Omac1_Sign
**
** Synopsis:  Generates a signature using an OMAC of the data and an AES key
**
** Arguments: [f_pKey]      : The AES secret key
**                            - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                              be pointing to an OEM_TEE_KEY_AES.
**                              OEM can colwert it to a DRM_AES_KEY using 
**                              OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                            - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                              will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                              will be sufficient.
**            [f_pbData]    : The data to sign
**            [f_ibData]    : The starting byte offset of the data to sign
**            [f_cbData]    : The number of bytes to sign
**            [f_rgbTag]    : The generated OMAC1 tag
**
** Returns:   DRM_SUCCESS
**              Success
**            DRM_E_ILWALIDARG
**              The plain text is too long or one of the pointers is NULL
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Omac1_Sign(
    __in_ecount( 1 )                       const OEM_AES_KEY_CONTEXT    *f_pKey,
    __in_bcount( f_ibData + f_cbData )     const DRM_BYTE               *f_pbData,
    __in                                         DRM_DWORD               f_ibData,
    __in                                         DRM_DWORD               f_cbData,
    __out_bcount( DRM_AES_BLOCKLEN )             DRM_BYTE                f_rgbTag[ DRM_AES_BLOCKLEN ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbLU  [ DRM_AES_BLOCKLEN ] = { 0 };
    DRM_BYTE   rgbLU_1[ DRM_AES_BLOCKLEN ] = { 0 };

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Omac1_Sign );

    ChkArg( f_pbData != NULL && f_cbData > 0 );
    ChkArg( f_rgbTag != NULL );
    ChkArg( f_pKey   != NULL );

    ChkDR( _Omac1_GenerateSignInfo( f_pKey,
                                    rgbLU,
                                    rgbLU_1 ) );

    ChkDR( _Omac1_GenerateSignTag( f_pKey,
                                   f_pbData,
                                   f_ibData,
                                   f_cbData,
                                   rgbLU,
                                   rgbLU_1,
                                   f_rgbTag ) );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

/*********************************************************************************************
**
** Function:  Oem_Omac1_Verify
**
** Synopsis:  Verifies a signature using an OMAC of the data and an AES key
**
** Arguments: [f_pKey]           : The AES secret key.
**                                  - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                                    be pointing to an OEM_TEE_KEY_AES.
**                                    OEM can colwert it to a DRM_AES_KEY using 
**                                    OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                                  - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                                    will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                                    will be sufficient.
**            [f_pbData]         : The signed data.
**            [f_ibData]         : The starting byte offset of the data to verify.
**            [f_cbData]         : The number of bytes signed.
**            [f_pbSignature]    : Pointer to the buffer holding the signature.
**            [f_ibSignature]    : The starting byte offset of the signature to verify.
**
** Returns:   DRM_SUCCESS
**                The specified signature matches the computed signature
**            DRM_E_ILWALID_SIGNATURE
**                The specified signature does not match the computed signature
**            DRM_E_ILWALIDARG
**              One of the pointers is NULL
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Omac1_Verify(
    __in_ecount( 1 )                                const OEM_AES_KEY_CONTEXT   *f_pKey,
    __in_bcount( f_ibData + f_cbData )              const DRM_BYTE              *f_pbData,
    __in                                                  DRM_DWORD              f_ibData,
    __in                                                  DRM_DWORD              f_cbData,
    __in_bcount( f_ibSignature + DRM_AES_BLOCKLEN ) const DRM_BYTE              *f_pbSignature,
    __in                                                  DRM_DWORD              f_ibSignature )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_BYTE   rgbComputedSignature[ DRM_AES_BLOCKLEN ] = { 0 };

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMAES, PERF_FUNC_Oem_Omac1_Verify );

    ChkArg( f_pKey   != NULL );
    ChkArg( f_pbData != NULL && f_cbData > 0 );

    /*
    ** Compute the signature
    */
    ChkDR( Oem_Omac1_Sign( f_pKey, f_pbData, f_ibData, f_cbData, rgbComputedSignature ) );

    /*
    ** Compare the computed signature to the passed in signature
    */
    ChkBOOL( OEM_SELWRE_ARE_EQUAL( rgbComputedSignature, f_pbSignature + f_ibSignature, DRM_AES_BLOCKLEN ), DRM_E_ILWALID_SIGNATURE );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

/*********************************************************************************************
**
** Function:  Oem_Aes_AES128KDFCTR_r8_L128
**
** Synopsis:  Implements AES-128 CTR Key Derivation Function as described in
** "Recommendation for Key Derivation Using Pseudorandom Functions", NIST Special Publication 800-108,
** http://csrc.nist.gov/publications/nistpubs/800-108/sp800-108.pdf
**
** We only derive AES128 keys which are 128 bits long ((i.e. 'L'==128)
** Our psueo-random function (PRF) is Oem_Omac1_Sign that has output size of 128 bits (i.e. 'h'==128),
** So we always perform only one iteration since 'L' == 'h' and therefore Ceil(L/h) == 'n' == 1.
**
** We choose to use 8 bits (one byte) for the size of the binary representation of 'i'. (r == 8)
**
** Our 'Label' and 'Context' are input GUID's specified by the user.
** If 'Context' is not specified, a value of all zeroes will be used.
** L == 128 for output of AES-128 keys and is word size.
**
** Arguments: [f_pDeriverKey]       : AES key used for derivation
**                                   - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will 
**                                     be pointing to an OEM_TEE_KEY_AES.
**                                     OEM can colwert it to a DRM_AES_KEY using 
**                                     OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                                   - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                                     will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY* 
**                                     will be sufficient.
**            [f_pidLabel]          : Label value of 16 bytes
**            [f_pidContext]        : Context value of 16 bytes
**            [f_pbDerivedKey]      : Derived 16 bytes
**
** Returns:   DRM_SUCCESS
**                The specified signature matches the computed signature
**            DRM_E_ILWALIDARG
**              One of the pointers is NULL
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_AES128KDFCTR_r8_L128(
    __in_ecount( 1 )                                const OEM_AES_KEY_CONTEXT   *f_pDeriverKey,
    __in_ecount( 1 )                                const DRM_ID                *f_pidLabel,
    __in_ecount_opt( 1 )                            const DRM_ID                *f_pidContext,
    __out_ecount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE              *f_pbDerivedKey )
{
    DRM_RESULT     dr                    = DRM_SUCCESS;
    const DRM_BYTE cIteration            = 1;
    const DRM_BYTE cSeparator            = 0;
    const DRM_WORD cLenInBitsReturlwalue = 128;
    DRM_ID         idZeroes              = {{0}};
    DRM_BYTE       rgbKeyDerivationInput[
        sizeof(cIteration)
      + sizeof(*f_pidLabel)
      + sizeof(cSeparator)
      + sizeof(*f_pidContext)
      + sizeof(cLenInBitsReturlwalue)] = {0};

    ChkArg( f_pDeriverKey  != NULL );
    ChkArg( f_pidLabel     != NULL );
    ChkArg( f_pbDerivedKey != NULL );

    /* Populate the input data iteration */
    rgbKeyDerivationInput[0] = cIteration;

    /* Populate the input data label */
    OEM_SELWRE_MEMCPY(
        &rgbKeyDerivationInput[sizeof(cIteration)],
        f_pidLabel,
        sizeof(*f_pidLabel) );

    /* Populate the input data separator */
    rgbKeyDerivationInput[sizeof(cIteration) + sizeof(*f_pidLabel)] = cSeparator;

    /* Populate the input data context */
    OEM_SELWRE_MEMCPY(
        &rgbKeyDerivationInput[sizeof(cIteration) + sizeof(*f_pidLabel) + sizeof(cSeparator)],
        f_pidContext != NULL ? f_pidContext : &idZeroes,
        sizeof(*f_pidContext) );

    /* Populate the input data derived length */
    WORD_TO_NETWORKBYTES(
        rgbKeyDerivationInput,
        sizeof(cIteration) + sizeof(*f_pidLabel) + sizeof(cSeparator) + sizeof(*f_pidContext),
        cLenInBitsReturlwalue );

    /* Generate the derived key */
    ChkDR( Oem_Omac1_Sign(
        f_pDeriverKey,
        rgbKeyDerivationInput,
        0,
        sizeof(rgbKeyDerivationInput),
        f_pbDerivedKey ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
