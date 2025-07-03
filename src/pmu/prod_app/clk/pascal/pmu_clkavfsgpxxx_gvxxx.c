/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkavfsgpxxx_gvxxx.c
 * @brief  PMU Hal Functions from GP10X to VOLTA for Clocks AVFS
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
 * @brief Initialize various one-time parameters of the LUT
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 *
 * @return FLCN_OK                   sucessfully initialized the LUT parameters
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry for
 *                                   the NAFLL device
 */
FLCN_STATUS
clkNafllLutParamsInit_GP10X
(
    CLK_NAFLL_DEVICE *pNafllDev
)
{
    CLK_LUT_DEVICE *pLutDevice = &pNafllDev->lutDevice;
    LwU32           reg32;
    LwU32           data32;
    FLCN_STATUS     status = FLCN_OK;

    // Sanity check: Values above threshold are out of range
    if (pLutDevice->hysteresisThreshold > CLK_NAFLL_LUT_ILWALID_HYSTERESIS_THRESHOLD)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkNafllLutParamsInit_GP10X_exit;
    }
    else if (pLutDevice->hysteresisThreshold < CLK_NAFLL_LUT_ILWALID_HYSTERESIS_THRESHOLD)
    {
        reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_CFG);
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
    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
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
            goto clkNafllLutParamsInit_GP10X_exit;
        }
    }

    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                             _LUT_VSELECT,
                             pLutDevice->vselectMode, data32);
    REG_WR32(FECS, reg32, data32);

clkNafllLutParamsInit_GP10X_exit:
    return status;
}

/*!
 * @brief Get the current value of LUT V10 overrides.
 *
 * @param[in]   pNafllDev       Pointer to the NAFLL device
 * @param[out]  pOverrideMode   The current LUT override mode
 * @param[out]  pNdiv           The current NDIV value
 * @param[out]  pVfGain         The current VFGAIN value
 *
 * @return FLCN_OK                   LUT override set successfully
 * @return FLCN_ERR_ILWALID_INDEX    ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLut10OverrideGet_GP10X
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8               *pOverrideMode,
    LwU16              *pNdiv,
    LwU16              *pVfGain
)
{
    LwU32   reg32;
    LwU32   data32;

    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
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
        *pOverrideMode = DRF_VAL(_PTRIM_SYS,
                _NAFLL_LTCLUT_SW_FREQ_REQ, _SW_OVERRIDE_NDIV, data32);
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
 * @return FLCN_ERR_ILWALID_INDEX     ClkNafllRegMap_HAL doesn't have the entry
 *                                   for the NAFLL device
 */
FLCN_STATUS
clkNafllLut10OverrideSet_GP10X
(
    CLK_NAFLL_DEVICE   *pNafllDev,
    LwU8                overrideMode,
    LwU16               ndiv,
    LwU16               vfgain
)
{
    LwU32   reg32;
    LwU32   data32;

    reg32  = CLK_NAFLL_REG_GET(pNafllDev, LUT_SW_FREQ_REQ);
    data32 = REG_RD32(FECS, reg32);
    switch (overrideMode)
    {
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_HW_REQ, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_HW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_SW_REQ, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_SW_REQ, data32);
            break;
        }
        case LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN:
        {
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_NDIV, _USE_MIN, data32);
            data32 = FLD_SET_DRF(_PTRIM_SYS, _NAFLL_LTCLUT_SW_FREQ_REQ,
                                 _SW_OVERRIDE_VFGAIN, _USE_MIN, data32);
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
 * @brief Each DWORD in the LUT V10 can hold two V/F table entries. Merge
 *        two entries into a single DWORD before writing to the LUT.
 *
 * @param[in]   ndiv1    ndiv   value for entry 'i'
 * @param[in]   vfgain1  vfgain value for entry 'i'
 * @param[in]   ndiv2    ndiv   value for entry 'i + 1'
 * @param[in]   vfgain2  vfgain value for entry 'i + 1'
 *
 * @return the 32-bit packed value with all the 'in' params
 */
LwU32 clkNafllLut10EntriesPack_GP10X
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

/* ------------------------- Private Functions ------------------------------ */
