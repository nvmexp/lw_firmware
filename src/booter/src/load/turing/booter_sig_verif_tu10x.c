/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_sig_verif_tu10x.c
 */

//
// Includes
//
#include "booter.h"

#include "booter_objse.h"
#include "booter_objsha.h"
#include "booter_revocation.h"
#include "booter_lsverif_keys_rsa3k.h"

#define GET_MOST_SIGNIFICANT_BIT(keySize)      (keySize > 0 ? ((keySize - 1) & 7) : 0)
#define GET_ENC_MESSAGE_SIZE_BYTE(keySize)     (keySize + 7) >> 3;
#define PKCS1_MGF1_COUNTER_SIZE_BYTE           (4)
#define RSA_PSS_PADDING_ZEROS_SIZE_BYTE        (8)

// DMA_SIZE is in bytes. These buffers are LwU32.
LwU32 g_copyBufferA [COPY_BUFFER_A_SIZE_DWORD]                 ATTR_OVLY(".data") ATTR_ALIGNED(0x100);
LwU32 g_copyBufferB [COPY_BUFFER_B_SIZE_DWORD]                 ATTR_OVLY(".data") ATTR_ALIGNED(0x100);
LwU8  g_hashBuffer  [BOOTER_LS_HASH_BUFFER_SIZE_MAX_BYTE]      ATTR_OVLY(".data") ATTR_ALIGNED(4);
LwU8  g_hashResult  [BOOTER_LS_HASH_BUFFER_SIZE_MAX_BYTE]      ATTR_OVLY(".data") ATTR_ALIGNED(0x4);
LwU8  g_lsSignature [BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE] ATTR_OVLY(".data") ATTR_ALIGNED(0x100);

extern LwBool          g_bIsDebug;
extern BOOTER_DMA_PROP g_dmaProp[BOOTER_DMA_CONTEXT_COUNT];
extern LwU8            g_bounceBufferB[4096];

static BOOTER_STATUS _booterCheckRevocation(LwU32 lsUcodeVer) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterGetSignatureValidationId(LwU32 hashAlgoVer, LwU32 hashAlgo, LwU32 sigAlgoVer, LwU32 sigAlgo, LwU32 sigPaddingType, LwU32 *pValidationId) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterPlainTextSizeCheck(SHA_ID algoId, LwU32 size, LwU32 offset, LwU32 *pSizeNew, LwU32 *pOffsetNew, LwBool *pNeedAdjust) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterGeneratePlainTextHash(LwU32 falconId, SHA_ID algoId, LwU64 binBaseAddr, LwU32 binOffset,
                                                LwU32 binSize, LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterGeneratePlainTextHashNonAligned(LwU32 falconId, SHA_ID algoId, LwU64 binBaseAddr, LwU32 binOffset, LwU32 binSize,
                                                          LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode,
                                                          LwU32  offsetNew, LwU32  sizeNew) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterRsaDecryptLsSignature(LwU8 *pSignatureEnc, SE_KEY_SIZE keySizeBit, LwU8 *pSignatureDec) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterSignatureValidationHandler(LwU32 falconId, SHA_ID hashAlgoId, LwU8 *pPlainTextHash, LwU8 *pDecSig,
                                                     LwU32 keySizeBit, LwU32 hashSizeByte, LwU32 saltSizeByte) ATTR_OVLY(OVL_NAME);

static BOOTER_STATUS _booterEnforcePkcs1Mgf1(LwU8 *pMask, LwU32  maskedLen, LwU8  *pSeed, LwU32 seedLen, SHA_ID shaId) ATTR_OVLY(OVL_NAME);

static void          _endianRevert(LwU8 *pBuf, LwU32 bufSize) ATTR_OVLY(OVL_NAME);

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
 *@brief To validate LS signature with PKC-LS support and new wpr blob defines
 */
BOOTER_STATUS
booterValidateLsSignature_TU10X
(
    LwU32  falconId,
    LwU8  *pSignature,
    LwU64  binBaseAddr,
    LwU32  binOffset,
    LwU32  binSize,
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
    BOOTER_STATUS status = BOOTER_ERROR_UNKNOWN;
    SHA_ID  hashAlgoId = SHA_ID_LAST;
    LwU32  hashSizeByte;
    LwU32  saltSizeByte;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO_VER, pLsfUcodeDescWrapper, (void *)&hashAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO, pLsfUcodeDescWrapper, (void *)&hashAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_VER, pLsfUcodeDescWrapper, (void *)&sigAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO, pLsfUcodeDescWrapper, (void *)&sigAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_PADDING_TYPE, pLsfUcodeDescWrapper, (void *)&sigPaddingType));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterGetSignatureValidationId(hashAlgoVer, hashAlgo, sigAlgoVer, sigAlgo, sigPaddingType, &lsSigValidationId));

    if (lsSigValidationId == LS_SIG_VALIDATION_ID_1)
    {
        hashAlgoId   = SHA_ID_SHA_256;
        hashSizeByte = SHA_256_HASH_SIZE_BYTE;
        saltSizeByte = SHA_256_HASH_SIZE_BYTE;
    }
    else
    {
       // do nothing, let below switch check to filter sig validation id.
    }

    switch (lsSigValidationId)
    {
        case LS_SIG_VALIDATION_ID_1: // Id 1 : rsa3k-pss padding with SHA256 hash algorithm
        {   
            // Get plain text hash
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterGeneratePlainTextHash(falconId, hashAlgoId, binBaseAddr, binOffset, binSize,
                                                                            pPlainTextHash, pLsfUcodeDescWrapper, bIsUcode));

            // decrypt signature with RSA3K public keySize
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterRsaDecryptLsSignature(pSignature, RSA_KEY_SIZE_3072_BIT, pSignature));

            // call validation handler
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterSignatureValidationHandler(falconId, hashAlgoId, pPlainTextHash, pSignature,
                                              RSA_KEY_SIZE_3072_BIT, hashSizeByte, saltSizeByte));
        }
        break;

        default:
            return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    return BOOTER_OK;
}

/*!
 * Read revocation information from LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i)
 * if available.
 */
BOOTER_STATUS
booterReadRevocationRegFromGroup25_TU10X(LwU32 i, LwU32 *pVal)
{
    // LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i) doesn't exist on Turing/GA100.
    return BOOTER_ERROR_UNKNOWN;
}

/*!
 * Check for revocation based on LS ucode version.
 */
static BOOTER_STATUS _booterCheckRevocation
(
    LwU32 lsUcodeVer
)
{
    BOOTER_STATUS status = BOOTER_OK;

    LwU32 revNum = REF_VAL(GSPRM_LS_UCODE_VER_REV_NUM, lsUcodeVer);
    LwU32 branchNum = REF_VAL(GSPRM_LS_UCODE_VER_BRANCH_NUM, lsUcodeVer);

    LwU32 regIdx = branchNum / GSPRM_REVOCATION_NUM_BRANCHES_PER_REG;
    LwU32 regVal;

    LwU32 minRevNum;

    // check revision against hard-coded minimums
    minRevNum = gsprmRevocationGetHardcodedMinRevNumForBranch(branchNum);
    if (minRevNum == GSPRM_REVOCATION_ILWALID_REV_NUM)
    {
        return BOOTER_ERROR_REVOCATION_CHECK_FAIL;
    }
    if (revNum < minRevNum)
    {
        return BOOTER_ERROR_UCODE_REVOKED;
    }

    // read secure scratch register containing revocation information for branch
    if (regIdx == 0)
    {
        regVal = BOOTER_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_19);
    }
    else
    {
        // attempt to read LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i)
        status = booterReadRevocationRegFromGroup25_HAL(pBooter, regIdx - 1, &regVal);
        if (status != BOOTER_OK)
        {
            return BOOTER_ERROR_REVOCATION_CHECK_FAIL;
        }
    }

    // check revision against minimum from secure scratch register
    minRevNum = gsprmRevocationGetMinRevNumFromRegVal(regVal, branchNum);
    if (minRevNum == GSPRM_REVOCATION_ILWALID_REV_NUM)
    {
        return BOOTER_ERROR_REVOCATION_CHECK_FAIL;
    }
    if (revNum < minRevNum)
    {
        return BOOTER_ERROR_UCODE_REVOKED;
    }

    return status;
}

/*!
 * @ Get signature validation ID depends on signature parameters
 */
static BOOTER_STATUS
_booterGetSignatureValidationId
(
    LwU32 hashAlgoVer,
    LwU32 hashAlgo,
    LwU32 sigAlgoVer,
    LwU32 sigAlgo,
    LwU32 sigPaddingType,
    LwU32 *pValidationId
)
{
    BOOTER_STATUS status = BOOTER_OK;

    if (pValidationId == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    *pValidationId  = LS_SIG_VALIDATION_ID_ILWALID;

    if (hashAlgoVer    == LSF_HASH_ALGO_VERSION_1             &&
        sigAlgoVer     == LSF_SIGNATURE_ALGO_VERSION_1        &&
        sigAlgo        == LSF_SIGNATURE_ALGO_TYPE_RSA3K       &&
        sigPaddingType == LSF_SIGNATURE_ALGO_PADDING_TYPE_PSS &&
        hashAlgo       == LSF_HASH_ALGO_TYPE_SHA256)
    {
        *pValidationId  = LS_SIG_VALIDATION_ID_1;
    }
    else
    {
        status = BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    return status;
}

/*!
 *@brief Enforce PKCS1_MGF1 process
 */
static BOOTER_STATUS
_booterEnforcePkcs1Mgf1
(
    LwU8  *pMask,
    LwU32  maskedLen,
    LwU8  *pSeed,
    LwU32  seedLen,
    SHA_ID shaId
)
{
    SHA_CONTEXT     shaCtx;
    SHA_TASK_CONFIG taskCfg;
    BOOTER_STATUS   status, tmpStatus = BOOTER_ERROR_UNKNOWN;
    LwU32           outLen = 0, i, j;
    LwU8           *pFinalBuf = pMask;
    LwU8           *pTempStart = (LwU8 *)g_copyBufferB; // use g_copyBufferB as temp buffer in SHA operation process
    LwU8           *pTemp = pTempStart;
    LwU32           mdLen;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetHashSizeByte_HAL(pSha, shaId, &mdLen));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION));

    //
    // Because g_copyBuffer is declared as external variable, can't use sizeof(g_copyBufferB) to get g_copyBuffer's size.
    // In booter.h, we define copy buffer A/B size to help current function have a buffer size sanity check.
    // seedLen either equals to 0 or hash size(e.g sh256 hash  size is 32 bytes).
    // g_coyBufferB size is 1024 bytes; and its size is large enough to cover all combinations.
    // We still need to add size sanity check to pass coverity check.
    //
    if ((seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE) > COPY_BUFFER_B_SIZE_BYTE)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
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
    tmpStatus = shaReleaseMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION);

    status = (status == BOOTER_OK ? tmpStatus : status);

    return status;
}

/*!
 * @ The common part for SHA operation with single task configuration.
 */
static BOOTER_STATUS
_booterShaRunSingleTaskCommon
(
    SHA_ID           algoId,
    LwU32            sizeByte,
    SHA_TASK_CONFIG *pTaskCfg,
    LwU8            *pBufOut
)
{
    BOOTER_STATUS status = BOOTER_ERROR_UNKNOWN;
    SHA_CONTEXT shaCtx;

    shaCtx.algoId  = algoId;
    shaCtx.msgSize = sizeByte;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaOperationInit_HAL(pSha, &shaCtx));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask_HAL(pSha, &shaCtx, pTaskCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaReadHashResult_HAL(pSha, &shaCtx, (void *)pBufOut, LW_TRUE));

    return BOOTER_OK;
}

/*!
 * @ To apply SHA operation on plain text saved on DMEM
 */
static BOOTER_STATUS
_booterShaRunSingleTaskDmem
(
    SHA_ID  algoId,
    LwU8   *pBufIn,
    LwU8   *pBufOut,
    LwU32   sizeByte
)
{
    SHA_TASK_CONFIG taskCfg;
    BOOTER_STATUS  status, tmpStatus;

    // SoftReset will clear mutex, don't run any soft reset during SHA operation.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION));

    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.dmaIdx        = 0;
    taskCfg.size          = sizeByte;
    taskCfg.addr          = (LwU64)((LwU32)pBufIn);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_booterShaRunSingleTaskCommon(algoId, sizeByte, &taskCfg, pBufOut));

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION);

    status = (status == BOOTER_OK ? tmpStatus : status);

    return status;
}

/*!
 * @brief For multi task SHA operation, the non-last task size need to aligned with SHA block size.
 *        This function is used take a snity check for message size; and also return aligned offset
 *        and aligned size for onward SHA 256 operation.
 */
static BOOTER_STATUS
_booterPlainTextSizeCheck
(
    SHA_ID  algoId,
    LwU32   size,
    LwU32   offset,
    LwU32  *pSizeNew,
    LwU32  *pOffsetNew,
    LwBool *pNeedAdjust
)
{
    BOOTER_STATUS status = BOOTER_ERROR_ILWALID_ARGUMENT;
    LwU32 blockSizeByte;
    LwU32 blockCount;
    LwU64 temp64;
    LwU32 sizeRemainder;

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
        return BOOTER_ERROR_SHA_ILWALID_CONFIG;
    }

    sizeRemainder = (size % blockSizeByte);

    if (sizeRemainder)
    {
       temp64 = booterMultipleUnsigned32(blockCount, blockSizeByte);
       *pSizeNew = (LwU32)LwU64_LO32(temp64);

       temp64 = (LwU64)offset + temp64;
       *pOffsetNew = (LwU32)LwU64_LO32(temp64);

       *pNeedAdjust = LW_TRUE;
    }

    return BOOTER_OK;
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
static BOOTER_STATUS
_booterGeneratePlainTextHashNonAligned
(
    LwU32 falconId,
    SHA_ID algoId,
    LwU64 binBaseAddr,
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
    BOOTER_STATUS    status, tmpStatus;
    LwU32            depMapSize;
    LwU8            *pBufferByte = (LwU8 *)g_copyBufferA;
    LwU8            *pBufferByteOriginal = pBufferByte;
    LwU32            remainderSize;
    LwU32            temp32;
    LwU32            bytesToHash = 0;
    LwU32            bytesHashed = 0;

    // move pointer to the last byte and copy notification and dep map context
    // The aligned size is always less than original size.
    if (sizeNew >= binSize)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    remainderSize = binSize - sizeNew;

    // Get dependency map size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
                                                                        pLsfUcodeDescWrapper, (void *)&depMapSize));

    //First, sanity check for buffer size, to make sure buffer is large enough to store notifications, dep map context and reminader bytes in FB
    if ((remainderSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize) > BOOTER_FALCON_BLOCK_SIZE_IN_BYTES)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    //
    // Callwlating binary size, we concatenate falconID, ucodeVer and ucodeID to code and data section for ID 1 hash generation process.
    // Bin data is stored at FB; but concatenations are stored at DMEM, so we need to specify 2 tasks to complete SHA hash opertion.
    // (binary data) || fpfHwVersion || ucodeId
    //
    plainTextSize = binSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION));
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));

    // Task-1: SHA hash callwlation of bin data stored at FB but only for aligned size region
    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_FB;
    taskCfg.dmaIdx        = BOOTER_DMA_FB_WPR_CONTEXT;
    taskCfg.defaultHashIv = LW_TRUE;

    while(sizeNew > 0)
    {
        bytesToHash  = (sizeNew > 0xF00000) ? 0xF00000 : sizeNew; // (16 MB - 1) max
        binAddr      = (binBaseAddr + binOffset + bytesHashed);

        taskCfg.size = bytesToHash;
        taskCfg.addr = binAddr;
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

        bytesHashed  += bytesToHash;
        sizeNew      -= bytesToHash;
        taskCfg.defaultHashIv = LW_FALSE; // Don't use default IV for subsequent calls
    }

    //
    // Leverage g_copyBufferA as temp buffer to save remainder bytes in FB first.
    // Always read last 256 byte to temp buffer and concanate falocnId, ucodeVer, ucodeId and dep map context
    // into the buffer.
    //
    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].baseAddr = ((binBaseAddr + offsetNew)>>8);
    offsetNew = (offsetNew & 0xFF);

    if ((booterIssueDma_HAL(pBooter, pBufferByte, LW_FALSE, offsetNew, BOOTER_FALCON_BLOCK_SIZE_IN_BYTES,
            BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT])) != BOOTER_FALCON_BLOCK_SIZE_IN_BYTES)
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    // move pointer to the last byte and copy notification and dep map context
    pBufferByte += remainderSize;

    // Don't use doword pointer to copy dword value because pBuffreByte could NOT be dword aigned pointer.
    // In case we cast a pointer which contained addrees is NOT dword aligned poineter
    if (!booterMemcpy_HAL(pBooter, (void *)pBufferByte, (void *)&falconId, sizeof(falconId)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(falconId);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&temp32));
    if (!booterMemcpy_HAL(pBooter, (void *)pBufferByte, (void *)&temp32, sizeof(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                        pLsfUcodeDescWrapper,  (void *)&temp32));

    if (!booterMemcpy_HAL(pBooter, (void *)pBufferByte, (void *)&temp32, sizeof(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += sizeof(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
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
    tmpStatus = shaReleaseMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION);

    status = (status == BOOTER_OK ? tmpStatus : status);

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
static BOOTER_STATUS
_booterGeneratePlainTextHash
(
    LwU32 falconId,
    SHA_ID algoId,
    LwU64 binBaseAddr,
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
    BOOTER_STATUS    status, tmpStatus;
    LwU32            depMapSize;
    LwU32           *pBufferDw;
    LwBool           bNeedAdjust = LW_FALSE;
    LwU32            sizeNew = 0;
    LwU32            offsetNew = 0;
    LwU32            bytesToHash = 0;
    LwU32            bytesHashed = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterPlainTextSizeCheck(algoId, binSize, binOffset, &sizeNew, &offsetNew, &bNeedAdjust));

    if (bNeedAdjust)
    {
        return _booterGeneratePlainTextHashNonAligned(falconId, algoId, binBaseAddr, binOffset, binSize, pBufOut, pLsfUcodeDescWrapper, bIsUcode, offsetNew, sizeNew);
    }

    //
    // Callwlating binary size, we concatenate falconID, ucodeVer and ucodeID to code and data section for ID 1 hash generation process.
    // Bin data is stored at FB; but concatenations are stored at DMEM, so we need to specify 2 tasks to complete SHA hash opertion.
    // (binary data) || fpfHwVersion || ucodeId
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
                                                                        pLsfUcodeDescWrapper, (void *)&depMapSize));

    plainTextSize = binSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION));
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));

    // Task-1: SHA hash callwlation of bin data stored at FB
    taskCfg.srcType       = SHA_SRC_CONFIG_SRC_FB;
    taskCfg.dmaIdx        = BOOTER_DMA_FB_WPR_CONTEXT;
    taskCfg.defaultHashIv = LW_TRUE;

    while(binSize > 0)
    {
        bytesToHash  = (binSize > 0xF00000) ? 0xF00000 : binSize; // (16 MB - 1) max
        binAddr      = (binBaseAddr + binOffset + bytesHashed);

        taskCfg.size = bytesToHash;
        taskCfg.addr = binAddr;
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

        bytesHashed  += bytesToHash;
        binSize      -= bytesToHash;
        taskCfg.defaultHashIv = LW_FALSE; // Don't use default IV for subsequent calls
    }

    //
    // Task-2 : Leverage g_copyBufferA as temp buffer to save concatenation components
    // bin || falconId || lsUcodeVer || lsUocdeId || depMapCtx
    //
    pBufferDw    = (LwU32 *)g_copyBufferA;
    pBufferDw[0] = falconId;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&pBufferDw[1]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                      pLsfUcodeDescWrapper,  (void *)&pBufferDw[2]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
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
    tmpStatus = shaReleaseMutex_HAL(pSha, BOOTER_LOAD_LS_SIGNATURE_VALIDATION);

    status = (status == BOOTER_OK ? tmpStatus : status);

    return status;
}

/*!
 *@brief Signature handler for ID = LS_SIG_VALIDATION_ID_1 (RSA3K-PSS padding + SHA-256 hash)
 */
static BOOTER_STATUS
_booterSignatureValidationHandler
(
    LwU32 falconId,
    SHA_ID hashAlgoId,
    LwU8 *pPlainTextHash,
    LwU8 *pDecSig,
    LwU32 keySizeBit,
    LwU32 hashSizeByte,
    LwU32 saltSizeByte
)
{
    BOOTER_STATUS status = BOOTER_ERROR_UNKNOWN;
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
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (msBits == 0)
    {
       pEm++;
       emLen --;
    }

    if (emLen < hashSizeByte + 2)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (saltSizeByte > emLen - hashSizeByte - 2)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    if (pEm[emLen - 1] != 0xbc)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    // g_copyBufferA is used as a temp buffer
    pDb = (LwU8 *)g_copyBufferA;

    maskedDBLen = emLen - hashSizeByte - 1;
    pHash = pEm + maskedDBLen;

    if (_booterEnforcePkcs1Mgf1(pDb, maskedDBLen, pHash, hashSizeByte, hashAlgoId) != BOOTER_OK)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
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
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    if ((maskedDBLen - i) != saltSizeByte)
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterShaRunSingleTaskDmem(hashAlgoId, pHashGoldenStart,
                                                               pHashGoldenStart, msgGoldenLen))

    // compare signature
    if(booterMemcmp(pHash, pHashGoldenStart, hashSizeByte))
    {
        return BOOTER_ERROR_LS_SIG_VERIF_FAIL;
    }

    return BOOTER_OK;
}

/*!
 * @brief Verifies GSP-RM LS code and data signatures.
 */
BOOTER_STATUS
booterVerifyLsSignatures_TU10X(GspFwWprMeta *pWprMeta)
{
    BOOTER_STATUS status = BOOTER_OK;

    LwU32 falconId = LSF_FALCON_ID_GSP_RISCV;
    LwU64 baseAddr = pWprMeta->gspFwWprStart;
    LwU32 codeSize = LwU64_LO32(pWprMeta->sizeOfBootloader);
    LwU32 codeOffset = LwU64_LO32(pWprMeta->bootBinOffset - baseAddr);
    LwU32 dataSize = LwU64_LO32(pWprMeta->sizeOfRadix3Elf);
    LwU32 dataOffset = LwU64_LO32(pWprMeta->gspFwOffset - baseAddr);
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper = (PLSF_UCODE_DESC_WRAPPER) g_bounceBufferB;

    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (pWprMeta->sysmemAddrOfSignature >> 8);
    LwU32 dmaOff = LwU64_LO32(pWprMeta->sysmemAddrOfSignature) & 0xFF;
    if ((booterIssueDma_HAL(pBooter, g_bounceBufferB, LW_FALSE, dmaOff, pWprMeta->sizeOfSignature,
       BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != pWprMeta->sizeOfSignature)
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    LwU8 *pSignature = (LwU8 *)g_lsSignature;

    // Get the code signature
    if (g_bIsDebug)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_CODE,
                                                                            pLsfUcodeDescWrapper, (void *)pSignature));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_CODE,
                                                                            pLsfUcodeDescWrapper, (void *)pSignature));
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterValidateLsSignature_HAL(pBooter, falconId, pSignature, baseAddr, codeOffset, codeSize, pLsfUcodeDescWrapper, LW_TRUE));

    // Get the data signature
    if (g_bIsDebug)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_DATA,
                                                                            pLsfUcodeDescWrapper, (void *)pSignature));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_DATA,
                                                                            pLsfUcodeDescWrapper, (void *)pSignature));
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterValidateLsSignature_HAL(pBooter, falconId, pSignature, baseAddr, dataOffset, dataSize, pLsfUcodeDescWrapper, LW_FALSE)); 

    // Check if GSP-RM is revoked based on LS ucode version
    {
        BOOTER_STATUS status;
        LwU32 lsUcodeVer;
        LwU32 lsUcodeId;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER, pLsfUcodeDescWrapper, &lsUcodeVer));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLsfUcodeDescWrapperCtrl_HAL(pBooter, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID, pLsfUcodeDescWrapper, &lsUcodeId));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterCheckRevocation(lsUcodeVer));
    }

    return status;
}

/*!
 *@brief Decrypt LS signature for ID = LS_SIG_VALIDATION_ID_1 with RSA algorithm for PKC-LS
 */
static BOOTER_STATUS
_booterRsaDecryptLsSignature
(
    LwU8 *pSignatureEnc,
    SE_KEY_SIZE keySizeBit,
    LwU8 *pSignatureDec
)
{
    SE_PKA_REG sePkaReg;
    BOOTER_STATUS status, tmpStatus;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(seAcquireMutex_HAL(pSe));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(seContextIni_HAL(pSe, &sePkaReg, keySizeBit));

    // set LS RSA key components
    if (g_bIsDebug)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seInitializationRsaKeyDbg_HAL(pSe, g_rsaKeyModulus, g_rsaKeyExponent,
                                                                         g_rsaKeyMp, g_rsaKeyRsqr));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seSetRsaOperationKeys_HAL(pSe, &sePkaReg,
                                                                  g_rsaKeyModulus,
                                                                  g_rsaKeyExponent,
                                                                  g_rsaKeyMp,
                                                                  g_rsaKeyRsqr));
    }
    else
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seInitializationRsaKeyProd_HAL(pSe, g_rsaKeyModulus, g_rsaKeyExponent,
                                                                          g_rsaKeyMp, g_rsaKeyRsqr));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seSetRsaOperationKeys_HAL(pSe, &sePkaReg,
                                                                  g_rsaKeyModulus,
                                                                  g_rsaKeyExponent,
                                                                  g_rsaKeyMp,
                                                                  g_rsaKeyRsqr));
    }

    // decrypt signature
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_HAL(pSe, &sePkaReg, SE_RSA_PARAMS_TYPE_BASE, (LwU32 *)pSignatureEnc));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seStartPkaOperationAndPoll_HAL(pSe, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODEXP,
                                                                   sePkaReg.pkaRadixMask, SE_ENGINE_OPERATION_TIMEOUT_DEFAULT));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaGetParams_HAL(pSe, &sePkaReg, SE_RSA_PARAMS_TYPE_RSA_RESULT, (LwU32 *)pSignatureDec));

    _endianRevert((LwU8 *)pSignatureDec, (keySizeBit >> 3));

Cleanup:
    tmpStatus = seReleaseMutex_HAL(pSe);

    status = (status == BOOTER_OK ? tmpStatus : status);

    return status;
}
