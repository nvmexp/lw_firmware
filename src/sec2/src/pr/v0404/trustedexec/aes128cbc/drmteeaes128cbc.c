/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmtee.h>
#include <drmteesupported.h>
#include <drmteebase.h>
#include <drmteexb.h>
#include <drmteekbcryptodata.h>
#include <drmteecache.h>
#include <drmerr.h>
#include <oemtee.h>
#include <drmrivcrlparser.h>
#include <oembyteorder.h>
#include <drmxmrformatparser.h>
#include <drmderivedkey.h>
#include <drmbcertconstants.h>
#include <drmteedecryptinternal.h>
#include <drmteedebug.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_TEE_IMPL_AES128CBC_IsAES128CBCSupported( DRM_VOID )
{
    return TRUE;
}
#endif

#ifdef NONE
/*
** Synopsis:
**
** This function decrypts content using the decryption mode that was returned from
** DRM_TEE_DECRYPT_PrepareToDecrypt and allows multiple encrypted regions, multiple IVs,
** and encrypted region striping (i.e. 'cbcs' if specified, 'cbc1' if not).
**
** This function is called inside:
**  No top-level drmmanager functions.
**
** Operations Performed:
**
**  1. Verify the given DRM_TEE_CONTEXT.
**  2. Retrieve the data required for decryption as follows.
**     a. If you have provided the OEM key information via implementing the
**        OEM_TEE_DECRYPT_BuildExternalPolicyCryptoInfo, we will first attempt
**        to retrieve the decryption data from the OEM key info by calling
**        OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo.  If you
**        implement this function and successfully return the keys, session ID, and
**        policy, the CDKB will not be parsed and used.  This option can be used
**        if either the OEM key information can be stored in the TEE or the
**        decryption data can be cryptographically protected with better performance
**        by your own implementation.  If you choose this option and the data will
**        not remain in the TEE, you MUST make sure the keys are cryptographically
**        protected and the policy can be validated not to have changed (via
**        cryptographic signature).
**     b. If OEM_TEE_DECRYPT_ParseAndVerifyExternalPolicyCryptoInfo returns
**        DRM_E_NOTIMPL or the key count returned is zero, the given Content
**        Decryption Key Blob (CDKB) will be parsed and its signature verified.
**  3. If the decryption mode returned from DRM_TEE_DECRYPT_PrepareToDecrypt
**     was equal to OEM_TEE_DECRYPTION_MODE_NOT_SELWRE,
**     decrypt the given content (using the given auxiliary data)
**     into a clear buffer and return it via the Clear Content Data (CCD).
**     Otherwise, decrypt the given content (using the given auxiliary data)
**     into an opaque buffer and return an it via the CCD.
**     The OEM-defined opaque buffer format is specified by the specific
**     OEM-defined decryption mode that was returned
**     (i.e. the OEM may support multiple decryption modes).
**
** Arguments:
**
** f_pContext:                             (in/out) The TEE context returned from
**                                                  DRM_TEE_BASE_AllocTEEContext.
**                                                  If this parameter is NULL, the CDKB must be
**                                                  self-contained, i.e. it must include the
**                                                  data required to reconstitute the
**                                                  DRM_TEE_CONTEXT.
** f_pCDKB:                                    (in) The Content Decryption Key Blob (CDKB)
**                                                  returned from DRM_TEE_DECRYPT_PrepareToDecrypt.
** f_pOEMKeyInfo                               (in) The Oem Key Info returned from
**                                                  DRM_TEE_PROXY_DECRYPT_CreateOEMBlobFromCDKB.
** f_pEncryptedRegionInitializatiolwectorsHigh (in) Each entry represents the high 8 bytes of the
**                                                  AES128CBC initialization vector for the next
**                                                  encrypted region.
** f_pEncryptedRegionInitializatiolwectorsLow  (in) Each entry represents the low 8 bytes of the
**                                                  AES128CBC initialization vector for the next
**                                                  encrypted region.  If an 8 byte IV is being used,
**                                                  f_pEncryptedRegionInitializatiolwectorsLow->cqwData
**                                                  should be set to 0.
** f_pEncryptedRegionCounts                    (in) Each entry represents the number of DWORD pairs
**                                                  in f_pEncryptedRegionMappings which represent
**                                                  the next encrypted region which uses a single
**                                                  AES128CBC initialization vector.
**
**                                                  The number of entries in this list MUST
**                                                  be equal to the number of entries in
**                                                  f_pEncryptedRegionInitializatiolwectorsHigh.
**
**                                                  Two times the sum of all entries in this
**                                                  list MUST be equal to the number of entries
**                                                  in f_pEncryptedRegionMappings.
**
**                                                  Typically, this value will have one entry
**                                                  per sample inside the f_pEncrypted buffer.
** f_pEncryptedRegionMappings                  (in) The encrypted-region mappings for all
**                                                  encrypted regions.
**
**                                                  The number of DRM_DWORDs must be a multiple
**                                                  of two.  Each DRM_DWORD pair represents
**                                                  values indicating how many clear bytes
**                                                  followed by how many encrypted bytes are
**                                                  present in the data on input.
**
**                                                  A given AES128CBC Initialization Vector from
**                                                  f_pEncryptedRegionInitializatiolwectorsHigh
**                                                  will be used for two times the number
**                                                  of encrypted region mappings in
**                                                  f_pEncryptedRegionMappings (as will
**                                                  f_pEncryptedRegionInitializatiolwectorsLow
**                                                  if present).
**
**                                                  The sum of all of these DRM_DWORD pairs
**                                                  must be equal to bytecount(clear content),
**                                                  i.e. equal to f_pEncrypted->cb.
**
**                                                  If an entire region is encrypted, there will be
**                                                  two DRM_DWORDs with values 0, bytecount(encrypted content).
**                                                  If an entire region is clear, there will be
**                                                  two DRM_DWORDs with values bytecount(clear content), 0.
**
**                                                  The caller must already be aware of what information
**                                                  to pass to this function, as this information is
**                                                  format-/codec-specific.
** f_pEncryptedRegionSkip                      (in) If non-empty, the encrypted regions are encrypted
**                                                  with striping (i.e. 'cbcs' content).
**                                                  In this case, f_pEncryptedRegionSkip->cdwData == 2.
**                                                  f_pEncryptedRegionSkip->pdwData[0] is the number of
**                                                  encrypted BLOCKS (16 bytes each) in each encrypted stripe.
**                                                  f_pEncryptedRegionSkip->pdwData[1] is the number of
**                                                  clear BLOCKS (16 bytes each) in each clear stripe.
**                                                  For non-striped encrypted regions (i.e. 'cbc1' content),
**                                                  f_pEncryptedRegionSkip->cdwData should be set to 0
**                                                  OR f_pEncryptedRegionSkip->cdwData set to 2 with
**                                                  both entries set to 0.
** f_pEncrypted:                               (in) The encrypted bytes being decrypted.
** f_pCCD:                                    (out) The Clear Content Data (CCD) which represents the
**                                                  clear data that was decrypted by this function.
**                                                  This CCD is opaque unless the decryption mode
**                                                  returned by DRM_TEE_DECRYPT_PrepareToDecrypt
**                                                  was OEM_TEE_DECRYPTION_MODE_NOT_SELWRE.
**                                                  This should be freed with DRM_TEE_IMPL_BASE_FreeBlob.
**
**  Note: This function is analogous to DRM_TEE_AES128CTR_DecryptContentMultiple.
**
**  Example input parameters: Refer to DRM_TEE_AES128CTR_DecryptContentMultiple.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_TEE_AES128CBC_DecryptContentMultiple(
    __inout_opt                 DRM_TEE_CONTEXT              *f_pContext,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pCDKB,
    __in_tee_opt          const DRM_TEE_BYTE_BLOB            *f_pOEMKeyInfo,
    __in                  const DRM_TEE_QWORDLIST            *f_pEncryptedRegionInitializatiolwectorsHigh,
    __in_tee_opt          const DRM_TEE_QWORDLIST            *f_pEncryptedRegionInitializatiolwectorsLow,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionCounts,
    __in                  const DRM_TEE_DWORDLIST            *f_pEncryptedRegionMappings,
    __in_tee_opt          const DRM_TEE_DWORDLIST            *f_pEncryptedRegionSkip,
    __in                  const DRM_TEE_BYTE_BLOB            *f_pEncrypted,
    __out                       DRM_TEE_BYTE_BLOB            *f_pCCD )
{
    DRM_RESULT  dr = DRM_SUCCESS;

    ChkDR( DRM_TEE_AES128_DecryptContentHelper(
        f_pContext,
        DRM_TEE_POLICY_INFO_CALLING_API_AES128CBC_DECRYPTCONTENTMULTIPLE,
        f_pCDKB,
        f_pOEMKeyInfo,
        f_pEncryptedRegionInitializatiolwectorsHigh,
        f_pEncryptedRegionInitializatiolwectorsLow,
        f_pEncryptedRegionCounts,
        f_pEncryptedRegionMappings,
        f_pEncryptedRegionSkip,
        f_pEncrypted,
        f_pCCD ) );

ErrorExit:
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

