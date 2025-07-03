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
 * @file   pmu_clknafll_lut_30.c
 * @brief  File for CLK NAFLL and LUT routines specific to version 30.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the LUT_30 device
 *
 * @copydetails ClkNafllLutConstruct()
 */
FLCN_STATUS
clkNafllLutConstruct_30
(
   CLK_NAFLL_DEVICE             *pNafllDev,
   CLK_LUT_DEVICE               *pLutDevice,
   RM_PMU_CLK_LUT_DEVICE_DESC   *pLutDeviceDesc,
   LwBool                        bFirstConstruct
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pNafllDev      == NULL) ||
        (pLutDevice     == NULL) ||
        (pLutDeviceDesc == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllLutConstruct_30_exit;
    }

    status = clkNafllLutConstruct_SUPER(pNafllDev,
                pLutDevice, pLutDeviceDesc, bFirstConstruct);
    if (status != FLCN_OK)
    {
        goto clkNafllLutConstruct_30_exit;
    }

clkNafllLutConstruct_30_exit:
    return status;
}

/*!
 * @brief Set the LUT_30 SW override
 *
 * @copydoc ClkNafllLutOverride()
 */
FLCN_STATUS
clkNafllLutOverride_30
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideMode,
    LwU16               freqMHz
)
{
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA            lutEntry = {0};
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE  clkVfTupleOutput;
    LwU16                                        ndivOffset = 0;
    FLCN_STATUS                                  status;
    LwU8                                         vfLwrveIdx;

    if (pNafllDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_30_exit;
    }

    if ((overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) ||
        (overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN))
    {
        // Compute NDIV for the requested frequency
        status = clkNafllNdivFromFreqCompute(pNafllDev,
                    freqMHz, &lutEntry.lut30.ndiv, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllLutOverride_30_exit;
        }

        // Check if secondary lwrve is enabled
        if (PMU_CLK_NAFLL_IS_SEC_LWRVE_ENABLED())
        {
            // Compute the offsets for the requested frequency
            status = clkNafllLutOffsetTupleFromFreqCompute(pNafllDev,
                        freqMHz, &clkVfTupleOutput);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkNafllLutOverride_30_exit;
            }

            for (vfLwrveIdx = 0; vfLwrveIdx < clkDomainsNumSecVFLwrves(); vfLwrveIdx++)
            {
                //
                // If the secondary lwrve frequency is greater than primary lwrve, program the ndivOffset to ZERO.
                // If it is equal to primary lwrve, do not call Freq -> NDIV colwersion
                // interface as it will return ndiv = 1 which is a MUST have war for existing
                // quantization interfaces to work. @ref clkNafllNdivFromFreqCompute
                //
                if (freqMHz > clkVfTupleOutput.data.v20.sec[vfLwrveIdx].freqMHz)
                {
                    status = clkNafllNdivFromFreqCompute(pNafllDev,
                             (freqMHz - clkVfTupleOutput.data.v20.sec[vfLwrveIdx].freqMHz),
                             &ndivOffset,
                             LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkNafllLutOverride_30_exit;
                    }
                    lutEntry.lut30.ndivOffset[vfLwrveIdx] = (LwU8)ndivOffset;
                }
                lutEntry.lut30.dvcoOffset[vfLwrveIdx] = clkVfTupleOutput.data.v20.sec[vfLwrveIdx].dvcoOffsetCode;
            }
        }
    }

    status = clkNafllLutOverrideSet_HAL(&Clk, pNafllDev, overrideMode, &lutEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_30_exit;
    }

    // Cache all the values in the LUT device
    pNafllDev->lutDevice.swOverrideMode   = overrideMode;
    pNafllDev->lutDevice.swOverride.lut30 = lutEntry.lut30;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT)
    status = clkNafllLutQuickSlowdownForceEngage_HAL(&Clk, pNafllDev,
                        pNafllDev->lutDevice.bQuickSlowdownForceEngage,
                        clkDomainsNumSecVFLwrves());
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_30_exit;
    }
#endif

clkNafllLutOverride_30_exit:
    return status;
}

/*!
 * @brief Program all LUT_30 entries for a given NAFLL device.
 *
 * @copydetails ClkNafllLutEntriesProgram()
 */
FLCN_STATUS
clkNafllLutEntriesProgram_30
(
    CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntries[CLK_LUT_ENTRIES_PER_STRIDE()];
    LwU8          vfIdx;
    FLCN_STATUS   status = FLCN_OK;
    LW2080_CTRL_CLK_VF_ITERATION_STATE vfIterState =
        LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();
    LwU8          vfLwrveIdx;

    for (vfIdx = 0U; vfIdx < CLK_NAFLL_LUT_VF_NUM_ENTRIES();
         vfIdx += CLK_LUT_ENTRIES_PER_STRIDE())
    {
        status = clkNafllLutEntryCallwlate_30(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutEntriesProgram_30_exit;
        }

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_30(pNafllDev,
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_30_exit;
            }
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut30.ndiv             = 0;
            lutEntries[1].lut30.cpmMaxNdivOffset = 0;
            for (vfLwrveIdx = 0; vfLwrveIdx < clkDomainsNumSecVFLwrves(); vfLwrveIdx++)
            {
                lutEntries[1].lut30.ndivOffset[vfLwrveIdx] = 0;
                lutEntries[1].lut30.dvcoOffset[vfLwrveIdx] = 0;
            }
        }

        NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafllDev)
        {
            status = clkNafllLutDataRegWrite_HAL(&Clk,
                                                 pNafllDev,
                                                 lutEntries,
                                                 CLK_LUT_ENTRIES_PER_STRIDE());
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_30_exit;
            }
        }
        NAFLL_LUT_BROADCAST_ITERATOR_END(pNafllDev);
    }

clkNafllLutEntriesProgram_30_exit:
    return status;
}

/*!
 * @copydetails ClkNafllLutEntryCallwlate()
 *
 * @return  FLCN_ERR_OUT_OF_RANGE       Callwlated Voltage is out of range
 * @return  FLCN_ERR_OBJECT_NOT_FOUND   Clock Domain object is returned NULL
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Frequency Tuple Object for given rail index is invalid
 * @return  FLCN_ERR_ILWALID_VERSION    Invalid LUT Frequency Tuple version is set
 */
FLCN_STATUS
clkNafllLutEntryCallwlate_30
(
    CLK_NAFLL_DEVICE  *pNafllDev,
    LwU16              vfIdx,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntryData,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    LW2080_CTRL_CLK_VF_INPUT                    clkVfInput;
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE clkVfTupleOutput;
    FLCN_STATUS                                 status     = FLCN_OK;
    CLK_DOMAIN                                 *pDomain;
    CLK_DOMAIN_PROG                            *pDomainProg;
    LwU32                                       voltageuV  = 0;
    LwU16                                       ndivOffset = 0;
    LwU8                                        vfLwrveIdx;

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        (void)clkNafllLutEntryVoltCallwlate(pNafllDev, vfIdx, &voltageuV);

        // Sanity check
        if ((voltageuV < CLK_NAFLL_LUT_MIN_VOLTAGE_UV()) ||
            (voltageuV > CLK_NAFLL_LUT_MAX_VOLTAGE_UV()))
        {
            status = FLCN_ERR_OUT_OF_RANGE;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }

        pDomain = clkDomainsGetByApiDomain(pNafllDev->clkDomain);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }
        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }

        LW2080_CTRL_CLK_VF_INPUT_INIT(&clkVfInput);

        //
        // A maximum match may not have been found in the cases where the user has
        // specified a positive voltage delta and the lower voltages maybe no longer
        // be within the VF lwrve.  In those cases, taking the minimum match will
        // pick the frequency values corresponding to the lowest voltage in the VF
        // lwrve.
        //
        clkVfInput.flags   =
            FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
            clkVfInput.flags);
        clkVfInput.value   = voltageuV;

        status = clkDomainProgVoltToFreqTuple(
                    pDomainProg,
                    pNafllDev->railIdxForLut,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
                    &clkVfInput,
                    &clkVfTupleOutput,
                    pVfIterState);
        if ((status != FLCN_OK) ||
            (LW2080_CTRL_CLK_VF_VALUE_ILWALID == clkVfTupleOutput.inputBestMatch))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }

        //
        // On GPC,
        //      Version 30 will be used only for CPM,
        //      Else Version 20 will be used
        // On ALL other NAFLL clock domains
        //      Version 10 will be used.
        //
        if ((clkVfTupleOutput.version !=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_10) &&
            (clkVfTupleOutput.version !=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20) &&
            (clkVfTupleOutput.version !=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_30))
        {
            status = FLCN_ERR_ILWALID_VERSION;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }

        // Init to ZERO
        memset(pLutEntryData, 0, sizeof(LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA));

        // Corresponding NDIV value.
        status = clkNafllNdivFromFreqCompute(pNafllDev,
            clkVfTupleOutput.data.v30.pri.freqMHz,
            &pLutEntryData->lut30.ndiv,
            LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_30_exit;
        }

        // Update version specific params
        if (clkVfTupleOutput.version >=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20)
        {
            for (vfLwrveIdx = 0; vfLwrveIdx < clkDomainsNumSecVFLwrves(); vfLwrveIdx++)
            {
                //
                // If the secondary lwrve frequency is greater than primary lwrve, program the ndivOffset to ZERO.
                // If it is equal to primary lwrve, do not call Freq -> NDIV colwersion
                // interface as it will return ndiv = 1 which is a MUST have war for existing
                // quantization interfaces to work. @ref clkNafllNdivFromFreqCompute
                //
                if (clkVfTupleOutput.data.v30.pri.freqMHz > clkVfTupleOutput.data.v30.sec[vfLwrveIdx].freqMHz)
                {
                    status = clkNafllNdivFromFreqCompute(pNafllDev,
                        (clkVfTupleOutput.data.v30.pri.freqMHz - clkVfTupleOutput.data.v30.sec[vfLwrveIdx].freqMHz),
                        &ndivOffset,
                        LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkNafllLutEntryCallwlate_30_exit;
                    }
                    pLutEntryData->lut30.ndivOffset[vfLwrveIdx] = (LwU8)ndivOffset;
                }
                pLutEntryData->lut30.dvcoOffset[vfLwrveIdx] = clkVfTupleOutput.data.v30.sec[vfLwrveIdx].dvcoOffsetCode;
            }
        }

        if (clkVfTupleOutput.version ==
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_30)
        {
                // Callwlate CPM MAX NDIV offset value.
                status = clkNafllNdivFromFreqCompute(pNafllDev,
                            clkVfTupleOutput.data.v30.pri.cpmMaxFreqOffsetMHz,
                            &pLutEntryData->lut30.cpmMaxNdivOffset,
                            LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkNafllLutEntryCallwlate_30_exit;
                }
        }

clkNafllLutEntryCallwlate_30_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Get LUT_30 VF Lwrve data
 *
 * @copydetails ClkNafllLutVfLwrveGet()
 */
FLCN_STATUS
clkNafllLutVfLwrveGet_30
(
    CLK_NAFLL_DEVICE               *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_LWRVE   *pLutVfLwrve
)
{
    LwU16   vfIdx;
    FLCN_STATUS                        status      = FLCN_OK;
    LW2080_CTRL_CLK_VF_ITERATION_STATE vfIterState = LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();
    LwU8                               vfLwrveIdx;

    for (vfIdx = 0U; vfIdx < CLK_NAFLL_LUT_VF_NUM_ENTRIES();
         vfIdx += CLK_LUT_ENTRIES_PER_STRIDE())
    {
        LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntries[CLK_LUT_ENTRIES_PER_STRIDE()] = {{0U},};

        status = clkNafllLutEntryCallwlate_30(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutVfLwrveGet_30_exit;
        }

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_30(pNafllDev,
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutVfLwrveGet_30_exit;
            }
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut30.ndiv             = 0U;
            lutEntries[1].lut30.cpmMaxNdivOffset = 0U;
            for (vfLwrveIdx = 0; vfLwrveIdx < clkDomainsNumSecVFLwrves(); vfLwrveIdx++)
            {
                lutEntries[1].lut30.ndivOffset[vfLwrveIdx] = 0U;
                lutEntries[1].lut30.dvcoOffset[vfLwrveIdx] = 0U;
            }
        }

        pLutVfLwrve->lutVfEntries[vfIdx].type      = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30;
        pLutVfLwrve->lutVfEntries[vfIdx].data      = lutEntries[0];
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].type = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V30;
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].data = lutEntries[1];
    }

clkNafllLutVfLwrveGet_30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

