/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkavfsgp10x.c
 * @brief  PMU Hal Functions for GP10X for Clocks AVFS
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
 * @brief Update the temperature Index of the LUT
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   temperatureIdx  temp index to set
 *
 * @return FLCN_OK              LUT temperature Index set successfully
 */
FLCN_STATUS
clkNafllTempIdxUpdate_GP10X
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                temperatureIdx
)
{
    LwU32   reg32;
    LwU32   data32;

    // Update the temperature index
    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_CFG);
    data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_CFG, _TEMP_INDEX,
                             temperatureIdx, data32);
    REG_WR32(FECS, reg32, data32);

    return FLCN_OK;
}

/*!
 * @brief Update the LUT address write register at the given temperature index
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   temperatureIdx  The temperature index of the table to program
 *
 * @return FLCN_OK                   LUT address register updated successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT Invalid ID or temperature Idx
 */
FLCN_STATUS
clkNafllLutAddrRegWrite_GP10X
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                temperatureIdx
)
{
    LwU32   addressVal;
    LwU32   tmpResult;

    if ((pNafllDev == NULL) ||
        (temperatureIdx > CLK_LUT_TEMP_IDX_MAX))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    tmpResult = (LwU32)(temperatureIdx * CLK_LUT_STRIDE);
    addressVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_ADDR, _OFFSET,
                    tmpResult, 0U);
    addressVal = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_ADDR,
                    _AUTO_INC, _ON, addressVal);

    REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_ADDR), addressVal);

    return FLCN_OK;
}

/*!
 * @brief Update the V10 LUT table entries
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutEntries     Pointer to the LUT entries
 * @param[in]   numLutEntries   Number of VF entries to be updated in LUT
 *
 * @return FLCN_OK                    LUT updated successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid Nafll device
 */
FLCN_STATUS
clkNafllLut10DataRegWrite_GP10X
(
    CLK_NAFLL_DEVICE                   *pNafllDev,
    LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA  *pLutEntries,
    LwU8                                numLutEntries
)
{
    LwU32   dataVal;
    LwU8    entryIdx;

    if (pNafllDev == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Each DWORD in the LUT can hold two V/F table entries. Merge two entries
    // into a single DWORD before writing to the LUT.
    //
    for (entryIdx = 0; entryIdx < numLutEntries; entryIdx += 2U)
    {
        LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA *pLutEntryNext = NULL;

        if ((entryIdx + 1U) < numLutEntries)
        {
            pLutEntryNext = pLutEntries + 1;
        }
        // Pack 2 V/F entries into one DWORD
        dataVal = clkNafllLut10EntriesPack_HAL(&Clk,
                     pLutEntries->lut10.ndiv,
                     pLutEntries->lut10.vfgain,
                     (pLutEntryNext != NULL ?
                      pLutEntryNext->lut10.ndiv : 0U),
                     (pLutEntryNext != NULL ?
                      pLutEntryNext->lut10.vfgain : 0U));

        pLutEntries += 2U;

        REG_WR32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_WRITE_DATA), dataVal);
    }

    return FLCN_OK;
}


/*!
 * @brief Get bitmask of all supported NAFLLs on this chip
 *
 * @param[out]   pMask     Pointer to the bitmask
 */
void
clkNafllAllMaskGet_GP10X
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
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPCS);
}

/*!
 * @brief Set the PLDIV value for the NAFLL device
 *
 * @param[in]       pNafllDev   pointer to the NAFLL device
 * @param[in]       pldiv       PLDIV value to set for the NAFLL device
 *
 * @return FLCN_OK                   The PLDIV is set successfully
 * @retrun FLCN_ERR_ILWALID_ARGUMENT If the Nafll device is NULL
 */
FLCN_STATUS
clkNafllPldivSet_GP10X
(
   CLK_NAFLL_DEVICE  *pNafllDev,
   LwU8               pldiv
)
{
    LwU32  reg32;
    LwU32  data32;

    if (pNafllDev == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Update the coefficient register for PLDIV
    reg32  = CLK_NAFLL_REG_GET(pNafllDev, NAFLL_COEFF);
    data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_SYSNAFLL_COEFF, _PDIV,
                             pldiv, data32);
    REG_WR32(FECS, reg32, data32);

    return FLCN_OK;
}

/*!
 * @brief Get the PLDIV value for the NAFLL device
 *
 * @param[in]       pNafllDev   pointer to the NAFLL device
 *
 * @return The set PLDIV value for the NAFLL device
 * @return @ref CLK_NAFLL_PLDIV_DIV1 if the pldiv value is '0'
 */
LwU8
clkNafllPldivGet_GP10X
(
   CLK_NAFLL_DEVICE  *pNafllDev
)
{
    LwU32  reg32;
    LwU32  data32;
    LwU8   pldiv;

    // Read the coefficient register for PLDIV
    reg32  = CLK_NAFLL_REG_GET(pNafllDev, NAFLL_COEFF);
    data32 = REG_RD32(FECS, reg32);
    pldiv  = (LwU8)DRF_VAL(_PTRIM_SYS, _NAFLL_SYSNAFLL_COEFF, _PDIV, data32);

    // A value of 0 is same as DIV1
    if (pldiv == 0U) 
    {
        pldiv =  CLK_NAFLL_PLDIV_DIV1;
    }

    return pldiv;
}

/*!
 * @brief   Initialize register mapping within NAFLL device
 *
 * @param[in]   pNafllDev   Pointer to the NAFLL device
 *
 * @return  FLCN_ERR_ILWALID_INDEX  If register mapping init has failed
 * @return  FLCN_OK                 Otherwise
 */
FLCN_STATUS
clkNafllRegMapInit_GP10X
(
   CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LwU8 nafllId = pNafllDev->id;
    LwU8 idx;

    for (idx = 0U;
         (ClkNafllRegMap_HAL[idx].id != LW2080_CTRL_CLK_NAFLL_ID_MAX);
         idx++)
    {
        if ((ClkNafllRegMap_HAL[idx].id == nafllId))
        {
            pNafllDev->regMapIdx = idx;
            return FLCN_OK;
        }
    }

    return FLCN_ERR_ILWALID_INDEX;
}

/*!
 * @brief Update the RAM READ EN bit once the LUT is programmed
 *
 * @param[in]   pNafllDev Pointer to the NAFLL device
 * @param[in]   bEnable   Boolean to indicate if RAM_READ_EN needs to be enabled
 *                        or disabled
 *
 * @return FLCN_OK
 * 
 * @note This interface has dependency on LUT RAM update which is done by
 *          @ref clkNafllLutProgram. Make sure to always call this interface
 *          after LUT is programmed!
 */
FLCN_STATUS
clkNafllLutUpdateRamReadEn_GP10X
(
    CLK_NAFLL_DEVICE  *pNafllDev,
    LwBool             bEnable
)
{
    LwU32  reg32;
    LwU32  data32;
    LwU32  data32Old;

    // Update the RAM READ EN bit to the bEnable state
    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_CFG);
    data32Old = data32 = REG_RD32(FECS, reg32);

    if (bEnable)
    {
        data32 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_LTCLUT_CFG, _RAM_READ_EN, _YES,
                             data32);
    }
    else
    {
        data32 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_LTCLUT_CFG, _RAM_READ_EN, _NO,
                             data32);
    }

    // Write if only the bit flipped
    if (data32Old != data32)
    {
        REG_WR32(FECS, reg32, data32);
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
