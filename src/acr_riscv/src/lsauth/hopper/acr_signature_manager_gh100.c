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
 * @file: acr_signature_manager_gh100.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acr_signature.h"
#include "sec2shamutex.h"
#include "acr_objacr.h"
#include "acr_objse.h"

/* ------------------------ Application Includes --------------------------- */
#include "dev_se_seb.h"

#include <liblwriscv/sha.h>
#include <liblwriscv/print.h>

#ifdef LS_UCODE_MEASUREMENT
#include <libRTS.h>
#endif // LS_UCODE_MEASUREMENT

#ifdef PKC_LS_RSA3K_KEYS_GH100
#include "acr_lsverif_keys_rsa3k_dbg_gh100.h"
#include "acr_lsverif_keys_rsa3k_prod_gh100.h"
#endif

#include "config/g_acr_private.h"
#include "config/g_se_private.h"

#include "tegra_cdev.h"
#include "tegra_se.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_rsa.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define GET_MOST_SIGNIFICANT_BIT(keySize)      (keySize > 0 ? ((keySize - 1) & 7) : 0)
#define GET_ENC_MESSAGE_SIZE_BYTE(keySize)     (keySize + 7) >> 3;
#define PKCS1_MGF1_COUNTER_SIZE_BYTE           (4)
#define RSA_PSS_PADDING_ZEROS_SIZE_BYTE        (8)

/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
extern ACR_DMA_PROP  g_dmaProp;

/* ------------------------ Function Prototypes ---------------------------- */
static ACR_STATUS _acrEnforcePkcs1Mgf1(LwU8 *pMask, LwU32  maskedLen, LwU8  *pSeed, LwU32 seedLen, SHA_ALGO_ID shaId) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrGetSignatureValidaionId(LwU32 hashAlgoVer, LwU32 hashAlgo, LwU32 sigAlgoVer, LwU32 sigAlgo, LwU32 sigPaddingType, LwU32 *pValidationId) ATTR_OVLY(OVL_NAME);
static void       _endianRevert(LwU8 *pBuf, LwU32 bufSize) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrPlainTextSizeCheck(SHA_ALGO_ID algoId, LwU32 size, LwU32 offset, LwU32 *pSizeNew, LwU32 *pOffsetNew, LwBool *pNeedAdjust) ATTR_OVLY(OVL_NAME);


//
// Signature validation functions
// Supported IDs:
// LS_SIG_VALIDATION_ID_1 - SHA-256 + RSA3K PSS padding
// LS_SIG_VALIDATION_ID_2 - SHA-384 + RSA3K PSS padding
//
static ACR_STATUS _acrRsaDecryptLsSignature(LwU8 *pSignatureEnc, LwU32 keySizeBit, LwU8 *pSignatureDec) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrGeneratePlainTextHash(LwU32 falconId, SHA_ALGO_ID algoId, LwU32 binOffset,
                                                LwU32 binSize, LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrSignatureValidationHandler(LwU32 falconId, SHA_ALGO_ID hashAlgoId, LwU8 *pPlainTextHash, LwU8 *pDecSig,
                                                     LwU32 keySizeBit, LwU32 hashSizeByte, LwU32 saltSizeByte) ATTR_OVLY(OVL_NAME);

static ACR_STATUS _acrGeneratePlainTextHashNonAligned(LwU32 falconId, SHA_ALGO_ID algoId, LwU32 binOffset, LwU32 binSize,
                                                          LwU8 *pBufOut, PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper, LwBool bIsUcode,
                                                          LwU32  offsetNew, LwU32  sizeNew) ATTR_OVLY(OVL_NAME);

#ifdef LS_UCODE_MEASUREMENT
static ACR_STATUS _acrStoreLsUcodeMeasurement(LwU32  falconId, LwU8  *pPlainTextHash) ATTR_OVLY(OVL_NAME);
#endif // LS_UCODE_MEASUREMENT

/* ------------------------ Global Variables ------------------------------- */
LwU32 g_tmpShaBuffer[ACR_TMP_SHA_BUFFER_SIZE_DWORD]        ATTR_ALIGNED(0x100);
LwU8 g_lsSignature[ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE]  ATTR_OVLY(".data") ATTR_ALIGNED(256);
LwU8 g_hashBuffer[ACR_LS_HASH_BUFFER_SIZE_MAX_BYTE]        ATTR_OVLY(".data") ATTR_ALIGNED(4);
LwU8 g_hashResult[ACR_LS_HASH_BUFFER_SIZE_MAX_BYTE]        ATTR_OVLY(".data") ATTR_ALIGNED(4);

#ifdef PKC_LS_RSA3K_KEYS_GH100
// ------------------------ DEBUG key -------------------------------
LwU32 g_rsaKeyModulusDbg_GH100[RSA_KEY_SIZE_3072_DWORD] = {
    0x6404288d, 0xc3efcb20, 0xd0aae02c, 0x06adc570,
    0xb23c4ce6, 0x46ed46ab, 0x4392a48c, 0x9b3aa00d,
    0xb1102d00, 0xf4d95994, 0x5a5ff012, 0x2d5a61e7,
    0xfeab8251, 0x51dda5a3, 0xc475d928, 0xf1036983,
    0xd9207b7b, 0xc3aed23b, 0x6333437d, 0x27c6c5f6,
    0xb140f5ad, 0x02741a48, 0x5327dc45, 0x61b927d2,
    0xb050f67f, 0x4e67771b, 0x404c2842, 0x6e40169d,
    0xdb57e5c5, 0x4afb15a1, 0x3dc2ba0c, 0x6e0d7696,
    0x39e98c31, 0xd0e70fc9, 0x3d7c6406, 0x9315dd98,
    0x6d7a9422, 0xf3b1259b, 0x56911699, 0xf3b34a4a,
    0xe3145b36, 0x1c2f0b42, 0xc74cbbf8, 0xb750c689,
    0x6ca212ed, 0x3110a1ce, 0x75a9bcbf, 0x19ee7f65,
    0x420cb3fe, 0x0771cda1, 0x267fdb1f, 0x693ffa8e,
    0x6baf8baf, 0x4300a914, 0x97478df6, 0x78f8655b,
    0x95ebc445, 0x621fe604, 0x6099c478, 0xc17fc515,
    0x33875702, 0x66b4e135, 0xc2234dcf, 0x5416f007,
    0x06e1ec20, 0xc27f41e5, 0x15eaa226, 0x94da66c8,
    0xb3ed717c, 0x05f82c96, 0x1f649983, 0x1b161a04,
    0x4653a11f, 0x08c44fa9, 0xff5c792f, 0x630a22e3,
    0xbebc1e1d, 0x999fc596, 0xef16b029, 0x9dbff0fc,
    0x1b7eb531, 0xd441f918, 0xeae40439, 0x88b5f9e9,
    0x6f4d308a, 0x35449529, 0xc8337ce3, 0x508eae45,
    0xe059f539, 0x3ddf3dca, 0x598f19a7, 0x2f12abe4,
    0x6af4164f, 0x25a7f417, 0xac4a5570, 0xc6f1a2c0,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    };

LwU8 g_rsaKeyExponentDbg_GH100[4] = {0x00, 0x01, 0x00, 0x01};

// ------------------------ PROD key -------------------------------
LwU32 g_rsaKeyModulusProd_GH100[RSA_KEY_SIZE_3072_DWORD] = {

    0xdc8bc087, 0x50f85825, 0xaee60809, 0x20f7d42c,
    0xafbd36b0, 0xb380ae5f, 0xa63541bb, 0xeed9d7f8,
    0xbb9842dd, 0xf2012b1c, 0x74f42c45, 0xdcacb5e3,
    0xc46f5f8f, 0x0b4819ad, 0x59ccd47f, 0x9c1688da,
    0x300e9cfe, 0xf6e85d80, 0xdbb8abea, 0x18959f50,
    0x87146929, 0x86013fe2, 0x515f86d7, 0x31456b4b,
    0x5c8cb335, 0x8f9ef973, 0x4737dfbf, 0x25c68072,
    0xc472ab3f, 0x1fe8d2f5, 0xe4453b09, 0x5e9f086d,
    0x12c638a7, 0x47718d98, 0x0d80ad03, 0xc71e60d1,
    0x04cc9758, 0x4d1f4e69, 0x87977c4b, 0x7eae7ab5,
    0x5d1e7a95, 0x2f80b255, 0xbed5708b, 0x280e90e9,
    0x9c4fe984, 0xf8686b25, 0xbad98f93, 0xa872b617,
    0xa5537335, 0xcde5c134, 0x36a3a7a8, 0x847443d3,
    0xf74af0e2, 0x7a6e3820, 0xc40849cb, 0x4b3a3572,
    0x5a1300dc, 0xd400b002, 0xbb66892b, 0x86400bd0,
    0x2c901da5, 0x1ea93d8f, 0x99c0daa8, 0xd5253042,
    0xa2423ad4, 0xe55bf3d5, 0x98dbd12d, 0x3eae61d2,
    0x24a30d23, 0x6cd6910e, 0xa7a9b233, 0xc5013e30,
    0x43384fe7, 0xafde22cb, 0x53712f35, 0xcd734376,
    0xa1f067ac, 0x88ab98d2, 0xdcc5746b, 0x302bb08a,
    0x64f178e0, 0xa796a889, 0x5267006b, 0xc4c6dae8,
    0x904fa06e, 0xf0d11bb7, 0x577dd91a, 0x87d7112a,
    0xe21b4266, 0x6073e139, 0x4fea3fda, 0x7322b9d8,
    0x6d663ef8, 0x4151f82d, 0x2001ebd0, 0x88cf6ca2,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    };

LwU8 g_rsaKeyExponentProd_GH100[4] = {0x00, 0x01, 0x00, 0x01};
#endif

/*!
 *@brief To validate LS signature with PKC-LS support and new wpr blob defines
 */
ACR_STATUS
acrValidateLsSignature_GH100
(
    LwU32  falconId,
    LwU8  *pSignature,
    LwU32  binSize,
    LwU32  binOffset,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool bIsUcode
)
{
    LwU32       hashAlgoVer;
    LwU32       hashAlgo;
    LwU32       sigAlgoVer;
    LwU32       sigAlgo;
    LwU32       sigPaddingType;
    LwU32       lsSigValidationId;
    LwU8        *pPlainTextHash = g_hashResult;
    ACR_STATUS  status          = ACR_ERROR_UNKNOWN;
    SHA_ALGO_ID hashAlgoId      = SHA_ALGO_ID_LAST;
    LwU32       hashSizeByte    = 0;
    LwU32       saltSizeByte    = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO_VER, pLsfUcodeDescWrapper, (void *)&hashAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO, pLsfUcodeDescWrapper, (void *)&hashAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_VER, pLsfUcodeDescWrapper, (void *)&sigAlgoVer));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO, pLsfUcodeDescWrapper, (void *)&sigAlgo));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_PADDING_TYPE, pLsfUcodeDescWrapper, (void *)&sigPaddingType));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetSignatureValidaionId(hashAlgoVer, hashAlgo, sigAlgoVer, sigAlgo, sigPaddingType, &lsSigValidationId));

#ifdef ACR_FMODEL_BUILD
    if (lsSigValidationId == LS_SIG_VALIDATION_ID_1)
    {
        hashAlgoId = SHA_ALGO_ID_SHA_256;
        hashSizeByte = SHA_256_HASH_SIZE_BYTE;
        saltSizeByte = SHA_256_HASH_SIZE_BYTE;
    }
#endif //ACR_FMODEL_BUILD
    if (lsSigValidationId == LS_SIG_VALIDATION_ID_2)
    {
        hashAlgoId = SHA_ALGO_ID_SHA_384;
        hashSizeByte = SHA_384_HASH_SIZE_BYTE;
        saltSizeByte = SHA_384_HASH_SIZE_BYTE;
    }

    switch (lsSigValidationId)
    {
#ifdef ACR_FMODEL_BUILD
        case LS_SIG_VALIDATION_ID_1: // Id 1 : rsa3k-pss padding with SHA256 hash algorithm
#endif //ACR_FMODEL_BUILD
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

            // Store plain text hash as ucode measurement
#ifdef LS_UCODE_MEASUREMENT
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrStoreLsUcodeMeasurement(falconId, pPlainTextHash));
#endif // LS_UCODE_MEASUREMENT
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
        switch(hashAlgo)
        {
#ifdef ACR_FMODEL_BUILD
            case LSF_HASH_ALGO_TYPE_SHA256:
            {
                *pValidationId  = LS_SIG_VALIDATION_ID_1;
                break;
            }
#endif //ACR_FMODEL_BUILD
            case LSF_HASH_ALGO_TYPE_SHA384:
            {
                *pValidationId  = LS_SIG_VALIDATION_ID_2;
                break;
            }
            default:
            {
                return ACR_ERROR_LS_SIG_VERIF_FAIL;
            }
        }
    }
    else
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    return ACR_OK;
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

/*
 * @brief Determine the HASH size depending on the HASH algorithm Id
 * @parma [in]  algoId : SHA algo Id
 * @param [out] pSize  : Hash size to be returned.
 * @return ACR_OK on success else ACR_ERROR_SHA_ILWALID_CONFIG
 */
static ACR_STATUS
_acrGetShaHashSizeByte
(
    SHA_ALGO_ID  algoId,
    LwU32        *pSize
)
{
    if (pSize == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (algoId)
    {
#ifdef ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_1:
             *pSize = SHA_1_HASH_SIZE_BYTE;
             break;
        case SHA_ALGO_ID_SHA_224:
              *pSize = SHA_224_HASH_SIZE_BYTE;
              break;
        case SHA_ALGO_ID_SHA_256:
             *pSize = SHA_256_HASH_SIZE_BYTE;
             break;
#endif //ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_384:
             *pSize = SHA_384_HASH_SIZE_BYTE;
             break;
#ifdef ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_512:
            *pSize = SHA_512_HASH_SIZE_BYTE;
            break;
        case SHA_ALGO_ID_SHA_512_224:
            *pSize = SHA_512_224_HASH_SIZE_BYTE;
            break;
        case SHA_ALGO_ID_SHA_512_256:
            *pSize = SHA_512_256_HASH_SIZE_BYTE;
            break;
#endif //ACR_FMODEL_BUILD
         default:
              return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    return ACR_OK;
}

/*
 * @brief Get Hash size in block bytes based on SHA Algo id
 * @param [in]  algoId: SHA Algo Id
 * @param [out] pBlockSizeByte : SHA HASH in block size bytes
 * @return ACR_STATUS ACR_OK if sucessfull else ACR_ERROR_SHA_ILWALID_CONFIG
 */

static ACR_STATUS
_acrGetShaHashBlockSizeByte
(
   SHA_ALGO_ID algoId,
   LwU32  *pBlockSizeByte
)
{
    switch (algoId)
    {
#ifdef ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_1:
        case SHA_ALGO_ID_SHA_224:
        case SHA_ALGO_ID_SHA_256:
        {
            *pBlockSizeByte = SHA_256_BLOCK_SIZE_BYTE;
        }
        break;
#endif //ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_384:
#ifdef ACR_FMODEL_BUILD
        case SHA_ALGO_ID_SHA_512:
        case SHA_ALGO_ID_SHA_512_224:
        case SHA_ALGO_ID_SHA_512_256:
#endif //ACR_FMODEL_BUILD
        {
            *pBlockSizeByte = SHA_512_BLOCK_SIZE_BYTE;
        }
        break;
        default:
            return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    return ACR_OK;
}

/*!
 *@brief Enforce PKCS1_MGF1 process
 */
static ACR_STATUS
_acrEnforcePkcs1Mgf1
(
    LwU8        *pMask,
    LwU32       maskedLen,
    LwU8        *pSeed,
    LwU32       seedLen,
    SHA_ALGO_ID shaId
)
{
    SHA_CONTEXT     shaCtx;
    SHA_TASK_CONFIG taskCfg;
    ACR_STATUS      status     = ACR_ERROR_UNKNOWN;
    LwU32           outLen     = 0, i, j;
    LwU8           *pFinalBuf  = pMask;
    LwU8           *pTempStart = (LwU8 *)g_tmpShaBuffer;
    LwU8           *pTemp      = pTempStart;
    LwU32           mdLen;
    LWRV_STATUS    shaStatus   = LWRV_ERR_SHA_ENG_ERROR;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetShaHashSizeByte(shaId, &mdLen));
    CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    //
    // seedLen either equals to 0 or hash size(e.g sh256 hash  size is 32 bytes).
    // g_tmpShaBuffer size is 64 bytes; and its size is large enough to cover all combinations.
    // We still need to add size sanity check to pass coverity check.
    //
    if ((seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE) > ACR_TMP_SHA_BUFFER_SIZE_BYTE)
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
    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = LW_TRUE;
    taskCfg.dmaIdx         = 0;
    taskCfg.size           = seedLen + PKCS1_MGF1_COUNTER_SIZE_BYTE;
    taskCfg.addr           = (LwU64)(pTempStart);

    shaCtx.algoId          = shaId;

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

        CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit(&shaCtx), ACR_ERROR_LIBLWRISCV_SHA_ERROR);
        CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask(&shaCtx, &taskCfg), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

        if (outLen + mdLen <= maskedLen)
        {
            shaCtx.pBufOut = pFinalBuf;
            shaCtx.bufSize = mdLen;
            CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult(&shaCtx, LW_TRUE), ACR_ERROR_LIBLWRISCV_SHA_ERROR);
            outLen += mdLen;
            pFinalBuf += mdLen;
        }
        else
        {
            //The last operation, so we can overwrite pTempStart
            shaCtx.pBufOut = pTempStart;
            shaCtx.bufSize = mdLen;
            CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult(&shaCtx, LW_TRUE), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

            for (j = 0; j < maskedLen - outLen; j++)
            {
                pFinalBuf[j] = pTempStart[j];
            }
            outLen = maskedLen;
        }
        i++;
    }

Cleanup:
    shaStatus = shaReleaseMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    if (status == ACR_OK)
    {
        CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaStatus, ACR_ERROR_LIBLWRISCV_SHA_ERROR);
    }

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
    SHA_ALGO_ID algoId,
    LwU32       size,
    LwU32       offset,
    LwU32       *pSizeNew,
    LwU32       *pOffsetNew,
    LwBool      *pNeedAdjust
)
{
    ACR_STATUS status = ACR_ERROR_ILWALID_ARGUMENT;
    LwU32 blockSizeByte;
    LwU32 blockCount;
    LwU64 temp64;
    LwU32 sizeRemiander;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetShaHashBlockSizeByte(algoId, &blockSizeByte));

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
       temp64 = (LwU64)blockCount * blockSizeByte;
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
       |     ccodeId      |  |
       |     (DMEM)       |  |
       |==================|  |
       | Dep Map Context  |  |
       |     (DMEM)       |  |
       |==================| ---
*/
static ACR_STATUS
_acrGeneratePlainTextHashNonAligned
(
    LwU32                   falconId,
    SHA_ALGO_ID             algoId,
    LwU32                   binOffset,
    LwU32                   binSize,
    LwU8                    *pBufOut,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool                  bIsUcode,
    LwU32                   offsetNew,
    LwU32                   sizeNew
)
{
    LwU64            binAddr;
    LwU32            plainTextSize;
    SHA_CONTEXT      shaCtx;
    SHA_TASK_CONFIG  taskCfg;
    ACR_STATUS       status;
    LwU32            depMapSize;
    LwU8            *pBufferByte = (LwU8 *)g_tmpGenericBuffer;
    LwU8            *pBufferByteOriginal = pBufferByte;
    LwU32            remainderSize;
    LwU32            temp32;
    LwU32            hashSizeByte;
    LWRV_STATUS      shaStatus = LWRV_ERR_SHA_ENG_ERROR;

    // The aligned size is always less than original size.
    if (sizeNew >= binSize)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    remainderSize = binSize - sizeNew;

    // Get dependency map size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetShaHashSizeByte(algoId, &hashSizeByte));

    CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION), ACR_ERROR_LIBLWRISCV_SHA_ERROR);
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    shaCtx.bufSize = hashSizeByte;
    shaCtx.pBufOut = pBufOut;
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit(&shaCtx), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    // Task-1: SHA hash callwlation of bin data stored at FB but only for aligned size region
    binAddr                = ((g_dmaProp.wprBase << 8) + binOffset);
    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_FB;
    taskCfg.bDefaultHashIv = LW_TRUE;
    taskCfg.dmaIdx         = g_dmaProp.ctxDma;
    taskCfg.size           = sizeNew;
    taskCfg.addr           = binAddr;
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask(&shaCtx, &taskCfg), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    //
    // Leverage g_tmpGenericBuffer as temp buffer to save remainder bytes in FB first.
    // Always read last 256 byte to temp buffer and concanate falocnId, ucodeVer, ucodeId and dep map context
    // into the buffer.
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrIssueDma_HAL(pAcr, pBufferByte, LW_FALSE, offsetNew, ACR_FALCON_BLOCK_SIZE_IN_BYTES,
                                    ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    // move pointer to the last byte and copy notification and dep map context
    pBufferByte += remainderSize;

    // Don't use doword pointer to copy dword value because pBuffreByte could NOT be dword aigned pointer.
    // In case we cast a pointer which contained addrees is NOT dword aligned poineter
    if (!acrMemcpy_HAL(pAcr, (void *)pBufferByte, (void *)&falconId, LW_SIZEOF32(falconId)))
    {
        goto Cleanup;
    }

    pBufferByte += LW_SIZEOF32(falconId);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&temp32));
    if (!acrMemcpy_HAL(pAcr, (void *)pBufferByte, (void *)&temp32, LW_SIZEOF32(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += LW_SIZEOF32(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                        pLsfUcodeDescWrapper,  (void *)&temp32));

    if (!acrMemcpy_HAL(pAcr, (void *)pBufferByte, (void *)&temp32, LW_SIZEOF32(temp32)))
    {
        goto Cleanup;
    }

    pBufferByte += LW_SIZEOF32(temp32);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
                                                                        pLsfUcodeDescWrapper,  (void *)pBufferByte));

    binAddr                = (LwU64)(pBufferByteOriginal);
    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = LW_FALSE;
    taskCfg.dmaIdx         = 0;
    taskCfg.size           = remainderSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;
    taskCfg.addr           = binAddr;

    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask(&shaCtx, &taskCfg), ACR_ERROR_LIBLWRISCV_SHA_ERROR);


    // read hash result out
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult(&shaCtx, LW_TRUE), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

Cleanup:
    shaStatus = shaReleaseMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    if (status == ACR_OK)
    {
        CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaStatus, ACR_ERROR_LIBLWRISCV_SHA_ERROR);
    }

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
   |     ccodeId      |  /
   |     (DMEM)       |  |
   |==================|  |
   | Dep Map Context  |  |
   |     (DMEM)       |  |
   |==================| ---

 */
static ACR_STATUS
_acrGeneratePlainTextHash
(
    LwU32                   falconId,
    SHA_ALGO_ID             algoId,
    LwU32                   binOffset,
    LwU32                   binSize,
    LwU8                    *pBufOut,
    PLSF_UCODE_DESC_WRAPPER pLsfUcodeDescWrapper,
    LwBool                  bIsUcode
)
{
    LwU64            binAddr;
    LwU32            plainTextSize;
    SHA_CONTEXT      shaCtx;
    SHA_TASK_CONFIG  taskCfg;
    ACR_STATUS       status;
    LwU32            depMapSize;
    LwU32           *pBufferDw;
    LwBool           bNeedAdjust = LW_FALSE;
    LwU32            sizeNew;
    LwU32            offsetNew;
    LwU32            hashSizeByte;
    LWRV_STATUS      shaStatus   = LWRV_ERR_SHA_ENG_ERROR;

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
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE,
                                                                        pLsfUcodeDescWrapper, (void *)&depMapSize));

    plainTextSize = binSize + LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetShaHashSizeByte(algoId, &hashSizeByte));

    CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION), ACR_ERROR_LIBLWRISCV_SHA_ERROR);
    shaCtx.algoId  = algoId;
    shaCtx.msgSize = plainTextSize;
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit(&shaCtx), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    // Task-1: SHA hash callwlation of bin data stored at FB
    binAddr                = ((g_dmaProp.wprBase << 8) + binOffset);
    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_FB;
    taskCfg.bDefaultHashIv = LW_TRUE;
    taskCfg.dmaIdx         = g_dmaProp.ctxDma;
    taskCfg.size           = binSize;
    taskCfg.addr           = binAddr;
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask(&shaCtx, &taskCfg), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    //
    // Task-2 : Leverage g_tmpGenericBuffer as temp buffer to save concatenation components
    // bin || falconId || lsUcodeVer || lsUocdeId || depMapCtx
    //
    pBufferDw    = (LwU32 *)g_tmpGenericBuffer;
    pBufferDw[0] = falconId;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER,
                                                                      pLsfUcodeDescWrapper, (void *)&pBufferDw[1]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID,
                                                                      pLsfUcodeDescWrapper,  (void *)&pBufferDw[2]));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX,
                                                                      pLsfUcodeDescWrapper,  (void *)&pBufferDw[3]));

    binAddr                = (LwU64)(pBufferDw);
    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = LW_FALSE;
    taskCfg.dmaIdx         = 0;
    taskCfg.size           = LS_SIG_VALIDATION_ID_1_NOTIONS_SIZE_BYTE + depMapSize;
    taskCfg.addr           = binAddr;
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask(&shaCtx, &taskCfg), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    shaCtx.pBufOut = pBufOut;
    shaCtx.bufSize = hashSizeByte;
    // read hash result out
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult(&shaCtx, LW_TRUE), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

Cleanup:
    shaStatus = shaReleaseMutex(ACR_AHESASC_LS_SIGNATURE_VALIDATION);

    if (status == ACR_OK)
    {
        CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaStatus, ACR_ERROR_LIBLWRISCV_SHA_ERROR);
    }

    return status;
}

/*!
 *
 * @brief Signature handler:
 * LS_SIG_VALIDATION_ID_1 (RSA3K-PSS padding + SHA-256 hash)
 * LS_SIG_VALIDATION_ID_2 (RSA3K-PSS padding + SHA-384 hash)
 *
 *@brief Signature handler for ID = LS_SIG_VALIDATION_ID_1 (RSA3K-PSS padding + SHA-256 hash)
 */
static ACR_STATUS
_acrSignatureValidationHandler
(
    LwU32       falconId,
    SHA_ALGO_ID hashAlgoId,
    LwU8        *pPlainTextHash,
    LwU8        *pDecSig,
    LwU32       keySizeBit,
    LwU32       hashSizeByte,
    LwU32       saltSizeByte
)
{
    ACR_STATUS      status            = ACR_ERROR_UNKNOWN;
    LwU32           msBits            = GET_MOST_SIGNIFICANT_BIT(keySizeBit);
    LwU32           emLen             = GET_ENC_MESSAGE_SIZE_BYTE(keySizeBit);
    LwU8            *pEm              = pDecSig;
    LwU8            *pDb              = NULL;
    LwU8            *pHashGoldenStart = NULL;
    LwU8            *pHashGolden      = NULL;
    LwU8            *pHash            = NULL;
    LwU32           msgGoldenLen;
    SHA_TASK_CONFIG taskCfg;
    SHA_CONTEXT     shaCtx;
    LwU32           maskedDBLen, i, j;

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

    // g_tmpGenericBuffer is used as a temp buffer
    pDb = (LwU8 *)g_tmpGenericBuffer;

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
        pDb[0] &= (LwU8)(0xFF >> (8 - msBits));
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

    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = LW_TRUE;
    taskCfg.dmaIdx         = 0;
    taskCfg.size           = msgGoldenLen;
    taskCfg.addr           = (LwU64)(pHashGoldenStart);

    shaCtx.algoId          = hashAlgoId;
    shaCtx.msgSize         = msgGoldenLen;
    shaCtx.mutexToken      = ACR_AHESASC_LS_SIGNATURE_VALIDATION;
    shaCtx.pBufOut         = pHashGoldenStart;
    
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetShaHashSizeByte(hashAlgoId, &(shaCtx.bufSize)));

    CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(shaRunSingleTaskCommon(&taskCfg, &shaCtx), ACR_ERROR_LIBLWRISCV_SHA_ERROR);

    // compare signature using acrMemcmp
    if (acrMemcmp(pHash, pHashGoldenStart, hashSizeByte) != 0)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }

    return ACR_OK;
}

#ifdef PKC_LS_RSA3K_KEYS_GH100

/*!
 * @brief Decrypt LS signature for ID = (LS_SIG_VALIDATION_ID_1 / LS_SIG_VALIDATION_ID_2)
 * with RSA algorithm for PKC-LS by CCC library
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
    uint8_t                      *pExp = NULL;
    uint8_t                      *pModulus = NULL;


    // set LS RSA key components
    if (acrIsDebugModeEnabled_HAL(pAcr))
    {
        pExp = (uint8_t *)g_rsaKeyExponentDbg_GH100;
        expSize = sizeof(g_rsaKeyExponentDbg_GH100);
        pModulus = (uint8_t *)g_rsaKeyModulusDbg_GH100;
    }
    else
    {
        pExp = (uint8_t *)g_rsaKeyExponentProd_GH100;
        expSize = sizeof(g_rsaKeyExponentProd_GH100);
        pModulus = (uint8_t *)g_rsaKeyModulusProd_GH100;
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
    rsa_econtext.rsa_flags       |= RSA_FLAGS_BIG_ENDIAN_EXPONENT;
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
#endif // PKC_LS_RSA3K_KEYS_GH100

#ifdef LS_UCODE_MEASUREMENT
/*!
 *@brief Store LS ucode measurement for given falconId
 */
ACR_STATUS
_acrStoreLsUcodeMeasurement
(
    LwU32  falconId,
    LwU8  *pPlainTextHash
)
{
    ACR_STATUS          status      = ACR_OK;
    ACR_FLCN_CONFIG     flcnCfg;
    RTS_ERROR           rtsError;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(rtsStoreMeasurement(&rtsError, (LwU32 *)pPlainTextHash, flcnCfg.msrIndex, LW_FALSE, LW_TRUE), ACR_ERROR_RTS_STORE_MEASUREMENT_FAILED);

    return status;
}
#endif // LS_UCODE_MEASUREMENT
