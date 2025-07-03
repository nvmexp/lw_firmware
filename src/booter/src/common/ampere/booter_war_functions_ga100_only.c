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
 * @file: acr_war_functions_ga100_only.c 
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "dev_pri_ringstation_sys.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_fbpa.h"
#include "dev_fb.h"

#include "mmu/mmucmn.h"
#include "hwproject.h"

#include "config/g_acr_private.h"

/*!
 * @brief Isolate LW_PLTCG_LTC*_LTS*_TSTG_CFG_2 address to only be accessible by FECS.
 *        Bug 2823165
 */
ACR_STATUS
acrProgramDecodeTrapToIsolateTSTGRegisterToFECSWARBug2823165_GA100(void)
{
    LwU32 trapMatch    = 0;
    LwU32 trapMatchCfg = 0;
    LwU32 trapAction   = 0;
    LwU32 trapMask     = 0;

    // match address 0x00140098.  match SOURCE_ID=0x4 (FECS)
    trapMatch = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP21_MATCH, _ADDR, 0x140098, trapMatch);
    trapMatch = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP21_MATCH, _SOURCE_ID, LW_PPRIV_SYS_PRI_SOURCE_ID_FECS, trapMatch);
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP21_MATCH, trapMatch);
    
    // _TRAP_APPLICATION=0xf (affect PL0,PL1,PL2, and PL3). _ILWERT_SOURCE_ID_MASK=1, _IGNORE_READ=1
    trapMatchCfg = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP21_MATCH_CFG, _TRAP_APPLICATION, _ALL_LEVELS_ENABLED, trapMatchCfg);
    trapMatchCfg = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP21_MATCH_CFG, _ILWERT_SOURCE_ID_MATCH, _ILWERTED, trapMatchCfg);
    trapMatchCfg = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP21_MATCH_CFG, _IGNORE_READ, _IGNORED, trapMatchCfg);
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP21_MATCH_CFG, trapMatchCfg);

    // match 0x001[4567].[02468aceACE]98.  match SOURCE_ID=0x4 (FECS)
    trapMask = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP21_MASK, _SOURCE_ID, LW_PPRIV_SYS_PRI_SOURCE_ID_FECS, trapMask);
    trapMask = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP21_MASK, _ADDR, 0x3FE00, trapMask);
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP21_MASK, trapMask);

    // _FORCE_ERROR_RETURN = 1
    trapAction = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP21_ACTION, _FORCE_ERROR_RETURN, _ENABLE, trapAction);
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP21_ACTION, trapAction);
      
    return ACR_OK;
}

#define NUM_TO_BITMASK(n) ((0x1 << n) - 1) 

ACR_STATUS
acrGetUsableFbSizeInMB_GA100(LwU64 *pUsableFbSizeMB)
{
    LwU32 activeFBPMask;
    LwU32 activeFBPAMask;
    LwU32 maskFBPFuse;
    LwU32 maskFBPAFuse;
    LwU32 memorySizeFBPA;
    LwU64 totalFBSizeMB;
    LwU32 numFBPs;
    LwU32 numFBPAsPerFBP;
    LwU32 numFBPAs;
    LwU32 addr;
    LwU32 data;
    LwU32 i;
    LwU32 j;
    LwU32 lwrrentFBPAIndex;

    if (pUsableFbSizeMB == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Read various PTOP registers.
    data = ACR_REG_RD32( BAR0, LW_PTOP_SCAL_NUM_FBPS);
    numFBPs = DRF_VAL(_PTOP, _SCAL_NUM_FBPS, _VALUE, data);

    data = ACR_REG_RD32( BAR0, LW_PTOP_SCAL_NUM_FBPA_PER_FBP);
    numFBPAsPerFBP = DRF_VAL(_PTOP, _SCAL_NUM_FBPA_PER_FBP, _VALUE, data);

    data = ACR_REG_RD32( BAR0, LW_PTOP_SCAL_NUM_FBPAS);
    numFBPAs = DRF_VAL(_PTOP, _SCAL_NUM_FBPAS, _VALUE, data);

    // Read various fuse registers.
    data = ACR_REG_RD32( BAR0, LW_FUSE_STATUS_OPT_FBP);
    maskFBPFuse = DRF_VAL(_FUSE, _STATUS_OPT_FBP, _DATA, data);

    // Read FBPA information using FBIO fuse - Bug 2062796
    data = ACR_REG_RD32( BAR0, LW_FUSE_STATUS_OPT_FBIO);
    maskFBPAFuse = DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, data);


    // Get the active FBP bitmask.
    activeFBPMask = ~maskFBPFuse;
    activeFBPMask &= NUM_TO_BITMASK(numFBPs);

    // Get the active FBPA bitmask.
    activeFBPAMask = ~maskFBPAFuse;
    activeFBPAMask &= NUM_TO_BITMASK(numFBPAs);

    //
    // Loop through all FBPs to determine total available memory.
    //
    totalFBSizeMB = 0;
    lwrrentFBPAIndex = 0;
    for (i = 0; ((LwU32)BIT(i)) <= activeFBPMask; i++)
    {
        // Skip over floorswept FBPs.
        if (!(activeFBPMask & BIT(i)))
        {
            continue;
        }

        // iterate over all FBPAs within FBP
        for ( j = 0; j < numFBPAsPerFBP; j++ )
        {
            lwrrentFBPAIndex = (numFBPAsPerFBP*i)+j;

            // Skip over floorswept FBPAs.
            if (activeFBPAMask & BIT(lwrrentFBPAIndex))
            {
                addr = LW_PFB_FBPA_0_CSTATUS + (lwrrentFBPAIndex*LW_FBPA_PRI_STRIDE);
                data = ACR_REG_RD32( BAR0, addr );

                memorySizeFBPA = DRF_VAL(_PFB, _FBPA_CSTATUS, _RAMAMOUNT, data);

                totalFBSizeMB += memorySizeFBPA;
            }
        }
    }
    *pUsableFbSizeMB = totalFBSizeMB;
    
    return ACR_OK;
}


ACR_STATUS
acrCheckWprRangeWithRowRemapperReserveFB_GA100(
    LwU64 wprStartAddrInBytes,
    LwU64 wprEndAddrInBytes
)
{
    ACR_STATUS  status                          = ACR_ERROR_UNKNOWN;
    LwU64 acrFbHoleSizeInBytes                  = 0;
    LwU64 acrStartAddressOfBottomBandInBytes    = 0;
    LwU64 acrEndAddressOfBottomBandInBytes      = 0;
    LwU64 acrEndAddressOfTopBandInBytes         = 0;
    LwU64 acrStartAddressOfTopBandInBytes       = 0;    
    LwU64 lwrMemLockRangeStartAddrInBytes       = 0;
    LwU64 lwrMemLockRangeEndAddrInBytes         = 0;
    LwU64 fbHoleInMB                            = 0;
    LwU64 fbSizeCStatusInMB                     = 0;
    LwU64 fbSizeLocalMemRangeInMB               = 0;    
    
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetUsableFbSizeInMB_HAL(pAcr, &fbSizeCStatusInMB));

    if (fbSizeCStatusInMB == 0) 
    {
         return ACR_ERROR_ILWALID_USABLE_FB_SIZE;
    }
    
    fbSizeLocalMemRangeInMB = acrGetLocalMemRangeInMB_HAL(pAcr);
    
    //To identify if vbios has fix to set local mem range to full FB in case if row remapper is enabled. 
    //Based on that we need to cross check that WPR is within available FB and does not collide with FB hole
    if(fbSizeCStatusInMB < fbSizeLocalMemRangeInMB){ 
    
        fbHoleInMB = fbSizeLocalMemRangeInMB - fbSizeCStatusInMB;
                        
        acrFbHoleSizeInBytes                 =   (fbHoleInMB << SHIFT_1MB) ;
        acrStartAddressOfBottomBandInBytes   =   (0x0) ;
        acrEndAddressOfBottomBandInBytes     =   (acrStartAddressOfBottomBandInBytes + acrFbHoleSizeInBytes - 1) ;
        acrEndAddressOfTopBandInBytes        =   ((fbSizeLocalMemRangeInMB << SHIFT_1MB) - 1);
        acrStartAddressOfTopBandInBytes      =   (acrEndAddressOfTopBandInBytes + 1 - acrFbHoleSizeInBytes);       
        
       

        // Check1: Bottom Fb hole sized band, WPR start addr check
        if (wprStartAddrInBytes <= acrEndAddressOfBottomBandInBytes)
        {
            return ACR_ERROR_WPR_START_ADDR_BOTTOM_FB_HOLE_SIZED_BAND_CHECK_FAIL;
        }

        // Check2: Bottom Fb hole sized band, WPR end addr check
        if (wprEndAddrInBytes <= acrEndAddressOfBottomBandInBytes)
        {
             return ACR_ERROR_WPR_END_ADDR_BOTTOM_FB_HOLE_SIZED_BAND_CHECK_FAIL;
        }

        // Check3: Top Fb hole sized band, WPR start addr check
        if (wprStartAddrInBytes >= acrStartAddressOfTopBandInBytes)
        {
            return ACR_ERROR_WPR_START_ADDR_TOP_FB_HOLE_SIZED_BAND_CHECK_FAIL;
        }

        // Check4: Top Fb hole sized band, WPR end addr check 
        if (wprEndAddrInBytes >= acrStartAddressOfTopBandInBytes)
        {
            return ACR_ERROR_WPR_END_ADDR_TOP_FB_HOLE_SIZED_BAND_CHECK_FAIL;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwrMemLockRange_HAL(pAcr, &lwrMemLockRangeStartAddrInBytes, &lwrMemLockRangeEndAddrInBytes));

        if (lwrMemLockRangeStartAddrInBytes > lwrMemLockRangeEndAddrInBytes)
        {
            return ACR_ERROR_ROW_REMAPPER_WAR_BUG_2968134_MEMLOCK_RANGE_NOT_ENABLED;
        }

        if (lwrMemLockRangeStartAddrInBytes != acrStartAddressOfTopBandInBytes)
        {
            return ACR_ERROR_ROW_REMAPPER_WAR_BUG_2968134_MEMLOCK_RANGE_START_ADDR_NOT_PROPERLY_SET_ON_TOP_BAND;
        }

        if (lwrMemLockRangeEndAddrInBytes != acrEndAddressOfTopBandInBytes)
        {
            return ACR_ERROR_ROW_REMAPPER_WAR_BUG_2968134_MEMLOCK_RANGE_END_ADDR_NOT_PROPERLY_SET_ON_TOP_BAND;
        }
    }
    return ACR_OK;   
}

ACR_STATUS
acrDisableMemoryLockRangeRowRemapWARBug2968134_GA100()
{
    ACR_STATUS  status                              = ACR_ERROR_UNKNOWN;
    LwU64       lwrMemLockRangeStartAddrInBytes     = 0;
    LwU64       lwrMemLockRangeEndAddrInBytes       = 0;
    LwU64       memSizeCStatusMB                    = 0;
    LwU64       memSizeLocalMemRangeMB              = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetUsableFbSizeInMB_HAL(pAcr, &memSizeCStatusMB));
  
    if (memSizeCStatusMB == 0) 
    {
         return ACR_ERROR_ILWALID_USABLE_FB_SIZE;
    }
    
    memSizeLocalMemRangeMB = acrGetLocalMemRangeInMB_HAL(pAcr);
    
   
    if(memSizeCStatusMB < memSizeLocalMemRangeMB)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwrMemLockRange_HAL(pAcr, 
                                                                    &lwrMemLockRangeStartAddrInBytes,
                                                                    &lwrMemLockRangeEndAddrInBytes));

        if (lwrMemLockRangeStartAddrInBytes > lwrMemLockRangeEndAddrInBytes)
        {
            return ACR_ERROR_ROW_REMAPPER_WAR_BUG_2968134_MEMLOCK_RANGE_NOT_ENABLED;
        }

        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_LO, DRF_DEF(_PFB, _PRI_MMU_LOCK_ADDR_LO, _VAL, _INIT));

        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_HI, DRF_DEF(_PFB, _PRI_MMU_LOCK_ADDR_HI, _VAL, _INIT));
    }

    return ACR_OK;
}

ACR_STATUS
acrEnableMemoryLockRangeRowRemapWARBug2968134_GA100()
{
    ACR_STATUS  status                              = ACR_ERROR_UNKNOWN;
    LwU64       lwrMemLockRangeStartAddrInBytes     = 0;
    LwU64       lwrMemLockRangeEndAddrInBytes       = 0;
    LwU64       memSizeCStatusMB                    = 0;
    LwU64       memSizeLocalMemRangeMB              = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetUsableFbSizeInMB_HAL(pAcr, &memSizeCStatusMB));

    if (memSizeCStatusMB == 0) 
    {
         return ACR_ERROR_ILWALID_USABLE_FB_SIZE;
    }
    
    memSizeLocalMemRangeMB = acrGetLocalMemRangeInMB_HAL(pAcr);

    LwU32 memlockRangeStartAddr4K   = 0;
    LwU32 memlockRangeEndAddr4K     = 0;
    
   
    if(memSizeCStatusMB < memSizeLocalMemRangeMB)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwrMemLockRange_HAL(pAcr, 
                                                                    &lwrMemLockRangeStartAddrInBytes,
                                                                    &lwrMemLockRangeEndAddrInBytes));

        // If memory lock is already enabled somehow, raise an alarm - fail ACR Unload
        if (lwrMemLockRangeStartAddrInBytes <= lwrMemLockRangeEndAddrInBytes)
        {
            return ACR_ERROR_ROW_REMAPPER_WAR_BUG_2968134_CANT_ENABLE_MEMLOCK_RANGE_ALREADY_ENABLED;
        }
        memlockRangeStartAddr4K = memSizeCStatusMB << COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT;
        memlockRangeEndAddr4K   = (memSizeLocalMemRangeMB << COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT) - 1;

        
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_LO, DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_LO, _VAL, memlockRangeStartAddr4K));

        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCK_ADDR_HI, DRF_NUM(_PFB, _PRI_MMU_LOCK_ADDR_HI, _VAL, memlockRangeEndAddr4K));
    }

    return ACR_OK;
}


