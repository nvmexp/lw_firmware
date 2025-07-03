/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <lwstatus.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/dma.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/fence.h>
#include <lwriscv/gcc_attrs.h>
#include "partitions.h"
#include "partrpc.h"
#include "sec2_hostif.h"
#include "rmlsfm.h"
#include "class/clcba2.h"
#include "sec2/sec2ifcmn.h"
#include "crypto_system_config.h"
#include "tegra_se.h"

#define APP_MTHD_MASK (0x3F)

#define VALID_MASK_BIT(x) ((x) & APP_MTHD_MASK)

#define PAYLOAD_VALID  (BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_SRC_ADDR_LO)))      | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_SRC_ADDR_HI)))      | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_DST_ADDR_LO)))      | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_DST_ADDR_HI)))      | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_SIZE)))             | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_HI))) | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_LO))) | \
                        BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_EXELWTE))))

#define SEMAPHORE_32_BIT_VALID  (BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_SEMAPHORE_A)))                 | \
                                 BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_SEMAPHORE_B)))                 | \
                                 BIT64(VALID_MASK_BIT(SEC2_METHOD_TO_METHOD_ID(LWCBA2_SET_SEMAPHORE_PAYLOAD_LOWER))))

#define LOCAL_COPY_BUF_SIZE_BYTES (4096)

//
// The register manual lwrrently doesn't define this as an indexed field.
// Because of this, we need to define our own. We have asked the hardware
// Folks to provide an indexed field definition in the manuals, so we don't
// need to do this here.
//
#define LW_PRGNLCL_FBIF_REGIONCFG_T(i) ((i)*4+3):((i)*4)

//
// Worklaunch repurposes these aperture IDs for source and destination copy
// targets. We will save/restore its settings when we are done.
//
#define SEC2_WORKLAUNCH_DMAIDX_SRC (RM_SEC2_DMAIDX_VIRT)
#define SEC2_WORKLAUNCH_DMAIDX_DST (RM_SEC2_DMAIDX_PHYS_VID_FN0)

#define GCM_IV_MESSAGE_COUNTER_DWORD        (0) // 0th dword is the message counter
#define GCM_COUNTER_SIZE_BYTES              ((GCM_IV_SIZE_BYTES) + 4) // 96 bit IV + 4 byte AES counter

// Hardcoded key for AES GCM decryption. The same key is lwrrently hardcoded
// in RM and provided to the client.
#define GCM_KEY_BITS                        ((uint32_t)256)
#define GCM_KEY_BYTES                       ((uint32_t)(GCM_KEY_BITS >> 3))
static const LwU8 key[GCM_KEY_BYTES] =
{
    0x41, 0x2F, 0x78, 0x38,
    0x47, 0x28, 0x44, 0x3F,
    0x50, 0x62, 0x4B, 0x2B,
    0x56, 0x67, 0x53, 0x65,
    0x33, 0x70, 0x59, 0x6B,
    0x39, 0x76, 0x36, 0x73,
    0x26, 0x42, 0x24, 0x79,
    0x40, 0x48, 0x29, 0x45
};

// AES Block counter for the 128 bit IV in big endian format
#define CTR_START_PROCESS_BIGENDIAN         ((uint32_t)0x01000000)

// Used to save/restore the target aperture settings for copies in/out
typedef struct
{
    LwU32 regionCfg;
    LwU32 srcTransCfg;
    LwU32 dstTransCfg;
} SEC2_WORKLAUNCH_SAVED_APERTURE_SETTINGS;

static LwU8 copyBuf[LOCAL_COPY_BUF_SIZE_BYTES] __attribute__((aligned(256)));

static SEC2_CTX_WORKLAUNCH wl_data;

static SEC2_WORKLAUNCH_SAVED_APERTURE_SETTINGS savedApertures;

static LwBool bCryptoInitialized;

// Look in the method array for the last method that matches the address we're
// looking for
static LW_STATUS _getMethodData
(
    LwU32  methodAddr,
    LwU32 *pVal
)
{
    LwBool bFound = LW_FALSE;

    for (LwU32 i = 0; i < wl_data.methodCount; i++)
    {
        if (wl_data.methods[i][WORKLAUNCH_METHOD_ARRAY_IDX_ADDR] == methodAddr)
        {
            *pVal = wl_data.methods[i][WORKLAUNCH_METHOD_ARRAY_IDX_DATA];
            bFound = LW_TRUE;
            // keep looking until the end in case we received multiple methods
            // with the same ID. We use the last method data for that ID.
        }
    }
    if (bFound)
        return LW_OK;
    return LW_ERR_OBJECT_NOT_FOUND;
}

static LW_STATUS _releaseSemaphore
(
    LwU64 semAddr,
    LwU64 semPayload,
    GCC_ATTR_UNUSED LwBool b64Bit,
    GCC_ATTR_UNUSED LwBool bTimestamp,
    LwBool bNotifyIntr
)
{
    LwU32 payload = LwU64_LO32(semPayload);
    LW_STATUS status = dmaPa((LwU64)&payload, semAddr, 4, RM_SEC2_DMAIDX_VIRT, LW_FALSE);
    if (status != LW_OK)
    {
        return status;
    }

    // TODO-ssapre: add support for 64 bit releases and timestamps

    // Interrupt if requested
    if (bNotifyIntr)
    {
        // ensure semaphore write is visible when interrupt reaches
        riscvFenceRW();

        // ensure interrupt is sent to CPU nonstall
        LwU32 val = localRead(LW_PRGNLCL_RISCV_IRQDEST);
        val = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, val);
        localWrite(LW_PRGNLCL_RISCV_IRQDEST, val);

        val = localRead(LW_PRGNLCL_RISCV_IRQTYPE);
        val = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQTYPE, _SWGEN1, _HOST_NONSTALL, val);
        localWrite(LW_PRGNLCL_RISCV_IRQTYPE, val);

        localWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_SHIFTMASK(LW_PRGNLCL_FALCON_IRQSSET_SWGEN1));
    }

    return status;
}

static void _s_setupTargetApertures
(
    LwBool bSave
)
{
#ifdef STAGING_TEMPORARY_CODE
    LwBool bCprDst = LW_TRUE;
    LwU32 tmp;
    if (_getMethodData(LWCBA2_ENABLE_CPR_DST, &tmp) != LW_OK)
    {
        // if special method is not sent, target is non-CPR
        bCprDst = LW_FALSE;
    }
#endif

    if (bSave)
    {
        LwU32 val;
        
        val = localRead(LW_PRGNLCL_FBIF_REGIONCFG);
        savedApertures.regionCfg = val;
        val = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T,
            SEC2_WORKLAUNCH_DMAIDX_SRC, LSF_UNPROTECTED_REGION_ID, val);

        val = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T,
                SEC2_WORKLAUNCH_DMAIDX_DST, LSF_VPR_REGION_ID, val);

        #ifdef STAGING_TEMPORARY_CODE
        if (!bCprDst)
            val = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T,
                SEC2_WORKLAUNCH_DMAIDX_DST, LSF_UNPROTECTED_REGION_ID, val);
        #endif

        localWrite(LW_PRGNLCL_FBIF_REGIONCFG, val);

        val = localRead(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_SRC));
        savedApertures.srcTransCfg = val;
        val = FLD_SET_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, val);
        localWrite(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_SRC), val);

        val = localRead(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_DST));
        savedApertures.dstTransCfg = val;
        val = FLD_SET_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, val);
        localWrite(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_DST), val);        

    }
    else
    {
        localWrite(LW_PRGNLCL_FBIF_REGIONCFG, savedApertures.regionCfg);
        localWrite(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_SRC), savedApertures.srcTransCfg);
        localWrite(LW_PRGNLCL_FBIF_TRANSCFG(SEC2_WORKLAUNCH_DMAIDX_DST), savedApertures.dstTransCfg);
    }
}

/*!
 * @brief  Process a chunk of data for AES-GCM decryprion or auth tag
 *
 * @param[in]  pEconext      AES context structure to use
 * @param[in]  pSrc          Source buffer
 * @param[in]  pDst          Destination buffer
 * @param[in]  inSizeBytes   Size of the buffer to process. For auth tag, this is the total size processed since is_first = true
 * @param[out] outSizeBytes  Size of the output buffer. For auth tag, this is the size of the buffer that holds the auth tag.
 *
 * @returns LW_ERR_GENERIC   if engine_aes_process_block_locked fails
 *          LW_OK            on success
 */
static LW_STATUS _processChunk
(
    struct se_engine_aes_context* pEconext,
    LwU8 *pSrc,
    LwU8 *pDst,
    LwU32 inSizeBytes,
    LwU32 outSizeBytes
)
{
    struct se_data_params input_params = {0};    
    input_params.src = pSrc;
    input_params.dst = pDst;
    input_params.input_size = inSizeBytes;
    input_params.output_size = outSizeBytes;

    status_t res = engine_aes_process_block_locked(&input_params, pEconext);
    if (res != NO_ERROR)
    {
        return LW_ERR_GENERIC;
    }

    return LW_OK;
}

LW_STATUS _initializeSeEngAndEngCtx
(
    const struct engine_s ** ppEngine,
    struct se_engine_aes_context *pEcontext,
    uint32_t *pGcmCounterBigEndian,
    LwU8 *pComputedTag
)
{
    status_t res = ccc_select_engine(ppEngine, CCC_ENGINE_CLASS_AES, ENGINE_SE0_AES1);
    if (res != NO_ERROR)
        return LW_ERR_ILWALID_STATE;

    // Initialize Engine
    pEcontext->engine = *ppEngine;
    pEcontext->is_first = true;
    pEcontext->is_last = false; // only used during encryption/tag generation
    pEcontext->cmem = NULL;
    pEcontext->is_encrypt = false;
    pEcontext->is_hash = false; // must be FALSE (engine_aes_process_arg_check:2964])
    pEcontext->se_ks.ks_keydata = key;
    pEcontext->se_ks.ks_bitsize = GCM_KEY_BITS; // IS_VALID_AES_KEYSIZE_BITS
    pEcontext->se_ks.ks_subkey_slot = 0; // < SE_AES_MAX_KEYSLOTS

    pEcontext->aes_flags = (AES_FLAGS_DYNAMIC_KEYSLOT | AES_FLAGS_CTR_MODE_BIG_ENDIAN | AES_FLAGS_HW_CONTEXT);
    pEcontext->algorithm = TE_ALG_AES_GCM;
    pEcontext->gcm.counter = pGcmCounterBigEndian;
    pEcontext->gcm.tag = (uint8_t*) pComputedTag;

    return LW_OK;
}

/*!
 * @brief  Decrypt and copy the user provided buffer to CPR
 *
 * @param[in]  copySrc       RISCV PA for the source buffer in GMMU VA space
 * @param[in]  copyDst       RISCV PA for the destination buffer in GMMU VA space
 * @param[in]  sizeBytes     Number of bytes to decrypt and copy
 * @param[in]  pExpectedTag  Holds the expected tag passed in by client
 * @param[out] pErrorCode    Contains the class error code if return status is LW_ERR_ILWALID_ARGUMENT
 *
 * @returns LW_ERR_ILWALID_ARGUMENT if we have a user caused error
 *          LW_ERR_ILWALID_STATE    if we have an error that the user didn't cause
 *          LW_OK                   on success
 */
static LW_STATUS _decryptCopy
(
    LwU64 copySrc,
    LwU64 copyDst,
    LwU32 sizeBytes,
    LwU8 *pExpectedTag,
    LwU32 *pErrorCode
)
{
    LwU32 offset = 0;
    LWRV_STATUS status;
    struct se_engine_aes_context econtext = {0};
    engine_t *engine = NULL;
    uint32_t gcmCounterBigEndian[GCM_COUNTER_SIZE_BYTES] = {0};
    LwU8 computedTag[AUTH_TAG_BUF_SIZE_BYTES] = {0};
    LwU32 totalSizeBytes = sizeBytes; // used for tag generation
#ifdef STAGING_TEMPORARY_CODE    
    LwBool bDisableDecrypt = LW_FALSE;
#endif

    if (sizeBytes == 0)
        return LW_OK;

    if (!bCryptoInitialized)
    {
        // only need to do this once per engine reset.
        if (tegra_init_crypto_devices() != NO_ERROR)
        {
            return LW_ERR_ILWALID_STATE;
        }
        bCryptoInitialized = LW_TRUE;
    }

    // pre-increment message counter
    wl_data.iv[GCM_IV_MESSAGE_COUNTER_DWORD]++;
    if (wl_data.iv[GCM_IV_MESSAGE_COUNTER_DWORD] == 0)
    {
        *pErrorCode = 0x0000000C; // TODO-ssapre: hardcoded until SDK macro is promoted over
        return LW_ERR_ILWALID_ARGUMENT;
    }        

    memcpy((void*) gcmCounterBigEndian, (void*) wl_data.iv, GCM_IV_SIZE_BYTES);
    // start the AES block counter from 1
    gcmCounterBigEndian[3] = CTR_START_PROCESS_BIGENDIAN;

    if (_initializeSeEngAndEngCtx((const struct engine_s **)&engine, &econtext, gcmCounterBigEndian, computedTag) != LW_OK)
        return LW_ERR_ILWALID_STATE;
    
    while (sizeBytes != 0)
    {
        LwU32 copySize = (sizeBytes > LOCAL_COPY_BUF_SIZE_BYTES) ? LOCAL_COPY_BUF_SIZE_BYTES : sizeBytes;

        status = dmaPa((LwU64)copyBuf, copySrc + offset, copySize, SEC2_WORKLAUNCH_DMAIDX_SRC, LW_TRUE /*Read to TCM*/);
        if (status != LWRV_OK)
        {
            if (status == LWRV_ERR_DMA_NACK)
            {
                *pErrorCode = LWCBA2_ERROR_DECRYPT_COPY_DMA_NACK;
                return LW_ERR_ILWALID_ARGUMENT;
            }
            return LW_ERR_ILWALID_STATE;
        }

#ifdef STAGING_TEMPORARY_CODE
        LwU32 tmp;
        if (_getMethodData(LWCBA2_DISABLE_DECRYPT, &tmp) == LW_OK)
        {
            bDisableDecrypt = LW_TRUE;
        }

        if (!bDisableDecrypt)
#endif
        {
            if (_processChunk(&econtext, copyBuf, copyBuf, copySize, copySize) != LW_OK)
            {
                return LW_ERR_ILWALID_STATE;
            }
            status = dmaPa((LwU64)copyBuf, copyDst + offset, copySize, SEC2_WORKLAUNCH_DMAIDX_DST, LW_FALSE /*Write from TCM*/);
            if (status != LWRV_OK)
            {
                if (status == LWRV_ERR_DMA_NACK)
                {
                    *pErrorCode = LWCBA2_ERROR_DECRYPT_COPY_DMA_NACK;
                    return LW_ERR_ILWALID_ARGUMENT;
                }
                return LW_ERR_ILWALID_STATE;
            }            
        }
#ifdef STAGING_TEMPORARY_CODE
        else
        {
            status = dmaPa((LwU64)copyBuf, copyDst + offset, copySize, SEC2_WORKLAUNCH_DMAIDX_DST, LW_FALSE /*Write from TCM*/);
            if (status != LWRV_OK)
            {
                if (status == LWRV_ERR_DMA_NACK)
                {
                    *pErrorCode = LWCBA2_ERROR_DECRYPT_COPY_DMA_NACK;
                    return LW_ERR_ILWALID_ARGUMENT;
                }
                return LW_ERR_ILWALID_STATE;
            }               
        }
        sizeBytes -= copySize;
        offset    += copySize;
    }

    if (!bDisableDecrypt)
#endif
    {
#ifdef STAGING_TEMPORARY_CODE
        LwBool bDisableAuthTag = LW_FALSE;
        LwU32 tmp;
        if (_getMethodData(LWCBA2_DISABLE_AUTH_TAG, &tmp) == LW_OK)
        {
            bDisableAuthTag = LW_TRUE;
        }        

        if (!bDisableAuthTag)
#endif            
        {
            // Modify econtext for tag generation
            econtext.is_first = false;
            econtext.is_last = true;
            econtext.aes_flags |= (AES_FLAGS_AE_FINALIZE_TAG | AES_FLAGS_HW_CONTEXT);

            if (_processChunk(&econtext, NULL, computedTag, totalSizeBytes, AUTH_TAG_BUF_SIZE_BYTES) != LW_OK)
                return LW_ERR_ILWALID_STATE;

            if (memcmp(pExpectedTag, computedTag, AUTH_TAG_BUF_SIZE_BYTES) != 0)
            {
                *pErrorCode = LWCBA2_ERROR_DECRYPT_COPY_AUTH_TAG_MISMATCH;
                return LW_ERR_ILWALID_ARGUMENT;
            }
        } 
    }

    return LW_OK;
}

void processWorklaunch(void)
{
    WL_REQUEST *req = rpcGetRequest();
    WL_REPLY *rep = rpcGetReply();

    LwU32 sizeBytes = 0;
    LwU32 copySrcLo = 0;
    LwU32 copySrcHi = 0;
    LwU32 copyDstLo = 0;
    LwU32 copyDstHi = 0;
    LwU32 tagSrcHi = 0;
    LwU32 tagSrcLo = 0;
    LwU32 execute = 0;
    LwBool bNotify = LW_FALSE;
    LwBool bNotifyOnBegin = LW_FALSE;
    LwU32 semA, semB, semPayloadLo, semD;
    LwU32 bExelwteTrigger = LW_FALSE;
    LwBool bAperturesDirty = LW_TRUE; // don't trust apertures set up by other partition

    rep->errorCode = LWCBA2_ERROR_NONE;

    memcpy(&wl_data, &(req->worklaunchParams), sizeof(wl_data));

    if (_getMethodData(LWCBA2_SEMAPHORE_D, &semD) != LW_OK)
    {
        bExelwteTrigger = LW_TRUE;
    }

    _s_setupTargetApertures(LW_TRUE /*bSave*/);
    bAperturesDirty = LW_FALSE; // we overwrote the DMA apertures

    if (bExelwteTrigger)
    {
        // fetch all the methods we expect with an execute
        if (!(wl_data.validMask & PAYLOAD_VALID))
        {
            rep->errorCode = LWCBA2_ERROR_MISSING_METHODS;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_SIZE, &sizeBytes);
        if ((sizeBytes & DRF_MASK(LWCBA2_DECRYPT_COPY_SIZE_DATA)) != sizeBytes)
        {
            rep->errorCode = LWCBA2_ERROR_MISALIGNED_SIZE;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_SRC_ADDR_LO, &copySrcLo);
        if ((copySrcLo & DRF_MASK(LWCBA2_DECRYPT_COPY_SRC_ADDR_LO_DATA)) != copySrcLo)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_SRC_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_SRC_ADDR_HI, &copySrcHi);
        if ((copySrcHi & DRF_MASK(LWCBA2_DECRYPT_COPY_SRC_ADDR_HI_DATA)) != copySrcHi)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_SRC_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_DST_ADDR_LO, &copyDstLo);
        if ((copyDstLo & DRF_MASK(LWCBA2_DECRYPT_COPY_DST_ADDR_LO_DATA)) != copyDstLo)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_DEST_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_DST_ADDR_HI, &copyDstHi);
        if ((copyDstHi & DRF_MASK(LWCBA2_DECRYPT_COPY_DST_ADDR_HI_DATA)) != copyDstHi)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_DEST_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_LO, &tagSrcLo);
        if ((tagSrcLo & DRF_MASK(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_LO_DATA)) != tagSrcLo)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_AUTH_TAG_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        _getMethodData(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_HI, &tagSrcHi);
        if ((tagSrcHi & DRF_MASK(LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_HI_DATA)) != tagSrcHi)
        {
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_AUTH_TAG_ADDR_MISALIGNED_POINTER;
            goto processWorklaunch_return;
        }

        // we may have semaphore methods.
        _getMethodData(LWCBA2_SEMAPHORE_A, &semA);
        _getMethodData(LWCBA2_SEMAPHORE_B, &semB);
        _getMethodData(LWCBA2_SET_SEMAPHORE_PAYLOAD_LOWER, &semPayloadLo);
    }
    else
    {
        // Standalone semaphore trigger

        // expect valid 32 bit payload
        if (!(wl_data.validMask & SEMAPHORE_32_BIT_VALID))
        {
            rep->errorCode = LWCBA2_ERROR_MISSING_METHODS;
            goto processWorklaunch_return;
        }
        _getMethodData(LWCBA2_SEMAPHORE_A, &semA);
        _getMethodData(LWCBA2_SEMAPHORE_B, &semB);
        _getMethodData(LWCBA2_SET_SEMAPHORE_PAYLOAD_LOWER, &semPayloadLo);

        LwU64 semAddr = ((LwU64)semA << 32) | semB;
        LwU64 semPayload = semPayloadLo;
        if (_releaseSemaphore(semAddr, semPayload, LW_FALSE /*32 bit*/,
                              LW_FALSE /* bTimestamp */,
                              FLD_TEST_DRF(CBA2, _SEMAPHORE_D, _NOTIFY_INTR, _ENABLE, semD)) != LW_OK)
        {
            rep->errorCode = LWCBA2_ERROR_SEMAPHORE_RELEASE_DMA_NACK;
            goto processWorklaunch_return;
        }
        // we're done
        goto processWorklaunch_success;
    }

    _getMethodData(LWCBA2_EXELWTE, &execute);

    if (FLD_TEST_DRF(CBA2, _EXELWTE, _NOTIFY, _ENABLE, execute))
    {
        bNotify = LW_TRUE;
        bNotifyOnBegin = FLD_TEST_DRF(CBA2, _EXELWTE, _NOTIFY_ON, _BEGIN, execute);
    }

    if (bNotify)
    {
        // sanity check we got all the methods while we're at it
        if (!(wl_data.validMask & SEMAPHORE_32_BIT_VALID))
        {
            rep->errorCode = LWCBA2_ERROR_MISSING_METHODS;
            goto processWorklaunch_return;
        }

        if (bNotifyOnBegin)
        {
            LwU64 semAddr = ((LwU64)semA << 32) | semB;
            LwU64 semPayload = semPayloadLo;

            if (_releaseSemaphore(semAddr, semPayload, LW_FALSE /*32 bit*/,
                                  LW_FALSE /* bTimestamp */,
                                  FLD_TEST_DRF(CBA2, _EXELWTE, _NOTIFY_INTR, _ENABLE, execute)) != LW_OK)
            {
                rep->errorCode = LWCBA2_ERROR_SEMAPHORE_RELEASE_DMA_NACK;
                goto processWorklaunch_return;
            }
        }
    }

    // DMA in expected tag
    LwU64 tagSrc = tagSrcLo | ((LwU64)tagSrcHi << 32);
    LwU8 expectedTag[AUTH_TAG_BUF_SIZE_BYTES];
    LWRV_STATUS rvStatus = dmaPa((LwU64)expectedTag, tagSrc, AUTH_TAG_BUF_SIZE_BYTES, SEC2_WORKLAUNCH_DMAIDX_SRC, LW_TRUE /*Read to TCM*/);
    if (rvStatus != LWRV_OK)
    {
        if (rvStatus == LWRV_ERR_DMA_NACK)
            rep->errorCode = LWCBA2_ERROR_DECRYPT_COPY_DMA_NACK;
        else
            rep->errorCode = 0xFF; // TODO-ssapre: add error codes for internal errors
        goto processWorklaunch_return;
    }  

    LwU32 errorCode;
    LwU64 copySrc = copySrcLo | ((LwU64)copySrcHi << 32);
    LwU64 copyDst = copyDstLo | ((LwU64)copyDstHi << 32);

    LW_STATUS status = _decryptCopy(copySrc, copyDst, sizeBytes, expectedTag, &errorCode);
    if (status != LW_OK)
    {
        if (status == LW_ERR_ILWALID_ARGUMENT)
        {
            // user caused error
            rep->errorCode = errorCode;
        }
        else
        {
            rep->errorCode = 0xFF; // TODO-ssapre: add error codes for internal errors
        }
        goto processWorklaunch_return;
    }

    if (FLD_TEST_DRF(CBA2, _EXELWTE, _FLUSH_DISABLE, _FALSE, execute))
    {
        // membar.sys after copy finishes.
        riscvFenceRW();
    }

    // release semaphore if requested
    if (bNotify && !bNotifyOnBegin)
    {
        // We've already sanity tested that we received all semaphore methods.
        LwU64 semAddr = ((LwU64)semA << 32) | semB;
        LwU64 semPayload = semPayloadLo;

        if (_releaseSemaphore(semAddr, semPayload, LW_FALSE /*32 bit*/,
                              LW_FALSE /* bTimestamp */,
                              FLD_TEST_DRF(CBA2, _EXELWTE, _NOTIFY_INTR, _ENABLE, execute)) != LW_OK)
        {
            rep->errorCode = LWCBA2_ERROR_SEMAPHORE_RELEASE_DMA_NACK;
            goto processWorklaunch_return;
        }
        goto processWorklaunch_success;
    }

processWorklaunch_success:
    rep->errorCode = LWCBA2_ERROR_NONE;
    memcpy(rep->iv, wl_data.iv, GCM_IV_SIZE_BYTES);

processWorklaunch_return:
    // restore apertures if we overwrote them
    if (!bAperturesDirty)
        _s_setupTargetApertures(LW_FALSE /*bSave*/);
    return;
}

// C "entry" to partition, has stack and liblwriscv
int partition_wl_main(GCC_ATTR_UNUSED LwU64 arg0,
                      GCC_ATTR_UNUSED LwU64 arg1,
                      GCC_ATTR_UNUSED LwU64 arg2,
                      GCC_ATTR_UNUSED LwU64 arg3,
                      GCC_ATTR_UNUSED LwU64 partition_origin)
{
    LW_STATUS status = LW_OK;

    { // raise PLM to l3
        uint64_t data64;
        data64 = csr_read(LW_RISCV_CSR_SRSP);
        data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL3, data64);
        csr_write(LW_RISCV_CSR_SRSP, data64);
    }

    processWorklaunch();

    partitionSwitchNoreturn(status, 0, 0, 0, PARTITION_ID_RTOS, partition_origin);

    // We should not reach this code ever
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
    return 0;
}

void partition_wl_trap_handler(void)
{
    pPrintf("In Partition acr trap handler at 0x%lx, cause 0x%lx val 0x%lx\n",
            csr_read(LW_RISCV_CSR_SEPC),
            csr_read(LW_RISCV_CSR_SCAUSE),
            csr_read(LW_RISCV_CSR_STVAL));

    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    while(1)
    {
    }
}
