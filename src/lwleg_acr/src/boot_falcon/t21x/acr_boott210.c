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
 * @file: acr_boott210.c
 */

//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_bus.h"
#include "dev_pwr_csb.h"
#include "dev_falcon_v4.h"

#include "mmu/mmucmn.h"

#include "dev_pwr_falcon_csb.h"
#include "dev_master.h"

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "g_acr_private.h"
#include "g_acrlib_private.h"

// Static Functions
static ACR_STATUS acrPrivLoadTargetFalcon_GM200 (LwU32 dstOff, LwU32 fbOff, LwU32 sizeInBytes, LwU32 regionID, LwBool bIsDstImem, PACR_FLCN_CONFIG pFlcnCfg) GCC_ATTRIB_SECTION("imem_acr", "acrPrivLoadTargetFalcon_GM200");
static void isBootstrapOwnPresent(LwBool *bootstrapOwnPresent) GCC_ATTRIB_SECTION("imem_acr", "isBootstrapOwnPresent");
#if defined(ACR_SIG_VERIF)
static ACR_STATUS acrScrubContentAndWriteWpr(PLSF_LSB_HEADER pLsfHeader) GCC_ATTRIB_SECTION("imem_acr", "acrScrubgContentAndWriteWpr");
static void acrGetAllBilwersions(LwU32 *pFalconBilwersions) GCC_ATTRIB_SECTION("imem_acr", "acrGetAllBilwersions");
static inline LwU8 * acrGetUcodeSignature(PLSF_LSB_HEADER pLsfHeader, LwU32 sectionIndex) GCC_ATTRIB_ALWAYSINLINE();
#endif

#ifdef ACR_SAFE_BUILD
static ACR_STATUS verifyLoadedFalconUcode(LwU32 dstOff, LwU32 fbOff, LwU32 sizeInBytes, LwBool bIsDstImem, PACR_FLCN_CONFIG pFlcnCfg) GCC_ATTRIB_SECTION("imem_acr", "verifyLoadedFalconUcode");
#endif

// Global Variables

/*! g_bSetReqCtx Global variable to store if REQUIRE_CTX needs to be set or not for falcons. */
LwBool            g_bSetReqCtx         ATTR_OVLY(".data") = LW_FALSE;
/*! g_bIsDebug Global variable to store the STATUS of the Debug Signal going to the SCP for the board on which current binary is exelwting */
LwBool            g_bIsDebug           ATTR_OVLY(".data") = LW_FALSE;
/*! g_lsbHeader Global variable to store the LSB header read from WPR */
LSF_LSB_HEADER    g_lsbHeader          ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
/*! g_dmaProp Global variable to store DMA properties for DMAing to/from SYSMEM & falcon's IMEM/DMEM */
ACR_DMA_PROP      g_dmaProp            ATTR_OVLY(".data") ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);
/*! xmemStore Global variable to store ucode contents data/code */
LwU32             xmemStore[FLCN_IMEM_BLK_SIZE_IN_BYTES/sizeof(LwU32)] ATTR_OVLY(".data") ATTR_ALIGNED(FLCN_IMEM_BLK_SIZE_IN_BYTES);

/*! g_blCmdLineArgs Global variable to store bootloader arguments */
ACR_FLCN_BL_CMDLINE_ARGS  g_blCmdLineArgs       ATTR_OVLY(".data") ATTR_ALIGNED(ACR_FLCN_BL_CMDLINE_ALIGN);
/*! GET_WPR_HEADER Macro to get desired WPR header using index(ind) as argument from global buffer "g_pWprHeader" containing all WPR headers. */
#define GET_WPR_HEADER(ind)          ((g_pWprHeader) + (ind))
/*! CHECK_FALCON_ID_RANGE_AND_ERROR to check whether falcon ID has an entry within the range of falcon IDs */
#define CHECK_FALCON_ID_RANGE_AND_ERROR(id) do                                  \
    {                                                                           \
        if(((id) >= (LSF_FALCON_ID_END)) && ((id) != (LSF_FALCON_ID_ILWALID)))   \
        {                                                                       \
            return ACR_ERROR_FLCN_ID_NOT_FOUND;                                 \
        }                                                                       \
    }                                                                           \
    while(LW_FALSE)

#ifndef ACR_MMU_INDEXED
/*! LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT Macro defining the WPR and VPR RegionCfg addr alignment. */
#define LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT 0x0000000lw
#endif

/*!
 * @brief Polls for Falcon reset to complete.
 *        Make sure not to reset bootstrapping owner falcon i.e. PMU falcon here.\n
 *        Call acrlibPollForScrubbing_HAL which polls for scrubbing to complete.\n
 *
 * @param[in] validIndexMap  Mask of all valid falcon IDs
 *
 * @return ACR_OK If reset completes successfully.
 * @return ACR_ERR_TIMEOUT If Poll for scrubbing timeouts.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 *
 */
ACR_STATUS
acrPollForResetCompletion_GM200
( 
    LwU32        validIndexMap
)
{
    ACR_STATUS      status       = ACR_OK;
    LwU32           index        = 0;
    PLSF_WPR_HEADER pWprHeader   = LW_NULL;
    ACR_FLCN_CONFIG flcnCfg;

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }

        pWprHeader = GET_WPR_HEADER(index);

        // Dont reset if falcon id is bootstrap owner
#ifndef ACR_SAFE_BUILD
        if (acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->falconId))
        {
            continue;
        }
#endif

        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pAcrlib, pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) 
                                     == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        //
        // Poll for scrubbing to complete
        // TODO: Do polling for all falcons at once. Saves perf.
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPollForScrubbing_HAL(pAcrlib, &flcnCfg)); 
    }

    return status;
}

/*!
 * @brief Resets engine and falcon if applicable. For some engines,
 *        only falcon can be reset and not the entire unit. Also waits
 *        for scrubbing to complete. Donot reset if falcon id is bootstrap owner.
 *
 * @param[in] validIndexMap List of valid falcon IDs
 * @return  ACR_OK If falcon reset engine completes successfully.
 * @return  ACR_ERR_TIMEOUT If falcon reset engine timesout due to scrubbing not complete.
 * @return  ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 *
 */
ACR_STATUS
acrResetEngineFalcon_GM200
(
    LwU32        validIndexMap
)
{
    ACR_STATUS       status          = ACR_OK;
    LwU32            index           = 0;
    PLSF_WPR_HEADER  pWprHeader      = LW_NULL;
    ACR_FLCN_CONFIG  flcnCfg;

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }
        pWprHeader = GET_WPR_HEADER(index);

        // Dont reset if falcon id is bootstrap owner
#ifndef ACR_SAFE_BUILD
        if (acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->falconId))
        {
            continue;
        }
#endif

        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pAcrlib, pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) 
                                          == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibResetFalcon_HAL(pAcrlib, &flcnCfg, LW_FALSE));
    }

    return status;
}

/*!
 * @brief Do sanity check on the bootloader arguments provided.
 *        -# Check and return error if blDataSize is greater than expected size.
 *        -# Read loader config into g_blCmdLineArgs from WPR by issuing DMA.
 *        -# Write KNOWN HALT ASM code into RESERVED location for FECS/GPCCS/PMU falcon. Return error if falcon id is not FECS/GPCCS/PMU falcon id.
 *        -# Write back modified loader config to WPR by issuing DMA.
 * @param[in] falconId Falcon id for bootloader data sanity needs to be checked.
 * @param[in] blDataOffset Bootloader data offset in WPR region.
 * @param[in] blDataSize   Bootloader size.
 *
 * @return ACR_ERROR_DMA_FAILURE If DMA failure in seen in reading/writing loader config.
 * @return ACR_ERROR_BLDATA_SANITY If sanity for BL data fails.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If falcon Id does not belong to PMU, FECS or GPCCS.
 * @return ACR_OK                  if BL data sanity passes.
 */
ACR_STATUS
acrSanityCheckBlData_GM200
(
    LwU32              falconId,
    LwU32              blDataOffset,
    LwU32              blDataSize
)
{
    //
    // We dont expect bootloader data for falcons other than PMU and
    // GR falcons
    //
    if ( (blDataOffset >= g_scrubState.scrubTrack) && (blDataSize > 0U))
    {
        // Bootloader data size is not expected to be more than 256 bytes
        if (blDataSize > LSF_LS_BLDATA_EXPECTED_SIZE)
        {
            return ACR_ERROR_BLDATA_SANITY;
        }

        // Read loader config
        if((acrIssueDma_HAL(pAcr, (LwU32)&g_blCmdLineArgs, LW_FALSE,  blDataOffset, 
            blDataSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp))
            != blDataSize)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        // Write KNOWN HALT ASM code into RESERVED location..

	if (falconId == LSF_FALCON_ID_PMU ||
 		falconId == LSF_FALCON_ID_FECS ||
 		falconId == LSF_FALCON_ID_GPCCS)
 	{
 		g_blCmdLineArgs.genericBlArgs.reserved[0] = ACR_FLCN_BL_RESERVED_ASM;
 	}
 	else
 	{
 		return ACR_ERROR_FLCN_ID_NOT_FOUND;
 	}

        // Write back loader config
        if((acrIssueDma_HAL(pAcr, (LwU32)&g_blCmdLineArgs, LW_FALSE,  blDataOffset, 
            blDataSize, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp))
            != blDataSize)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

    }

    return ACR_OK;
}

/*!
 * @brief Reads LSB header into a global buffer which is then copied to local buffer pointed out by pLsbHeader.
 *        Global buffer is used to match the required alignment as enforced by falcon DMA engine. This is achieved by calling IssueDma_HAL().
 *
 * @param[in]  pWprHeader WPR header for a particular falcon ID of interest
 * @param[out] pLsbHeader Pointer to global buffer holding LSB header contents.
 *
 * @return ACR_OK Reading of LSB header is successful from WPR.
 * @return ACR_ERROR_DMA_FAILURE Reading of LSB header fails due to DMA failure.
 *
 */
ACR_STATUS
acrReadLsbHeader_GM200
(
    PLSF_WPR_HEADER  pWprHeader,
    PLSF_LSB_HEADER  pLsbHeader
)
{
    LwU32            lsbSize = LW_ALIGN_UP(sizeof(LSF_LSB_HEADER), 
                               LSF_LSB_HEADER_ALIGNMENT);

    // Read the LSB  header
    if ((acrIssueDma_HAL(pAcr, (LwU32)&g_lsbHeader, LW_FALSE, pWprHeader->lsbOffset, lsbSize, 
             ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != lsbSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }
 
    acrMemcpy(pLsbHeader, &g_lsbHeader, sizeof(LSF_LSB_HEADER));
    return ACR_OK;
}

/*
 * @brief Scrub the code and data content on error scenarios. Also write the WPR header back to WPR.
 *        g_scrubState.zeroBuf global zero buffer is used to scrub code & data of falcon. Scrubbing is achieved by DMAing zero buffer to WPR region.
 *
 * @param[in] pLsfHeader LSB header.
 *
 * @return ACR_OK If scrubbing of content i.e. code/data and writing back of WPR headers is successful.
 * @return ACR_ERROR_DMA_FAILURE If scrubbing of data/code fails due to DMA failure. 
 */
static
ACR_STATUS acrScrubContentAndWriteWpr(PLSF_LSB_HEADER pLsfHeader)
{
    ACR_STATUS status = ACR_OK;

    // Scrub code
    if ((acrIssueDma_HAL(pAcr, (LwU32)g_scrubState.zeroBuf, LW_FALSE, pLsfHeader->ucodeOffset, pLsfHeader->ucodeSize,
       ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != pLsfHeader->ucodeSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Scrub data
    if ((acrIssueDma_HAL(pAcr, (LwU32)g_scrubState.zeroBuf, LW_FALSE, pLsfHeader->ucodeOffset + pLsfHeader->ucodeSize,
        pLsfHeader->dataSize, ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != pLsfHeader->dataSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    } 

    // Write WPR header back to FB and return...
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteWprHeader_HAL(pAcr));
    return status;
}

/*!
 * @brief Fetches bin version from all WPR entries
 *        ALL WPR headers enteries are fetched and bilwersions corresponsing to each valid falcon ID is stored in the pFalconBilwersions.
 *
 * @param[out] pFalconBilwersions Ptr to ouput array having bin versions for each falconid.
 */
static
void acrGetAllBilwersions(LwU32 *pFalconBilwersions)
{

    LwU32           index = 0;
    PLSF_WPR_HEADER pWprHeader;

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }

        pFalconBilwersions[pWprHeader->falconId] = pWprHeader->bilwersion;

    }
}

/*
 * @brief Function which will look if Bootstrap owner ucode is present in WPR.
 *        Loop through each WPR header for all valid falcon ids and break the loop when falcon id matches with LWRRENT_BOOTSTRAP_OWNER_ID
 *
 * @param[out] Pointer to if bootstrap owner present bool flag  
 */
static void isBootstrapOwnPresent(LwBool *bootstrapOwnPresent)
{
    LwU32 index;
    PLSF_WPR_HEADER         pWprHeader;

    *bootstrapOwnPresent = LW_FALSE;
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index=0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }
#ifndef ACR_SAFE_BUILD
        if (acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->falconId) == LW_TRUE)
        {
            *bootstrapOwnPresent = LW_TRUE;
            break;
        }
#endif
    }
    return;
}

#ifdef ACR_SAFE_BUILD
static ACR_STATUS
verifyLoadedFalconUcode
(
    LwU32            dstOff,
    LwU32            fbOff,
    LwU32            sizeInBytes,
    LwBool           bIsDstImem,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       bytesXfered   = 0;
    LwU32       tag = 0;
    LwU32       i = 0;
    LwU32       data;
    LwU32       aincr;

    aincr  = 0x2000000; //(1U << 25U) ccomplains for MISRA 12.2 violation. Basically this is to configure DMEM/IMEM in auto read increment mode.

    // Setup the MEMC Register
    if (bIsDstImem)
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0U),
					REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8U) | aincr);
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0U),
					REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8U) | aincr);
    }

    // Start the DMA, read ucode and compare them.
    while (bytesXfered < sizeInBytes)
    {
        // Read the next block
        if ((acrIssueDma_HAL(pAcr, (LwU32)xmemStore, LW_FALSE, (fbOff + bytesXfered), 
                             FLCN_IMEM_BLK_SIZE_IN_BYTES, ACR_DMA_FROM_FB, 
                             ACR_DMA_SYNC_AT_END, &g_dmaProp)) != FLCN_IMEM_BLK_SIZE_IN_BYTES)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        if (bIsDstImem)
        {
            // Setup the tags for the instruction memory.
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0U), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
            {
                data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0U));
                if (data != xmemStore[i])
                {
                    return ACR_ERROR_UNKNOWN;
                }
            }
            tag++;
            CHECK_WRAP_AND_ERROR(tag == LW_U16_MAX + 1U);
        }
        else
        {
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
            {
                data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0U));
                if (data != xmemStore[i])
                {
                    return ACR_ERROR_UNKNOWN;
                }
            }
        }
        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;

    }
    return status;
}
#endif
/*!
 * @brief Setup IMEMC/DMEMC to configure IMEM/DMEM for writing based on whether transfer request target is IMEM or DMEM.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM 
 * @param[in] pFlcnCfg     Falcon config
 * @param[in] bIsDstImem TRUE if destination is IMEM
 *
 */
static void
acrSetupMemConfigRegisters
(
    PACR_FLCN_CONFIG      pFlcnCfg,
    LwU32                 dstOff,
    LwBool                bIsDstImem
)
{
    if (bIsDstImem)
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0U),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0U) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) | 
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1U));
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0U),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0U) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1U));
    }
}

/*!
 * @brief Use priv access/writes to load target falcon memory.
 *        -# Support loading into both IMEM and DMEM of target falcon from WPR.
 *        -# Expect size to be 256 multiple.
 *        -# Make sure that size of ucode to be transferred is not greater than target falcon's IMEM/DMEM size.
 *        -# Setup IMEM/DMEM for writing by calling acrSetupMemConfigRegisters.
 *        -# Loop till (bytesXfered < sizeInBytes) and DMA one block from SYSMEM(WPR) to xmemStore global storage.
 *        -# xmemStore is then used to write to IMEM/DMEM by using IMEMD/DMEMD register.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbOff        Offset from wprBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] pFlcnCfg     Falcon config
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_TGT_DMA_FAILURE  If dstOff, fbOff and sizeInBytes is not aligned to FLCN_IMEM_BLK_SIZE_IN_BYTES.
 * @return ACR_ERROR_DMA_FAILURE  If DMA failure is seen.
 * @return ACR_ERROR_UNEXPECTED_ARGS if size of ucode to be loaded on target falcon exceeds its IMEM/DMEM size.
 * @return ACR_OK  If acrPrivLoadTargetFalcon_GM200 successfully exelwtes.
 */
static ACR_STATUS
acrPrivLoadTargetFalcon_GM200
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwBool              bIsDstImem,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       bytesXfered   = 0;
    LwU32       tag;
    LwU32       i;
    LwBool      bIsSizeOk     = LW_FALSE; 

    //
    // Sanity Checks
    // Only does 256B Transfer
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    bIsSizeOk = acrlibCheckIfUcodeFitsFalcon_HAL(pAcrlib, pFlcnCfg, sizeInBytes, bIsDstImem);
    if (bIsSizeOk == LW_FALSE)
    {
        return ACR_ERROR_UNEXPECTED_ARGS;
    }

    // Setup the MEMC Register
    acrSetupMemConfigRegisters(pFlcnCfg, dstOff, bIsDstImem);

    // Start the DMA and bitbang the ucode.
    tag = 0;
    while (bytesXfered < sizeInBytes)
    {
        // Read the next block
        if ((acrIssueDma_HAL(pAcr, (LwU32)xmemStore, LW_FALSE, (fbOff + bytesXfered), 
                             FLCN_IMEM_BLK_SIZE_IN_BYTES, ACR_DMA_FROM_FB, 
                             ACR_DMA_SYNC_AT_END, &g_dmaProp)) != FLCN_IMEM_BLK_SIZE_IN_BYTES)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        if (bIsDstImem)
        {
            // Setup the tags for the instruction memory.
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0U), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
            {
                acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0U), xmemStore[i]);
            }
            tag++;
            CHECK_WRAP_AND_ERROR(tag == LW_U16_MAX + 1U);
        }
        else
        {
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32); i++)
            {
                acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0U), xmemStore[i]);
            }
        }
        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;

    }

    return status;
}

/*!
 * @brief Setup Falcon in LS mode. This ilwolves
 *        -# Check if the code validation completed before going forward.
 *        -# Setup LS Falcon registers by calling acrSetupLSFalcon_HAL.
 *        -# Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary.
 *        -# Check if code needs to be loaded at start of IMEM or at end of IMEM using LSF flag.
 *        -# If the code needs to be loaded at end of IMEM, find farthest IMEM block from where loading can start using acrlibFindFarthestImemBl_HAL.
 *        -# BASED on LSF header's "FORCE_PRIV_LOAD" flag, load LS falcon's IMEM/DMEM with falcon code/data by PRIV writes i.e. acrPrivLoadTargetFalcon_GM200 or by DMA i.e. acrlibIssueTargetFalconDma_HAL.
 *        -# Read back code/data written to LS falcon's IMEM/DMEM and compare it with WPR contents.
 *        -# Setup/Program BOOTVEC for falcon.
 *        -# Check if Falcon wants virtual ctx based on LSF flag and set accordingly.
 *        -# Update WPR header status to "LSF_IMAGE_STATUS_BOOTSTRAP_READY" and return. 
 *
 * @param[in] pLsbHeaders  LSB headers
 * @param[in] validIndexMap List of valid falcon IDs
 *
 * @return ACR_ERROR_DMA_FAILURE In case there is DMA failure.
 * @return ACR_OK If acrSetupLSFalcon_GM200 completes successfully.  
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 */
ACR_STATUS
acrSetupLSFalcon_GM200
(
    PLSF_LSB_HEADER    pLsbHeaders,
    LwU32              validIndexMap
)
{
    ACR_STATUS       status     = ACR_OK;
    PLSF_LSB_HEADER  pLsbHeader = LW_NULL;
    LwU32            index      = 0;
    PLSF_WPR_HEADER  pWprHeader;
    ACR_FLCN_CONFIG  flcnCfg;
    LwU64            blWprBase;
    LwU64            blWprOffset;
    LwU32            farthestImemBl;
    LwU32            dst;
    LwU32            targetMaskPlmOldValue = 0;
    LwU32            targetMaskOldValue    = 0;
#ifndef ACR_SAFE_BUILD
    ACR_DMA_PROP     dmaProp;
#endif

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }

        pWprHeader = GET_WPR_HEADER(index);
        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pAcrlib, pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) 
                                       == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        pLsbHeader = &(pLsbHeaders[index]);

        // Check if the code validation completed before going forward
        if ((pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_DONE) &&
            (pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_SKIPPED) &&
            (pWprHeader->status != LSF_IMAGE_STATUS_BOOTSTRAP_READY))
        {
            continue;
        }

        //
        // Lock falcon reg space.
        if (pWprHeader->falconId != LSF_FALCON_ID_PMU)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcr, LSF_FALCON_ID_PMU, &flcnCfg,
                                  &targetMaskPlmOldValue, &targetMaskOldValue, LW_TRUE));
        }

        CHECK_WRAP_AND_ERROR(pLsbHeader->blCodeSize > (LW_U32_MAX - FLCN_IMEM_BLK_SIZE_IN_BYTES));
        pLsbHeader->blCodeSize = LW_ALIGN_UP(pLsbHeader->blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetRegisters_HAL(pAcrlib, &flcnCfg));

        // Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, flcnCfg.imemPLM);
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, flcnCfg.dmemPLM);

        //
        // Load code into IMEM
        // BL starts at offset, but since the IMEM VA is not zero, we need to make
        // sure FBOFF is equal to the expected IMEM VA. So adjusting the FBBASE to make
        // sure FBOFF equals to VA as expected.
        //

        blWprOffset = (LwU64)pLsbHeader->ucodeOffset - (LwU64)pLsbHeader->blImemOffset;

        blWprBase = ((g_dmaProp.wprBase) + ((LwU64)blWprOffset >> FLCN_IMEM_BLK_ALIGN_BITS));
        CHECK_WRAP_AND_ERROR(blWprBase < g_dmaProp.wprBase);

#ifndef ACR_SAFE_BUILD
        // Check if code needs to be loaded at start of IMEM or at end of IMEM
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, pLsbHeader->flags))
        {
            dst = 0;
        }
        else
#endif
        {
            farthestImemBl = acrlibFindFarthestImemBl_HAL(pAcrlib, &flcnCfg, pLsbHeader->blCodeSize);
            CHECK_WRAP_AND_ERROR(farthestImemBl > LW_U32_MAX/FLCN_IMEM_BLK_SIZE_IN_BYTES);
            dst = (farthestImemBl * FLCN_IMEM_BLK_SIZE_IN_BYTES);
        }

#ifndef ACR_ONLY_BO
        if(!acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->falconId))
        {
            if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE, pLsbHeader->flags))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_GM200(0,
                    pLsbHeader->ucodeOffset + pLsbHeader->appCodeOffset, pLsbHeader->appCodeSize,
                    g_dmaProp.regionID, LW_TRUE, &flcnCfg))
#ifdef ACR_SAFE_BUILD
                // Read back code and compare with WPR contents
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(verifyLoadedFalconUcode(0, pLsbHeader->ucodeOffset + pLsbHeader->appCodeOffset, pLsbHeader->appCodeSize, LW_TRUE, &flcnCfg))
#endif
                // Load data into DMEM
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_GM200(0,
                      pLsbHeader->ucodeOffset + pLsbHeader->appDataOffset, pLsbHeader->appDataSize,
                      g_dmaProp.regionID, LW_FALSE, &flcnCfg))
#ifdef ACR_SAFE_BUILD
                // Read back data and compare with WPR contents
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(verifyLoadedFalconUcode(0, pLsbHeader->ucodeOffset + pLsbHeader->appDataOffset, pLsbHeader->appDataSize, LW_FALSE, &flcnCfg))
#endif
                // Set the BOOTVEC
                acrlibSetupBootvec_HAL(pAcrlib, &flcnCfg, 0);
            }
            else
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                      dst, blWprBase, pLsbHeader->blImemOffset, 
                      pLsbHeader->blCodeSize, g_dmaProp.regionID, LW_TRUE, LW_TRUE, &flcnCfg))
#ifdef ACR_SAFE_BUILD
                // Read back code and compare with WPR contents
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(verifyLoadedFalconUcode(dst, pLsbHeader->ucodeOffset, pLsbHeader->blCodeSize, LW_TRUE, &flcnCfg))
#endif
                // Load data into DMEM
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                    0, g_dmaProp.wprBase, pLsbHeader->blDataOffset, pLsbHeader->blDataSize,
                    g_dmaProp.regionID, LW_TRUE, LW_FALSE, &flcnCfg))
#ifdef ACR_SAFE_BUILD
                // Read back data and compare with WPR contents
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(verifyLoadedFalconUcode(0, pLsbHeader->blDataOffset, pLsbHeader->blDataSize, LW_FALSE, &flcnCfg))
#endif
                // Set the BOOTVEC
                acrlibSetupBootvec_HAL(pAcrlib, &flcnCfg, pLsbHeader->blImemOffset);
            }
        }
        else
#endif
        {
#ifndef ACR_SAFE_BUILD
            dmaProp.wprBase  = blWprBase;
            dmaProp.ctxDma   = g_dmaProp.ctxDma;
            dmaProp.regionID = g_dmaProp.regionID;
            if((acrIssueDma_HAL(pAcr, (LwU32)dst, LW_TRUE,  pLsbHeader->blImemOffset, 
                pLsbHeader->blCodeSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaProp))
                != pLsbHeader->blCodeSize)
            {
                return ACR_ERROR_DMA_FAILURE;
            }

            dmaProp.wprBase  = g_dmaProp.wprBase;
            if((acrIssueDma_HAL(pAcr, 0U, LW_FALSE,  pLsbHeader->blDataOffset, 
                pLsbHeader->blDataSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaProp))
                != pLsbHeader->blDataSize)
            {
                return ACR_ERROR_DMA_FAILURE;
            }
            // Set the BOOTVEC
            acrlibSetupBootvec_HAL(pAcrlib, &flcnCfg, pLsbHeader->blImemOffset);
#endif
       }

        // Check if Falcon wants virtual ctx
#ifndef ACR_SAFE_BUILD
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, pLsbHeader->flags))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_FALSE));
        }
        else
#endif
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_TRUE));
        }

        g_bSetReqCtx = LW_FALSE;
#ifndef ACR_SAFE_BUILD
        // Check if Falcon wants REQUIRE_CTX
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, pLsbHeader->flags))
        {
            g_bSetReqCtx = LW_TRUE;
        }
#endif
        //
        // For the falcon which is lwrrently exelwting this, set REQUIRE_CTX only after
        // all DMAs are completed
        //
        if (!acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->falconId))
        {
            acrlibSetupDmaCtl_HAL(pAcrlib, &flcnCfg, g_bSetReqCtx);
        }

        // Set the status in WPR header
        pWprHeader->status = LSF_IMAGE_STATUS_BOOTSTRAP_READY;

        //
        // Free falcon reg space
        //
        if (pWprHeader->falconId != LSF_FALCON_ID_PMU)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcr, LSF_FALCON_ID_PMU, &flcnCfg,
                                      &targetMaskPlmOldValue, &targetMaskOldValue, LW_FALSE));
        }
    }

    return status;
}

#ifndef ACR_SAFE_BUILD
/*
 * @brief Function which will take care of bootstrapping the falcons
 *        after once WPR is validated
 *
 * @param[in] none  
 * @param[out] status of bootstrap  
 */
ACR_STATUS acrBootstrapValidatedUcode(void)
{
    LwU32 index;
    PLSF_WPR_HEADER         pWprHeader;
    LwU32 validWprMask = 0;
    ACR_STATUS status = ACR_OK;
    LSF_LSB_HEADER          lsbHeaders[LSF_FALCON_ID_END + 1];
    PLSF_LSB_HEADER         pLsfHeader;
    ACR_FLCN_CONFIG         boOwnFlcnCfg;

    LwBool bootstrapOwnPresent = LW_FALSE;
    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL(pAcr));
    isBootstrapOwnPresent(&bootstrapOwnPresent);
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index=0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }
// Check for bLazyBootstrap
        if ((pWprHeader->bLazyBootstrap != 0U) && (bootstrapOwnPresent == LW_TRUE))
        {
            continue;
        }

        if (((pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_DONE) ||
            (pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_SKIPPED) ||
            (pWprHeader->status == LSF_IMAGE_STATUS_BOOTSTRAP_READY)))
        {
            validWprMask |= BIT32(index);
        }

        // Lets reset the status
        pLsfHeader = &(lsbHeaders[index]);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_HAL(pAcr, pWprHeader, pLsfHeader));
    }
    // Reset the engine and falcon
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrResetEngineFalcon_HAL(pAcr, validWprMask));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPollForResetCompletion_HAL(pAcr, validWprMask));

    //
    // Setup the LS falcon
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSFalcon_HAL(pAcr, lsbHeaders, validWprMask));

    // Write WPR header back to FB to communicate the status
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteWprHeader_HAL(pAcr));

    // Set the DMACTL require ctx for bootstrap owner if needed
    if (acrlibGetFalconConfig_HAL(pAcrlib, ACR_LSF_LWRRENT_BOOTSTRAP_OWNER, LSF_FALCON_INSTANCE_DEFAULT_0, &boOwnFlcnCfg) == ACR_OK)
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, &boOwnFlcnCfg, g_bSetReqCtx);
    }

    return status;
}
#endif

#ifdef ACR_SIG_VERIF
/*!
 * @brief Fetch ucode signatures based on Index as well as board type i.e DEBUG/PROD.
 *
 * @param[in]  pLsfHeader   Pointer to LSF header for falcon.
 * @param[in]  sectionIndex Value indicating which data section the signatures are to be fetched for.
 *
 * @return pSignature Pointer to the corresponding ucode signature.
 */
static inline LwU8 *
acrGetUcodeSignature
(
    PLSF_LSB_HEADER   pLsfHeader,
    LwU32             sectionIndex
)
{
    LwU8   *pSignature;

    if (g_bIsDebug) {
        pSignature = pLsfHeader->signature.dbgKeys[sectionIndex];
    } else {
        pSignature = pLsfHeader->signature.prdKeys[sectionIndex];
    }

    return pSignature;
}

/*!
 * @brief Function which will take care of validating/authenticating the LSF ucodes.
 *        -# Mark LSF IMAGE status to IMAGE_COPY. It means LSF IMAGE for a particular falcon ID as specified by loop is present in WPR(i.e. successfully copied).
 *        -# pLsfHeader->signature LSF header contains signatures for both debug/productions type of boards.
 *        -# Fetch ucode signatures by calling acrGetUcodeSignatures.
 *        -# Validate/Authenticate data/code part of LS image by calling acrVerifySignature_HAL. 
 *        -# Check for LS ucode revocation by using falconBilwersions by calling acrCheckForRevocation_HAL.
 *        -# In case of LS ucode signature validation/Revocation failure, scrub WPR and return with error to calling function.  
 *        -# Mark status in LSF image header as LSF_IMAGE_STATUS_VALIDATION_DONE if validation of signature and revocation check passes.
 * 
 * @param[in] pWprHeader WPR header for a particular falcon ID of interest.
 * @param[in] pLsfHeader Pointer to LSF header for falcon.
 * 
 * @return ACR_OK                 If functions compeletes without any failures and loading of LS falcons is complete.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If WPR Header does not belong to any PMU or GR falcons.
 */
static ACR_STATUS
acrAuthenticateLSFUcodes
(
    PLSF_WPR_HEADER         pWprHeader,
    PLSF_LSB_HEADER         pLsfHeader
)
{
    LwU32                   falconBilwersions[LSF_FALCON_ID_END];
    acrGetAllBilwersions(falconBilwersions);

    ACR_STATUS   status        = ACR_OK;
    ACR_STATUS   oldstatus     = ACR_OK;
    pWprHeader->status = LSF_IMAGE_STATUS_COPY;

    if ((pWprHeader->falconId == LSF_FALCON_ID_PMU) || (pWprHeader->falconId == LSF_FALCON_ID_FECS) ||
        (pWprHeader->falconId == LSF_FALCON_ID_GPCCS))
    {
        LwU32  falconId = pWprHeader->falconId;
        LwU8   *pSignature;


        // Get the ucode signature
        pSignature = acrGetUcodeSignature(pLsfHeader, ACR_LS_VERIF_SIGNATURE_CODE_INDEX);
        
        //
        // Verify code signature
        //
        if ((status = acrVerifySignature_HAL(pAcr, pSignature, falconId, pLsfHeader->ucodeSize, 
                                        pLsfHeader->ucodeOffset, pLsfHeader, LW_TRUE)) != ACR_OK)
        {
            // Ucode verif failed
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED;
            goto VERIF_FAIL;

        }

        // Get the data signature
        pSignature = acrGetUcodeSignature(pLsfHeader, ACR_LS_VERIF_SIGNATURE_DATA_INDEX);
        //
        // Verify data signature
        //
        if ((status = acrVerifySignature_HAL(pAcr, pSignature, falconId, pLsfHeader->dataSize, 
                                        (pLsfHeader->ucodeSize + pLsfHeader->ucodeOffset), pLsfHeader, LW_FALSE)) != ACR_OK) //UcodeSize will be the dataOffset
        {
            // Ucode verif failed
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED;
            goto VERIF_FAIL;
        }

VERIF_FAIL:
        if ((pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED) || 
            (pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED))
        {
#ifdef ACR_FMODEL_BUILD
            if ((falconId == LSF_FALCON_ID_FECS) || (falconId == LSF_FALCON_ID_GPCCS))
            {
                //
                // TODO: Resolve this
                // Some falcons such as FECS and GPCCS doesnt have signatures for binaries in debug elwiroment (fmodel)
                // Filed HW bug 200179871
                //
                pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_SKIPPED;
                status = ACR_OK;
            }
            else
            {
#endif
                oldstatus = status;
                status    = acrScrubContentAndWriteWpr(pLsfHeader);

                if (oldstatus != ACR_OK) {
                    return oldstatus;
                } else {
                    return status;
                }
#ifdef ACR_FMODEL_BUILD
            }
#endif
        }
        else
        {
            // Both ucode and data validation passed, set the status now
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_DONE;

        }
    }
    else
    {
        pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_SKIPPED;
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return status;
}
#endif

/*!
 * @brief Function which will take care of bootstrapping the falcons.
 *        This function will evolve overtime.
 *        -# Read WPR headers into local buffer.
 *        -# Get all bin versions of ucodes present in WPR. 
 *        -# Setup scrubbing tracker so as to scrub un-used/empty regions in between different data structures present in WPR.
 *        -# Loop through all WPR headers and break the loop once invalid falcon Id is found.
 *        -# In case of safety build, return error if lazy bootstrap is true for any LS falcons.
 *        -# Check the status of the image through WPR header. If image is aleady validated or bootstrapped, continue.
 *        -# Read LSB header.
 *        -# Do Sanity check on the bootloader data of the ucode. We expect bootloader data for FECS/GPCCS falcons.
 *        -# Call the function acrAuthenticateLSFUcodes which will take care of validating/authenticating the LSF ucodes.
 *        -# Use bLazyBootstrap flag to decide if the falcon needs be to be loaded by ACR or not.
 *        -# Get bin versions of all ucodes present in WPR blob. 
 *        -# Reset engine and falcon and poll for its completion.
 *        -# Setup LS falcon by calling acrSetupLSFalcon_HAL.
 *        -# Update WPR header by writing the local copy of WPR header buffer by calling function acrWriteWprHeader_HAL.
 *        -# Set the DMACTL require ctx for bootstrap owner if needed. 
 *        -# global g_desc.
 *
 * @return ACR_ERROR_REGION_PROP_ENTRY_NOT_FOUND  If region prop entry is not found.
 * @return ACR_ERROR_SCRUB_SIZE   If there is some failure seen in scrubbing unused WPR region.
 * @return ACR_OK                 If functions compeletes without any failures and loading of LS falcons is complete.
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If falcon ID does not have entry within the end of WPR header.
 */
ACR_STATUS
acrBootstrapFalcons_GM200(void)
{
    ACR_STATUS              status        = ACR_OK;
    LwU32                   index         = 0;
    LwU32                   validWprMask  = 0;
    LwU32                   wprRegionSize = 0;
    LSF_LSB_HEADER          lsbHeaders[LSF_FALCON_ID_END + 1];
    PLSF_LSB_HEADER         pLsfHeader;
    ACR_FLCN_CONFIG         boOwnFlcnCfg;
    PLSF_WPR_HEADER         pWprHeader;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwBool                   bootstrapOwnPresent = LW_FALSE;

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL(pAcr));
    // Sets up scrubbing
    g_scrubState.scrubTrack = 0;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, 0U, LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END),
                                LSF_WPR_HEADER_ALIGNMENT)));

    isBootstrapOwnPresent(&bootstrapOwnPresent);
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index=0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

#ifdef ACR_SAFE_BUILD
	if(pWprHeader->bLazyBootstrap != 0U)
        {
            return ACR_ERROR_UNEXPECTED_ARGS;
        }
#endif

	CHECK_FALCON_ID_RANGE_AND_ERROR(pWprHeader->falconId);
        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            // Scrub till the end then

            // Get the end address of WPR region
            if (g_dmaProp.regionID > 0U)
            {
                pRegionProp = &(g_desc.regions.regionProps[g_dmaProp.regionID - 1U]);
            }
            else
            {
                return ACR_ERROR_REGION_PROP_ENTRY_NOT_FOUND;
            }

            CHECK_WRAP_AND_ERROR(pRegionProp->endAddress < pRegionProp->startAddress);
            wprRegionSize = (LwU32)(pRegionProp->endAddress - pRegionProp->startAddress) << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, wprRegionSize, 0));
            break;
        }

        // Scrub the gap between last known scrub point and this LSB header
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, pWprHeader->lsbOffset, LW_ALIGN_UP(sizeof(LSF_LSB_HEADER),
                               LSF_LSB_HEADER_ALIGNMENT)));

        // 
        // Check the status of the image
        //   If image is aleady validated or bootstrapped, continue
        // If the bootstrap owner is not PMU, continue
        //
        //
#ifndef ACR_SAFE_BUILD
        if ((pWprHeader->status < LSF_IMAGE_STATUS_COPY) ||
            (!(acrlibIsBootstrapOwner_HAL(pAcrlib, pWprHeader->bootstrapOwner))))
        {
            continue;
        }
#endif

        // Lets reset the status
        pLsfHeader = &(lsbHeaders[index]);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_HAL(pAcr, pWprHeader, pLsfHeader));

        // Scrub the gap between last known scrub point and ucode/data
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, pLsfHeader->ucodeOffset, pLsfHeader->ucodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, (pLsfHeader->ucodeOffset + pLsfHeader->ucodeSize), pLsfHeader->dataSize));

        // We dont expect bootloader data for falcons other than PMU and GR.
        if ((status = acrSanityCheckBlData_HAL(pAcr, pWprHeader->falconId, pLsfHeader->blDataOffset, pLsfHeader->blDataSize))!= ACR_OK)
        {
            return status;
        }
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrScrubUnusedWprWithZeroes_HAL(pAcr, pLsfHeader->blDataOffset, pLsfHeader->blDataSize));

#ifdef ACR_SIG_VERIF
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAuthenticateLSFUcodes(pWprHeader, pLsfHeader));
#else
        pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_SKIPPED;
#endif  // ACR_SIG_VERIF

#ifdef ACR_SUB_WPR
        // Setup LS Falcon Sub WPRs
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprs_HAL(pAcr, pWprHeader->falconId, pLsfHeader));
#endif // ACR_SUB_WPR

        // Check for bLazyBootstrap
        if ((pWprHeader->bLazyBootstrap != 0U) && (bootstrapOwnPresent == LW_TRUE)) 
        {
            continue;
        }

        // Only falcons with valid code and data are added to this bitmask
        validWprMask                |= BIT32(index);
    }

    // Compare the size now to make sure we accounted for all of WPR region
    if (g_scrubState.scrubTrack != wprRegionSize)
    {
        return ACR_ERROR_SCRUB_SIZE;
    }

#ifndef ACR_ONLY_BO
    // Reset the engine and falcon
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrResetEngineFalcon_HAL(pAcr, validWprMask));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPollForResetCompletion_HAL(pAcr, validWprMask));
#endif

    //
    // Setup the LS falcon
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSFalcon_HAL(pAcr, lsbHeaders, validWprMask));

    // Write WPR header back to FB to communicate the status
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteWprHeader_HAL(pAcr));

    // Set the DMACTL require ctx for bootstrap owner if needed
    if (acrlibGetFalconConfig_HAL(pAcrlib, ACR_LSF_LWRRENT_BOOTSTRAP_OWNER, LSF_FALCON_INSTANCE_DEFAULT_0, &boOwnFlcnCfg) == ACR_OK)
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, &boOwnFlcnCfg, g_bSetReqCtx);
    }

    return status;
}
