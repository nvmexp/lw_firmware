/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_regime_10.c
 * @brief  Clock code related to the V 1.0 AVFS regime logic and clock programming.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
static FLCN_STATUS s_clkNafllDvcoMinCompute(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, LwU32 voltageuV)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "s_clkNafllDvcoMinCompute");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Inject a VF change as requested for the given NAFLL domain
 *
 * @param[in] pClkList  Pointer to RM_PMU_CLK_CLK_DOMAIN_LIST object to process.
 * @param[in] pVoltList Pointer to RM_PMU_VOLT_VOLT_RAIL_LIST object to process.
 *
 * @return FLCN_OK
 *      Object succesfully processed
 * @return other
 *      Propagates return values from various calls
 */
FLCN_STATUS
clkVfChangeInject
(
    RM_PMU_CLK_CLK_DOMAIN_LIST *pClkList,
    RM_PMU_VOLT_VOLT_RAIL_LIST *pVoltList
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        clkIdx;
    LwU32       nafllIdMask;
    LwU8        nafllId;
    RM_PMU_CLK_FREQ_TARGET_SIGNAL target = {{0}};

    //
    // Attach all the IMEM overlays required.
    // imem_perf
    // imem_libPerf
    // imem_perfClkAvfs
    // imem_libClkFreqController
    // imem_libClkFreqCountedAvg
    // imem_clkLibCntr
    // imem_clkLibCntrSwSync
    // imem_libLw64
    // imem_libLw64Umul
    // imem_perfVfe
    // imem_libLwF32
    // imem_libVoltApi     - Patch CLK_SET_INFO - clkNafllLutVoltageGet
    //

    // Create array list all the IMEM overlays required.
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        for (clkIdx = 0; clkIdx < pClkList->numDomains; clkIdx++)
        {
            // Get mask of NAFLLs for the given clock domain
            status = clkNafllDomainToIdMask(
                        pClkList->clkDomains[clkIdx].clkDomain,
                        &nafllIdMask);
            if (status != FLCN_OK)
            {
                goto clkVfChangeInject_exit;
            }

            // Program all NAFLLs
            FOR_EACH_INDEX_IN_MASK(32, nafllId, nafllIdMask)
            {
                CLK_NAFLL_DEVICE *pNafllDev = clkNafllDeviceGet(nafllId);
                if (pNafllDev == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto clkVfChangeInject_exit;
                }

                if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
                {
                    //
                    // The number of rails available during clkVfChangeInject is
                    // smaller than the number available in other places.
                    // Need to perform a second check here to avoid buffer overflow.
                    //
                    if (pNafllDev->railIdxForLut >= RM_PMU_VF_INJECT_MAX_VOLT_RAILS)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_STATE;
                        goto clkVfChangeInject_exit;
                    }
                }

                // Override ADC only in case of simulation/emulation where the voltage is not modeled.
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_10) &&
                    (IsEmulation() || IsSimulation()))
                {
                    CLK_ADC_DEVICE *pAdcDev = pNafllDev->pLogicAdcDevice;
                    status = clkAdcPreSiliconSwOverrideSet(pAdcDev,
                                pVoltList->rails[pNafllDev->railIdxForLut].voltageuV,
                                LW2080_CTRL_CLK_ADC_CLIENT_ID_RM,
                                LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        goto clkVfChangeInject_exit;
                    }
                }

                // Build the target signal based on input request
                target.super.freqKHz  = pClkList->clkDomains[clkIdx].clkFreqKHz;
                PMU_HALT_COND(pClkList->clkDomains[clkIdx].regimeId ==
                    LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID);
                target.super.regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID;

                // Patch the invalid target frequency. This is due to CLK_SET_INFO interface.
                if ((target.super.freqKHz / 1000) == 0)
                {
                    target.super.freqKHz = (clkNafllFreqMHzGet(pNafllDev) * 1000);
                }

                // Patch the invalid target voltage. This is due to CLK_SET_INFO interface.
                if (pVoltList->rails[pNafllDev->railIdxForLut].voltageuV  == 0)
                {
                    pVoltList->rails[pNafllDev->railIdxForLut].voltageuV =
                        clkNafllLutVoltageGet(pNafllDev);
                }

                //
                // Before programming the frequency, compute the new DVCO Min based
                // on the voltage that is set (decreasing change) or is going to be
                // set (increasing change)
                //
                status = s_clkNafllDvcoMinCompute(pNafllDev,
                            pNafllDev->pStatus,
                            pVoltList->rails[pNafllDev->railIdxForLut].voltageuV);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfChangeInject_exit;
                }
                target.super.dvcoMinFreqMHz = pNafllDev->pStatus->dvco.minFreqMHz;

                if (target.super.dvcoMinFreqMHz == 0U)
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    PMU_BREAKPOINT();
                    goto clkVfChangeInject_exit;
                }

                //
                // Determine the target frequency regime using target frequency and
                // target voltage.
                //
                target.super.regimeId = clkNafllDevTargetRegimeGet(pNafllDev,
                                            (target.super.freqKHz / 1000),
                                            target.super.dvcoMinFreqMHz,
                                            &pVoltList->rails[pNafllDev->railIdxForLut]);
                // Sanity check the target regime.
                if (!LW2080_CTRL_CLK_NAFLL_REGIME_IS_VALID(target.super.regimeId))
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto clkVfChangeInject_exit;
                }

                // Config NAFLL dynamic status params based on the input target request.
                status = clkNafllConfig(pNafllDev, pNafllDev->pStatus, &target);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfChangeInject_exit;
                }

                // Program NAFLL based on the callwlated NAFLL configurations.
                status = clkNafllProgram(pNafllDev, pNafllDev->pStatus);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfChangeInject_exit;
                }

                // PP-TODO : Create NAFLL clean up interface.
                pNafllDev->pStatus->regime.lwrrentRegimeId =
                    pNafllDev->pStatus->regime.targetRegimeId;
                pNafllDev->pStatus->regime.lwrrentFreqMHz  =
                    pNafllDev->pStatus->regime.targetFreqMHz;

            }
            FOR_EACH_INDEX_IN_MASK_END;

            // Update counted average frequency, used by frequency controllers.
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
            {
                status = clkFreqCountedAvgUpdate(
                    pClkList->clkDomains[clkIdx].clkDomain,
                    (pClkList->clkDomains[clkIdx].clkFreqKHz));
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfChangeInject_exit;
                }
            }
        }

clkVfChangeInject_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}


/*!
 * @brief Compute the DVCO Min for the given NAFLL
 *
 * The DVCO Min will be a function of speedo and voltage. Whenever the
 * set voltage changes we need to call this function so that the DVCO Min can
 * be updated. The set voltage changes in the following two conditions
 * 1. Directed voltage change during a V/F switch
 * 2. Voltage offset is applied by the frequency controller
 *
 * For now, we are going to compute the DVCO Min only during directed voltage
 * switches. CS has agreed to margin for the voltage offsets applied by the
 * frequency controller
 *
 * @param[in] pNafllDev  Pointer to the NAFLL device
 * @param[in] pStatus    Pointer to the NAFLL device's dynamic status params.
 * @param[in] voltageuV  The voltage value for which DVCO Min is to be computed
 *
 * @return Pointer to the NAFLL device
 */
static FLCN_STATUS
s_clkNafllDvcoMinCompute
(
   CLK_NAFLL_DEVICE                         *pNafllDev,
   LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus,
   LwU32                                     voltageuV
)
{
    FLCN_STATUS                 status = FLCN_OK;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
    RM_PMU_PERF_VFE_EQU_RESULT  result;

    // Evaluate the DVCO min only if the VFE index is valid.
    if (pNafllDev->dvco.minFreqIdx != PMU_PERF_VFE_EQU_INDEX_ILWALID)
    {
        vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal.voltage.varValue = voltageuV;

        // Grab the PERF DMEM read semaphore
        perfReadSemaphoreTake();

        status = vfeEquEvaluate(pNafllDev->dvco.minFreqIdx, &vfeVarVal, 1,
                     LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ, &result);

        // Release the PERF DMEM read semaphore
        perfReadSemaphoreGive();

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllDvcoMinCompute_exit;
        }

        //
        // If we have a valid VFE index, we don't expect the computed frequency
        // to be zero. Assert and return a FLCN_ERROR if this happens.
        //
        if (result.freqMHz == 0)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERROR;
            goto s_clkNafllDvcoMinCompute_exit;
        }
        pStatus->dvco.minFreqMHz    = (LwU16)result.freqMHz;
        Clk.clkNafll.dvcoMinFreqMHz[pNafllDev->railIdxForLut] =
            (LwU16)result.freqMHz;
    }
    else
    {
        pStatus->dvco.minFreqMHz    = 1U;

        //
        // PP-TODO : MODS sanity for GP107 has old VBIOS with incorrect POR.
        //           This is a temporary WAR to unblock submission while the
        //           MODS PVS sanity team update the VBIOS.
        //
        // status = FLCN_ERR_ILWALID_STATE;
        // PMU_BREAKPOINT();
        // goto s_clkNafllDvcoMinCompute_exit;
    }

s_clkNafllDvcoMinCompute_exit:
    return status;
}
