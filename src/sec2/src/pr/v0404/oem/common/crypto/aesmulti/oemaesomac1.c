/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMAESMULTI_C
#include <drmtypes.h>
#include <drmmathsafe.h>
#include <oembyteorder.h>
#include <oem.h>
#include <oemaesmulti.h>
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
EXIT_PK_NAMESPACE_CODE;

