/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmteecache.h>
#include <oemtee.h>
#include <oemteecrypto.h>
#include <oemteeinternal.h>
#include <oembyteorder.h>
#include <oemcommon.h>
#include <drmmathsafe.h>
#include <oemteecryptointernaltypes.h>
#include <drmlastinclude.h>
#include <aes128.h>
#include "dev_disp.h"
#include "dev_disp_addendum.h"
#include "dev_sec_csb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_lwdec_pri.h"
#include "sec2_objsec2.h"
#include "playready/3.0/lw_playready.h"
#include "lwosselwreovly.h"
#include <drmconstants.h>
#include <drmxmrconstants.h>

#include "config/g_pr_private.h"

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT* should not be const.");

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)
/*
** Synopsis:
**
** Computes DM hash on the given buffer (in HS mode).
**
** Arguments:
**
** f_pHash: (out)   128 bit aligned buffer of size atleast 16 bytes to hold the DM hash
** f_pData: (in)    128 bit aligned buffer of size atleast f_dwSize bytes on which DM hash is computed.
** f_dwSize:(in)    Size of buffer (in multiples of 16 Bytes) on which DM hash is computed.
**
** IMPORTANT NOTE: As per the caller requirements, f_pbHash must be pre-initialized by caller either with zero or a random seed
**
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
**
** The function doesn't validate the function arguments before exelwtion.
** Caller must ensure that all the function arguments supplied to the
** function satisfy the constraits mentioned in the corresponding argument
** description above.
**
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
*/
// LWE (nkuo): Stack can be at VA, but SCP can only deal with PA. So we need these global buffers to make sure SCP operates with PA
DRM_BYTE g_prCDKBData[sizeof(LW_KB_CDKBData)] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0};
DRM_BYTE g_prSignature[sizeof(LW_KB_SIGNATURE)] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0};

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL ComputeDmhash_HS(
    __inout                     DRM_BYTE    *f_pbHash,
    __in                        DRM_BYTE    *f_pbData,
    __in                        DRM_DWORD    f_dwSize)
{
    DRM_RESULT dr         = DRM_SUCCESS;
    DRM_DWORD  dwDoneSize = 0;
    DRM_BYTE  *pbKey      = NULL;

    // Validate the signature of libMemHs before calling functions on this overlay
    OS_SEC_HS_LIB_VALIDATE(libMemHs, HASH_SAVING_DISABLE);

    // Copy the data into global buffers to make sure SCP operates with PA
    memcpy_hs( g_prSignature, f_pbHash, sizeof(LW_KB_SIGNATURE) );
    memcpy_hs( g_prCDKBData, f_pbData, sizeof(LW_KB_CDKBData) );

    while (dwDoneSize < f_dwSize)
    {
        pbKey = (DRM_BYTE *)&(g_prCDKBData[dwDoneSize]);

        /* H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1}) */
        falc_scp_trap(TC_INFINITY);
        falc_trapped_dmwrite(falc_sethi_i((DRM_DWORD)pbKey, SCP_R2));
        falc_dmwait();
        falc_trapped_dmwrite(falc_sethi_i((DRM_DWORD)g_prSignature, SCP_R3));
        falc_dmwait();
        falc_scp_key(SCP_R2);
        falc_scp_encrypt(SCP_R3, SCP_R4);
        falc_scp_xor(SCP_R4, SCP_R3);
        falc_trapped_dmread(falc_sethi_i((DRM_DWORD)g_prSignature, SCP_R3));
        falc_dmwait();
        falc_scp_trap(0);

        dwDoneSize += 16;
    }

    // Copy the data back to the input buffer
    memcpy_hs( f_pbHash, g_prSignature, sizeof(LW_KB_SIGNATURE) );

    // Clear the temporary buffer
    memset_hs( g_prCDKBData, 0, sizeof(LW_KB_CDKBData) );
    memset_hs( g_prSignature, 0, sizeof(LW_KB_SIGNATURE) );

    return dr;
}

/*
** Synopsis:
**
** Generates/Verify signature of the provided data using DM hash and an AES key
**
** Arguments:
**
** f_pCryptoData:       (in)        Data whose signature is computed/verified.
** f_pSignature:        (in/out)    Holds the signature.
** f_cryptoDataOp:      (in)        Indicate whether to generate or verify the signature.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL _LW_CryptoData_SignOrVerify(
    __in                        LW_KB_CDKBData    *f_pCryptoData,
    __inout                     LW_KB_SIGNATURE   *f_pSignature,
    __in                        LW_CryptoDataOp    f_cryptoDataOp)

{
    DRM_RESULT       dr = DRM_SUCCESS;
    DRM_DWORD        fCryptoMode;
    COMP_DMHASH_ARGS dmhashArgs;
    DRM_BOOL         bInLsMode;

    AssertChkArg( f_pCryptoData != NULL );
    AssertChkArg( f_pSignature  != NULL );

    // Make sure LWDEC is in LSMODE
    ChkDR(OEM_TEE_FlcnStatusToDrmStatus(prIsLwdecInLsMode_HAL(&PrHal,(LwBool *)&bInLsMode)));
    if (!bInLsMode)
    {
        ChkDR( DRM_E_OEM_HS_CHK_NOT_IN_LSMODE );
    }

    /*
     * We need a 16 byte aligned array to push into/fetch from SCP. So lets
     * allocate a bigger byte array so we can align it to 16 byte in worst case
     */
    DRM_BYTE  tempCDKBData[sizeof(LW_KB_CDKBData) + SCP_BUF_ALIGNMENT];
    DRM_BYTE  tempSignature[sizeof(LW_KB_SIGNATURE) + SCP_BUF_ALIGNMENT];

    /* Set the pointer to the next 16 byte aligned address within temp buffers */
    DRM_BYTE *pTempCDKBData16BAligned  = (SCP_BUF_ALIGNMENT - (((DRM_DWORD)&tempCDKBData[0])  & (SCP_BUF_ALIGNMENT - 1))) + &tempCDKBData[0];
    DRM_BYTE *pTempSignature16BAligned = (SCP_BUF_ALIGNMENT - (((DRM_DWORD)&tempSignature[0]) & (SCP_BUF_ALIGNMENT - 1))) + &tempSignature[0];

    /* To compute DM-Hash - Make sure size of LW_KB_CDKBData is multiple of 16 */
    ChkBOOL( (sizeof(LW_KB_CDKBData) & DM_HASH_BLOCK_SIZE_MASK) == 0, DRM_E_OEM_ILWALID_SIZE_OF_CDKBDATA );

    /* Copy input data into the local aligned buffer */
    OEM_SELWRE_MEMCPY(pTempCDKBData16BAligned, f_pCryptoData, sizeof(LW_KB_CDKBData));

    /* Set starting hash value in this local aligned buffer to zero */
    OEM_SELWRE_MEMSET(pTempSignature16BAligned, 0, sizeof(LW_KB_SIGNATURE));

    // Prepare the required arguments for HS exelwtion
    dmhashArgs.f_pbHash = pTempSignature16BAligned;
    dmhashArgs.f_pbData = pTempCDKBData16BAligned;

    // Compute the DM-Hash at HS mode
    ChkDR(OEM_TEE_HsCommonEntryWrapper(PR_HS_DMHASH, OVL_INDEX_IMEM(libMemHs), (DRM_VOID *)&dmhashArgs));

    fCryptoMode = DRF_DEF(_PR_AES128, _KDF, _MODE, _GEN_OEM_BLOB_SIGN_KEY);
    /* Sign the final DM-Hash value using AES 128 bit scp mask keys  */
    ChkDR(OEM_TEE_CRYPTO_AES128_EncryptOneBlock(NULL, pTempSignature16BAligned, fCryptoMode));

    if (f_cryptoDataOp == lwCryptoDataOp_Verify)
    {
        /* Now verify if the generated signature matches the input */
        ChkBOOL(OEM_SELWRE_ARE_EQUAL(pTempSignature16BAligned, f_pSignature->m_rgbSignature, sizeof(LW_KB_SIGNATURE)) == TRUE, DRM_E_ILWALID_SIGNATURE);
    }
    else
    {   /* Copy computed signature from local aligned buffer to output buffer */
        OEM_SELWRE_MEMCPY(f_pSignature->m_rgbSignature, pTempSignature16BAligned, sizeof(LW_KB_SIGNATURE));
    }

ErrorExit:
    return dr;
}

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function
** MUST have a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted
** content and you need to be given policy info from the license before
** DRM_TEE_AES128CTR_PrepareToDecrypt is called AND/OR content decryption is
** offloaded to a dedicated streaming crypto provider (SCP) which doesn't understand
** the CDKB format (and the SCP therefore requires an OEM-specific format for the
** policy and key normally contained in the CDKB). This function should then be used
** to construct an OEM-specific blob for one or both of these purposes as required by
** you.
**
** Synopsis:
**
** This function acts in one of two modes.
**
** If the f_pContentKey parameter is NULL, then this function is being called by
** DRM_TEE_AES128CTR_PreparePolicyInfo, and the output blob should be
** the OEM-specific policy blob it requires before DRM_TEE_AES128CTR_PrepareToDecrypt.
** The OEM-specific policy blob should then be used by you for any purpose it requires
** in order to enforce the policies given in the f_pPolicy parameter.
** This means that the output blob will be returned from DRM_TEE_AES128CTR_PreparePolicyInfo.
** This scenario will only occur if the DRM_TEE_PROPERTY_SUPPORTS_PREPARE_POLICY_INFO
** property is returned as part of the properties output from OEM_TEE_BASE_GetVersionInformation.
** If this function returns DRM_E_NOTIMPL in this scenario, the error will be ignored.
**
** Otherwise, this function is being called by DRM_TEE_AES128CTR_PrepareToDecrypt
** and the output data should be the OEM-specific replacement for the CDKB which not only
** contains any information in the previous case but also includes the content key.
** This means that the output blob will be returned from DRM_TEE_AES128CTR_PrepareToDecrypt.
** In this case, protection and security of the key info lies entirely within your implementation.
** This scenario will mean that the DRM_TEE_AES128CTR_DecryptContent function should never be called
** because the SCP will be performing decryption.
** This scenario will only occur if DRM_TEE_AES128CTR_PrepareToDecrypt was passed an output
** parameter in which to place the OEM-specific key info.
** If this function returns DRM_E_NOTIMPL in this scenario, the error will be interpreted as
** requesting that a second copy of the CDKB be returned by DRM_TEE_AES128CTR_PrepareToDecrypt.
**
** Depending on the capabilities of your TEE, some subset of the input parameters should be
** serialized into the output data in either scenario unless you do not require this data
** outside the TEE.  In that case, this function should return DRM_E_NOTIMPL.
**
** If any of the input data is stored inside the output parameter f_ppbInfo, it must be protected
** in the following way:
**      * non-TEE code MUST not be able to read any key material (content or sample protection key)
**      * non-TEE code MUST not be able to modify any of the policy info
**      * non-TEE code MUST not be able to modify the session ID
** This means it must be cryptographically protected with a key known only to the TEE and must
** not be able to be modified or replaced with new key material.
**
** Another option, is to store input data (keys, policy info, and session ID) inside the TEE and
** return a handle to the data in the f_ppbInfo parameter.  The function
** OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo will use this OEM-specific handle blob
** to retrieve the data during content decryption and slice header parsing.  Handle validation is
** OEM-specific, and thus MUST be enforced by you. If this option is chosen, the policy information
** and keys will have to be deep-copied so that pointer references are valid inside the TEE once the
** function exits.
**
** External Crypto Policy Call Flow:
**
** 1. Decryption Context Setup
**    During the initial decryptor setup for playing protected content, the system will query
**    if the TEE properties include the following option: DRM_TEE_PROPERTY_REQUIRES_PREPARE_POLICY_INFO.
**    As indicated earlier, this value is set in the function OEM_TEE_BASE_GetVersionInformation.
**    If this option is set, OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo will be initially
**    called with only the policy info parameter and session ID set.  For this step, you can
**    evaluate the license policy information and pre-initialize the OEM information object if
**    required. You may choose to skip this step by returning DRM_E_NOTIMPL if the content key
**    parameter is NULL.
**
** 2. Prepare to Decrypt
**    During this stage, the system validates the license chain, sets up sample protection
**    (if required), and finalizes the license policy information.  The updated license policy
**    is validated by calling OEM_TEE_AES128CTR_EnforcePolicy and then the content decryption
**    key blob (CDKB) is created.
**
** 3. Create OEM-specific Content Decryption Info
**    After the CDKB has been created, the system allows you to create your own decryption
**    information by calling OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo with all the
**    information required to decrypt protected content.  If you implement this function and
**    provide your own content decryption blob, the system will attempt to use this data
**    instead of the CDKB when performing content decryption.  This data must either be a handle
**    to memory that is only accessible by the TEE or must be cryptographically protected
**    because it will be transported to and from the TEE to the normal world.  The raw data
**    will be accessed later via the function OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo.
**
** 4. Process H.264 Slice Headers
**    During playback, it may be required to decrypt and process the H.264 slice headers. During
**    this process, a call will be made to OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo
**    to retrieve  content decryption information required to decrypt the content. The parameters
**    returned from OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo must match the input
**    parameters to OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo exactly.  If your implementation
**    of OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo returns DRM_E_NOTIMPL, then the
**    CDKB will be processed and its data will be used for decryption.
**
** 5. Content Decryption
**    During content decryption, a call will be made to OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo
**    to retrieve content decryption information required to decrypt the content. The parameters
**    returned from OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo must match the input
**    parameters to OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo exactly.  If your implementation
**    of OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo returns DRM_E_NOTIMPL, then the
**    CDKB will be processed and its data will be used for decryption.
**
** Operations Performed:
**
** 1. Any operation(s) performed are OEM-specific.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_pContentKey:               (in) The content key for content decryption.
** f_pSampleProtectionKey:      (in) The sample protection key if this is audio content
**                                   which is leaving the TEE in sample-protected form.
** f_cbOEMData:                 (in) The size of the initialization data.
** f_pbOEMData:                 (in) The initialization data passed to
**                                   DRM_TEE_AES128CTR_CreateOEMBlobFromCDKB.
** f_pPolicy:                   (in) The policy structure.
** f_pcbInfo:                  (out) The size of the OEM info.
** f_ppbInfo:                  (out) The OEM info to be sent from the calling
**                                   function to code outside the TEE.
**                                   This must be allocated by OEM_TEE_BASE_SelwreMemAlloc.
**                                   The caller will free this using OEM_TEE_BASE_SelwreMemFree.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const DRM_ID                     *f_pidSession,
    __in_opt                                  const OEM_TEE_KEY                *f_pContentKey,
    __in_opt                                  const OEM_TEE_KEY                *f_pSampleProtectionKey,
    __in                                            DRM_DWORD                   f_cbOEMData,
    __in_bcount_opt( f_cbOEMData )            const DRM_BYTE                   *f_pbOEMData,
    __in                                      const DRM_TEE_POLICY_INFO        *f_pPolicy,
    __out                                           DRM_DWORD                  *f_pcbInfo,
    __deref_out_bcount( *f_pcbInfo )                DRM_BYTE                  **f_ppbInfo )
{
    DRM_RESULT       dr              = DRM_SUCCESS;
    LW_KB_CDKBData  *pCDKBCryptoData = NULL;
    LW_KBData       *pLWKBData       = NULL;
    DRM_DWORD        cKeys           = 0;
    DRM_BYTE        *pbKey           = NULL;
    DRM_DWORD        fCryptoMode     = DRF_DEF(_PR_AES128, _KDF, _MODE, _GEN_OEM_BLOB_ENC_KEY);

    AssertChkArg( f_pContext    != NULL );
    AssertChkArg( f_pPolicy     != NULL );
    AssertChkArg( f_pContentKey != NULL );
    AssertChkArg( f_pcbInfo     != NULL );
    AssertChkArg( f_ppbInfo     != NULL );
    AssertChkArg( f_pidSession  != NULL );

    if(f_pPolicy->dwDecryptionMode == OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION)
    {
        ChkBOOL(f_pSampleProtectionKey != NULL, DRM_E_TEE_ILWALID_KEY_DATA);
        cKeys = 2;
    }
    else
    {
        /* We always get atleast one key i.e. content decryption key */
        cKeys = 1;
        ChkBOOL(f_pSampleProtectionKey == NULL, DRM_E_TEE_ILWALID_KEY_DATA);
    }

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, sizeof(LW_KBData), ( DRM_VOID ** )&pLWKBData ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pLWKBData, sizeof(LW_KBData) ) );

    pCDKBCryptoData = &pLWKBData->oCryptoData;

    /* Fill in the OEM CDKB Data */
    pCDKBCryptoData->dwVersion        = OEM_CDKB_LWRRENT_VERSION;
    pCDKBCryptoData->cKeys            = cKeys;
    pCDKBCryptoData->dwDecryptionMode = f_pPolicy->dwDecryptionMode;

    OEM_TEE_MEMCPY(&pCDKBCryptoData->idSession, f_pidSession, sizeof(DRM_ID));

    /* wrap content key  */
    pbKey = pCDKBCryptoData->rgKeys[CONTENT_KEY_INDEX].rgbRawKey;
    OEM_TEE_MEMCPY(pbKey, f_pContentKey->pKey, DRM_AES_KEYSIZE_128);

    /* wrap key using scp mask keys */
    ChkDR( OEM_TEE_CRYPTO_AES128_EncryptOneBlock( NULL, pbKey, fCryptoMode ) );

    /* wrap sample protection key if available */
    if(f_pSampleProtectionKey != NULL)
    {
        pbKey = pCDKBCryptoData->rgKeys[SAMPLEPROT_KEY_INDEX].rgbRawKey;
        OEM_TEE_MEMCPY(pbKey, f_pSampleProtectionKey->pKey, DRM_AES_KEYSIZE_128);

        /* wrap key using scp mask keys */
        ChkDR( OEM_TEE_CRYPTO_AES128_EncryptOneBlock( NULL, pbKey, fCryptoMode ) );
    }

    /* Sign crypto data */
    ChkDR(_LW_CryptoData_SignOrVerify( pCDKBCryptoData, &pLWKBData->oSignature, lwCryptoDataOp_Sign ));

    *f_pcbInfo = sizeof(LW_KBData);
    *f_ppbInfo = (DRM_BYTE *)pLWKBData;

ErrorExit:
    if (DRM_FAILED(dr))
    {
        OEM_TEE_BASE_SelwreMemFree(f_pContext, ( DRM_VOID ** )&pLWKBData);
    }
    return dr;
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function
** MUST have a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted
** content and you need to be given policy info from the license before
** DRM_TEE_AES128CTR_PrepareToDecrypt is called AND/OR content decryption is
** offloaded to a dedicated streaming crypto provider (SCP) which doesn't understand
** the CDKB format (and the SCP therefore requires an OEM-specific format for the
** policy and key normally contained in the CDKB). This function should then be used
** to parse and validate the OEM-specific blob created in the function
** OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo.  This function must return the
** policy information, content key, TEE session ID, and the sample protection key
** (if available) associated with the OEM-specific blob.
**
** Synopsis:
**
** This function returns the content key, sample protection key, the session ID, and
** the content policy information based on the provided OEM-specific information
** (f_pbInfo).  The output values MUST be the match exactly the values passed into
** the OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo function. If you return
** DRM_E_NOTIMPL for this function, the CDKB will be used to decrypt content and
** provide policy information.
**
** This function is used by AES128CTR decryption and H.264 slice header parser to
** obtain the keys for decrypting content and policies to enforce during decryption.
**
** Operations Performed:
**
** 1. Parse and validate the OEM-specific info input (returned by the function
**    OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo).
** 2. Return the policy info, session ID, content key and the sample protection key
**    (if present).
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_cbInfo:                    (in) The size of the OEM information.
** f_pbInfo:                    (in) The OEM information created in the call to
**                                   OEM_TEE_AES128CTR_BuildExternalPolicyCryptoInfo.
** f_ppPolicyInfo:             (out) The policy information structure associated
**                                   with the provided OEM info parameter (f_pbInfo).
**                                   This parameter MUST be allocated with
**                                   OEM_TEE_BASE_SelwreMemAlloc.  It will be freed
**                                   by the caller using OEM_TEE_BASE_SelwreMemFree.
**                                   If the policy information contains restrictions,
**                                   this allocation MUST include the restrictions
**                                   and restriction configuration data.  The policy
**                                   structure MUST have updated pointer values for
**                                   DRM_TEE_POLICY_INFO::pRestrictions and
**                                   DRM_TEE_POLICY_RESTRICTION::pbRestrictionConfiguration
**                                   that reference a location inside this parameter's
**                                   buffer.
** f_pidSession:               (out) The TEE Session identifier associated with the
**                                   provided OEM info parameter (f_pbInfo).
** f_pContentKey:              (out) The content key used for decryption that is
**                                   associated with the provided OEM info parameter
**                                   f_pbInfo.
** f_pSampleProtectionKey:     (out) The sample protection key associated with the
**                                   provided OEM info (if present).
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_ParseAndVerifyExternalPolicyCryptoInfo(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                            DRM_DWORD                   f_cbInfo,
    __in_bcount( f_cbInfo )                   const DRM_BYTE                   *f_pbInfo,
    __deref_out                                     DRM_TEE_POLICY_INFO       **f_ppPolicyInfo,
    __out                                           DRM_ID                     *f_pidSession,
    __out                                           OEM_TEE_KEY                *f_pContentKey,
    __out                                           OEM_TEE_KEY                *f_pSampleProtKey )
{
    return DRM_E_NOTIMPL;
}
#endif

/*
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this function MUST have
** a valid implementation for your PlayReady port.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function ensures that only supported decryption modes are used for AES 128 CTR
** content decryption.
**
** Operations Performed:
**
**  1. Determine whether the given decryption mode is supported.  If so, return.
**  2. If not, changes the decryption mode to the MOST secure decryption mode that is supported.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pdwDecryptionMode:  (in/out) On input, the requested decryption mode.
**                                On output, the decryption mode that will be used.
*/
#if defined (SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL
OEM_TEE_AES128CTR_RemapRequestedDecryptionModeToSupportedDecryptionMode(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __inout                                         DRM_DWORD                  *f_pdwDecryptionMode )
{
    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );
    DRMASSERT( f_pdwDecryptionMode != NULL );

    if( *f_pdwDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_NOT_SELWRE
     && *f_pdwDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_HANDLE
     && *f_pdwDecryptionMode != OEM_TEE_AES128CTR_DECRYPTION_MODE_SAMPLE_PROTECTION )
    {
        *f_pdwDecryptionMode = OEM_TEE_AES128CTR_DECRYPTION_MODE_HANDLE;
    }
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation that is
** specific to your PlayReady port if your PlayReady port supports policy enforcement
** in the TEE.
**
** This function is only used if the client supports playback of AES CTR encrypted content.
** Certain PlayReady clients, such as PRND Transmitters, may not need to implement this function.
**
** Synopsis:
**
** This function enforces the content protection policy.  This function is called by:
** DRM_TEE_AES128CTR_PrepareToDecrypt
** DRM_TEE_AES128CTR_DecryptContent
** DRM_TEE_H264_PreProcessEncryptedData.
** You MUST examine the DRM_TEE_POLICY_INFO structure for all provided policies
** and enforce them as per PlayReady Compliance rules.
** If the provided policy cannot be enforced, this function MUST return an error code
** other than DRM_E_NOTIMPL.  Returning DRM_E_NOTIMPL implies that OEM TEE level policy
** enforcement is either not supported or not required.
**
** Arguments:
**
** f_pContext:              (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
** f_ePolicyCallingAPI:         (in) The calling API providing f_pPolicy.
** f_pPolicy:                   (in) The current content protection policy.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_AES128CTR_EnforcePolicy(
    __inout                                        OEM_TEE_CONTEXT                  *f_pContext,
    __in                                           DRM_TEE_POLICY_INFO_CALLING_API   f_ePolicyCallingAPI,
    __in                                     const DRM_TEE_POLICY_INFO              *f_pPolicy )
{
    DRM_RESULT dr   = DRM_SUCCESS;
    LwU32      i    = 0;
    UPDATE_DISP_BLANKING_POLICY_ARGS udpateDispBlankingPolicyArgs = {0};

    // As per Lwpu design, we should execute EnforcePolicy only if calling API is PrepareToDecrypt.
    AssertChkArg( f_ePolicyCallingAPI == DRM_TEE_POLICY_INFO_CALLING_API_AES128CTR_PREPARETODECRYPT );

    LwBool  type1LockingRequired = LW_FALSE;
    LwU32   policyValForContext  = 0;

    // Check OPL levels. We will only need to check UNCOMPRRESSED_DIGITAL OPL.
    // LWPU will ignore all other OPLs even if present.
    if ( f_pPolicy->rgwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] <= 250 )
    {
        //
        // f_pPolicy will never be null since it is a local variable in the parent function. So no need to check for NULL.
        // f_pPolicy->rgwOutputProtectionLevels will also never be NULL.
        //
        LW_PDISP_VPR_POLICY_CTRL_BLANK_VPR_ON_NONE(policyValForContext);
    }
    else if ( (f_pPolicy->rgwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] >= 251) &&
              (f_pPolicy->rgwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] <= 300) )
    {
        LW_PDISP_VPR_POLICY_CTRL_BLANK_VPR_ON_NO_HDCP(policyValForContext);
    }
    else if ( f_pPolicy->rgwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] >= 301 )
    {
        // Treat OPL value >= 301 as invalid, and return error from here so the video player fails the playback.
        ChkDR( DRM_E_ILWALID_LICENSE );
    }

    for ( i  = 0; i < f_pPolicy->cRestrictions ; i++ )
    {
        //
        // A guideline for future changes here, whenever new restrictions are being added here in this loop:
        // If we need to handle multiple restrictions, that can specify opposite or conflicting behaviour, then
        // we need to either return ERROR if such conflicting restrictions are found,
        // or handle the higher priority restriction, while ignoring the lower priority one.
        // The behaviour being coded up here (returning error, or ignoring lower priority restriction) should be
        // communicated to Microsoft and we should receive a sign-off on that behaviour.
        //
        // f_pPolicy->pRestrictions[i < cRestrictions] will always be valid here since this is dynamically created
        // with the size 'f_pPolicy->cRestrictions' in _ParseLicenseRestrictions function which also checks for
        // out of bounds before writing the data, so no need to add a NULL check here.
        //
        if( OEM_SELWRE_ARE_EQUAL( &g_guidHDCPTypeRestriction, f_pPolicy->pRestrictions[i].idRestrictionType.rgb, sizeof(DRM_GUID)) )
        {
            // HDCP type 1 restriction. Do some error checking first.

            // restriction category should be digital video.
            if (XMR_OBJECT_TYPE_EXPLICIT_DIGITAL_VIDEO_OUTPUT_PROTECTION_CONTAINER !=
                f_pPolicy->pRestrictions[i].wRestrictionCategory)
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            // In case of HDCP type1, digital video OPL must be >= 271, otherwise it is an incorrectly formed license.
            if (f_pPolicy->rgwOutputProtectionLevels[OUTPUT_PROTECTION__UNCOMPRESSED_DIGITAL_VIDEO_INDEX] < 271)
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            // Size of the BCD has to be 32bits i.e. 1 DWORD
            if ( sizeof (DRM_DWORD) != f_pPolicy->pRestrictions[i].cbRestrictionConfiguration)
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            // Extract the BCD value and colwert to little endian
            DRM_DWORD bcdValue = *((DRM_DWORD*)(f_pPolicy->pRestrictions[i].pbRestrictionConfiguration));
            DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD(bcdValue, &bcdValue);

            // Value of BCD has to be 1, if not, return error.
            if ( 1 != bcdValue )
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            //
            // Error checks done. Lets take this as a valid license. Disallow noHDCP and HDCP1X.
            // Yes, TYPE1 GUID should not affect analog output. For blanking analog output,
            // license creator authority should use "Digital-only-token" GUID in the license.
            //
            LW_PDISP_VPR_POLICY_CTRL_BLANK_VPR_ON_NON_HDCP22_TYPE1_OR_INTERNAL(policyValForContext);

            type1LockingRequired = LW_TRUE;
        }

        if( OEM_SELWRE_ARE_EQUAL( &g_guidVGA_MaxRes, f_pPolicy->pRestrictions[i].idRestrictionType.rgb, sizeof(DRM_GUID)) )
        {
            // restriction category should be analog video.
            if (XMR_OBJECT_TYPE_EXPLICIT_ANALOG_VIDEO_OUTPUT_PROTECTION_CONTAINER !=
                f_pPolicy->pRestrictions[i].wRestrictionCategory)
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            // Error checks done. Lets take this as a valid license. This policy says to restrict analog to 520000 pixels, lets blank entirely.
            LW_PDISP_VPR_POLICY_CTRL_BLANK_VPR_ON_ANALOG(policyValForContext);
        }

        if( OEM_SELWRE_ARE_EQUAL( &g_guidDigitalOnlyToken, f_pPolicy->pRestrictions[i].idRestrictionType.rgb, sizeof(DRM_GUID)) )
        {
            // restriction category should be analog video.
            if (XMR_OBJECT_TYPE_EXPLICIT_ANALOG_VIDEO_OUTPUT_PROTECTION_CONTAINER !=
                f_pPolicy->pRestrictions[i].wRestrictionCategory)
            {
                ChkDR( DRM_E_ILWALID_LICENSE );
            }

            // Value and size of this BCD can be ignored as per Microsoft.

            // Error checks done. Lets take this as a valid license. This policy says the content is digital only, so lets blank analog entirely.
            LW_PDISP_VPR_POLICY_CTRL_BLANK_VPR_ON_ANALOG(policyValForContext);
        }
    }

    udpateDispBlankingPolicyArgs.f_policyValForContext  = policyValForContext;
    udpateDispBlankingPolicyArgs.f_type1LockingRequired = type1LockingRequired;
    ChkDR( OEM_TEE_HsCommonEntryWrapper(PR_HS_UPDATE_DISP_BLANKING_POLICY, OVL_INDEX_IMEM(libVprPolicyHs), (void *)&udpateDispBlankingPolicyArgs) );

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

PREFAST_POP; /* __WARNING_NONCONST_PARAM_25004 */

