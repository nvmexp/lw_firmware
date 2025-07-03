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
 * @file   pmu_clkavfsga100.c
 * @brief  PMU Hal Functions for GA100 for Clocks AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get the current value of LUT V30 overrides.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[out]  pOverrideMode   The current LUT override mode
 * @param[out]  pLutEntry       LUT entry to fill in.
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 * @note NDIV_OFFSET / DVCO_OFFSET have been split to A/B versions, and are
 *       controlled via modular LUT.  
 *       On GA100 only one secondary lwrve is POR so SW will only update _A and
 *       program _B to zero.
 */
FLCN_STATUS
clkNafllLutOverrideGet_GA100
(
    CLK_NAFLL_DEVICE                    *pNafllDev,
    LwU8                                *pOverrideMode,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA   *pLutEntry
)
{
    LwU32   reg32;
    LwU32   data32;
    LwU8    overrideMode;


    if (pNafllDev == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);

    // Get override mode.
    overrideMode = DRF_VAL(_PTRIM_SYS,
                _NAFLL_LTCLUT_SW_FREQ_REQ, _SW_OVERRIDE_NDIV, data32);

    if (pLutEntry != NULL)
    {
        pLutEntry->lut30.ndiv =
            DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV, data32);
        if ((LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ != overrideMode) &&
            (pLutEntry->lut30.ndiv == 0))
        {
            //
            // If the current regime is VR, SW override for ndiv will be equal
            // zero. Client should accept output frequency "0" which will indicate
            // that the current regime is VR and SW do not control the frequency.
            // They MUST read clock counters (Effective Frequency).
            //
            PMU_BREAKPOINT();
        }

        pLutEntry->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0] =
            DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV_OFFSET_A, data32);

        pLutEntry->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0] =
            DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _DVCO_OFFSET_A, data32);

        if (clkDomainsNumSecVFLwrves() > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
        {
            pLutEntry->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1] =
                DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV_OFFSET_B, data32);

            pLutEntry->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1] =
                DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _DVCO_OFFSET_B, data32);
        }
    }

    if (pOverrideMode != NULL)
    {
        *pOverrideMode = overrideMode;
    }

    return FLCN_OK;
}

/*!
 * @brief Set the LUT V30 SW override
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   overrideMode    The LUT override mode to set
 * @param[out]  pLutEntry       LUT entry values to override.
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 * @note NDIV_OFFSET / DVCO_OFFSET have been split to A/B versions, and are
 *       controlled via modular LUT.
 *       On GA100 only one secondary lwrve is POR so SW will only update _A and
 *       program _B to zero.
 */
FLCN_STATUS
clkNafllLutOverrideSet_GA100
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LwU8                                overrideMode,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntry
)
{
    LwU32       reg32;
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    if ((pNafllDev == NULL) ||
        (pLutEntry == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutOverrideSet_GA100_exit;
    }

    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);

    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_HW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_SW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_MIN, data32);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkNafllLutOverrideSet_GA100_exit;
        }
    }

    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _NDIV, pLutEntry->lut30.ndiv, data32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _NDIV_OFFSET_A,
                             pLutEntry->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                             data32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _DVCO_OFFSET_A,
                             pLutEntry->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                             data32);

    if (clkDomainsNumSecVFLwrves() > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
    {
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _NDIV_OFFSET_B,
                                 pLutEntry->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                 data32);
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _DVCO_OFFSET_B,
                                 pLutEntry->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                 data32);
    }

    REG_WR32(FECS, reg32, data32);

clkNafllLutOverrideSet_GA100_exit:
    return status;
}

/*!
 * @brief Update the V20 LUT table entries
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutEntries     Pointer to the LUT entries
 * @param[in]   numLutEntries   Number of VF entries to be updated in LUT
 *
 * @return FLCN_OK                    LUT updated successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid Nafll device
 * @return FLCN_ERR_ILWALID_INDEX     ClkNafllRegMap_HAL doesn't have the entry
 *                                    for the NAFLL device
 */
FLCN_STATUS
clkNafllLutDataRegWrite_GA100
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntries,
    LwU8                                numLutEntries
)
{
    LwU8        entryIdx;
    LW_STATUS   status  = FLCN_OK;

    if ((pNafllDev == NULL) ||
        (pLutEntries == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutDataRegWrite_GA100_exit;
    }

    //
    // Each DWORD in the LUT can hold two V/F table entries.
    // _DVCO_OFFSET_B and _NDIV_OFFSET_B are not programmed from software as of
    // now.
    //
    for (entryIdx = 0; entryIdx < numLutEntries; entryIdx += 2)
    {
        LwU32   ndivTuple               = 0U;
        LwU32   offsetTuple             = 0U;
        LwU32   cpmMaxNdivOffsetTuple   = 0U;
        LwU8    numLutEntriesToPack     = 1U;

        if ((entryIdx + 1) < numLutEntries)
        {
            numLutEntriesToPack = 2;
        }

        ndivTuple   = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_DATA,
                                  _VAL0_NDIV, pLutEntries->lut30.ndiv, ndivTuple);
        offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                  _VAL0_NDIV_OFFSET_A,
                                  pLutEntries->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                                  offsetTuple);
        offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                  _VAL0_DVCO_OFFSET_A,
                                  pLutEntries->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                                  offsetTuple);

        if (clkDomainsNumSecVFLwrves() > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
        {
            offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                      _VAL0_NDIV_OFFSET_B,
                                      pLutEntries->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                      offsetTuple);
            offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                      _VAL0_DVCO_OFFSET_B,
                                      pLutEntries->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                      offsetTuple);
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
        {
            cpmMaxNdivOffsetTuple =
                FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_CPM_DATA,
                _VAL0_CPM_OFFSET_MAX, pLutEntries->lut30.cpmMaxNdivOffset, cpmMaxNdivOffsetTuple);
        }

        // Increment to get to next stride.
        pLutEntries++;

        if (numLutEntriesToPack > 1)
        {
            ndivTuple   = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_DATA,
                                      _VAL1_NDIV, pLutEntries->lut30.ndiv, ndivTuple);
            offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                      _VAL1_NDIV_OFFSET_A,
                                      pLutEntries->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                                      offsetTuple);
            offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                      _VAL1_DVCO_OFFSET_A,
                                      pLutEntries->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0],
                                      offsetTuple);

            if (clkDomainsNumSecVFLwrves() > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
            {
                offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                          _VAL1_NDIV_OFFSET_B,
                                          pLutEntries->lut30.ndivOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                          offsetTuple);
                offsetTuple = FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_OFFSET_DATA,
                                          _VAL1_DVCO_OFFSET_B,
                                          pLutEntries->lut30.dvcoOffset[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1],
                                          offsetTuple);
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
            {
                cpmMaxNdivOffsetTuple =
                    FLD_SET_DRF_NUM(_PTRIM_GPC, _GPCLUT_WRITE_CPM_DATA,
                    _VAL1_CPM_OFFSET_MAX, pLutEntries->lut30.cpmMaxNdivOffset, cpmMaxNdivOffsetTuple);
            }

            // Increment to get to next stride.
            pLutEntries++;
        }

        REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_DATA), ndivTuple);
        if (CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_OFFSET_DATA) != LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED)
        {
            REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_OFFSET_DATA), offsetTuple);
        }
        if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM)) &&
            (CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_CPM_DATA) != LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED))
        {
            REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_CPM_DATA), cpmMaxNdivOffsetTuple);
        }
    }

clkNafllLutDataRegWrite_GA100_exit:
    return status;
}

/*!
 * @brief Override CPM NDIV Offset output to ZERO or clear the override
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutEntries     Pointer to the LUT entries
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkNafllCpmNdivOffsetOverride_GA100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwBool              bOverride
)
{
    LwU32   regData;

    if (CLK_NAFLL_REG_GET(pNafllDev, AVFS_CPM_CLK_CFG) != LW2080_CTRL_CLK_NAFLL_ID_UNDEFINED)
    {
        regData = REG_RD32(FECS, CLK_NAFLL_REG_GET(pNafllDev, AVFS_CPM_CLK_CFG));

        if (bOverride)
        {
            regData = FLD_SET_DRF(_PTRIM_GPC, _AVFS_CPM_CLK_CFG, _CLEAR, _SET, regData);
        }
        else
        {
            regData = FLD_SET_DRF(_PTRIM_GPC, _AVFS_CPM_CLK_CFG, _CLEAR, _UNSET, regData);
        }

        REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, AVFS_CPM_CLK_CFG), regData);
    }

    return FLCN_OK;
}

/*!
 * @brief Get bitmask of all supported NAFLLs on this chip
 *
 * @param[out]   pMask     Pointer to the bitmask
 */
void
clkNafllAllMaskGet_GA100
(
    LwU32   *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_NAFLL_ID_SYS) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_XBAR)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC2)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC3)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC4)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC5)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC6)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC7)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPCS)|
#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_HOST)|
#endif
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_LWD);
}

/*!
 * @brief   Set or clear ADC code and mode of operation on ADC_MUX
 *          that controls the NAFLL LUT entry selection for target
 *          frequency.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   overrideMode    SW override mode of operation.
 * @param[in]   overrideCode    SW override ADC code.
 *
 * @return FLCN_OK                      Operation completed successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT    Invalid SW override mode of operation passed
 *                                      Invalid NAFLL pointer passed
 */
FLCN_STATUS
clkNafllAdcMuxOverrideSet_GA100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideCode,
    LwU8                overrideMode
)
{
    LwU32   data32;

    // Sanity check
    if (pNafllDev == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read the current settings of ADC MUX and keep the HW output intact.
    data32 = REG_RD32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_ADC_OVERRIDE));

    // Compute values to write
    switch(overrideMode)
    {
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_SW_REQ:
        {
            // Set ADC_DOUT to the ADC code to override with
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                     _ADC_DOUT, overrideCode, data32);
            // Set SW_OVERRIDE_ADC_USE_SW
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _USE_SW, data32);
            break;
        }
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_HW_REQ:
        {
            //
            // Reset ADC_DOUT to the ADC code ZERO in HW mode. Note that this
            // param is unused in HW mode but explicitly setting to ZERO to
            // reduce confusion.
            //
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                     _ADC_DOUT, 0, data32);

            // Set SW_OVERRIDE_ADC_USE_HW
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _USE_HW, data32);

            break;
        }
        case LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_MIN:
        {
            // Set ADC_DOUT to the ADC code to override with
            data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                     _ADC_DOUT, overrideCode, data32);

            // Set SW_OVERRIDE_ADC_USE_MIN
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_ADC_OVERRIDE,
                                 _SW_OVERRIDE_ADC, _USE_MIN, data32);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    // Now the register writes
    REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_ADC_OVERRIDE), data32);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
