/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#ifndef OEMECCP256IMPL_H
#define OEMECCP256IMPL_H

#include <drmerr.h>
#include <elwrve.h>
#include <oemeccp256.h>

ENTER_PK_NAMESPACE;

DRM_API DRM_NO_INLINE DRM_RESULT DRM_CALL OEM_ECC_InitializeBignumStackOEMCtxImpl(
    __inout  DRM_VOID* f_pContext,
    __in     DRM_DWORD f_cbContextSize,
    __in_opt DRM_VOID* f_pOEMContext );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_InitializeBignumStackImpl(
    __inout  DRM_VOID* f_pContext,
    __in     DRM_DWORD f_cbContextSize );

DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenerateHMACKey_P256Impl(
    __inout                     PLAINTEXT_P256   *f_pKeys,
    __inout_bcount(f_cbBigCtx)  struct bigctx_t  *f_pBigCtx,
    __in                        DRM_DWORD         f_cbBigCtx );

DRM_API DRM_RESULT DRM_CALL OEM_ECC_CanMapToPoint_P256Impl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS ) const  digit_t   f_rgdNumber[],
    __inout                                        struct bigctx_t *f_pBigCtx );

DRM_API DRM_RESULT DRM_CALL OEM_ECC_MapX2PointP256Impl(
    __in_ecount(ECC_P256_INTEGER_SIZE_IN_DIGITS)   const digit_t  *f_pX,
    __out_ecount(ECC_P256_INTEGER_SIZE_IN_DIGITS)        digit_t  *f_pY,
    __inout_opt                                          digit_t   f_rgdSuppliedTemps[],
    __inout                                       struct bigctx_t *f_pBigCtx );

DRM_API_VOID DRM_VOID DRM_CALL OEM_ECC_ZeroPublicKey_P256Impl(
    __inout        PUBKEY_P256     *f_pPubkey );

DRM_API_VOID DRM_VOID DRM_CALL OEM_ECC_ZeroPrivateKey_P256Impl(
    __inout        PRIVKEY_P256    *f_pPrivkey );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_Decrypt_P256Impl(
    __in    const  PRIVKEY_P256    *f_pPrivkey,
    __in    const  CIPHERTEXT_P256 *f_pCiphertext,
    __out          PLAINTEXT_P256  *f_pPlaintext,
    __inout struct bigctx_t        *f_pBigCtx,
    __in           DRM_DWORD        f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_Encrypt_P256Impl(
    __in    const  PUBKEY_P256      *f_pPubkey,
    __in    const  PLAINTEXT_P256   *f_pPlaintext,
    __out          CIPHERTEXT_P256  *f_pCiphertext,
    __inout struct bigctx_t         *f_pBigCtx,
    __in           DRM_DWORD         f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPairRestrictedPriv_P256Impl(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_Sign_P256Impl(
    __in_ecount( f_cbMessageLen ) const  DRM_BYTE         f_rgbMessage[],
    __in                          const  DRM_DWORD        f_cbMessageLen,
    __in                          const  PRIVKEY_P256    *f_pPrivkey,
    __out                                SIGNATURE_P256  *f_pSignature,
    __inout                       struct bigctx_t        *f_pBigCtx,
    __in                                 DRM_DWORD        f_cbBigCtx );

DRM_API DRM_NO_INLINE DRM_RESULT DRM_CALL OEM_ECDSA_Verify_P256Impl(
    __in_ecount( f_cbMessageLen )              const  DRM_BYTE        f_rgbMessage[],
    __in                                       const  DRM_DWORD       f_cbMessageLen,
    __in                                       const  PUBKEY_P256    *f_pPubkey,
    __in                                       const  SIGNATURE_P256 *f_pSignature,
    __inout                                    struct bigctx_t       *f_pBigCtx,
    __in                                              DRM_DWORD       f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Colwert_BigEndianBytesToDigitsImpl(
  __in_ecount( ( f_cBits + 7 ) / 8 )              const DRM_BYTE  f_rgbInBytes[],
  __out_ecount( DRM_BITS_TO_DIGITS( f_cBits ) )         digit_t   f_rgdOutDigits[],
                                                  const DRM_DWORD f_cBits );

DRM_API DRM_RESULT DRM_CALL Colwert_DigitsToBigEndianBytesImpl(
  __in_ecount( DRM_BITS_TO_DIGITS( f_cBits ) )      const digit_t   f_rgdInDigits[],
  __out_ecount( ( f_cBits + 7 ) / 8 )                     DRM_BYTE  f_rgbOutBytes[],
  __in                                              const DRM_DWORD f_cBits );

DRM_API DRM_RESULT DRM_CALL Colwert_P256_PointToPlaintextImpl(
    __in_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES ) const  digit_t          f_rgptPoint[],
    __in_ecount( 1 )                                const  elwrve_t        *f_pElwrve,
    __out_ecount( 1 )                                      PLAINTEXT_P256  *f_pPlaintext );


DRM_API DRM_RESULT DRM_CALL Colwert_P256_BigEndianBytesToPointImpl(
    __in_ecount( ECC_P256_POINT_SIZE_IN_BYTES )              const  DRM_BYTE   f_rgbInBytes[],
    __in_ecount( 1 )                                         const  elwrve_t  *f_pElwrve,
    __out_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES )                digit_t    f_rgptOutPoint[],
    __inout                                                  struct bigctx_t  *f_pBigCtx );

DRM_API DRM_RESULT DRM_CALL Colwert_P256_BigEndianBytesToDigitsImpl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )              const DRM_BYTE f_rgdInBytes[],
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS )                  digit_t  f_rgbOutDigits[] );

DRM_API DRM_RESULT DRM_CALL Colwert_P256_BigEndianBytesToDigitsModOrderImpl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )   const DRM_BYTE  f_rgbBytes[],
    __in_ecount( 1 )                                const elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS )       digit_t   f_rgdDigits[] )
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_API DRM_RESULT DRM_CALL Colwert_P256_ModularIntToDigitsModOrderImpl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_ECAFFINES ) const  digit_t   f_rgecInModularInt[],
    __in_ecount( 1 )                                  const  elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS )          digit_t   f_rgbOutDigits[] );

DRM_API DRM_RESULT DRM_CALL Colwert_P256_DigitsToBigEndianBytesImpl(
    __in_ecount( ECC_P256_INTEGER_SIZE_IN_DIGITS )              const digit_t  f_rgdInDigits[],
    __out_ecount( ECC_P256_INTEGER_SIZE_IN_BYTES )                    DRM_BYTE f_rgbOutBytes[] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Colwert_P256_PointToBigEndianBytes(
    __in_ecount( ECC_P256_POINT_SIZE_IN_ECAFFINES )           const digit_t   f_rgptInPoint[],
    __in_ecount( 1 )                                          const elwrve_t *f_pElwrve,
    __out_ecount( ECC_P256_POINT_SIZE_IN_BYTES )                    DRM_BYTE  f_rgbOutBytes[] );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_SignHash_P256Impl(
    __in_ecount( f_cbHash )       const  DRM_BYTE        f_rgbHash[],
    __in                          const  DRM_DWORD       f_cbHash,
    __in                          const  PRIVKEY_P256   *f_pPrivkey,
    __out                                SIGNATURE_P256 *f_pSignature,
    __inout                       struct bigctx_t       *f_pBigCtx,
    __in                                 DRM_DWORD       f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPair_P256Impl(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECC_GenKeyPair_P256Impl_LWIDIA(
    __out          PUBKEY_P256  *f_pPubKey,
    __out          PRIVKEY_P256 *f_pPrivKey,
    __inout struct bigctx_t     *f_pBigCtx,
    __in           DRM_DWORD     f_cbBigCtx)
GCC_ATTRIB_NO_STACK_PROTECT();

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_SignHash_P256Impl_LWIDIA(
    __in_ecount( f_cbHash )       const  DRM_BYTE        f_rgbHash[],
    __in                          const  DRM_DWORD       f_cbHash,
    __in                          const  PRIVKEY_P256   *f_pPrivkey,
    __out                                SIGNATURE_P256 *f_pSignature,
    __inout                       struct bigctx_t       *f_pBigCtx,
    __in                                 DRM_DWORD       f_cbBigCtx )
GCC_ATTRIB_NO_STACK_PROTECT();


EXIT_PK_NAMESPACE;

#endif // OEMECCP256IMPL_H

