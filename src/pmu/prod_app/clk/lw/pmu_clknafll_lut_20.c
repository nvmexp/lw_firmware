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
 * @file   pmu_clknafll_lut_20.c
 * @brief  File for CLK NAFLL and LUT routines specific to versin 20.
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
 * @brief Construct the LUT_20 device
 *
 * @copydetails ClkNafllLutConstruct()
 */
FLCN_STATUS
clkNafllLutConstruct_20
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
        goto clkNafllLutConstruct_20_exit;
    }

    status = clkNafllLutConstruct_SUPER(pNafllDev,
                pLutDevice, pLutDeviceDesc, bFirstConstruct);
    if (status != FLCN_OK)
    {
        goto clkNafllLutConstruct_20_exit;
    }

clkNafllLutConstruct_20_exit:
    return status;
}

/*!
 * @brief Set the LUT_20 SW override
 *
 * @copydetails clkNafllLutOverride()
 */
FLCN_STATUS
clkNafllLutOverride_20
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

    if (pNafllDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_20_exit;
    }

    if ((overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) ||
        (overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN))
    {
        // Compute NDIV for the requested frequency
        status = clkNafllNdivFromFreqCompute(pNafllDev,
                    freqMHz, &lutEntry.lut20.ndiv, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllLutOverride_20_exit;
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
                goto clkNafllLutOverride_20_exit;
            }

            //
            // If the secondary lwrve frequency is greater than primary lwrve, program the ndivOffset to ZERO.
            // If it is equal to primary lwrve, do not call Freq -> NDIV colwersion
            // interface as it will return ndiv = 1 which is a MUST have war for existing
            // quantization interfaces to work. @ref clkNafllNdivFromFreqCompute
            //
            if (freqMHz > clkVfTupleOutput.data.v20.sec[0].freqMHz)
            {
                status = clkNafllNdivFromFreqCompute(pNafllDev,
                         (freqMHz - clkVfTupleOutput.data.v20.sec[0].freqMHz),
                         &ndivOffset,
                         LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkNafllLutOverride_20_exit;
                }
                lutEntry.lut20.ndivOffset = (LwU8)ndivOffset;
            }
            lutEntry.lut20.dvcoOffset = clkVfTupleOutput.data.v20.sec[0].dvcoOffsetCode;
        }
    }

    status = clkNafllLutOverrideSet_HAL(&Clk, pNafllDev, overrideMode, &lutEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_20_exit;
    }

    // Cache all the values in the LUT device
    pNafllDev->lutDevice.swOverrideMode        = overrideMode;
    pNafllDev->lutDevice.swOverride.lut20.ndiv = lutEntry.lut20.ndiv;

    pNafllDev->lutDevice.swOverride.lut20.ndivOffset = lutEntry.lut20.ndivOffset;
    pNafllDev->lutDevice.swOverride.lut20.dvcoOffset = lutEntry.lut20.dvcoOffset;

clkNafllLutOverride_20_exit:
    return status;
}

/*!
 * @brief Program all LUT_20 entries for a given NAFLL device.
 *
 * @copydetails clkNafllLutEntriesProgram()
 */
FLCN_STATUS
clkNafllLutEntriesProgram_20
(
    CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntries[CLK_LUT_ENTRIES_PER_STRIDE()];
    LwU16         vfIdx;
    FLCN_STATUS   status = FLCN_OK;
    LW2080_CTRL_CLK_VF_ITERATION_STATE vfIterState =
        LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();

    for (vfIdx = 0U; vfIdx < CLK_NAFLL_LUT_VF_NUM_ENTRIES();
         vfIdx += CLK_LUT_ENTRIES_PER_STRIDE())
    {
        status = clkNafllLutEntryCallwlate_20(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutEntriesProgram_20_exit;
        }

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_20(pNafllDev,
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_20_exit;
            }
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut20.ndiv       = 0U;
            lutEntries[1].lut20.ndivOffset = 0U;
            lutEntries[1].lut20.dvcoOffset = 0U;
        }

        NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafllDev)
        {
            status = clkNafllLutDataRegWrite_HAL(&Clk,
                                                 pNafllDev,
                                                 lutEntries,
                                                 CLK_LUT_ENTRIES_PER_STRIDE());
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_20_exit;
            }
        }
        NAFLL_LUT_BROADCAST_ITERATOR_END(pNafllDev);
    }

clkNafllLutEntriesProgram_20_exit:
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
clkNafllLutEntryCallwlate_20
(
    CLK_NAFLL_DEVICE  *pNafllDev,
    LwU16              vfIdx,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntryData,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    LW2080_CTRL_CLK_VF_INPUT                    clkArbInput;
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE clkArbOutput;
    FLCN_STATUS                                 status     = FLCN_OK;
    CLK_DOMAIN                                 *pDomain;
    CLK_DOMAIN_PROG                            *pDomainProg;
    LwU32                                       voltageuV  = 0U;
    LwU16                                       ndivOffset = 0U;

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
            goto clkNafllLutEntryCallwlate_20_exit;
        }

        pDomain = clkDomainsGetByApiDomain(pNafllDev->clkDomain);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_20_exit;
        }
        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_20_exit;
        }

        LW2080_CTRL_CLK_VF_INPUT_INIT(&clkArbInput);

        //
        // A maximum match may not have been found in the cases where the user has
        // specified a positive voltage delta and the lower voltages maybe no longer
        // be within the VF lwrve.  In those cases, taking the minimum match will
        // pick the frequency values corresponding to the lowest voltage in the VF
        // lwrve.
        //
        clkArbInput.flags   =
            (LwU8)FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
            clkArbInput.flags);
        clkArbInput.value   = voltageuV;

        status = clkDomainProgVoltToFreqTuple(
                    pDomainProg,
                    pNafllDev->railIdxForLut,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
                    &clkArbInput,
                    &clkArbOutput,
                    pVfIterState);
        if ((status != FLCN_OK) ||
            (LW2080_CTRL_CLK_VF_VALUE_ILWALID == clkArbOutput.inputBestMatch))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_20_exit;
        }

        if ((clkArbOutput.version !=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_10) &&
            (clkArbOutput.version !=
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20))
        {
            status = FLCN_ERR_ILWALID_VERSION;
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_20_exit;
        }

        // Corresponding NDIV value.
        status = clkNafllNdivFromFreqCompute(pNafllDev,
            clkArbOutput.data.v10.pri.freqMHz,
            &pLutEntryData->lut20.ndiv,
            LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllLutEntryCallwlate_20_exit;
        }

        // Init default values.
        pLutEntryData->lut20.ndivOffset = 0U;
        pLutEntryData->lut20.dvcoOffset = 0U;

        if (clkArbOutput.version ==
                LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20)
        {
            //
            // If the secondary lwrve frequency is greater than primary lwrve, program the ndivOffset to ZERO.
            // If it is equal to primary lwrve, do not call Freq -> NDIV colwersion
            // interface as it will return ndiv = 1 which is a MUST have war for existing
            // quantization interfaces to work. @ref clkNafllNdivFromFreqCompute
            //
            if (clkArbOutput.data.v10.pri.freqMHz > clkArbOutput.data.v20.sec[0].freqMHz)
            {
                status = clkNafllNdivFromFreqCompute(pNafllDev,
                    (clkArbOutput.data.v10.pri.freqMHz - clkArbOutput.data.v20.sec[0].freqMHz),
                    &ndivOffset,
                    LW_TRUE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkNafllLutEntryCallwlate_20_exit;
                }
                pLutEntryData->lut20.ndivOffset = (LwU8)ndivOffset;
            }

            pLutEntryData->lut20.dvcoOffset = clkArbOutput.data.v20.sec[0].dvcoOffsetCode;
        }

clkNafllLutEntryCallwlate_20_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Get LUT_20 VF Lwrve data
 *
 * @copydetails ClkNafllLutVfLwrveGet()
 */
FLCN_STATUS
clkNafllLutVfLwrveGet_20
(
    CLK_NAFLL_DEVICE               *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_LWRVE   *pLutVfLwrve
)
{
    LwU16   vfIdx;
    FLCN_STATUS                        status      = FLCN_OK;
    LW2080_CTRL_CLK_VF_ITERATION_STATE vfIterState = LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();

    for (vfIdx = 0U; vfIdx < CLK_NAFLL_LUT_VF_NUM_ENTRIES();
         vfIdx += CLK_LUT_ENTRIES_PER_STRIDE())
    {
        LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntries[CLK_LUT_ENTRIES_PER_STRIDE()] = {{0U},};

        status = clkNafllLutEntryCallwlate_20(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutVfLwrveGet_20_exit;
        }

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_20(pNafllDev,
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutVfLwrveGet_20_exit;
            }
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut20.ndiv       = 0U;
            lutEntries[1].lut20.ndivOffset = 0U;
            lutEntries[1].lut20.dvcoOffset = 0U;
        }

        pLutVfLwrve->lutVfEntries[vfIdx].type      = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20;
        pLutVfLwrve->lutVfEntries[vfIdx].data      = lutEntries[0];
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].type = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V20;
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].data = lutEntries[1];
    }

clkNafllLutVfLwrveGet_20_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
