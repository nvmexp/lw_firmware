/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_fusegm200.c
 * @brief   PMU Hal Functions for GM20X+ for fuses
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"
#include "pmu_objpcm.h"
#include "config/g_fuse_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Timeout value (in ns) for acquiring fuse mutex.
 */
#define PMU_FUSE_MUTEX_ACQ_TIMEOUT_NS                     64000 // 64us

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Access (R/W) fuse register.
 *
 * @param[in]       fuseReg     Address of the fuse register to access.
 * @param[in/out]   pFuseValue  Location storing fuse value.
 * @param[in]       bRead       Defines operation (READ / WRITE).
 */
void
fuseAccess_GM200
(
    LwU32   fuseReg,
    LwU32  *pFuseVal,
    LwBool  bRead
)
{
    LwU8        token;
    LwU32       jtagIntfc;
    FLCN_STATUS status;

    // Unlock fuses. Using GPMUTEX to avoid potential race condition with RM
    status = mutexAcquire(LW_MUTEX_ID_GPMUTEX,
                          PMU_FUSE_MUTEX_ACQ_TIMEOUT_NS,
                          &token);
    PMU_HALT_COND(status == FLCN_OK);

    jtagIntfc = fuseJtagIntfcClkUngate_HAL();

#if (PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV)
# define LW_FUSE_OPT_GC6_ISLAND_DISABLE_OLD  0x00820568 /* RW-4R */
# define LW_FUSE_OPT_SPARE_FS_OLD            0x00820398 /* RW-4R */
# define LW_FUSE_OPT_LWLINK_DISABLE_OLD      0x00820684 /* RW-4R */

    // lwbugs/2665949 , for netlists prior to NET16 patch fuse register addrs
    if (IsGPU(_GH100) && IsEmulation() && !IsNetListOrLater(16))
    {
        if (fuseReg == LW_FUSE_OPT_GC6_ISLAND_DISABLE)
        {
            fuseReg = LW_FUSE_OPT_GC6_ISLAND_DISABLE_OLD;
        }
        else if (fuseReg == LW_FUSE_OPT_SPARE_FS)
        {
            fuseReg = LW_FUSE_OPT_SPARE_FS_OLD;
        }
        else if (fuseReg == LW_FUSE_OPT_LWLINK_DISABLE)
        {
            fuseReg = LW_FUSE_OPT_LWLINK_DISABLE_OLD;
        }
    }
#endif // (PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV)

    if (bRead)
    {
        // Get the data
        *pFuseVal = REG_RD32(BAR0, fuseReg);
    }
    else
    {
        // Write the fuse data.
        REG_WR32(BAR0, fuseReg, *pFuseVal);
    }

    fuseJtagIntfcClkGateRestore_HAL(&Fuse, jtagIntfc);

    (void)mutexRelease(LW_MUTEX_ID_GPMUTEX, token);
}

/*!
 * @brief    The access to the fuse registers is controlled by the JTAG clock.
 *           As fuse registers are typically read during initialization leaving
 *           the JTAG clock gated rest of the time and only ungating/gating when
 *           needed will help to save overall power, according to HW.
 *           The JTAG clock gating feature is disabled by default and is driven
 *           from PMU. Enable the PMU_ALTER_JTAG_CLK_AROUND_FUSE_ACCESS feature
 *           for required profile(s) in pmu-config.cfg if you want this enabled.
 *
 * @returns  value read from LW_PTRIM_SYS_JTAGINTFC to be used in the restore
 */
LwU32
fuseJtagIntfcClkUngate_GM200(void)
{
    LwU32 jtagIntfc = REG_RD32(BAR0, LW_PTRIM_SYS_JTAGINTFC);
    if (FLD_TEST_DRF(_PTRIM, _SYS_JTAGINTFC, _JTAGTM_INTFC_CLK_EN, _OFF, jtagIntfc))
    {
        REG_WR32(BAR0, LW_PTRIM_SYS_JTAGINTFC,
                 FLD_SET_DRF(_PTRIM, _SYS_JTAGINTFC, _JTAGTM_INTFC_CLK_EN, _ON,
                             jtagIntfc));
    }
    return jtagIntfc;
}

/*!
 * @brief   If required, restore the JTAG clk to it's original "gating" setting
 *
 * @param[in]  jtag   Original value read from LW_PTRIM_SYS_JTAGINTFC.
 */
void
fuseJtagIntfcClkGateRestore_GM200
(
    LwU32 jtagIntfc
)
{
    // Restore the JTAG clk to it's original "gating" setting.
    if (FLD_TEST_DRF(_PTRIM, _SYS_JTAGINTFC, _JTAGTM_INTFC_CLK_EN, _OFF, jtagIntfc))
    {
        REG_WR32(BAR0, LW_PTRIM_SYS_JTAGINTFC, jtagIntfc);
    }
}
