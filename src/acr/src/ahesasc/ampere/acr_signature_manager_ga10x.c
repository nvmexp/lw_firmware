/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_signature_manager_ga10x.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_static_inline_functions.h"
#include "acr_signature.h"
#include "sec2shamutex.h"
#include <falc_debug.h>
#include "acr_objacr.h"
#include "acr_objacrlib.h"
#include "acr_objsha.h"
#include "acr_objse.h"
#include "dev_se_seb.h"

#ifdef PKC_LS_RSA3K_KEYS_GA10X
#include "acr_lsverif_keys_rsa3k_dbg_ga10x.h"
#include "acr_lsverif_keys_rsa3k_prod_ga10x.h"
#endif

#ifdef LIB_CCC_PRESENT
#include "tegra_cdev.h"
#include "tegra_se.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_rsa.h"
#endif

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"
#include "config/g_sha_private.h"
#include "config/g_se_private.h"

#define GET_MOST_SIGNIFICANT_BIT(keySize)      (keySize > 0 ? ((keySize - 1) & 7) : 0)
#define GET_ENC_MESSAGE_SIZE_BYTE(keySize)     (keySize + 7) >> 3;
#define PKCS1_MGF1_COUNTER_SIZE_BYTE           (4)
#define RSA_PSS_PADDING_ZEROS_SIZE_BYTE        (8)

extern LwU32         g_copyBufferA[];
extern LwU32         g_copyBufferB[];
extern ACR_DMA_PROP  g_dmaProp;
extern LwBool        g_bIsDebug;

#if defined(AHESASC) && defined(NEW_WPR_BLOBS)

LwU8 g_lsSignature[ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE]  ATTR_OVLY(".data") ATTR_ALIGNED(256);
LwU8 g_hashBuffer[ACR_LS_HASH_BUFFER_SIZE_MAX_BYTE]        ATTR_OVLY(".data") ATTR_ALIGNED(4);
LwU8 g_hashResult[ACR_LS_HASH_BUFFER_SIZE_MAX_BYTE]        ATTR_OVLY(".data") ATTR_ALIGNED(4);

static ACR_STATUS _acrShaRunSingleTaskCommon(LwU32 algoId, LwU32 sizeByte, SHA_TASK_CONFIG *pTaskCfg, LwU8 *pBufOut) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrShaRunSingleTaskDmem(LwU32 algoId, LwU8 *pBufIn, LwU8 *pBufOut, LwU32 sizeByte) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrEnforcePkcs1Mgf1(LwU8 *pMask, LwU32  maskedLen, LwU8  *pSeed, LwU32 seedLen, LwU8 shaId) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrGetSignatureValidaionId(LwU32 hashAlgoVer, LwU32 hashAlgo, LwU32 sigAlgoVer, LwU32 sigAlgo, LwU32 sigPaddingType, LwU32 *pValidationId) ATTR_OVLY(OVL_NAME);
static void       _endianRevert(LwU8 *pBuf, LwU32 bufSize) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrPlainTextSizeCheck(LwU32 algoId, LwU32 size, LwU32 offset, LwU32 *pSizeNew, LwU32 *pOffsetNew, LwBool *pNeedAdjust) ATTR_OVLY(OVL_NAME);

// Relative local static functions for ID = LS_SIG_VALIDATION_ID_1 and LS_SIG_VALIDATION_ID_2

static ACR_STATUS _acrRsaDecryptLsSignature(LwU8 *pSignatureEnc, LwU32 keySizeBit, LwU8 *pSignatureDec) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrGeneratePlainTextHash(LwU32 falconId, LwU32 algoId, LwU32 binOffset,
                                                LwU32 binSize, LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrSignatureValidationHandler(LwU32 falconId, LwU32 hashAlgoId, LwU8 *pPlainTextHash, LwU8 *pDecSig,
                                                     LwU32 keySizeBit, LwU32 hashSizeByte, LwU32 saltSizeByte) ATTR_OVLY(OVL_NAME);

static ACR_STATUS _acrGeneratePlainTextHashNonAligned(LwU32 falconId, LwU32 algoId, LwU32 binOffset, LwU32 binSize,
                                                          LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode,
                                                          LwU32  offsetNew, LwU32  sizeNew) ATTR_OVLY(OVL_NAME);

#ifdef PKC_LS_RSA3K_KEYS_GA10X
static ACR_STATUS _seInitializationRsaKeyProd_GA10X(LwU32 *pModulusProd, LwU32 *pExponentProd,
                                                    LwU32 *pMpProd, LwU32 *pRsqrProd) ATTR_OVLY(OVL_NAME);

static ACR_STATUS _seInitializationRsaKeyDbg_GA10X(LwU32 *pModulusDbg, LwU32 *pExponentDbg,
                                                   LwU32 *pMpDbg, LwU32 *pRsqrDbg) ATTR_OVLY(OVL_NAME);
#endif

/*!
 *@brief To validate LS signature with PKC-LS support and new wpr blob defines
 */
ACR_STATUS
acrValidateLsSignature_GA10X
(
    LwU32  falconId,
    LwU8  *pSignature,
    LwU32  binSize,
    LwU32  binOffset,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool bIsUcode
)
{
    LwU32  hashAlgoVer;
    LwU32  hashAlgo;
    LwU32  sigAlgoVer;
    LwU32  sigAlgo;
    LwU32  sigPaddingType;
    LwU32  lsSigValidationId;
    LwU8  *pPlainTextHash = g_hashResult;
    ACR_STATUS status = ACR_ERROR_UNKNOWN;
    LwU32  hashAlgoId = SHA_ID_LAST;
    LwU32  hashSizeByte;
    LwU32  saltSizeByte;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO_VER, pLsfUcodeDescWrapper, (void *)&hashAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO, pLsfUcodeDescWrapper, (void *)&hashAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_VER, pLsfUcodeDescWrapper, (void *)&sigAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO, pLsfUcodeDescWrapper, (void *)&sigAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_PADDING_TYPE, pLsfUcodeDescWrapper, (void *)&sigPaddingType));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetSignatureValidaionId(hashAlgoVer, hashAlgo, sigAlgoVer, sigAlgo, sigPaddingType, &lsSigValidationId));

    if (lsSigValidationId == LS_SIG_VALIDATION_ID_1)
    {
        hashAlgoId = SHA_ID_SHA_256;
        hashSizeByte = SHA_256_HASH_SIZE_BYTE;
        saltSizeByte = SHA_256_HASH_SIZE_BYTE;
    }
    else if (lsSigValidationId == LS_SIG_VALIDATION_ID_2)
    {
        hashAlgoId = SHA_ID_SHA_384;
        hashSizeByte = SHA_384_HASH_SIZE_BYTE;
        saltSizeByte = SHA_384_HASH_SIZE_BYTE;
    }
    else
    {
       // do nothing, let below switch check to filter sig validation id.
    }

    switch (lsSigValidationId)
    {
        case LS_SIG_VALIDATION_ID_1: // Id 1 : rsa3k-pss padding with SHA256 hash algorithm
        case LS_SIG_VALIDATION_ID_2: // Id 2 : rsa3k-pss padding with SHA384 hash algorithm
        {
            // Get plain text hash
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGeneratePlainTextHash(falconId, hashAlgoId, binOffset, binSize,
                                                                            pPlainTextHash, pLsfUcodeDescWrapper, bIsUcode));

            // decrypt signature with RSA3K public keySize
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrRsaDecryptLsSignature(pSignature, RSA_KEY_SIZE_3072_BIT, pSignature));

            // call validation handler
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrSignatureValidationHandler(falconId, hashAlgoId, pPlainTextHash, pSignature,
                                              RSA_KEY_SIZE_3072_BIT, hashSizeByte, saltSizeByte));
        }
        break;

        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

/*!
 * @ Get signature validation ID depends on signature parameters
 */
static ACR_STATUS
_acrGetSignatureValidaionId
(
    LwU32 hashAlgoVer,
    LwU32 hashAlgo,
    LwU32 sigAlgoVer,
    LwU32 sigAlgo,
    LwU32 sigPaddingType,
    LwU32 *pValidationId
)
{
    ACR_STATUS status = ACR_OK;

    if (pValidationId == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pValidationId  = LS_SIG_VALIDATION_ID_ILWALID;

    if (hashAlgoVer    == LSF_HASH_ALGO_VERSION_1        &&
        sigAlgoVer     == LSF_SIGNATURE_ALGO_VERSION_1   &&
        sigAlgo        == LSF_SIGNATURE_ALGO_TYPE_RSA3K  &&
        sigPaddingType == LSF_SIGNATURE_ALGO_PADDING_TYPE_PSS)
    {
        if (hashAlgo == LSF_HASH_ALGO_TYPE_SHA256)
        {
            *pValidationId  = LS_SIG_VALIDATION_ID_1;
        }
        else if (hashAlgo == LSF_HASH_ALGO_TYPE_SHA384)
        {
            *pValidationId  = LS_SIG_VALIDATION_ID_2;
        }
        else
        {
            status = ACR_ERROR_LS_SIG_VERIF_FAIL;
        }
    }
    else
    {
        status = ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    return status;
}

/*!
 * @ The common part for SHA operation with single task configuration.
 */
static ACR_STATUS
_acrShaRunSingleTaskCommon
(
    SHA_ID           algoId,
    LwU32            sizeByte,
    SHA_TASK_CONFIG *pTaskCfg,
    LwU8            *pBufOut
)
{
    ACR_STATUS status = ACR_ERROR_UNKNOWN;
    SHA_CONTEXT shaCtx;

    shaCtx.algoId  = algoId;
    shaCtx.msgSize = sizeByte;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaOperationInit_HAL(pSha, &shaCtx));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask_HAL(pSha, &shaCtx, pTaskCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pBufOut, LW_TRUE));

    return ACR_OK;
}

/*!
 * @ To apply SHA operation on plain text saved on DMEM
 */
static ACR_STATUS
_acrShaRunSingleTaskDmem
(
    SHA_ID  algoId,
    LwU8   *pBufIn,
    LwU8   *pBufOut,
    LwU32   sizeByte
)
{
    SHA_TASK_CONFIG taskCfg;
    ACR_STATUS  status, tmpStatus;

    // SoftReset will clear mutex, don't run any soft reset during SHA operation.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION));

    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.dmaIdx        = 0;
    taskCfg.size          = sizeByte;
    taskCfg.addr          = (LwU64)((LwU32)pBufIn);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrShaRunSingleTaskCommon(algoId, sizeByte, &taskCfg, pBufOut));

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/*!
 * @brief Function to revert endian
 */
static void
_endianRevert
(
    LwU8 *pBuf,
    LwU32 bufSize
)
{
    LwU32 i;
    LwU8 tmp;

    for (i = 0; i < (bufSize >> 1); i++)
    {
        tmp = pBuf[i];
        pBuf[i] = pBuf[bufSize - 1 - i];
        pBuf[bufSize - 1 - i] = tmp;
    }
}

/*!
 *@brief Enforce PKCS1_MGF1 process
 */
static ACR_STATUS
_acrEnforcePkcs1Mgf1
(
    LwU8  *pMask,
    LwU32  maskedLen,
    LwU8  *pSeed,
    LwU32  seedLen,
    LwU8   shaId
)
{
    SHA_CONTEXT     shaCtx;
    SHA_TASK_CONFIG taskCfg;
    ACR_STATUS      status, tmpStatus = ACR_ERROR_UNKNOWN;
    LwU32           outLen = 0, i, j;
    LwU8           *pFinalBuf = pMask;
    LwU8           *pTempStart = (LwU8 *)g_copyBufferB; // use g_copyBufferB as temp buffer in SHA operation process
    LwU8           *pTemp = pTempStart;
    LwU32           mdLen;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetHashSizeByte_HAL(pSha, shaId, &mdLen));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION));

    //
    // Because g_copyBuffer is declared as external variable, can't use sizeof(g_copyBufferB) to get g_copyBuffer's size.
    // In acr.h, we define copy buffer A/B size to help current function have a buffer size sanity check.
    // seedLen either equals to 0 or hash size(e.g sh256 hash  size is 32 bytes).
    // g_coyBufferB size is 1024 bytes; and its size is large enough to cover all combinations.
    // We still need to add size sanity check to pass coverity check.
    //
    if ((seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE) > COPY_BUFFER_B_SIZE_BYTE)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // copy seed to temp buffer
    for (i = 0; i < seedLen; i++)
    {
        pTemp[i] = pSeed[i];
    }

    pTemp += seedLen;

    // prepare SHA task configuration
    shaCtx.algoId         = shaId;

    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.dmaIdx        = 0;
    taskCfg.size          = seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE;
    taskCfg.addr          = (LwU64)((LwU32)pTempStart);

    i = 0;
    while (outLen < maskedLen)
    {
        // set counter value
        pTemp[0] = (LwU8)((i >> 24) & 0xFF);
        pTemp[1] = (LwU8)((i >> 16) & 0xFF);
        pTemp[2] = (LwU8)((i >> 8)  & 0xFF);
        pTemp[3] = (LwU8)( i        & 0xFF);

        // Need to set msgSize for each SHA operation.
        shaCtx.msgSize        = seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE;
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

        if (outLen + mdLen <= maskedLen)
        {
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pFinalBuf, LW_TRUE));
            outLen += mdLen;
            pFinalBuf += mdLen;
        }
        else
        {
            //The last operation, so we can overwrite pTempStart
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pTempStart, LW_TRUE));

            for (j = 0; j < maskedLen - outLen; j++)
            {
                pFinalBuf[j] = pTempStart[j];
            }
            outLen = maskedLen;
        }
        i++;
    }

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/*!
 * @brief For multi task SHA operation, the non-last task size need to aligned with SHA block size.
 *        This function is used take a snity check for message size; and also return aligned offset
 *        and aligned size for onward SHA 256 operation.
 */
static ACR_STATUS
_acrPlainTextSizeCheck
(
    LwU32   algoId,
    LwU32   size,
    LwU32   offset,
    LwU32  *pSizeNew,
    LwU32  *pOffsetNew,
    LwBool *pNeedAdjust
)
{
    ACR_STATUS status = ACR_ERROR_ILWALID_ARGUMENT;
    LwU32 blockSizeByte;
    LwU32 blockCount;
    LwU64 temp64;
    LwU32 sizeRemiander;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetBlockSizeByte_HAL(pSha, algoId, &blockSizeByte));

    //
    // TODO: eddichang Lwrrently RiscV image is not 64 byte aligned but for SHA 256, SHA engine needs to divide total
    // message size into several 64-byte chunks. For SHA 256 multi task operation, except last block; the intermediate
    // block size must be 64-byte aligned. We need to compute aligned offset and size for SHA256 operation on FB.
    //

    // Assume "size" must be larger than "blockSizeByte"
    blockCount = (size / blockSizeByte);

    if (blockCount == 0)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    sizeRemiander = (size % blockSizeByte);

    if (sizeRemiander)
    {
       temp64 = acrMultipleUnsigned32(blockCount, blockSizeByte);
       *pSizeNew = (LwU32)LwU64_LO32(temp64);

       temp64 = (LwU64)offset + temp64;
       *pOffsetNew = (LwU32)LwU64_LO32(temp64);

       *pNeedAdjust = LW_TRUE;
    }

    return ACR_OK;
}

/*!
 * @brief To generate hash for the message, which size is not aligned with SHA operation requirement.

  ------------|=================|-- binOffset
   |    |     |                 | |
   |    |     |     Bin data    | \
   |    |     |      (FB)       |  => SHA operation : Task -1
   | sizeNew  |                 | /
   |    |     |                 | |
binSize |     |                 | |
   |    ----- |=================|-- offsetNew
   |          |                 |
   |          | remainder data  | ===================|
   |          |      (FB)       |                    |
  ------------|=================|                    |
                                                     |
                                                     |
  |==================(copy to DMEM)==================|
  |
  |
  |==> |==================| ---
       |                  |  |
       |  remiander data  |  |
       |     (DMEM)       |  |
       |                  |  |
       |==================|  |
       |    falconId      |  |
       |     (DMEM)       |  \
       |==================|   => SHA operation : Task-2
       |    ucodeVer      |  /
       |     (DMEM)       |  |
       |==================|  |
       |     ucodeId      |  |
       |     (DMEM)       |  |
       |==================|  |
       | Dep Map Context  |  |
       |     (DMEM)       |  |
       |==================| ---
*/
static ACR_STATUS
_acrGeneratePlainTextHashNonAligned
(
    LwU32 falconId,
    LwU32 algoId,
    LwU32 binOffset,
    LwU32 binSize,
    LwU8 *pBufOut,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool bIsUcode,
    LwU32  offsetNew,
    LwU32  sizeNew
)
{
    LwU64            binAddr;
    LwU32            plainTextSize;
    SHA_CONTEXT      shaCtx;
    SHA_TASK_CONFIG  taskCfg;
    ACR_STATUS       status, tmpStatus;
    LwU32            depMapSize;
    LwU8            *pBufferByte = (LwU8 *)g_copyBufferA;
    LwU8            *pBufferByteOriginal = pBufferByte;
    LwU32            remainderSize;
    LwU32            temp32;

    // The aligned size is always less than original size.
    if (sizeNew >= binSize)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    remainderSize = binSize - sizeNew;

    // Get dependency map size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
                                                                        pLsfUcodeDescWrapper, (void *)&depMapSize));

    //First, sanity check for buffer size, to make sure buffer is large enough to store notifications, dep map context and reminader bytes in FB
    if ((remainderSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize) > ACR_FALCON_BLOCK_SIZE_IN_BYTES)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    //
    // Callwlating binary size, we concatenate falconID, ucodeVer and ucodeID to code and data section for ID 1 hash generation process.
    // Bin data is stored at FB; but concatenations are stored at DMEM, so we need to specify 2 tasks to complete SHA hash opertion.
    // (binary data) || fpfHwVersion || ucodeId
    //
    plainTextSize = binSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION));
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));

    // Task-1: SHA hash callwlation of bin data stored at FB but only for aligned size region
    binAddr               = ((g_dmaProp.wprBase << 8) + binOffset);
    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_FB;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.dmaIdx        = g_dmaProp.ctxDma;
    taskCfg.size          = sizeNew;
    taskCfg.addr          = binAddr;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

    //
    // Leverage g_copyBufferA as temp buffer to save remainder bytes in FB first.
    // Always read last 256 byte to temp buffer and concanate falocnId, ucodeVer, ucodeId and dep map context
    // into the buffer.
    //
    if ((acrIssueDma_HAL(pAcr, pBufferByte, LW_FALSE, offsetNew, ACR_FALCON_BLOCK_SIZE_IN_BYTES,
            ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != ACR_FALCON_BLOCK_SIZE_IN_BYTES)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // move pointer to the last byte and copy notification and dep map context
    pBufferByte += remainderSize;

    // Don't use doword pointer to copy dword value because pBuffreByte could NOT be dword aigned pointer.
    // In case we cast a pointer which contained addrees is NOT dword aligned poineter
    if (!acrlibMemcpy_HAL(pAcrlib, (void *)pBufferByte, (void *)&falconId, sizeof(falconId)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(falconId);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&temp32));
    if (!acrlibMemcpy_HAL(pAcrlib, (void *)pBufferByte, (void *)&temp32, sizeof(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                        pLsfUcodeDescWrapper,  (void *)&temp32));

    if (!acrlibMemcpy_HAL(pAcrlib, (void *)pBufferByte, (void *)&temp32, sizeof(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
                                                                        pLsfUcodeDescWrapper,  (void *)pBufferByte));

    binAddr               = (LwU64)((LwU32)pBufferByteOriginal);

    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_FALSE;
    taskCfg.dmaIdx        = 0;
    taskCfg.size          = remainderSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;
    taskCfg.addr          = binAddr;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

    // read hash result out
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pBufOut, LW_TRUE));

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/*!
 * @brief Plain text hash generation for ID = LS_SIG_VALIDATION_ID_1 (Binary data + ucodeVer + ucodeId + Dep Map Context)
 *
   ---------|=================|-- binOffset
   |        |                 | |
   |        |     Bin data    | |
   |        |      (FB)       | |
   |        |                 | |
   |        |                 | \
   |        |                 |    => SHA operation : Task -1
binSize     |                 | /
   |        |                 | |
   |        |                 | |
   |        |                 | |
   |        |                 | |
  ----------|=================| --

   |==================| ---
   |    falconId      |  |
   |     (DMEM)       |  |
   |==================|  |
   |    ucodeVer      |  |
   |     (DMEM)       |  \
   |==================|    => SHA operation : Task-2
   |     ucodeId      |  /
   |     (DMEM)       |  |
   |==================|  |
   | Dep Map Context  |  |
   |     (DMEM)       |  |
   |==================| ---

 */
static ACR_STATUS
_acrGeneratePlainTextHash
(
    LwU32 falconId,
    LwU32 algoId,
    LwU32 binOffset,
    LwU32 binSize,
    LwU8 *pBufOut,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool bIsUcode
)
{
    LwU64            binAddr;
    LwU32            plainTextSize;
    SHA_CONTEXT      shaCtx;
    SHA_TASK_CONFIG  taskCfg;
    ACR_STATUS       status, tmpStatus;
    LwU32            depMapSize;
    LwU32           *pBufferDw;
    LwBool           bNeedAdjust = LW_FALSE;
    LwU32            sizeNew;
    LwU32            offsetNew;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrPlainTextSizeCheck(algoId, binSize, binOffset, &sizeNew, &offsetNew, &bNeedAdjust));

    if (bNeedAdjust)
    {
        return _acrGeneratePlainTextHashNonAligned(falconId, algoId, binOffset, binSize, pBufOut, pLsfUcodeDescWrapper, bIsUcode, offsetNew, sizeNew);
    }

    //
    // Callwlating binary size, we concatenate falconID, ucodeVer and ucodeID to code and data section for ID 1 hash generation process.
    // Bin data is stored at FB; but concatenations are stored at DMEM, so we need to specify 2 tasks to complete SHA hash opertion.
    // (binary data) || fpfHwVersion || ucodeId
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
                                                                        pLsfUcodeDescWrapper, (void *)&depMapSize));

    plainTextSize = binSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION));
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));

    // Task-1: SHA hash callwlation of bin data stored at FB
    binAddr               = ((g_dmaProp.wprBase << 8) + binOffset);
    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_FB;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.dmaIdx        = g_dmaProp.ctxDma;
    taskCfg.size          = binSize;
    taskCfg.addr          = binAddr;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

    //
    // Task-2 : Leverage g_copyBufferA as temp buffer to save concatenation components
    // bin || falconId || lsUcodeVer || lsUocdeId || depMapCtx
    //
    pBufferDw    = (LwU32 *)g_copyBufferA;
    pBufferDw[0] = falconId;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&pBufferDw[1]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                      pLsfUcodeDescWrapper,  (void *)&pBufferDw[2]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
                                                                      pLsfUcodeDescWrapper,  (void *)&pBufferDw[3]));

    binAddr               = (LwU64)((LwU32)pBufferDw);
    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_FALSE;
    taskCfg.dmaIdx        = 0;
    taskCfg.size          = LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;
    taskCfg.addr          = binAddr;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

    // read hash result out
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pBufOut, LW_TRUE));

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/*!
 *@brief Signature handler for ID = LS_SIG_VALIDATION_ID_1 (RSA3K-PSS padding + SHA-256 hash) and LS_SIG_VALIDATION_ID_2 (RSA3K-PSS padding + SHA-384 hash)
 */
static ACR_STATUS
_acrSignatureValidationHandler
(
    LwU32 falconId,
    LwU32 hashAlgoId,
    LwU8 *pPlainTextHash,
    LwU8 *pDecSig,
    LwU32 keySizeBit,
    LwU32 hashSizeByte,
    LwU32 saltSizeByte
)
{
    ACR_STATUS status = ACR_ERROR_UNKNOWN;
    LwU32  msBits = GET_MOST_SIGNIFICANT_BIT(keySizeBit);
    LwU32  emLen = GET_ENC_MESSAGE_SIZE_BYTE(keySizeBit);
    LwU32  maskedDBLen, i, j;
    LwU8  *pEm = pDecSig;
    LwU8  *pDb = NULL;
    LwU8  *pHashGoldenStart = NULL;
    LwU8  *pHashGolden = NULL;
    LwU8  *pHash = NULL;
    LwU32  msgGoldenLen;

    if (pEm[0] & (0xFF << msBits))
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (msBits == 0)
    {
       pEm++;
       emLen --;
    }

    if (emLen < hashSizeByte + 2)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (saltSizeByte > emLen - hashSizeByte - 2)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (pEm[emLen - 1] != 0xbc)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    // g_copyBufferA is used as a temp buffer
    pDb = (LwU8 *)g_copyBufferA;

    maskedDBLen = emLen - hashSizeByte - 1;
    pHash = pEm + maskedDBLen;

    if (_acrEnforcePkcs1Mgf1(pDb, maskedDBLen, pHash, hashSizeByte, hashAlgoId) != ACR_OK)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    for (i = 0; i < maskedDBLen; i++)
    {
        pDb[i] ^= pEm[i];
    }

    if (msBits)
    {
        pDb[0] &= 0xFF >> (8 - msBits);
    }

    // find first non-zero value in pDb[]
    for (i = 0; pDb[i] == 0 && i < (maskedDBLen - 1); i++);

    if (pDb[i++] != 0x1)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    if ((maskedDBLen - i) != saltSizeByte)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    // prepare golden hash
    pHashGoldenStart = g_hashBuffer;
    pHashGolden = pHashGoldenStart;

    // Insert 8 zeros
    for (j = 0 ; j < RSA_PSS_PADDING_ZEROS_SIZE_BYTE ; j++)
    {
        pHashGolden[j] = 0;
    }
    pHashGolden += RSA_PSS_PADDING_ZEROS_SIZE_BYTE;

    // Insert SHA hash result
    for (j =0; j < hashSizeByte; j++)
    {
        pHashGolden[j] = pPlainTextHash[j];
    }
    pHashGolden += hashSizeByte;

    msgGoldenLen = hashSizeByte + RSA_PSS_PADDING_ZEROS_SIZE_BYTE;

    if (maskedDBLen - i)
    {
        for (j = 0; j < maskedDBLen - i; j++)
        {
            pHashGolden[j] = pDb[i + j];
        }
        msgGoldenLen += (maskedDBLen - i);
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrShaRunSingleTaskDmem(hashAlgoId, pHashGoldenStart,
                                                               pHashGoldenStart, msgGoldenLen))

    // compare signature
    if(acrMemcmp(pHash, pHashGoldenStart, hashSizeByte))
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    return ACR_OK;
}

#ifdef PKC_LS_RSA3K_KEYS_GA10X
static ACR_STATUS
_seInitializationRsaKeyProd_GA10X
(
    LwU32 *pModulusProd,
    LwU32 *pExponentProd,
    LwU32 *pMpProd,
    LwU32 *pRsqrProd
)
{
    if (pModulusProd == NULL || pExponentProd == NULL ||
        pMpProd      == NULL || pRsqrProd     == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pModulusProd[0]   = 0x97d2077d; pModulusProd[1]   = 0xcb1bdd87; pModulusProd[2]   = 0xea017216; pModulusProd[3]   = 0x2a58daa6;
    pModulusProd[4]   = 0xb41fbb9c; pModulusProd[5]   = 0xd1815254; pModulusProd[6]   = 0x884a9782; pModulusProd[7]   = 0xe1a6e4ad;
    pModulusProd[8]   = 0x8d131aff; pModulusProd[9]   = 0xee466176; pModulusProd[10]  = 0xbfa3b809; pModulusProd[11]  = 0x2952c580;
    pModulusProd[12]  = 0x1db2dd1e; pModulusProd[13]  = 0xfcc90b6a; pModulusProd[14]  = 0xf5cb0b37; pModulusProd[15]  = 0x45333dad;
    pModulusProd[16]  = 0x06f73c16; pModulusProd[17]  = 0x2acf9e56; pModulusProd[18]  = 0x8d3a3a5c; pModulusProd[19]  = 0xda89fe4d;
    pModulusProd[20]  = 0x309f0093; pModulusProd[21]  = 0x00b4969c; pModulusProd[22]  = 0x72c7e0c9; pModulusProd[23]  = 0x6f0a91e0;
    pModulusProd[24]  = 0xf6666fe6; pModulusProd[25]  = 0x4f03e2f7; pModulusProd[26]  = 0x9fbf9999; pModulusProd[27]  = 0x8640ae4e;
    pModulusProd[28]  = 0xf19da7e4; pModulusProd[29]  = 0x2016632e; pModulusProd[30]  = 0xaf28b3f3; pModulusProd[31]  = 0xe5635aae;
    pModulusProd[32]  = 0xa12ce1db; pModulusProd[33]  = 0xef2054d3; pModulusProd[34]  = 0x50abe007; pModulusProd[35]  = 0xc6a4447c;
    pModulusProd[36]  = 0x5daa4f1c; pModulusProd[37]  = 0xa1d23b2c; pModulusProd[38]  = 0x7aebafa2; pModulusProd[39]  = 0x4172ded3;
    pModulusProd[40]  = 0x0a2fe0b7; pModulusProd[41]  = 0x9c630bf0; pModulusProd[42]  = 0x06200523; pModulusProd[43]  = 0x27379c83;
    pModulusProd[44]  = 0x857bab7c; pModulusProd[45]  = 0x6278ea3d; pModulusProd[46]  = 0xf72531ed; pModulusProd[47]  = 0xd91c7ad3;
    pModulusProd[48]  = 0x151655c6; pModulusProd[49]  = 0x06c0578d; pModulusProd[50]  = 0x621769cf; pModulusProd[51]  = 0xd43b2ae9;
    pModulusProd[52]  = 0xa8618aa1; pModulusProd[53]  = 0x04db96b7; pModulusProd[54]  = 0x491b55a4; pModulusProd[55]  = 0x0f06f12a;
    pModulusProd[56]  = 0xfb622f9c; pModulusProd[57]  = 0x4abbc16e; pModulusProd[58]  = 0x6e946c1f; pModulusProd[59]  = 0x8856a346;
    pModulusProd[60]  = 0xeb238023; pModulusProd[61]  = 0xace79ca9; pModulusProd[62]  = 0x56eb7878; pModulusProd[63]  = 0x885938cc;
    pModulusProd[64]  = 0x92a437db; pModulusProd[65]  = 0xb195706d; pModulusProd[66]  = 0x8547eb0f; pModulusProd[67]  = 0xb584df78;
    pModulusProd[68]  = 0x972b572c; pModulusProd[69]  = 0xc49abf79; pModulusProd[70]  = 0x80e3a025; pModulusProd[71]  = 0x355d4307;
    pModulusProd[72]  = 0x3532328b; pModulusProd[73]  = 0xd345fd12; pModulusProd[74]  = 0x86054da2; pModulusProd[75]  = 0x49ca5313;
    pModulusProd[76]  = 0x14550c80; pModulusProd[77]  = 0xc541f5cc; pModulusProd[78]  = 0x3d6b606d; pModulusProd[79]  = 0x966e8468;
    pModulusProd[80]  = 0x639b0b6b; pModulusProd[81]  = 0xde5b07e7; pModulusProd[82]  = 0xbbc231ad; pModulusProd[83]  = 0xf3981414;
    pModulusProd[84]  = 0xc77e6979; pModulusProd[85]  = 0x027800f7; pModulusProd[86]  = 0x2d7fd7a8; pModulusProd[87]  = 0x49b8ec2e;
    pModulusProd[88]  = 0xec466c05; pModulusProd[89]  = 0x82e5acbe; pModulusProd[90]  = 0x15c61888; pModulusProd[91]  = 0xabe6c6d5;
    pModulusProd[92]  = 0x6f98bdd5; pModulusProd[93]  = 0xd3935670; pModulusProd[94]  = 0x9e6158ce; pModulusProd[95]  = 0x9da1c2f7;
    pModulusProd[96]  = 0x00000000; pModulusProd[97]  = 0x00000000; pModulusProd[98]  = 0x00000000; pModulusProd[99]  = 0x00000000;
    pModulusProd[100] = 0x00000000; pModulusProd[101] = 0x00000000; pModulusProd[102] = 0x00000000; pModulusProd[103] = 0x00000000;
    pModulusProd[104] = 0x00000000; pModulusProd[105] = 0x00000000; pModulusProd[106] = 0x00000000; pModulusProd[107] = 0x00000000;
    pModulusProd[108] = 0x00000000; pModulusProd[109] = 0x00000000; pModulusProd[110] = 0x00000000; pModulusProd[111] = 0x00000000;
    pModulusProd[112] = 0x00000000; pModulusProd[113] = 0x00000000; pModulusProd[114] = 0x00000000; pModulusProd[115] = 0x00000000;
    pModulusProd[116] = 0x00000000; pModulusProd[117] = 0x00000000; pModulusProd[118] = 0x00000000; pModulusProd[119] = 0x00000000;
    pModulusProd[120] = 0x00000000; pModulusProd[121] = 0x00000000; pModulusProd[122] = 0x00000000; pModulusProd[123] = 0x00000000;
    pModulusProd[124] = 0x00000000; pModulusProd[125] = 0x00000000; pModulusProd[126] = 0x00000000; pModulusProd[127] = 0x00000000;

    pExponentProd[0]   = 0x00010001; pExponentProd[1]   = 0x00000000; pExponentProd[2]   = 0x00000000; pExponentProd[3]   = 0x00000000;
    pExponentProd[4]   = 0x00000000; pExponentProd[5]   = 0x00000000; pExponentProd[6]   = 0x00000000; pExponentProd[7]   = 0x00000000;
    pExponentProd[8]   = 0x00000000; pExponentProd[9]   = 0x00000000; pExponentProd[10]  = 0x00000000; pExponentProd[11]  = 0x00000000;
    pExponentProd[12]  = 0x00000000; pExponentProd[13]  = 0x00000000; pExponentProd[14]  = 0x00000000; pExponentProd[15]  = 0x00000000;
    pExponentProd[16]  = 0x00000000; pExponentProd[17]  = 0x00000000; pExponentProd[18]  = 0x00000000; pExponentProd[19]  = 0x00000000;
    pExponentProd[20]  = 0x00000000; pExponentProd[21]  = 0x00000000; pExponentProd[22]  = 0x00000000; pExponentProd[23]  = 0x00000000;
    pExponentProd[24]  = 0x00000000; pExponentProd[25]  = 0x00000000; pExponentProd[26]  = 0x00000000; pExponentProd[27]  = 0x00000000;
    pExponentProd[28]  = 0x00000000; pExponentProd[29]  = 0x00000000; pExponentProd[30]  = 0x00000000; pExponentProd[31]  = 0x00000000;
    pExponentProd[32]  = 0x00000000; pExponentProd[33]  = 0x00000000; pExponentProd[34]  = 0x00000000; pExponentProd[35]  = 0x00000000;
    pExponentProd[36]  = 0x00000000; pExponentProd[37]  = 0x00000000; pExponentProd[38]  = 0x00000000; pExponentProd[39]  = 0x00000000;
    pExponentProd[40]  = 0x00000000; pExponentProd[41]  = 0x00000000; pExponentProd[42]  = 0x00000000; pExponentProd[43]  = 0x00000000;
    pExponentProd[44]  = 0x00000000; pExponentProd[45]  = 0x00000000; pExponentProd[46]  = 0x00000000; pExponentProd[47]  = 0x00000000;
    pExponentProd[48]  = 0x00000000; pExponentProd[49]  = 0x00000000; pExponentProd[50]  = 0x00000000; pExponentProd[51]  = 0x00000000;
    pExponentProd[52]  = 0x00000000; pExponentProd[53]  = 0x00000000; pExponentProd[54]  = 0x00000000; pExponentProd[55]  = 0x00000000;
    pExponentProd[56]  = 0x00000000; pExponentProd[57]  = 0x00000000; pExponentProd[58]  = 0x00000000; pExponentProd[59]  = 0x00000000;
    pExponentProd[60]  = 0x00000000; pExponentProd[61]  = 0x00000000; pExponentProd[62]  = 0x00000000; pExponentProd[63]  = 0x00000000;
    pExponentProd[64]  = 0x00000000; pExponentProd[65]  = 0x00000000; pExponentProd[66]  = 0x00000000; pExponentProd[67]  = 0x00000000;
    pExponentProd[68]  = 0x00000000; pExponentProd[69]  = 0x00000000; pExponentProd[70]  = 0x00000000; pExponentProd[71]  = 0x00000000;
    pExponentProd[72]  = 0x00000000; pExponentProd[73]  = 0x00000000; pExponentProd[74]  = 0x00000000; pExponentProd[75]  = 0x00000000;
    pExponentProd[76]  = 0x00000000; pExponentProd[77]  = 0x00000000; pExponentProd[78]  = 0x00000000; pExponentProd[79]  = 0x00000000;
    pExponentProd[80]  = 0x00000000; pExponentProd[81]  = 0x00000000; pExponentProd[82]  = 0x00000000; pExponentProd[83]  = 0x00000000;
    pExponentProd[84]  = 0x00000000; pExponentProd[85]  = 0x00000000; pExponentProd[86]  = 0x00000000; pExponentProd[87]  = 0x00000000;
    pExponentProd[88]  = 0x00000000; pExponentProd[89]  = 0x00000000; pExponentProd[90]  = 0x00000000; pExponentProd[91]  = 0x00000000;
    pExponentProd[92]  = 0x00000000; pExponentProd[93]  = 0x00000000; pExponentProd[94]  = 0x00000000; pExponentProd[95]  = 0x00000000;
    pExponentProd[96]  = 0x00000000; pExponentProd[97]  = 0x00000000; pExponentProd[98]  = 0x00000000; pExponentProd[99]  = 0x00000000;
    pExponentProd[100] = 0x00000000; pExponentProd[101] = 0x00000000; pExponentProd[102] = 0x00000000; pExponentProd[103] = 0x00000000;
    pExponentProd[104] = 0x00000000; pExponentProd[105] = 0x00000000; pExponentProd[106] = 0x00000000; pExponentProd[107] = 0x00000000;
    pExponentProd[108] = 0x00000000; pExponentProd[109] = 0x00000000; pExponentProd[110] = 0x00000000; pExponentProd[111] = 0x00000000;
    pExponentProd[112] = 0x00000000; pExponentProd[113] = 0x00000000; pExponentProd[114] = 0x00000000; pExponentProd[115] = 0x00000000;
    pExponentProd[116] = 0x00000000; pExponentProd[117] = 0x00000000; pExponentProd[118] = 0x00000000; pExponentProd[119] = 0x00000000;
    pExponentProd[120] = 0x00000000; pExponentProd[121] = 0x00000000; pExponentProd[122] = 0x00000000; pExponentProd[123] = 0x00000000;
    pExponentProd[124] = 0x00000000; pExponentProd[125] = 0x00000000; pExponentProd[126] = 0x00000000; pExponentProd[127] = 0x00000000;

    pMpProd[0]   = 0xf3a4162b; pMpProd[1]   = 0x944e7aa9; pMpProd[2]   = 0xae263ede; pMpProd[3]   = 0xd55869cc;
    pMpProd[4]   = 0xf38f66f4; pMpProd[5]   = 0xd3777ddc; pMpProd[6]   = 0x49b48ebd; pMpProd[7]   = 0x7b99cd0a;
    pMpProd[8]   = 0x36705fd2; pMpProd[9]   = 0x9b5e992f; pMpProd[10]  = 0x9a0b857c; pMpProd[11]  = 0x5772c509;
    pMpProd[12]  = 0x34ba11cb; pMpProd[13]  = 0x9418dc76; pMpProd[14]  = 0xb5008d95; pMpProd[15]  = 0xdfc86d08;
    pMpProd[16]  = 0xd090f817; pMpProd[17]  = 0xbc2261d5; pMpProd[18]  = 0xfcc17a2e; pMpProd[19]  = 0xd88da8b1;
    pMpProd[20]  = 0xad67aec8; pMpProd[21]  = 0x64acfd49; pMpProd[22]  = 0x13d8ad51; pMpProd[23]  = 0x3aa0833a;
    pMpProd[24]  = 0x844e9aed; pMpProd[25]  = 0xe2f547e9; pMpProd[26]  = 0x337f0f19; pMpProd[27]  = 0xe6bd10e8;
    pMpProd[28]  = 0xfd6c4164; pMpProd[29]  = 0xa601c175; pMpProd[30]  = 0x9f37e85d; pMpProd[31]  = 0x9a5bc3ef;
    pMpProd[32]  = 0x547c5eac; pMpProd[33]  = 0x356376d9; pMpProd[34]  = 0xfca36d79; pMpProd[35]  = 0x625f89d4;
    pMpProd[36]  = 0x4b426b59; pMpProd[37]  = 0xb15a68f9; pMpProd[38]  = 0x567c8bf3; pMpProd[39]  = 0x6f5b07d1;
    pMpProd[40]  = 0x6f7d8d3e; pMpProd[41]  = 0xb2080463; pMpProd[42]  = 0x7c739fde; pMpProd[43]  = 0x6e39a65b;
    pMpProd[44]  = 0x37eb85ad; pMpProd[45]  = 0xa90ca1bc; pMpProd[46]  = 0x03fd0992; pMpProd[47]  = 0x2e1d34c8;
    pMpProd[48]  = 0x4445a550; pMpProd[49]  = 0x3bbc9f25; pMpProd[50]  = 0xdfd0bb6e; pMpProd[51]  = 0xc8d7982a;
    pMpProd[52]  = 0x9abb639c; pMpProd[53]  = 0xd52c153d; pMpProd[54]  = 0x3eb5289e; pMpProd[55]  = 0x0408c285;
    pMpProd[56]  = 0x2f1d43e7; pMpProd[57]  = 0xb17be13a; pMpProd[58]  = 0x5207f2e0; pMpProd[59]  = 0x04e597ec;
    pMpProd[60]  = 0x9f05b0cf; pMpProd[61]  = 0x437cd38d; pMpProd[62]  = 0x9f88952d; pMpProd[63]  = 0x255ea16f;
    pMpProd[64]  = 0xfad42f13; pMpProd[65]  = 0x503ba423; pMpProd[66]  = 0x4b85f82b; pMpProd[67]  = 0x734bc52c;
    pMpProd[68]  = 0x4fc7208e; pMpProd[69]  = 0x56237ddb; pMpProd[70]  = 0x24718671; pMpProd[71]  = 0x4d30dd53;
    pMpProd[72]  = 0x87ca112a; pMpProd[73]  = 0x0d0ae5ff; pMpProd[74]  = 0xecd1c969; pMpProd[75]  = 0x1b9accbe;
    pMpProd[76]  = 0x1a323ee0; pMpProd[77]  = 0x351e9668; pMpProd[78]  = 0x059c46bb; pMpProd[79]  = 0x0610fcf3;
    pMpProd[80]  = 0xe838f18b; pMpProd[81]  = 0xe7ace78c; pMpProd[82]  = 0xa29b3a6c; pMpProd[83]  = 0xbe1f6373;
    pMpProd[84]  = 0x3e2631c4; pMpProd[85]  = 0xc2b3383a; pMpProd[86]  = 0x6b0c93bd; pMpProd[87]  = 0x1e4f7312;
    pMpProd[88]  = 0x7ffc3a64; pMpProd[89]  = 0x0eb89669; pMpProd[90]  = 0xfb0a3604; pMpProd[91]  = 0xbcf51658;
    pMpProd[92]  = 0x6f696b20; pMpProd[93]  = 0x1eef5872; pMpProd[94]  = 0x050a1603; pMpProd[95]  = 0x8dec1876;
    pMpProd[96]  = 0x00000000; pMpProd[97]  = 0x00000000; pMpProd[98]  = 0x00000000; pMpProd[99]  = 0x00000000;
    pMpProd[100] = 0x00000000; pMpProd[101] = 0x00000000; pMpProd[102] = 0x00000000; pMpProd[103] = 0x00000000;
    pMpProd[104] = 0x00000000; pMpProd[105] = 0x00000000; pMpProd[106] = 0x00000000; pMpProd[107] = 0x00000000;
    pMpProd[108] = 0x00000000; pMpProd[109] = 0x00000000; pMpProd[110] = 0x00000000; pMpProd[111] = 0x00000000;
    pMpProd[112] = 0x00000000; pMpProd[113] = 0x00000000; pMpProd[114] = 0x00000000; pMpProd[115] = 0x00000000;
    pMpProd[116] = 0x00000000; pMpProd[117] = 0x00000000; pMpProd[118] = 0x00000000; pMpProd[119] = 0x00000000;
    pMpProd[120] = 0x00000000; pMpProd[121] = 0x00000000; pMpProd[122] = 0x00000000; pMpProd[123] = 0x00000000;
    pMpProd[124] = 0x00000000; pMpProd[125] = 0x00000000; pMpProd[126] = 0x00000000; pMpProd[127] = 0x00000000;

    pRsqrProd[0]   = 0x224f8a58; pRsqrProd[1]   = 0x7fd35e69; pRsqrProd[2]   = 0xd5428432;  pRsqrProd[3]  = 0x3933bd96;
    pRsqrProd[4]   = 0x0e8f82cf; pRsqrProd[5]   = 0x0c60c807; pRsqrProd[6]   = 0xd40f7235;  pRsqrProd[7]  = 0xcb8dba01;
    pRsqrProd[8]   = 0x80e177fc; pRsqrProd[9]   = 0xfb4d1497; pRsqrProd[10]  = 0x5e32401d; pRsqrProd[11]  = 0xf37a8ec1;
    pRsqrProd[12]  = 0x2909af14; pRsqrProd[13]  = 0xe59d12bf; pRsqrProd[14]  = 0x73e00c2f; pRsqrProd[15]  = 0x42eec21b;
    pRsqrProd[16]  = 0xb200df01; pRsqrProd[17]  = 0xb1843678; pRsqrProd[18]  = 0x345e638a; pRsqrProd[19]  = 0x5c52d9f1;
    pRsqrProd[20]  = 0xddce30c9; pRsqrProd[21]  = 0xeedb7ba3; pRsqrProd[22]  = 0xbb012224; pRsqrProd[23]  = 0xbfda4fc8;
    pRsqrProd[24]  = 0x2e344848; pRsqrProd[25]  = 0xeb5975e3; pRsqrProd[26]  = 0x612b3234; pRsqrProd[27]  = 0x79989124;
    pRsqrProd[28]  = 0xc2c5eafb; pRsqrProd[29]  = 0xd0354953; pRsqrProd[30]  = 0xfaec9d0d; pRsqrProd[31]  = 0x59f0a7f6;
    pRsqrProd[32]  = 0x7662bcc2; pRsqrProd[33]  = 0x7b9e91a6; pRsqrProd[34]  = 0x0ab4c31e; pRsqrProd[35]  = 0xda31da4a;
    pRsqrProd[36]  = 0x1108ce7c; pRsqrProd[37]  = 0x0b91dd1d; pRsqrProd[38]  = 0x556efe91; pRsqrProd[39]  = 0x33e48326;
    pRsqrProd[40]  = 0x49292556; pRsqrProd[41]  = 0xd402ea6a; pRsqrProd[42]  = 0x9507d63c; pRsqrProd[43]  = 0x15cc5032;
    pRsqrProd[44]  = 0x6df5a84d; pRsqrProd[45]  = 0xdddb2a85; pRsqrProd[46]  = 0x1b3e3714; pRsqrProd[47]  = 0x73618845;
    pRsqrProd[48]  = 0xeab7226f; pRsqrProd[49]  = 0x879732de; pRsqrProd[50]  = 0xcfe96975; pRsqrProd[51]  = 0x238898af;
    pRsqrProd[52]  = 0x5da73178; pRsqrProd[53]  = 0x590d49fa; pRsqrProd[54]  = 0xd86caf30; pRsqrProd[55]  = 0x68d4665c;
    pRsqrProd[56]  = 0xf78b4c62; pRsqrProd[57]  = 0xa23bc48e; pRsqrProd[58]  = 0x27963d8a; pRsqrProd[59]  = 0x665e4d64;
    pRsqrProd[60]  = 0x6f24efec; pRsqrProd[61]  = 0xbbfe2bb1; pRsqrProd[62]  = 0x45e7ddc8; pRsqrProd[63]  = 0x3e15b000;
    pRsqrProd[64]  = 0xfdde2554; pRsqrProd[65]  = 0x07214438; pRsqrProd[66]  = 0xc5a26daa; pRsqrProd[67]  = 0x93e5e828;
    pRsqrProd[68]  = 0xec0e8801; pRsqrProd[69]  = 0x8392b22c; pRsqrProd[70]  = 0xfc95516e; pRsqrProd[71]  = 0xf7836245;
    pRsqrProd[72]  = 0xaf722c1e; pRsqrProd[73]  = 0x3f894d6f; pRsqrProd[74]  = 0x1794d543; pRsqrProd[75]  = 0x9dddd415;
    pRsqrProd[76]  = 0x3dbc5449; pRsqrProd[77]  = 0xa81ee21c; pRsqrProd[78]  = 0x24e7a33a; pRsqrProd[79]  = 0xea34e845;
    pRsqrProd[80]  = 0x5cb9c616; pRsqrProd[81]  = 0x41e67128; pRsqrProd[82]  = 0x8bde36ca; pRsqrProd[83]  = 0x0617f59a;
    pRsqrProd[84]  = 0xbf99b654; pRsqrProd[85]  = 0x4d111429; pRsqrProd[86]  = 0x44b414ed; pRsqrProd[87]  = 0x2d71d3ec;
    pRsqrProd[88]  = 0xac452c61; pRsqrProd[89]  = 0xac1bf8fb; pRsqrProd[90]  = 0x7e5ce60b; pRsqrProd[91]  = 0x3890e9cf;
    pRsqrProd[92]  = 0xfe90d686; pRsqrProd[93]  = 0x7c7fd6ae; pRsqrProd[94]  = 0xe2bab52e; pRsqrProd[95]  = 0x9022bcab;
    pRsqrProd[96]  = 0x00000000; pRsqrProd[97]  = 0x00000000; pRsqrProd[98]  = 0x00000000; pRsqrProd[99]  = 0x00000000;
    pRsqrProd[100] = 0x00000000; pRsqrProd[101] = 0x00000000; pRsqrProd[102] = 0x00000000; pRsqrProd[103] = 0x00000000;
    pRsqrProd[104] = 0x00000000; pRsqrProd[105] = 0x00000000; pRsqrProd[106] = 0x00000000; pRsqrProd[107] = 0x00000000;
    pRsqrProd[108] = 0x00000000; pRsqrProd[109] = 0x00000000; pRsqrProd[110] = 0x00000000; pRsqrProd[111] = 0x00000000;
    pRsqrProd[112] = 0x00000000; pRsqrProd[113] = 0x00000000; pRsqrProd[114] = 0x00000000; pRsqrProd[115] = 0x00000000;
    pRsqrProd[116] = 0x00000000; pRsqrProd[117] = 0x00000000; pRsqrProd[118] = 0x00000000; pRsqrProd[119] = 0x00000000;
    pRsqrProd[120] = 0x00000000; pRsqrProd[121] = 0x00000000; pRsqrProd[122] = 0x00000000; pRsqrProd[123] = 0x00000000;
    pRsqrProd[124] = 0x00000000; pRsqrProd[125] = 0x00000000; pRsqrProd[126] = 0x00000000; pRsqrProd[127] = 0x00000000;

    return ACR_OK;
}

static ACR_STATUS
_seInitializationRsaKeyDbg_GA10X
(
    LwU32 *pModulusDbg,
    LwU32 *pExponentDbg,
    LwU32 *pMpDbg,
    LwU32 *pRsqrDbg
)
{
    if (pModulusDbg == NULL || pExponentDbg == NULL ||
        pMpDbg      == NULL || pRsqrDbg     == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pModulusDbg[0]   = 0x6404288d; pModulusDbg[1]   = 0xc3efcb20; pModulusDbg[2]   = 0xd0aae02c; pModulusDbg[3]  = 0x06adc570;
    pModulusDbg[4]   = 0xb23c4ce6; pModulusDbg[5]   = 0x46ed46ab; pModulusDbg[6]   = 0x4392a48c; pModulusDbg[7]  = 0x9b3aa00d;
    pModulusDbg[8]   = 0xb1102d00; pModulusDbg[9]   = 0xf4d95994; pModulusDbg[10]  = 0x5a5ff012; pModulusDbg[11] = 0x2d5a61e7;
    pModulusDbg[12]  = 0xfeab8251; pModulusDbg[13]  = 0x51dda5a3; pModulusDbg[14]  = 0xc475d928; pModulusDbg[15] = 0xf1036983;
    pModulusDbg[16]  = 0xd9207b7b; pModulusDbg[17]  = 0xc3aed23b; pModulusDbg[18]  = 0x6333437d; pModulusDbg[19] = 0x27c6c5f6;
    pModulusDbg[20]  = 0xb140f5ad; pModulusDbg[21]  = 0x02741a48; pModulusDbg[22]  = 0x5327dc45; pModulusDbg[23] = 0x61b927d2;
    pModulusDbg[24]  = 0xb050f67f; pModulusDbg[25]  = 0x4e67771b; pModulusDbg[26]  = 0x404c2842; pModulusDbg[27] = 0x6e40169d;
    pModulusDbg[28]  = 0xdb57e5c5; pModulusDbg[29]  = 0x4afb15a1; pModulusDbg[30]  = 0x3dc2ba0c; pModulusDbg[31] = 0x6e0d7696;
    pModulusDbg[32]  = 0x39e98c31; pModulusDbg[33]  = 0xd0e70fc9; pModulusDbg[34]  = 0x3d7c6406; pModulusDbg[35] = 0x9315dd98;
    pModulusDbg[36]  = 0x6d7a9422; pModulusDbg[37]  = 0xf3b1259b; pModulusDbg[38]  = 0x56911699; pModulusDbg[39] = 0xf3b34a4a;
    pModulusDbg[40]  = 0xe3145b36; pModulusDbg[41]  = 0x1c2f0b42; pModulusDbg[42]  = 0xc74cbbf8; pModulusDbg[43] = 0xb750c689;
    pModulusDbg[44]  = 0x6ca212ed; pModulusDbg[45]  = 0x3110a1ce; pModulusDbg[46]  = 0x75a9bcbf; pModulusDbg[47] = 0x19ee7f65;
    pModulusDbg[48]  = 0x420cb3fe; pModulusDbg[49]  = 0x0771cda1; pModulusDbg[50]  = 0x267fdb1f; pModulusDbg[51] = 0x693ffa8e;
    pModulusDbg[52]  = 0x6baf8baf; pModulusDbg[53]  = 0x4300a914; pModulusDbg[54]  = 0x97478df6; pModulusDbg[55] = 0x78f8655b;
    pModulusDbg[56]  = 0x95ebc445; pModulusDbg[57]  = 0x621fe604; pModulusDbg[58]  = 0x6099c478; pModulusDbg[59] = 0xc17fc515;
    pModulusDbg[60]  = 0x33875702; pModulusDbg[61]  = 0x66b4e135; pModulusDbg[62]  = 0xc2234dcf; pModulusDbg[63] = 0x5416f007;
    pModulusDbg[64]  = 0x06e1ec20; pModulusDbg[65]  = 0xc27f41e5; pModulusDbg[66]  = 0x15eaa226; pModulusDbg[67] = 0x94da66c8;
    pModulusDbg[68]  = 0xb3ed717c; pModulusDbg[69]  = 0x05f82c96; pModulusDbg[70]  = 0x1f649983; pModulusDbg[71] = 0x1b161a04;
    pModulusDbg[72]  = 0x4653a11f; pModulusDbg[73]  = 0x08c44fa9; pModulusDbg[74]  = 0xff5c792f; pModulusDbg[75] = 0x630a22e3;
    pModulusDbg[76]  = 0xbebc1e1d; pModulusDbg[77]  = 0x999fc596; pModulusDbg[78]  = 0xef16b029; pModulusDbg[79] = 0x9dbff0fc;
    pModulusDbg[80]  = 0x1b7eb531; pModulusDbg[81]  = 0xd441f918; pModulusDbg[82]  = 0xeae40439; pModulusDbg[83] = 0x88b5f9e9;
    pModulusDbg[84]  = 0x6f4d308a; pModulusDbg[85]  = 0x35449529; pModulusDbg[86]  = 0xc8337ce3; pModulusDbg[87] = 0x508eae45;
    pModulusDbg[88]  = 0xe059f539; pModulusDbg[89]  = 0x3ddf3dca; pModulusDbg[90]  = 0x598f19a7; pModulusDbg[91] = 0x2f12abe4;
    pModulusDbg[92]  = 0x6af4164f; pModulusDbg[93]  = 0x25a7f417; pModulusDbg[94]  = 0xac4a5570; pModulusDbg[95] = 0xc6f1a2c0;
    pModulusDbg[96]  = 0x00000000; pModulusDbg[97]  = 0x00000000; pModulusDbg[98]  = 0x00000000; pModulusDbg[99] = 0x00000000;
    pModulusDbg[100] = 0x00000000; pModulusDbg[101] = 0x00000000; pModulusDbg[102] = 0x00000000; pModulusDbg[103] = 0x00000000;
    pModulusDbg[104] = 0x00000000; pModulusDbg[105] = 0x00000000; pModulusDbg[106] = 0x00000000; pModulusDbg[107] = 0x00000000;
    pModulusDbg[108] = 0x00000000; pModulusDbg[109] = 0x00000000; pModulusDbg[110] = 0x00000000; pModulusDbg[111] = 0x00000000;
    pModulusDbg[112] = 0x00000000; pModulusDbg[113] = 0x00000000; pModulusDbg[114] = 0x00000000; pModulusDbg[115] = 0x00000000;
    pModulusDbg[116] = 0x00000000; pModulusDbg[117] = 0x00000000; pModulusDbg[118] = 0x00000000; pModulusDbg[119] = 0x00000000;
    pModulusDbg[120] = 0x00000000; pModulusDbg[121] = 0x00000000; pModulusDbg[122] = 0x00000000; pModulusDbg[123] = 0x00000000;
    pModulusDbg[124] = 0x00000000; pModulusDbg[125] = 0x00000000; pModulusDbg[126] = 0x00000000; pModulusDbg[127] = 0x00000000;

    pExponentDbg[0]   = 0x00010001; pExponentDbg[1]   = 0x00000000; pExponentDbg[2]   = 0x00000000; pExponentDbg[3]  = 0x00000000;
    pExponentDbg[4]   = 0x00000000; pExponentDbg[5]   = 0x00000000; pExponentDbg[6]   = 0x00000000; pExponentDbg[7]  = 0x00000000;
    pExponentDbg[8]   = 0x00000000; pExponentDbg[9]   = 0x00000000; pExponentDbg[10]  = 0x00000000; pExponentDbg[11]  = 0x00000000;
    pExponentDbg[12]  = 0x00000000; pExponentDbg[13]  = 0x00000000; pExponentDbg[14]  = 0x00000000; pExponentDbg[15]  = 0x00000000;
    pExponentDbg[16]  = 0x00000000; pExponentDbg[17]  = 0x00000000; pExponentDbg[18]  = 0x00000000; pExponentDbg[19]  = 0x00000000;
    pExponentDbg[20]  = 0x00000000; pExponentDbg[21]  = 0x00000000; pExponentDbg[22]  = 0x00000000; pExponentDbg[23]  = 0x00000000;
    pExponentDbg[24]  = 0x00000000; pExponentDbg[25]  = 0x00000000; pExponentDbg[26]  = 0x00000000; pExponentDbg[27]  = 0x00000000;
    pExponentDbg[28]  = 0x00000000; pExponentDbg[29]  = 0x00000000; pExponentDbg[30]  = 0x00000000; pExponentDbg[31]  = 0x00000000;
    pExponentDbg[32]  = 0x00000000; pExponentDbg[33]  = 0x00000000; pExponentDbg[34]  = 0x00000000; pExponentDbg[35]  = 0x00000000;
    pExponentDbg[36]  = 0x00000000; pExponentDbg[37]  = 0x00000000; pExponentDbg[38]  = 0x00000000; pExponentDbg[39]  = 0x00000000;
    pExponentDbg[40]  = 0x00000000; pExponentDbg[41]  = 0x00000000; pExponentDbg[42]  = 0x00000000; pExponentDbg[43]  = 0x00000000;
    pExponentDbg[44]  = 0x00000000; pExponentDbg[45]  = 0x00000000; pExponentDbg[46]  = 0x00000000; pExponentDbg[47]  = 0x00000000;
    pExponentDbg[48]  = 0x00000000; pExponentDbg[49]  = 0x00000000; pExponentDbg[50]  = 0x00000000; pExponentDbg[51]  = 0x00000000;
    pExponentDbg[52]  = 0x00000000; pExponentDbg[53]  = 0x00000000; pExponentDbg[54]  = 0x00000000; pExponentDbg[55]  = 0x00000000;
    pExponentDbg[56]  = 0x00000000; pExponentDbg[57]  = 0x00000000; pExponentDbg[58]  = 0x00000000; pExponentDbg[59]  = 0x00000000;
    pExponentDbg[60]  = 0x00000000; pExponentDbg[61]  = 0x00000000; pExponentDbg[62]  = 0x00000000; pExponentDbg[63]  = 0x00000000;
    pExponentDbg[64]  = 0x00000000; pExponentDbg[65]  = 0x00000000; pExponentDbg[66]  = 0x00000000; pExponentDbg[67]  = 0x00000000;
    pExponentDbg[68]  = 0x00000000; pExponentDbg[69]  = 0x00000000; pExponentDbg[70]  = 0x00000000; pExponentDbg[71]  = 0x00000000;
    pExponentDbg[72]  = 0x00000000; pExponentDbg[73]  = 0x00000000; pExponentDbg[74]  = 0x00000000; pExponentDbg[75]  = 0x00000000;
    pExponentDbg[76]  = 0x00000000; pExponentDbg[77]  = 0x00000000; pExponentDbg[78]  = 0x00000000; pExponentDbg[79]  = 0x00000000;
    pExponentDbg[80]  = 0x00000000; pExponentDbg[81]  = 0x00000000; pExponentDbg[82]  = 0x00000000; pExponentDbg[83]  = 0x00000000;
    pExponentDbg[84]  = 0x00000000; pExponentDbg[85]  = 0x00000000; pExponentDbg[86]  = 0x00000000; pExponentDbg[87]  = 0x00000000;
    pExponentDbg[88]  = 0x00000000; pExponentDbg[89]  = 0x00000000; pExponentDbg[90]  = 0x00000000; pExponentDbg[91]  = 0x00000000;
    pExponentDbg[92]  = 0x00000000; pExponentDbg[93]  = 0x00000000; pExponentDbg[94]  = 0x00000000; pExponentDbg[95]  = 0x00000000;
    pExponentDbg[96]  = 0x00000000; pExponentDbg[97]  = 0x00000000; pExponentDbg[98]  = 0x00000000; pExponentDbg[99]  = 0x00000000;
    pExponentDbg[100] = 0x00000000; pExponentDbg[101] = 0x00000000; pExponentDbg[102] = 0x00000000; pExponentDbg[103] = 0x00000000;
    pExponentDbg[104] = 0x00000000; pExponentDbg[105] = 0x00000000; pExponentDbg[106] = 0x00000000; pExponentDbg[107] = 0x00000000;
    pExponentDbg[108] = 0x00000000; pExponentDbg[109] = 0x00000000; pExponentDbg[110] = 0x00000000; pExponentDbg[111] = 0x00000000;
    pExponentDbg[112] = 0x00000000; pExponentDbg[113] = 0x00000000; pExponentDbg[114] = 0x00000000; pExponentDbg[115] = 0x00000000;
    pExponentDbg[116] = 0x00000000; pExponentDbg[117] = 0x00000000; pExponentDbg[118] = 0x00000000; pExponentDbg[119] = 0x00000000;
    pExponentDbg[120] = 0x00000000; pExponentDbg[121] = 0x00000000; pExponentDbg[122] = 0x00000000; pExponentDbg[123] = 0x00000000;
    pExponentDbg[124] = 0x00000000; pExponentDbg[125] = 0x00000000; pExponentDbg[126] = 0x00000000; pExponentDbg[127] = 0x00000000;

    pMpDbg[0]   = 0x9e0225bb; pMpDbg[1]   = 0xb73499df; pMpDbg[2]   = 0x6ff8a557; pMpDbg[3]   = 0xa008ebbe;
    pMpDbg[4]   = 0xfad37abe; pMpDbg[5]   = 0x553e5a40; pMpDbg[6]   = 0xd640088a; pMpDbg[7]   = 0x59ff459a;
    pMpDbg[8]   = 0xe1ec5eef; pMpDbg[9]   = 0x5860a6e7; pMpDbg[10]  = 0xb0bbfcc8; pMpDbg[11]  = 0x7f90a7f7;
    pMpDbg[12]  = 0x5a06dba3; pMpDbg[13]  = 0xc1ccae30; pMpDbg[14]  = 0x89d22079; pMpDbg[15]  = 0x61cf9d7a;
    pMpDbg[16]  = 0xfbb160f5; pMpDbg[17]  = 0xfb58f508; pMpDbg[18]  = 0x27e6dc64; pMpDbg[19]  = 0xa930f925;
    pMpDbg[20]  = 0x4fc1aaa2; pMpDbg[21]  = 0xe28d7f71; pMpDbg[22]  = 0xc778fe4a; pMpDbg[23]  = 0x1709668b;
    pMpDbg[24]  = 0x2d549f84; pMpDbg[25]  = 0x17828048; pMpDbg[26]  = 0xdcf03ec8; pMpDbg[27]  = 0x6f048e42;
    pMpDbg[28]  = 0xe582c272; pMpDbg[29]  = 0x24815bfd; pMpDbg[30]  = 0x4595a593; pMpDbg[31]  = 0xb0c362a6;
    pMpDbg[32]  = 0x9ac89758; pMpDbg[33]  = 0xacb00404; pMpDbg[34]  = 0x4de639ce; pMpDbg[35]  = 0x065946c9;
    pMpDbg[36]  = 0x73ce13ee; pMpDbg[37]  = 0x4a7c37e5; pMpDbg[38]  = 0x240b3952; pMpDbg[39]  = 0x3dbcbf59;
    pMpDbg[40]  = 0xd8d6c32c; pMpDbg[41]  = 0xad72e736; pMpDbg[42]  = 0x38d53615; pMpDbg[43]  = 0xc25ce7c0;
    pMpDbg[44]  = 0x6affd3a9; pMpDbg[45]  = 0x00776194; pMpDbg[46]  = 0x39bace8e; pMpDbg[47]  = 0x8ae30378;
    pMpDbg[48]  = 0xfd4b4155; pMpDbg[49]  = 0x59fa40df; pMpDbg[50]  = 0x7106134d; pMpDbg[51]  = 0xd0290f0f;
    pMpDbg[52]  = 0x1e77e75f; pMpDbg[53]  = 0xb5918ad4; pMpDbg[54]  = 0x0e2e8578; pMpDbg[55]  = 0x48c96e79;
    pMpDbg[56]  = 0x645a9703; pMpDbg[57]  = 0xa4ad05bf; pMpDbg[58]  = 0xb4e1091d; pMpDbg[59]  = 0x7fcdc0c9;
    pMpDbg[60]  = 0x5a2fe7ea; pMpDbg[61]  = 0x60770ad0; pMpDbg[62]  = 0x13162c8c; pMpDbg[63]  = 0xea602c8f;
    pMpDbg[64]  = 0x1e842a04; pMpDbg[65]  = 0x09bed660; pMpDbg[66]  = 0xfde85fa1; pMpDbg[67]  = 0xbf4fabcc;
    pMpDbg[68]  = 0x7f659cac; pMpDbg[69]  = 0x7dc7801e; pMpDbg[70]  = 0x70c40690; pMpDbg[71]  = 0x4884cbb7;
    pMpDbg[72]  = 0x21b03271; pMpDbg[73]  = 0xd8b069ab; pMpDbg[74]  = 0xee1c840b; pMpDbg[75]  = 0x25769431;
    pMpDbg[76]  = 0x998d2994; pMpDbg[77]  = 0x3f5933f3; pMpDbg[78]  = 0x111ac7df; pMpDbg[79]  = 0x3d4aca2c;
    pMpDbg[80]  = 0x6226059a; pMpDbg[81]  = 0x407914d5; pMpDbg[82]  = 0x4114fd36; pMpDbg[83]  = 0x931183ae;
    pMpDbg[84]  = 0x9f04cdcd; pMpDbg[85]  = 0x1936377e; pMpDbg[86]  = 0x572d44f4; pMpDbg[87]  = 0x9764d4d1;
    pMpDbg[88]  = 0xec729b5b; pMpDbg[89]  = 0xbb903cce; pMpDbg[90]  = 0xac72ad36; pMpDbg[91]  = 0x016cc881;
    pMpDbg[92]  = 0x306aadba; pMpDbg[93]  = 0xc58fb04a; pMpDbg[94]  = 0x9b8898f1; pMpDbg[95]  = 0x46a12ba5;
    pMpDbg[96]  = 0x00000000; pMpDbg[97]  = 0x00000000; pMpDbg[98]  = 0x00000000; pMpDbg[99]  = 0x00000000;
    pMpDbg[100] = 0x00000000; pMpDbg[101] = 0x00000000; pMpDbg[102] = 0x00000000; pMpDbg[103] = 0x00000000;
    pMpDbg[104] = 0x00000000; pMpDbg[105] = 0x00000000; pMpDbg[106] = 0x00000000; pMpDbg[107] = 0x00000000;
    pMpDbg[108] = 0x00000000; pMpDbg[109] = 0x00000000; pMpDbg[110] = 0x00000000; pMpDbg[111] = 0x00000000;
    pMpDbg[112] = 0x00000000; pMpDbg[113] = 0x00000000; pMpDbg[114] = 0x00000000; pMpDbg[115] = 0x00000000;
    pMpDbg[116] = 0x00000000; pMpDbg[117] = 0x00000000; pMpDbg[118] = 0x00000000; pMpDbg[119] = 0x00000000;
    pMpDbg[120] = 0x00000000; pMpDbg[121] = 0x00000000; pMpDbg[122] = 0x00000000; pMpDbg[123] = 0x00000000;
    pMpDbg[124] = 0x00000000; pMpDbg[125] = 0x00000000; pMpDbg[126] = 0x00000000; pMpDbg[127] = 0x00000000;

    pRsqrDbg[0]   = 0xc6508f68; pRsqrDbg[1]   = 0x987ae9a7; pRsqrDbg[2]   = 0x7eb51ee0; pRsqrDbg[3]   = 0x16e46d51;
    pRsqrDbg[4]   = 0xecea09e9; pRsqrDbg[5]   = 0x59895cba; pRsqrDbg[6]   = 0xeb92760d; pRsqrDbg[7]   = 0xab0ff839;
    pRsqrDbg[8]   = 0xff7b078e; pRsqrDbg[9]   = 0x3cca27de; pRsqrDbg[10]  = 0x59c694be; pRsqrDbg[11]  = 0xe38e795d;
    pRsqrDbg[12]  = 0x6c0a6ce2; pRsqrDbg[13]  = 0xa571a526; pRsqrDbg[14]  = 0xf97f68bc; pRsqrDbg[15]  = 0x64c6ef87;
    pRsqrDbg[16]  = 0x15eef093; pRsqrDbg[17]  = 0x3ec7525c; pRsqrDbg[18]  = 0xfc6b2a91; pRsqrDbg[19]  = 0x48a61d96;
    pRsqrDbg[20]  = 0x88dd7e9c; pRsqrDbg[21]  = 0xa6cdf464; pRsqrDbg[22]  = 0x2401e83e; pRsqrDbg[23]  = 0xb2690d86;
    pRsqrDbg[24]  = 0x96cd6ebb; pRsqrDbg[25]  = 0xb99b3821; pRsqrDbg[26]  = 0xeebc6d55; pRsqrDbg[27]  = 0xd0e2418c;
    pRsqrDbg[28]  = 0x8b5fbfe7; pRsqrDbg[29]  = 0x2a78cab9; pRsqrDbg[30]  = 0x71a4a1a0; pRsqrDbg[31]  = 0x367c2b3d;
    pRsqrDbg[32]  = 0xd5fe5b3e; pRsqrDbg[33]  = 0x134db196; pRsqrDbg[34]  = 0x5045948c; pRsqrDbg[35]  = 0x0328ca7d;
    pRsqrDbg[36]  = 0x254ec2e6; pRsqrDbg[37]  = 0x2277f32b; pRsqrDbg[38]  = 0xa76a0e9a; pRsqrDbg[39]  = 0x79b8c7c0;
    pRsqrDbg[40]  = 0xdf60dde0; pRsqrDbg[41]  = 0x6e3c80c1; pRsqrDbg[42]  = 0xf729b334; pRsqrDbg[43]  = 0x2f80a655;
    pRsqrDbg[44]  = 0x9401d3f4; pRsqrDbg[45]  = 0x4aa6a86a; pRsqrDbg[46]  = 0x1f694f50; pRsqrDbg[47]  = 0x126426d8;
    pRsqrDbg[48]  = 0xf9f8ebb8; pRsqrDbg[49]  = 0xa26de21f; pRsqrDbg[50]  = 0x46277fc7; pRsqrDbg[51]  = 0xbc805050;
    pRsqrDbg[52]  = 0xf07d0a06; pRsqrDbg[53]  = 0x33b25fcf; pRsqrDbg[54]  = 0x4ca79081; pRsqrDbg[55]  = 0x7474438e;
    pRsqrDbg[56]  = 0xa8d92aae; pRsqrDbg[57]  = 0xca962471; pRsqrDbg[58]  = 0x6251a267; pRsqrDbg[59]  = 0x11a9b153;
    pRsqrDbg[60]  = 0x920011ce; pRsqrDbg[61]  = 0xfc9fca68; pRsqrDbg[62]  = 0xa60dbe22; pRsqrDbg[63]  = 0x378f0e2d;
    pRsqrDbg[64]  = 0x50828ddb; pRsqrDbg[65]  = 0x1664b74c; pRsqrDbg[66]  = 0x5fbea6c8; pRsqrDbg[67]  = 0x51b166bf;
    pRsqrDbg[68]  = 0x1aa7a429; pRsqrDbg[69]  = 0x2e43c1d7; pRsqrDbg[70]  = 0xb17a4ca7; pRsqrDbg[71]  = 0xbe8ccf40;
    pRsqrDbg[72]  = 0x1c23c38f; pRsqrDbg[73]  = 0x1c23a492; pRsqrDbg[74]  = 0xd3cf1d42; pRsqrDbg[75]  = 0x6471c1d5;
    pRsqrDbg[76]  = 0xf8092276; pRsqrDbg[77]  = 0xa2a1246c; pRsqrDbg[78]  = 0x1f379705; pRsqrDbg[79]  = 0x25f8f305;
    pRsqrDbg[80]  = 0xeea8c2a7; pRsqrDbg[81]  = 0xfbd51fc1; pRsqrDbg[82]  = 0x89a497d9; pRsqrDbg[83]  = 0xc4d6d331;
    pRsqrDbg[84]  = 0x5e138984; pRsqrDbg[85]  = 0xf668372e; pRsqrDbg[86]  = 0x444d7e74; pRsqrDbg[87]  = 0x3ddeef60;
    pRsqrDbg[88]  = 0x30866f94; pRsqrDbg[89]  = 0xef650561; pRsqrDbg[90]  = 0xa55909c9; pRsqrDbg[91]  = 0x60e83d95;
    pRsqrDbg[92]  = 0x2c94ec95; pRsqrDbg[93]  = 0x90ca829e; pRsqrDbg[94]  = 0x64bb6c8b; pRsqrDbg[95]  = 0x828a5d9e;
    pRsqrDbg[96]  = 0x00000000; pRsqrDbg[97]  = 0x00000000; pRsqrDbg[98]  = 0x00000000; pRsqrDbg[99]  = 0x00000000;
    pRsqrDbg[100] = 0x00000000; pRsqrDbg[101] = 0x00000000; pRsqrDbg[102] = 0x00000000; pRsqrDbg[103] = 0x00000000;
    pRsqrDbg[104] = 0x00000000; pRsqrDbg[105] = 0x00000000; pRsqrDbg[106] = 0x00000000; pRsqrDbg[107] = 0x00000000;
    pRsqrDbg[108] = 0x00000000; pRsqrDbg[109] = 0x00000000; pRsqrDbg[110] = 0x00000000; pRsqrDbg[111] = 0x00000000;
    pRsqrDbg[112] = 0x00000000; pRsqrDbg[113] = 0x00000000; pRsqrDbg[114] = 0x00000000; pRsqrDbg[115] = 0x00000000;
    pRsqrDbg[116] = 0x00000000; pRsqrDbg[117] = 0x00000000; pRsqrDbg[118] = 0x00000000; pRsqrDbg[119] = 0x00000000;
    pRsqrDbg[120] = 0x00000000; pRsqrDbg[121] = 0x00000000; pRsqrDbg[122] = 0x00000000; pRsqrDbg[123] = 0x00000000;
    pRsqrDbg[124] = 0x00000000; pRsqrDbg[125] = 0x00000000; pRsqrDbg[126] = 0x00000000; pRsqrDbg[127] = 0x00000000;

    return ACR_OK;
}

#ifdef LIB_CCC_PRESENT
/*!
 *@brief Decrypt LS signature for ID = (LS_SIG_VALIDATION_ID_1 / LS_SIG_VALIDATION_ID_2)  with RSA algorithm for PKC-LS by CCC library
 */
static ACR_STATUS
_acrRsaDecryptLsSignature
(
    LwU8 *pSignatureEnc,
    LwU32 keySizeBit,
    LwU8 *pSignatureDec
)
{
    ACR_STATUS                   status = ACR_OK;
    struct se_data_params        input;
    struct se_engine_rsa_context rsa_econtext;
    status_t                     statusCcc = ERR_GENERIC;
    engine_t                     *pEngine = NULL;
    uint32_t                     expSize;
    uint32_t                     modulusSize;
    uint8_t                      *pExp = NULL;
    uint8_t                      *pModulus = NULL;

    //
    // set LS RSA key components
    // TODO: eddichang ACR-RISCV HS siganture can cover DMEM, we can refactor _seInitializationRsaKeyDbg_GA10X()
    // and _seInitializationRsaKeyProd_GA10X() once ACR move to RISCV.
    //
    if (g_bIsDebug)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_seInitializationRsaKeyDbg_GA10X(g_rsaKeyModulusDbg_GA10X, g_rsaKeyExponentDbg_GA10X,
                                                                           g_rsaKeyMpDbg_GA10X, g_rsaKeyRsqrDbg_GA10X));
        pExp = (uint8_t *)g_rsaKeyExponentDbg_GA10X;
        expSize = sizeof(g_rsaKeyExponentDbg_GA10X);
        pModulus = (uint8_t *)g_rsaKeyModulusDbg_GA10X;
        modulusSize = sizeof(g_rsaKeyModulusDbg_GA10X);
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_seInitializationRsaKeyProd_GA10X(g_rsaKeyModulusProd_GA10X, g_rsaKeyExponentProd_GA10X,
                                                                            g_rsaKeyMpProd_GA10X, g_rsaKeyRsqrProd_GA10X));

        pExp = (uint8_t *)g_rsaKeyExponentProd_GA10X;
        expSize = sizeof(g_rsaKeyExponentProd_GA10X);
        pModulus = (uint8_t *)g_rsaKeyModulusProd_GA10X;
        modulusSize = sizeof(g_rsaKeyModulusProd_GA10X);
    }

    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(ccc_select_engine((const struct engine_s **)&pEngine, ENGINE_CLASS_RSA, ENGINE_PKA),
                                          ACR_ERROR_LWPKA_SELECT_ENGINE_FAILED);

    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(lwpka_acquire_mutex(pEngine), ACR_ERROR_LWPKA_ACQUIRE_MUTEX_FAILED);

    // clear struct first
    acrMemsetByte((void *)&input, sizeof(struct se_data_params), 0);

    /* Fill in se_data_params required by CCC */
    input.input_size  = (keySizeBit >> 3);
    input.output_size = (keySizeBit >> 3);
    input.src = (uint8_t *)pSignatureEnc; /* Char string to byte vector */
    input.dst = (uint8_t *)pSignatureDec; /* Char string to byte vector */

    // clear struct first
    acrMemsetByte((void *)&rsa_econtext, sizeof(struct se_engine_rsa_context), 0);

    /* Fill in se_engine_rsa_context required by CCC */
    rsa_econtext.engine           = pEngine;
    rsa_econtext.rsa_size         = (keySizeBit >> 3);
    rsa_econtext.rsa_flags        = RSA_FLAGS_DYNAMIC_KEYSLOT;
    rsa_econtext.rsa_keytype      = KEY_TYPE_RSA_PUBLIC;
    rsa_econtext.rsa_pub_exponent = (uint8_t *)pExp;
    rsa_econtext.rsa_pub_expsize  = expSize;
    rsa_econtext.rsa_modulus      = (uint8_t *)pModulus;

    CHECK_STATUS_CCC_OK_OR_GOTO_CLEANUP(engine_lwpka_rsa_modular_exp_locked(&input, &rsa_econtext),
                                        ACR_ERROR_LWPKA_MODULAR_EXP_LOCK_FAILED);

Cleanup:

    lwpka_release_mutex(pEngine);

    _endianRevert((LwU8 *)pSignatureDec, (keySizeBit >> 3));

    return status;
}

#else

/*!
 *@brief Decrypt LS signature for ID = LS_SIG_VALIDATION_ID_1 with RSA algorithm for PKC-LS
 */
static ACR_STATUS
_acrRsaDecryptLsSignature
(
    LwU8 *pSignatureEnc,
    LwU32 keySizeBit,
    LwU8 *pSignatureDec
)
{
    SE_PKA_REG sePkaReg;
    ACR_STATUS status, tmpStatus;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(seAcquireMutex_HAL(pSe));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(seContextIni_HAL(pSe, &sePkaReg, keySizeBit));

    // set LS RSA key components
    if (g_bIsDebug)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_seInitializationRsaKeyDbg_GA10X(g_rsaKeyModulusDbg_GA10X, g_rsaKeyExponentDbg_GA10X,
                                                                         g_rsaKeyMpDbg_GA10X, g_rsaKeyRsqrDbg_GA10X));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seSetRsaOperationKeys_HAL(pSe, &sePkaReg,
                                                                  g_rsaKeyModulusDbg_GA10X,
                                                                  g_rsaKeyExponentDbg_GA10X,
                                                                  g_rsaKeyMpDbg_GA10X,
                                                                  g_rsaKeyRsqrDbg_GA10X));
    }
    else
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_seInitializationRsaKeyProd_GA10X(g_rsaKeyModulusProd_GA10X, g_rsaKeyExponentProd_GA10X,
                                                                          g_rsaKeyMpProd_GA10X, g_rsaKeyRsqrProd_GA10X));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seSetRsaOperationKeys_HAL(pSe, &sePkaReg,
                                                                  g_rsaKeyModulusProd_GA10X,
                                                                  g_rsaKeyExponentProd_GA10X,
                                                                  g_rsaKeyMpProd_GA10X,
                                                                  g_rsaKeyRsqrProd_GA10X));
    }

    // decrypt signature
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_HAL(pSe, &sePkaReg, SE_RSA_PARAMS_TYPE_BASE, (LwU32 *)pSignatureEnc));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seStartPkaOperationAndPoll_HAL(pSe, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODEXP,
                                                                   sePkaReg.pkaRadixMask, SE_ENGINE_OPERATION_TIMEOUT_DEFAULT));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaGetParams_HAL(pSe, &sePkaReg, SE_RSA_PARAMS_TYPE_RSA_RESULT, (LwU32 *)pSignatureDec));

    _endianRevert((LwU8 *)pSignatureDec, (keySizeBit >> 3));

Cleanup:
    tmpStatus = seReleaseMutex_HAL(pSe);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}
#endif // LIB_CCC_PRESENT

#endif //PKC_LS_RSA3K_KEYS_GA10X

#endif // AHESASC && NEW_WPR_BLOBS
