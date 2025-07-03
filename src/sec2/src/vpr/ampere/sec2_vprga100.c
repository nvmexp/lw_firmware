/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprga100.c
 * @brief  VPR HAL Functions for GA100
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_top.h"
#include "dev_fbpa.h"
#include "dev_gc6_island.h"
#include "hwproject.h"
#include "lwdevid.h"

/* ------------------------- System Includes -------------------------------- */
#include "csb.h"
#include "flcnretval.h"
#include "sec2_bar0_hs.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "apm/sec2_apmdefines.h"
#include "config/g_vpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
static FLCN_STATUS _vprCheckApmEnabledHs_GA100(void)
    GCC_ATTRIB_SECTION("imem_vprHs", "_vprCheckApmEnabledHs_GA100");
#endif

/*!
 * @brief vprIsVprEnableInMmu_GA10X: 
 *  Checks whether VPR is enable in MMU
 *
 * @return FLCN_ERR_VPR_APP_VPR_IS_ALREADY_ENABLED if vpr is already enabled
 *         else FLCN_OK
 */
FLCN_STATUS
vprIsVprEnableInMmu_GA100
(
    void
)
{
    LwU32 cyaLo            = 0;
    FLCN_STATUS flcnStatus = FLCN_ERROR;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, &cyaLo));
    if (FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_CYA_LO, _IN_USE, _TRUE, cyaLo))
    {
        flcnStatus = FLCN_ERR_VPR_APP_VPR_IS_ALREADY_ENABLED;
    }
ErrorExit:
        return flcnStatus;
}



/*!
 * @brief Set trust levels for all VPR clients
 */
FLCN_STATUS
vprSetupClientTrustLevel_GA100(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERROR;
    LwU32 cyaLo            = 0;
    LwU32 cyaHi            = 0;

    //
    // Check if VPR is already enabled, if yes fail with error
    // Since we need to setup client trust levels before enabling VPR
    //
    CHECK_FLCN_STATUS(vprIsVprEnableInMmu_HAL()); 
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_HI, &cyaHi));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, &cyaLo));

    // Setup VPR CYA's
    //
    // VPR_AWARE list from CYA_LO : CE2 
    // GRAPHICS list from CYA_LO  : TEX, PE
    //
    cyaLo = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_DEFAULT, LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_DEFAULT_USE_CYA) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CPU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_HOST,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PERF,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PMU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CE2,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SEC,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_FE,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PD,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SCC,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SKED,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_L1,      LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_TEX,     LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PE,      LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE));


    //
    // VPR_AWARE list from CYA_HI : LWENC, LWDEC
    // GRAPHICS list from CYA_LO  : RASTER, GCC, PROP
    //
    cyaHi = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_RASTER,     LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GCC,        LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GPCCS,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP,       LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_READ,  LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_WRITE, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_DNISO,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWDEC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GSP,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED)  |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_OFA,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED));

    // 
    // *_VPR_CYA_LO_TRUST_DEFAULT bit is the knob to apply trust levels programmed in *_LO and *_HI registers.
    //  So *_LO register is programmed after *_HI.
    //
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_HI, cyaHi));
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo));

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get VPR min address
 */
FLCN_STATUS
vprGetMinStartAddress_GA100(LwU64 *pVprMinAddr)
{
    *pVprMinAddr = 0;
    return FLCN_OK;
}

/*!
 * @brief Get VPR max address from secure scratch register
 */
FLCN_STATUS
vprGetMaxEndAddress_GA100(LwU64 *pVprMaxEndAddr)
{
    FLCN_STATUS flcnStatus    = FLCN_OK;
    LwU16       lowerMag      = 0;    // LwU16 on purpose
    LwU16       lowerScale    = 0;    // LwU16 on purpose
    LwU64       totalFbSizeMB = 0;
    LwBool      bEccMode;
    LwU32       data;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE, &data));

    lowerScale = (LwU16)DRF_VAL(_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_SCALE, data);
    lowerMag   = (LwU16)DRF_VAL(_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _LOWER_MAG, data);
    bEccMode   = DRF_VAL(_PFB, _PRI_MMU_LOCAL_MEMORY_RANGE, _ECC_MODE, data);

    ct_assert(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT == 0);
    ct_assert(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT == 0);

    //
    // Another errorcheck: LOWER_SCALE, LOWER_MAG must not be _INIT, they should have been
    // initialized to sane values by devinit. If we find them at _INIT values, something is
    // wrong, so bail out with error.
    //
    if (lowerScale == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE_INIT ||
        lowerMag == LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG_INIT)
    {
        return FLCN_ERR_VPR_APP_ILWALID_REQUEST_END_ADDR;
    }

    //
    // Start of FB = address 0x0
    // End of FB   = lowerMag * (2^(lowerScale+20))
    // Avoiding the +20 can give us the value in MBs
    // For FB, it is even easier to callwlate "totalFbSizeMB = lowerMag << lowerScale", instead of "totalFbSizeMB = lowerMag *  (1 << lowerScale)"
    // lowerScale is 4 bits in HW, so it will max out at value 0xF, and lowerMag can be max 0x3f (6 bits)
    // With max values, the multiplication result will still fit in an LwU32
    //
    totalFbSizeMB = (LwU64)((LwU32)lowerMag << lowerScale);
    //
    // With ECC_MODE set, the callwlations are:
    // lower_range = 15/16 * LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_MAG * (2^(LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE_LOWER_SCALE+20))
    // TODO: Update this logic to read FB size directly from secure scratch registers, Bug 2168823 has details
    //
    if (bEccMode)
    {
        totalFbSizeMB = (totalFbSizeMB >> 4) * (LwU64)15;
    }

    *pVprMaxEndAddr = (totalFbSizeMB << SHIFT_1MB) - 1;
ErrorExit:
    return flcnStatus;
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
/*!
 * @brief Gets usable FB size by iterating over all active FBPAs
 *        inside all active FBPs and aclwmulating their sizes
 */
FLCN_STATUS
vprGetUsableFbSizeInMb_GA100(LwU64 *pUsableFbSizeMB)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
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
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read various PTOP registers.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PTOP_SCAL_NUM_FBPS, &data));
    numFBPs = DRF_VAL(_PTOP, _SCAL_NUM_FBPS, _VALUE, data);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PTOP_SCAL_NUM_FBPA_PER_FBP, &data));
    numFBPAsPerFBP = DRF_VAL(_PTOP, _SCAL_NUM_FBPA_PER_FBP, _VALUE, data);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PTOP_SCAL_NUM_FBPAS, &data));
    numFBPAs = DRF_VAL(_PTOP, _SCAL_NUM_FBPAS, _VALUE, data);

    // Read various fuse registers.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_STATUS_OPT_FBP, &data));
    maskFBPFuse = DRF_VAL(_FUSE, _STATUS_OPT_FBP, _DATA, data);

    // Read FBPA information using FBIO fuse - Bug 2062796
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_STATUS_OPT_FBIO, &data));
    maskFBPAFuse = DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, data);

    // Get the active FBP bitmask.
    activeFBPMask = ~maskFBPFuse;
    activeFBPMask &= (BIT(numFBPs) - 1);

    // Get the active FBPA bitmask.
    activeFBPAMask = ~maskFBPAFuse;
    activeFBPAMask &= (BIT(numFBPAs) - 1);

    // Loop through all FBPs to determine total available memory.
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
        for (j = 0; j < numFBPAsPerFBP; j++)
        {
            lwrrentFBPAIndex = (numFBPAsPerFBP * i) + j;

            // Skip over floorswept FBPAs.
            if (activeFBPAMask & BIT(lwrrentFBPAIndex))
            {
                addr = LW_PFB_FBPA_0_CSTATUS + (lwrrentFBPAIndex * LW_FBPA_PRI_STRIDE);
                CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(addr, &data));

                memorySizeFBPA = DRF_VAL(_PFB, _FBPA_CSTATUS, _RAMAMOUNT, data);

                totalFBSizeMB += memorySizeFBPA;
            }
        }
    }
    *pUsableFbSizeMB = totalFBSizeMB;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Ensures that the VPR range provided by RM (untrusted) does not fall
 *        inside FB holes introduced by row remapper
 */
FLCN_STATUS
vprCheckVprRangeWithRowRemapperReserveFb_GA100
(
    LwU64 vprStartAddrInBytes,
    LwU64 vprEndAddrInBytes
)
{
    FLCN_STATUS  flcnStatus                 = FLCN_OK;
    LwU64 fbHoleSizeInBytes                 = 0;
    LwU64 fbStartAddressOfBottomBandInBytes = 0;
    LwU64 fbEndAddressOfBottomBandInBytes   = 0;
    LwU64 fbEndAddressOfTopBandInBytes      = 0;
    LwU64 fbStartAddressOfTopBandInBytes    = 0;
    LwU64 fbSizeCStatusInMB                 = 0;
    LwU64 fbSizeCStatusInBytes              = 0;
    LwU64 fbSizeLocalMemEndAddress          = 0;
    LwU64 fbSizeLocalMemRangeInBytes        = 0;

    CHECK_FLCN_STATUS(vprGetUsableFbSizeInMb_HAL(&VprHal, &fbSizeCStatusInMB));

    if (fbSizeCStatusInMB == 0)
    {
         return FLCN_ERR_ILWALID_ARGUMENT;
    }

    fbSizeCStatusInBytes = fbSizeCStatusInMB << SHIFT_1MB;

    // vprGetMaxEndAddress returns LOCAL_MEM end address
    CHECK_FLCN_STATUS(vprGetMaxEndAddress_HAL(&VprHal, &fbSizeLocalMemEndAddress));
    fbSizeLocalMemRangeInBytes = fbSizeLocalMemEndAddress + 1;

    //
    // To identify if vbios has fix to set local mem range to full FB in case if row remapper is enabled. 
    // Based on that we need to cross check that WPR is within available FB and does not collide with FB hole
    //
    if (fbSizeCStatusInBytes < fbSizeLocalMemRangeInBytes)
    {
        fbHoleSizeInBytes                 = fbSizeLocalMemRangeInBytes - fbSizeCStatusInBytes;
        fbStartAddressOfBottomBandInBytes = 0;
        fbEndAddressOfBottomBandInBytes   = fbStartAddressOfBottomBandInBytes + fbHoleSizeInBytes - 1;
        fbEndAddressOfTopBandInBytes      = fbSizeLocalMemRangeInBytes - 1;
        fbStartAddressOfTopBandInBytes    = fbEndAddressOfTopBandInBytes + 1 - fbHoleSizeInBytes;

        // Check 1: Bottom FB hole sized band, VPR start addr check
        if (vprStartAddrInBytes <= fbEndAddressOfBottomBandInBytes)
        {
            return FLCN_ERR_VPR_APP_ILWALID_REQUEST_START_ADDR;
        }

        // Check2: Top Fb hole sized band, VPR start addr check
        if (vprStartAddrInBytes >= fbStartAddressOfTopBandInBytes)
        {
            return FLCN_ERR_VPR_APP_ILWALID_REQUEST_START_ADDR;
        }

        // Check3: Top Fb hole sized band, VPR end addr check 
        if (vprEndAddrInBytes >= fbStartAddressOfTopBandInBytes)
        {
            return FLCN_ERR_VPR_APP_ILWALID_REQUEST_END_ADDR;
        }
    }
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if APM is enabled or not.
 *          1. Check for scratch populated by VBIOS
 *          2. Check for CHIP & SKU IDs
 *          3. Check if InitHS has populated the right scratch value
 *          4. Restrict it to only an allowed list of PDIs for now
 */

// ipp1-0019
#define LW_APM_CHIP_1_PDI_0 0xc9353c4b
#define LW_APM_CHIP_1_PDI_1 0x3e3dd5db

// aus-a12-04
#define LW_APM_CHIP_2_PDI_0 0xf8a7f536
#define LW_APM_CHIP_2_PDI_1 0xfbedcfc8

// aus-a12-03
#define LW_APM_CHIP_3_PDI_0 0x988cfeb9
#define LW_APM_CHIP_3_PDI_1 0xe906c56d

// ipp1-i602
#define LW_APM_CHIP_4_PDI_0 0xc0f87586
#define LW_APM_CHIP_4_PDI_1 0xd397d122

// ipp1-1625
#define LW_APM_CHIP_5_PDI_0 0x626545f2
#define LW_APM_CHIP_5_PDI_1 0x17cdc2f2

// ipp1-1607
#define LW_APM_CHIP_6_PDI_0 0x13df3294
#define LW_APM_CHIP_6_PDI_1 0x77f3ebbb

// ipp1-1379
#define LW_APM_CHIP_7_PDI_0 0x380ffd6a
#define LW_APM_CHIP_7_PDI_1 0x739390c3

// ipp1-1606
#define LW_APM_CHIP_8_PDI_0 0xf1a033de
#define LW_APM_CHIP_8_PDI_1 0x18317cde

// ipp1-1611
#define LW_APM_CHIP_9_PDI_0 0x5082c4a6
#define LW_APM_CHIP_9_PDI_1 0xfe26153b

// ipp1-1377
#define LW_APM_CHIP_10_PDI_0 0x401E7337
#define LW_APM_CHIP_10_PDI_1 0xF2943230

// ipp1-1733
#define LW_APM_CHIP_11_PDI_0 0xef1040f1
#define LW_APM_CHIP_11_PDI_1 0x77250af3

// ipp1-1734
#define LW_APM_CHIP_12_PDI_0 0xeb802290
#define LW_APM_CHIP_12_PDI_1 0xa0a0d84d

// ipp1-1621
#define LW_APM_CHIP_13_PDI_0 0xd6df888e
#define LW_APM_CHIP_13_PDI_1 0x7cb33799

// ipp1-1618
#define LW_APM_CHIP_14_PDI_0 0xe69fc9b0
#define LW_APM_CHIP_14_PDI_1 0x1df73add

// ipp1-2208
#define LW_APM_CHIP_15_PDI_0 0x3bdc9c4a
#define LW_APM_CHIP_15_PDI_1 0x3de39f97

// ipp1-2209
#define LW_APM_CHIP_16_PDI_0 0x5c89852f
#define LW_APM_CHIP_16_PDI_1 0x008f639f

// ipp1-2210
#define LW_APM_CHIP_17_PDI_0 0x41580728
#define LW_APM_CHIP_17_PDI_1 0x73c540d4

// ipp1-2211
#define LW_APM_CHIP_18_PDI_0 0x23fa02f8
#define LW_APM_CHIP_18_PDI_1 0x824339db

// ipp1-2212
#define LW_APM_CHIP_19_PDI_0 0x7a1cda98
#define LW_APM_CHIP_19_PDI_1 0x2dde4264

// ipp1-2213
#define LW_APM_CHIP_20_PDI_0 0x89c61b1e
#define LW_APM_CHIP_20_PDI_1 0x8e6c780f

// ipp1-1626
#define LW_APM_CHIP_21_PDI_0 0xa4188f42
#define LW_APM_CHIP_21_PDI_1 0x78e4372a

// ipp1-1627
#define LW_APM_CHIP_22_PDI_0 0x7cb0fae8
#define LW_APM_CHIP_22_PDI_1 0xa63c99c2

// ipp1-1628
#define LW_APM_CHIP_23_PDI_0 0xe874fd4c
#define LW_APM_CHIP_23_PDI_1 0x42d51d80

static FLCN_STATUS
_vprCheckApmEnabledHs_GA100(void)
{
    FLCN_STATUS flcnStatus   = FLCN_ERR_HS_APM_NOT_ENABLED;
    LwU32       chipId       = 0;
    LwU32       devId        = 0;
    LwU32       scratch      = 0;
    LwU32       val          = 0;
    LwU32       apmMode      = 0;
    LwU32       pdi0, pdi1;

    // Ignore SKU check if debug
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS, &val));
    if (val != LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO)
    {
        //
        // Check if PDI is one of valid PDIs
        //
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PDI_0, &pdi0));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PDI_1, &pdi1));

        if (!(((pdi0 == LW_APM_CHIP_1_PDI_0) && (pdi1 == LW_APM_CHIP_1_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_2_PDI_0) && (pdi1 == LW_APM_CHIP_2_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_3_PDI_0) && (pdi1 == LW_APM_CHIP_3_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_4_PDI_0) && (pdi1 == LW_APM_CHIP_4_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_5_PDI_0) && (pdi1 == LW_APM_CHIP_5_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_6_PDI_0) && (pdi1 == LW_APM_CHIP_6_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_7_PDI_0) && (pdi1 == LW_APM_CHIP_7_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_8_PDI_0) && (pdi1 == LW_APM_CHIP_8_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_9_PDI_0) && (pdi1 == LW_APM_CHIP_9_PDI_1))   ||
              ((pdi0 == LW_APM_CHIP_10_PDI_0) && (pdi1 == LW_APM_CHIP_10_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_11_PDI_0) && (pdi1 == LW_APM_CHIP_11_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_12_PDI_0) && (pdi1 == LW_APM_CHIP_12_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_13_PDI_0) && (pdi1 == LW_APM_CHIP_13_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_14_PDI_0) && (pdi1 == LW_APM_CHIP_14_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_15_PDI_0) && (pdi1 == LW_APM_CHIP_15_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_16_PDI_0) && (pdi1 == LW_APM_CHIP_16_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_17_PDI_0) && (pdi1 == LW_APM_CHIP_17_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_18_PDI_0) && (pdi1 == LW_APM_CHIP_18_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_19_PDI_0) && (pdi1 == LW_APM_CHIP_19_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_20_PDI_0) && (pdi1 == LW_APM_CHIP_20_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_21_PDI_0) && (pdi1 == LW_APM_CHIP_21_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_22_PDI_0) && (pdi1 == LW_APM_CHIP_22_PDI_1)) ||
              ((pdi0 == LW_APM_CHIP_23_PDI_0) && (pdi1 == LW_APM_CHIP_23_PDI_1))))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }

        // Check for APM mode
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20, &apmMode));

        if (!FLD_TEST_DRF(_SEC2, _APM_VBIOS_SCRATCH, _CCM, _ENABLE, apmMode))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }

        // SKU Check
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PCIE_DEVIDA, &devId));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chipId));
        devId  = DRF_VAL( _FUSE, _OPT_PCIE_DEVIDA, _DATA, devId);
        chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

        // Adding ct_assert to alert incase if the DevId values are changed for the defines
        ct_assert(LW_PCI_DEVID_DEVICE_P1001_SKU230 == 0x20B5);

        if (!((chipId == LW_PMC_BOOT_42_CHIP_ID_GA100) && ( devId == LW_PCI_DEVID_DEVICE_P1001_SKU230)))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }
    }

    // APM Scratch check
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(
                      LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0(LW_SEC2_APM_SCRATCH_INDEX), &scratch));

    if (scratch != LW_SEC2_APM_STATUS_VALID)
    {
        flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
        goto ErrorExit;
    }

    flcnStatus = FLCN_OK;

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief Checks whether APM can be enabled or not.
 *
 * @return FLCN_ERROR if not supported
 *         else FLCN_OK
 */


FLCN_STATUS
vprCheckIfApmIsSupported_GA100
(
    void
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

    CHECK_FLCN_STATUS(_vprCheckApmEnabledHs_GA100());

ErrorExit:
    return flcnStatus;
}
#endif
