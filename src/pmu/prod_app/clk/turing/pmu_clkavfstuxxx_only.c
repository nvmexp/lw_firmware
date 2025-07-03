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
 * @file   pmu_clkavfstuxxx_only.c
 * @brief  PMU Hal Functions for TURING only for Clocks AVFS
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
 * @brief Update the V20 LUT table entries
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutEntries     Pointer to the LUT entries
 * @param[in]   numLutEntries   Number of VF entries to be updated in LUT
 *
 * @return FLCN_OK                   LUT updated successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT At least one of the input arguments passed
 *                                   is invalid
 */
FLCN_STATUS
clkNafllLutDataRegWrite_TU10X
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntries,
    LwU8                                numLutEntries
)
{
    LwU32   dataVal;
    LwU8    entryIdx;

    if ((pNafllDev == NULL) ||
        (pLutEntries == NULL))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Each DWORD in the LUT can hold two V/F table entries. Merge two entries
    // into a single DWORD before writing to the LUT.
    //
    for (entryIdx = 0; entryIdx < numLutEntries; entryIdx += 2U)
    {
        LwU8 numLutEntriesToPack = 1;

        if ((entryIdx + 1U) < numLutEntries)
        {
            numLutEntriesToPack = 2U;
        }

        // Pack 2 V/F entries into one DWORD
        dataVal = clkNafllLut20EntriesPack_HAL(&Clk, pNafllDev, pLutEntries, numLutEntriesToPack);

        REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_DATA), dataVal);
    }

    return FLCN_OK;
}

/*!
 * @brief Get the current value of LUT V20 overrides.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[out]  pOverrideMode   The current LUT override mode
 * @param[out]  pLutEntry       LUT entry to fill in.
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLutOverrideGet_TU10X
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
    overrideMode = (LwU8)DRF_VAL(_PTRIM_SYS,
                _NAFLL_LTCLUT_SW_FREQ_REQ, _SW_OVERRIDE_NDIV, data32);

    if (pLutEntry != NULL)
    {
        pLutEntry->lut20.ndiv =
            (LwU16)DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV, data32);
        if ((LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ != overrideMode) &&
            (pLutEntry->lut20.ndiv == 0U))
        {
            //
            // If the current regime is VR, SW override for ndiv will be equal
            // zero. Client should accept output frequency "0" which will indicate
            // that the current regime is VR and SW do not control the frequency.
            // They MUST read clock counters (Effective Frequency).
            //
            PMU_BREAKPOINT();
        }

        pLutEntry->lut20.ndivOffset =
            (LwU8)DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV_OFFSET, data32);
        if (pLutEntry->lut20.ndivOffset == 0U)
        {
            //
            // ZERO is acceptable value when both primary and secondary VF lwrve
            // points to the exact same frequency value for given LUT entry.
            //
            // PMU_BREAKPOINT();
        }

        pLutEntry->lut20.dvcoOffset =
            (LwU8)DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _DVCO_OFFSET, data32);
        if (pLutEntry->lut20.dvcoOffset == 0U)
        {
            //
            // ZERO is acceptable value when both primary and secondary VF lwrve
            // points to the exact same frequency value for given LUT entry.
            //
            // PMU_BREAKPOINT();
        }
    }

    if (pOverrideMode != NULL)
    {
        *pOverrideMode = overrideMode;
    }

    return FLCN_OK;
}

/*!
 * @brief Set the LUT V20 SW override
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   overrideMode    The LUT override mode to set
 * @param[out]  pLutEntry       LUT entry values to override.
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLutOverrideSet_TU10X
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
        goto clkNafllLutOverrideSet_TU10X_exit;
    }

    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);

    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_HW_REQ, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_HW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_SW_REQ, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_SW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_MIN, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_MIN, data32);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            break;
        }
    }

    if (status != FLCN_OK)
    {
        goto clkNafllLutOverrideSet_TU10X_exit;
    }

    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _NDIV, pLutEntry->lut20.ndiv, data32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _NDIV_OFFSET, pLutEntry->lut20.ndivOffset, data32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ,
                             _DVCO_OFFSET, pLutEntry->lut20.dvcoOffset, data32);

    REG_WR32(FECS, reg32, data32);

clkNafllLutOverrideSet_TU10X_exit:
    return status;
}

/*!
 * @brief Each DWORD in the LUT V20 can hold two V/F table entries. Merge
 *        two entries into a single DWORD before writing to the LUT.
 *
 * @param[in]   pNafllDev           Pointer to the NAFLL device
 * @param[in]   pLutEntries         LUT entries values to program.
 * @param[in]   numLutEntriesToPack Number of entries to pack in one word.
 *
 * @return the 32-bit packed value with all the 'in' params
 */
LwU32 clkNafllLut20EntriesPack_TU10X
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntries,
    LwU8                                numLutEntriesToPack
)
{
    LwU32 dataVal = 0;

    if ((pNafllDev == NULL)   ||
        (pLutEntries == NULL) ||
        (numLutEntriesToPack == 0U))
    {
        PMU_BREAKPOINT();
        return dataVal;
    }

    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                              _VAL0_NDIV_OFFSET, pLutEntries->lut20.ndivOffset, 0);
    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                              _VAL0_NDIV, pLutEntries->lut20.ndiv, dataVal);
    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                              _VAL0_DVCO_OFFSET, pLutEntries->lut20.dvcoOffset, dataVal);

    // Increment to get to next stride.
    pLutEntries++;

    if (numLutEntriesToPack > 1U)
    {
        dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                                  _VAL1_NDIV_OFFSET, pLutEntries->lut20.ndivOffset, dataVal);
        dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                                  _VAL1_NDIV, pLutEntries->lut20.ndiv, dataVal);
        dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSLUT_WRITE_DATA,
                                  _VAL1_DVCO_OFFSET, pLutEntries->lut20.dvcoOffset, dataVal);

        // Increment to get to next stride.
        pLutEntries++;
    }

    if (numLutEntriesToPack > 2U)
    {
        PMU_BREAKPOINT();
    }

    return dataVal;
}

/* ------------------------- Private Functions ------------------------------ */
