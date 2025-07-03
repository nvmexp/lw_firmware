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
 * @file: acr_wprt234.c
 */

//
// Includes
//
#include "acr.h"
#include "pmuifcmn.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "hwproject.h"
#include "external/lwmisc.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

//
// Function Definitions
//

/*!
 * @brief Check if Locked WPR is present\n
 *        Read MMU_WPR(i)_ADDR_LO/HI registers to know WPR start/end address.\n
 *        Make sure ucodeBlobsize is not greater than wpr size.\n
 *        Read READ/WRITE mask to ascertain WPR is created with correct permissions.\n
 *        If WPR region is already known, validate it using regionprops.\n
 *        Or else
 *        Update region properties(regionProp) in g_desc with the WPR info found i.e. start/end address, read/write mask, wprRegion ID. 
 *
 * @param[in]  pWprIndex                     Index into g_desc region properties holding wpr region
 *
 *   @global_START
 *   @global_{g_desc.regions , Region descriptors}
 *   @global_{g_desc.wprRegionID , wprRegionID specified by lwgpu-rm}
 *   @global_END
 *
 * @return     ACR_OK                        If ACR regions are locked 
 * @return     ACR_ERROR_ILWALID_REGION_ID   If ACR region details fetched doesn't match that of RM
 * @return     ACR_ERROR_NO_WPR              If no valid WPR region
 */
ACR_STATUS
acrFindWprRegions_T234
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS               status          = ACR_OK;
    LwU32                    i               = 0;
    LwU64                    start           = 0;
    LwU64                    end             = 0;
    LwU32                    writeMask       = 0;
    LwU32                    readMask        = 0;
    LwBool                   bWprRegionFound = LW_FALSE;

    *pWprIndex = 0xFFFFFFFF;
    
    // Find WPR region
    for(i = 0; i < LW_MMU_WRITE_PROTECTION_REGIONS; i++)
    {
        // Read ReadMask
        readMask = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
        readMask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR, i + 1U, readMask);

        // Read WriteMask
        writeMask = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
        writeMask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR, i + 1U, writeMask);

        //
        // TODO: This is needed because of a bug in manual defines. This will be removed once the
        // the bug is fixed.
        //
        writeMask &= 0xFU;
        // TODO: Change expected RMASK to correct value after RMASK of WPR is changed in CHEETAH RM & Bootloaders.
        if ((readMask  <= LSF_WPR_REGION_RMASK_FOR_TEGRA) && (writeMask == LSF_WPR_REGION_WMASK))
        {
            // Found WPR region

            // Read start address
            start = (LwU64)ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
            start = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, start);
            start = start << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;

            // Read end address
            end = (LwU64)ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
            end = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, end);
            end = end << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT; 

            if (start < end)
            {
                // End address always point to start of the last aligned address
                if (g_desc.ucodeBlobSize > (LwU32)(end - start))
                {
                    return ACR_ERROR_NO_WPR;
                }
                end += ACR_REGION_ALIGN;
                start >>= LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;
                end   >>= LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;
                bWprRegionFound = LW_TRUE;
            }
            else
            {
                start = end = 0;
            }

            break;
        }
    }

    if (bWprRegionFound && (g_desc.regions.noOfRegions > 0U))
    {
        //Found a valid WPR region
        PRM_FLCN_ACR_REGION_PROP regionProp;

        if ((i+1U) != LSF_WPR_EXPECTED_REGION_ID)
        {
            return ACR_ERROR_ILWALID_REGION_ID;
        }

        regionProp = &(g_desc.regions.regionProps[i]);

        if (g_desc.wprRegionID > 0U)
        {
            //
            // If WPR region is already known, validate it.
            // This is the case when RM loads ACR binary.
            //
            if (i >= g_desc.regions.noOfRegions)
            {
                return ACR_ERROR_ILWALID_REGION_ID;
            }

            if (regionProp->regionID != g_desc.wprRegionID)
            {
                return ACR_ERROR_ILWALID_REGION_ID;
            }

            if ((start     != regionProp->startAddress) ||
                (end       != regionProp->endAddress)   ||
                (readMask  != regionProp->readMask)     ||
                (writeMask != regionProp->writeMask))
            {
                return ACR_ERROR_NO_WPR;
            }
        }
        else
        {
            //
            // If WPR region is unknown, populate it.
            // This is the case when Android loads ACR binary.
            // TODO check start and end addresses for overflow.
            //
            g_desc.wprRegionID       = i + 1U;
            regionProp->startAddress = (LwU32)start;
            regionProp->endAddress   = (LwU32)end;
            regionProp->readMask     = readMask;
            regionProp->writeMask    = writeMask;
        }

        *pWprIndex = i;
    }
    else
    {
        if (g_desc.wprRegionID > 0U)
        {
            status = ACR_ERROR_ILWALID_REGION_ID;
        }
        else
        {
            status = ACR_ERROR_NO_WPR;
        }
    }

    return status;
}

/*!
 * @brief Program particular falcon SubWPR in PRI MMU register pair.\n
 *        Write the sub WPR start address and read mask to MMU_FALCON_<FALCON>_CFGA register.\n
 *        Write the sub WPR end address and write mask to corresponding MMU_FALCON_<FALCON>_CFGB register.\n
 * 
 * @param[in]  falconId     Falcon ID for the which the programming is to be done.
 * @param[in]  flcnSubWprId Sub-WPR Id indicating code section or data section for the falconID.
 * @param[in]  startAddr    Start address of the sub section of WPR.
 * @param[in]  endAddr      End address of the sub section of WPR.
 * @param[in]  readMask     Read priv level mask to be applied for the sub WPR. 
 * @param[in]  writeMask    Write priv level mask to be applied for the sub WPR.
 * 
 * @return     ACR_OK       If sub wpr registers are programmed successfully.
 */
ACR_STATUS
acrProgramFalconSubWpr_T234(LwU32 falconId, LwU8 flcnSubWprId, LwU32 startAddr, LwU32 endAddr, LwU8 readMask, LwU8 writeMask)
{
    ACR_STATUS      status  = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           regCfga = 0;
    LwU32           regCfgb = 0;
    LwU32           addr    = 0;
    // Get falcon config
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));


    //
    // Update subWPR in MMU
    // SubWPR registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWPR of same falcon are 8 bytes seperated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ALLOW_READ, readMask, regCfga);
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, startAddr, regCfga);
    addr    = (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId));
    ACR_REG_WR32(BAR0, addr, regCfga);

    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ALLOW_WRITE, writeMask, regCfgb);
    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, endAddr, regCfgb);
    addr    = (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                    + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF);
    ACR_REG_WR32(BAR0, addr, regCfgb);

    return status;
}

/*!
 * @brief Setup Sub WPR for code and data part of falcon's ucode in FB-WPR
 *        Callwlate the start and end address for falcon code and data from the LsfHeader details.\n
 *        Fetch the read, write masks and sub WPR region IDs based on falcon ID.\n
 *        Call acrProgramFalconSubWpr to progrma the corresponding MMU registers to setup sub WPR.\n
 * 
 * @param[in]  falconId   Falcon ID for the which the programming is to be done.
 * @param[in]  pLsfHeader Pointer to LSF header for falcon.
 *
 * @return     ACR_OK                           If sub WPRs are setup successfully for the falcon.
 * @return     ACR_ERROR_ILWALID_ARGUMENT       If invalid argument is detected.
 * @return     ACR_ERROR_VARIABLE_SIZE_OVERFLOW If variable size overflow is detected.
 * @return     ACR_ERROR_FLCN_ID_NOT_FOUND      If falcon ID does not have entry within the end of WPR header.
 */
ACR_STATUS
acrSetupFalconCodeAndDataSubWprs_T234
(
    LwU32 falconId,
    PLSF_LSB_HEADER pLsfHeader
)
{
    ACR_STATUS status = ACR_OK;
    LwU64      codeStartAddr;
    LwU64      dataStartAddr;
    LwU64      codeEndAddr;
    LwU64      dataEndAddr;
    LwU32      codeSubWprId;
    LwU32      dataSubWprId;

    if (pLsfHeader == LW_NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    codeStartAddr = (g_dmaProp.wprBase << 8) + pLsfHeader->ucodeOffset;
    codeEndAddr   = (codeStartAddr + pLsfHeader->ucodeSize - 1);

    dataStartAddr = codeStartAddr + pLsfHeader->ucodeSize;
    dataEndAddr   = dataStartAddr + pLsfHeader->dataSize + pLsfHeader->blDataSize - 1;

    // Sanitize above callwlation for size overflow
    if ((codeStartAddr < g_dmaProp.wprBase) || (codeEndAddr < codeStartAddr) ||
        (dataStartAddr < codeStartAddr) || (dataEndAddr < dataStartAddr))
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    switch (falconId)
    {
#ifndef ACR_SAFE_BUILD
        case LSF_FALCON_ID_PMU:
            return ACR_OK;
#endif //ACR_SAFE_BUILD
        case LSF_FALCON_ID_FECS:
            codeSubWprId = FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_GPCCS:
            codeSubWprId = GPCCS_SUB_WPR_ID_0;
            dataSubWprId = GPCCS_SUB_WPR_ID_1;
            break;
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    //
    // Give read only permission for code section and read/write permission for
    // data section for each falcons.
    //

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, codeSubWprId, (codeStartAddr >> SHIFT_4KB),
                                        (codeEndAddr >> SHIFT_4KB), ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, dataSubWprId, (dataStartAddr >> SHIFT_4KB),
                                        (dataEndAddr >> SHIFT_4KB), ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_L3));

    return status;
}
