/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfga10x.c
 * @brief  Light Secure Falcon (LSF) GA10X Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_fuse.h"

#ifdef UPROC_RISCV
# include "sbilib/sbilib.h"
#endif // UPROC_RISCV

/* ------------------------ Application includes --------------------------- */
#include "pmu_objpmu.h"
#include "rmlsfm.h"
#include "config/g_lsf_private.h"
#ifdef UPROC_RISCV
#include "riscv_csr.h"
#endif // UPROC_RISCV

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Verifies this Falcon is running in secure mode.
 */
LwBool
lsfVerifyFalconSelwreRunMode_GA10X(void)
{
#ifdef UPROC_RISCV
    extern LwU32 dmaIndex;
    extern LwU64 mmWprId;

    // Verify that U mode code is allowed to run at L2
    if ((DRF_VAL64(_RISCV_CSR, _SPM, _UPLM, csr_read(LW_RISCV_CSR_SPM)) &
         LWBIT64(2)) != 0U)
    {
        //
        // Verify that S mode is running at L2
        // (this code should run in S mode at pre-init)
        //
        if (DRF_VAL64(_RISCV, _CSR_SRSP, _SRPL, csr_read(LW_RISCV_CSR_SRSP)) ==
            LW_RISCV_CSR_SRSP_SRPL_LEVEL2)
        {
            LwU32 wprId = REG_FLD_RD_DRF(CSB, _CPWR, _RISCV_BCR_DMACFG_SEC, _WPRID);

            if (wprId == (LwU32)mmWprId)
            {
                //
                // MMINTS-TODO: move this out to lsfInit_* or to another function.
                // This function should be doing validation, not configuration!
                //
                LwU32 i;

                //
                // Program the PMU DMA idx to WPRID, clear all others to default.
                // Use TRANSCFG size as the guideline because REGIONCFG and TRANSCFG
                // should match in number (e.g. 8 on GA10X)
                //
                for (i = 0; i < LW_CPWR_FBIF_TRANSCFG__SIZE_1; i++)
                {
                    if (i == dmaIndex)
                    {
                        riscvFbifRegionCfgSet(i, wprId);
                    }
                    else
                    {
                        riscvFbifRegionCfgSet(i, 0);
                    }
                }

                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
#else // UPROC_RISCV
    return lsfVerifyFalconSelwreRunMode_GM20X();
#endif // UPROC_RISCV
}

/*!
 * @brief Setup aperture settings (protected TRANSCONFIG regs)
 */
void
lsfInitApertureSettings_GA10X(void)
{
#ifdef UPROC_RISCV
    //
    // The following setting updates only take affect if the
    // the PMU is LSMODE && UCODE_LEVEL > 0
    //

    // UCODE aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_UCODE,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic PHYS_VID aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_PHYS_VID,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    // Generic COHERENT_SYSMEM aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_PHYS_SYS_COH,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    // Generic NONCOHERENT_SYSMEM aperture
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_PHYS_SYS_NCOH,
             (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _NONCOHERENT_SYSMEM)));

#else // UPROC_RISCV
    lsfInitApertureSettings_GM20X();
#endif // UPROC_RISCV
}
