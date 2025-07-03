/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_decrypt_ls_ucode_gh100.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "dev_fb.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_acr_private.h"
#include "scp_secret_usage.h"
#include <liblwriscv/scp.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>


/* ------------------------ Macros & Defines ------------------------------- */

// Key used when the binaries are prod encrypted
// TODO: uncomment this cnad remove hardcoded value of key once
// changes of scp_secret_usage is moved to chips_a
// #define ACR_LS_ENCRYPT_KEY_INDEX_PROD       SCP_SECRET_KEY_INDEX_RM_LS_ENCRYPTION
#define ACR_LS_ENCRYPT_KEY_INDEX_PROD       (31)

/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
// Buffer used for LS Ucode Decryption
LwU8  g_tmpGenericBuffer[ACR_GENERIC_BUF_SIZE_IN_BYTES]   ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES)   ATTR_OVLY(".data");


/* ------------------------ External Definitions --------------------------- */
extern ACR_DMA_PROP  g_dmaProp;
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8 g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];

/* ------------------------ Static Variables ------------------------------- */

// Temporary buffer used to store kdf Salt
static LwU8  g_tmpSaltBuffer[ACR_AES128_SIG_SIZE_IN_BYTES]      ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data");

// IV Used for LS Ucode Decryption
static LwU8  g_lsEncryptIV[ACR_AES128_SIG_SIZE_IN_BYTES]        ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data");

static LwU8 g_lsEncKdfSalt[ACR_AES128_SIG_SIZE_IN_BYTES]        ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data") =
{
    0xB6,0xC2,0x31,0xE9,0x03,0xB2,0x77,0xD7,0x0E,0x32,0xA0,0x69,0x8F,0x4E,0x80,0x62
};

#ifdef ACR_CMAC_KDF_ENABLED

// To store Full Label for CMAC KDF
static   CMAC_KDF_FULL_LABEL g_kdfFullLabel                     ATTR_ALIGNED(ACR_AES128_KEY_SIZE_IN_BYTES)     ATTR_OVLY(".data");

// Vector used while deriving IV using CMAC KDF
LwU8     tempVector[ACR_AES128_KEY_SIZE_IN_BYTES]               ATTR_ALIGNED(ACR_AES128_KEY_SIZE_IN_BYTES)     ATTR_OVLY(".data");

#endif // ACR_CMAC_KDF_ENABLED

/*!
 * @brief: acrDecryptAndWriteToWpr_GH100
 *         Decrypt the output after signature verification and write back to WPR
 *
 * @return ACR_ERROR_UNREACHABLE_CODE If no LS Falcon present
 * @return ACR_OK                     On success
 */
ACR_STATUS
acrDecryptAndWriteToWpr_GH100(LwU32 *failingEngine)
{
    ACR_STATUS              status              = ACR_OK;
    LwU32                   index               = 0;
    LwU32                   ucodeOffset         = 0;
    LwU32                   falconId            = 0;
    LwU32                   lsCodeSize          = 0;
    LwU32                   lsDataSize          = 0;
    LwU32                   lsbOffset           = 0;
    LwU32                   bUcodeLsEncrypted   = 0;
    LwU32                   encryptionAlgoVer   = 0;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper   = NULL;
    LSF_UCODE_DESC_WRAPPER  lsfDescWrapper;
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper   = (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper;

    //
    // Loop through each WPR header.
    // Break the loop when we see an invalid falcon ID
    //
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWprHeaderWrapper, (void *)&lsbOffset));

        // Check if it is an invalid falconID
        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            if (NULL == pLsbHeaderWrapper)
            {
                // ACR should get enabled if we have atleast one LSFM falcon
                return ACR_ERROR_UNREACHABLE_CODE;
            }

            break;
        }

        if (!acrIsFalconSupported(falconId))
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, pLsbHeaderWrapper));

        // Get the LS Ucode Desc
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                        pLsbHeaderWrapper, (void *)&lsfDescWrapper));

        // Check if LS Ucode is encrypted
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ENCRYPTED_FLAG,
                                                                        &lsfDescWrapper, (void *)&bUcodeLsEncrypted));

        // Get the Ucode Offset
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                         pLsbHeaderWrapper, (void *)&ucodeOffset));

        // Get the Code Size
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                         pLsbHeaderWrapper, (void *)&lsCodeSize));

#ifdef ACR_CMAC_KDF_ENABLED
        // Get Encryption Algo version
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_LS_ENCRYPTION_ALGO_VER,
                                                                        &lsfDescWrapper, (void *)&encryptionAlgoVer));
#endif // ACR_CMAC_KDF_ENABLED

        if (bUcodeLsEncrypted == 0)
        {
#ifdef ACR_FMODEL_BUILD
            // On fmodel, we allow skipping LS decryption due to latency concerns

            // Setup SubWprs for code and data part of falcon ucodes in FB-WPR
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprsExt_HAL(pAcr, falconId, pLsbHeaderWrapper));
            continue;
#else  // ACR_FMODEL_BUILD
            *failingEngine = falconId;
            return ACR_ERROR_LS_UCODE_IS_NOT_ENCRYPTED;
#endif // ACR_FMODEL_BUILD
        }

        // Get the Data Size
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&lsDataSize));

        // Get the IV
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_AES_CBC_IV,
                                                                        &lsfDescWrapper, (void *)&g_lsEncryptIV));

        //
        // We'll be decrypting the the entire LS Ucode except for the LS_SIG_GROUP
        // block present in it. Therefore, the code size will be exluding the
        // LS_SIG_GROUP block size from it.
        //
        if (lsCodeSize > 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDmaAndDecrypt_HAL(pAcr, ucodeOffset,
                                                            (lsCodeSize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE), falconId, LW_TRUE, encryptionAlgoVer, g_lsEncryptIV));
        }

        // Get the IV again
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_AES_CBC_IV,
                                                                        &lsfDescWrapper, (void *)&g_lsEncryptIV));

        // Decryption of the data part of the Ucode.
        if (lsDataSize > 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDmaAndDecrypt_HAL(pAcr,
                                                                    (ucodeOffset + lsCodeSize), lsDataSize, falconId, LW_FALSE, encryptionAlgoVer, g_lsEncryptIV));
        }

#ifdef IS_LASSAHS_REQUIRED 
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGenerateLassahsSignature_HAL(pAcr, pLsbHeaderWrapper, falconId, lsCodeSize, ucodeOffset, LW_TRUE));
#endif // IS_LASSAHS_REQUIRED 

        // Setup SubWprs for code and data part of falcon ucodes in FB-WPR
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprsExt_HAL(pAcr, falconId, pLsbHeaderWrapper));
    }

    return status;
}

/*!
 * @brief: acrIssueDmaAndDecrypt_GH100
 *         Issue DMA to read and write from WPR (in loop) and ilwoke decryption
 *
 * @param[in]  binaryOffset      Offset of the code/data part of the Ucode.
 * @param[in]  binarySize        Size of the code/data part of the Ucode.
 *
 * @return ACR_OK                             On success
 *         ACR_ERROR_DMA_FAILURE              if DMA Operation fails
 *         ACR_ERROR_UNKNOWN                  if Decryption fails
 *         ACR_ERROR_DECRYPTED_SIZE_MISMATCH  if Decrypted size mismatches with origial binary size
 */
ACR_STATUS
acrIssueDmaAndDecrypt_GH100
(
    LwU32  binaryOffset,
    LwU32  binarySize,
    LwU32  falconID,
    LwBool bIsCode,
    LwU32  encryptionAlgoVer,
    LwU8   *iVector
)
{
    ACR_STATUS status               = ACR_OK;
    LwU32      lwrDecryptedByteSize = 0;
    LwU32      bytesXfered          = 0;

    // Initialize the Buffers
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);

#ifdef ACR_CMAC_KDF_ENABLED
    // Get Derived IV using CMAC Key Derivation function
    if (encryptionAlgoVer == LSF_KDF_ALGO_CMAC)
    {
         CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrCmacKdfWrapper_HAL(pAcr, falconID, bIsCode, LW_FALSE, iVector));
    }
#endif // ACR_CMAC_KDF_ENABLED

    // Start the decryption loop
    while (lwrDecryptedByteSize < binarySize)
    {
        // Set the number of bytes to be decrypted, for the current iteration
        if ((binarySize - lwrDecryptedByteSize) >= ACR_GENERIC_BUF_SIZE_IN_BYTES)
        {
            bytesXfered = ACR_GENERIC_BUF_SIZE_IN_BYTES;
        }
        else
        {
            bytesXfered = binarySize - lwrDecryptedByteSize;
        }

        //
        // DMA write to DMEM from WPR Region. This function will write the contents of WPR
        // to the buffer. The buffer used is of size 4k.
        //
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrIssueDma_HAL(pAcr, (void *)g_tmpGenericBuffer , LW_FALSE, binaryOffset + lwrDecryptedByteSize,
                                         bytesXfered, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

        // Decrypt the contents of the buffer using AES CBC
        if ((status = acrDecryptAesCbcBuffer_HAL(pAcr, g_tmpGenericBuffer, binaryOffset + lwrDecryptedByteSize, bytesXfered, falconID, bIsCode, encryptionAlgoVer, iVector)) != ACR_OK)
        {
            goto Cleanup;
        }


        // Increment the current decrypted size to access the next 4k block in WPR
        lwrDecryptedByteSize += bytesXfered;
    }

    // If the decrypted byte size mismatches with the original binary size, it has failed
    if (lwrDecryptedByteSize != binarySize)
    {
        status = ACR_ERROR_DECRYPTED_SIZE_MISMATCH;
    }

Cleanup:
    // Scrub the contents of the Buffers
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);
    acrMemset_HAL(pAcr, g_tmpSaltBuffer, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);

    return status;
}

/*!
 * @brief: acrDecryptAesCbcBuffer_GH100
 *         Decrypt the contents of the buffer
 *
 * @param[in]  pBuffer      Address of the 4k Byte buffer used for decryption
 * @param[out] fbOffset     Address of FB buffer used for decryption
 * @param[in]  bytesXfered  Number of bytes to be decrypted
 * @param[in]  falconID     ID of the current falcon
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Buffer pointer is NULL
 *         ACR_ERROR_AES_CBC_DECRYPTION_FAILED If Decryption fails
 */
ACR_STATUS
acrDecryptAesCbcBuffer_GH100
(
    LwU8  *pBuffer,
    LwU32  fbOffset,
    LwU32  bytesXfered,
    LwU32  falconID,
    LwBool bIsCode,
    LwU32  encryptionAlgoVer,
    LwU8  *iVector
)
{
    ACR_STATUS   status             = ACR_OK;
    const SCP_KEY_DESC *derivedKey  = NULL;
    LwU8  tempIV[ACR_AES128_SIG_SIZE_IN_BYTES];

    // Clear the Temp salt Buffers and iv
    acrMemset_HAL(pAcr, g_tmpSaltBuffer, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);
    acrMemset_HAL(pAcr, tempIV, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);

    // save last block of cipher text to send as iv feedback for next block
    acrMemcpy_HAL(pAcr, (void *)tempIV, (void *) &(pBuffer[bytesXfered - 16]), ACR_AES128_SIG_SIZE_IN_BYTES);

#ifdef ACR_CMAC_KDF_ENABLED
    if (encryptionAlgoVer == LSF_KDF_ALGO_CMAC)
    {
        //
        // scpDecrypt API clears all SCP Registers after performing decryption functionality. 
        // Due to this restriction, derived key needs to be generated and passed to API for every
        // block
        // TODO @chetang: To optimize generated sub-key & derived key in SCP such that it can be reused for code/data 
        //
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrCmacKdfWrapper_HAL(pAcr, falconID, bIsCode, LW_TRUE, iVector));

        derivedKey  = SCP_KEY_RAW_REGISTER(SCP_R5);
    }
    else if (encryptionAlgoVer == LSF_KDF_ALGO_AES)
    {
#endif // ACR_CMAC_KDF_ENABLED

        // copy salt to  tmpSaltBuffer for deriving Kdf 
        acrMemcpy_HAL(pAcr, (void *)g_tmpSaltBuffer, (void *)g_lsEncKdfSalt, ACR_AES128_SIG_SIZE_IN_BYTES);

        // XOR the falconID with the kdfSalt
        ((LwU32*)g_tmpSaltBuffer)[0] ^= (falconID);

        // Derive the key from scp secret
        if (acrIsDebugModeEnabled_HAL(pAcr))
        {
            derivedKey  = SCP_KEY_DERIVED_ECB(ACR_LS_ENCRYPT_KEY_INDEX_DEBUG, g_tmpSaltBuffer);
        }
        else
        {
            derivedKey  = SCP_KEY_DERIVED_ECB(ACR_LS_ENCRYPT_KEY_INDEX_PROD, g_tmpSaltBuffer);
        }

#ifdef ACR_CMAC_KDF_ENABLED
    }
    else
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto Cleanup;
    }
#endif // ACR_CMAC_KDF_ENABLED

    // Setup the dmaindex for scpdma to directly transfer data to fb
    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(scpConfigureShortlwt((LwU8)(g_dmaProp.ctxDma)), ACR_ERROR_SCP_LIB_SCPDMA_FAILED);

    // Adding check for Coverity/CERT-C fix
    CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS((LwU64)LW_RISCV_AMAP_FBGPA_START, ((g_dmaProp.wprBase) << 8), LW_U64_MAX);

    CHECK_LIBLWRISCV_STATUS_OK_OR_GOTO_CLEANUP(scpDecrypt(derivedKey, SCP_CIPHER_MODE_CBC, (uintptr_t)pBuffer,
        (uintptr_t)((LwU64)LW_RISCV_AMAP_FBGPA_START + ((g_dmaProp.wprBase) << 8) + fbOffset), bytesXfered, (uintptr_t)iVector), ACR_ERROR_AES_CBC_DECRYPTION_FAILED);

    // Copy TempIv to iv to send as feedback for next CBC block
    acrMemcpy_HAL(pAcr, (void *)iVector, (void *)tempIV, ACR_AES128_SIG_SIZE_IN_BYTES);

    return ACR_OK;

Cleanup:

    // Clears all data from the SCP unit
    scpShutdown();

    acrMemset_HAL(pAcr, g_tmpSaltBuffer, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);
    acrMemset_HAL(pAcr, tempIV, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);

    return status;
}

#ifdef ACR_CMAC_KDF_ENABLED
/*!
 * @brief: acrCmacKdfWrapper_GH100
 *         ACR Top level Wrapper for CMAC-KDF.
 *
 * @param[in]     falconID         falcon ID
 * @param[in]     bIsCode          Boolen flag to determine Code or Data 
 * @param[in]     bGetDerivedKey   Boolen flag to find, whether to get derived Key/IV
 * @param[in/out] pbaseIV          Pointer to IV Vector.
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Buffer pointer is NULL
 */
ACR_STATUS
acrCmacKdfWrapper_GH100
(
   LwU32        falconId,
   LwBool       bIsCode,
   LwBool       bGetDerivedKey,
   LwU8         *pbaseIV
)
{
    ACR_STATUS status = ACR_OK;

    // Loads hardware secret in SCP_R7 register
    if (acrIsDebugModeEnabled_HAL(pAcr))
    {
        scp_secret(ACR_LS_ENCRYPT_KEY_INDEX_DEBUG, SCP_R7);
    }
    else
    { 
        scp_secret(ACR_LS_ENCRYPT_KEY_INDEX_PROD, SCP_R7);
    }

    if (bGetDerivedKey)
    {
       if (bIsCode)
       {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetFullLabel_HAL(pAcr, falconId, ACR_CMAC_KDF_CODE, &g_kdfFullLabel));
       }
       else
       {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetFullLabel_HAL(pAcr, falconId, ACR_CMAC_KDF_DATA, &g_kdfFullLabel));
       }

       // Get Derived Key
       CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetDerivedKey_HAL(pAcr, &g_kdfFullLabel));
    }
    else
    {
        acrMemset_HAL(pAcr, tempVector, 0x0, ACR_AES128_KEY_SIZE_IN_BYTES);

        // tempVector is to be specified by user to API, used for XOR operation with IV         // It is 0x0 for Code, and 0x1 for data.
        if (!bIsCode)
        {   
            tempVector[ACR_AES128_KEY_SIZE_IN_BYTES -1] = 1;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetFullLabel_HAL(pAcr, falconId, ACR_CMAC_KDF_IV, &g_kdfFullLabel));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetDerivedIv_HAL(pAcr, pbaseIV, tempVector, &g_kdfFullLabel));
    }

    return status;
}

/*!
 * @brief: acrCmacKdfGetDerivedIV_GH100
 *         API for Getting Derived IV using CMAC Key derivation function.
 *
 * @param[in]     pFullLabel    Pointer to Full Label message. 
 * @param[in]     ptempVector   Pointer to temporary vector provided by user to XOR with IV
 * @param[in/out] pbaseIV       Pointer to IV Vector.
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Buffer pointer is NULL
 */
ACR_STATUS
acrCmacKdfGetDerivedIv_GH100
(
   LwU8                *pbaseIV,
   LwU8                *ptempVector,
   CMAC_KDF_FULL_LABEL *pFullLabel
)
{
    ACR_STATUS status = ACR_OK;

    if (pbaseIV == NULL || ptempVector == NULL || pFullLabel == NULL)
    {
       return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Get derived key in SCP_R5
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCmacKdfGetDerivedKey_HAL(pAcr, pFullLabel));

    // Use K3 as key for Encryption to get derived IV.
    scp_key(SCP_R5);

    scp_load((uintptr_t)tempVector, SCP_R1);
    scp_wait_dma();

    scp_load((uintptr_t)pbaseIV, SCP_R3);
    scp_wait_dma();

    // XOR Base IV with temp vector
    scp_xor(SCP_R1, SCP_R3);

    // Get derived IV(IV1/ IV2) 
    scp_encrypt(SCP_R3, SCP_R1);

    scp_store(SCP_R1, (uintptr_t)pbaseIV);
    scp_wait_dma();

    return status;
}

/*!
 * @brief: acrCmacGetFullLabel_GH100
 *         Generate the KDF Full Label to be passed as message to PRF function 
 *
 * @param[in]  falconID    ID of the current falcon
 * @param[in]  kdfType     KDF type to be derived based on Code, Data and IV
 * @param[out] pFullLabel  Pointer to Full Label to be constructed.
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Wrong KDF type is recieved.
 */
ACR_STATUS
acrCmacKdfGetFullLabel_GH100
(
    LwU32               falconID,
    ACR_CMAC_KDF_TYPE   kdfType,
    CMAC_KDF_FULL_LABEL *pFullLabel
)
{
    ACR_STATUS  status  = ACR_OK;

    if (pFullLabel == NULL)
    {
         return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Clear Buffer
    acrMemset_HAL(pAcr, (void *)pFullLabel, 0, sizeof(CMAC_KDF_FULL_LABEL));

    // Construct KDF Message based on Full label type(CODE/DATA/IV)
    pFullLabel->length  = (LwU32)ACR_CMAC_KDF_FULL_LABEL_SIZE;
    pFullLabel->context = falconID;

   switch (kdfType)
   {
       case ACR_CMAC_KDF_CODE:
         pFullLabel->counter    =  1;
         pFullLabel->label[1]   = 'k';
         pFullLabel->label[2]   = 'e';
         pFullLabel->label[3]   = 'y';
         break;

       case ACR_CMAC_KDF_DATA:
         pFullLabel->counter    =  2;
         pFullLabel->label[1]   = 'k';
         pFullLabel->label[2]   = 'e';
         pFullLabel->label[3]   = 'y';
         break;

       case ACR_CMAC_KDF_IV:
         pFullLabel->counter    =  3;
         pFullLabel->label[2]   = 'i';
         pFullLabel->label[3]   = 'v';
         break;

       default:
         return ACR_ERROR_ILWALID_ARGUMENT;
         break; 
   }

   return status;
}

/*!
 * @brief: acrCmacKdfGetDerivedKey_GH100
 *         API for Getting Derived Key using CMAC Key derivation function.
 *         Derived Key generated will be stored in SCP_R5 register.
 *
 * Note:   API requires secret key to be pre loaded in SCP_R7 register.  
 *
 * @param[in] pFullLabel      Pointer to Full Label. Full Label size needs to be 16 Byte.
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Buffer pointer is NULL
 */
ACR_STATUS
acrCmacKdfGetDerivedKey_GH100
(
   void         *pFullLabel
)
{
   ACR_STATUS  status    = ACR_OK;

   if (pFullLabel == NULL)
   {
       return ACR_ERROR_ILWALID_ARGUMENT;
   }

   // Use secret Key Index for Encryption/Decryption
   scp_key(SCP_R7);

   scp_load((uintptr_t)pFullLabel, SCP_R6);
   scp_wait_dma();

   // Clear register
   scp_xor(SCP_R0, SCP_R0);
   
   // Encrypt Zero vector.
   scp_encrypt(SCP_R0, SCP_R1);

   // Get Subkey
   scp_cmac_sk(SCP_R1, SCP_R2);
 
   // XOR SubKey with FULL Label
   scp_xor(SCP_R2, SCP_R6);

   // Get derived Key in SCP_R5
   scp_encrypt(SCP_R6, SCP_R5);

   return status;
}
#endif // ACR_CMAC_KDF_ENABLED

