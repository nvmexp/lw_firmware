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
 * @file   pmu_clkavfsgp100.c
 * @brief  PMU Hal Functions for GP100 for Clocks AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * GP100 implementation differs from GP102+ since GP100 was running out of DMEM
 * and we had to aggressively decrease its memory footprint.
 */
#define CLK_NAFLL_REG_GET_GP100(_pNafllDev,_type)                              \
    ((_pNafllDev)->nafllRegAddrBase +                                          \
     (LW_PTRIM_SYS_NAFLL_SYS##_type -                                          \
      LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_ADDR))

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Initialize various one-time parameters of the LUT
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 *
 * @return FLCN_OK                   sucessfully initialized the LUT parameters
 * @return FLCN_ERR_ILWALID_INDEX    _nafllMap_GP100 doesn't have the entry for
 *                                   the NAFLL device
 */
FLCN_STATUS
clkNafllLutParamsInit_GP100
(
    CLK_NAFLL_DEVICE *pNafllDev
)
{
    CLK_LUT_DEVICE *pLutDevice = &pNafllDev->lutDevice;
    LwU32           reg32;
    LwU32           data32;
    FLCN_STATUS     status = LW_OK;

    // Sanity check: Values above threshold are out of range
    if (pLutDevice->hysteresisThreshold > CLK_NAFLL_LUT_ILWALID_HYSTERESIS_THRESHOLD)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllLutParamsInit_GP100_exit;
    }
    else if (pLutDevice->hysteresisThreshold < CLK_NAFLL_LUT_ILWALID_HYSTERESIS_THRESHOLD)
    {
        reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_CFG);
        data32 = REG_RD32(FECS, reg32);
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_CFG,
                                 _HYSTERESIS_THRESHOLD,
                                 pLutDevice->hysteresisThreshold, data32);
        REG_WR32(FECS, reg32, data32);
    }
    //
    // Use the default hysteresis(INIT value of register) when not below
    // invalid value(0xF) in VBIOS, do not change the register value
    //

    // Update the VSELECT mode
    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);
    switch (pLutDevice->vselectMode)
    {
        case LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_LOGIC:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _LUT_VSELECT, _LOGIC, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_MIN:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _LUT_VSELECT, _MIN, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_LUT_VSELECT_SRAM:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _LUT_VSELECT, _SRAM, data32);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkNafllLutParamsInit_GP100_exit;
        }
    }

    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                             _LUT_VSELECT,
                             pLutDevice->vselectMode, data32);
    REG_WR32(FECS, reg32, data32);

clkNafllLutParamsInit_GP100_exit:
    return status;
}

/*!
 * @brief Update the temperature Index of the LUT
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   temperatureIdx  temp index to set
 *
 * @return FLCN_OK                   LUT temperature Index set successfully
 * @return FLCN_ERR_ILWALID_INDEX     _nafllMap_GP100 doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllTempIdxUpdate_GP100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                temperatureIdx
)
{
    LwU32   reg32;
    LwU32   data32;

    // Update the temperature index
    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_CFG);
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
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid ID or temperature Idx
 * @return FLCN_ERR_ILWALID_INDEX     _nafllMap_GP100 doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLutAddrRegWrite_GP100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                temperatureIdx
)
{
    LwU32   addressVal;

    if ((pNafllDev == NULL) ||
        (temperatureIdx > CLK_LUT_TEMP_IDX_MAX))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    addressVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_ADDR, _OFFSET,
                    (temperatureIdx * CLK_LUT_STRIDE), 0);
    addressVal = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_ADDR,
                    _AUTO_INC, _ON, addressVal);

    REG_WR32(FECS, CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_WRITE_ADDR), addressVal);

    return FLCN_OK;
}

/*!
 * @brief Update the LUT V10 table entries
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   pLutEntries     Pointer to the LUT entries
 * @param[in]   numLutEntries   Number of entries to be updated in LUT
 *
 * @return FLCN_OK                   LUT updated successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid Nafll device
 * @return FLCN_ERR_ILWALID_INDEX     _nafllMap_GP100 doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLut10DataRegWrite_GP100
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
    for (entryIdx = 0; entryIdx < numLutEntries; entryIdx += 2)
    {
        LW2080_CTRL_CLK_LUT_VF_ENTRY_DATA *pLutEntryNext = NULL;

        if ((entryIdx + 1) < numLutEntries)
        {
            pLutEntryNext = pLutEntries + 1;
        }
        // Pack 2 V/F entries into one DWORD
        dataVal = clkNafllLut10EntriesPack_HAL(&Clk,
                     pLutEntries->lut10.ndiv,
                     pLutEntries->lut10.vfgain,
                     (pLutEntryNext != NULL ?
                      pLutEntryNext->lut10.ndiv : 0),
                     (pLutEntryNext != NULL ?
                      pLutEntryNext->lut10.vfgain : 0));

        pLutEntries += 2;

        REG_WR32(FECS, CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_WRITE_DATA), dataVal);
    }

    return FLCN_OK;
}

/*!
 * @brief Get the current value of LUT V10 overrides.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[out]  pOverrideMode   The current LUT override mode (NOT SUPPORTED)
 * @param[out]  pNdiv           The current NDIV value
 * @param[out]  pVfGain         The current VFGAIN value
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    _nafllMap_GP10X doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLut10OverrideGet_GP100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8               *pOverrideMode,
    LwU16              *pNdiv,
    LwU16              *pVfGain
)
{
    LwU32   reg32;
    LwU32   data32;

    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);

    if (pNdiv != NULL) 
    {
        *pNdiv = DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _NDIV, data32);
        PMU_HALT_COND(*pNdiv != 0);
    }

    if (pVfGain != NULL) 
    {
        *pVfGain = DRF_VAL(_PTRIM_SYS, _NAFLL_SYSLUT_SW_FREQ_REQ, _VFGAIN, data32);
        PMU_HALT_COND(*pVfGain != 0);
    }

    if (pOverrideMode != NULL) 
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief Set the LUT V10 SW override
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[in]   overrideMode    The LUT override mode to set
 * @param[in]   ndiv            The NDIV value to override
 * @param[in]   vfgain          The VFGAIN value to override
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX     _nafllMap_GP100 doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLut10OverrideSet_GP100
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideMode,
    LwU16               ndiv,
    LwU16               vfgain
)
{
    LwU32   reg32;
    LwU32   data32;

    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);
    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_HW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_SW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_MIN, data32);
            break;
        }
        default:
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                             _NDIV, ndiv, data32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                             _VFGAIN, vfgain, data32);

    REG_WR32(FECS, reg32, data32);

    return FLCN_OK;
}

/*!
 * @brief Get bitmask of all supported NAFLLs on this chip
 *
 * @param[out]   pMask     Pointer to the bitmask
 */
void
clkNafllAllMaskGet_GP100
(
    LwU32   *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_NAFLL_ID_SYS) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_LTC) |
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
 * @return PM_OK                     The PLDIV is set successfully
 * @retrun FLCN_ERR_ILWALID_ARGUMENT  If the pNafllDev is NULL
 * @return FLCN_ERR_ILWALID_INDEX     _nafllMap_GP100 doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllPldivSet_GP100
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
    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, NAFLL_COEFF);
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
 * @return pldiv    The set PLDIV value for the NAFLL device
 */
LwU8
clkNafllPldivGet_GP100
(
   CLK_NAFLL_DEVICE  *pNafllDev
)
{
    LwU32  reg32;
    LwU32  data32;
    LwU8   pldiv;

    // Read the coefficient register for PLDIV
    reg32  = CLK_NAFLL_REG_GET_GP100(pNafllDev, NAFLL_COEFF);
    data32 = REG_RD32(FECS, reg32);
    pldiv  = DRF_VAL(_PTRIM_SYS, _NAFLL_SYSNAFLL_COEFF, _PDIV, data32);

    // A value of 0 is same as DIV1
    if (pldiv == 0) 
    {
        pldiv =  CLK_NAFLL_PLDIV_DIV1;
    }

    return pldiv;
}

/*!
 * @brief Each DWORD in the LUT V10 can hold two V/F table entries. Merge two entries
 *        into a single DWORD before writing to the LUT.
 *
 * @param[in]   ndiv1    ndiv   value for entry 'i'
 * @param[in]   vfgain1  vfgain value for entry 'i'
 * @param[in]   ndiv2    ndiv   value for entry 'i + 1'
 * @param[in]   vfgain2  vfgain value for entry 'i + 1'
 *
 * @return the 32-bit packed value with all the 'in' params
 */
LwU32 clkNafllLut10EntriesPack_GP100
(
    LwU16 ndiv1,
    LwU16 vfgain1,
    LwU16 ndiv2,
    LwU16 vfgain2
)
{
    LwU32 dataVal;
    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_DATA,
                              _VAL0_VFGAIN, vfgain1, 0);
    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_DATA,
                              _VAL0_NDIV, ndiv1, dataVal);

    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_DATA,
                              _VAL1_VFGAIN, vfgain2, dataVal);
    dataVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_WRITE_DATA,
                              _VAL1_NDIV, ndiv2, dataVal);
    return dataVal;
}

/*!
 * @brief   Initialize register maping within NAFLL device
 *
 * @param[in]   pNafllDev   Pointer to the NAFLL device
 *
 * @return  FLCN_ERR_ILWALID_INDEX  If reister mapping init has failed
 * @return  FLCN_OK                 Otherwise
 */
FLCN_STATUS
clkNafllRegMapInit_GP100
(
   CLK_NAFLL_DEVICE    *pNafllDev
)
{
    LwU8    nafllId = pNafllDev->id;
    LwU32   nafllRegAddrBase;

    switch (nafllId)
    {
        case LW2080_CTRL_CLK_NAFLL_ID_SYS:
            nafllRegAddrBase = LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_ADDR;
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_LTC:
            nafllRegAddrBase = LW_PTRIM_SYS_NAFLL_LTCLUT_WRITE_ADDR;
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_XBAR:
            nafllRegAddrBase = LW_PTRIM_SYS_NAFLL_XBARLUT_WRITE_ADDR;
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC0:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(0);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC1:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(1);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC2:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(2);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC3:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(3);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC4:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(4);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPC5:
            nafllRegAddrBase = LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(5);
            break;
        case LW2080_CTRL_CLK_NAFLL_ID_GPCS:
            nafllRegAddrBase = LW_PTRIM_GPC_BCAST_GPCLUT_WRITE_ADDR;
            break;
        default:
            return FLCN_ERR_ILWALID_INDEX;
    }

    pNafllDev->nafllRegAddrBase = nafllRegAddrBase;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
