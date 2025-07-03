/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMECCP256_C
#include <drmerr.h>
#include <drmprofile.h>
#include <bigdefs.h>
#include <bignum.h>
#include <fieldpriv.h>
#include <ecurve.h>
#include <mprand.h>
#include <oemeccp256impl.h>
#include <oemsha256.h>
#include <oem.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
DRM_API_VOID DRM_VOID DRM_CALL OEM_ECC_ZeroPublicKey_P256(
    __out        PUBKEY_P256     *f_pPubkey )
{
    OEM_ECC_ZeroPublicKey_P256Impl( f_pPubkey );
}
#endif

#if defined(SEC_COMPILE)
/*************************************************************************************************
**
**  Function: OEM_ECDSA_Verify_P256
**
**  Synopsis: Test to see if a message and a ECDSA signature match
**
**  Algorithm: x = message, P = public key, G = generator pt
**             (s,t) = signature, q = field order and the ECDSA modulus
**             w = inverse( t ) MOD q
**             i = w * SHA1(x) MOD q
**             j = w * s MOD q
**             (u, v) = iG + jP
**             Verify(x, (s, t) ) := u MOQ q == s
**
**  Arguments:
**   IN   [f_rgbMessage ]:   Message to check signature
**   IN   [f_cbMessageLen]: Message length in bytes
**   IN   [f_pPubkey]:      Pointer to ECC Public Key struct containing the pubkey
**   IN   [f_pSignature ]:   Pointer to the signature for the message
**     OUT[f_pfVerified]:   Returned true if signature was valid, else false
**        [f_pBigCtx] :   Pointer to DRMBIGNUM_CONTEXT_STRUCT for temporary memory allocations.
**        [f_cbBigCtx]:    size of allocated f_pBigCtx
**
**  Returns:    S_OK if not errors in the function, else corresponding
**              DRM_RESULT error code. [f_pfVerified] param tells if the
**              signature matched the message ( and pub key )
**
*************************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_ECDSA_Verify_P256(
    __in_ecount( f_cbMessageLen )              const  DRM_BYTE         f_rgbMessage[],
    __in                                       const  DRM_DWORD        f_cbMessageLen,
    __in                                       const  PUBKEY_P256     *f_pPubkey,
    __in                                       const  SIGNATURE_P256  *f_pSignature,
    __inout                                    struct bigctx_t        *f_pBigCtx,
    __in                                              DRM_DWORD        f_cbBigCtx )
{
    DRM_RESULT  dr        = DRM_SUCCESS;
    CLAW_AUTO_RANDOM_CIPHER

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRM_ECC_P256, PERF_FUNC_OEM_ECDSA_Verify_P256 );
#endif

    ChkDR( OEM_ECDSA_Verify_P256Impl(
        f_rgbMessage,
        f_cbMessageLen,
        f_pPubkey,
        f_pSignature,
        f_pBigCtx,
        f_cbBigCtx ) );

ErrorExit:

#if DRM_SUPPORT_ECCPROFILING
    DRM_PROFILING_LEAVE_SCOPE;
#endif

    return dr;
} /* end OEM_ECDSA_Verify_P256 */
#endif

EXIT_PK_NAMESPACE_CODE;

