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
 * @file: acr_verify_ls_signature_ga10x.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_signature.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"
#include "dev_fb.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Buffer to load ucode while callwlating DMHASH
#define ACR_UCODE_LOAD_BUF_SIZE        ACR_FALCON_BLOCK_SIZE_IN_BYTES
extern LwU8 g_UcodeLoadBuf[2][ACR_UCODE_LOAD_BUF_SIZE];
extern LwU8 g_LsSigGrpLoadBuf[ACR_LS_SIG_GRP_LOAD_BUF_SIZE];
extern LwU8 g_dmHashForGrp[ENUM_LS_SIG_GRP_ID_TOTAL][ACR_AES128_DMH_SIZE_IN_BYTES];

#ifdef LIB_CCC_PRESENT
#include "tegra_cdev.h"
#endif

#if !defined(BSI_LOCK)
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
#endif

#ifdef AHESASC
extern ACR_SCRUB_STATE  g_scrubState;
#endif

//
// Extern Variables
//
extern LwU8             g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX];
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;
extern LwBool           g_bIsDebug;
extern LwU8             g_lsSignature[];

// static local functions
extern LwBool     _acrIsFalconSupported(LwU32 falconId);
extern ACR_STATUS _acrCallwlateLsGrpSigHash(LwU8 *pUcode, LwU32 startPc, LwU32  sizeOfUcode,
                                            LwU32 totalLsGrpEntries, ACR_LS_SIG_GRP_ENTRY *pLsSigGrpEntries);

#if defined(AHESASC) && defined(NEW_WPR_BLOBS)
/*!
 * Generate LASSA HS signature(AES-DM)
 */
ACR_STATUS
acrGenerateLassahsSignature_GA10X
(
    PLSF_LSB_HEADER_WRAPPER pWrapper,
    LwU32                   falconId,
    LwU32                   binarySize,
    LwU32                   binOffset,
    LwBool                  bIsUcode
)
{
    ACR_STATUS      status                                = ACR_OK;
    LwU32           doneSize                              = 0;
    LwU32           actSize                               = 0;
    LwU32           blCodeSize                            = 0;
    LwU32           doneBufInd                            = 0;
    LwU32           pendBufInd                            = 1;
    LwU32           tmp                                   = 0;
    LwU32           maxLsSigGrpEntriesPossibleInLwrrUcode = 0;
    LwU32           i                                     = 0;
    LwU32           numLsSigGrpEntriesInLwrrUcode         = 0;
    LwBool          bLsSigGrp                             = LW_FALSE;
    ACR_LS_SIG_GRP_ENTRY  *pLsSigGrpEntry                 = NULL;
    ACR_LS_SIG_GRP_HEADER *pLsSigGrpHeader                = NULL;

    // Reset the hash for LS sig groups
    for (i = 0; i < ENUM_LS_SIG_GRP_ID_TOTAL; i++)
    {
        acrlibMemset_HAL(pAcrlib, g_dmHashForGrp[i], 0x0, 16);
    }

    if (binarySize > ACR_UCODE_LOAD_BUF_SIZE)
        actSize = ACR_UCODE_LOAD_BUF_SIZE;
    else
        actSize = binarySize;

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

        if (binarySize > ACR_LS_SIG_GRP_LOAD_BUF_SIZE)
        {
            //
            // The size of LS grp depends on the size of g_LsSigGrpLoadBuf buffer. At present, code doesn't expect more than 20
            // entries. (3 MAGIC + 1 NoofEntries + (20 * 3)) -> 64 DWORDS -> 256 bytes
            //
            acrIssueDma_HAL(pAcr, (void *)g_LsSigGrpLoadBuf, LW_FALSE,  (binOffset + binarySize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE),
                ACR_LS_SIG_GRP_LOAD_BUF_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp);

            pLsSigGrpHeader = (ACR_LS_SIG_GRP_HEADER *) g_LsSigGrpLoadBuf;

            //
            // Compare the MAGIC pattern and number of elements.
            // MAGIC pattern is temporary solution. In future, we would like to pass the presence of LS group details
            // via LSB header.
            //
            if ((pLsSigGrpHeader->magicPattern1 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_1) &&
                (pLsSigGrpHeader->magicPattern2 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_2) &&
                (pLsSigGrpHeader->magicPattern3 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_3) &&
                (pLsSigGrpHeader->numLsSigGrps > 0))
            {

                bLsSigGrp                     = LW_TRUE;
                pLsSigGrpEntry                = (ACR_LS_SIG_GRP_ENTRY *) ((LwU32) g_LsSigGrpLoadBuf + sizeof(ACR_LS_SIG_GRP_HEADER));
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
        }
    }

    // LsSigGroup signature is still using AES
    if (bLsSigGrp == LW_TRUE)
    {
        acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset,
            actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp);

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

            if (doneSize < binarySize)
            {
                actSize = binarySize - doneSize;
                if (actSize > ACR_UCODE_LOAD_BUF_SIZE)
                    actSize = ACR_UCODE_LOAD_BUF_SIZE;

                // Schedule one more for pendBufInd
                acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset + doneSize,
                    actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp);
            }

            //
            // Bootloader resides first and not accounted in LS sig grp callwlation
            // This check will be false only for first block
            //
            status = acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE, pWrapper, &blCodeSize);

            if (status != ACR_OK)
            {
                goto label_return;
            }

            if ((doneSize - tmp) >= blCodeSize)
            {
                // Use the previously completed buffer to callwlate LS signature for overlay groups
                status = _acrCallwlateLsGrpSigHash(g_UcodeLoadBuf[doneBufInd], (doneSize - tmp - blCodeSize), tmp, numLsSigGrpEntriesInLwrrUcode , pLsSigGrpEntry);
                if (status != ACR_OK)
                {
                    goto label_return;
                }
            }
        } while (doneSize < binarySize);

        //
        // Copy LS Grp signatures. Grp ID 0 is invalid
        //
        for (i = ENUM_LS_SIG_GRP_ID_1; i <= ENUM_LS_SIG_GRP_ID_TOTAL; i++)
        {
            // g_dmHashForGrp starts from 0th index
            if (falconId == LSF_FALCON_ID_SEC2)
            {
                status = acrCopyLsGrpSigToRegsForSec2_HAL(pAcr, (LwU32 *)(g_dmHashForGrp[i - 1]), i);
                if (status != ACR_OK)
                {
                    goto label_return;
                }
            }
        }
    }

label_return:
    return status;
}

#ifdef ACR_SIG_VERIF
/*
 * @brief Scrub the code and data content on error scenarios. Also write the WPR header back to FB.
 */
static
ACR_STATUS _acrScrubContentAndWriteWpr
(
    PLSF_LSB_HEADER_WRAPPER pWrapper
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 ucodeOffset;
    LwU32 ucodeSize;
    LwU32 dataSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pWrapper, (void *)&ucodeOffset));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                        pWrapper, (void *)&ucodeSize));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                        pWrapper, (void *)&dataSize));

    // Scrub code
    if ((acrIssueDma_HAL(pAcr, g_scrubState.zeroBuf, LW_FALSE, ucodeOffset, ucodeSize,
         ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != ucodeSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Scrub data
    if ((acrIssueDma_HAL(pAcr, g_scrubState.zeroBuf, LW_FALSE, (ucodeOffset + ucodeSize), dataSize,
         ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != dataSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Write all WPR header back to FB and return...
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteAllWprHeaderWrappers_HAL(pAcr));

    return status;
}

/*!
 * @brief Fetches bin version from all WPR entries
 */
static ACR_STATUS
_acrGetAllBilwersions
(
    LwU32 *pFalconBilwersions
)
{

    LwU32           index = 0;
    PLSF_WPR_HEADER_WRAPPER pWrapper;
    LwU32           falconId;
    LwU32           lsbOffset;
    LwU32           bilwersion;
    ACR_STATUS      status;

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        // Assign invalid Bin version to all falcons first
        pFalconBilwersions[index] = LSF_FALCON_BIN_VERSION_ILWALID;
    }

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        pWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWrapper, (void *)&lsbOffset));

        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            break;
        }

        // Overiding bin version for falcons present in WPR header along with value
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_BIN_VERSION,
                                          pWrapper, (void *)&bilwersion));
        pFalconBilwersions[falconId] = bilwersion;
    }

    return ACR_OK;
}

/*!
 * @brief Function which checks for revocation info on LS falcon and corresponding dependencies
 */
ACR_STATUS
acrCheckForLSRevocationExt_GA10X
(
    LwU32 falconId,
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper,
    LwU32 *pBilwersions
)
{
    LSF_UCODE_DESC_WRAPPER   lsfDescWrapper;
    LwU32                    bilwersion;
    ACR_STATUS               status;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                        pLsbHeaderWrapper, (void *)&lsfDescWrapper));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_BIN_VERION,
                                                                        &lsfDescWrapper, (void *)&bilwersion));

    // Make sure the version that we used for sig callwlation is what is present in WPRHEADER
    if (bilwersion != pBilwersions[falconId])
    {
        return ACR_ERROR_REVOCATION_CHECK_FAIL;
    }

    // Check if dependencies are passing
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_CHECK_DEPENDENCY,
                                                                         &lsfDescWrapper, (void *)pBilwersions));

    return ACR_OK;
}
#endif // ACR_SIG_VERIF

ACR_STATUS
acrValidateSignatureAndScrubUnusedWprExt_GA10X
(
    void
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 index = 0;
    LwU32 wprRegionSize = 0;
    LSF_LSB_HEADER_WRAPPER   lsbHeaderWrpper;
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper = &lsbHeaderWrpper;
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper = NULL;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwU32  falconId;
    LwU32  lsbOffset;
    LwU32  blDataOffset;
    LwU32  blDataSize;
    LwU32  ucodeOffset;
    LwU32  ucodeSize;
    LwU32  dataSize;
    LwU32  hsOvlSigBlobSize     = 0;
    LwU32  hsOvlSigBlobOffset   = 0;
    LwBool bHsOvlSigBlobPresent = LW_FALSE;

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllWprHeaderWrappers_HAL(pAcr));

    //
    // Signature-verification is always enabled for RISC-V engines when
    // RISC-V LS support is enabled.
    //
#ifdef ACR_SIG_VERIF
    LwU32 falconBilwersions[LSF_FALCON_ID_END];
    LSF_UCODE_DESC_WRAPPER   lsfUcodeDescWrapper;
    status = _acrGetAllBilwersions(falconBilwersions);

    if (status != ACR_OK)
    {
        return status;
    }

#ifdef LIB_CCC_PRESENT
    status_t statusCcc = ERR_GENERIC;
    // init_crypto_devices() only needs to call once before multi RSA operation
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(init_crypto_devices(), ACR_ERROR_LWPKA_INIT_CRYPTO_DEVICE_FAILED);
#endif // LIB_CCC_PRESENT
#endif // ACR_SIG_VERIF

    // Sets up scrubbing
    g_scrubState.scrubTrack = 0;

    acrScrubUnusedWprWithZeroes_HAL(pAcr, 0, LW_ALIGN_UP((sizeof(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                                                          LSF_WPR_HEADER_ALIGNMENT));
                                                          
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index = 0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWprHeaderWrapper, (void *)&lsbOffset));

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            // Scrub till the end of LS ucodes content
            if (NULL == pLsbHeaderWrapper)
            {
                // ACR should get enabled if we have at least one LSFM falcon
                return ACR_ERROR_UNREACHABLE_CODE;
            }

            //
            // Get the end address of LS ucodes within WPR region i.e. region before shared data subWPRs
            // We do not need to scrub the shared data subWprs here
            //
            pRegionProp = &(g_desc.regions.regionProps[ACR_WPR1_REGION_IDX]);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&blDataOffset));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blDataSize));

            LwU64 lsfEndAddr = LW_ALIGN_UP(((pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT)
                                             + blDataOffset + blDataSize), SUB_WPR_SIZE_ALIGNMENT);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                                pLsbHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

            if(bHsOvlSigBlobPresent == LW_TRUE)
            {

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                    pLsbHeaderWrapper, (void *)&hsOvlSigBlobOffset));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                    pLsbHeaderWrapper, (void *)&hsOvlSigBlobSize));

                lsfEndAddr = LW_ALIGN_UP(((pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT)
                                           + hsOvlSigBlobOffset + hsOvlSigBlobSize), SUB_WPR_SIZE_ALIGNMENT);
            }

            wprRegionSize = (LwU32)(lsfEndAddr - ((LwU64)pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT));

            // Scrub the empty WPR after last LS ucode but till Data subWPRs
            acrScrubUnusedWprWithZeroes_HAL(pAcr, wprRegionSize, 0);
            break;
        }

        // Scrub the gap between last known scrub point and this LSB header
        acrScrubUnusedWprWithZeroes_HAL(pAcr, lsbOffset, LW_ALIGN_UP(sizeof(LSF_LSB_HEADER_WRAPPER),
                                                                                 LSF_LSB_HEADER_ALIGNMENT));

        // Lets reset the status
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, (void *)&lsbHeaderWrpper));

        // Scrub the gap between last known scrub point and ucode/data
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                            pLsbHeaderWrapper, (void *)&ucodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&ucodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&dataSize));
        acrScrubUnusedWprWithZeroes_HAL(pAcr, ucodeOffset, ucodeSize);
        acrScrubUnusedWprWithZeroes_HAL(pAcr, ucodeOffset + ucodeSize, dataSize);

        // We don't expect bootloader data for falcons other than PMU and DPU.
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                            pLsbHeaderWrapper, (void *)&blDataOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&blDataSize));
        if ((status = acrSanityCheckBlData_HAL(pAcr, falconId, blDataOffset, blDataSize))!= ACR_OK)
        {
            return status;
        }

        acrScrubUnusedWprWithZeroes_HAL(pAcr, blDataOffset, blDataSize);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                            pLsbHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

        if(bHsOvlSigBlobPresent == LW_TRUE)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&hsOvlSigBlobOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&hsOvlSigBlobSize));
            acrScrubUnusedWprWithZeroes_HAL(pAcr, hsOvlSigBlobOffset, hsOvlSigBlobSize);
        }

        if (!acrIsFalconSupported(falconId))
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

//
// Signature-verification is always enabled for RISC-V engines when
// RISC-V LS support is enabled.
//
#ifdef ACR_SIG_VERIF
        ACR_STATUS oldstatus = ACR_OK;
        LwU8      *pSignature = (LwU8 *)g_lsSignature;

        // LWENC makefile uses LWENC0 as falcon Id for all LWENCs.
        if (falconId == LSF_FALCON_ID_LWENC1 || falconId == LSF_FALCON_ID_LWENC2)
        {
            falconId = LSF_FALCON_ID_LWENC0;
        }

        // Get the lsf ucode descriptor
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                            pLsbHeaderWrapper, (void *)&lsfUcodeDescWrapper));

        // Get the code signature
        if (g_bIsDebug)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_CODE,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_CODE,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }

        // Verify code signature
        if ((status = acrValidateLsSignature_HAL(pAcr, falconId, pSignature, ucodeSize, ucodeOffset, &lsfUcodeDescWrapper, LW_TRUE)) != ACR_OK)
        {
            acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
            goto VERIF_FAIL;
        }

        // Get the data signature
        if (g_bIsDebug)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_DATA,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfUcodeDescWrapperCtrl_HAL(pAcrlib, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_DATA,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }

        // Verify data signature
        if ((status = acrValidateLsSignature_HAL(pAcr, falconId, pSignature, dataSize, (ucodeSize + ucodeOffset), &lsfUcodeDescWrapper, LW_FALSE)) != ACR_OK)
        {
            acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
            goto VERIF_FAIL;
        }

        status = acrCheckForLSRevocationExt_HAL(pAcr, falconId, pLsbHeaderWrapper, falconBilwersions);

        if (status != ACR_OK)
        {
            acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
            goto VERIF_FAIL;
        }

VERIF_FAIL:

        if (status != ACR_OK)
        {
            oldstatus = status;

#ifdef ACR_FMODEL_BUILD
            if ((falconId == LSF_FALCON_ID_FECS) || (falconId == LSF_FALCON_ID_GPCCS))
            {
                //
                // TODO: Resolve this
                // Some falcons such as FECS and GPCCS doesnt have signatures for binaries in debug elwiroment (fmodel)
                // Filed HW bug 200179871
                //
                oldstatus = ACR_OK;
            }
#endif // ACR_FMODEL_BUILD
            status = _acrScrubContentAndWriteWpr(pLsbHeaderWrapper);

            if (oldstatus != ACR_OK)
                return oldstatus;
            else
                return status;
        }
#endif // ACR_SIG_VERIF
    }

    // Compare the size now to make sure we accounted for all of WPR region
    if (g_scrubState.scrubTrack != wprRegionSize)
    {
        return ACR_ERROR_SCRUB_SIZE;
    }

    return status;
}

#endif //defined(AHESASC) && defined (NEW_WPR_BLOBS)
