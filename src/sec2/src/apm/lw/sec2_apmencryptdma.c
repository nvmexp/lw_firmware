/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_apmencryptdma.c
 * @brief  Encryption/decryption functions for APM methods
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_csb.h"


/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"
#include "sec2_chnmgmt.h"
#include "apm/apm_mthds.h"
#include "apm/sec2_apm.h"
#include "sec2_objsec2.h"
#include "sec2_hostif.h"
#include "lib_intfcshahw.h"
#include "class/cl95a1.h"
#include "apm/sec2_apmscpcrypt.h"
#include "apm/apm_keys.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define  APM_DMA_BUFFER_SIZE_IN_BYTES   4096
#define  APM_DMA_BUFFER_ALIGNMENT       256
#define  APM_DMA_BUFFER_SIZE_IN_WORDS   APM_DMA_BUFFER_SIZE_IN_BYTES/4
#define  APM_AES_BLOCK_SIZE_IN_BYTES    16
#define  APM_HMAC_DIGEST_SIZE_IN_BYTES  SHA_256_HASH_SIZE_BYTE
#define  APM_DMA_HMAC_MUTEX             128
#define  APM_MIN_CRYPT_SIZE_IN_BYTES    16

/* ------------------------- Global Variables ------------------------------- */

/*!
 * Global buffers used for cryptography operations.
 */
static LwU8 g_apmBuf[APM_DMA_BUFFER_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmBuf");

static LwU8 g_apmDigestCalc[APM_HMAC_DIGEST_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmDigest");

static LwU8 g_apmDigestRead[APM_HMAC_DIGEST_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmDigestRead");

static LwU8 g_apmIVCopy[APM_IV_SIZE_IN_BYTES]
    GCC_ATTRIB_ALIGN(APM_IV_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmIVCopy");

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
LwU32 apmDoCryptedDma(APM_COPY_DATA *pApmCopyData, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_apm", "_apmDoCryptedDma");
LwU32 apmDecryptDma(APM_COPY_DATA *pApmCopyData)
    GCC_ATTRIB_SECTION("imem_apm", "_apmDecryptDma");
LwU32 apmEncryptDma(APM_COPY_DATA *pApmCopyData)
    GCC_ATTRIB_SECTION("imem_apm", "_apmEncryptDma");
LwU32 apmInitiateCopy(void)
    GCC_ATTRIB_SECTION("imem_apm", "apmInitiateCopy");

/* ------------------------- Public Functions ------------------------------- */

/*!
 *  @brief Handles encrypt/decrypt to/from sysmem/VPR.
 * 
 *         In addition to encrypt/decrypt, this function also callwlates the HMAC digest
 *         using the SHA HW library.
 */
LwU32
apmDoCryptedDma(APM_COPY_DATA *pApmCopyData, LwBool bIsEncrypt)
{
    FLCN_STATUS      flcnStatus = FLCN_OK;
    RM_FLCN_MEM_DESC apmSrcDesc;
    RM_FLCN_MEM_DESC apmDstDesc;
    RM_FLCN_MEM_DESC clientMemDesc;  
    HMAC_CONTEXT     hmacContext;
    LwU32            copyBytesProcessed         = 0;
    LwU32            copyBytesLwrrentProcessing = 0;
    LwU32            copyBytesPadCryptSize      = 0;
    LwU32            extraBytes                 = 0;
    LwU32            intStatus                  = 0;
    LwU64            shaSrcAddr                 = 0;
    LwU8             *pApmBuf        = (LwU8 *)&g_apmBuf;
    LwU8             *pApmDigestCalc = (LwU8 *)&g_apmDigestCalc;
    LwU8             *pApmKey        = (LwU8 *)&g_apmSymmetricKey;
    LwU8             *pApmIv         = (LwU8 *)&g_apmIVCopy;
    LwU8             *pApmDigestRead = (LwU8 *)&g_apmDigestRead;
    LwU64            ivAddress       = (LwU32) &g_apmIV; 
    LwU32            regionCfg;
    LwU32            vprRegionCfg;
    LwU32            unprotRegionCfg;
    LwBool           bShaMtxAcquired = LW_FALSE;
    LwU32            errorCode       = LW95A1_ERROR_NONE;

    //
    // Save FBIF_REGIONCFG so that we can restore at the end.
    //
    if (CSB_REG_RD32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, &regionCfg) != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
        SEC2_HALT();
    }

    //
    // setup vpr and unprotected region config to use for read/write later
    //
    vprRegionCfg    = FLD_IDX_SET_DRF_NUM(_CSEC, _FBIF_REGIONCFG, _T,
                      RM_SEC2_DMAIDX_VIRT, LSF_VPR_REGION_ID, regionCfg);
    unprotRegionCfg = FLD_IDX_SET_DRF_NUM(_CSEC, _FBIF_REGIONCFG, _T,
                      RM_SEC2_DMAIDX_VIRT, LSF_UNPROTECTED_REGION_ID, regionCfg);

    //
    // Setup source and dest memory descriptors
    //
    apmSrcDesc.address.lo = (pApmCopyData->apmCopySrcLo );
    apmSrcDesc.address.hi = pApmCopyData->apmCopySrcHi;
    apmSrcDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                         RM_SEC2_DMAIDX_VIRT, 0 );

    apmDstDesc.address.lo = (pApmCopyData->apmCopyDstLo );
    apmDstDesc.address.hi = pApmCopyData->apmCopyDstHi;
    apmDstDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                         RM_SEC2_DMAIDX_VIRT, 0 );

    //
    // Initialize HMAC context and operation
    //
    memset((LwU8 *)&hmacContext, 0, sizeof(hmacContext));
    hmacContext.algoId = SHA_ID_SHA_256;
    hmacContext.msgSize = pApmCopyData->apmCopySizeBytes;
    hmacContext.mutexId = APM_DMA_HMAC_MUTEX;
    hmacContext.keySize = APM_KEY_SIZE_IN_BYTES;

    //
    // Copy key into context buffer.
    // Assumes APM_KEY_SIZE_IN_BYTES <= SHA_256_BLOCK_SIZE_BYTE.
    //
    memcpy(hmacContext.keyBuffer, pApmKey, hmacContext.keySize);

    //
    // Initialize SHA HW. Until we own mutex, return directly.
    //
    if (shahwSoftReset() != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
        goto ErrorExit;
    }

    if (shahwAcquireMutex(hmacContext.mutexId) != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
        goto ErrorExit;
    }
    else
    {
        bShaMtxAcquired = LW_TRUE;
    }

    if (shahwHmacSha256Init(&hmacContext, &intStatus) != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
        goto ErrorExit;
    }

    //
    // Make a copy of the IV into another buffer. The IV is updated automatically by
    // apmScpCtrCryptWithKey. We need to make a copy of IV to use within encrypt/decrypt
    // loop while preserving original IV for subsequent calls for encryption/decryption.
    //
    memcpy(g_apmIVCopy, g_apmIV, APM_IV_SIZE_IN_BYTES);

    //
    // Process the largest amount of data possible at one time
    //
    copyBytesProcessed = 0;
    while (copyBytesProcessed < pApmCopyData->apmCopySizeBytes)
    {
        copyBytesLwrrentProcessing = pApmCopyData->apmCopySizeBytes - copyBytesProcessed;
        if (copyBytesLwrrentProcessing > APM_DMA_BUFFER_SIZE_IN_BYTES)
        {
            copyBytesLwrrentProcessing = APM_DMA_BUFFER_SIZE_IN_BYTES;
        }

        //
        // Wait here to ensure any SHA operation on buffer has finished,
        // before we clobber with new data read.
        //
        while ((flcnStatus = shahwWaitTaskComplete(&intStatus)) == FLCN_ERR_TIMEOUT) {}
        if (flcnStatus != FLCN_OK)
        {
            errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
            goto ErrorExit;
        }

        //
        // Setup DMA Size params
        //
        apmSrcDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, copyBytesLwrrentProcessing, apmSrcDesc.params);
        apmDstDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, copyBytesLwrrentProcessing, apmDstDesc.params);

        //
        // Setup REGIONCFG for the DMA Read.
        // If encrypt == TRUE, we are reading from VPR.
        // If decrypt == TRUE, we are reading from unprotected.
        //
        if (bIsEncrypt)
        {
            if (CSB_REG_WR32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, vprRegionCfg) != FLCN_OK)
            {
                errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
                SEC2_HALT();
            }
        }
        else
        {
            if (CSB_REG_WR32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, unprotRegionCfg) != FLCN_OK)
            {
                errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
                SEC2_HALT();
            }
        }

        //
        // Perform dma read of data to be encrypted/decrypted to temporary buffer.
        //
        if ((flcnStatus = dmaReadUnrestricted(pApmBuf, &apmSrcDesc, copyBytesProcessed, copyBytesLwrrentProcessing)) != FLCN_OK)
        {
            errorCode = (flcnStatus == FLCN_ERR_DMA_NACK) ?
                LW95A1_ERROR_APM_COPY_DMA_NACK : LW95A1_OS_ERROR_DMA_CONFIG;
            goto ErrorExit;
        }

        //
        // If we are doing decryption, callwlate HMAC on the data before doing the decryption.
        //
        if (!bIsEncrypt)
        {
            //
            // Set SHA data address.  We aren't using the shortlwt so this is DMEM.
            // Wait for completion, as otherwise we have race condition with SCP.
            //
            shaSrcAddr = (LwU64)(LwU32)(pApmBuf);
            if (shahwHmacSha256Update(&hmacContext, copyBytesLwrrentProcessing, shaSrcAddr,
                                       SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_TRUE) != FLCN_OK)
            {
                errorCode =  LW95A1_OS_ERROR_SHA_FAILURE;
                goto ErrorExit;
            }
        }

        //
        // Uses AES-CTR mode to perform encryption / decryption. Pass LW_TRUE for the parameter bOutputIv
        // to update the IV automatically.
        //
        // If the copy size is not a multiple of 16B then we have to pad the data to be a multiple of 16B.
        // SCP operates on 16B blocks. Since the local temp buffer, pAmpBuf is a multiple of 16B we will
        // have enough room to pad.
        // 
        copyBytesPadCryptSize = 0;
        extraBytes = copyBytesLwrrentProcessing % APM_MIN_CRYPT_SIZE_IN_BYTES;
        if (extraBytes != 0)
        {
            //
            // First callwlate the number of bytes to be cleared in temp buffer
            //
            copyBytesPadCryptSize = (APM_MIN_CRYPT_SIZE_IN_BYTES) - extraBytes;
            memset(pApmBuf + copyBytesLwrrentProcessing, 0, copyBytesPadCryptSize);
        }

        if (apmScpCtrCryptWithKey(pApmKey, LW_FALSE, pApmBuf,
                                 (copyBytesLwrrentProcessing + copyBytesPadCryptSize),
                                  pApmBuf, pApmIv, LW_TRUE) != FLCN_OK)
        {
            errorCode = LW95A1_OS_ERROR_AES_CTR_FAILURE;
            goto ErrorExit;
        }

        //
        // If we are doing encryption, callwlate HMAC on the data after doing the encryption.
        //
        if (bIsEncrypt)
        {
            //
            // Set SHA data address.  We aren't using the shortlwt so this is DMEM.
            // We don't need to wait for completion now, as no race condition with DMA write.
            // However, we must wait for completion before writing any new data to buffer.
            //
            shaSrcAddr = (LwU64)(LwU32)(pApmBuf);
            if (shahwHmacSha256Update(&hmacContext, copyBytesLwrrentProcessing, shaSrcAddr,
                                       SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_FALSE) != FLCN_OK)
            {
                errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
                goto ErrorExit;
            }
        }

        //
        // Setup REGIONCFG for the DMA write
        // If encrypt == TRUE, we are writing to unprotected.
        // If decrypt == TRUE, we are writing to protected.
        //
        if (bIsEncrypt)
        {
            if (CSB_REG_WR32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, unprotRegionCfg) != FLCN_OK)
            {
                errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
                SEC2_HALT();
            }
        }
        else
        {
            if (CSB_REG_WR32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, vprRegionCfg) != FLCN_OK)
            {
                errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
                SEC2_HALT();
            }
        }

        //
        // Perform dma write of data from temporary buffer to destination.
        //
        if ((flcnStatus = dmaWriteUnrestricted(pApmBuf, &apmDstDesc, copyBytesProcessed, copyBytesLwrrentProcessing)) != FLCN_OK)
        {
            errorCode = (flcnStatus == FLCN_ERR_DMA_NACK) ?
                LW95A1_ERROR_APM_COPY_DMA_NACK : LW95A1_OS_ERROR_DMA_CONFIG;
            goto ErrorExit;
        }
        copyBytesProcessed += copyBytesLwrrentProcessing;
    }

    sec2FbifFlush_HAL();

    //
    // Restore FBIF_REGIONCFG now that we are done reading/writing VPR.
    //
    if (CSB_REG_WR32_ERRCHK(LW_CSEC_FBIF_REGIONCFG, regionCfg) != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_REGISTER_ACCESS;
        SEC2_HALT();
    }

    //
    // Output the HMAC digest to a temporary buffer
    //
    if (shahwHmacSha256Final(&hmacContext, (LwU32 *)pApmDigestCalc, LW_TRUE, &intStatus) != FLCN_OK)
    {
        errorCode = LW95A1_OS_ERROR_SHA_FAILURE;
        goto ErrorExit;
    }

    //
    // Perform final encryption/decryption steps.
    //
    if (bIsEncrypt)
    {
        // 
        // Copy the digest out to the method address for the client
        //
        clientMemDesc.address.lo = pApmCopyData->apmDigestAddrLo ;
        clientMemDesc.address.hi = pApmCopyData->apmDigestAddrHi;
        clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                                RM_SEC2_DMAIDX_VIRT, 0 );
        clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, APM_HMAC_DIGEST_SIZE_IN_BYTES, clientMemDesc.params);
        if ((flcnStatus = dmaWrite(&pApmDigestCalc[0], &clientMemDesc, 0, APM_HMAC_DIGEST_SIZE_IN_BYTES)) != FLCN_OK)
        {
            errorCode = (flcnStatus == FLCN_ERR_DMA_NACK) ?
                LW95A1_ERROR_APM_COPY_DMA_NACK : LW95A1_OS_ERROR_DMA_CONFIG;
            goto ErrorExit;
        }

        if (pApmCopyData->bApmEncryptIVSent)
        {
            // 
            // Copy the IV address out to the methods for the client
            //
            clientMemDesc.address.lo = pApmCopyData->apmEncryptIVAddrLo;
            clientMemDesc.address.hi = pApmCopyData->apmEncryptIVAddrHi;
            clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                RM_SEC2_DMAIDX_VIRT, 0);
            clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, sizeof(LwU64), clientMemDesc.params);
            if ((flcnStatus = dmaWrite(&ivAddress, &clientMemDesc, 0, sizeof(LwU64))) != FLCN_OK)
            {
                errorCode = (flcnStatus == FLCN_ERR_DMA_NACK) ?
                    LW95A1_ERROR_APM_COPY_DMA_NACK : LW95A1_OS_ERROR_DMA_CONFIG;
                goto ErrorExit;
            }
        }
    }
    else
    {
        // 
        // Compare the computed HMAC digest to the values read from the methods
        //

        //
        // Read the HMAC digest values passed from client
        //
        clientMemDesc.address.lo = pApmCopyData->apmDigestAddrLo ;
        clientMemDesc.address.hi = pApmCopyData->apmDigestAddrHi;
        clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                                RM_SEC2_DMAIDX_VIRT, 0 );
        clientMemDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, APM_HMAC_DIGEST_SIZE_IN_BYTES, clientMemDesc.params); 
        if ((flcnStatus = dmaRead(&pApmDigestRead[0], &clientMemDesc, 0, APM_HMAC_DIGEST_SIZE_IN_BYTES)) != FLCN_OK)
        {
            errorCode = (flcnStatus == FLCN_ERR_DMA_NACK) ?
                LW95A1_ERROR_APM_COPY_DMA_NACK : LW95A1_OS_ERROR_DMA_CONFIG;
            goto ErrorExit;
        }

        //
        // Compare the callwlated HMAC digest values
        //
        if (memcmp(pApmDigestRead, pApmDigestCalc, APM_HMAC_DIGEST_SIZE_IN_BYTES) != 0)
        {
            errorCode = LW95A1_ERROR_APM_DIGEST_CHECK_FAILURE;
            goto ErrorExit;
        }
    }

ErrorExit:
    //
    // Best effort to release mutex
    //
    if (bShaMtxAcquired)
    {
        (void)shahwReleaseMutex(hmacContext.mutexId);
    }

    return errorCode;
}

/*!
 *  @brief Calls apmDoCryptedDma for decryption case.
 */
LwU32
apmDecryptDma(APM_COPY_DATA *pApmCopyData)
{
  return (apmDoCryptedDma(pApmCopyData, LW_FALSE));
}

/*!
 *  @brief Calls apmDoCryptedDma for encryption case.
 */
LwU32
apmEncryptDma(APM_COPY_DATA *pApmCopyData)
{
  return (apmDoCryptedDma(pApmCopyData, LW_TRUE));
}

/*!
 *  @brief The function initiates the encryption/decryption process.
 *
 *         This function checks that all methods required for encypt/decrypt
 *         have been sent.  It also checks the source and destination address 
 *         alignment.
 */
LwU32
apmInitiateCopy(void)
{
    APM_COPY_DATA    apmCopyData = { 0 };
    LwU32            errorCode   = LW95A1_ERROR_NONE;

    //
    // Make sure all common encrypt/decrypt methods have been sent
    //
    if (!( (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY)))               &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY_SRC_ADDR_LO)))   &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY_SRC_ADDR_HI)))   &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY_DST_ADDR_LO)))   &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY_DST_ADDR_HI)))   &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(COPY_SIZE_BYTES)))    &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(DIGEST_ADDR_LO)))     &&
           (APM_IS_METHOD_VALID(APM_METHOD_ID(DIGEST_ADDR_HI))) ))
    {
        errorCode = LW95A1_ERROR_APM_ILWALID_METHOD;
        goto ErrorExit;
    }

    //
    // Get APM copy data from methods.
    //
    apmCopyData.apmCopyType        = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY);
    apmCopyData.apmCopySrcLo       = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY_SRC_ADDR_LO);
    apmCopyData.apmCopySrcHi       = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY_SRC_ADDR_HI);
    apmCopyData.apmCopyDstLo       = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY_DST_ADDR_LO);
    apmCopyData.apmCopyDstHi       = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY_DST_ADDR_HI);
    apmCopyData.apmCopySizeBytes   = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(COPY_SIZE_BYTES);
    apmCopyData.apmDigestAddrLo    = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(DIGEST_ADDR_LO);
    apmCopyData.apmDigestAddrHi    = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(DIGEST_ADDR_HI);

    // 
    // Make sure source and destination addresses are properly aligned.
    //
    if (((apmCopyData.apmCopySrcLo &
          DRF_SHIFTMASK(LW95A1_APM_COPY_SRC_ADDR_LO_DATA)) != apmCopyData.apmCopySrcLo) ||
         ((apmCopyData.apmCopySrcHi &
           DRF_SHIFTMASK(LW95A1_APM_COPY_SRC_ADDR_HI_DATA)) != apmCopyData.apmCopySrcHi))
    {
        errorCode = LW95A1_ERROR_APM_SRC_ADDR_MISALIGNED_POINTER;
        goto ErrorExit;
    }
    if (((apmCopyData.apmCopyDstLo &
          DRF_SHIFTMASK(LW95A1_APM_COPY_DST_ADDR_LO_DATA)) != apmCopyData.apmCopyDstLo) ||
        ((apmCopyData.apmCopyDstHi &
          DRF_SHIFTMASK(LW95A1_APM_COPY_DST_ADDR_HI_DATA)) != apmCopyData.apmCopyDstHi))
    {
        errorCode = LW95A1_ERROR_APM_DEST_ADDR_MISALIGNED_POINTER;
        goto ErrorExit;
    }
    if (((apmCopyData.apmDigestAddrLo &
          DRF_SHIFTMASK(LW95A1_APM_DIGEST_ADDR_LO_DATA)) != apmCopyData.apmDigestAddrLo) ||
        ((apmCopyData.apmDigestAddrHi &
          DRF_SHIFTMASK(LW95A1_APM_DIGEST_ADDR_HI_DATA)) != apmCopyData.apmDigestAddrHi))
    {
        errorCode = LW95A1_ERROR_APM_DIGEST_ADDR_MISALIGNED_POINTER;
        goto ErrorExit;
    }

    //
    // Make sure copy bytes size is a legal value
    //
    if ((apmCopyData.apmCopySizeBytes &
        DRF_SHIFTMASK(LW95A1_APM_COPY_SIZE_BYTES_DATA)) != apmCopyData.apmCopySizeBytes)
    {
        errorCode = LW95A1_ERROR_APM_COPY_MISALIGNED_SIZE;
        goto ErrorExit;
    }

    //
    // Perform encryption/decryption.
    //
    if (apmCopyData.apmCopyType == LW95A1_APM_COPY_TYPE_ENCRYPT)
    {
        apmCopyData.bApmEncryptIVSent = LW_FALSE;

        //
        // Check to see if ENCRYPT_IV methods have been sent.
        //
        if ((APM_IS_METHOD_VALID(APM_METHOD_ID(ENCRYPT_IV_ADDR_LO))) &&
            (APM_IS_METHOD_VALID(APM_METHOD_ID(ENCRYPT_IV_ADDR_HI))))
        {
            apmCopyData.bApmEncryptIVSent = LW_TRUE;

            //
            // Get APM encrypt IV data from methods.
            //
            apmCopyData.apmEncryptIVAddrLo = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(ENCRYPT_IV_ADDR_LO);
            apmCopyData.apmEncryptIVAddrHi = APM_GET_METHOD_PARAM_OFFSET_FROM_ID(ENCRYPT_IV_ADDR_HI);

            //
            // Check address alignment of Encrypt IV
            //
            if (((apmCopyData.apmEncryptIVAddrLo &
                  DRF_SHIFTMASK(LW95A1_APM_ENCRYPT_IV_ADDR_LO_DATA)) != apmCopyData.apmEncryptIVAddrLo) ||
                ((apmCopyData.apmEncryptIVAddrHi &
                  DRF_SHIFTMASK(LW95A1_APM_ENCRYPT_IV_ADDR_HI_DATA)) != apmCopyData.apmEncryptIVAddrHi))
            {
                errorCode = LW95A1_ERROR_APM_IV_ADDR_MISALIGNED_POINTER;
                goto ErrorExit;
            }
        }

        //
        // Do encryption
        //
        errorCode = apmEncryptDma(&apmCopyData);
    }
    else
    {
        //
        // Do decryption
        //
        errorCode = apmDecryptDma(&apmCopyData);
    }

ErrorExit:
    return errorCode;
}
