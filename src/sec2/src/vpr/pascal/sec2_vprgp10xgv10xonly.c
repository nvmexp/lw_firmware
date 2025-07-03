/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "flcntypes.h"
#include "sec2sw.h"
#include "main.h"
#include "lw_mutex.h"
#include "lib_mutex.h"
#include "lwosselwreovly.h"
#include "vpr/sec2_vpr.h"
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"
#include "dev_fuse.h"
#include "sec2_objsec2.h"
#include "dev_fb.h"
#include "dev_ltc.h"
#include "dev_pri_ringmaster.h"
#include "dev_disp.h"
#include "dev_master.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_sec_addendum.h"
#include "sec2_bar0_hs.h"
#include "sec2_csb.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

static LwU64 __attribute__ ((noinline)) _multiplyUint32WithUint32ResultToUint64_hs(LwU32 a, LwU32 b)
                                            GCC_ATTRIB_SECTION("imem_vprHs", "multiplyUint32WithUint32ResultToUint64_hs");
/*
 * Function to multiply two 32 bit numbers to give 64 bit product
 * Need this separate function implementation because of falcon limitation
 */
static LwU64
_multiplyUint32WithUint32ResultToUint64_hs
(
    LwU32 a,
    LwU32 b
)
{
    LwU16 a1, a2;
    LwU16 b1, b2;
    LwU32 a2b2, a1b2, a2b1, a1b1;
    LwU64 result;

    /*
     * Falcon has a 16-bit multiplication instruction.
     * Break down the 32-bit multiplication into 16-bit operations.
     */
    a1 = a >> 16;
    a2 = a & 0xffff;

    b1 = b >> 16;
    b2 = b & 0xffff;

    a2b2 = a2 * b2;
    a1b2 = a1 * b2;
    a2b1 = a2 * b1;
    a1b1 = a1 * b1;

    result = (LwU64)a2b2 + ((LwU64)a1b2 << 16) +
            ((LwU64)a2b1 << 16) + ((LwU64)a1b1 << 32);
    return result;
}

/*!
 * @brief Check if requested VPR region clashes with CBC range
 */
FLCN_STATUS
vprCheckIfClashWithCbcRange_GP10X(void)
{
    LwU32 cbcBase           = 0;
    LwU64 cbcStartAddr      = 0;
    LwU64 vprMinStartAddr   = 0;
    LwU64 vprMaxEndAddr     = 0;
    LwU64 diffRegionFromCbc = 0;
    LwU32 numActiveLTCs     = 0;
    FLCN_STATUS flcnStatus  = FLCN_OK;

    //
    // Get CBC start address from base address
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PLTCG_LTCS_LTSS_CBC_BASE, &cbcBase));
    cbcBase = DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_BASE, _ADDRESS, cbcBase);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_ROP_L2, &numActiveLTCs));
    numActiveLTCs = DRF_VAL(_PPRIV, _MASTER_RING_ENUMERATE_RESULTS_ROP_L2, _COUNT, numActiveLTCs);

    cbcStartAddr = _multiplyUint32WithUint32ResultToUint64_hs(numActiveLTCs, cbcBase);
    cbcStartAddr  = cbcStartAddr << LW_PLTCG_LTC0_LTS0_CBC_BASE_ALIGNMENT_SHIFT;

    CHECK_FLCN_STATUS(vprGetMaxEndAddress_HAL(&VprHal, &vprMaxEndAddr));
    CHECK_FLCN_STATUS(vprGetMinStartAddress_HAL(&VprHal, &vprMinStartAddr));

    //
    // The CBC region can be a maximum of 384MB on Pascal, as explained below,
    // GP100 supports max 512 MB, but we dont support VPR on GP100.
    // GP102 supports max 384 MB (This is max for a VPR capable GP10X).
    // GP104 supports max 256 MB.
    // Check that:
    // 1. CBC base is not within the VPR region
    // 2. If CBC region is below the VPR region, the VPR region
    //    must start at least 384MB above the CBC base.
    // 3. If the CBC region is above the VPR region, the VPR start
    //    must be least 384MB, to prevent a wrap-around attack.
    //
    if ((cbcStartAddr >= vprMinStartAddr) &&
        (cbcStartAddr <= vprMaxEndAddr))
    {
        flcnStatus = FLCN_ERR_VPR_APP_CBC_RANGE_CLASH;
        goto ErrorExit;
    }

    diffRegionFromCbc = vprMinStartAddr - cbcStartAddr;
    if ((cbcStartAddr < vprMinStartAddr) &&
        (diffRegionFromCbc < NUM_BYTES_IN_384_MB))
    {
        flcnStatus = FLCN_ERR_VPR_APP_CBC_RANGE_CLASH;
        goto ErrorExit;
    }

    if ((cbcStartAddr > vprMaxEndAddr) &&
        (vprMinStartAddr < NUM_BYTES_IN_384_MB))
    {
        flcnStatus = FLCN_ERR_VPR_APP_CBC_RANGE_CLASH;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: VPR info can be read by querying VPR_INFO registers in MMU
 * VPR start and end address will be 4k aligned in MMU registers
 */
FLCN_STATUS
vprReadVprInfo_GP10X
(
    LwU32 index,
    LwU32 *pVal
)
{
    LwU32       cmd        = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Set return val to 0,  if we get an error it won't be updated
    LwU32       tmpVal = 0;

    // Check the out parameter for NULL
    if (pVal == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    while (LW_TRUE)
    {
        cmd = DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _INDEX,index);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_INFO, cmd));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_INFO, &cmd));

        //
        // Ensure that VPR info that we read has correct index. This is because
        // hacker can keep polling this reigster to yield invalid expected
        // value or RM also uses this register and can autoincreament
        //
        if (FLD_TEST_DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _INDEX, index, cmd))
        {
            tmpVal = DRF_VAL(_PFB, _PRI_MMU_VPR_INFO, _DATA, cmd);

            if (index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO ||
                index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_HI)
            {
                // Basically left shift by 4 bits and return value of read/write mask.
                tmpVal = DRF_NUM(_PFB, _PRI_MMU_VPR_INFO, _DATA, tmpVal);
                break;
            }
            else if (index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI)
            {
                break;
            }
            else
            {
                //
                // If we are here, then the coder needs to be made aware of the fact that
                // the index they are trying to use should be added to one of the if/else
                // conditions above, otherwise this function does not support that index.
                // Error report and halt so this is brought to immediate attention.
                //
                vprWriteStatusToFalconMailbox_HAL(&VprHal, FLCN_ERR_VPR_APP_ILWALID_INDEX);
                falc_halt();
            }
        }
    }

    // Update return val
    *pVal = tmpVal;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: WPR info can be read by querying WPR_INFO registers in MMU
 */
FLCN_STATUS
vprReadWprInfo_GP10X
(
    LwU32 index,
    LwU32 *pVal
)
{
    LwU32       cmd        = 0;
    LwU32       tmpVal     = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Set return val to 0,  if we get an error it won't be updated
    tmpVal = 0;

    // Check the out parameter for NULL
    if (pVal == NULL)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    while (LW_TRUE)
    {
        cmd = DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_INFO, cmd));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_WPR_INFO, &cmd));

        //
        // Ensure that WPR info that we read has correct index. This is because
        // hacker can keep polling this register to yield invalid expected
        // value or RM also uses this register and can auto increment
        //
        if (FLD_TEST_DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index, cmd))
        {
            tmpVal = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, cmd);

            if (index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_VPR_PROTECTED_MODE)
            {
                // Basically left shift by 4 bits and return value of read/write mask.
                tmpVal = DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _DATA, tmpVal);
                break;
            }
            else if (index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO ||
                     index == LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI)
            {
                break;
            }
            else
            {
                //
                // If we are here, then the coder needs to be made aware of the fact that
                // the index they are trying to use should be added to one of the if/else
                // conditions above, otherwise this function does not support that index.
                // Error report and halt so this is brought to immediate attention.
                //
                vprWriteStatusToFalconMailbox_HAL(&VprHal, FLCN_ERR_VPR_APP_ILWALID_INDEX);
                falc_halt();
            }
        }
    }

    // Update return val
    *pVal = tmpVal;

ErrorExit:
    return flcnStatus;
}

/*!
 * This function takes a VPR or WPR index and data pair, and programs that
 * information in MMU. All VPR & WPR indexes listed for the register
 * LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX* are supported.
 * Prereq - Before calling this function we need to acquire mutex for VPR_WPR_WRITE register
 *          i.e. mutex LW_MUTEX_ID_MMU_VPR_WPR_WRITE
 */
FLCN_STATUS
vprWriteVprWprData_GP10X
(
    LwU32 index,
    LwU32 data
)
{
    LwU32       cmd         = 0;
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       readInfo    = 0;

    if (index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO         ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI         ||
        index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_PROTECTED_MODE)
    {
        cmd = data;
    }
    else if (index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO  ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI  ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO ||
             index == LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI)
    {
        cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _DATA, data, cmd);
    }
    else
    {
        //
        // If we are here, then the coder needs to be made aware of the fact that
        // the index they are trying to use should be added to one of the if/else
        // conditions above, otherwise this function does not support that index.
        // Error report and halt so this is brought to immediate attention.
        //
        vprWriteStatusToFalconMailbox_HAL(&VprHal, FLCN_ERR_VPR_APP_ILWALID_INDEX);
        falc_halt();
    }

    cmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _INDEX, index, cmd);

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_WPR_WRITE, cmd));

    //
    // Validate MMU_VPR_WPR_WRITE register write by reading back
    //
    switch(index)
    {
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_PROTECTED_MODE:
            CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_VPR_PROTECTED_MODE, &readInfo));
            if (readInfo != data)
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO:
            CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO, &readInfo));
            if (readInfo != (data & MASK_MAX_4KB_MEMLOCK_ADDR_MMU))
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI:
            CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI, &readInfo));
            if (readInfo != (data & MASK_MAX_4KB_MEMLOCK_ADDR_MMU))
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO:
            CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO, &readInfo));
            if (readInfo != data)
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI:
            CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_HI, &readInfo));
            if (readInfo != data)
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO:
            CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO, &readInfo));
            if ((readInfo >> MB_ALIGN_TO_4K_ALIGN_SHIFT) != (data >> MB_ALIGN_TO_4K_ALIGN_SHIFT))
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        case LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI:
            CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI, &readInfo));
            if ((readInfo >> MB_ALIGN_TO_4K_ALIGN_SHIFT) != (data >> MB_ALIGN_TO_4K_ALIGN_SHIFT))
            {
                flcnStatus = FLCN_ERR_VPR_APP_VPR_WPR_WRITE_FAILED;
            }
            break;
        default:
            //
            // If we are here, then the coder needs to be made aware of the fact that
            // the index they are trying to use should be added to one of the if/else
            // conditions above, otherwise this function does not support that index.
            // Error report and halt so this is brought to immediate attention.
            //
            flcnStatus = FLCN_ERR_VPR_APP_ILWALID_INDEX;
            break;
    }

ErrorExit:
    if (flcnStatus != FLCN_OK)
    {
        vprWriteStatusToFalconMailbox_HAL(&VprHal, flcnStatus);
        falc_halt();
    }
    return flcnStatus;
}

/*!
 * @brief: Validate and set mem lock range for write protection before scrubbing
 *         Pre-condition : Need to acquire mutex LW_MUTEX_ID_MMU_VPR_WPR_WRITE before calling this function
 *                         since we access VPR_WPR_WRITE register to setup memlock range
 */
FLCN_STATUS
vprSetMemLockRange_GP10X
(
    LwU64   startAddrInBytes,
    LwU64   endAddrInBytes
)
{
    LwU32 memlockLwrrStartAddr4KAligned;
    LwU32 memlockLwrrEndAddr4KAligned;
    LwU32 startAddr4KAligned;
    LwU32 endAddr4KAligned;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Right shift to get address in 4K unit i.e LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT
    startAddr4KAligned = startAddrInBytes >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;
    endAddr4KAligned   = endAddrInBytes   >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;

    if (startAddr4KAligned > endAddr4KAligned)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Check if mem lock range is already in use
    CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_LO, &memlockLwrrStartAddr4KAligned));
    CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_LOCK_ADDR_HI, &memlockLwrrEndAddr4KAligned));

    //
    // Here we are doing comparison between 4K aligned addresses to avoid compiler issue of comparing two 64 bit numbers
    // Also since memlock range is 4k aligned, this is not an issue functionally
    //
    if (memlockLwrrEndAddr4KAligned >= memlockLwrrStartAddr4KAligned)
    {
        flcnStatus =  FLCN_ERR_VPR_APP_MEMLOCK_ALREADY_SET;
        goto ErrorExit;
    }
    else
    {
        CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO, startAddr4KAligned));
        CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI, endAddr4KAligned));
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Release memory lock after scrubbing
 */
FLCN_STATUS
vprReleaseMemlock_GP10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_LO, ILWALID_MEMLOCK_RANGE_ADDR_LO));
    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_LOCK_ADDR_HI, ILWALID_MEMLOCK_RANGE_ADDR_HI));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Enable VPR in MMU
 */
FLCN_STATUS
vprEnableVprInMmu_GP10X
(
    LwBool bEnableVpr
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       cyaLo;

    CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_CYA_LO, &cyaLo));
    cyaLo = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_CYA_LO_IN_USE, bEnableVpr, cyaLo);
    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO, cyaLo));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Setup VPR in MMU
 */
FLCN_STATUS
vprSetupVprRangeInMmu_GP10X
(
    LwU64 startAddrInBytes,
    LwU64 endAddrInBytes
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO,
        (LwU32)(startAddrInBytes >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT)));
    CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI,
        (LwU32)(endAddrInBytes >> LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT)));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Miscellaneous settings based on if VPR is enabled or not
 */
FLCN_STATUS
vprUpdateMiscSettingsIfEnabled_GP10X
(
    LwBool bEnabled
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Update DISP_CRC
    CHECK_FLCN_STATUS(vprUpdateDispCrc_HAL(&VprHal, bEnabled));

    if (bEnabled)
    {
        LwU32 protectedMode;

        CHECK_FLCN_STATUS(vprReadWprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_WPR_INFO_INDEX_VPR_PROTECTED_MODE, &protectedMode));
        protectedMode = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_WPR_WRITE, _VPR_NON_COMPRESSIBLE, _TRUE, protectedMode);
        CHECK_FLCN_STATUS(vprWriteVprWprData_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_PROTECTED_MODE, protectedMode));
    }
    else
    {
        CHECK_FLCN_STATUS(vprClearDisplayBlankingPolicies_HAL(&VprHal));
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief vprGetLwrVprRangeInBytes_GP10X: 
 *  Get the current VPR Range
 *
 * @param [out] pVprStartAddrInBytes   Start address of VPR
 * @param [out] pVprEndAddrInBytes     End address of VPR
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprGetLwrVprRangeInBytes_GP10X
(
    LwU64 *pVprStartAddrInBytes,
    LwU64 *pVprEndAddrInBytes
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32 regVal = 0;

    if (!pVprStartAddrInBytes || !pVprEndAddrInBytes)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    // Read VPR Start Address and align to byte boundary
    CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO, &regVal));
    *pVprStartAddrInBytes = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;

    // Read VPR End Address and align to byte boundary
    CHECK_FLCN_STATUS(vprReadVprInfo_HAL(&VprHal, LW_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI, &regVal));
    *pVprEndAddrInBytes  = (LwU64)regVal << LW_PFB_PRI_MMU_VPR_INFO_ADDR_ALIGNMENT;
    *pVprEndAddrInBytes |= ((NUM_BYTES_IN_1_MB) - 1);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief vprSetVprOffsetInBytes_GP10X: 
 *  Set the VPR offset in Bytes
 *
 * @param [out] pVprOffsetInBytes   
 * @param [in]  vprStartAddrIn4K     Start address of VPR (4k Aligned)
 *
 * @return FLCN_OK on success
 */
FLCN_STATUS
vprSetVprOffsetInBytes_GP10X
(
    LwU64 *pVprOffsetInBytes,
    LwU64 vprStartAddrIn4K
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (!pVprOffsetInBytes)
    {
        flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
        goto ErrorExit;
    }

    *pVprOffsetInBytes = vprStartAddrIn4K << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;

ErrorExit:
    return flcnStatus;
}
