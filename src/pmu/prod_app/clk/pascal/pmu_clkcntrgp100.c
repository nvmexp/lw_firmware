/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntrgp100.c
 * @brief  PMU Hal Functions for generic GP100+ clock counter functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objfuse.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*
 * Special macro to define maximum number of supported clock counters.
 * { SYS + LTC + XBAR + GPC + 6 unicast GPCs + MCLK }
 */
#define CLK_CNTR_ARRAY_SIZE_MAX_GP100              11

/*
 * Helper macro to get clock counter register.
 */
#define CLK_CNTR_REG_GET_GP100(pCntr, type)                                    \
    ((pCntr)->cfgReg + (LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_##type -       \
      LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG))

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static inline FLCN_STATUS s_clkCntrEnable_GP100(CLK_CNTR *pCntr)
    GCC_ATTRIB_ALWAYSINLINE();
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initializes all SW state associated with the Clock Counter feature.
 */
void
clkCntrInitSw_GP100()
{
    LwU8        numCntrs    = 0;
    LwU8        domIdx      = 0;
    LwU8        gpcIdx      = 0;
    LwU32       clkDomMask  = 0;
    FLCN_STATUS status      = FLCN_OK;

    // Initialize CLK_CNTRS SW state.
    Clk.cntrs.pCntr         = clkCntrArrayGet_HAL(&Clk);
    Clk.cntrs.hwCntrSize    = clkCntrWidthGet_HAL(&Clk);

    // Initialize per-clock counter SW state.
    clkDomMask = clkCntrDomMaskGet_HAL(&Clk);
    FOR_EACH_INDEX_IN_MASK(32, domIdx, clkDomMask)
    {
        Clk.cntrs.pCntr[numCntrs].clkDomain = BIT(domIdx);
        Clk.cntrs.pCntr[numCntrs].partIdx = LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;
        status = clkCntrDomInfoGet_HAL(&Clk, &Clk.cntrs.pCntr[numCntrs]);
        if (status != FLCN_OK)
        {
            PMU_HALT();
        }
        numCntrs++;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Unicast GPCs
    if ((clkDomMask & CLK_DOMAIN_MASK_GPC) != 0)
    {
        LwU32 gpcMask = clkCntrGpcMaskGet_HAL(&Clk);
        FOR_EACH_INDEX_IN_MASK(32, gpcIdx, gpcMask)
        {
            Clk.cntrs.pCntr[numCntrs].clkDomain = CLK_DOMAIN_MASK_GPC;
            Clk.cntrs.pCntr[numCntrs].partIdx = gpcIdx;
            status = clkCntrDomInfoGet_HAL(&Clk, &Clk.cntrs.pCntr[numCntrs]);
            if (status != FLCN_OK)
            {
                PMU_HALT();
            }
            numCntrs++;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    Clk.cntrs.numCntrs = numCntrs;
}

/*!
 * Returns pointer to the static clock counter array.
 */
CLK_CNTR *
clkCntrArrayGet_GP100()
{
    static CLK_CNTR ClkCntrArray_GP100[CLK_CNTR_ARRAY_SIZE_MAX_GP100] = {{ 0 }};
    return ClkCntrArray_GP100;
}

/*!
 * Returns the bit-width of the HW clock counter.
 *
 * @return Counter bit-width.
 */
LwU8
clkCntrWidthGet_GP100()
{
    return (LwU8)(DRF_SIZE(LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CNT0_LSB) +
        DRF_SIZE(LW_PTRIM_GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CNT1_MSB));
}

/*!
 * Initialize the HW state of supported clock counters.
 */
void
clkCntrInitHw_GP100()
{
    FLCN_STATUS status;
    LwU8        i = 0;

    while (i < Clk.cntrs.numCntrs)
    {
        status = clkCntrEnable_HAL(&Clk, &Clk.cntrs.pCntr[i++]);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }
}

/*!
 * INIT time version of @ref s_clkCntrEnable_GP100
 */
FLCN_STATUS
clkCntrEnable_GP100
(
    CLK_CNTR   *pCntr
)
{
    return s_clkCntrEnable_GP100(pCntr);
}

/*!
 * Runtime version of @ref s_clkCntrEnable_GP100
 */
FLCN_STATUS
clkCntrEnableRuntime_GP100
(
    CLK_CNTR   *pCntr
)
{
    return s_clkCntrEnable_GP100(pCntr);
}

/*!
 * Reads current HW counter value for the requested counter
 *
 * @param[in] pCntr     Pointer to the requested clock counter object.
 *
 * @return Current HW counter value.
 */
LwU64
clkCntrRead_GP100
(
    CLK_CNTR   *pCntr
)
{
    LWU64_TYPE cntr;
    LwU32      data;

    // Stop updating counter value in registers.
    data = REG_RD32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG));
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
        _CONTINOUS_UPDATE, _DISABLED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    // Read counter value
    LWU64_LO(cntr) = DRF_VAL(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CNT0,
        _LSB, REG_RD32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CNT0)));
    LWU64_HI(cntr) = DRF_VAL(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CNT1,
        _MSB, REG_RD32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CNT1)));

    // Start updating counter value in registers.
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
        _CONTINOUS_UPDATE, _ENABLED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    return cntr.val;
}

/*!
 * Get the mask of enabled GPCs.
 *
 * @return  Mask of enabled GPCs.
 *
 */
LwU32
clkCntrGpcMaskGet_GP100()
{
    LwU32 gpcMask;
    LwU32 maxGpcCount;

    // Read max GPC count
    maxGpcCount = DRF_VAL(_PTOP, _SCAL_NUM_GPCS, _VALUE,
                    REG_RD32(BAR0, LW_PTOP_SCAL_NUM_GPCS));

    //
    // Read GPC mask from fuse.
    // In case LW_FUSE_STATUS_OPT_GPC_IDX_ENABLE is zero below becomes
    // GPC floorswept mask else it is GPC mask.
    //
    gpcMask = fuseRead(LW_FUSE_STATUS_OPT_GPC);

    // Ilwert the mask value if 0 is enabled.
#if LW_FUSE_STATUS_OPT_GPC_IDX_ENABLE == 0
    gpcMask = ~gpcMask;
#endif

    // Trim the mask based on max supported GPCs.
    gpcMask = gpcMask & (BIT(maxGpcCount) - 1);

    return gpcMask;
}

/*!
 * Get the mask of supported clock domains
 *
 * @return  Mask of supported clock doamins.
 *
 */
LwU32
clkCntrDomMaskGet_GP100()
{
    LwU32 clkDomMask;

    clkDomMask =
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_SYS2CLK_CAPABILITY_INIT != 0)
                    LW2080_CTRL_CLK_DOMAIN_SYS2CLK   |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_LTC2CLK_CAPABILITY_INIT != 0)
                    LW2080_CTRL_CLK_DOMAIN_LTC2CLK   |
#endif
#if (LW_PTRIM_SYS_PHYSICAL_CLKS_0_XBAR2CLK_CAPABILITY_INIT != 0)
                    LW2080_CTRL_CLK_DOMAIN_XBAR2CLK  |
#endif
                    LW2080_CTRL_CLK_DOMAIN_GPC2CLK   |
                    LW2080_CTRL_CLK_DOMAIN_MCLK;

    return clkDomMask;
}

/*!
 * @brief Enable/disable the given clock counter logic from continuously updating
 *        counter value in registers
 *
 * @details CONTINUOUS_UPDATE field for FR clock counters can be used to
 * enable/disable the counter logic from updating the registers in order
 * to save power. It can be disabled for clocks gated during low power modes
 * before disabling HW access to counters and can be re-enabled before the
 * enabing HW access to clock counters.
 * 
 * @note For GA10x and later, explicit SLCG was added into FR counter HW that
 * utilizes the CONTINUOUS_UPDATE field.
 * 
 * @param[in] pCntr     Pointer to CLK_CNTR object.
 * @param[in] bEnable   Flag to indicate if the counter logic needs to be
 *                      enabled/disabled
 *
 * @return FLCN_OK
 *      Counter logic successfully enabled/disabled
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Given clock counter object is NULL
 */
FLCN_STATUS
clkCntrEnableContinuousUpdate_GP100
(
    CLK_CNTR   *pCntr,
    LwBool      bEnable
)
{
    LwU32   data;

    if (pCntr == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    data = REG_RD32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG));

    if (bEnable)
    {
        // Start updating counter value in registers.
        data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                           _CONTINOUS_UPDATE, _ENABLED, data);
    }
    else
    {
        // Stop updating counter value in registers
        data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                           _CONTINOUS_UPDATE, _DISABLED, data);
    }

    REG_WR32_STALL(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Enable the given clock counter such that it starts counting its
 * configured source (pCntr->cntrSource).
 * 
 * @param[in]   pCntr  Pointer to the counter that needs to be enabled
 *
 * @return FLCN_OK                   If counter is enabled successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT If pCntr is NULL
 */
static inline FLCN_STATUS
s_clkCntrEnable_GP100
(
    CLK_CNTR   *pCntr
)
{
    LwU32   data;

    if (pCntr == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read config register
    data = REG_RD32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG));

    // Reset clock counter.
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _COUNT_UPDATE_CYCLES, _INIT, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _CONTINOUS_UPDATE, _ENABLED, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _START_COUNT, _DISABLED, data);
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _RESET, _ASSERTED, data);
    data = FLD_SET_DRF_NUM(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                            _SOURCE, pCntr->cntrSource, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    //
    // Based on the clock counter design, it takes 16 clock cycles of the
    // "counted clock" for the counter to completely reset. Considering
    // 27MHz as the slowest clock during boot time, delay of 16/27us (~1us)
    // should be sufficient. See Bug 1953217.
    //
    OS_PTIMER_SPIN_WAIT_US(1);

    // Un-reset clock counter
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _RESET, _DEASSERTED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    //
    // Enable clock counter.
    // Note : Need to write un-reset and enable signal in different
    // register writes as the source (register block) and destination
    // (FR counter) are on the same clock and far away from each other,
    // so the signals can not reach in the same clock cycle hence some
    // delay is required between signals.
    //
    data = FLD_SET_DRF(_PTRIM, _GPC_BCAST_FR_CLK_CNTR_NCGPCCLK_CFG,
                        _START_COUNT, _ENABLED, data);
    REG_WR32(FECS, CLK_CNTR_REG_GET_GP100(pCntr, CFG), data);

    return FLCN_OK;
}
