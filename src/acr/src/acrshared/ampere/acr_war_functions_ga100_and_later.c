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
 * @file: acr_war_functions_ga100_and_later.c 
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "dev_fb.h"

#include "mmu/mmucmn.h"

#include "config/g_acr_private.h"

ACR_STATUS
acrCheckWprRangeWithRowRemapperReserveFB_GA10X(
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
    if (fbSizeCStatusInMB < fbSizeLocalMemRangeInMB)
    { 
  
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

    }
    return ACR_OK;   
}

LwU64
acrGetLocalMemRangeInMB_GA100()
{
    LwU16          lowerMag             = 0;    // LwU16 on purpose
    LwU16          lowerScale           = 0;    // LwU16 on purpose
    LwBool         bEccMode;
    LwU64          totalFbSizeMB       = 0;

    //
    // Another errorcheck: LOWER_SCALE, LOWER_MAG must not be _INIT, they should have been
    // initialized to sane values by devinit. If we find them at _INIT values, something is
    // wrong, so bail out with error.
    //
    lowerScale = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_SCALE);
    lowerMag   = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_MAG);
    bEccMode   = REG_RD_DRF(BAR0, _PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _ECC_MODE);

    ct_assert(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT == 0);
    ct_assert(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT   == 0);

    if (lowerScale == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT ||
        lowerMag   == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT)
    {
        return 0;
    }

    //
    // Start of FB = address 0x0
    // End of FB   = lowerMag * (2^(lowerScale+20))
    // Avoiding the +20 in the equation will give us FB size in MB instead of bytes
    // (2 ^ someValue) can be easily computed as (1 << someValue)
    // For FB, it is even easier to callwlate "totalFbSizeMB = lowerMag << lowerScale", instead of "totalFbSizeMB = lowerMag *  (1 << lowerScale)"
    // lowerScale is 4 bits in HW, so it will max out at value 0xF, and lowerMag can be max 0x3f (6 bits)
    // With max values, the multiplication result will still fit in an LwU32
    //
    totalFbSizeMB = (LwU64)( (LwU32)lowerMag << lowerScale );
    //
    // With ECC_MODE set, the callwlations are:
    // lower_range = 15/16 * LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG * (2^(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE+20))
    // TODO: Update this logic to read FB size directly from secure scratch registers, Bug 2168823 has details
    //
    if (bEccMode)
    {
        totalFbSizeMB = ( totalFbSizeMB >> 4 ) * (LwU64)15;
    }

    return totalFbSizeMB;
}


