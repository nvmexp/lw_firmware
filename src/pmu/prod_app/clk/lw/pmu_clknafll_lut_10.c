/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll.c
 * @brief  File for CLK NAFLL and LUT routines.
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
static FLCN_STATUS _clkNafllLutEntryNdivCallwlate(CLK_NAFLL_DEVICE *pNafllDev, LwU32 voltuV, LwU16 *pNdiv, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "_clkNafllLutEntryNdivCallwlate");
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define  VFGAIN_MAX   (0xF)

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the LUT_10 device
 *
 * @copydetails ClkNafllLutConstruct()
 *
 * @return  FLCN_ERR_NO_FREE_MEM    Error allocating memory for the V/F entries
 */
FLCN_STATUS
clkNafllLutConstruct_10
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
        goto clkNafllLutConstruct_10_exit;
    }

    status = clkNafllLutConstruct_SUPER(pNafllDev,
                pLutDevice, pLutDeviceDesc, bFirstConstruct);
    if (status != FLCN_OK)
    {
        goto clkNafllLutConstruct_10_exit;
    }

    pLutDevice->vselectMode         = pLutDeviceDesc->vselectMode;
    pLutDevice->hysteresisThreshold = pLutDeviceDesc->hysteresisThreshold;

    status = clkNafllLutParamsInit_HAL(&Clk, pNafllDev);
    if (status != FLCN_OK)
    {
        goto clkNafllLutConstruct_10_exit;
    }

    //
    // Initialize parameters which are required to be initialized just for the
    // first time construction.
    //
    if (bFirstConstruct)
    {
        if (PMUCFG_FEATURE_ENABLED(
             PMU_PERF_PROGRAM_LUT_BEFORE_ENABLING_ADC_WAR_BUG_1629772))
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = clkNafllProgramLutBeforeEnablingAdcWar(pNafllDev, LW_TRUE);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            if (status != FLCN_OK)
            {
                goto clkNafllLutConstruct_10_exit;
            }
        }
    }

clkNafllLutConstruct_10_exit:
    return status;
}

/*!
 * @brief Set the LUT_10 SW override
 *
 * @copydetails ClkNafllLutOverride()
 */
FLCN_STATUS
clkNafllLutOverride_10
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideMode,
    LwU16               freqMHz
)
{
    LwU16       ndiv   = 0;
    LwU16       vfgain = 0;
    FLCN_STATUS status;

    if (pNafllDev == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_10_exit;
    }

    if ((overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) ||
        (overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN))
    {
        // Init vfgain
        vfgain = pNafllDev->vfGain;

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_LUT_OVERRIDE_MIN_WAR_BUG_1588803) &&
            (overrideMode == LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN))
        {
            //
            // Bug GP100 1588803: design limitation in the LUT_OVERRIDE mux that
            // prevents us from overriding the NDIV and VFGAIN separately. Bug
            // 1592573 has been logged to fix this limitation in GP10X and later
            // chips.
            //
            // Till the bug gets fixed the WAR is to set the VFGAIN to MAX value
            // (4 bits, so max is 0xF). This ensures that the VFGAIN from the LUT
            // will always gets forwarded to the DVCO.
            //
            vfgain = VFGAIN_MAX;
        }

        // Compute NDIV for the requested frequency
        status = clkNafllNdivFromFreqCompute(pNafllDev, freqMHz, &ndiv, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllLutOverride_10_exit;
        }
    }


    status = clkNafllLut10OverrideSet_HAL(&Clk, pNafllDev, overrideMode, ndiv, vfgain);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkNafllLutOverride_10_exit;
    }

    // Cache all the values in the LUT device
    pNafllDev->lutDevice.swOverrideMode          = overrideMode;
    pNafllDev->lutDevice.swOverride.lut10.ndiv   = ndiv;
    pNafllDev->lutDevice.swOverride.lut10.vfgain = vfgain;

clkNafllLutOverride_10_exit:
    return status;
}

/*!
 * @brief Fill LUT entry 0 and override the ADC output to 0 before ADC is enabled
 * *NOTE*: Make sure to attach/detach perfClkAvfs before/after this function is called.
 *
 * Bug 1628051: ADC should only read the LUT after it is programmed. Otherwise,
 * X from LUT ram will propagate. For GP100, the proposed WAR is that SW
 * overrides the ADC output, and only programs one entry of the LUT before ADC
 * is enabled. So the data from the LUT is deterministic and no X propagation
 * issues are seen.
 *
 * For bug 200103904, it is necessary that ADC_DOUT is overridden to 0 and
 * consequently the LUT entry 0 to be filled with some value because when we
 * enable the ADC read for the first time, if the read address determined by
 * TEMP_INDEX and ADC_DOUT is NOT 0, and it is the same as the last LUT write
 * address, the read will not happen, and this will cause X propagation issue.
 *
 * @param[in]   pNafllDev   Pointer to the NAFLL device
 * @param[in]   bSetWar     Flag to set or clear the WAR.
 *                          Sets the WAR when TRUE and clears otherwise
 * --Setting the WAR ilwolves programming LUT entry 0 to value 0 and setting
 *   ADC override 0
 * --Clearing the WAR ilwolves clearing the ADC override
 *
 * @return FLCN_OK                   LUT temperature index set successfully
 * @return FLCN_ERROR                In case the nafll device corresponding to
 *                                   the device index is NULL
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid ID or temperature index
 * @return FLCN_ERR_ILWALID_INDEX     _lutMap_GP10X doesn't have an entry for
 *                                   the NAFLL ID passed as argument
 */
FLCN_STATUS
clkNafllProgramLutBeforeEnablingAdcWar
(
   CLK_NAFLL_DEVICE *pNafllDev,
   LwBool            bSetWar
)
{
    FLCN_STATUS   status      = FLCN_OK;
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntry = {0};

    if (bSetWar)
    {
        LwU8 nextTempIndex = clkNafllLutNextTempIdxGet(pNafllDev);

        // Program 0th entry of LUT at the next temperature index
        status = clkNafllLutAddrRegWrite_HAL(&Clk, pNafllDev, nextTempIndex);
        if (status != FLCN_OK)
        {
            goto clkNafllProgramLutBeforeEnablingAdcWar_exit;
        }

        // Program Ndiv = 28 & Vfgain = 2 at 0th LUT entry
        lutEntry.lut10.ndiv   = 28;
        lutEntry.lut10.vfgain = 2;
        status = clkNafllLut10DataRegWrite_HAL(&Clk, pNafllDev, &lutEntry, 1);
        if (status != FLCN_OK)
        {
            goto clkNafllProgramLutBeforeEnablingAdcWar_exit;
        }

        status = clkNafllTempIdxUpdate(pNafllDev, nextTempIndex);
        if (status != FLCN_OK)
        {
            goto clkNafllProgramLutBeforeEnablingAdcWar_exit;
        }
    }

    if (pNafllDev->lutDevice.vselectMode ==
            LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_LOGIC ||
        pNafllDev->lutDevice.vselectMode ==
            LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_MIN)
    {
        // Set/Clear override for the logic ADC device
        status = clkAdcPreSiliconSwOverrideSet(pNafllDev->pLogicAdcDevice,
                                   CLK_NAFLL_LUT_MIN_VOLTAGE_UV(),
                                   LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                   bSetWar);
        if (status != FLCN_OK)
        {
            goto clkNafllProgramLutBeforeEnablingAdcWar_exit;
        }
    }

    if (pNafllDev->lutDevice.vselectMode ==
            LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_SRAM ||
        pNafllDev->lutDevice.vselectMode ==
            LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_MIN)
    {
        // Set/Clear override for the SRAM ADC device
        status = clkAdcPreSiliconSwOverrideSet(pNafllDev->pSramAdcDevice,
                                   CLK_NAFLL_LUT_MIN_VOLTAGE_UV(),
                                   LW2080_CTRL_CLK_ADC_CLIENT_ID_PMU,
                                   bSetWar);
        if (status != FLCN_OK)
        {
            goto clkNafllProgramLutBeforeEnablingAdcWar_exit;
        }
    }

clkNafllProgramLutBeforeEnablingAdcWar_exit:
    return status;
}

/*!
 * @brief Program all LUT_10 entries for a given NAFLL device.
 *
 * @copydetails ClkNafllLutEntriesProgram()
 */
FLCN_STATUS
clkNafllLutEntriesProgram_10
(
    CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA lutEntries[CLK_LUT_ENTRIES_PER_STRIDE()];
    LwU8          vfIdx;
    FLCN_STATUS   status = FLCN_OK;
    LW2080_CTRL_CLK_VF_ITERATION_STATE vfIterState =
        LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();

    for (vfIdx = 0U; vfIdx < CLK_NAFLL_LUT_VF_NUM_ENTRIES();
         vfIdx += CLK_LUT_ENTRIES_PER_STRIDE())
    {
        status = clkNafllLutEntryCallwlate_10(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutEntriesProgram_10_exit;
        }
        lutEntries[0].lut10.vfgain = pNafllDev->vfGain;

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_10(pNafllDev,
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_10_exit;
            }
            lutEntries[1].lut10.vfgain = pNafllDev->vfGain;
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut10.ndiv   = 0;
            lutEntries[1].lut10.vfgain = 0;
        }

        NAFLL_LUT_BROADCAST_ITERATOR_BEGIN(pNafllDev)
        {
            status = clkNafllLut10DataRegWrite_HAL(&Clk,
                                                 pNafllDev,
                                                 lutEntries,
                                                 CLK_LUT_ENTRIES_PER_STRIDE());
            if (status != FLCN_OK)
            {
                goto clkNafllLutEntriesProgram_10_exit;
            }
        }
        NAFLL_LUT_BROADCAST_ITERATOR_END(pNafllDev);
    }

clkNafllLutEntriesProgram_10_exit:
    return status;
}

/*!
 * @copydoc ClkNafllLutEntryCallwlate()
 */
FLCN_STATUS
clkNafllLutEntryCallwlate_10
(
    CLK_NAFLL_DEVICE  *pNafllDev,
    LwU16              vfIdx,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntryData,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    LwU32 voltuV = 0;

    (void)clkNafllLutEntryVoltCallwlate(pNafllDev, vfIdx, &voltuV);

    return _clkNafllLutEntryNdivCallwlate(pNafllDev,
                                          voltuV,
                                          &pLutEntryData->lut10.ndiv,
                                          pVfIterState);
}

/*!
 * @brief Get LUT_10 VF Lwrve data
 *
 * @copydetails ClkNafllLutVfLwrveGet()
 */
FLCN_STATUS
clkNafllLutVfLwrveGet_10
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

        status = clkNafllLutEntryCallwlate_10(pNafllDev,
                                              vfIdx,
                                              &lutEntries[0],
                                              &vfIterState);
        if (status != FLCN_OK)
        {
            goto clkNafllLutVfLwrveGet_10_exit;
        }
        lutEntries[0].lut10.vfgain = pNafllDev->vfGain;

        if ((vfIdx + 1U) < CLK_NAFLL_LUT_VF_NUM_ENTRIES())
        {
            status = clkNafllLutEntryCallwlate_10(pNafllDev, 
                                                  (vfIdx + 1U),
                                                  &lutEntries[1],
                                                  &vfIterState);
            if (status != FLCN_OK)
            {
                goto clkNafllLutVfLwrveGet_10_exit;
            }
            lutEntries[1].lut10.vfgain = pNafllDev->vfGain;
        }
        else
        {
            // Odd number of V/F entries
            lutEntries[1].lut10.ndiv   = 0U;
            lutEntries[1].lut10.vfgain = 0U;
        }

        pLutVfLwrve->lutVfEntries[vfIdx].type       = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10;
        pLutVfLwrve->lutVfEntries[vfIdx].data       = lutEntries[0];
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].type  = LW2080_CTRL_CLK_NAFLL_DEVICE_TYPE_V10;
        pLutVfLwrve->lutVfEntries[vfIdx + 1U].data  = lutEntries[1];
    }

clkNafllLutVfLwrveGet_10_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Helper to callwlate the ndiv for given lut entry using
 * its input voltage.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   voltuV          Input voltage for this lut entry.
 * @param[out]  pNdiv           Ndiv value to be filled back
 * @param[in]   pVfIterState    VF iterator to speed up VF lookup.
 *
 * @return FLCN_OK      Ndiv was callwlated successfully
 * @return other        Propagates return values from various calls
 */
static FLCN_STATUS
_clkNafllLutEntryNdivCallwlate
(
    CLK_NAFLL_DEVICE                    *pNafllDev,
    LwU32                                voltuV,
    LwU16                               *pNdiv,
    LW2080_CTRL_CLK_VF_ITERATION_STATE  *pVfIterState
)
{
    FLCN_STATUS status;
    LwU16       freqMHz;

    //
    // Now look-up the frequency for the input voltage value in the
    // SOURCE POR V/F lwrve
    //
    status = clkNafllLutFreqMHzGet(pNafllDev,
                                   voltuV,
                                   LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
                                   &freqMHz,
                                   pVfIterState);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkNafllLutEntryNdivCallwlate_exit;
    }

    // Corresponding NDIV value.
    status = clkNafllNdivFromFreqCompute(pNafllDev, freqMHz, pNdiv, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkNafllLutEntryNdivCallwlate_exit;
    }

_clkNafllLutEntryNdivCallwlate_exit:
    return status;
}
