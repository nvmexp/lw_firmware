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
 * @file: acr_verify_ls_signature_gh100.c
 */
/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "acr.h"
#include "acr_signature.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "dev_gc6_island.h"
#include "dev_fb.h"
#include "dev_top.h"
#include "tegra_cdev.h"
#include "config/g_acr_private.h"


/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */

// Buffer to load ucode while callwlating DMHASH
#define ACR_UCODE_LOAD_BUF_SIZE        ACR_FALCON_BLOCK_SIZE_IN_BYTES

/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
extern LwU8             g_lsSignature[];
extern LwBool           _acrIsFalconSupported(LwU32 falconId);
extern LwU8             g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8             g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;


/* ------------------------ Function Prototypes ---------------------------- */

#ifdef IS_LASSAHS_REQUIRED
ACR_STATUS _acrCallwlateLsGrpSigHash(LwU8 *pUcode, LwU32 startPc, LwU32  sizeOfUcode,
                                     LwU32 totalLsGrpEntries, ACR_LS_SIG_GRP_ENTRY *pLsSigGrpEntries) ATTR_OVLY(OVL_NAME);
#endif // IS_LASSAHS_REQUIRED 

ACR_STATUS _acrScrubRegionWithZeroes(LwU32 regionOffset, LwU32 regionSize) ATTR_OVLY(OVL_NAME);

/* ------------------------ Global Variables ------------------------------- */
ACR_FLCN_BL_CMDLINE_ARGS  g_blCmdLineArgs       ATTR_OVLY(".data") ATTR_ALIGNED(ACR_FLCN_BL_CMDLINE_ALIGN);

#ifdef IS_LASSAHS_REQUIRED
LwU8 g_UcodeLoadBuf[2][ACR_UCODE_LOAD_BUF_SIZE]                                ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
LwU8 g_LsSigGrpLoadBuf[ACR_LS_SIG_GRP_LOAD_BUF_SIZE]                           ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
LwU8 g_dmHashForGrp[ENUM_LS_SIG_GRP_ID_TOTAL][ACR_AES128_DMH_SIZE_IN_BYTES]    ATTR_ALIGNED(ACR_AES128_DMH_SIZE_IN_BYTES)   ATTR_OVLY(".data");
#endif // IS_LASSAHS_REQUIRED 


#ifdef IS_LASSAHS_REQUIRED
/*!
 * Callwlate LS signature for overlay groups. This function takes an ucode block and its PC as inputs.
 * It then walks through list of overlays per group and check if any part of ucode block falls within any range
 * and if yes, uses the code to callwlate the DMHASH for that group.
 *
 * @param[in] pUcode            Block of ucode which will be checked for LS Grp
 * @param[in] startPc           Starting PC of the block provided
 * @param[in] sizeOfUcode       Size of the given block
 * @param[in] totalLsGrpEntries Total LS grp entries found for this falcon's ucode
 * @param[in] pLsSigGrpEntries  Pointer to start of LS Grp entries
 */
ACR_STATUS _acrCallwlateLsGrpSigHash
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
    LwU32         j             = 0;
    LwBool        bFound        = LW_FALSE;
    LwU32         nStart        = 0;
    LwU32         nEnd          = 0;
    LwU8          *pU           = 0;
    LwU32         endPc;

    // avoiding wrap around
    CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(startPc, sizeOfUcode, LW_U32_MAX);

    endPc = startPc + sizeOfUcode;
    
    // Sanity checks
    if ((pUcode == 0) || (!sizeOfUcode) || (pLsSigGrpEntries == 0))
    {
        status = ACR_ERROR_UNEXPECTED_ARGS;
        goto Cleanup;
    }

    // Check if this block needs to be accounted for
    for (i = 0; i < totalLsGrpEntries; i++)
    {
        bFound = LW_FALSE;

        // StartPc is within this range
        if ((startPc >= pLsSigGrpEntries[i].startPc) &&
            (startPc < pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nStart = startPc;

            // Get the end
            if (endPc < pLsSigGrpEntries[i].endPc)
                nEnd = endPc;
            else
                nEnd = pLsSigGrpEntries[i].endPc;

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

        if (bFound == LW_TRUE)
        {
            // Ensure GRPID is within expected range
            if ((pLsSigGrpEntries[i].grpId == ENUM_LS_SIG_GRP_ID_ILWALID) ||
                ((pLsSigGrpEntries[i].grpId > ENUM_LS_SIG_GRP_ID_TOTAL) && (pLsSigGrpEntries[i].grpId != ENUM_LS_SIG_GRP_ID_ALL)))
            {
                status = ACR_ERROR_ILWALID_LS_SIG_GRP_ID;
                goto Cleanup;
            }
            // Ensure array index >= 0 and nEnd >= nStart
            if ((nStart >= startPc) && (nEnd >= nStart))
            {
                pU = &(pUcode[nStart - startPc]);
                if (pLsSigGrpEntries[i].grpId == ENUM_LS_SIG_GRP_ID_ALL)
                {
                    for (j = ENUM_LS_SIG_GRP_ID_1; j <= ENUM_LS_SIG_GRP_ID_TOTAL; j++)
                    {
                        // g_dmHashForGrp starts from 0th index
                        acrCallwlateDmhash_HAL(pAcr, g_dmHashForGrp[(j - 1)], pU, (nEnd - nStart));
                    }
                }
                else
                {
                    // g_dmHashForGrp starts from 0th index
                    acrCallwlateDmhash_HAL(pAcr, g_dmHashForGrp[(pLsSigGrpEntries[i].grpId - 1)], pU, (nEnd - nStart));
                }
            }
            else
            {
                // array index < 0 
                status = ACR_ERROR_ILWALID_OPERATION;
                goto Cleanup;
            }
        }
    }

Cleanup:
    return status;
}
#endif // IS_LASSAHS_REQUIRED

/*!
 * At present we have only one set of registers reserved enough to hold signature for one group.
 * Only 2 DWORDS are saved and compared.
 */
ACR_STATUS
acrCopyLsGrpSigToRegsForSec2_GH100(LwU32 *pSig, LwU32 grpId)
{
    ACR_STATUS status = ACR_OK;
    LwU32      regAddrD0 = 0;
    LwU32      regAddrD1 = 0;

    switch (grpId)
    {
        case ENUM_LS_SIG_GRP_ID_1:
            regAddrD0 = LW_PGC6_BSI_SELWRE_SCRATCH_0;
            regAddrD1 = LW_PGC6_BSI_SELWRE_SCRATCH_1;
            break;
        default:
            status = ACR_ERROR_ILWALID_LS_SIG_GRP_ID;
            goto label_return;
    }

    ACR_REG_WR32(BAR0, regAddrD0, pSig[0]);
    ACR_REG_WR32(BAR0, regAddrD1, pSig[1]);

label_return:
    return status;
}


/*!
 * @brief Do sanity check on the bootloader arguments provided
 */
ACR_STATUS
acrSanityCheckBlData_GH100
(
    LwU32              falconId,
    LwU32              blDataOffset,
    LwU32              blDataSize,
    LwU32              lwrScrubPosition
)
{
    ACR_STATUS status = ACR_OK;
    //
    // We dont expect bootloader data for falcons other than PMU, DPU, SEC2 and
    // GR falcons
    //
    if ( (blDataOffset >= lwrScrubPosition) && (blDataSize > 0))
    {
        // Bootloader data size is not expected to be more than 256 bytes
        if (blDataSize > LSF_LS_BLDATA_EXPECTED_SIZE)
        {
            return ACR_ERROR_BLDATA_SANITY;
        }

        // Read loader config
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)&g_blCmdLineArgs, LW_FALSE,  blDataOffset,
                                          blDataSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

        // Write KNOWN HALT ASM code into RESERVED location..

        switch (falconId)
        {
            case LSF_FALCON_ID_SEC2:
            case LSF_FALCON_ID_FECS:
            case LSF_FALCON_ID_GPCCS:
                g_blCmdLineArgs.genericBlArgs.reserved[0] = ACR_FLCN_BL_RESERVED_ASM;
                break;
#ifdef FBFALCON_PRESENT
            case LSF_FALCON_ID_FBFALCON:
                // TODO: Move FbFlcn to generic bootloader. Filed bug 200406898
                g_blCmdLineArgs.legacyBlArgs.reserved = ACR_FLCN_BL_RESERVED_ASM;
                break;
#endif
            default:
                return ACR_ERROR_BLDATA_SANITY;
                break;
        }

        // Write back loader config
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)&g_blCmdLineArgs, LW_FALSE,  blDataOffset,
                                          blDataSize, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    }

    return status;
}

/*!
 * @brief Scrub FB region with zeroes
 *
 * @param[in] regionOffset  offset of region in FB.
 * @param[in] regionSize    size of region to be scrubbed.
 */
ACR_STATUS
_acrScrubRegionWithZeroes
(
    LwU32   regionOffset,
    LwU32   regionSize
)
{
    LwU32         sizeScrubbed = 0;
    LwU32         scrubSize    = 0;
    ACR_STATUS    status       = ACR_OK;

    while(sizeScrubbed < regionSize)
    {
        scrubSize = LW_MIN(regionSize - sizeScrubbed, ACR_GENERIC_BUF_SIZE_IN_BYTES);
        // Scrub region
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, g_tmpGenericBuffer, LW_FALSE, regionOffset, scrubSize,
                                         ACR_DMA_TO_FB, ACR_DMA_SYNC_NO, &g_dmaProp));
        regionOffset += scrubSize;
        sizeScrubbed += scrubSize;
    }

    return status;
}

/*!
 * @brief Scrub FB with zeroes
 *
 * @param[in] wprIndex  Index into g_desc region properties holding WPR region
 */
ACR_STATUS
acrScrubUnusedWprWithZeroes_GH100
(
    LwU32    nextKnownOffset,
    LwU32    size,
    LwU32    *pLwrScrubPosition
)
{
    LwU32         gap          = 0;
    ACR_STATUS    status       = ACR_OK;

    if (*pLwrScrubPosition == 0)
    {
        // Zero out the buf
        acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);
    }

    if (nextKnownOffset >= *pLwrScrubPosition)
    {       
        gap = (nextKnownOffset) - (*pLwrScrubPosition);

        if (gap > 0)
        {
#ifndef ACR_SKIP_SCRUB
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrScrubRegionWithZeroes(*pLwrScrubPosition, gap));
#endif

        }

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(*pLwrScrubPosition, gap, LW_U32_MAX);
        *pLwrScrubPosition += gap;

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(*pLwrScrubPosition, size, LW_U32_MAX);
        *pLwrScrubPosition += size;
    }
    else
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    return ACR_OK;
}

#ifdef IS_LASSAHS_REQUIRED
/*!
 * Generate LASSA HS signature(AES-DM)
 */
ACR_STATUS
acrGenerateLassahsSignature_GH100
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
        acrMemset_HAL(pAcr, g_dmHashForGrp[i], 0x0, 16);
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
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrIssueDma_HAL(pAcr, (void *)g_LsSigGrpLoadBuf, LW_FALSE,  (binOffset + binarySize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE),
                ACR_LS_SIG_GRP_LOAD_BUF_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

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
                pLsSigGrpEntry                = (ACR_LS_SIG_GRP_ENTRY *) (g_LsSigGrpLoadBuf + sizeof(ACR_LS_SIG_GRP_HEADER));
                numLsSigGrpEntriesInLwrrUcode = pLsSigGrpHeader->numLsSigGrps;

                // Check whether numLsSigGrpEntriesInLwrrUcode  provided is a sane value
                maxLsSigGrpEntriesPossibleInLwrrUcode  = ((ACR_LS_SIG_GRP_LOAD_BUF_SIZE - (sizeof(ACR_LS_SIG_GRP_HEADER))) / sizeof(ACR_LS_SIG_GRP_ENTRY));
                if (numLsSigGrpEntriesInLwrrUcode > maxLsSigGrpEntriesPossibleInLwrrUcode )
                {
                    //Invalid number of entries, error
                    status = ACR_ERROR_ILWALID_LS_SIG_GRP_ENTRY_COUNT;
                    goto Cleanup;
                }
            }
        }
    }

    // LsSigGroup signature is still using AES
    if (bLsSigGrp == LW_TRUE)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset,
            actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp));

        do
        {
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
                CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset + doneSize,
                    actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp));
            }

            //
            // Bootloader resides first and not accounted in LS sig grp callwlation
            // This check will be false only for first block
            //
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE, pWrapper, &blCodeSize));

            if ((doneSize - tmp) >= blCodeSize)
            {
                // Use the previously completed buffer to callwlate LS signature for overlay groups
                CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrCallwlateLsGrpSigHash(g_UcodeLoadBuf[doneBufInd], (doneSize - tmp - blCodeSize), tmp, numLsSigGrpEntriesInLwrrUcode , pLsSigGrpEntry));
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
                CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrCopyLsGrpSigToRegsForSec2_HAL(pAcr, (LwU32 *)(g_dmHashForGrp[i - 1]), i));
            }
        }
    }

Cleanup:
    return status;
}
#endif // IS_LASSAHS_REQUIRED 



#ifdef ACR_SIG_VERIF
/*
 * @brief Scrub the code and data content on error scenarios. Also write the WPR header back to FB.
 */
static
ACR_STATUS _acrScrubContentAndWriteWpr
(
    PLSF_LSB_HEADER_WRAPPER pWrapper,
    LwU32                   wprIndex
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 ucodeOffset;
    LwU32 ucodeSize;
    LwU32 dataSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pWrapper, (void *)&ucodeOffset));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                        pWrapper, (void *)&ucodeSize));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                        pWrapper, (void *)&dataSize));

    // Scrub Code
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrScrubRegionWithZeroes(ucodeOffset, ucodeSize));

    // Scrub Data
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrScrubRegionWithZeroes((ucodeOffset + ucodeSize), dataSize));

    // Write all WPR header back to FB and return...
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteAllWprHeaderWrappers_HAL(pAcr, wprIndex));

    return status;
}

/*!
 * @brief Fetches bin version from all WPR entries
 */
static ACR_STATUS
_acrGetAllBilwersions
(
    LwU32 *pFalconBilwersions,
    LwU32  wprIndex
)
{

    LwU32           index = 0;
    PLSF_WPR_HEADER_WRAPPER pWrapper;
    LwU32           falconId;
    LwU32           lsbOffset;
    LwU32           bilwersion;
    LwU32           numOfFalcons = LSF_FALCON_ID_END;
    ACR_STATUS      status;

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        // Assign invalid Bin version to all falcons first
        pFalconBilwersions[index] = LSF_FALCON_BIN_VERSION_ILWALID;
    }
 
    // We only have GSP-RM in WPR2  
    if (wprIndex == ACR_WPR2_REGION_IDX)
    {
        numOfFalcons = 1;
    }
 
    for (index=0; index < numOfFalcons; index++)
    {
        pWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            pWrapper, (void *)&lsbOffset));

        if (IS_FALCONID_ILWALID(falconId, lsbOffset))
        {
            break;
        }

        // Overiding bin version for falcons present in WPR header along with value
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_BIN_VERSION,
                                          pWrapper, (void *)&bilwersion));
        pFalconBilwersions[falconId] = bilwersion;
    }

    return ACR_OK;
}

/*!
 * @brief Function which checks for revocation info on LS falcon and corresponding dependencies
 */
ACR_STATUS
acrCheckForLSRevocationExt_GH100
(
    LwU32 falconId,
    PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper,
    LwU32 *pBilwersions
)
{
    LSF_UCODE_DESC_WRAPPER   lsfDescWrapper;
    LwU32                    bilwersion;
    ACR_STATUS               status;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                        pLsbHeaderWrapper, (void *)&lsfDescWrapper));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_BIN_VERION,
                                                                        &lsfDescWrapper, (void *)&bilwersion));

    // Make sure the version that we used for sig callwlation is what is present in WPRHEADER
    if (bilwersion != pBilwersions[falconId])
    {
        return ACR_ERROR_REVOCATION_CHECK_FAIL;
    }

    // Check if dependencies are passing
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_CHECK_DEPENDENCY,
                                                                         &lsfDescWrapper, (void *)pBilwersions));

    return ACR_OK;
}
#endif // ACR_SIG_VERIF

ACR_STATUS
acrValidateSignatureAndScrubUnusedWprExt_GH100
(
    LwU32 *failingEngine,
    LwU32  wprIndex
)
{
    ACR_STATUS status        = ACR_OK;
    LwU32      index         = 0;
    LwU32      wprRegionSize = 0;
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper = (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper;
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
    LwU64  numOfFalcons         = LSF_FALCON_ID_END;
    LwBool bHsOvlSigBlobPresent = LW_FALSE;
    LwU32  lwrScrubPosition     = 0;

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllWprHeaderWrappers_HAL(pAcr, wprIndex));

    //
    // Signature-verification is always enabled for RISC-V engines when
    // RISC-V LS support is enabled.
    //
#ifdef ACR_SIG_VERIF
    LwU32 falconBilwersions[LSF_FALCON_ID_END];
    LSF_UCODE_DESC_WRAPPER   lsfUcodeDescWrapper;
    status = _acrGetAllBilwersions(falconBilwersions, wprIndex);

    if (status != ACR_OK)
    {
        return status;
    }
#endif // ACR_SIG_VERIF

    if (wprIndex == ACR_WPR1_REGION_IDX)
    {
        acrScrubUnusedWprWithZeroes_HAL(pAcr, 0, LW_ALIGN_UP((LW_SIZEOF32(LSF_WPR_HEADER_WRAPPER) * LSF_FALCON_ID_END),
                                                              LSF_WPR_HEADER_ALIGNMENT), &lwrScrubPosition);
    }
    else
    {
        // We only have GSP-RM Blob in WPR2
        acrScrubUnusedWprWithZeroes_HAL(pAcr, 0, LW_ALIGN_UP((LW_SIZEOF32(LSF_WPR_HEADER_WRAPPER)),
                                                              LSF_WPR_HEADER_ALIGNMENT), &lwrScrubPosition);
        numOfFalcons = 0;     
    }
 
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index = 0; index <= numOfFalcons; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);

        // Check if this is the end of WPR header by checking falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
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

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&blDataOffset));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blDataSize));

            LwU64 lsfEndAddr = LW_ALIGN_UP((((LwU64)pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT)
                                             + blDataOffset + blDataSize), SUB_WPR_SIZE_ALIGNMENT);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                                pLsbHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

            if(bHsOvlSigBlobPresent == LW_TRUE)
            {

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                    pLsbHeaderWrapper, (void *)&hsOvlSigBlobOffset));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                    pLsbHeaderWrapper, (void *)&hsOvlSigBlobSize));

                lsfEndAddr = LW_ALIGN_UP(((pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT)
                                           + hsOvlSigBlobOffset + hsOvlSigBlobSize), SUB_WPR_SIZE_ALIGNMENT);
            }

            wprRegionSize = (LwU32)(lsfEndAddr - ((LwU64)pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT));

            // Scrub the empty WPR after last LS ucode but till Data subWPRs
            acrScrubUnusedWprWithZeroes_HAL(pAcr, wprRegionSize, 0, &lwrScrubPosition);
            break;
        }

        // Scrub the gap between last known scrub point and this LSB header
        acrScrubUnusedWprWithZeroes_HAL(pAcr, lsbOffset, LW_ALIGN_UP(LW_SIZEOF32(LSF_LSB_HEADER_WRAPPER),
                                                                                 LSF_LSB_HEADER_ALIGNMENT), &lwrScrubPosition);

        // Lets reset the status
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, pLsbHeaderWrapper));

        // Scrub the gap between last known scrub point and ucode/data
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                            pLsbHeaderWrapper, (void *)&ucodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&ucodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&dataSize));
        acrScrubUnusedWprWithZeroes_HAL(pAcr, ucodeOffset, ucodeSize, &lwrScrubPosition);
        acrScrubUnusedWprWithZeroes_HAL(pAcr, ucodeOffset + ucodeSize, dataSize, &lwrScrubPosition);

        // We don't expect bootloader data for falcons other than PMU and DPU.
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                            pLsbHeaderWrapper, (void *)&blDataOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&blDataSize));
        if ((status = acrSanityCheckBlData_HAL(pAcr, falconId, blDataOffset, blDataSize, lwrScrubPosition))!= ACR_OK)
        {
            return status;
        }

        acrScrubUnusedWprWithZeroes_HAL(pAcr, blDataOffset, blDataSize, &lwrScrubPosition);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                            pLsbHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

        if(bHsOvlSigBlobPresent == LW_TRUE)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&hsOvlSigBlobOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&hsOvlSigBlobSize));
            acrScrubUnusedWprWithZeroes_HAL(pAcr, hsOvlSigBlobOffset, hsOvlSigBlobSize, &lwrScrubPosition);
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
        LwU8      *pSignature = (LwU8 *)g_lsSignature;

#ifdef ACR_FMODEL_BUILD
        //
        // Skip LS signature verification on fmodel.
        // Reasons:
        // 1. FECS and GPCCS don't have signatures on fmodel. Bug 200179871.
        // 2. Sig verif takes long time on fmodel, so skipping for perf reasons.
        //
        continue;
#endif // ACR_FMODEL_BUILD

        // Get the lsf ucode descriptor
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER,
                                                                            pLsbHeaderWrapper, (void *)&lsfUcodeDescWrapper));

        // Get the code signature
        if (acrIsDebugModeEnabled_HAL(pAcr))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_CODE,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_CODE,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }

        // Verify code signature
        if ((status = acrValidateLsSignature_HAL(pAcr, falconId, pSignature, ucodeSize, ucodeOffset, &lsfUcodeDescWrapper, LW_TRUE)) != ACR_OK)
        {
            *failingEngine = falconId;
            goto VERIF_FAIL;
        }

        // Get the data signature
        if (acrIsDebugModeEnabled_HAL(pAcr))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_DATA,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfUcodeDescWrapperCtrl_HAL(pAcr, LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_DATA,
                                                                                &lsfUcodeDescWrapper, (void *)pSignature));
        }

        // Verify data signature
        if ((status = acrValidateLsSignature_HAL(pAcr, falconId, pSignature, dataSize, (ucodeSize + ucodeOffset), &lsfUcodeDescWrapper, LW_FALSE)) != ACR_OK)
        {
            *failingEngine = falconId;
            goto VERIF_FAIL;
        }
        status = acrCheckForLSRevocationExt_HAL(pAcr, falconId, pLsbHeaderWrapper, falconBilwersions);

        if (status != ACR_OK)
        {
            *failingEngine = falconId;
            goto VERIF_FAIL;
        }

VERIF_FAIL:

        if (status != ACR_OK)
        {
            //
            // Scrub code and data if sig verif has failed.
            // Not checking status (return value) of scrubbing here since Sig Verif failure is the actual
            // error which needs to be returned.
            //

            (void)_acrScrubContentAndWriteWpr(pLsbHeaderWrapper, wprIndex);

            return status;
        }
#endif // ACR_SIG_VERIF
    }

    // Compare the size now to make sure we accounted for all of WPR region.Skip for WPR2
    if ((wprIndex ==  ACR_WPR1_REGION_IDX) && (lwrScrubPosition != wprRegionSize))
    {
        return ACR_ERROR_SCRUB_SIZE;
    }

    return status;
}
