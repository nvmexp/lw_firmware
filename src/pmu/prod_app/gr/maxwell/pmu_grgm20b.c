
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
 * @file   pmu_grgm20b.c
 * @brief  HAL-interface for the GM20X Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_bar0.h"

#include "dev_top.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_fbp.h"

#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initialize the mask for GR PRIV Error Handling
 *
 * Internally till Pascal, we are blocking the GR
 * PRIV access
 */
void
grPgPrivErrHandlingInit_GM20X(void)
{
    LwU32 i;
    LwU32 maxTpcCount;
    LwU32 maxGpcCount;
    LwU32 maxTpcPerGpcCount;
    LwU32 numLTCPerFBP;

    // get max TPC count (maxGpcCount * maxTpcPerGpcCount)
    maxGpcCount = DRF_VAL(_PTOP, _SCAL_NUM_GPCS, _VALUE,
                          REG_RD32(BAR0, LW_PTOP_SCAL_NUM_GPCS));

    maxTpcPerGpcCount = DRF_VAL(_PTOP, _SCAL_NUM_TPC_PER_GPC, _VALUE,
                            REG_RD32(BAR0, LW_PTOP_SCAL_NUM_TPC_PER_GPC));

    maxTpcCount = maxGpcCount * maxTpcPerGpcCount;

    Gr.privBlock.sysLo = BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2ds_pri)      |
                         BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2pd_pri)      |
                         BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2pdb_pri)     |
                         BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2rastwod_pri) |
#if (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV) || PMU_PROFILE_GA10X_RISCV || PMU_PROFILE_GA100))
                         BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2fe_pri)     |
#endif // (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV) || PMU_PROFILE_GA10X_RISCV || PMU_PROFILE_GA100))
                         BIT(LW_PPRIV_SYS_PRI_MASTER_fecs2scc_pri);

    for (i = 0; i < maxTpcCount; i++)
    {
        Gr.privBlock.gpcLo |= BIT(LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri0+i);
    }

    Gr.privBlock.gpcHi =
        LwU64_HI32(BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2crstr_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2frstr_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2gcc_pri)        |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2gpmpd_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2gpmreport_pri)  |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2gpmsd_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2prop_pri)       |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2rasterarb_pri)  |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2setup_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2swdx_pri)       |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2tc_pri)         |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2wdxps_pri)      |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2widclip_pri)    |
                   BIT64(LW_PPRIV_GPC_PRI_MASTER_gpccs2zlwll_pri));

    numLTCPerFBP = DRF_VAL(_PTOP, _SCAL_NUM_LTC_PER_FBP, _VALUE,
                           REG_RD32(BAR0, LW_PTOP_SCAL_NUM_LTC_PER_FBP));

    Gr.privBlock.fbpLo = 0;

    for (i = 0; i < numLTCPerFBP; i++)
    {
        Gr.privBlock.fbpLo |= (BIT(LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri0+i) |
                               BIT(LW_PPRIV_FBP_PRI_MASTER_becs2rdm_pri0+i)  |
                               BIT(LW_PPRIV_FBP_PRI_MASTER_becs2zrop_pri0+i));
    }
}


/*!
 * Activate GR PRIV access Error Handling
 *
 * @param[in] bActivate    True: Enable the error for priv access,
 *                         False: Disable the error for priv access
 */
void
grPgPrivErrHandlingActivate_GM20X(LwBool bActivate)
{
    LwU32 sysPriBlockLo = 0;
    LwU32 gpcPriBlockLo = 0;
    LwU32 fbpPriBlockLo = 0;
    LwU32 gpcPriBlockHi = 0;

    sysPriBlockLo = Gr.privBlock.sysLo;
    gpcPriBlockLo = Gr.privBlock.gpcLo;
    gpcPriBlockHi = Gr.privBlock.gpcHi;
    fbpPriBlockLo = Gr.privBlock.fbpLo;

    //
    // LW_PPRIV_SYS_PRIV_FS_CONFIG is 64 bit bus - index 0 is for lower 32 bits.
    //
    if (bActivate)
    {
        sysPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0)) & ~sysPriBlockLo;
        gpcPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0)) & ~gpcPriBlockLo;
        gpcPriBlockHi =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1)) & ~gpcPriBlockHi;
        fbpPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0)) & ~fbpPriBlockLo;
    }
    else
    {
        sysPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0)) | sysPriBlockLo;
        gpcPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0)) | gpcPriBlockLo;
        gpcPriBlockHi =
            REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1)) | gpcPriBlockHi;
        fbpPriBlockLo =
            REG_RD32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0)) | fbpPriBlockLo;
    }

    // gate PRIV access
    REG_WR32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0), sysPriBlockLo);
    REG_WR32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0), gpcPriBlockLo);
    REG_WR32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1), gpcPriBlockHi);
    REG_WR32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0), fbpPriBlockLo);

    // Flush registers
    sysPriBlockLo = REG_RD32(BAR0, LW_PPRIV_SYS_PRIV_FS_CONFIG(0));
    gpcPriBlockLo = REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(0));
    gpcPriBlockHi = REG_RD32(BAR0, LW_PPRIV_GPC_PRIV_FS_CONFIG(1));
    fbpPriBlockLo = REG_RD32(BAR0, LW_PPRIV_FBP_PRIV_FS_CONFIG(0));
}

