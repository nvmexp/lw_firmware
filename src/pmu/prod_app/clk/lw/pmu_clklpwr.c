/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clklpwr.c
 * @brief  File for clock routines designed specifically for low power features.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_clkLpwrClkInitDeinit(LwU32 clkDomain, LwBool bInitialize)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "s_clkLpwrClkInitDeinit");

static FLCN_STATUS s_clkNafllSwStateInit(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_clkLpwr", "s_clkNafllSwStateInit");

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Clocks specific initialization routines for different LPWR features
 *
 * @return FLCN_OK  Init was successful
 */
FLCN_STATUS
clkLpwrPostInit()
{
    // To-do akshatam: move this to a CLK_LPWR specific DMEM overlay
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Cache NAFLL devinit settings
        (void)clkNafllsCacheGpcsNafllInitSettings_HAL(&Clk);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @brief Handles de-initilization of clocks needed during GPC-RG entry.
 *
 * This interfaces does:
 *   1. Disable clock counter access
 *   2. De-Initialize the ADC devices - reset booleans indicating the current
 *      state
 *
 * @param[in]  clkDomainMask    Mask of clock domains which are going to be
 *                              gated for GPC-RG.
 *
 * @return FLCN_OK  If all the de-initialization happens successfully
 * @return other    Descriptive error code from sub-routines on failure
 */
//! @li Design: https://confluence.lwpu.com/display/RMCLOC/Memsys+Clocks+Architecture+and+Design
FLCN_STATUS
clkGrRgDeInit
(
    LwU32 clkDomainMask
)
{
    FLCN_STATUS     status      = FLCN_OK;
    LwU8            clkDomBit;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntrSwSync)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Return early if the feature isn't supported and also break as it isn't
        // expected for us to land here if the feature is disabled.
        //
        if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
        {
            status = FLCN_ERR_FEATURE_NOT_ENABLED;
            PMU_BREAKPOINT();
            goto clkGrRgDeInit_exit;
        }

        // Sanity check if clock domain mask is valid
        if (clkDomainMask == 0U)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkGrRgDeInit_exit;
        }

        //
        // Assert if the given clock domains aren't disabled by this point.
        // Clock domains are lwrrently disabled from LPWR side in GPC-RG entry
        // sequence which happens prior to this function. Pre-silicon is an exception
        // and the platform check should be removed once bug 2908724 is fixed.
        //
        if (IsSilicon() && !CLK_DOMAINS_DISABLED(clkDomainMask))
        {
            //status = FLCN_ERR_ILWALID_STATE;
            //PMU_BREAKPOINT();
            //goto clkGrRgDeInit_exit;
        }

        // Iterate over all the domains in the mask to de-init each one of them
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            // Get clock domain corresponding to the index we're at
            LwU32          clkDomain   = LWBIT32(clkDomBit);

            // Disable HW clock counter access
            clkCntrAccessSync(clkDomain,
                              LW2080_CTRL_CLK_CLIENT_ID_GR_RG,
                              CLK_CNTR_ACCESS_DISABLED);

            // Reset output double buffer
            status = clkFreqDomainsResetDoubleBuffer(clkDomain);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkGrRgDeInit_exit;
            }

            // Call in the helper to De-Init the NAFLL/ADC/LUT devices
            status = s_clkLpwrClkInitDeinit(clkDomain, LW_FALSE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkGrRgDeInit_exit;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

    #if PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR_VERIF)
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            // Get clock domain corresponding to the index we're at
            LwU32     clkDomain = LWBIT32(clkDomBit);
            LwU8      cntrIdx = 0;
            CLK_CNTR *pCntr;

            ClkFreqDomain *pFreqDomain = CLK_FREQ_DOMAIN_GET(clkDomain);

            // If it doesn't exist, flag the error and exit the loop
            if (pFreqDomain == NULL)
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                goto clkGrRgDeInit_exit;
            }

            // 1. Verify Double Buffer is reset

            // Ensure that the entire signal is reset, not just the frequency
            if ((CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).path     != LW2080_CTRL_CMD_CLK_SIGNAL_PATH_EMPTY  ) ||
                (CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).fracdiv  != LW_TRISTATE_INDETERMINATE              ) ||
                (CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).regimeId != LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID) ||
                (CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).source   != LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID ) ||
                (CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).path     != LW2080_CTRL_CMD_CLK_SIGNAL_PATH_EMPTY  ) ||
                (CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).fracdiv  != LW_TRISTATE_INDETERMINATE              ) ||
                (CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).regimeId != LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID) ||
                (CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).source   != LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID ))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_DOUBLE_BUF) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                    |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // 2. Verify the clock counter state
            while (cntrIdx < Clk.cntrs.numCntrs)
            {
                pCntr = &Clk.cntrs.pCntr[cntrIdx++];

                if ((pCntr->clkDomain == clkDomain) &&
                    ((pCntr->accessDisableClientsMask & BIT(LW2080_CTRL_CLK_CLIENT_ID_GR_RG)) == 0U))
                {
                    // Log the failure here
                    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_CLK_CNTR) |
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                  |
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                    PMU_BREAKPOINT();
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
        //dbgPrintf(LEVEL_ALWAYS, "\n clkGrRgInit - Finished De-Init verif!\n");
    #endif

clkGrRgDeInit_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Handles initialization of clocks needed during GPC-RG critical part of exit sequence.
 *
 * This interfaces does:
 *   1. Initialize the NAFLL device to boot settings - FFR and boot freq
 *   2. Enable clock counter(s)
 *   3. Reset and restore HW access to clock counter(s)
 *
 * @param[in]  clkDomainMask    Mask of clock domains which are going to be
 *                              gated for GPC-RG.
 *
 * @return FLCN_OK  If all the de-initialization happens successfully
 * @return other    Descriptive error code from sub-routines on failure
 */
//! @li Design: https://confluence.lwpu.com/display/RMCLOC/Memsys+Clocks+Architecture+and+Design
FLCN_STATUS
clkGrRgInit
(
    LwU32 clkDomainMask
)
{
    FLCN_STATUS status;
    LwU8        clkDomBit;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntrSwSync)
    };

    //
    // Return early if the feature isn't supported and also break as it isn't
    // expected for us to land here if the feature is disabled.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    // Sanity check if clock domain mask is valid
    if (clkDomainMask == 0U)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // NAFLL init in FFR regime to boot frequency.
        // This is taken care of with restore of registers to the post devinit
        // values which is cached during init hence no domain is being passed.
        // As of April 2020, GPC-RG gates only GPC clock so we restore registers
        // specific to GPC clock.
        //
        status = clkNafllsProgramGpcsNafllInitSettings_HAL(&Clk);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }

        // Iterate over all the domains in the mask now to initialize each one of them
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            // Get clock domain corresponding to the index we're at
            LwU32          clkDomain   = LWBIT32(clkDomBit);

            // Enable clock counter(s)
            status = clkCntrDomainEnable(clkDomain);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }

            //
            // Reset cached last HW counter value for this domain.
            // This is needed to ensure we add the right difference to the SW counter
            // in clkCntrRead once the clock is ungated and the HW counter access is
            // restored.
            //
            status = clkCntrDomainResetCachedHwLast(clkDomain);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
            }

            // Enable clock counter HW access now
            clkCntrAccessSync(clkDomain,
                              LW2080_CTRL_CLK_CLIENT_ID_GR_RG,
                              CLK_CNTR_ACCESS_ENABLED_TRIGGER_CALLBACK);

            // NAFLL state Init
            status = s_clkNafllSwStateInit(clkDomain);
            if (status != FLCN_OK)
            {
                return status;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR_VERIF)
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            LwU32     clkDomain = LWBIT32(clkDomBit);
            LwU8      i = 0, ndivCache, ndivRead, data;
            CLK_CNTR *pCntr;

            // 1. NAFLL state - FFR regime and Boot Freq
            CLK_NAFLL_DEVICE *pNafllDev = clkNafllPrimaryDeviceGet(clkDomain);

            if (clkNafllRegimeGet(pNafllDev) != LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR)
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_REGIME) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                      |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_INIT));
                PMU_BREAKPOINT();
            }

            ndivCache = clkNafllsGetCachedNdivValue_HAL(&Clk);
            ndivRead  = clkNafllGetGpcProgrammedNdiv_HAL(&Clk);

            if (ndivCache != ndivRead)
            {
                data = ndivCache << 1 | ndivRead << 9;
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_FREQ) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                    |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_INIT)           |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DATA,       data));
                PMU_BREAKPOINT();
            }

            // 2. Verify the clock counter state
            while (i < Clk.cntrs.numCntrs)
            {
                pCntr = &Clk.cntrs.pCntr[i++];

                if ((pCntr->clkDomain == clkDomain) &&
                    ((pCntr->accessDisableClientsMask & BIT(LW2080_CTRL_CLK_CLIENT_ID_GR_RG)) != 0U))
                {
                    // Log the failure here
                    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_CLK_CNTR) |
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                  |
                        DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_INIT));
                    PMU_BREAKPOINT();
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
        //dbgPrintf(LEVEL_ALWAYS, "\n clkGrRgInit - Finished Init verif!\n");
#endif
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Handles non-critical initialization/restoration of clocks needed during GPC-RG exit.
 *
 * Things that this API addresses:
 *   1. Initialize ADC device
 *   2. Reprogram NAFLL LUT and enable LUT HW access.
 *   3. Restore NAFLL HMMA settings.
 *   4. Restore CPM configs
 *   5. Enable ADC Counters HW Access
 *
 * @param[in]  clkDomainMask    Mask of clock domains which are going to be
 *                              gated for GPC-RG.
 *
 * @return FLCN_OK  If all the de-initialization happens successfully
 * @return other    Descriptive error code from sub-routines on failure
 */
//! @li Design: https://confluence.lwpu.com/display/RMCLOC/Memsys+Clocks+Architecture+and+Design
FLCN_STATUS
clkGrRgRestore
(
    LwU32 clkDomainMask
)
{
    FLCN_STATUS     status;
    LwU8            clkDomBit;

    //
    // Return early if the feature isn't supported and also break as it isn't
    // expected for us to land here if the feature is disabled.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR))
    {
        status = FLCN_ERR_FEATURE_NOT_ENABLED;
        PMU_BREAKPOINT();
        goto clkGrRgRestore_exit;
    }

    // Sanity check if clock domain mask is valid
    if (clkDomainMask == 0U)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkGrRgRestore_exit;
    }

    //
    // Restore the devinit setting for NAFLL extended features before actually
    // programming the LUT.
    // These devinit settings are used during LUT programming to restore the
    // slowdown settings.
    //
    status = clkNafllsProgramGpcsNafllExtendedSettings_HAL();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkGrRgRestore_exit;
    }

    // Iterate over all the domains in the mask to de-init each one of them
    FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
    {
        // Get clock domain corresponding to the index we're at
        LwU32          clkDomain   = LWBIT32(clkDomBit);

        //
        // Call in the helper to restore the NAFLL/ADC/LUT devices for this
        // domain.
        //
        status = s_clkLpwrClkInitDeinit(clkDomain, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkGrRgRestore_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

clkGrRgRestore_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper function for Initializing/De-initializing a given clock domain
 *
 * @param[in]   clkDomain     Clock domain id to initialize of de-initialize
 * @param[in]   bInitialize   Flag to indicate Init/De-init operation
 *
 * TODO - @akshatam/@kwadhwa to make this function a public interface which can
 * be used for initializing/de-initializing given AVFS clock domain. Also
 * add _CLIENT_ID as an in-param so that it can be re-used for other LPWR
 * features as well.
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_STATE    if either NAFLL/ADC device does not exist
 *                                   for the given clock domain
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
static FLCN_STATUS s_clkLpwrClkInitDeinit
(
    LwU32  clkDomain,
    LwBool bInitialize
)
{
    FLCN_STATUS        status;
    CLK_NAFLL_DEVICE  *pNafllDev;
    LwU32              nafllIdMask;
    LwU32              nafllDevIdx;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // To-do kwadhwa/akshatam: Add IsSilicon check here
        //
        if (!CLK_IS_AVFS_SUPPORTED())
        {
            //
            // On RTL, we run the GPC-RG tests with a reduced VBIOS (no Avfs Tables)
            // We need to return early in such a scenario
            //
            status = FLCN_OK;
            goto s_clkLpwrClkInitDeinit_exit;
        }

        // Sanity check the input argument
        if (clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_clkLpwrClkInitDeinit_exit;
        }

        // Mask of all Nafll devices
        status = clkNafllDomainToIdMask(clkDomain, &nafllIdMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkLpwrClkInitDeinit_exit;
        }

        // Now for the actual restore/De-init
        FOR_EACH_INDEX_IN_MASK(32, nafllDevIdx, nafllIdMask)
        {
            pNafllDev = clkNafllDeviceGet(nafllDevIdx);
            if (pNafllDev == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_clkLpwrClkInitDeinit_exit;
            }

            //
            // Logic ADC device - must exist
            //
            if (pNafllDev->pLogicAdcDevice == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_clkLpwrClkInitDeinit_exit;
            }

            if (bInitialize)
            {
                //
                // Initialize the NAFLL ADC Aclwmulators. ISINK ADCs can't be
                // disabled so no need to re-initialize its Aclwmulators.
                // Secondly, the Aclwmulator register itself is L3 write
                // protected so L2 PMU ucode shouldn't attempt to write to it.
                //
                if (BOARDOBJ_GET_TYPE(pNafllDev->pLogicAdcDevice) !=
                        LW2080_CTRL_CLK_ADC_DEVICE_TYPE_V30_ISINK_V10)
                {
                    status = clkAdcAccInit_HAL(&Clk, pNafllDev->pLogicAdcDevice,
                                &pNafllDev->pLogicAdcDevice->adcAccSample);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_clkLpwrClkInitDeinit_exit;
                    }
                }

                // Reset the aclwmulator sample
                pNafllDev->pLogicAdcDevice->adcAccSample.adcAclwmulatorVal = 0U;
                pNafllDev->pLogicAdcDevice->adcAccSample.adcNumSamplesVal  = 0U;
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_ADC_IS_SENSED_VOLTAGE_PTIMER_BASED))
                {
                    LwU64 timeNsLast;
                    osPTimerTimeNsLwrrentGetAsLwU64(&timeNsLast);
                    RM_FLCN_U64_PACK(&pNafllDev->pLogicAdcDevice->adcAccSample.timeNsLast, &timeNsLast);
                }

                // Set the ADC to correct calibration state
                status = clkAdcCalProgram(pNafllDev->pLogicAdcDevice, bInitialize);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkLpwrClkInitDeinit_exit;
                }
            }

            // Now the _IDDQ state of the ADC, if supported
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_ALWAYS_ON))
            {
                status = clkAdcPowerOnAndEnable(pNafllDev->pLogicAdcDevice,
                            LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG,
                            LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED,
                            bInitialize);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkLpwrClkInitDeinit_exit;
                }
            }

            if (!bInitialize)
            {
                // Set the ADC to correct calibration state
                status = clkAdcCalProgram(pNafllDev->pLogicAdcDevice, bInitialize);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkLpwrClkInitDeinit_exit;
                }
            }

            //
            // Restore - NAFLL LUT reprogram - including restoration of slowdown
            // settings
            //
            if (bInitialize)
            {
                status = clkNafllLutProgram(pNafllDev);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkLpwrClkInitDeinit_exit;
                }
            }
            // Reset the LUT state
            else
            {
                pNafllDev->lutDevice.bInitialized = LW_FALSE;
                // Update the RAM_READ_EN bit as well to indicate the SW state
                status = clkNafllLutUpdateRamReadEn_HAL(&Clk, pNafllDev, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkLpwrClkInitDeinit_exit;
                }
            }

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_LPWR_VERIF)
        if (bInitialize)
        {
            // Ensure ADC Aclwmulator state is initialized
            if (!clkAdcAccIsInitialized_HAL(&Clk, pNafllDev->pLogicAdcDevice))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_ACC_INIT) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                      |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Ensure ADC calibration state is enabled
            if (!pNafllDev->pLogicAdcDevice->bHwCalEnabled)
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_CAL) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                 |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Ensure ADC power is On
            if (!pNafllDev->pLogicAdcDevice->bPoweredOn)
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_POWER) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                 |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Ensure the ADC SW disable state is correctly set
            if (((pNafllDev->pLogicAdcDevice->disableClientsMask & BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG)) != 0))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_EN_MASK) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                     |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE)         |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DATA,       pNafllDev->pLogicAdcDevice->disableClientsMask));
                PMU_BREAKPOINT();
            }
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
            if (((pNafllDev->pLogicAdcDevice->hwRegAccessClientsMask & BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG)) != 0))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_REG) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                    |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE)        |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DATA,       pNafllDev->pLogicAdcDevice->hwRegAccessClientsMask));
                PMU_BREAKPOINT();
            }
#endif
            // Ensure the HW enable state also matches
            if (!clkAdcIsHwEnabled_HAL(&Clk, pNafllDev->pLogicAdcDevice))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_EN_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                         |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Ensure the HW IDDQ state also matches
            if (!clkAdcIsHwPoweredOn_HAL(&Clk, pNafllDev->pLogicAdcDevice))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_IDDQ_POWER_ON) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                           |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Verify the NAFLL LUT SW state
            if (!CLK_NAFLL_LUT_IS_INITIALIZED(pNafllDev))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_SW_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                            |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }

            // Verify the NAFLL LUT HW state
            if (!clkNafllLutGetRamReadEn_HAL(&Clk, pNafllDev))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_HW_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                            |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_RESTORE));
                PMU_BREAKPOINT();
            }
        }
        else
        {
            // Ensure ADC calibration state is disabled
            if (pNafllDev->pLogicAdcDevice->bHwCalEnabled)
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_CAL) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                 |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // Ensure ADC device is powered-Off
            if (pNafllDev->pLogicAdcDevice->bPoweredOn)
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_POWER) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                 |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // Ensure the ADC SW disable state is correctly set
            if (((pNafllDev->pLogicAdcDevice->disableClientsMask & BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG)) == 0))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_EN_MASK) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                     |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT)         |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DATA,       pNafllDev->pLogicAdcDevice->disableClientsMask));
                PMU_BREAKPOINT();
            }
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_PER_DEVICE_REG_ACCESS_SYNC)
            if (((pNafllDev->pLogicAdcDevice->hwRegAccessClientsMask & BIT(LW2080_CTRL_CLK_ADC_CLIENT_ID_LPWR_GRRG)) == 0))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_REG) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                    |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT)        |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DATA,       pNafllDev->pLogicAdcDevice->hwRegAccessClientsMask));
                PMU_BREAKPOINT();
            }
#endif
            // Ensure the HW enable state also matches
            if (clkAdcIsHwEnabled_HAL(&Clk, pNafllDev->pLogicAdcDevice))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_HW_EN_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                         |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // Ensure the HW IDDQ state also matches
            if (clkAdcIsHwPoweredOn_HAL(&Clk, pNafllDev->pLogicAdcDevice))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_ADC_IDDQ_POWER_ON) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                           |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // Verify the NAFLL LUT SW state
            if (CLK_NAFLL_LUT_IS_INITIALIZED(pNafllDev))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_SW_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                            |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }

            // Verify the NAFLL LUT HW state
            if (clkNafllLutGetRamReadEn_HAL(&Clk, pNafllDev))
            {
                // Log the failure here
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_LOLWS),
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _ERR_STATE,  LW_MAILBOX_CLK_LPWR_ERR_STATE_NAFLL_LUT_HW_STATE) |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _DOMAIN_IDX, BIT_IDX_32(clkDomain))                            |
                    DRF_NUM(_MAILBOX, _CLK_LPWR, _STATE,      LW_MAILBOX_CLK_LPWR_STATE_GRRG_DEINIT));
                PMU_BREAKPOINT();
            }
        }
        //dbgPrintf(LEVEL_ALWAYS, "\n s_clkLpwrClkInitDeinit - Finished verif for request: %u nafll-id: %u\n", bInitialize, pNafllDev->id);
#endif
        }
        FOR_EACH_INDEX_IN_MASK_END;

s_clkLpwrClkInitDeinit_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Helper function for initializing NAFLL SW state for the given clock
 * domain.
 *
 * @param[in]   clkDomain     Clock domain ID to initialize NAFLL SW state for
 *
 * TODO - @akshatam/@kwadhwa to make this function a public interface which can
 * be re-used at other places like @ref clkNafllRegimeInit and @ref clkRead_Nafll.
 * Avoiding that change now to avoid risk for GA10x bring-up.
 *
 * @return FLCN_OK                   Operation completed successfully
 * @return FLCN_ERR_ILWALID_STATE    if either NAFLL/ADC device does not exist
 *                                   for the given clock domain
 * @return other                     Descriptive status code from sub-routines
 *                                   on success/failure
 */
static FLCN_STATUS s_clkNafllSwStateInit
(
    LwU32  clkDomain
)
{
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA   lutEntry     = {0};
    CLK_NAFLL_DEVICE                   *pNafllDev;
    FLCN_STATUS                         status;
    LwU32                               nafllIdMask;
    LwU32                               nafllDevIdx;
    LwU8                                overrideMode;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // To-do kwadhwa/akshatam: Add IsSilicon check here
        //
        if (!CLK_IS_AVFS_SUPPORTED())
        {
            //
            // On RTL, we run the GPC-RG tests with a reduced VBIOS (no Avfs Tables)
            // We need to return early in such a scenario
            //
            status = FLCN_OK;
            goto s_clkNafllSwStateInit_exit;
        }

        // Sanity check the input argument
        if (clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_clkNafllSwStateInit_exit;
        }

        // Get mask of all NAFLL devices corresponding to the given domain
        status = clkNafllDomainToIdMask(clkDomain, &nafllIdMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkNafllSwStateInit_exit;
        }

        // Iterate over all the NAFLL devices to init each of their state
        FOR_EACH_INDEX_IN_MASK(32, nafllDevIdx, nafllIdMask)
        {
            pNafllDev = clkNafllDeviceGet(nafllDevIdx);
            if (pNafllDev == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_clkNafllSwStateInit_exit;
            }

            // Callwlate the target frequency based on current programmed NDIV
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V30))
            {
                status = clkNafllLutOverrideGet_HAL(&Clk,
                            pNafllDev, &overrideMode, &lutEntry);
            }
#if (PMU_PROFILE_TU10X)
            else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V20))
            {
                status = clkNafllLutOverrideGet_HAL(&Clk,
                            pNafllDev, &overrideMode, &lutEntry);
            }
#endif
            else
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto s_clkNafllSwStateInit_exit;;
            }

            // Callwlate the programmed frequency from the NDIV read
#if (PMU_PROFILE_TU10X)
            pNafllDev->pStatus->regime.lwrrentFreqMHz =
                ((pNafllDev->inputRefClkFreqMHz * lutEntry.lut20.ndiv) /
                 (pNafllDev->mdiv * pNafllDev->inputRefClkDivVal * pNafllDev->pStatus->pldiv));

#else
            pNafllDev->pStatus->regime.lwrrentFreqMHz =
                ((pNafllDev->inputRefClkFreqMHz * lutEntry.lut30.ndiv) /
                 (pNafllDev->mdiv * pNafllDev->inputRefClkDivVal * pNafllDev->pStatus->pldiv));
#endif

            //
            // Adjust the frequency value based on 1x/2x DVCO.
            // Case 1: When 1x domains are supported(i.e. VF lwrve having 1x
            //         frequency) and 2x DVCO is in use, half the frequency.
            // Case 2: When 1x domains are not supported(i.e. VF lwrve having 2x
            //         frequency) and 1x DVCO is in use, double the frequency.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) &&
                !pNafllDev->dvco.bDvco1x)
            {
                pNafllDev->pStatus->regime.lwrrentFreqMHz /= 2;
            }
            else if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAINS_1X_SUPPORTED) &&
                     pNafllDev->dvco.bDvco1x)
            {
                pNafllDev->pStatus->regime.lwrrentFreqMHz *= 2;
            }

            // Find out the programmed regime
            switch (overrideMode)
            {
                case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
                {
                    pNafllDev->pStatus->regime.lwrrentRegimeId =
                        LW2080_CTRL_CLK_NAFLL_REGIME_ID_VR;
                    break;
                }
                case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
                {
                    pNafllDev->pStatus->regime.lwrrentRegimeId =
                        LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
                    break;
                }
                case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
                {
                    pNafllDev->pStatus->regime.lwrrentRegimeId =
                        LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
                    break;
                }
                default:
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto s_clkNafllSwStateInit_exit;
                }
            }


            // Do NOT re-callwlate DVCO min frequency
            pNafllDev->pStatus->dvco.bMinReached = LW_FALSE;

            // Get the current programmed PLDIV
            pNafllDev->pStatus->pldivEngage = LW_TRISTATE_FALSE;
            pNafllDev->pStatus->pldiv = clkNafllPldivGet_HAL(&Clk, pNafllDev);

            // In case PLDIV is engaged, change the _FFR to _FFR_BELOW_DVCO_MIN
            if ((pNafllDev->pStatus->regime.lwrrentRegimeId == LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR) &&
                (pNafllDev->pStatus->pldiv != LW2080_CTRL_CLK_NAFLL_CLK_NAFLL_PLDIV_DIV1))
            {
                pNafllDev->pStatus->regime.lwrrentRegimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN;
                pNafllDev->pStatus->pldivEngage = LW_TRISTATE_TRUE;
                pNafllDev->pStatus->regime.lwrrentFreqMHz /= pNafllDev->pStatus->pldiv;
            }

            // Set target frequency and regime equal to their current values
            pNafllDev->pStatus->regime.targetFreqMHz  = pNafllDev->pStatus->regime.lwrrentFreqMHz;
            pNafllDev->pStatus->regime.targetRegimeId = pNafllDev->pStatus->regime.lwrrentRegimeId;
        }
        FOR_EACH_INDEX_IN_MASK_END;

s_clkNafllSwStateInit_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

