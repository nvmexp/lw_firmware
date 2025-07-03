/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_verift210.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"


#include "acr_objacr.h"
#include "lwctassert.h"

#include "config/g_acr_private.h"

//
// Global Variables
//
/*! Global buffer containing LS ucode*/
LwU8 g_UcodeLoadBuf[2][ACR_UCODE_LOAD_BUF_SIZE]                                ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
/*! Global buffer containing LS SIG group data*/
ACR_LS_SIG_GRP_HEADER g_LsSigGrpLoadBuf[ACR_LS_SIG_GRP_LOAD_BUF_SIZE/sizeof(ACR_LS_SIG_GRP_HEADER)] ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
/*! Global buffer containing HASH of the ucode*/
LwU8 g_dmHash[ACR_AES128_DMH_SIZE_IN_BYTES]                                    ATTR_ALIGNED(ACR_AES128_DMH_SIZE_IN_BYTES)   ATTR_OVLY(".data");
/*! Global buffer containing HASH of LS SIG GRP DATA*/
LwU8 g_dmHashForGrp[ACR_ENUM_LS_SIG_GRP_ID_TOTAL][ACR_AES128_DMH_SIZE_IN_BYTES] ATTR_ALIGNED(ACR_AES128_DMH_SIZE_IN_BYTES)   ATTR_OVLY(".data");
/*! Global array containing non-secret key for key derivation function*/
LwU8 g_kdfSalt[ACR_AES128_SIG_SIZE_IN_BYTES]                                   ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)   ATTR_OVLY(".data") =
{
    0xB6,0xC2,0x31,0xE9,0x03,0xB2,0x77,0xD7,0x0E,0x32,0xA0,0x69,0x8F,0x4E,0x80,0x62
};

#if defined(ACR_SIG_VERIF)
/*!
 * @brief Callwlate DMhash with the given buffer.
 *        It uses falcon SCP instructions to load data and callwlate DMhash.
 * @param[out] pHash pointer to DMhash to be callwlated.
 * @param[in]  pData pointer to data for which Dmhash needs to callwlated.
 * @param[in]  size  Size of the data to be processed. 
 */
void acrCallwlateDmhash_GM200
(
    LwU8           *pHash,
    LwU8           *pData,
    LwU32          size
)
{
    LwU32  doneSize = 0;
    LwU8   *pKey;

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pHash, SCP_R3));
    falc_dmwait();

    while(doneSize < size)
    {
        pKey = (LwU8 *)&(pData[doneSize]);
        // H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1})
        falc_trapped_dmwrite(falc_sethi_i((LwU32)pKey, SCP_R2));
        falc_dmwait();
        falc_scp_key(SCP_R2);
        falc_scp_encrypt(SCP_R3, SCP_R4);
        falc_scp_xor(SCP_R4, SCP_R3);

        doneSize += 16U;
    }

    falc_trapped_dmread(falc_sethi_i((LwU32)pHash, SCP_R3));
    falc_dmwait();
    falc_scp_trap(0);
}

#ifndef ACR_SAFE_BUILD
/*!
 * @brief Callwlate LS signature for overlay groups. This function takes an ucode block and its PC as inputs.
 *        It then walks through list of overlays per group and check if any part of ucode block falls within any range
 *        and if yes, uses the code to callwlate the DMHASH for that group.
 *
 * @param[in] pUcode            Block of ucode which will be checked for LS Grp
 * @param[in] startPc           Starting PC of the block provided
 * @param[in] sizeOfUcode       Size of the given block
 * @param[in] totalLsGrpEntries Total LS grp entries found for this falcon's ucode
 * @param[in] pLsSigGrpEntries  Pointer to start of LS Grp entries
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_UNEXPECTED_ARGS         If sanity checks on input args fail.
 * @return ACR_ERROR_ILWALID_LS_SIG_GRP_ID   If LS Grp ID is outside expected range.
 * @return ACR_OK                            If acrCallwlateLsGrpSigHash completes successfully.
 */
static ACR_STATUS acrCallwlateLsGrpSigHash 
(
    LwU8                 *pUcode,
    LwU32                startPc,
    LwU32                sizeOfUcode,
    LwU32                totalLsGrpEntries,
    ACR_LS_SIG_GRP_ENTRY *pLsSigGrpEntries
)
{
    ACR_STATUS    status        = ACR_OK;
    LwU32         i             = 0;
    LwBool        bFound        = LW_FALSE;
    LwU8          *pU           = LW_NULL;
    LwU32         nStart        = 0;
    LwU32         nEnd          = 0;
    LwU32         endPc         = startPc + sizeOfUcode;

    CHECK_WRAP_AND_ERROR(endPc < startPc);

    // Sanity checks
    if ((pUcode == LW_NULL) || (sizeOfUcode == 0U) || (pLsSigGrpEntries == LW_NULL))
    {
        status = ACR_ERROR_UNEXPECTED_ARGS;
        goto label_return;
    }

    // Check if this block needs to be accounted for
    for (i = 0; i < totalLsGrpEntries; i++)
    {

        // StartPc is within this range
        if ((startPc >= pLsSigGrpEntries[i].startPc) &&
            (startPc < pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nStart = startPc;

            // Get the end
            if (endPc < pLsSigGrpEntries[i].endPc) {
                nEnd = endPc;
            } else {
                nEnd = pLsSigGrpEntries[i].endPc;
            }

        }
        // End PC is within the range
        else if ((endPc > pLsSigGrpEntries[i].startPc) &&
                 (endPc <= pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nEnd = endPc;

            // Get the start
            nStart = pLsSigGrpEntries[i].startPc;

        }
        // Both start and end are outside of range
        else if ((startPc <= pLsSigGrpEntries[i].startPc) &&
                  (endPc >= pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nStart = pLsSigGrpEntries[i].startPc;
            nEnd   = pLsSigGrpEntries[i].endPc;

        }
        else
        {
            bFound = LW_FALSE;
		}

        if (bFound == LW_TRUE)
        {
            // Ensure GRPID is within expected range
            if ((pLsSigGrpEntries[i].grpId == ACR_ENUM_LS_SIG_GRP_ID_ILWALID) ||
                (pLsSigGrpEntries[i].grpId > ACR_ENUM_LS_SIG_GRP_ID_TOTAL))
            {
                status = ACR_ERROR_ILWALID_LS_SIG_GRP_ID;
                goto label_return; 
            }

            CHECK_WRAP_AND_ERROR(nStart < startPc);
            pU = &(pUcode[nStart - startPc]);
            // g_dmHashForGrp starts from 0th index
            CHECK_WRAP_AND_ERROR(nEnd < nStart);
            acrCallwlateDmhash_HAL(pAcr, g_dmHashForGrp[(pLsSigGrpEntries[i].grpId - 1U)], pU, (nEnd - nStart));
        }
    }

label_return:
    return status;
}
#endif

/*!
 * @brief Encrypts the DMHASH and callwlates signatures.
 *        Expects the inputs to be atleast 16B aligned.\n
 *        Derive key using g_kdfSalt and falconID by XOR operation.\n
 *        Derive key using g_kdfSalt and falconID by XOR operation.\n
 *        Load XORed ouput in SCP register.\n
 *        Load Keys as per value of g_bIsDebug.(signal going to SCP i.e. debug or production)\n
 *        Load DmHash in SCP register and generate signatures and store them back into pDmHash and return.
 *
 * @param[in]  pSaltBuf      Input KDF Salt buffer containing data.
 * @param[in]  falconId      Falcon ID for which verification is in progress.
 * @param[in]  pDmHash       pointer containing Dmhash of the data.
 * @param[in]  bUseFalconId  flag to decide if falcon id should be used to generate derived key.
 * @param[out] pDmHash       pointer containing derived/callwlated signatures.
 *
 * @return ACR_ERROR_UNEXPECTED_ALIGNMENT If Input buffers are not aligned by ACR_AES128_SIG_SIZE_IN_BYTES.
 * @return ACR_OK                         If the signatures are successully derived.        
 */
static
ACR_STATUS acrDeriveLsVerifKeyAndEncryptDmHash
(
    LwU8   *pSaltBuf,  //16B aligned
    LwU8   *pDmHash,   //16B aligned
    LwU32  falconId,
    LwBool bUseFalconId
)
{

    // Check for alignment
    if ( (( (LwU32)pSaltBuf % ACR_AES128_SIG_SIZE_IN_BYTES ) != 0U) ||
         (( (LwU32)pDmHash % ACR_AES128_SIG_SIZE_IN_BYTES ) != 0U) ) 
    {
        return ACR_ERROR_UNEXPECTED_ALIGNMENT;
    }

    //
    // Derive key
    // derived key = AES-ECB((g_kdfSalt XOR falconID), key)
    // Signature = AES-ECB(dmhash, derived key)
    //

    // Use pSaltBuf as tmp Buf
    acrMemcpy(pSaltBuf, g_kdfSalt, 16);

    if (bUseFalconId == LW_TRUE)
    {
        acrMemxor(pSaltBuf, &falconId, 4);
    }

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(pSaltBuf), SCP_R3)); // Load XORed salt
    falc_dmwait();
    if (g_bIsDebug) {
        falc_scp_secret(ACR_LS_VERIF_KEY_INDEX_DEBUG, SCP_R2);              // Load the keys accordingly
    } else {
        falc_scp_secret(ACR_LS_VERIF_KEY_INDEX_PROD, SCP_R2);               // Load the keys accordingly
    }
    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R3, SCP_R4);                                       // Derived key is in R4
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pDmHash, SCP_R3));            // Load the hash
    falc_dmwait();
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R2);                                       // Signature is in R2
    falc_trapped_dmread(falc_sethi_i((LwU32)pDmHash, SCP_R2));             // Read back the signature
    falc_dmwait();
    falc_scp_trap(0);

    return ACR_OK;
}

/*!
 * @brief Check if versioning is supported and include it in signature callwlation
 *
 * @param[in] pLsfHeader Pointer to LSF header for falcon.
 * 
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If variable size overflow is detected.
 * @return ACR_OK If signatures are successfully verified. 
 */
static ACR_STATUS
acrAddVersionInSigCalc
(
    PLSF_LSB_HEADER pLsfHeader
)
{
    LwU32           tmp                                   = 0;
    LwU32           tmp_off                               = 0;
    LwU8            *pTmp                                 = LW_NULL;

    if (pLsfHeader->signature.bSupportsVersioning > 0U)
    {
        // Using UcodeLoadBuf[0] for version info packing
        acrMemset(g_UcodeLoadBuf[0], 0x0, ACR_UCODE_LOAD_BUF_SIZE);

        // Check to make sure size is as expected
        ct_assert(ACR_UCODE_LOAD_BUF_SIZE >= (LW_ALIGN_UP((0x4 + (LSF_FALCON_ID_END * 4 * 2)), 16))); 

        pTmp = g_UcodeLoadBuf[0];

        // Version number goes first
        acrMemcpy(pTmp, &(pLsfHeader->signature.version), sizeof(pLsfHeader->signature.version));
        pTmp += 4U;

        // Now the map
        acrMemcpy(pTmp, &(pLsfHeader->signature.depMap), pLsfHeader->signature.depMapCount);

        // Total size of version + map
        CHECK_WRAP_AND_ERROR(pLsfHeader->signature.depMapCount > LW_U32_MAX/4U);
        tmp_off = pLsfHeader->signature.depMapCount * 4U;
        CHECK_WRAP_AND_ERROR(tmp_off > LW_U32_MAX/2U);
        CHECK_WRAP_AND_ERROR(0x4U > LW_U32_MAX - (tmp_off * 2U));
        tmp = 0x4U + (tmp_off * 2U);
        CHECK_WRAP_AND_ERROR(tmp > LW_U32_MAX - 15U);
        tmp = LW_ALIGN_UP(tmp, 16U);

        // Callwlate the hash now
        acrCallwlateDmhash_HAL(pAcr, g_dmHash, g_UcodeLoadBuf[0], tmp);
    }

    return ACR_OK;
}
/*!
 * @brief Check if entire ucode has been verified and copy the next block if not.
 * 
 * @param[in] binarysize size of binary to be verified.
 * @param[in] binOffset  Offset of the binary to be verified in WPR.
 * @param[in] doneSize   size of binary already verified.
 * @param[in] actSize    actual size of the binary.
 * @param[in] pendBufInd Buffer index to ucode data pending verification.
 * 
 * @return ACR_ERROR_DMA_FAILURE  If failure in DMA is observed when trying to DMA data from WPR.
 * @return ACR_OK If signatures are successfully verified.
 */
static ACR_STATUS
acrCheckSizeAndIssueDma
(
    LwU32           binarysize,
    LwU32           binOffset,
    LwU32           doneSize,
    LwU32           actSize,
    LwU32           pendBufInd
)
{
    if (doneSize < binarysize)
    {
        actSize = binarysize - doneSize;
        if (actSize > ACR_UCODE_LOAD_BUF_SIZE) {
            actSize = ACR_UCODE_LOAD_BUF_SIZE;
        }

        // Schedule one more for pendBufInd
        CHECK_WRAP_AND_ERROR(binOffset > LW_U32_MAX - doneSize);
        if(acrIssueDma_HAL(pAcr, (LwU32)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset + doneSize,
            actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp) != actSize)
        {
            return ACR_ERROR_DMA_FAILURE;
        }
    }

    return ACR_OK;
}

/*!
 * @brief Verify LS ucode signature both code/data.
 *        DMA LS SIG group details from the ucode.\n
 *        Check whether numLsSigGrpEntriesInLwrrUcode provided a sane value or not. If not, return with error.\n
 *        DMA blocks of size ACR_UCODE_LOAD_BUF_SIZE(starting from binOffset) and callwlate their DMHash in a loop till full binary size is covered.\n
 *        Call acrCheckVersionAndAdd to check if versioning is supported and include it in signature callwlation.\n
 *        Call acrDeriveLsVerifKeyAndEncryptDmHash to generate signatures using DMHash obtained.\n
 *        Compare signatures with generated signatures and return failure if there is a mismatch.
 *
 * @param[in] pSignature LS falcon ucode signatures.
 * @param[in] falconId   Falcon ID of the falcon ucode to be verified.
 * @param[in] binarysize size of binary to be verified.
 * @param[in] binOffset  Offset of the binary to be verified in WPR.
 * @param[in] pLsfHeader Pointer to LSF header for falcon.
 * @param[in] bIsUcode   Flag to determine if it code part or data part of ucode.
 *
 * @return ACR_ERROR_DMA_FAILURE  If failure in DMA is observed when trying to DMA data from WPR. 
 * @return ACR_ERROR_ILWALID_LS_SIG_GRP_ENTRY_COUNT If invalid LS sig group entry count in present in the code block. 
 * @return ACR_ERROR_LS_SIG_VERIF_FAIL If callwlated signatures do not match with pSignature.(Signatures of the ucode supplied along with ucode).
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If variable size overflow is detected.
 * @return ACR_OK If signatures are successfully verified. 
 */
ACR_STATUS
acrVerifySignature_GM200
(
    LwU8            *pSignature,
    LwU32           falconId,
    LwU32           binarysize,
    LwU32           binOffset,
    PLSF_LSB_HEADER pLsfHeader,
    LwBool          bIsUcode
)
{
    ACR_STATUS      status                                = ACR_OK;
    LwU32           doneSize                              = 0;
    LwU32           actSize                               = 0;
    LwU32           doneBufInd                            = 0;
    LwU32           pendBufInd                            = 1;
    LwU32           tmp                                   = 0;
    LwU32           i                                     = 0;
    LwU32           dmaSize                               = 0;
    ACR_LS_SIG_GRP_HEADER *pLsSigGrpHeader                = LW_NULL; 
#ifndef ACR_SAFE_BUILD
    LwU32           maxLsSigGrpEntriesPossibleInLwrrUcode = 0;
    LwBool          bLsSigGrp                             = LW_FALSE;
    LwU32           numLsSigGrpEntriesInLwrrUcode         = 0;
    ACR_LS_SIG_GRP_ENTRY  *pLsSigGrpEntry                 = LW_NULL;
#endif

    // Reset the global hash
    acrMemset(g_dmHash, 0x0, 16);

    // Reset the hash for LS sig groups
    for (i = 0; i < ACR_ENUM_LS_SIG_GRP_ID_TOTAL; i++)
    {
        acrMemset(g_dmHashForGrp[i], 0x0, 16);
    }

    if (binarysize > ACR_UCODE_LOAD_BUF_SIZE) {
        actSize = ACR_UCODE_LOAD_BUF_SIZE;
    } else {
        actSize = binarysize;
    }
    if (bIsUcode == LW_TRUE)
    {
        // Read LS_SIG_GROUP details. It is last block of ucode

        //
        // Individual falcons will place the LS group details at the end of their ucode in the following format.
        //
        //  ------ Ucode
        //  ------ LS Grp block start (1 block at present)
        //  ------ 3 MAGIC DWORDS to identify the presence of LS group for this ucode
        //  ------ 4th DWORD gives the total number of entries
        //  ------ 5th DWORD is where the LS GRP entry starts
        //  ------ --- LS GRP Entry has 3 DWORDS. 
        //  ------ --- --- Start, end and grp ID
        //


	// |-----------------------------------------------|
	// | |
	// | Ucode |
	// | |
	// | |
	// |-- |-----------------------------------------------| --|
	// | | Magic Pattern #1 | |
	// | | Magic Pattern #2 | |
	// | | Magic Pattern #3 | |
	// | | Number of sig grp entries | |
	// | | Start PC | |
	// LS Sig Grp Entry #1 --| | End PC | |
	// | | Grp ID | |-- LS Sig Grp Block
	// |-- |-----------------------------------------------| |
	// | | Start PC | |
	// LS Sig Grp Entry #2 --| | End PC | |
	// | | Grp ID | |
	// |-- |-----------------------------------------------| |
	// ... |
 	// |
	// |-- |-----------------------------------------------| |
	// | | Start PC | |
	// LS Sig Grp Entry #N --| | End PC | |
	// | | Grp ID | |
	// |-- |-----------------------------------------------| --|
        //

        if (binarysize > ACR_LS_SIG_GRP_LOAD_BUF_SIZE)
        {
            //
            // The size of LS grp depends on the size of g_LsSigGrpLoadBuf buffer. At present, code doesn't expect more than 20 
            // entries. (3 MAGIC + 1 NoofEntries + (20 * 3)) -> 64 DWORDS -> 256 bytes
            //
            dmaSize = binarysize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE;
            CHECK_WRAP_AND_ERROR(binarysize < ACR_LS_SIG_GRP_LOAD_BUF_SIZE);
            CHECK_WRAP_AND_ERROR(binOffset > (LW_U32_MAX - dmaSize));
            if(acrIssueDma_HAL(pAcr, (LwU32)g_LsSigGrpLoadBuf, LW_FALSE,  (binOffset + dmaSize),
                ACR_LS_SIG_GRP_LOAD_BUF_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp) != ACR_LS_SIG_GRP_LOAD_BUF_SIZE)
            {
                return ACR_ERROR_DMA_FAILURE;
            }

            pLsSigGrpHeader =  g_LsSigGrpLoadBuf;

#ifndef ACR_SAFE_BUILD
            //
            // Compare the MAGIC pattern and number of elements.
            // MAGIC pattern is temporary solution. In future, we would like to pass the presence of LS group details
            // via LSB header.
            //
            if ((pLsSigGrpHeader->magicPattern1 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_1) &&
                (pLsSigGrpHeader->magicPattern2 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_2) &&
                (pLsSigGrpHeader->magicPattern3 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_3) &&
                (pLsSigGrpHeader->numLsSigGrps > 0U))
            {

                bLsSigGrp                     = LW_TRUE;
                pLsSigGrpEntry                = (ACR_LS_SIG_GRP_ENTRY *) ((void *) (g_LsSigGrpLoadBuf + 1U));
                numLsSigGrpEntriesInLwrrUcode = pLsSigGrpHeader->numLsSigGrps;

                // Check whether numLsSigGrpEntriesInLwrrUcode  provided is a sane value
                maxLsSigGrpEntriesPossibleInLwrrUcode  = ((ACR_LS_SIG_GRP_LOAD_BUF_SIZE - (sizeof(ACR_LS_SIG_GRP_HEADER))) / sizeof(ACR_LS_SIG_GRP_ENTRY));
                if (numLsSigGrpEntriesInLwrrUcode > maxLsSigGrpEntriesPossibleInLwrrUcode )
                {
                    //Invalid number of entries, error
                    status = ACR_ERROR_ILWALID_LS_SIG_GRP_ENTRY_COUNT;
                    goto label_return;
                }

            }
#endif
        }
    }

    if(acrIssueDma_HAL(pAcr, (LwU32)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset,
        actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp) != actSize)
    {
            return ACR_ERROR_DMA_FAILURE;
    }

    do
    {
        falc_dmwait();
        doneSize += actSize;

        // Swap the pend and done buffer indexes
        tmp = doneBufInd;
        doneBufInd = pendBufInd;
        pendBufInd = tmp;

        // Save the old actualSize
        tmp = actSize;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckSizeAndIssueDma(binarysize, binOffset, doneSize, actSize, pendBufInd));

        // Handle the done buffer now
        acrCallwlateDmhash_HAL(pAcr, g_dmHash, g_UcodeLoadBuf[doneBufInd], tmp);
#ifndef ACR_SAFE_BUILD
        if (bLsSigGrp == LW_TRUE)
        {
            //
            // Bootloader resides first and not accounted in LS sig grp callwlation
            // This check will be false only for first block
            //
            if ((doneSize - tmp) >= pLsfHeader->blCodeSize)
            {
                // Use the previously completed buffer to callwlate LS signature for overlay groups
                status = acrCallwlateLsGrpSigHash(g_UcodeLoadBuf[doneBufInd], (doneSize - tmp - pLsfHeader->blCodeSize), tmp, numLsSigGrpEntriesInLwrrUcode , pLsSigGrpEntry);
                if (status != ACR_OK)
                {
                    goto label_return;
                }
            }
        }
#endif
    } while (doneSize < binarysize);

    // Check if versioning is supported and include it in signature callwlation
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAddVersionInSigCalc(pLsfHeader));

    if ((status = acrDeriveLsVerifKeyAndEncryptDmHash(g_UcodeLoadBuf[0], g_dmHash, falconId, LW_TRUE)) != ACR_OK)
    {
        goto label_return;
    }

    //
    // g_dmHash has the signature now
    // compare with given signature
    //

    if( acrMemcmp(g_dmHash, pSignature, 16) != ACR_OK)
    {
        status = ACR_ERROR_LS_SIG_VERIF_FAIL;
        goto label_return;
    }

label_return:
    return status;
}
#endif
