/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkavfsad10x.c
 * @brief  PMU Hal Functions for AD10X+ for Clocks AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get bitmask of all supported NAFLLs on this chip
 *
 * @param[out]   pMask     Pointer to the bitmask
 */
void
clkNafllAllMaskGet_AD10X
(
    LwU32   *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_NAFLL_ID_SYS)   |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_XBAR)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC2)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC3)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC4)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC5)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC6)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC7)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC8)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC9)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC10) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC11) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPCS)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_HOST)  |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_LWD);
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
clkNafllLutUpdateRamReadEn_AD10X
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
        data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _RAM_READ_EN, _YES,
                             data32);

        //
        // RAM_READ_EN bits for NDIV OFFSET and CPM MAX NDIV LUT types are
        // applicable only for GPC LUTs.
        //
        if (((LWBIT_TYPE(pNafllDev->id, LwU32) & LW2080_CTRL_CLK_NAFLL_MASK_UNICAST_GPC) != 0) ||
            (pNafllDev->id == LW2080_CTRL_CLK_NAFLL_ID_GPCS))
        {
            data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _OFFSET_RAM_READ_EN, _YES,
                                data32);

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
            {
                data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _CPM_RAM_READ_EN, _YES,
                                    data32);
            }
        }
    }
    else
    {
        data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _RAM_READ_EN, _NO,
                             data32);

        //
        // RAM_READ_EN bits for NDIV OFFSET and CPM MAX NDIV LUT types are
        // applicable only for GPC LUTs.
        //
        if (((LWBIT_TYPE(pNafllDev->id, LwU32) & LW2080_CTRL_CLK_NAFLL_MASK_UNICAST_GPC) != 0) ||
            (pNafllDev->id == LW2080_CTRL_CLK_NAFLL_ID_GPCS))
        {
            data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _OFFSET_RAM_READ_EN, _NO,
                                data32);

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
            {
                data32 = FLD_SET_DRF(_PTRIM, _GPC_GPCLUT_CFG, _CPM_RAM_READ_EN, _NO,
                                    data32);
            }
        }
    }

    // Write if only the bit flipped
    if (data32Old != data32)
    {
        REG_WR32(FECS, reg32, data32);
    }

    return FLCN_OK;
}

/*!
 * @brief Update the register that controls operation of the quick_slowdown
 * module based on SW support / capabilities.
 *
 * @return FLCN_OK                Operation completed successfully
 * @return FLCN_ERR_ILWALID_STATE If the slowdown control fields are not
 *                                consistent with devinit values
 *
 * @note We originally required REQ_OVR, REQ_OVR_VAL programming due to a bug
 * in the quick slowdown FSM. Forking this HAL to update the seq after the FSM
 * issue fix.
 */
FLCN_STATUS
clkNafllLutUpdateQuickSlowdown_AD10X(void)
{
    LwU32       reg32;
    LwU32       data32;
    LwU32       data32Old;

    //
    // Enable the quick slowdown feature if supported i.e. VBIOS has provided
    // the required secondary V/F lwrve data
    //
    if (PMU_CLK_NAFLL_IS_SEC_LWRVE_ENABLED())
    {
        reg32  = LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL;
        data32Old = data32 = REG_RD32(FECS, reg32);

        data32 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL,
                             _ENABLE, _YES, data32);

        if (data32Old != data32)
        {
            REG_WR32(FECS, reg32, data32);
        }

        // Check and enable the second secondary V/F lwrve, if supported
        if (clkDomainsNumSecVFLwrves() > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
        {
            reg32  = LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_B_CTRL;
            data32Old = data32 = REG_RD32(FECS, reg32);

            data32 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_AVFS_QUICK_SLOWDOWN_B_CTRL,
                                 _ENABLE, _YES, data32);

            if (data32Old != data32)
           {
                 REG_WR32(FECS, reg32, data32);
            }
        }
    }

    return FLCN_OK;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_LUT_QUICK_SLOWDOWN_ENGAGE_SUPPORT)
/*!
 * @brief Force engage/disengage quick slowdown feature per NAFLL device per
 *        secondary VF lwrve
 *
 * @param[in]   pNafllDev                   Pointer to the NAFLL device
 * @param[in]   bQuickSlowdownForceEngage   Array of booleans to force the state
 *                                          of the quick slowdown feature
 * @param[in]   numVfLwrves                 Number of VF lwrves to enforce the
 *                                          quick slowdown
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT  If number of VF lwrves passed are more
 *                                    enabled.
 *         FLCN_OK                    Otherwise
 *
 * @note This interface can only be called after the LUT is programmed and only
 * supported if the secondary VF lwrves are supported/enabled.
 */
FLCN_STATUS 
clkNafllLutQuickSlowdownForceEngage_AD10X
(
    CLK_NAFLL_DEVICE *pNafllDev,
    LwBool            bQuickSlowdownForceEngage[],
    LwU8              numVfLwrves
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32  reg32;
    LwU32  data32;
    LwU32  data32Old;

    if (numVfLwrves > clkDomainsNumSecVFLwrves())
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkNafllLutQuickSlowdownForceEngage_AD10X_exit;
    }

    if (PMU_CLK_NAFLL_IS_SEC_LWRVE_ENABLED())
    {
        reg32 = CLK_NAFLL_REG_GET(pNafllDev, QUICK_SLOWDOWN_CTRL);
        data32Old = data32 = REG_RD32(FECS, reg32);

        // Engage/disengage quick_slowdown
        if (bQuickSlowdownForceEngage[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_0])
        {
            data32 = FLD_SET_DRF(_PTRIM, _GPC_AVFS_QUICK_SLOWDOWN_CTRL,
                                 _REQ_SW_SLOWDOWN, _YES,
                                 data32);
        }
        else
        {
            data32 = FLD_SET_DRF(_PTRIM, _GPC_AVFS_QUICK_SLOWDOWN_CTRL,
                                 _REQ_SW_SLOWDOWN, _NO,
                                 data32);
        }

        // Write if only the bit flipped
        if (data32Old != data32)
        {
            REG_WR32(FECS, reg32, data32);
        }

        if (numVfLwrves > LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1)
        {
            reg32 = CLK_NAFLL_REG_GET(pNafllDev, QUICK_SLOWDOWN_B_CTRL);
            data32Old = data32 = REG_RD32(FECS, reg32);

            // Engage/disengage quick_slowdown
            if (bQuickSlowdownForceEngage[LW2080_CTRL_CLK_NAFLL_LUT_VF_LWRVE_SEC_1])
            {
                data32 = FLD_SET_DRF(_PTRIM, _GPC_AVFS_QUICK_SLOWDOWN_B_CTRL,
                                     _REQ_SW_SLOWDOWN, _YES,
                                     data32);
            }
            else
            {
                data32 = FLD_SET_DRF(_PTRIM, _GPC_AVFS_QUICK_SLOWDOWN_B_CTRL,
                                     _REQ_SW_SLOWDOWN, _NO,
                                     data32);
            }

            // Write if only the bit flipped
            if (data32Old != data32)
            {
                REG_WR32(FECS, reg32, data32);
            }
        }
    }

clkNafllLutQuickSlowdownForceEngage_AD10X_exit:
    return status;
}
#endif

/* ------------------------- Private Functions ------------------------------ */
