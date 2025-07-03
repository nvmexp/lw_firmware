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
 * @file   pmu_clkavfsga10x.c
 * @brief  PMU Hal Functions for GA10x for Clocks AVFS
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
#define GPC_BCAST_GPCNAFLL_DVCO_SLOW        (0U)
#define GPC_BCAST_GPCNAFLL_COEFF            (1U)
#define GPC_BCAST_GPCNAFLL_CFG1             (2U)
#define GPC_BCAST_GPCNAFLL_CFG2             (3U)
#define GPC_BCAST_GPCNAFLL_CFG3             (4U)
#define GPC_BCAST_GPCNAFLL_CTRL1            (5U)
#define GPC_BCAST_GPCNAFLL_CTRL2            (6U)
#define GPC_BCAST_GPCLUT_SW_FREQ_REQ        (7U)
#define GPC_BCAST_CLK_SRC_CONTROL           (8U)
#define GPC_BCAST_SEL_VCO                   (9U)
#define GPC_BCAST_GPCCLK_OUT               (10U)
#define GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL (11U)
#define GPC_BCAST_AVFS_CPM_CFG             (12U)
#define GPC_BCAST_AVFS_CPM_CLK_CFG         (13U)
#define GPC_BCAST_AVFS_CPM_THRESHOLD_LOW   (14U)
#define GPC_BCAST_AVFS_CPM_THRESHOLD_HIGH  (15U)
#define GPC_BCAST_MAX                      (16U)

// Hopper+ GPC2CLK is renamed to GPCCLK
#ifndef LW_PTRIM_GPC_BCAST_SEL_VCO_GPC2CLK_OUT
#define LW_PTRIM_GPC_BCAST_SEL_VCO_GPC2CLK_OUT        LW_PTRIM_GPC_BCAST_SEL_VCO_GPCCLK_OUT                                                          
#define LW_PTRIM_GPC_BCAST_SEL_VCO_GPC2CLK_OUT_BYPASS LW_PTRIM_GPC_BCAST_SEL_VCO_GPCCLK_OUT_BYPASS                                                   
#define LW_PTRIM_GPC_BCAST_SEL_VCO_GPC2CLK_OUT_VCO    LW_PTRIM_GPC_BCAST_SEL_VCO_GPCCLK_OUT_VCO                                                      
#endif

/* ------------------------- Static Variables ------------------------------- */
/*
 * Array holding GPCs NAFLL init settings indexed by @ref GPC_BCAST_<XYZ>
 */
static LwU32 GpcsNafllInitSettings[GPC_BCAST_MAX]
    GCC_ATTRIB_SECTION("dmem_perfChangeSeqLpwr", "GpcsNafllInitSettings");

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Helper interface to cache the GPCs NAFLL settings.
 *
 * @return FLCN_OK  GPCs NAFLL settings cached successfully
 * @return other    Error during cache.
 */
FLCN_STATUS
clkNafllsCacheGpcsNafllInitSettings_GA10X(void)
{
    LwU32   reg32;

    // 1. Cache VCO settings.
    reg32  = LW_PTRIM_GPC_BCAST_SEL_VCO;
    GpcsNafllInitSettings[GPC_BCAST_SEL_VCO] = REG_RD32(FECS, reg32);

    // 2. Cache source of frequency.
#if (PMU_PROFILE_TU10X)
    reg32  = LW_PTRIM_GPC_BCAST_GPCPLL_DYN;
    GpcsNafllInitSettings[GPC_BCAST_CLK_SRC_CONTROL] = REG_RD32(FECS, reg32);
#else
    reg32  = LW_PTRIM_GPC_BCAST_CLK_SRC_CONTROL;
    GpcsNafllInitSettings[GPC_BCAST_CLK_SRC_CONTROL] = REG_RD32(FECS, reg32);

    //
    // Skip the NAFLL init sequence cache on GA10x+ if GPC was NOT initialized
    // on NAFLL path by VBIOS devinit sequence. This check is NOT required for
    // TURING as it is always running on NAFLL path for testing purposes.
    //
    if ((!FLD_TEST_DRF(_PTRIM, _GPC_BCAST_SEL_VCO,
         _GPC2CLK_OUT, _VCO, GpcsNafllInitSettings[GPC_BCAST_SEL_VCO])) ||
        (!FLD_TEST_DRF(_PTRIM, _GPC_BCAST_CLK_SRC_CONTROL,
         _GPCCLK_MODE, _GPCNAFLL, GpcsNafllInitSettings[GPC_BCAST_CLK_SRC_CONTROL])))
    {
        return FLCN_OK;
    }
#endif

    // 3. Cache the _GPCCLK_OUT_SDIV14
    reg32 = LW_PTRIM_GPC_BCAST_GPCCLK_OUT;
    GpcsNafllInitSettings[GPC_BCAST_GPCCLK_OUT] = REG_RD32(FECS, reg32);

    // 4. Cache DVCO slowdown ratio and FLL lock timeout.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_DVCO_SLOW;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_DVCO_SLOW] = REG_RD32(FECS, reg32);

    // 5. Cache the coefficients of the NAFLL.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_COEFF;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_COEFF] = REG_RD32(FECS, reg32);

    //
    // 6. Cache initial value for FLL aclwmulator and the FLL error signal observation
    //    length for FLL lock detect.
    //
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG2;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG2] = REG_RD32(FECS, reg32);

    // 7. Cache override for tuning bits for the internal DVCO.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG3;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG3] = REG_RD32(FECS, reg32);

    // 8. Cache the spare FLL control register and setup bits.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL1;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CTRL1] = REG_RD32(FECS, reg32);

    // 9. Cache FLL override bits and debug data selection.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL2;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CTRL2] = REG_RD32(FECS, reg32);

    // 10. Cache boot frequency and regime.
    reg32  = LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ;
    GpcsNafllInitSettings[GPC_BCAST_GPCLUT_SW_FREQ_REQ] = REG_RD32(FECS, reg32);

    // 11. Cache NAFLL Power On and IDDQ settings.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1;
    GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG1] = REG_RD32(FECS, reg32);

    // 12. Cache the Slowdown/HMMA devinit settings
    reg32 = LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL;
    GpcsNafllInitSettings[GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL] = REG_RD32(FECS, reg32);

#if (!(PMU_PROFILE_TU10X))
    // 13. Cache the CPM devinit settings
    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG;
    GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_CFG] = REG_RD32(FECS, reg32);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_CLK_CFG;
    GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_CLK_CFG] = REG_RD32(FECS, reg32);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_THRESHOLD_LOW;
    GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_THRESHOLD_LOW] = REG_RD32(FECS, reg32);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_THRESHOLD_HIGH;
    GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_THRESHOLD_HIGH] = REG_RD32(FECS, reg32);
#endif

    return FLCN_OK;
}

/*!
 * @brief Helper interface to program the GPCs NAFLL settings.
 *
 * @return FLCN_OK  GPCs NAFLL settings programmed successfully
 * @return other    Error during programming.
 */
FLCN_STATUS
clkNafllsProgramGpcsNafllInitSettings_GA10X(void)
{
    LwU32   reg32;
    LwU32   dataCfg1;
    LwU32   dataCfg1Old;
    LwU8    vfGain;

#if (!(PMU_PROFILE_TU10X))

    //
    // Skip the NAFLL init sequenece if GPC was NOT initialized
    // on NAFLL path by VBIOS devinit sequence.
    //
    if ((!FLD_TEST_DRF(_PTRIM, _GPC_BCAST_SEL_VCO,
         _GPC2CLK_OUT, _VCO, GpcsNafllInitSettings[GPC_BCAST_SEL_VCO])) ||
        (!FLD_TEST_DRF(_PTRIM, _GPC_BCAST_CLK_SRC_CONTROL,
         _GPCCLK_MODE, _GPCNAFLL, GpcsNafllInitSettings[GPC_BCAST_CLK_SRC_CONTROL])))
    {
        //
        // On RTL, we are using reduced VBIOS which runs GPC on
        // PLL source. Only one specific test VBIOS of RTL contains
        // the devinit sequence that init GPC on NAFLL source. In
        // order to not fail loudly on PLL based VBIOS, returning
        // silently here without hitting PMU BREAKPOINT.
        //
        return FLCN_OK;
    }
#endif

    //
    // In TURING, we are using TPC-PG sequence of reset instead of new GPC-RG reset
    // sequence. The TPC-PG sequence only reset certain parts of GPC chiplet and
    // do not reset the GPC NAFLL registers. So gpcclk (and the entire priv bus) is
    // still on the NAFLL clock when we start following sequence and our step of
    // setting the NAFLL.ENABLE = 0 kills gpcclk resulting into GPU HANG. So for
    // temp WAR, We are switching the GPC to BYPASS path as it should be done in
    // GPC-RG GA10x+ sequence.
    //
#if (PMU_PROFILE_TU10X)
    reg32  = LW_PTRIM_GPC_BCAST_SEL_VCO;
    REG_WR32(FECS, reg32, LW_PTRIM_GPC_BCAST_SEL_VCO_GPC2CLK_OUT_BYPASS);
#endif


    // 1. Restore the _GPCCLK_OUT_SDIV14
    reg32 = LW_PTRIM_GPC_BCAST_GPCCLK_OUT;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCCLK_OUT]);

    // 2. Program DVCO slowdown ratio and FLL lock timeout.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_DVCO_SLOW;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_DVCO_SLOW]);

    // 3. Program the VFGAIN value in the CFG1.
    reg32    = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1;
    dataCfg1 = 0;

    vfGain   = DRF_VAL(_PTRIM_GPC_BCAST, _GPCNAFLL_CFG1, _VFGAIN,
                  GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG1]);
    dataCfg1 = FLD_SET_DRF_NUM(_PTRIM_GPC_BCAST, _GPCNAFLL_CFG1,
                             _VFGAIN, vfGain, dataCfg1);

    REG_WR32(FECS, reg32, dataCfg1);

    // 4. Program the coefficients of the NAFLL.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_COEFF;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_COEFF]);

    //
    // 5. Program  initial value for FLL aclwmulator and the FLL error signal observation
    //    length for FLL lock detect.
    //
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG2;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG2]);

    // 6. Program override for tuning bits for the internal DVCO.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG3;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CFG3]);

    // 7. Program the spare FLL control register and setup bits.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL1;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CTRL1]);

    // 8. Program FLL override bits and debug data selection.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL2;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCNAFLL_CTRL2]);

    // 9. Program boot frequency and regime.
    reg32  = LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_GPCLUT_SW_FREQ_REQ]);

    // 10. program IDDQ settings and Power On NAFLL.
    reg32  = LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1;

    //
    // 10.1 Update IDDQ settings.
    // 'dataCfg1' is already set to VfGain step 3 above
    //
    dataCfg1Old = dataCfg1;
    dataCfg1 = FLD_SET_DRF(_PTRIM_GPC_BCAST, _GPCNAFLL_CFG1,
                    _IDDQ, _POWER_ON, dataCfg1);

    // Realized that _IDDQ_POWER_ON is the _INIT value for CFG1
    if (dataCfg1 != dataCfg1Old)
    {
        REG_WR32(FECS, reg32, dataCfg1);

        // 10.2 wait for 1us between IDDQ = 0 and EN = 1.
        OS_PTIMER_SPIN_WAIT_US(1);
    }

    // 10.3 Enable NAFLL.
    dataCfg1 = FLD_SET_DRF(_PTRIM_GPC_BCAST, _GPCNAFLL_CFG1,
                    _ENABLE, _YES, dataCfg1);

    REG_WR32(FECS, reg32, dataCfg1);

    // Poll for frequency lock or DVCO min reached signal.
    while (LW_TRUE)
    {
        dataCfg1 = REG_RD32(FECS, reg32);
        if (FLD_TEST_DRF(_PTRIM_GPC_BCAST, _GPCNAFLL_CFG1, _LOCK_OR_MIN_REACHED,
                _TRUE, dataCfg1))
        {
            break;
        }
    }

    // 11. Program source of frequency.
#if (PMU_PROFILE_TU10X)
    reg32  = LW_PTRIM_GPC_BCAST_GPCPLL_DYN;
#else
    reg32  = LW_PTRIM_GPC_BCAST_CLK_SRC_CONTROL;
#endif

    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_CLK_SRC_CONTROL]);

    reg32  = LW_PTRIM_GPC_BCAST_SEL_VCO;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_SEL_VCO]);

    return FLCN_OK;
}

/*!
 * @brief Helper interface to program the GPCs NAFLL extended settings.
 *
 * These would include the HMMA & CPM settings
 *
 * @return FLCN_OK  GPCs NAFLL settings programmed successfully
 * @return other    Error during programming.
 */
FLCN_STATUS
clkNafllsProgramGpcsNafllExtendedSettings_GA10X(void)
{
    LwU32   reg32;

    // 12. Restore the Slowdown/HMMA devinit settings
    reg32 = LW_PTRIM_GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_AVFS_QUICK_SLOWDOWN_CTRL]);

#if (!(PMU_PROFILE_TU10X))
    // 13. Restore the CPM devinit settings
    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_CFG;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_CFG]);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_CLK_CFG;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_CLK_CFG]);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_THRESHOLD_LOW;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_THRESHOLD_LOW]);

    reg32 = LW_PTRIM_GPC_BCAST_AVFS_CPM_THRESHOLD_HIGH;
    REG_WR32(FECS, reg32, GpcsNafllInitSettings[GPC_BCAST_AVFS_CPM_THRESHOLD_HIGH]);
#endif

    return FLCN_OK;
}

/*!
 * @brief Helper interface to get the cached NDIV value.
 *
 * @return Ndiv value cached in the GpcsNafllInitSettings
 */
LwU32
clkNafllsGetCachedNdivValue_GA10X(void)
{
    return (DRF_VAL(_PTRIM, _GPC_BCAST_GPCLUT_SW_FREQ_REQ, _NDIV, 
                    GpcsNafllInitSettings[GPC_BCAST_GPCLUT_SW_FREQ_REQ]));
}

/*!
 * @brief Get the RAM READ EN bit for the LUT
 *
 * @param[in]   pNafllDev  Pointer to the NAFLL device
 *
 * @return LW_TRUE  LUT_RAM_READ_EN is set
 * @return LW_FALSE LUT_RAM_READ_EN is not set or NAFLL device is NULL
 * 
 * @note Update the interface for GB10X MemSys (bug 200684738) to accept the
 *          LUT type (NDIV, NDIV OFFSET and CPM MAX NDIV). Separate RAM_READ_EN
 *          bits are available for each of these LUT types from Ada.
 */
LwBool
clkNafllLutGetRamReadEn_GA10X
(
    CLK_NAFLL_DEVICE  *pNafllDev
)
{
    if (pNafllDev == NULL)
    {
        return LW_FALSE;
    }

    return FLD_TEST_DRF(_PTRIM, _SYS_NAFLL_LTCLUT_CFG, _RAM_READ_EN, _YES,
               REG_RD32(FECS, CLK_NAFLL_REG_GET(pNafllDev, LUT_CFG)));
}

/*!
 * @brief Get the _NDIV value from LUT_STATUS (LUT_DEBUG2) register
 *
 * @return Ndiv value programmed lwrrently
 */
LwU32
clkNafllGetGpcProgrammedNdiv_GA10X(void)
{
    
    return 
#if (!(PMU_PROFILE_TU10X))
            (DRF_VAL(_PTRIM, _GPC_BCAST_GPCLUT_STATUS, _NDIV, REG_RD32(FECS, LW_PTRIM_GPC_BCAST_GPCLUT_STATUS)));
#else
            (DRF_VAL(_PTRIM, _GPC_BCAST_GPCLUT_DEBUG2, _NDIV, REG_RD32(FECS, LW_PTRIM_GPC_BCAST_GPCLUT_DEBUG2)));
#endif
}

/* ------------------------- Private Functions ------------------------------ */
