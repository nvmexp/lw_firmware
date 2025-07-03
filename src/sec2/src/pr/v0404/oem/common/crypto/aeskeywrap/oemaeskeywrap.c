/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** oemaeskeywrap.c
**
** This file implements the AES Key Wrap algorithm to selwrely encrypt a plaintext key with
** any associated integrity information and data per the following NIST specification:
**
** http://csrc.nist.gov/groups/ST/toolkit/dolwments/kms/key-wrap.pdf
*/

#define DRM_BUILDING_DRMAESKEYWRAP_C

#include <oembyteorder.h>
#include <oemaes.h>
#include <oemeccp256.h>
#include <oemaeskeywrap.h>
#include <oemcommon.h>
#include <oembroker.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#define OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS_SHIFT 1
#define OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS       2
#define OEM_AESKEYWRAP_ROUNDS                   6

#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT DRM_CALL _AesKeyWrap_WrapKey128(
    __in                          const OEM_AES_KEY_CONTEXT *f_pKey,
    __in_ecount( f_cdwPlaintext ) const DRM_DWORD           *f_pdwPlaintext,
    __in                          const DRM_DWORD            f_cdwPlaintext,
    __out_ecount( f_cdwCiphertext )     DRM_DWORD           *f_pdwCiphertext,
    __in                                DRM_DWORD            f_cdwCiphertext )
GCC_ATTRIB_NO_STACK_PROTECT();
static DRM_NO_INLINE DRM_RESULT DRM_CALL _AesKeyWrap_WrapKey128(
    __in                          const OEM_AES_KEY_CONTEXT *f_pKey,
    __in_ecount( f_cdwPlaintext ) const DRM_DWORD           *f_pdwPlaintext,
    __in                          const DRM_DWORD            f_cdwPlaintext,
    __out_ecount( f_cdwCiphertext )     DRM_DWORD           *f_pdwCiphertext,
    __in                                DRM_DWORD            f_cdwCiphertext )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT  dr                                                        = DRM_SUCCESS;
    DRM_DWORD   iBlock                                                    = 0;
    DRM_DWORD   iRound                                                    = 0;
    DRM_DWORD   cBlocks                                                   = 0;
    DRM_DWORD   cIterations                                               = 0;
    DRM_DWORD  *pdwLwrrentBlock                                           = NULL;
    DRM_DWORD   rgdwWorkingBlock[ OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS * 2 ] = {0};

    DRMCASSERT( DRM_IS_INTEGER_POWER_OF_TW0( OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS ) );

    AssertChkArg( f_pKey           != NULL );
    AssertChkArg( f_pdwPlaintext   != NULL );
    AssertChkArg( f_pdwCiphertext  != NULL );
    AssertChkArg( f_cdwPlaintext > OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS );
    /* Power of two, so use & instead of % for better performance */
    AssertChkArg( ( f_cdwPlaintext & ( OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS - 1 ) ) == 0 );
    AssertChkArg( f_cdwCiphertext == f_cdwPlaintext + OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS );

    /* Power of two, so use >> instead of / for better performance */
    cBlocks = f_cdwPlaintext >> OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS_SHIFT;

    /* A = IV This constant comes from the NIST spec referenced above */
    rgdwWorkingBlock[0] = rgdwWorkingBlock[1] = 0xa6a6a6a6;

    /* Ri = Pi */
    OEM_SELWRE_MEMCPY( f_pdwCiphertext + OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS, f_pdwPlaintext, f_cdwPlaintext * sizeof(DRM_DWORD) );

    for( iRound = 0; iRound < OEM_AESKEYWRAP_ROUNDS; iRound++ )
    {
        pdwLwrrentBlock = f_pdwCiphertext;

        for( iBlock = 0; iBlock < cBlocks; iBlock++ )
        {
            /* Ri = Pi */
            pdwLwrrentBlock += OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS;

            /* B = AES-Encrypt(k,A|Ri) */
            rgdwWorkingBlock[2] = pdwLwrrentBlock[0];
            rgdwWorkingBlock[3] = pdwLwrrentBlock[1];
            ChkDR( Oem_Broker_Aes_EncryptOneBlock( f_pKey, (DRM_BYTE*)rgdwWorkingBlock ) );

            /* A = MSB64(B) ^ t where t = (n*j)+i*/
            cIterations++;
            rgdwWorkingBlock[1] ^= MAKE_DWORD_BIG_ENDIAN( cIterations );

            /* Ri = LSB64(B) */
            pdwLwrrentBlock[0] = rgdwWorkingBlock[2];
            pdwLwrrentBlock[1] = rgdwWorkingBlock[3];
        }
    }

    /* C0 = A */
    pdwLwrrentBlock = f_pdwCiphertext;
    pdwLwrrentBlock[0] = rgdwWorkingBlock[0];
    pdwLwrrentBlock[1] = rgdwWorkingBlock[1];

ErrorExit:
    if( DRM_FAILED( dr ) && f_pdwCiphertext != NULL && f_cdwCiphertext > 0 )
    {
        OEM_SELWRE_ZERO_MEMORY( f_pdwCiphertext, f_cdwCiphertext * sizeof(DRM_DWORD) );
    }

    return dr;
}

static DRM_NO_INLINE DRM_RESULT DRM_CALL _AesKeyWrap_UnwrapKey128(
    __in                           const OEM_AES_KEY_CONTEXT    *f_pKey,
    __in_ecount( f_cdwCiphertext ) const DRM_DWORD              *f_pdwCiphertext,
    __in                           const DRM_DWORD               f_cdwCiphertext,
    __out_ecount( f_cdwPlaintext )       DRM_DWORD              *f_pdwPlaintext,
    __in                                 DRM_DWORD               f_cdwPlaintext )
GCC_ATTRIB_NO_STACK_PROTECT();
static DRM_NO_INLINE DRM_RESULT DRM_CALL _AesKeyWrap_UnwrapKey128(
    __in                           const OEM_AES_KEY_CONTEXT    *f_pKey,
    __in_ecount( f_cdwCiphertext ) const DRM_DWORD              *f_pdwCiphertext,
    __in                           const DRM_DWORD               f_cdwCiphertext,
    __out_ecount( f_cdwPlaintext )       DRM_DWORD              *f_pdwPlaintext,
    __in                                 DRM_DWORD               f_cdwPlaintext )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT  dr                                                        = DRM_SUCCESS;
    DRM_DWORD   iBlock                                                    = 0;
    DRM_DWORD   iRound                                                    = 0;
    DRM_DWORD   cBlocks                                                   = 0;
    DRM_DWORD   cIterations                                               = 0;
    DRM_DWORD  *pdwLwrrentBlock                                           = NULL;
    DRM_DWORD   rgdwWorkingBlock[ OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS * 2 ] = {0};

    DRMCASSERT( DRM_IS_INTEGER_POWER_OF_TW0( OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS ) );

    AssertChkArg( f_pKey          != NULL );
    AssertChkArg( f_pdwCiphertext != NULL );
    AssertChkArg( f_pdwPlaintext  != NULL );
    AssertChkArg( f_cdwCiphertext > OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS * 2 );
    /* Power of two, so use & instead of % for better performance */
    AssertChkArg( ( f_cdwCiphertext & ( OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS - 1 ) ) == 0 );
    DRMCASSERT( OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS * sizeof(DRM_DWORD) * 2 == DRM_AES_BLOCKLEN );
    AssertChkArg( f_cdwPlaintext == f_cdwCiphertext - OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS );

    /* Power of two, so use >> instead of / for better performance */
    cBlocks = ( f_cdwCiphertext >> OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS_SHIFT ) - 1;
    cIterations = cBlocks * OEM_AESKEYWRAP_ROUNDS;

    /* A = C0 */
    rgdwWorkingBlock[0] = f_pdwCiphertext[0];
    rgdwWorkingBlock[1] = f_pdwCiphertext[1];

    /* Ri = Ci */
    OEM_SELWRE_MEMCPY( f_pdwPlaintext, f_pdwCiphertext + OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS, f_cdwPlaintext * sizeof(DRM_DWORD) );

    for( iRound = 0; iRound < OEM_AESKEYWRAP_ROUNDS; iRound++ )
    {
        pdwLwrrentBlock = f_pdwPlaintext + OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS * cBlocks;

        for( iBlock = 0; iBlock < cBlocks; iBlock++ )
        {
            /* Ri = Ci */
            pdwLwrrentBlock -= OEM_AESKEYWRAP_BLOCKLEN_IN_DWORDS;

            /* B = AES-Decrypt(k,(A^t)|Ri) where t = (n*j)+i */
            rgdwWorkingBlock[1] ^= MAKE_DWORD_BIG_ENDIAN( cIterations );
            rgdwWorkingBlock[2] = pdwLwrrentBlock[0];
            rgdwWorkingBlock[3] = pdwLwrrentBlock[1];
            cIterations--;
            ChkDR( Oem_Broker_Aes_DecryptOneBlock( f_pKey, (DRM_BYTE*)rgdwWorkingBlock ) );

            /* Ri = LSB64(B) */
            pdwLwrrentBlock[0] = rgdwWorkingBlock[2];
            pdwLwrrentBlock[1] = rgdwWorkingBlock[3];
        }
    }

    /* A == IV This constant comes from the NIST spec referenced above */
    if( rgdwWorkingBlock[0] != 0xa6a6a6a6 || rgdwWorkingBlock[0] != rgdwWorkingBlock[1] )
    {
        ChkDR( DRM_E_CRYPTO_FAILED );
    }

ErrorExit:
    if( DRM_FAILED( dr ) && f_pdwPlaintext != NULL && f_cdwPlaintext > 0 )
    {
        OEM_SELWRE_ZERO_MEMORY( f_pdwPlaintext, f_cdwPlaintext * sizeof(DRM_DWORD) );
    }

    return dr;
}

/*********************************************************************************************
** Function:  Oem_AesKeyWrap_WrapKeyAES128
**
** Synopsis:  Wraps the AES-128 plaintext key data and 8 additional bytes
**            using the provided AES encryption key
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pPlaintext]  : A pointer to the plaintext key data to be wrapped
**            [f_pCiphertext] : A pointer to the wrapped ciphertext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_WrapKeyAES128(
    __in                         const OEM_AES_KEY_CONTEXT          *f_pKey,
    __in_ecount( 1 )             const OEM_UNWRAPPED_KEY_AES_128    *f_pPlaintext,
    __out_ecount( 1 )                  OEM_WRAPPED_KEY_AES_128      *f_pCiphertext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pPlaintext  != NULL );
    ChkArg( f_pCiphertext != NULL );

    ChkDR( _AesKeyWrap_WrapKey128(
        f_pKey,
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw),
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*********************************************************************************************
** Function:  Oem_AesKeyWrap_WrapKeyAES128_Only
**
** Synopsis:  Wraps the AES-128 plaintext key data
**            using the provided AES encryption key
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pPlaintext]  : A pointer to the plaintext key data to be wrapped
**            [f_pCiphertext] : A pointer to the wrapped ciphertext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**
** Comments:  The plaintext key data must be 128 bits in length.
**            The resulting ciphertext will be 192 bits in length.
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_WrapKeyAES128_Only(
    __in                         const OEM_AES_KEY_CONTEXT              *f_pKey,
    __in_ecount( 1 )             const OEM_UNWRAPPED_KEY_AES_128_ONLY   *f_pPlaintext,
    __out_ecount( 1 )                  OEM_WRAPPED_KEY_AES_128_ONLY     *f_pCiphertext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pPlaintext  != NULL );
    ChkArg( f_pCiphertext != NULL );

    ChkDR( _AesKeyWrap_WrapKey128(
        f_pKey,
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw),
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*********************************************************************************************
** Function:  Oem_AesKeyWrap_WrapKeyECC256
**
** Synopsis:  Wraps the ECC-256 plaintext key data and 8 additional bytes
**            using the provided AES encryption key
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pPlaintext]  : A pointer to the plaintext key data to be wrapped
**            [f_pCiphertext] : A pointer to the wrapped ciphertext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_WrapKeyECC256(
    __in                         const OEM_AES_KEY_CONTEXT       *f_pKey,
    __in_ecount( 1 )             const OEM_UNWRAPPED_KEY_ECC_256 *f_pPlaintext,
    __out_ecount( 1 )                  OEM_WRAPPED_KEY_ECC_256   *f_pCiphertext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pPlaintext  != NULL );
    ChkArg( f_pCiphertext != NULL );

    ChkDR( _AesKeyWrap_WrapKey128(
        f_pKey,
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw),
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw) ) );

ErrorExit:
    return dr;
}

/*********************************************************************************************
** Function:  Oem_AesKeyWrap_UnwrapKeyAES128
**
** Synopsis:  Unwraps the AES-128 ciphertext key data using the provided AES decryption key
**            and the additional data with it.
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pCiphertext] : A pointer to the ciphertext key data to be unwrapped
**            [f_pPlaintext]  : A pointer to the unwrapped plaintext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**            DRM_E_CRYPTO_FAILED
**                the key unwrapping failed the data integrity check
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_UnwrapKeyAES128(
    __in                          const OEM_AES_KEY_CONTEXT       *f_pKey,
    __in_ecount( 1 )              const OEM_WRAPPED_KEY_AES_128   *f_pCiphertext,
    __out_ecount( 1 )                   OEM_UNWRAPPED_KEY_AES_128 *f_pPlaintext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pPlaintext  != NULL );

    ChkDR( _AesKeyWrap_UnwrapKey128(
        f_pKey,
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw),
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*********************************************************************************************
** Function:  Oem_AesKeyWrap_UnwrapKeyAES128_Only
**
** Synopsis:  Unwraps the AES-128 ciphertext key data using the provided AES decryption key
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pCiphertext] : A pointer to the ciphertext key data to be unwrapped
**            [f_pPlaintext]  : A pointer to the unwrapped plaintext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**            DRM_E_CRYPTO_FAILED
**                the key unwrapping failed the data integrity check
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_UnwrapKeyAES128_Only(
    __in                          const OEM_AES_KEY_CONTEXT            *f_pKey,
    __in_ecount( 1 )              const OEM_WRAPPED_KEY_AES_128_ONLY   *f_pCiphertext,
    __out_ecount( 1 )                   OEM_UNWRAPPED_KEY_AES_128_ONLY *f_pPlaintext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pPlaintext  != NULL );

    ChkDR( _AesKeyWrap_UnwrapKey128(
        f_pKey,
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw),
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*********************************************************************************************
** Function:  Oem_AesKeyWrap_UnwrapKeyECC256
**
** Synopsis:  Unwraps the ECC-256 ciphertext key data using the provided AES decryption key
**            and the additional data with it.
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pCiphertext] : A pointer to the ciphertext key data to be unwrapped
**            [f_pPlaintext]  : A pointer to the unwrapped plaintext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**            DRM_E_CRYPTO_FAILED
**                the key unwrapping failed the data integrity check
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_UnwrapKeyECC256(
    __in                          const OEM_AES_KEY_CONTEXT       *f_pKey,
    __in_ecount( 1 )              const OEM_WRAPPED_KEY_ECC_256   *f_pCiphertext,
    __out_ecount( 1 )                   OEM_UNWRAPPED_KEY_ECC_256 *f_pPlaintext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pPlaintext  != NULL );

    ChkDR( _AesKeyWrap_UnwrapKey128(
        f_pKey,
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw),
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
/*********************************************************************************************
** Function:  Oem_AesKeyWrap_UnwrapKeyECC256_Only
**
** Synopsis:  Unwraps the ECC-256 ciphertext key data using the provided AES decryption key.
**
** Arguments: [f_pKey]        : A pointer to the AES encryption key
**            [f_pCiphertext] : A pointer to the ciphertext key data to be unwrapped
**            [f_pPlaintext]  : A pointer to the unwrapped plaintext key data
**
** Returns:   DRM_SUCCESS
**                success
**            DRM_E_ILWALIDARG
**                one of the arguments was NULL or improperly initialized
**            DRM_E_CRYPTO_FAILED
**                the key unwrapping failed the data integrity check
**
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_AesKeyWrap_UnwrapKeyECC256_Only(
    __in                          const OEM_AES_KEY_CONTEXT            *f_pKey,
    __in_ecount( 1 )              const OEM_WRAPPED_KEY_ECC_256_ONLY   *f_pCiphertext,
    __out_ecount( 1 )                   OEM_UNWRAPPED_KEY_ECC_256_ONLY *f_pPlaintext )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pKey        != NULL );
    ChkArg( f_pCiphertext != NULL );
    ChkArg( f_pPlaintext  != NULL );

    ChkDR( _AesKeyWrap_UnwrapKey128(
        f_pKey,
        f_pCiphertext->m_rgdw,
        DRM_NO_OF(f_pCiphertext->m_rgdw),
        f_pPlaintext->m_rgdw,
        DRM_NO_OF(f_pPlaintext->m_rgdw) ) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;
