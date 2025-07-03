/*
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2tu10xonly.c
 * @brief  SEC2 HAL Functions for TU10X only
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"

//#include "lwosselwreovly.h"
#include "lwdevid.h"
#include "lw_mutex.h"
#include "lib_mutex.h"

#include "dev_master.h"
#include "sec2_bar0.h"
//#include "sec2_csb.h"
#include "dev_therm.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_sys.h"
//#include "dev_fuse.h"
//#include "dev_fifo.h"
//#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */


/*!
 * @brief: WAR to allow CPU(level0) to access FBPA_PM registers
 * On Turing, we have protected FBPA registers to L2 writable using PLM LW_PFB_FBPA_0_PM_PRIV_LEVEL_MASK
 * But in devtools SW stack, register LW_PFB_FBPA_*_PM needs to be programmed by level0 SW running on CPU
 * Also, from security perspective it is fine if we do not keep these FBPA_PM registers protected
 * Because of this problem, devtools is lwrrently broken on Turing on production SKUs where this PLM is raised via fuse
 * To resolve this, the WAR is to raise the CPU/XVE(devtools) accesses to level2 using 3 decode traps(12,13,14)
 * Bug 2369597 has details
 */
FLCN_STATUS
sec2ProvideAccessOfFbpaRegistersToCpuWARBug2369597Hs_TU10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    FLCN_STATUS tmpStatus    = FLCN_OK;
    LwU8        mutexToken   = LW_MUTEX_OWNER_ID_ILWALID;
    LwU32       trap12Action = 0;
    LwU32       trap13Action = 0;
    LwU32       trap14Action = 0;

    //
    // We need to raise access for three range of registers (8 LW_PFB_FBPA_*_PM, 3 LW_PFB_FBPA_MC_*_PM, and 1 LW_PFB_FBPA_PM)
    // using 3 decode traps as per http://lwbugs/2369597/97
    // Since we are using 3 decode traps which are costly resource, we want to have a mechanism in place
    // such that we can reclaim these decode traps if we have higher priority issue than this one.
    // For this, we are acquiring a level3 mutex and checking decode trap ACTION to crosscheck if it is already programmed or not
    //

    // Acquire mutex before programming 3 decode traps(12,13,14) for bug 2369597
    CHECK_FLCN_STATUS(mutexAcquire_hs(LW_MUTEX_ID_DECODE_TRAPS_WAR_TU10X_BUG_2369597, LW_MUTEX_ACQUIRE_TIMEOUT_NS, &mutexToken));

    //
    // Crosscheck if decode traps are already in use, otherwise fail with error
    // Comparing with value Zero which is safe for Turing, filed HW bug 200455412 to get DECODE_TRAP_ACTION_ALL_ACTIONS_DISABLED define added
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION, &trap12Action));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION, &trap13Action));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION, &trap14Action));
    if ((trap12Action != 0) || (trap13Action != 0) || (trap14Action != 0))
    {
        //
        // In case of driver unload-reload, the traps would already be programmed so we cannot fail here.
        // Instead we can cross check each and every setting, but it will be an overkill in HS code for a trivial issue of lowering security
        // So for now, just silently skipping the decode trap settings
        //
        // flcnStatus = FLCN_ERR_HS_DECODE_TRAP_ALREADY_IN_USE;
        goto ErrorExit;
    }

    //
    // Setting up decode trap to raise cpu access to level2 for 8 LW_PFB_FBPA_*_PM registers
    // Make sure that we program all registers of decode trap12
    //

    // Set trap on PL0 access
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP12_MATCH_CFG, _TRAP_APPLICATION_LEVEL0, _ENABLE)));

    // Set DATA1 such that resultant priv access level is 2
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP12_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_2)));

    // Set trap on LW_PFB_FBPA_*_PM, LW_PFB_FBPA_MC_*_PM, and LW_PFB_FBPA_PM registers
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP12_MATCH, _ADDR, LW_PFB_FBPA_0_PM)));

    // Set trap on all FBPs as well as broadcast and multicast versions of the register and all subids
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP12_MASK, _ADDR,  0x1c000) |
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP12_MASK, _SUBID, 0x3f)));

    // Scrub DATA2 to make sure it does not have any stale data
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP12_DATA2, _V, _I)));

    // Set decode trap ACTION to SET_PRIV_LEVEL
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP12_ACTION, _SET_PRIV_LEVEL, _ENABLE)));

    //
    // Setting up decode trap to raise cpu access to level2 for 3 LW_PFB_FBPA_MC_*_PM registers to level2
    // This also protects register address 0x0098C100 which is unused on Turing
    // Make sure that we program all registers of decode TRAP13
    //

    // Set trap on PL0 access
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP13_MATCH_CFG, _TRAP_APPLICATION_LEVEL0, _ENABLE)));

    // Set DATA1 such that resultant priv access level is 2
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP13_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_2)));

    // Set trap on LW_PFB_FBPA_*_PM, LW_PFB_FBPA_MC_*_PM, and LW_PFB_FBPA_PM registers
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP13_MATCH, _ADDR, LW_PFB_FBPA_MC_0_PM)));

    // Set trap on all FBPs as well as broadcast and multicast versions of the register and all subids
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP13_MASK, _ADDR,  0xc000) |
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP13_MASK, _SUBID, 0x3f)));

    // Scrub DATA2 to make sure it does not have any stale data
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP13_DATA2, _V, _I)));

    // Set decode trap ACTION to SET_PRIV_LEVEL
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP13_ACTION, _SET_PRIV_LEVEL, _ENABLE)));

    //
    // Setting up decode trap to raise cpu access to level2 for one register LW_PFB_FBPA_PM to level2
    // Make sure that we program all registers of decode TRAP14
    //

    // Set trap on PL0 access
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH_CFG, _TRAP_APPLICATION_LEVEL0, _ENABLE)));

    // Set DATA1 such that resultant priv access level is 2
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP14_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_2)));

    // Set trap on LW_PFB_FBPA_*_PM, LW_PFB_FBPA_MC_*_PM, and LW_PFB_FBPA_PM registers
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH, _ADDR, LW_PFB_FBPA_PM)));

    // Set trap on all FBPs as well as broadcast and multicast versions of the register and all subids
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK,
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP14_MASK, _ADDR,  0x0) |
                            DRF_NUM( _PPRIV, _SYS_PRI_DECODE_TRAP14_MASK, _SUBID, 0x3f)));

    // Scrub DATA2 to make sure it does not have any stale data
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP14_DATA2, _V, _I)));

    // Set decode trap ACTION to SET_PRIV_LEVEL
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION,
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP14_ACTION, _SET_PRIV_LEVEL, _ENABLE)));

ErrorExit:
    // Don't overwrite an earlier failure, after ErrorExit

    // Release mutex after accessing TARGET_MASK registers
    if (mutexToken != LW_MUTEX_OWNER_ID_ILWALID)
    {
        tmpStatus = mutexRelease_hs(LW_MUTEX_ID_DECODE_TRAPS_WAR_TU10X_BUG_2369597, mutexToken);
        if (flcnStatus == FLCN_OK)
        {
            flcnStatus = tmpStatus;
        }
    }
    return flcnStatus;
}

/*!
 * @brief: On few boards of TU102(DevIds 0x1E07 and 0x1E04) and TU106(DevIds 0x1F02 and 0x1F07),
 * we need to override level3 TSOSC register settings done by VBIOS to avoid thermal HW slowdown before SW slowdown
 * This is required to be done here because the issue was found later where VBIOS update was not possible
 * WARNING : It is not ideal to add such WAR here, So please do not update/reuse/copy this code
 * This function should be deprecated once everyone moves to new VBIOS
 * Details are in bug 2369687(TU102) and 2379506(TU106)
 */
FLCN_STATUS
sec2UpdateTsoscSettingsWARBug2369687And2379506And2460727Hs_TU10X(void)
{
    FLCN_STATUS flcnStatus      = FLCN_OK;
    LwU32       chipId          = 0;
    LwU32       devId           = 0;
    LwBool      bNeedToApplyWar = LW_FALSE;

    //
    // This WAR is required only for few boards of TU102 with devId's 0x1E04 and 0x1E07
    // and TU106 with devId's 0x1F02 and 0x1F07
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PCIE_DEVIDA, &devId));
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chipId));
    devId  = DRF_VAL( _FUSE, _OPT_PCIE_DEVIDA, _DATA, devId); 
    chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

    // Adding ct_assert to alert incase if the DevId values are changed for the defines
    ct_assert(LW_PCI_DEVID_DEVICE_TU102_PG150_SKU30 == 0x1E07);
    ct_assert(LW_PCI_DEVID_DEVICE_TU102_PG150_SKU31 == 0x1E04);
    ct_assert(LW_PCI_DEVID_DEVICE_TU106_PG160_SKU50 == 0x1F02);
    ct_assert(LW_PCI_DEVID_DEVICE_TU106_PG160_SKU52 == 0x1F07);
    ct_assert(LW_PCI_DEVID_DEVICE_TU106_PG160_SKU40 == 0x1F08);

    //
    // These SKUs (PG160_SKU42 and PG160_SKU43) share the same DevID as PG160_SKU40,
    // adding ct_asserts for these to ensure the patch below will still apply to them
    //
    ct_assert(LW_PCI_DEVID_DEVICE_TU106_PG160_SKU42 == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU40);
    ct_assert(LW_PCI_DEVID_DEVICE_TU106_PG160_SKU43 == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU40);

    // Check if WAR is required
    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            if ((devId == LW_PCI_DEVID_DEVICE_TU102_PG150_SKU30) ||
                (devId == LW_PCI_DEVID_DEVICE_TU102_PG150_SKU31))
            {
                bNeedToApplyWar = LW_TRUE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            if ((devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU50) ||
                (devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU52) ||
                (devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU40))
            {
                bNeedToApplyWar = LW_TRUE;
            }
            break;
        default:
            goto ErrorExit;
    }

    if (bNeedToApplyWar)
    {
        // Update Tsosc index register so that we can program indexed tsosc registers incrementally
        LwU32 tsoscIndexReg = 0;
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_THERM_GPC_TSOSC_INDEX, &tsoscIndexReg));
        tsoscIndexReg = FLD_SET_DRF( _THERM, _GPC_TSOSC_INDEX, _VALUE, _MIN, tsoscIndexReg);
        tsoscIndexReg = FLD_SET_DRF( _THERM, _GPC_TSOSC_INDEX, _WRITEINCR, _ENABLED, tsoscIndexReg);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_INDEX, tsoscIndexReg));

        //
        // TSOSC hotspot offset register updates
        // These values are obtained by vbios vbod session CL 24930122(TU102) and CL 24969083(TU106) by thermal team
        // Not doing R-M-W of register here as this WAR is only required for TU102 and TU106 for which
        // we are already programming all required fields of it
        //
        // Keeping this WAR here have below security concern,
        // Security issue 1: 
        //      HW provides 2 registers to update Thermal sensor TSOSC values, INDEX and DATA.
        //      Here the INDEX register is not protected, but DATA register is protected at level3.
        //      SEC2 writes INDEX register once (and sets it to auto-increment mode), then writes the DATA register 6 times.
        //      No matter what SEC2 does, INDEX can still change behind its back.
        //      If the same thing is done in VBIOS, the attack surface is less since the VBIOS runs out of IFR
        //      on Turing as soon as chip gets reset. For an attacker, racing with SEC2is far easier than racing with VBIOS.
        //      Moving this fix to driver is making it easier to exploit.
        // Security issue 2: 
        //      Incorrect programming of these registers could lead to any kind of 
        //      related/unrelated security bug other units of HW
        //
        // For this we have signoff from HW team that
        // it will not cause HW damage or any other security concern since we are not programming these values to 0
        //

        //
        // TSOSC_HS_OFFSET register settings are different for TU102 and TU106
        // 6 GPCs are supported on TU102, while 3 are supported on TU106
        //
        if (chipId == LW_PMC_BOOT_42_CHIP_ID_TU102)
        {
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xC00));
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xB00));
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xD08));
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xC00));
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xD58));
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xD58));
        }
        else if (chipId == LW_PMC_BOOT_42_CHIP_ID_TU106)
        {
            if ((devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU50) ||
                (devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU52))
            {
                // Perforce Checkin 24969083.2006 BSS change affecting PG160 SKUs 50 and 52
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xE98));
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xB48));
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xF60));
            }
            else if (devId == LW_PCI_DEVID_DEVICE_TU106_PG160_SKU40)
            {
                // Perforce Checkin 25419695.2006 BSS change affecting PG160 SKUs 40, 42, and 43
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xA80));
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xB60));
                CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_HS_OFFSET, 0xB60));
            }
        }

        // Disable auto write increment of Tsosc index register
        tsoscIndexReg = FLD_SET_DRF( _THERM, _GPC_TSOSC_INDEX, _WRITEINCR, _DISABLED, tsoscIndexReg);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_GPC_TSOSC_INDEX, tsoscIndexReg));
    }

ErrorExit:
    return flcnStatus;
}

