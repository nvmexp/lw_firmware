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
 * @file   pmu_clkmontu10x.c
 * @brief  PMU Hal Functions for handling chips specific implementation for 
 * clock monitors
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "clk3/generic_dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "clk/clk_clockmon.h"
#include "clk3/clk3.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// 'UL' Addresses Misra violation 10.8 for an expression below
#define CLK_CLOCK_MON_XTAL_FREQ_MHZ                         27U

#define CLK_CLOCK_MON_REF_WINDOW_DC_CHECK_COUNT_PERCENT     45U

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Mapping between the clk api domain and the various clock monitor registers
 */
static CLK_CLOCK_MON_ADDRESS_MAP s_clockMonMap_TU10X[] =
{
    {
        LW2080_CTRL_CLK_DOMAIN_GPCCLK,
        {
            LW_PTRIM_GPC_BCAST_FMON_THRESHOLD_HIGH_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_THRESHOLD_LOW_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_REF_WINDOW_COUNT_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_REF_WINDOW_DC_CHECK_COUNT_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_CONFIG_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_ENABLE_STATUS_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_FAULT_STATUS_GPCCLK_DIV2,
            LW_PTRIM_GPC_BCAST_FMON_CLEAR_COUNTER_GPCCLK_DIV2,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_SYSCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK,
            LW_PTRIM_SYS_FMON_CONFIG_SYSCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_SYSCLK,
        }
    },
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    {
        LW2080_CTRL_CLK_DOMAIN_HUBCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_HUBCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_HUBCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_HUBCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_HUBCLK,
            LW_PTRIM_SYS_FMON_CONFIG_HUBCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_HUBCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_HUBCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_HUBCLK,
        }
    },
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
    {
        LW2080_CTRL_CLK_DOMAIN_HOSTCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_HOSTCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_HOSTCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_HOSTCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_HOSTCLK,
            LW_PTRIM_SYS_FMON_CONFIG_HOSTCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_HOSTCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_HOSTCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_HOSTCLK,
        }
    },
#endif
    {
        LW2080_CTRL_CLK_DOMAIN_XBARCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_CONFIG_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_XBARCLK_DIV2,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_XBARCLK_DIV2,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_LWDCLK,
        {
            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_LWDCLK,
            LW_PTRIM_SYS_FMON_THRESHOLD_LOW_LWDCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_LWDCLK,
            LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_LWDCLK,
            LW_PTRIM_SYS_FMON_CONFIG_LWDCLK,
            LW_PTRIM_SYS_FMON_ENABLE_STATUS_LWDCLK,
            LW_PTRIM_SYS_FMON_FAULT_STATUS_LWDCLK,
            LW_PTRIM_SYS_FMON_CLEAR_COUNTER_LWDCLK,
        }
    },
    {
        LW2080_CTRL_CLK_DOMAIN_MCLK,
        {
            LW_PTRIM_FBPA_BCAST_FMON_THRESHOLD_HIGH_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_THRESHOLD_LOW_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_REF_WINDOW_COUNT_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_REF_WINDOW_DC_CHECK_COUNT_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_CONFIG_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_ENABLE_STATUS_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_FAULT_STATUS_DRAMCLK,
            LW_PTRIM_FBPA_BCAST_FMON_CLEAR_COUNTER_DRAMCLK,
        }
    },
};

/* ------------------------- Macros and Defines ----------------------------- */
#define CLK_CLOCK_MON_SUPPORTED_NUM  LW_ARRAY_ELEMENTS(s_clockMonMap_TU10X)

#define CLK_CLOCK_MON_REG_GET_TU10X(idx,_type)                                \
    (s_clockMonMap_TU10X[idx].regAddr[CLK_CLOCK_MON_REG_TYPE_##_type])

#define CLK_CLOCK_MON_ENABLE_STATUS_TIMEOUT_US      (1000U)    // 1 ms

#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS                \
    DRF_EXTENT(LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS) :  \
    DRF_BASE(LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS)

#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS_FALSE           \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_REF_CLK_STATUS, _FALSE) | \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_ERR_CLK_STATUS, _FALSE) | \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_CLK_STATUS,     _FALSE)

#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS_TRUE           \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_REF_CLK_STATUS, _TRUE) | \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_ERR_CLK_STATUS, _TRUE) | \
    DRF_DEF(_PTRIM_SYS, _FMON_ENABLE_STATUS, _FMON_ENABLE_FMON_CLK_STATUS,     _TRUE)

/* ------------------------- Prototypes ------------------------------------- */
static LwU32 s_clkClockMonIdxGetFromApiDomain(LwU32 clkApiDomain)
    GCC_ATTRIB_SECTION("imem_libClockMon", "s_clkClockMonIdxGetFromApiDomain");
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Interface to enable or disable clock monitors
 *
 * @param[in]   clkApiDomain    Clock domain LW2080_CTRL_CLK_DOMAIN_ value
 * @param[in]   bEnable         Boolean value to enable to disable clock monitors
 *
 * @return FLCN_OK                    Clock Monitor enabled or disabled as per request
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid Api domain for clock monitors
 * @return FLCN_ERR_ILWALID_STATE     Status registers not cleared or set as expected
 */
FLCN_STATUS
clkClockMonEnable_TU10X
(
    LwU32  clkApiDomain,
    LwBool bEnable
)
{
    LwU32       clkMonIdx;
    LwU32       reg32;
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    clkMonIdx = s_clkClockMonIdxGetFromApiDomain(clkApiDomain);
    if (clkMonIdx >= CLK_CLOCK_MON_SUPPORTED_NUM)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkClockMonEnable_TU10X_exit;
    }

    // Disable clock monitors
    if (!bEnable)
    {
        // 1.Clear the fmon_enable (FMON_ENABLE) bit.
        reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_CONFIG);
        data32 = REG_RD32(FECS, reg32);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_ENABLE, _DISABLE, data32);
        REG_WR32(FECS, reg32, data32);

        // 2.Read fmon_enable_status field to make sure all bits are cleared.
        reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_ENABLE_STATUS);
#ifndef UTF_REG_MOCKING
        if (!PMU_REG32_POLL_US(
                USE_FECS(reg32),                                                        // Address
                DRF_SHIFTMASK(LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS), // Mask
                LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS_FALSE,          // Value
                CLK_CLOCK_MON_ENABLE_STATUS_TIMEOUT_US,                                 // Timeout
                PMU_REG_POLL_MODE_BAR0_EQ))                                             // Mode
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkClockMonEnable_TU10X_exit;
        }
#endif
    }
    else
    {
        // 5.Enable all the 4 kinds of faults (FMON_CONFIG).
        reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_CONFIG);
        data32 = REG_RD32(FECS, reg32);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_REPORT_OVERFLOW_ERROR, _ENABLE, data32);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_REPORT_LOW_THRESH_VIOL, _ENABLE, data32);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_REPORT_HIGH_THRESH_VIOL, _ENABLE, data32);
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_REPORT_DC_FAULT_VIOL, _ENABLE, data32);
        REG_WR32(FECS, reg32, data32);

        // 6.Enable ref_clk_window (FMON_REF_CLK_WINDOW_EN)
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_REF_CLK_WINDOW_EN, _ENABLE, data32);
        REG_WR32(FECS, reg32, data32);

        // 7.Enable fmon counter (FMON_ENABLE_FMON_COUNTER)
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_ENABLE_FMON_COUNTER, _ENABLE, data32);
        REG_WR32(FECS, reg32, data32);

        // 8.Enable fmon_enable (FMON_ENABLE)
        data32 = FLD_SET_DRF(_PTRIM_SYS, _FMON_CONFIG, _FMON_ENABLE, _ENABLE, data32);
        REG_WR32(FECS, reg32, data32);

        // 9.Read the per clock domain fmon_enable_status fields (FMON_ENABLE_STATUS) to make sure all bits are set.
        reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_ENABLE_STATUS);
#ifndef UTF_REG_MOCKING
        if (!PMU_REG32_POLL_US(
                USE_FECS(reg32),                                                        // Address
                DRF_SHIFTMASK(LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS), // Mask
                LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_STATUS_TRUE,           // Value
                CLK_CLOCK_MON_ENABLE_STATUS_TIMEOUT_US,                                 // Timeout
                PMU_REG_POLL_MODE_BAR0_EQ))                                             // Mode
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkClockMonEnable_TU10X_exit;
        }
#endif
    }

clkClockMonEnable_TU10X_exit:
    return status;
}

/*!
 * @brief Interface to program clock monitors
 *
 * @param[in] pClkDomainMonListItem   Pointer to LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM value
 *
 * @return FLCN_OK                    Clock Monitor enabled or disabled as per request
 * @return FLCN_ERR_ILWALID_ARGUMENT  Invalid Api domain for clock monitors
 */
FLCN_STATUS
clkClockMonProgram_TU10X
(
    LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM *pClkDomainMonListItem
)
{
    LwU32       clkMonIdx;
    LwU32       reg32;
    LwU32       data32;
    LwU32       refClkFreqMHz;
    LwU32       nRefcount;
    LwU32       threshRefcount;
    LwU32       clkMonLowThreshold;
    LwU32       clkMonHighThreshold;
    LwU64       tmp1 = 0;
    LwU64       tmp2 = 0;
    FLCN_STATUS status = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    clkMonIdx = s_clkClockMonIdxGetFromApiDomain(pClkDomainMonListItem->clkApiDomain);
    if (clkMonIdx >= CLK_CLOCK_MON_SUPPORTED_NUM)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkClockMonProgram_TU10X_exit;
    }

    // Eq 1. Nrefcount = Fmon_Ref_Window x Frefclk
    status = clkClockMonRefFreqGet_HAL(&Clk,
                pClkDomainMonListItem->clkApiDomain, &refClkFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClockMonProgram_TU10X_exit;
    }

    nRefcount = (LwU32)Clk.domains.clkMonRefWinUsec * refClkFreqMHz;

    // 3.Use Eq 1 to program the FMON_REF_WINDOW_COUNT and FMON_REF_WINDOW_DC_CHECK_COUNT
    reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_REF_WINDOW_COUNT);
    data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _FMON_REF_WINDOW_COUNT, _FMON_REF_WINDOW_COUNT, nRefcount, data32);
    REG_WR32(FECS, reg32, data32);

    //
    // DC_CHECK_COUNT set to 45 % of REF_WINDOW_COUNT as suggested by Arun Kumar PV
    //
    reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_REF_WINDOW_DC_CHECK_COUNT);
    data32 = REG_RD32(FECS, reg32);
    data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _FMON_REF_WINDOW_DC_CHECK_COUNT, _FMON_REF_WINDOW_DC_CHECK_COUNT,
                (nRefcount * CLK_CLOCK_MON_REF_WINDOW_DC_CHECK_COUNT_PERCENT) / 100U, data32);
    REG_WR32(FECS, reg32, data32);

    // Read the clock domain frequency
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Eq 2. C = Fmon_Ref_Window x  Fmonclk
        threshRefcount = Clk.domains.clkMonRefWinUsec * (pClkDomainMonListItem->clkFreqMHz);

        //
        // Eq 3. Cthlow = C (1 - Ethlow)
        //      lowThreshold = ((threshRefcount(32.0) * (1 - lowThresholdPercent)(20.12)) = result(52.12) >> 12
        //
        tmp1 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, 1);
        tmp2 = (LwU64) pClkDomainMonListItem->lowThresholdPercent;
        lw64Sub(&tmp1, &tmp1, &tmp2);
        tmp2 = (LwU64) threshRefcount;
        lwU64Mul(&tmp1, &tmp2, &tmp1);
        clkMonLowThreshold = (LwU32)(LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(52, 12, tmp1));

        //
        // Eq 4. Cthhigh = C (1 + Ethhigh)
        //      highThreshold = ((threshRefcount(32.0) * (1 + highThresholdPercent)(20.12)) = result(52.12) >> 12
        //
        tmp1 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, 1);
        tmp2 = (LwU64) pClkDomainMonListItem->highThresholdPercent;
        lw64Add(&tmp1, &tmp1, &tmp2);
        tmp2 = (LwU64) threshRefcount;
        lwU64Mul(&tmp1, &tmp2, &tmp1);
        clkMonHighThreshold = (LwU32)(LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(52, 12 , tmp1));

        // 4.Use Eq 3 and 4 to program the lower (FMON_COUNT_THRESH_LOW) and upper threshold (FMON_COUNT_THRESH_HIGH).
        reg32 = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_THRESHOLD_HIGH);
        data32 = REG_RD32(FECS, reg32);
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _FMON_THRESHOLD_HIGH, _FMON_COUNT_THRESH_HIGH, clkMonHighThreshold, data32);
        REG_WR32(FECS, reg32, data32);

        reg32  = CLK_CLOCK_MON_REG_GET_TU10X(clkMonIdx, FMON_THRESHOLD_LOW);
        data32 = REG_RD32(FECS, reg32);
        data32 = FLD_SET_DRF_NUM(_PTRIM_SYS, _FMON_THRESHOLD_LOW, _FMON_COUNT_THRESH_LOW, clkMonLowThreshold, data32);
        REG_WR32(FECS, reg32, data32);

clkClockMonProgram_TU10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Checks if PRIMARY FAULT_STATUS register is cleared or not
 *
 * @return FLCN_OK                 FAULT_STATUS register is cleared
 * @return FLCN_ERR_ILWALID_STATE  On error
 */
FLCN_STATUS
clkClockMonCheckFaultStatus_TU10X
(
    void
)
{
    LwU32       data32;
    FLCN_STATUS status = FLCN_OK;

    data32  = REG_RD32(FECS, LW_PTRIM_SYS_FMON_MASTER_STATUS_REGISTER);

    if (FLD_TEST_DRF(_PTRIM_SYS, _FMON_MASTER_STATUS_REGISTER, _FINAL_FMON_FAULT_OUT_STATUS, _FALSE, data32))
    {
        goto clkClockMonCheckFaultStatus_TU10X_exit;
    }
    else
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkClockMonCheckFaultStatus_TU10X_exit;
    }

clkClockMonCheckFaultStatus_TU10X_exit:
    return status;
}

/*!
 * @brief Obtains the reference frequency for clock monitors
 *
 * @param[in]  clkApiDomain     Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[out] pClkFreqMHz      Buffer to return the reference frequency
 * 
 * @return FLCN_OK  Always
 */
FLCN_STATUS
clkClockMonRefFreqGet_TU10X
(
    LwU32   clkApiDomain,
    LwU32  *pClkFreqMHz
)
{
    *pClkFreqMHz = CLK_CLOCK_MON_XTAL_FREQ_MHZ;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Returns index for given clkApiDomain
 *
 * @param[in]   clkApiDomain          Clock domain LW2080_CTRL_CLK_DOMAIN_ value
 *
 * @return   Get the clock mon index for the given clock api domain
 */
static LwU32
s_clkClockMonIdxGetFromApiDomain
(
    LwU32 clkApiDomain
)
{
    LwU32 idx;

    for (idx = 0; idx < CLK_CLOCK_MON_SUPPORTED_NUM; idx++)
    {
        if (s_clockMonMap_TU10X[idx].clkApiDomain == clkApiDomain)
        {
            break;
        }
    }

    return idx;
}
