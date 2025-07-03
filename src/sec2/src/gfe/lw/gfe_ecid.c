/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gfe_ecid.c
 * @brief  Processes the Read ECID method
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objgfe.h"
#include "sec2_chnmgmt.h"

#include "gfe/gfe_mthds.h"
#include "gfe/gfe_key.h"
#include "sha256.h"
#include "class/cl95a1.h"
#include "tsec_drv.h"
#include "scp_crypt.h"

/* ------------------------- Macros and Defines ----------------------------- */

// Colwenience macro to set the memdesc for DMA reads/writes
#define SET_MEMDESC(pMemDesc, addr32)                                           \
do                                                                              \
{                                                                               \
    (pMemDesc)->params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,  \
                                        RM_SEC2_DMAIDX_VIRT, 0);                \
    (pMemDesc)->address.lo = (addr32) << 8;                                     \
    (pMemDesc)->address.hi = (addr32) >> 24;                                    \
} while (LW_FALSE)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static LwU8 _gfeRsBuf[GFE_RESPONSE_BUF_SIZE]
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_gfe", "_gfeRsBuf");

static LwU8 _gfeKeyBuf[GFE_RSA_PRIV_KEY_SIZE]
    GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_gfe", "_gfeKeyBuf");

#ifndef GFE_GEN_MODE
static LwU8 _gfeSignBuf[GFE_RSA_SIGN_SIZE]
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_gfe", "_gfeSignBuf");
#endif

/* ------------------------- Private Functions ------------------------------ */
#ifndef GFE_GEN_MODE
static LwU32 _gfeDecryptAndValidateRsaKey(LwU8 *pKeyBuf, LwU8 *pIvBuf)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeDecryptAndValidateEncryptedKey");
static LwU32 _gfeRsaSignMessage(LwU8 *pMsg, LwU32 msgSize, LwU8 *pKeyBuf,
                                LwU8 *pSignBuf, LwU32 signSize)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeRsaSignMessage");
static LwU32 _gfeComputeEcidHashAndRsaSign(gfe_read_ecid_param *pReadEcid,
                                          LwU8 *pActKeyBuf,
                                          LwU8 *pSignBuf, LwU32 signSize)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeComputeEcidHashAndRsaSign");
static LwU32 _gfeCalcHashAndVerifyRsaKey(LwU8 *pKeyBuf, LwU32 keysize)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeCalcHashAndVerify");
#else
static FLCN_STATUS _gfeEncryptKeyAndCorrespondingHash(LwU8 *pKeyBuf, LwU8 *pIvBuf, LwU32 keysize)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeEncryptKeyAndCorrespondingHash");
#endif
static FLCN_STATUS _gfeCrypt(LwU8 *pSrcBuf, LwU32 size, LwU8 *pDest, LwU8 *pIvBuf,
                       LwBool bEncrypt)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeCrypt");
static LwBool _gfeIsParamOverlap(LwU32 offset1, LwU32 bufSize1,
                                 LwU32 offset2, LwU32 bufSize2)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeIsParamOverlap");
static FLCN_STATUS _gfeDoesBufOverlapOffset(LwU32 offset1, LwU32 bufSize1,
                                            LwU32 offset2, LwBool *bBIsOverlap)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeDoesBufOverlapOffset");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Handle the Read ECID method. DMA in the encrypted key, decrypt it and sign it
 * with the ECID hash.
 */
FLCN_STATUS
gfeHandleReadEcid(void)
{
    LwU32               paramOffset;
    LwU32               sigBufOffset;
    LwU32               keyBufOffset;
    LwU8                *pIvBuf      = _gfeKeyBuf;
    LwU8                *pActKeyBuf  = _gfeKeyBuf + GFE_RSA_AES_IV_SIZE_BYTES;
    gfe_read_ecid_param *pReadEcid   = NULL;
    RM_FLCN_MEM_DESC    memdesc;
    FLCN_STATUS         status       = FLCN_OK;

    // Return code returned to client
    LwU32               rc           = LW95A1_GFE_READ_ECID_ERROR_NONE;
    LwU32               *pSig       = NULL;

    //
    // FLCN_OK if rc was returned to client
    // FLCN_ERR_ILLEGAL_OPERATION if DMA NACK was received, and cleanup is needed
    //
    FLCN_STATUS         retValToSec2  = FLCN_OK;

    paramOffset  = GFE_GET_METHOD_PARAM_OFFSET(GFE_GET_SEC2_METHOD_ID(GFE_METHOD_ID(READ_ECID)));
    sigBufOffset = GFE_GET_METHOD_PARAM_OFFSET(GFE_GET_SEC2_METHOD_ID(GFE_METHOD_ID(SET_ECID_SIGN_BUF)));
    keyBufOffset = GFE_GET_METHOD_PARAM_OFFSET(GFE_GET_SEC2_METHOD_ID(GFE_METHOD_ID(SET_PRIV_KEY_BUF)));

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));

    // Check if the param offsets are valid
    if ((paramOffset == 0) || (sigBufOffset == 0) || (keyBufOffset == 0))
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_ILWALID_BUF_SIZE;
        goto gfeHandleReadEcid_exit;
    }


    //
    // Make sure that the param buffers don't wrap around, and that the
    // buffers don't overlap. This check isn't really bulletproof given that
    // we are working in GPU VA space. This is primarily a sanity check and
    // security operations in code below can not assume that there is no
    // overlap at physical level.
    //
    if (_gfeIsParamOverlap(paramOffset, GFE_RESPONSE_BUF_SIZE,
                           sigBufOffset, GFE_RSA_SIGN_SIZE) ||
        _gfeIsParamOverlap(sigBufOffset, GFE_RSA_SIGN_SIZE,
                           keyBufOffset, GFE_RSA_PRIV_KEY_SIZE) ||
        _gfeIsParamOverlap(keyBufOffset, GFE_RSA_PRIV_KEY_SIZE,
                           paramOffset, GFE_RESPONSE_BUF_SIZE))
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_ILWALID_PARAM;
        goto gfeHandleReadEcid_exit;
    }

    // Read the param
    SET_MEMDESC(&memdesc, paramOffset);
    if ((status = dmaRead(_gfeRsBuf, &memdesc, 0, GFE_RESPONSE_BUF_SIZE)) != FLCN_OK)
    {
        goto gfeHandleReadEcid_exit;
    }
    pReadEcid = (gfe_read_ecid_param *)&_gfeRsBuf;

    //
    // Check for SEC2 in PROD mode.
    // If GFE is not in PROD mode (PROD HS signed) and GFE task is triggered without PROD signing of
    // gfeCryptHs, then SEC2 would go in bad state. To avoid this situation, checking
    // whether GFE task is HS signed or not.
    //
    if(!Sec2.bDebugMode)
    {
#ifdef IS_PKC_ENABLED

#ifdef RUNTIME_HS_OVL_SIG_PATCHING
        pSig = OS_SEC_HS_SIGNATURE_PKC_RUNTIME_SIG_PATCHING(OVL_INDEX_IMEM(gfeCryptHS));
#else
        // TODO : Support for non RUNTIME_HS_OVL_SIG_PATCHING but PKC signature needs to be added later on.
        ct_assert(0);
#endif // RUNTIME_HS_OVL_SIG_PATCHING

#else
        pSig = OS_SEC_HS_SIGNATURE(OVL_INDEX_IMEM(gfeCryptHS));
#endif // IS_PKC_ENABLED

        // Checking 4 Signature Dwords(even for PKC signature) is enough to make sure if gfeCryptHs is Prod Signed
        if(pSig[0] == 0 && pSig[1] == 0 && pSig[2] == 0 && pSig[3] == 0)
        {
            rc = LW95A1_GFE_READ_ECID_ERROR_HS_OVERLAY_NOT_PROD_SIGNED;
            goto gfeHandleReadEcid_exit;
        }
    }
#ifdef GFE_GEN_MODE
    LwU32 keysize = 0;

    if (pReadEcid->signMode == LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024)
    {
        keysize = GFE_RSA_PRIV_KEY_SIZE_ORI;
    }
    else if (pReadEcid->signMode == LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_2048)
    {
        keysize = (GFE_RSA_PRIV_KEY_SIZE_ORI * 2);
    }
    else
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_SIGN_MODE_UNSUPPORTED;
        goto gfeHandleReadEcid_exit;
    }

    //
    // if GEN mode, keyBufOffset points to buffer holding the private key
    // Copy only the key, no need hash (its not there yet)
    //
    SET_MEMDESC(&memdesc, keyBufOffset);
    if ((status = dmaRead(pActKeyBuf, &memdesc, 0, keysize)) != FLCN_OK)
    {
        goto gfeHandleReadEcid_exit;
    }

    // Generate a random IV first
    if (gfeGetRandNo(pIvBuf, GFE_RSA_AES_IV_SIZE_BYTES) != FLCN_OK)
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_INTERNAL_ERROR;
        goto gfeHandleReadEcid_exit;
    }

    // Encrypt the key and the corresponding SHA-2 hash
    if (_gfeEncryptKeyAndCorrespondingHash(pActKeyBuf, pIvBuf, keysize) !=
        FLCN_OK)
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_INTERNAL_ERROR;
        goto gfeHandleReadEcid_exit;
    }

    //
    // Copy the encrypted key back (along with IV) to client.
    // sigBufOffset points to the output buffer
    // Include the size of the hash as well
    //
    SET_MEMDESC(&memdesc, sigBufOffset);
    if ((status = dmaWrite(_gfeKeyBuf, &memdesc, 0,
            GFE_RSA_AES_IV_SIZE_BYTES + keysize + SHA256_HASH_BLOCK)) != FLCN_OK)
    {
        goto gfeHandleReadEcid_exit;
    }
#else
    // Check for signMode
#ifdef USE_RSA_1K
    if (pReadEcid->signMode != LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024)
#else
    if (pReadEcid->signMode != LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_2048)
#endif
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_SIGN_MODE_UNSUPPORTED;
        goto gfeHandleReadEcid_exit;
    }

    // Read the key buf along with IV
    SET_MEMDESC(&memdesc, keyBufOffset);
    if ((status = dmaRead(_gfeKeyBuf, &memdesc, 0, GFE_RSA_PRIV_KEY_SIZE)) !=
            FLCN_OK)
    {
        goto gfeHandleReadEcid_exit;
    }

    if (pReadEcid->programID == 0)
    {
        rc = LW95A1_GFE_READ_ECID_ERROR_ILWALID_PROGRAM_ID;
        goto gfeHandleReadEcid_exit;
    }

    // Read and fill in the hashed ECID
    gfeReadDeviceInfo_HAL(&GfeHal, &(pReadEcid->dinfo));

    // Initialize ucode Version
    pReadEcid->ucodeVersion = GFE_APP_VERSION;

    //
    // Don't permit any context switch until the RSA signing is done. This
    // ensures that another task can not accidentally leak RSA key from DMEM.
    // This is a temporary solution until we switch to use RSA signing with
    // SE engine. The idea would be to get HS code to write decrypted RSA key
    // directly into SE key slot and use it for signing with very little HS
    // code footprint.
    //
    lwrtosENTER_CRITICAL();
    {
        //
        // We get IV through the buffer provided by the user
        // pActKeyBuf holds the decrypted key after this call
        //
        rc = _gfeDecryptAndValidateRsaKey(pActKeyBuf, pIvBuf);

        //
        // Compute the SHA256 hash of the ECID and sign it using the
        // decrypted key
        //
        if (rc == LW95A1_GFE_READ_ECID_ERROR_NONE)
        {
            rc = _gfeComputeEcidHashAndRsaSign(pReadEcid, pActKeyBuf,
                                               _gfeSignBuf, GFE_RSA_SIGN_SIZE);
        }

        //
        // Scrub the decrypted RSA key.
        // When the function _gfeComputeEcidHashAndRsaSign -> _gfeRsaSignMessage
        // below  copies the key into the stack it scrubs the key. Apart from this
        // buffer and the stack there is no other copy of the key (or parts of it).
        //
        memset(_gfeKeyBuf, 0, GFE_RSA_PRIV_KEY_SIZE);
    }
    lwrtosEXIT_CRITICAL();

    if (rc != LW95A1_GFE_READ_ECID_ERROR_NONE)
    {
        goto gfeHandleReadEcid_exit;
    }

    // Copy signature back to client
    SET_MEMDESC(&memdesc, sigBufOffset);
    if ((status = dmaWrite(_gfeSignBuf, &memdesc, 0,  GFE_RSA_SIGN_SIZE)) !=
            FLCN_OK)
    {
        goto gfeHandleReadEcid_exit;
    }
#endif

gfeHandleReadEcid_exit:
    // Check if we had a DMA error
    if (status != FLCN_OK)
    {
        if (status == FLCN_ERR_DMA_NACK)
        {
            rc = LW95A1_GFE_READ_ECID_ERROR_DMA_NACK;
        }
        else
        {
            rc = LW95A1_GFE_READ_ECID_ERROR_DMA_UNALIGNED;
        }
    }

    //
    // In case of DMA NACK, propagate an error to the caller to start a
    // channel recovery (RC).
    //
    // This is only required for a DMA NACK, and not for DMA unaligned error,
    // since in case of unaligned DMA, the alignment checks are done before
    // DMA is attempted. So, the channel is not impacted and semaphore
    // can still be successfully released indicating completion unless
    // semaphore release itself fails for any reason but in such a case
    // determination of RC will be the responsibility of the caller
    //
    if (rc == LW95A1_GFE_READ_ECID_ERROR_DMA_NACK)
    {
        retValToSec2 = FLCN_ERR_DMA_NACK;
    }
    else
    {
        retValToSec2 = FLCN_OK;
    }

    //
    // Write the response back to the client if we didn't get a DMA NACK
    // If there was a DMA NACK, we can't do any DMA till recovery is completed
    //
    if ((pReadEcid != NULL) && (retValToSec2 != FLCN_ERR_DMA_NACK))
    {
        pReadEcid->retCode = rc;
        SET_MEMDESC(&memdesc, paramOffset);
        if (dmaWrite(pReadEcid, &memdesc, 0, GFE_RESPONSE_BUF_SIZE) == FLCN_ERR_DMA_NACK)
        {
            retValToSec2 = FLCN_ERR_DMA_NACK;
        }
    }

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSha));
    return retValToSec2;
}

/*!
 * Check whether a buffer (as represented by the GFE param and buffer size) overlaps
 * the starting address of another buffer (as represented by the GFE param).
 * ILWALID_ARGUMENT is returned if there is an overflow when rounding up
 * bufSize1 or when adding offset1 to rounded-up bufSize1.
 * WARNING: This function should not be copied or used for a similar overlap check
 * given the above behaviour about failure being returned in case of overflow.
 */
static FLCN_STATUS
_gfeDoesBufOverlapOffset
(
    LwU32   offset1,
    LwU32   bufSize1,
    LwU32   offset2,
    LwBool *pBIsOverlap
)
{
    //
    // Offset parameters represent 40-bit addresses, and are already
    // right-shifted by 8 bits to fit in 32 bits.
    // Round up bufSize1 and shift so it can be used with offset1 and offset2.
    //
    LwU32 roundedUpBufSize1 = LW_ALIGN_UP(bufSize1, 0x100);
    LwU32 endOfRangePlus1;

    //
    // The buffer sizes are the size of _gfeRsBuf (GFE_RESPONSE_BUF_SIZE),
    // _gfeKeyBuf (GFE_RSA_PRIV_KEY_SIZE) or _gfeSignBuf (GFE_RSA_SIGN_SIZE)
    // so an overflow shouldn't happen. But check just in case and return an
    // error if so.
    //
    if (roundedUpBufSize1 < bufSize1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    endOfRangePlus1 = offset1 + (roundedUpBufSize1 >> 8);

    // Check for overflow
    if (endOfRangePlus1 < offset1)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check if offset2 falls in the range
    if ((offset1 <= offset2) && (endOfRangePlus1 > offset2))
    {
        *pBIsOverlap = LW_TRUE;
        return FLCN_OK;
    }
    *pBIsOverlap = LW_FALSE;
    return FLCN_OK;
}

/*!
 * Check whether two buffers (as represented by the GFE param and buffer size) overlap.
 * WARNING: This function should not be copied or used for a similar overlap check,
 * since the buffer size is expected to be much less than 2^32-1. If large sizes that
 * cause an overflow are passed in, this function treats it as an overlap.
 */
static LwBool
_gfeIsParamOverlap
(
    LwU32 offset1,
    LwU32 bufSize1,
    LwU32 offset2,
    LwU32 bufSize2
)
{
    LwBool      bIsOverlap;

    if ((_gfeDoesBufOverlapOffset(offset1, bufSize1, offset2, &bIsOverlap) != FLCN_OK) || bIsOverlap)
    {
        return LW_TRUE;
    }

    if ((_gfeDoesBufOverlapOffset(offset2, bufSize2, offset1, &bIsOverlap) != FLCN_OK) || bIsOverlap)
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

#ifndef GFE_GEN_MODE
LwU32
_gfeComputeEcidHashAndRsaSign
(
    gfe_read_ecid_param *pReadEcid,
    LwU8 *pActKeyBuf,
    LwU8 *pSignBuf,
    LwU32 signSize
)
{
    LwU8           hash[SHA256_HASH_BLOCK_SIZE];
    SHA256_CTX     ctx;

    //
    // Compute the payload doing the sha256 of the included items.
    // Server needs to do the same
    //
    sha256Init(&ctx);
    sha256Update((LwU8 *)&(pReadEcid->serverNonce),
                sizeof(pReadEcid->serverNonce), &ctx);
    sha256Update((LwU8 *)&(pReadEcid->programID),
                sizeof(pReadEcid->programID), &ctx);
    sha256Update((LwU8 *)&(pReadEcid->signMode),
                sizeof(pReadEcid->signMode), &ctx);
    sha256Update((LwU8 *)&(pReadEcid->dinfo),
                sizeof(pReadEcid->dinfo), &ctx);
    sha256Update((LwU8 *)&(pReadEcid->ucodeVersion),
                sizeof(pReadEcid->ucodeVersion), &ctx);
    sha256Final(&ctx, hash);

    // Sign the ECID hash using private key in pActKeyBuf
    return _gfeRsaSignMessage(hash, SHA256_HASH_BLOCK_SIZE, pActKeyBuf,
                              pSignBuf, signSize);
}

/*!
 * Decrypt the key using falcon HW secret and validate it.
 * pKeyBuf has to be aligned to use SCP
 */
LwU32
_gfeDecryptAndValidateRsaKey
(
    LwU8 *pKeyBuf,
    LwU8 *pIvBuf
)
{
    LwU32       keysize = GFE_RSA_PRIV_KEY_SIZE_ORI;
    FLCN_STATUS retVal;

    // Decrypt the key
    retVal = _gfeCrypt(pKeyBuf, keysize + SHA256_HASH_BLOCK,
                       pKeyBuf, pIvBuf, LW_FALSE);

    if (retVal != FLCN_OK)
    {
        return LW95A1_GFE_READ_ECID_ERROR_ILWALID_KEY;
    }

    return _gfeCalcHashAndVerifyRsaKey(pKeyBuf, keysize);
}

/*!
 * Verify the decrypted key by comparing its hash to the expected one
 */
LwU32
_gfeCalcHashAndVerifyRsaKey
(
    LwU8 *pKeyBuf,
    LwU32 keysize
)
{
    LwU8    sha2Hash[SHA256_HASH_BLOCK];

    // Let's callwlate the hash
    sha256(sha2Hash, pKeyBuf, keysize);

    if (memcmp(sha2Hash, pKeyBuf + keysize, SHA256_HASH_BLOCK))
    {
        return LW95A1_GFE_READ_ECID_ERROR_ILWALID_KEY;
    }

    return LW95A1_GFE_READ_ECID_ERROR_NONE;
}

/*!
 * Sign the given message using an RSA-1K key.
 * This function does not scrub the key when signing is completed.
 * gfeHandleReadEcid is expected to scrub the key.
 */
LwU32
_gfeRsaSignMessage
(
    LwU8 *pMsg,
    LwU32 msgSize,
    LwU8 *pKeyBuf,
    LwU8 *pSignBuf,
    LwU32 signSize
)
{
    LwU8 *pN  = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_N]);
    LwU8 *pP  = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_P]);
    LwU8 *pQ  = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_Q]);
    LwU8 *pDP = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_DP]);
    LwU8 *pDQ = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_DQ]);
    LwU8 *pIQ = &(pKeyBuf[GFE_RSA_PRIV_KEY_OFF_IQ]);
    LwU8 buf[GFE_RSA_SIGN_SIZE];

    // Add the padding
    gfeEmsaPssEncode(pMsg, msgSize, buf, GFE_RSA_SIGN_SIZE);

    // Sign here
    gfeRsaSign(pP, pQ, pDP, pDQ, pIQ, buf, pSignBuf, pN);

    return LW95A1_GFE_READ_ECID_ERROR_NONE;
}
#endif

#ifdef GFE_GEN_MODE
/*!
 * Hash and encrypt the key using falcon HW secret
 * pKeyBuf will be atleast 16B aligned
 */
FLCN_STATUS
_gfeEncryptKeyAndCorrespondingHash
(
    LwU8 *pKeyBuf,
    LwU8 *pIvBuf,
    LwU32 keysize
)
{
    LwU8 sha2Hash[SHA256_HASH_BLOCK];

    // Lets callwlate the hash
    sha256(sha2Hash, pKeyBuf, keysize);

    memcpy(pKeyBuf + keysize, sha2Hash, SHA256_HASH_BLOCK);

    return _gfeCrypt(pKeyBuf, keysize + SHA256_HASH_BLOCK,
                        pKeyBuf, pIvBuf, LW_TRUE);
}
#endif

/*!
 * Encrypt/decrypt the key using the falcon HW secret.
 * This sets up HS mode and calls into the GFE_CRYPT_HS overlay.
 *
 * @param[in]  pSrcBuf      Pointer to the input buffer
 * @param[in]  size         Length of the buffer
 * @param[out] pDest        Pointer to the output buffer
 * @param[in]  pIvBuf       Buffer holding the IV to be used
 * @param[in]  bEncrypt     To do encryption or decryption
 */
static FLCN_STATUS
_gfeCrypt
(
    LwU8 *pSrcBuf,
    LwU32 size,
    LwU8 *pDest,
    LwU8 *pIvBuf,
    LwBool bEncrypt
)
{
    FLCN_STATUS status = FLCN_OK;

    // Enter HS mode so we can use SCP
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(gfeCryptHS));
    // Attach and load libScpCryptHs HS lib overlay in IMEM
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
 
    // Setup entry into HS mode
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(gfeCryptHS), NULL, 0, HASH_SAVING_DISABLE);

    // Do the encryption/decryption
    status = gfeCryptEntry(bEncrypt, pSrcBuf, size, pDest, pIvBuf);

    // Cleanup after returning from HS mode
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    // Detach libScpCryptHs HS lib overlay
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(gfeCryptHS));

    return status;
}
