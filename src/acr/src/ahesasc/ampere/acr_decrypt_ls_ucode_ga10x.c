/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_decrypt_ls_ucode_ga10x.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fb.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"
#include "scp_secret_usage.h"

//
// The buffer being used for DMA operations is of 4096 bytes. This size was chosen
// after checking LW_PSEC_FALCON_HWCFG_DMAQUEUE_DEPTH bits of its respective register.
// These bits indicate the number of Kbytes that the DMAQUEUE can hold at once,
// and have been proved to provide an ideal size to be used in order to avoid pipeline
// stalling during DMA operations.
//
#define ACR_DECRYPT_BUF_SIZE_IN_BYTES       (0x1000)


// Key used when the binaries are prod encrypted
// TODO: uncomment this cnad remove hardcoded value of key once
// changes of scp_secret_usage is moved to chips_a
// #define ACR_LS_ENCRYPT_KEY_INDEX_PROD       SCP_SECRET_KEY_INDEX_RM_LS_ENCRYPTION
#define ACR_LS_ENCRYPT_KEY_INDEX_PROD       (31)

// Global and Extern Variables
extern LwBool        g_bIsDebug;
extern ACR_DMA_PROP  g_dmaProp;
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];


// Buffer used for LS Ucode Decryption
LwU8  g_lsDecryptionBuf[ACR_DECRYPT_BUF_SIZE_IN_BYTES]   ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES)   ATTR_OVLY(".data");

// Temporary buffer used to store kdf Salt
LwU8  g_tmpSaltBuffer[ACR_AES128_SIG_SIZE_IN_BYTES]      ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data");

// IV Used for LS Ucode Decryption
LwU8  g_lsEncryptIV[ACR_AES128_SIG_SIZE_IN_BYTES]          ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data");

LwU8 g_lsEncKdfSalt[ACR_AES128_SIG_SIZE_IN_BYTES]        ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)     ATTR_OVLY(".data") =
{
    0xB6,0xC2,0x31,0xE9,0x03,0xB2,0x77,0xD7,0x0E,0x32,0xA0,0x69,0x8F,0x4E,0x80,0x62
};


#if defined(AHESASC)
/*!
 * @brief: acrDecryptAndWriteToWpr_GA10X
 *         Decrypt the output after signature verification and write back to WPR
 *
 * @return ACR_ERROR_UNREACHABLE_CODE If no LS Falcon present
 * @return ACR_OK                     On success
 */
ACR_STATUS
acrDecryptAndWriteToWpr_GA10X(void)
{
    ACR_STATUS              status              = ACR_OK;
    LwU32                   index               = 0;
    LwU32                   ucodeOffset         = 0;
    LwU32                   falconId            = 0;
    LwU32                   lsCodeSize          = 0;
    LwU32                   lsDataSize          = 0;
    LwU32                   lsbOffset           = 0;
    LwU32                   bUcodeLsEncrypted   = 0;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper   = NULL;
    LSF_UCODE_DESC_WRAPPER  lsfDescWrapper;
    LSF_LSB_HEADER_WRAPPER  lsbHeaderWrapper;
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper   = &lsbHeaderWrapper;

    //
    // Loop through each WPR header.
    // Break the loop when we see an invalid falcon ID
    //
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
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

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, (void *)&lsbHeaderWrapper));

        // Get the LS Ucode Desc
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                        pLsbHeaderWrapper, (void *)&lsfDescWrapper));

        // Check if LS Ucode is encrypted
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ENCRYPTED_FLAG,
                                                                        &lsfDescWrapper, (void *)&bUcodeLsEncrypted));

        // Get the Ucode Offset
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                         pLsbHeaderWrapper, (void *)&ucodeOffset));

        // Get the Code Size
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                         pLsbHeaderWrapper, (void *)&lsCodeSize));

        if (bUcodeLsEncrypted == 0)
        {
#ifndef ACR_ENFORCE_LS_UCODE_ENCRYPTION
            //
            // On fmodel, we allow skipping LS decryption due to latency concerns
            // Setup SubWprs for code and data part of falcon ucodes in FB-WPR
            //
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprsExt_HAL(pAcr, falconId, pLsbHeaderWrapper));
            continue;
#else  // ACR_ENFORCE_LS_UCODE_ENCRYPTION
            acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
            return ACR_ERROR_LS_UCODE_IS_NOT_ENCRYPTED;
#endif // ACR_ENFORCE_LS_UCODE_ENCRYPTION
        }

        // Get the Data Size
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&lsDataSize));

        // Get the IV
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_AES_CBC_IV,
                                                                        &lsfDescWrapper, (void *)&g_lsEncryptIV));

        //
        // We'll be decrypting the the entire LS Ucode except for the LS_SIG_GROUP
        // block present in it. Therefore, the code size will be exluding the
        // LS_SIG_GROUP block size from it.
        //
        if (lsCodeSize > 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDmaAndDecrypt_HAL(pAcr, ucodeOffset,
                                                            (lsCodeSize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE), falconId, g_lsEncryptIV));
        }

        // Get the IV again
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_AES_CBC_IV,
                                                                        &lsfDescWrapper, (void *)&g_lsEncryptIV));

        // Decryption of the data part of the Ucode.
        if (lsDataSize > 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDmaAndDecrypt_HAL(pAcr,
                                                                    (ucodeOffset + lsCodeSize), lsDataSize, falconId, g_lsEncryptIV));
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGenerateLassahsSignature_HAL(pAcr, pLsbHeaderWrapper, falconId, lsCodeSize, ucodeOffset, LW_TRUE));

        // Setup SubWprs for code and data part of falcon ucodes in FB-WPR
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprsExt_HAL(pAcr, falconId, pLsbHeaderWrapper))
    }

    return status;
}

/*!
 * @brief: acrIssueDmaAndDecrypt_GA10X
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
acrIssueDmaAndDecrypt_GA10X
(
    LwU32  binaryOffset,
    LwU32  binarySize,
    LwU32  falconID,
    LwU8  *iVector
)
{
    ACR_STATUS status               = ACR_OK;
    LwU32      lwrDecryptedByteSize = 0;
    LwU32      bytesXfered          = 0;

    // Initialize the Buffers
    acrlibMemset_HAL(pAcrlib, g_lsDecryptionBuf, 0x0, ACR_DECRYPT_BUF_SIZE_IN_BYTES);
    acrlibMemset_HAL(pAcrlib, g_tmpSaltBuffer, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);

    // Start the decryption loop
    while (lwrDecryptedByteSize < binarySize)
    {
        // Set the number of bytes to be decrypted, for the current iteration
        if ((binarySize - lwrDecryptedByteSize) >= ACR_DECRYPT_BUF_SIZE_IN_BYTES)
        {
            bytesXfered = ACR_DECRYPT_BUF_SIZE_IN_BYTES;
        }
        else
        {
            bytesXfered = binarySize - lwrDecryptedByteSize;
        }

        //
        // DMA write to DMEM from WPR Region. This function will write the contents of WPR
        // to the buffer. The buffer used is of size 4k.
        //
        if (acrIssueDma_HAL(pAcr, (void *)g_lsDecryptionBuf , LW_FALSE, binaryOffset + lwrDecryptedByteSize,
                            bytesXfered, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp) != bytesXfered)
        {
            status = ACR_ERROR_DMA_FAILURE;
            goto Cleanup;
        }

        // Decrypt the contents of the buffer using AES CBC
        if ((status = acrDecryptAesCbcBuffer_HAL(pAcr, g_lsDecryptionBuf, bytesXfered, falconID, iVector)) != ACR_OK)
        {
            goto Cleanup;
        }

        //
        // DMA write to WPR from DMEM Region. This function will writeback the decrypted
        // contents of the buffer into the same location in the WPR from which they were fetched
        //
        if (acrIssueDma_HAL(pAcr, (void *)g_lsDecryptionBuf , LW_FALSE, binaryOffset + lwrDecryptedByteSize,
                            bytesXfered, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp) != bytesXfered)
        {
            status = ACR_ERROR_DMA_FAILURE;
            goto Cleanup;
        }

        // Increment the current decrypted size to access the next 4k block in WPR
        lwrDecryptedByteSize += bytesXfered;
    }

    // If the decrypted byte size mismatches with the original binary size, it has failed
    if (lwrDecryptedByteSize != binarySize)
    {
        return ACR_ERROR_DECRYPTED_SIZE_MISMATCH;
    }

Cleanup:
    // Scrub the contents of the Buffers
    acrlibMemset_HAL(pAcrlib, g_lsDecryptionBuf, 0x0, ACR_DECRYPT_BUF_SIZE_IN_BYTES);
    acrlibMemset_HAL(pAcrlib, g_tmpSaltBuffer, 0x0, ACR_AES128_SIG_SIZE_IN_BYTES);

    return status;
}

/*!
 * @brief: acrGetDerivedKeyAndLoadScpTrace0_GA10X
 *        Get the derived key for decryption and load instructions in scp sequencer
 *
 * @param[in]  falconID     ID of the current falcon
 *
 * @return ACR_OK on success
 *         ACR_ERROR_UNEXPECTED_ALIGNMENT if alignment of Salt buffer is wrong
 */
ACR_STATUS
acrGetDerivedKeyAndLoadScpTrace0_GA10X
(
    LwU32  falconID,
    LwU8   *iVector
)
{
    ACR_STATUS status = ACR_OK;

    // Check for alignment
    if (((LwU32)g_tmpSaltBuffer) % ACR_AES128_SIG_SIZE_IN_BYTES)
    {
        return ACR_ERROR_UNEXPECTED_ALIGNMENT;
    }

    //
    // Derive key formula :
    // Derived key = AES-ECB((g_lsEncKdfSalt XOR falconID), key)
    //

    // Use tmpSaltBuffer as temporary buffer to store Kdf Salt
    acrlibMemcpy_HAL(pAcrlib, g_tmpSaltBuffer, g_lsEncKdfSalt, ACR_AES128_SIG_SIZE_IN_BYTES);

    // XOR the falconID with the kdfSalt
    ((LwU32*)g_tmpSaltBuffer)[0] ^= (falconID);

    // Trap the DMA instructions indefinitely
    falc_scp_trap(TC_INFINITY);

    // Load XORed salt
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(g_tmpSaltBuffer), SCP_R1));
    falc_dmwait();

    // Select whether to use Debug or Prod key for decryption
    if (g_bIsDebug)
    {
        falc_scp_secret(ACR_LS_ENCRYPT_KEY_INDEX_DEBUG, SCP_R2);
    }
    else
    {
        falc_scp_secret(ACR_LS_ENCRYPT_KEY_INDEX_PROD, SCP_R2);
    }

    falc_scp_key(SCP_R2);

    // Derived Key is now in SCP_R3
    falc_scp_encrypt(SCP_R1, SCP_R3);

    //
    // the AES key in R4 is colwerted to the 10th round AES round key that is the
    // form of the key required for the decrypt instruction to work correctly.
    // Now the Decryption key is stored in SCP_R4
    //
    falc_scp_rkey10(SCP_R3, SCP_R4);

    //
    // Now our decryption key is in SCP_R4. We now load SCP_R4
    // index in the default Ku register. This is done so that we notify that
    // the decryption key is stored in SCP_R4.
    //
    falc_scp_key(SCP_R4);

    falc_trapped_dmwrite(falc_sethi_i((LwU32)(&(iVector[0])), SCP_R1));
    falc_dmwait();
    falc_scp_trap(0);

    // Load the instructions in the sequencer that'll be used for decryption.
    falc_scp_load_trace0(5);

    falc_scp_push(SCP_R5);
    falc_scp_decrypt(SCP_R5, SCP_R6);
    falc_scp_xor(SCP_R1, SCP_R6);
    falc_scp_mov(SCP_R5, SCP_R1);
    falc_scp_fetch(SCP_R6);

    falc_scp_trap(0);

    return status;
}


/*!
 * @brief: acrDecryptAesCbcBuffer_GA10X
 *         Decrypt the contents of the buffer
 *
 * @param[out] pBuffer      Address of the 4k Byte buffer used for decryption
 * @param[in]  bytesXfered  Number of bytes to be decrypted
 * @param[in]  falconID     ID of the current falcon
 *
 * @return ACR_OK                              On success
 *         ACR_ERROR_ILWALID_ARGUMENT          If Buffer pointer is NULL
 *         ACR_ERROR_AES_CBC_DECRYPTION_FAILED If Decryption fails
 */
ACR_STATUS
acrDecryptAesCbcBuffer_GA10X
(
    LwU8  *pBuffer,
    LwU32  bytesXfered,
    LwU32  falconID,
    LwU8  *iVector
)
{
    LwU32      dmwaddr       = 0;
    LwU32      numIterations = 0;
    LwU32      transferSize  = 0;
    LwU32      index         = 0;
    ACR_STATUS status        = ACR_ERROR_UNKNOWN;

    if ((pBuffer == NULL) || (iVector == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Get the derived key and load the scp sequencer with decryption instructions
    if ((status = acrGetDerivedKeyAndLoadScpTrace0_HAL(pAcr, falconID, iVector)) != ACR_OK)
    {
        return status;
    }

    while (bytesXfered > 0)
    {
        //
        // Use the SCP sequencer to do the encryption or decryption.
        // Find the number of iterations we need: each iteration consumes 16B,
        // and the number of iterations needs to be a power of two, up to 16,
        // so we can consume up to 256B per sequencer iteration.
        // Do this until the entire input is consumed.
        //

        numIterations = bytesXfered / 16;
        if (numIterations > 16)
        {
            numIterations = 16;
        }

        // Check if numIterations is power of 2. Else assume 16B
        if (numIterations & (numIterations - 1))
        {
            numIterations = 1;
        }

        // Number of bytes consumed
        transferSize = numIterations * 16;

        // Check for the buffer alignment
        if (((LwU32)(&(pBuffer[index]))) % transferSize)
        {
            // Not aligned. Switch to 16B
            transferSize = 16;
        }

        //
        // This switch statement is present because falc_scp_loop_trace0
        // requires an immediate operand.
        //
        switch (transferSize)
        {
            case 32:
            {
                dmwaddr = falc_sethi_i((LwU32)(&(pBuffer[index])), DMSIZE_32B);

                // Program the sequencer for 2 iterations
                falc_scp_loop_trace0(2);
                break;
            }
            case 64:
            {
                dmwaddr = falc_sethi_i((LwU32)(&(pBuffer[index])), DMSIZE_64B);

                // Program the sequencer for 4 iterations
                falc_scp_loop_trace0(4);
                break;
            }
            case 128:
            {
                dmwaddr = falc_sethi_i((LwU32)(&(pBuffer[index])), DMSIZE_128B);

                // Program the sequencer for 8 iterations
                falc_scp_loop_trace0(8);
                break;
            }
            case 256:
            {
                dmwaddr = falc_sethi_i((LwU32)(&(pBuffer[index])), DMSIZE_256B);

                // Program the sequencer for 16 iterations
                falc_scp_loop_trace0(16);
                break;
            }
            default:
            {
                // This is the lowest size possible. Iterate only once
                dmwaddr = falc_sethi_i((LwU32)(&(pBuffer[index])), DMSIZE_16B);

                // Program the sequencer for 1 iteration
                falc_scp_loop_trace0(1);
                transferSize = 16;
                break;
            }
        }

        // Start the sequencer operations
        falc_scp_trap_suppressed(1);
        falc_trapped_dmwrite(dmwaddr);

        falc_scp_trap_suppressed(2);
        falc_trapped_dmread(dmwaddr);
        falc_dmwait();

        falc_scp_trap_suppressed(0);

        bytesXfered -= transferSize;
        index       += transferSize;
    }

    falc_scp_trap(2);
    falc_trapped_dmread(falc_sethi_i((LwU32)(&(iVector[0])), SCP_R1));
    falc_dmwait();

    // Disable the Trap mode
    falc_scp_trap(0);

    //
    // Load secret index zero, which has ACL 4'b0011 (fetchable), into all SCP registers.
    // Explaination : https://confluence.lwpu.com/display/LW/GA10X+-+Usage+of+SCP+GPRs
    //
    falc_scp_secret(0, SCP_R1);
    falc_scp_secret(0, SCP_R2);
    falc_scp_secret(0, SCP_R3);
    falc_scp_secret(0, SCP_R4);
    falc_scp_secret(0, SCP_R5);
    falc_scp_secret(0, SCP_R6);

    // Scrub all the SCP Registers
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);

    // If this condition is true, it means the that above CBC algorithm has failed
    if (bytesXfered != 0)
    {
        return ACR_ERROR_AES_CBC_DECRYPTION_FAILED;
    }

    status = ACR_OK;

    return status;
}

#endif // AHESASC
