/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmuga10x.c
 * @brief   PMU HAL functions for GA10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "pmu_bar0.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#ifdef UPROC_RISCV
#include "riscv_csr.h"
#endif

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "perf/cf/perf_cf.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief  Validate the allowed core privilege levels
 *         (make sure PMU is not set to level 1 or level 3).
 *
 * @return  FLCN_OK    if the allowed core priv levels are legal for PMU,
 *          FLCN_ERROR otherwise
 */
FLCN_STATUS
pmuPrivLevelValidate_GA10X(void)
{
#ifdef UPROC_RISCV
    LwU64 uplm = DRF_VAL64(_RISCV_CSR, _SPM, _UPLM, csr_read(LW_RISCV_CSR_SPM));
    LwU64 srpl;

    // MMINTS-TODO: fork this for Falcon to check initial SCTL value

    //
    // We only permit level 2 and level 0 for PMU User mode. These are
    // encoded as LW_RISCV_CSR_SPM_UPLM_LEVEL2 (level 2 and level 0 privs)
    //
    if ((uplm & ~((LwU64)LW_RISCV_CSR_SPM_UPLM_LEVEL2)) != 0U)
    {
        // If some bit is set in UPLM besides level 2 and level 0,
        // SPM is not set correctly!
        return FLCN_ERROR;
    }

    //
    // SRSP is set by sepkern before bootup, and we don't expect to change it,
    // so check it instead of SSPM
    //
    srpl = DRF_VAL64(_RISCV, _CSR_SRSP, _SRPL, csr_read(LW_RISCV_CSR_SRSP));

    if ((srpl == LW_RISCV_CSR_SRSP_SRPL_LEVEL1) ||
        (srpl == LW_RISCV_CSR_SRSP_SRPL_LEVEL3))
    {
        return FLCN_ERROR;
    }

    // The priv level of S mode can't be less than the priv level of U mode!
    if (((uplm & LW_RISCV_CSR_SPM_UPLM_LEVEL2) > LW_RISCV_CSR_SPM_UPLM_LEVEL0) &&
        (srpl < LW_RISCV_CSR_SRSP_SRPL_LEVEL2))
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
#else // UPROC_RISCV
    return FLCN_ERROR;
#endif // UPROC_RISCV
}

/*!
 * @brief Set the user-mode privilege level
 *
 * @param[in]  privLevelExt  EXT privilege level
 * @param[in]  privLevelCsb  CSB privilege level
 */
void
pmuFlcnPrivLevelSet_GA10X
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
#ifdef UPROC_RISCV
    LwU8 privLevel = LW_MAX(privLevelCsb, privLevelExt);

    // Drop priv level to 0, we can always do that.
    csr_clear(LW_RISCV_CSR_RSP, DRF_SHIFTMASK64(LW_RISCV_CSR_RSP_URPL));

    // Raise priv level if possible
    if ((DRF_VAL64(_RISCV_CSR, _SPM, _UPLM, csr_read(LW_RISCV_CSR_SPM)) & LWBIT64(privLevel)) != 0U)
    {
        csr_set(LW_RISCV_CSR_RSP, DRF_NUM64(_RISCV_CSR, _RSP, _URPL, privLevel));
    }
#else // UPROC_RISCV
    pmuFlcnPrivLevelSet_GM20X(privLevelExt, privLevelCsb);
#endif // UPROC_RISCV
}

/*!
 * @brief   Cache if supervisor is in LS mode, and default privilege level of falcon,
 *          and configure reset PLM.
 */
void
pmuPreInitFlcnPrivLevelCacheAndConfigure_GA10X(void)
{
    // Cache if in LS mode
#ifdef UPROC_RISCV
    Pmu.bLSMode = (DRF_VAL64(_RISCV, _CSR_SRSP, _SRPL, csr_read(LW_RISCV_CSR_SRSP))) ==
                  LW_RISCV_CSR_SRSP_SRPL_LEVEL2;

    //
    // Raise priv level masks so that RM doesn't mess around with our operation
    // while we're running
    //
    pmuPreInitRaisePrivLevelMasks_HAL();
    pmuPreInitRaiseExtPrivLevelMasks_HAL();
#else // UPROC_RISCV
    pmuPreInitFlcnPrivLevelCacheAndConfigure_GM20X();
#endif // UPROC_RISCV
}

/*!
 * @brief More generic interface to lower priv level masks when we unload.
 *
 * Before lowering PLMs, this will also do memtune throttling if required, and
 * stall engine interfaces via ENGCTL (if this is not GC6 detach).
 *
 * This MUST be called in ISR or atomic (critical section) context!
 *
 * @param[in]  bIsGc6Detach  indicates if we're in GC6 detach path.
 *                           In GC6 detach path, we don't want to stall engine interfaces.
 */
void
pmuDetachUnloadSequence_GA10X(LwBool bIsGc6Detach)
{
    //
    // Trigger notification to the Memory Tuning Controller to take the necessary
    // actions on PMU exception events.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        perfCfMemTuneControllerPmuExceptionHandler_HAL();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LOWER_PMU_PRIV_LEVEL))
    {
        //
        // Lower the priv level of external units before we STALLREQ PMU
        // and lose access to BAR0
        //
        pmuLowerExtPrivLevelMasks_HAL();
    }

    if (!bIsGc6Detach)
    {
        //
        // If this not a GC6 detach, we can shut off all external interfaces via ENGCTL.
        // In GC6 detach we can't affort to do that since we need to perform some BAR0 accesses.
        // However, on GA10X in GC6 detach path ACR-unload will clear the decode trap on ENGCTL,
        // so RM will be able to stall PMU's external interfaces before resetting if it needs to.
        //
        //
        // MMINTS-TODO: we need to add the PLM for FALCON_ENGCTL to
        // pmuRaisePrivLevelMasks_HAL/pmuLowerPrivLevelMasks_HAL on AD10X+.
        // On GA10X, this doesn't have a PLM, so for DoS protection we rely on
        // SEC2 to set up a decode trap before PMU's exelwtion (that then gets
        // cleaned up by ACR-unload).
        //

        // Issue stall in ENGCTL.
        REG_WR32(CSB, LW_CPWR_FALCON_ENGCTL, DRF_DEF(_PPWR, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE));

        // Wait for ACK bit to be set.
        while (FLD_TEST_DRF(_CPWR, _FALCON_ENGCTL, _STALLACK, _FALSE, REG_RD32(CSB, LW_CPWR_FALCON_ENGCTL)))
        {
            // NOP
        }

        // As an additional check, make sure all external subunits are halted (as per reset sequence in IAS)
        while (!FLD_TEST_DRF_NUM(_CPWR, _FALCON_FHSTATE, _EXT_HALTED, 0x7FFF,
                                 REG_RD32(CSB, LW_CPWR_FALCON_FHSTATE)))
        {
            // NOP
        }

        //
        // At this point (if this is not GC6 exit) all of PMU's external interfaces should be stalled,
        // so it's safe for RM to reset us!
        //
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_LOWER_PMU_PRIV_LEVEL))
    {
        // Lower the priv level of secure reset so that RM can reset PMU.
        pmuLowerPrivLevelMasks_HAL();
    }
}
