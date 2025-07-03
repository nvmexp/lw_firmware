/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfgv11x.c
 * @brief  Light Secure Falcon (LSF) GV11X Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
/* ------------------------ Application includes --------------------------- */
#include "rmlsfm.h"

#ifdef UPROC_RISCV
# include "engine.h"
# include "sbilib/sbilib.h"
#endif // UPROC_RISCV

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Setup aperture settings (protected TRANSCONFIG regs)
 */
void
lsfInitApertureSettings_GA10B(void)
{
    //
    // The following setting updates only take affect if the
    // the PMU is LSMODE && UCODE_LEVEL > 0
    //

#ifdef UPROC_RISCV
    // Generic COHERENT_SYSMEM aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_PHYS_SYS_COH,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_PHYS_SYS_NCOH,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));

#else // UPROC_RISCV
    // MMINTS-TODO: delete this once the ga10b falcon profile is removed!

    // UCODE aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_UCODE),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));

    // Generic PHYS_VID aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_VID),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_COH),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_PHYS_SYS_NCOH),
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));
#endif // UPROC_RISCV
}

