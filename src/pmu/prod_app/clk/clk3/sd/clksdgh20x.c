/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Antone Vogt-Varvak
 *
 * @details     The main purpose of this file is to declare and define the
 *              schematic dag for the GH20x family, which
 *              - Use SWDIVs,
 *              - Have display, and
 *              - Use DDR (Double Data Rate) memory.
 *
 *              More generally, everything family-specific in Clocks 3.x goes
 *              in this file.  Nothing else should contain chip-specific data
 *              or logic since these data constructors contain flags to control
 *              specific behaviour.
 *
 *              In general, the schematic dag is declared and defined in
 *              "clk3/sd/clksd[chip].c", where [chip] is the first chip of
 *              the series of chips using that schematic.
 *
 * @warning     After modifying this file, please check it by running:
 *              //sw/dev/gpu_drv/chips_a/pmu_sw/prod_app/clk/clk3/clksdcheck.c
 *              See source code comments for instructions and information.
 */

/***************************************************************
 * Includes
 ***************************************************************/

// PMU
#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif                      // CLK_SD_CHECK

// Basic clocks stuff
#include "clk3/clksignal.h"
#include "clk3/sd/clksd.h"
#include "pmu_objclk.h"

// The manual
#include "dev_fbpa.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "clk3/generic_dev_trim.h"
#include "hwproject.h"

// Clock Frequency Domains
#include "clk3/dom/clkfreqdomain_bif.h"
#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"
#include "clk3/dom/clkfreqdomain_mclk.h"
#include "clk3/dom/clkfreqdomain_swdivhpll.h"

// Frequency Source Objects
#include "clk3/fs/clkhpll.h"
#include "clk3/fs/clkldivv2.h"
#include "clk3/fs/clkmux.h"
#include "clk3/fs/clkreadonly.h"
#include "clk3/fs/clkrovco.h"
#include "clk3/fs/clkrosdm.h"
#include "clk3/fs/clkswdiv4.h"  // TODO: Remove once MCLK HW changes have been checked in per bug 2991907.
#include "clk3/fs/clkxtal.h"

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_objfb.h"
#endif                      // CLK_SD_CHECK

/***************************************************************
 * Feature Check
 ***************************************************************/

#if !PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_DDR_SUPPORTED)
#error This schematic is specific to DDR (Double Data Rate) memory.
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS)
#error This schematic is specific to chips with display.
#endif


/***************************************************************
 * Schematic Structure
 ***************************************************************/


/*!
 * @brief       Schematic structure for this litter
 * @see         clkSchematicDag
 * @see         clkFreqDomainArray
 * @see         clkConstruct_SchematicDag
 * @private
 *
 * @details     Why isn't this in the header file?  Because this structure is
 *              entirely private.  There is only one instance of this struct,
 *              'clkSchematicDag'.  Nothing (other than initialization in this
 *              source file) references any of the symbols defined herein.
 *
 *              All other logic accesses these fields via 'clkFreqDomainArray'
 *              and/or similar pointers.
 *
 *              Why is this struct called 'ClkSchematicDag' instead of
 *              'ClkSchematicDag_[chip]'?  Because PMU builds are per-litter
 *              and there is one of these structures per litter.
 */
typedef struct
{
    //
    // Display-Specific Domains
    //
    ClkSwDivHPllFreqDomain              dispclk;
    ClkSwDivHPllFreqDomain              hubclk;

    //
    // PCIE Gen (BIF)
    //
    ClkBifFreqDomain                    pciegenclk;

    //
    // NAFLL Domains
    //
    // Bug 2242317 describes the revision to the schematic for these
    // clock domains.  In particular, per meeting on 18 July 2018,
    // comment #20 describes the specifics.
    //
    // Bug 1880093:  There is a hardware bug in Volta which makes the
    // bypass mux on AVFS domains useless.  It's not a problem because
    // we don't need it in the POR.  However, since it is potentially
    // useful in WARs, it has been fixed in Turing (and after).  It may
    // be removed at some post-Turing family.
    //
    ClkMultiNafllFreqDomain             gpcclk;
    ClkSingleNafllFreqDomain            sysclk;
    ClkSingleNafllFreqDomain            xbarclk;
    ClkSingleNafllFreqDomain            lwdclk;

    // MCLK on GH20x is for GDDR/SDDR (not HBM).
    struct
    {
        ClkMclkFreqDomain               domain;     // mclk.domain
        ClkRoVco                        drampll;    // mclk.drampll
        struct
        {
            ClkPDiv                     pdiv;       // mclk.refmpll.pdiv
            ClkRoSdm                    vco;        // mclk.refmpll.vco
        } refmpll;

        struct
        {
            ClkMux                      mux;        // mclk.swdiv.mux
            ClkFreqSrc                 *input[4];   // mclk.swdiv.input
            ClkLdivV2                   div2;       // mclk.swdiv.div2
            ClkLdivV2                   div3;       // mclk.swdiv.div3
        } swdiv;
    } mclk;

    //
    // Read-Only Domains
    //
    struct
    {
        ClkFixedFreqDomain              super;
        struct
        {
            ClkLdivV2                   div2;
            ClkLdivV2                   div3;
            ClkMux                      mux;
            ClkFreqSrc * const          input[4];
        } swdiv;
    } utilsclk;

    struct
    {
        ClkFixedFreqDomain              super;
        struct
        {
            ClkLdivV2                   div2;
            ClkLdivV2                   div3;
            ClkMux                      mux;
            ClkFreqSrc * const          input[4];
        } swdiv;
    } pwrclk;

    //
    // Frequency Source Objects Shared Among Domains
    //
    struct
    {
        ClkReadOnly                     readonly;   // defrost.readonly
        ClkRoVco                        vco;        // defrost.pll
    } defrost;                                      // Typically 1620MHz

    struct
    {
        ClkReadOnly                     readonly;   // sppll0.readonly
        ClkRoVco                        vco;        // sppll0.pll
    } sppll0;                                       // Typically 2700MHz

    struct
    {
        ClkReadOnly                     readonly;   // sppll1.readonly
        ClkRoVco                        vco;        // sppll1.pll
    } sppll1;                                       // Typically 2700MHz

    ClkXtal                             xtal;       // Typically 27MHz

} ClkSchematicDag;


const ClkFieldValue clkMuxValueMap_pwrclk[4]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_pwrclk") =
{
    // 0: xtal 27MHz w/o divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_0)
    },

    // 1: xtal 27MHz w/o divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_1)
    },

    // 2: defrost with divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _DEFROST_CLK)
    },

    // 3: XTAL with divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_PLL_REFCLK)
    },
};

const ClkFieldValue clkMuxValueMap_utilsclk[4]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_utilsclk") =
{
    // 0: xtal 27MHz w/o divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_UNIV_SEC_CLK_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_PLL_REFCLK_0)
    },

    // 1: xtal 27MHz w/o divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_UNIV_SEC_CLK_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_PLL_REFCLK_1)
    },

    // 2: defrost with divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_UNIV_SEC_CLK_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _DEFROST_CLK)
    },

    // 3: XTAL with divider
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_UNIV_SEC_CLK_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_PLL_REFCLK_2)
    },
};


/***************************************************************
 * Schematic DAG
 ***************************************************************/

/*!
 * @brief   The schematic itself
 *
 * @details The type of each object this structure is a struct whose name is
 *          'Clk[leafname]' where [leafname] is the base name of a leaf
 *          class such as 'Pll', 'Mux', 'ClkMclkFreqDomain' etc.
 *
 *          For the most part, these data do not change after initialization.
 *          Much of these data are initialized at compile-time.
 */
ClkSchematicDag clkSchematicDag
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkSchematicDag") =
{

    /* ------------------------ DISPCLK ------------------------------------- */
    /*
     * The block diagram of schema defined for this domain:
     *                                                    ______
     *                                                   |      |
     *        xtal ---+----------------------------------|0     |
     *                |     _________       ______       |      |
     *                |    |         |     |      |      |      |
     *                +----| DISPPLL |-----|0     |      |      |
     *                |    |_________|     |  SW2 |------|1     |______
     *                +--------------------|1     |      |      |      |
     *                 ______________      |______|      |  SW4 | LDIV |---> OUTPUT
     *                |              |                   |      |______|
     *      sppll0 ---| SPPLL0_PDIVA |-------------------|2     |
     *                |______________|                   |      |
     *                              ______________       |      | SWDIV
     *                             |              |      |      |
     *      sppll1 ----------------| SPPLL1_PDIVA |------|3     |
     *                             |______________|      |______|
     *
     * Regarding GA10x and altPathMaxKHz:  Bug 2557340/48 specifies 900MHz for
     * this value. However, for bug 2789595, NDIV_FRAC is disabled, which means
     * that we can achieve only multiples of 27MHz via the PLL.  As such, this
     * parameter is set to 918MHz=34*27MHz since it is the lowest achievable
     * frequency above 900MHz.  1/2% is subtracted to allow for margin.
     *
     * TODO:  Revisited the POR for Hopper and revise this value accordingly.
     */
    .dispclk.super.pVirtual                     = &clkVirtual_SwDivHPllFreqDomain,
    .dispclk.super.pRoot                        = &clkSchematicDag.dispclk.swdiv.div.super.super,
    .dispclk.altPathMaxKHz                      = 913410,       // bug 2789595: 918MHz=34*27MHz less 1/2%

    CLK_STATIC_INIT__SWDIV4(dispclk.swdiv, LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER)
    .dispclk.swdiv.input[CLK_INPUT_SWDIV_XTAL__SWDIVHPLLFREQDOMAIN]        = &clkSchematicDag.xtal.super,
    .dispclk.swdiv.input[CLK_INPUT_SWDIV_PLL__SWDIVHPLLFREQDOMAIN]         = &clkSchematicDag.dispclk.pllxtal.mux.super,
    .dispclk.swdiv.input[CLK_INPUT_SWDIV_SPPLL0_PDIV__SWDIVHPLLFREQDOMAIN] = &clkSchematicDag.dispclk.pdiv0.super.super,
    .dispclk.swdiv.input[CLK_INPUT_SWDIV_SPPLL1_PDIV__SWDIVHPLLFREQDOMAIN] = &clkSchematicDag.dispclk.pdiv1.super.super,
    .dispclk.swdiv.mux.muxDoneField             = CLK_DRF__FIELDVALUE( _PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _SWITCH_DIVIDER_DONE, _YES),

    .dispclk.pllxtal.mux.super.pVirtual         = &clkVirtual_Mux,
    .dispclk.pllxtal.mux.muxRegAddr             = LW_PVTRIM_SYS_DISPPLL_CFG,
    .dispclk.pllxtal.mux.muxValueMap            = clkMuxValueMap_pllxtal_SwDivHPllFreqDomain,
    .dispclk.pllxtal.mux.input                  = clkSchematicDag.dispclk.pllxtal.input,
    .dispclk.pllxtal.mux.count                  = CLK_INPUT_PLLXTAL_COUNT__SWDIVHPLLFREQDOMAIN,
    .dispclk.pllxtal.mux.glitchy                = LW_FALSE,
    .dispclk.pllxtal.input[CLK_INPUT_PLLXTAL_PLL__SWDIVHPLLFREQDOMAIN]
                                                = &clkSchematicDag.dispclk.pll.super.super,
    .dispclk.pllxtal.input[CLK_INPUT_PLLXTAL_XTAL__SWDIVHPLLFREQDOMAIN]
                                                = &clkSchematicDag.xtal.super,

    .dispclk.pll.super.super.pVirtual           = &clkVirtual_HPll,
    .dispclk.pll.super.pInput                   = &clkSchematicDag.xtal.super,
    .dispclk.pll.pdivTarget                     = 1,        // Bug 2557340/48
    .dispclk.pll.cfgRegAddr                     = LW_PVTRIM_SYS_DISPPLL_CFG,
    .dispclk.pll.coeffRegAddr                   = LW_PVTRIM_SYS_DISPPLL_COEFF,
    .dispclk.pll.cfg4RegAddr                    = LW_PVTRIM_SYS_DISPPLL_CFG4,
    .dispclk.pll.cfg2RegAddr                    = LW_PVTRIM_SYS_DISPPLL_CFG2,
    .dispclk.pll.source                         = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,

    .dispclk.pdiv0.super.super.pVirtual         = &clkVirtual_PDiv,
    .dispclk.pdiv0.super.pInput                 = &clkSchematicDag.sppll0.readonly.super.super,
    .dispclk.pdiv0.regAddr                      = LW_PVTRIM_SYS_SPPLL0_COEFF2,
    .dispclk.pdiv0.base                         = DRF_BASE(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA),
    .dispclk.pdiv0.size                         = DRF_SIZE(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA),

    .dispclk.pdiv1.super.super.pVirtual         = &clkVirtual_PDiv,
    .dispclk.pdiv1.super.pInput                 = &clkSchematicDag.sppll1.readonly.super.super,
    .dispclk.pdiv1.regAddr                      = LW_PVTRIM_SYS_SPPLL1_COEFF2,
    .dispclk.pdiv1.base                         = DRF_BASE(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA),
    .dispclk.pdiv1.size                         = DRF_SIZE(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA),


    /* ------------------------ HUBCLK ------------------------------------- */
    /*
     * The block diagram of schema defined for this domain:
     *                                                       ______
     *                                                      |      |
     *        xtal ---+-------------------------------------|0     |
     *                |     ____________       ______       |      |
     *                |    |            |     |      |      |      |
     *                +----| DISPHUBPLL |-----|0     |      |      |
     *                |    |____________|     |  SW2 |------|1     |______
     *                +-----------------------|1     |      |      |      |
     *                    ______________      |______|      |  SW4 | LDIV |---> OUTPUT
     *                   |              |                   |      |______|
     *      sppll0 ------| SPPLL0_PDIVB |-------------------|2     |
     *                   |______________|                   |      |
     *                                 ______________       |      | SWDIV
     *                                |              |      |      |
     *      sppll1 -------------------| SPPLL1_PDIVB |------|3     |
     *                                |______________|      |______|
     *
     * Regarding altPathMaxKHz:  Bug 2557340/48 specifies 675MHz for this value.
     * It was not impacted by GA10x bug 2789595 (NDIV_FRAC disablement) since it
     * is a multiple of 27MHz via the PLL.
     */

    .hubclk.super.pVirtual                          = &clkVirtual_SwDivHPllFreqDomain,
    .hubclk.super.pRoot                             = &clkSchematicDag.hubclk.swdiv.div.super.super,
    .hubclk.altPathMaxKHz                           = 675000,   // Per bug 2557340/48

    CLK_STATIC_INIT__SWDIV4(hubclk.swdiv, LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER)
    .hubclk.swdiv.input[CLK_INPUT_SWDIV_XTAL__SWDIVHPLLFREQDOMAIN]        = &clkSchematicDag.xtal.super,
    .hubclk.swdiv.input[CLK_INPUT_SWDIV_PLL__SWDIVHPLLFREQDOMAIN]         = &clkSchematicDag.hubclk.pllxtal.mux.super,
    .hubclk.swdiv.input[CLK_INPUT_SWDIV_SPPLL0_PDIV__SWDIVHPLLFREQDOMAIN] = &clkSchematicDag.hubclk.pdiv0.super.super,
    .hubclk.swdiv.input[CLK_INPUT_SWDIV_SPPLL1_PDIV__SWDIVHPLLFREQDOMAIN] = &clkSchematicDag.hubclk.pdiv1.super.super,
    .hubclk.swdiv.mux.muxDoneField                  = CLK_DRF__FIELDVALUE( _PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _SWITCH_DIVIDER_DONE, _YES),

    .hubclk.pllxtal.mux.super.pVirtual              = &clkVirtual_Mux,
    .hubclk.pllxtal.mux.muxRegAddr                  = LW_PVTRIM_SYS_DISPHUBPLL_CFG,
    .hubclk.pllxtal.mux.muxValueMap                 = clkMuxValueMap_pllxtal_SwDivHPllFreqDomain,
    .hubclk.pllxtal.mux.input                       = clkSchematicDag.hubclk.pllxtal.input,
    .hubclk.pllxtal.mux.count                       = CLK_INPUT_PLLXTAL_COUNT__SWDIVHPLLFREQDOMAIN,
    .hubclk.pllxtal.mux.glitchy                     = LW_FALSE,
    .hubclk.pllxtal.input[CLK_INPUT_PLLXTAL_PLL__SWDIVHPLLFREQDOMAIN]
                                                    = &clkSchematicDag.hubclk.pll.super.super,
    .hubclk.pllxtal.input[CLK_INPUT_PLLXTAL_XTAL__SWDIVHPLLFREQDOMAIN]
                                                    = &clkSchematicDag.xtal.super,

    .hubclk.pll.super.super.pVirtual                = &clkVirtual_HPll,
    .hubclk.pll.super.pInput                        = &clkSchematicDag.xtal.super,
    .hubclk.pll.pdivTarget                          = 2,        // Bug 2557340/48
    .hubclk.pll.cfgRegAddr                          = LW_PVTRIM_SYS_DISPHUBPLL_CFG,
    .hubclk.pll.coeffRegAddr                        = LW_PVTRIM_SYS_DISPHUBPLL_COEFF,
    .hubclk.pll.cfg2RegAddr                         = LW_PVTRIM_SYS_DISPHUBPLL_CFG2,
    .hubclk.pll.cfg4RegAddr                         = LW_PVTRIM_SYS_DISPHUBPLL_CFG4,
    .hubclk.pll.source                              = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,

    .hubclk.pdiv0.super.super.pVirtual              = &clkVirtual_PDiv,
    .hubclk.pdiv0.super.pInput                      = &clkSchematicDag.sppll0.readonly.super.super,
    .hubclk.pdiv0.regAddr                           = LW_PVTRIM_SYS_SPPLL0_COEFF2,
    .hubclk.pdiv0.base                              = DRF_BASE(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB),
    .hubclk.pdiv0.size                              = DRF_SIZE(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB),

    .hubclk.pdiv1.super.super.pVirtual              = &clkVirtual_PDiv,
    .hubclk.pdiv1.super.pInput                      = &clkSchematicDag.sppll1.readonly.super.super,
    .hubclk.pdiv1.regAddr                           = LW_PVTRIM_SYS_SPPLL1_COEFF2,
    .hubclk.pdiv1.base                              = DRF_BASE(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB),
    .hubclk.pdiv1.size                              = DRF_SIZE(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB),


    /* ------------------------ UTILSCLK ----------------------------------- */
    /*
     *                                     |\
     *                                     | \
     *       xtal ----- 0 -----------------|0 |
     *                                     |  |
     * pex refclk ----- 1 -----------------|1 |
     *                          ______     |  |
     *                         |      |    |  |
     *    defrost ----- 2 -----| DIV  |----|2 |----> OUTPUT
     *                         |______|    |  |
     *                                     |  |
     *                          ______     |  |
     *                         |      |    |  |
     *       xtal ----- 3 -----| DIV  |----|3 |
     *                         |______|    | /
     *                                     |/
     *
     */

    .utilsclk.super.pVirtual                     = &clkVirtual_FixedFreqDomain,
    .utilsclk.super.pRoot                        = &clkSchematicDag.utilsclk.swdiv.mux.super,

    .utilsclk.swdiv.div2.super.super.pVirtual    = &clkVirtual_LdivV2,
    .utilsclk.swdiv.div2.super.pInput            = &clkSchematicDag.defrost.readonly.super.super,
    .utilsclk.swdiv.div2.ldivRegAddr             = LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER,


    .utilsclk.swdiv.div3.super.super.pVirtual    = &clkVirtual_LdivV2,
    .utilsclk.swdiv.div3.super.pInput            = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.div3.ldivRegAddr             = LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER,


    .utilsclk.swdiv.mux.super.pVirtual           = &clkVirtual_Mux,
    .utilsclk.swdiv.mux.muxRegAddr               = LW_PTRIM_SYS_UNIV_SEC_CLK_SWITCH_DIVIDER,

    .utilsclk.swdiv.mux.muxValueMap              = clkMuxValueMap_utilsclk,
    .utilsclk.swdiv.mux.input                    = clkSchematicDag.utilsclk.swdiv.input,
    .utilsclk.swdiv.mux.count                    = LW_ARRAY_ELEMENTS(clkSchematicDag.utilsclk.swdiv.input),
    .utilsclk.swdiv.mux.glitchy                  = LW_FALSE,

    .utilsclk.swdiv.input[0x0u]                  = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.input[0x1u]                  = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.input[0x2u]                  = &clkSchematicDag.utilsclk.swdiv.div2.super.super,
    .utilsclk.swdiv.input[0x3u]                  = &clkSchematicDag.utilsclk.swdiv.div3.super.super,


    /* ------------------------ PWRCLK ------------------------------------- */
    /*
     *
     *                                   |\
     *                                   | \
     *     xtal ----- 0 -----------------|0 |
     *                                   |  |
     *     xtal ----- 1 -----------------|1 |
     *                        ______     |  |
     *                       |      |    |  |
     *  defrost ----- 2 -----| DIV  |----|2 |----> OUTPUT
     *                       |______|    |  |
     *                                   |  |
     *                        ______     |  |
     *                       |      |    |  |
     *     xtal ----- 3 -----| DIV  |----|3 |
     *                       |______|    | /
     *                                   |/
     *
     */

    .pwrclk.super.pVirtual                      = &clkVirtual_FixedFreqDomain,
    .pwrclk.super.pRoot                         = &clkSchematicDag.pwrclk.swdiv.mux.super,

    .pwrclk.swdiv.div2.super.super.pVirtual     = &clkVirtual_LdivV2,
    .pwrclk.swdiv.div2.super.pInput             = &clkSchematicDag.defrost.readonly.super.super,
    .pwrclk.swdiv.div2.ldivRegAddr              = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,

    .pwrclk.swdiv.div3.super.super.pVirtual     = &clkVirtual_LdivV2,
    .pwrclk.swdiv.div3.super.pInput             = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.div3.ldivRegAddr              = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,

    .pwrclk.swdiv.mux.super.pVirtual            = &clkVirtual_Mux,
    .pwrclk.swdiv.mux.muxRegAddr                = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,
    .pwrclk.swdiv.mux.muxValueMap               = clkMuxValueMap_pwrclk,
    .pwrclk.swdiv.mux.input                     = clkSchematicDag.pwrclk.swdiv.input,
    .pwrclk.swdiv.mux.count                     = LW_ARRAY_ELEMENTS(clkSchematicDag.pwrclk.swdiv.input),
    .pwrclk.swdiv.mux.glitchy                   = LW_FALSE,

    .pwrclk.swdiv.input[0u]                     = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.input[1u]                     = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.input[2u]                     = &clkSchematicDag.pwrclk.swdiv.div2.super.super,
    .pwrclk.swdiv.input[3u]                     = &clkSchematicDag.pwrclk.swdiv.div3.super.super,

    /* ------------------------ PCIEGENCLK ---------------------------------- */

    .pciegenclk.super.pVirtual                  = &clkVirtual_BifFreqDomain,
    .pciegenclk.super.pRoot                     = &clkSchematicDag.pciegenclk.bif.super,
    CLK_STATIC_INIT__DOMAIN(pciegenclk.super)
    .pciegenclk.bif.maxGenSpeed                 = RM_PMU_BIF_LINK_SPEED_GEN5PCIE,
    .pciegenclk.bif.super.pVirtual              = &clkVirtual_Bif,

    /* ------------------------ GPCCLK ------------------------------------- */

    .gpcclk.super.pVirtual                      = &clkVirtual_MultiNafllFreqDomain,
    .gpcclk.super.pRoot                         = &clkSchematicDag.gpcclk.nafll[0].super,   // Runtime override for each NAFLL source
    CLK_STATIC_INIT__DOMAIN(gpcclk.super)

    .gpcclk.nafll[0].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC0,
    .gpcclk.nafll[0].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[1].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC1,
    .gpcclk.nafll[1].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[2].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC2,
    .gpcclk.nafll[2].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[3].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC3,
    .gpcclk.nafll[3].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[4].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC4,
    .gpcclk.nafll[4].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[5].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC5,
    .gpcclk.nafll[5].super.pVirtual             = &clkVirtual_Nafll,

    .gpcclk.nafll[6].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC6,
    .gpcclk.nafll[6].super.pVirtual             = &clkVirtual_Nafll,

// We must initialize an element in gpcclk.nafll for each GPC.
#if LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 != 7
#error LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 does not match the number of array initializers.
#endif

//
// Note: NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN is the number of elements in
// the above gpcclk.nafll array.  This check makes sure we don't use more
// elements that we have allocated.
//
#if LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 > NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN
#error LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 exceeds the size of ClkMultiNafllFreqDomain::nafll.
#endif

    /* ------------------------ SYSCLK ------------------------------------- */

    .sysclk.super.pVirtual                      = &clkVirtual_SingleNafllFreqDomain,
    .sysclk.super.pRoot                         = &clkSchematicDag.sysclk.nafll.super,
    CLK_STATIC_INIT__DOMAIN(sysclk.super)

    .sysclk.nafll.id                            = LW2080_CTRL_CLK_NAFLL_ID_SYS,
    .sysclk.nafll.super.pVirtual                = &clkVirtual_Nafll,

    /* ------------------------ XBARCLK ------------------------------------- */

    .xbarclk.super.pVirtual                     = &clkVirtual_SingleNafllFreqDomain,
    .xbarclk.super.pRoot                        = &clkSchematicDag.xbarclk.nafll.super,
    CLK_STATIC_INIT__DOMAIN(xbarclk.super)

    .xbarclk.nafll.id                           = LW2080_CTRL_CLK_NAFLL_ID_XBAR,
    .xbarclk.nafll.super.pVirtual               = &clkVirtual_Nafll,

    /* ------------------------ LWDCLK ------------------------------------- */

    .lwdclk.super.pVirtual                      = &clkVirtual_SingleNafllFreqDomain,
    .lwdclk.super.pRoot                         = &clkSchematicDag.lwdclk.nafll.super,
    CLK_STATIC_INIT__DOMAIN(lwdclk.super)

    .lwdclk.nafll.id                            = LW2080_CTRL_CLK_NAFLL_ID_LWD,
    .lwdclk.nafll.super.pVirtual                = &clkVirtual_Nafll,

    /* ------------------------ MCLK --------------------------------------- */
    /*
     * The block diagram of schema defined for GDDR6 is below, based on section
     * 6.2 in Perforce hw/doc/gpu/SOC/Clocks/Documentation/POR/GA10X/GA10x_FB_CLK_IAS.docx
     * The main mux is really the bypass switches in the PLLs, but they are
     * controlled from the same register, so they look like one mux to software.
     * Similarly, there are several DRAMPLLs controlled by the same broadcast register.
     *
     *                ____________         ___________       ______
     *               |     |      |       |           |     |      |
     *      XTAL ----| VCO | PDIV |---+---|  DRAMPLL  |-----|2     |
     *               |_____|______|   |   |___________|     |      |
     *                  REFMPLL       |                     |      |
     *                                +---------------------|1     |
     *                                          ______      |      |---> OUTPUT
     *      XTAL -------------------------+    |      |     | FBIO |
     *                   _______          +----|0     |     | MUX  |
     *                  |       |         |    |      |     |      |
     *   DEFROST -------| LDIV2 |----+    +----|1     |     |      |
     *                  |_______|    |         |  SW4 |-----|0     |
     *                   _______     +---------|2     |     |______|
     *                  |       |              |      |
     *      XTAL -------| LDIV3 |--------------|3     |
     *                  |_______|              |______|
     *                \_________________________________/
     *                               SWDIV
     *
     * Starting with Hopper, SWDIVs (switch dividers) have a finite state machine
     * that orders the programming of the switch and dividers.  One feature of
     * the FSM is that ldiv2 and ldiv3 have the same divider value controlled
     * by the same register and field.  See bug 2991907/55 and in Perforce at
     * hw/doc/gpu/SOC/Clocks/Documentation/POR/GH100/Divider_switch.docx
     */
    .mclk.domain.super.pVirtual                         = &clkVirtual_MclkFreqDomain,
    .mclk.domain.super.pRoot                            = &clkSchematicDag.mclk.domain.mux.super,
    CLK_STATIC_INIT__DOMAIN(mclk.domain.super)

    .mclk.domain.input[CLK_INPUT_BYPASS__MCLK]          = &clkSchematicDag.mclk.swdiv.mux.super,
    .mclk.domain.input[CLK_INPUT_REFMPLL__MCLK]         = &clkSchematicDag.mclk.refmpll.pdiv.super.super,
    .mclk.domain.input[CLK_INPUT_DRAMPLL__MCLK]         = &clkSchematicDag.mclk.drampll.super.super,

    .mclk.domain.mux.super.pVirtual                     = &clkVirtual_Mux,
    .mclk.domain.mux.muxRegAddr                         = LW_PFB_FBPA_FBIO_CTRL_SER_PRIV,
    .mclk.domain.mux.muxValueMap                        = clkMuxValueMap_MclkFreqDomain,
    .mclk.domain.mux.input                              = clkSchematicDag.mclk.domain.input,
    .mclk.domain.mux.count                              = CLK_INPUT_COUNT__MCLK,
    .mclk.domain.mux.glitchy                            = LW_FALSE,

    .mclk.drampll.super.super.pVirtual                  = &clkVirtual_RoVco,
    .mclk.drampll.super.pInput                          = &clkSchematicDag.mclk.refmpll.pdiv.super.super,
    .mclk.drampll.coeffRegAddr                          = LW_PFB_FBPA_DRAMPLL_COEFF,
    .mclk.drampll.source                                = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .mclk.drampll.bDiv2Enable                           = LW_TRUE,

    .mclk.refmpll.pdiv.super.super.pVirtual             = &clkVirtual_PDiv,
    .mclk.refmpll.pdiv.super.pInput                     = &clkSchematicDag.mclk.refmpll.vco.super.super.super,
    .mclk.refmpll.pdiv.regAddr                          = LW_PFB_FBPA_REFMPLL_COEFF,
    .mclk.refmpll.pdiv.base                             = DRF_BASE(LW_PFB_FBPA_REFMPLL_COEFF_PLDIV),
    .mclk.refmpll.pdiv.size                             = DRF_SIZE(LW_PFB_FBPA_REFMPLL_COEFF_PLDIV),

    .mclk.refmpll.vco.super.super.super.pVirtual        = &clkVirtual_RoSdm,
    .mclk.refmpll.vco.super.super.pInput                = &clkSchematicDag.xtal.super,
    .mclk.refmpll.vco.super.coeffRegAddr                = LW_PFB_FBPA_REFMPLL_COEFF,
    .mclk.refmpll.vco.cfg2RegAddr                       = LW_PFB_FBPA_REFMPLL_CFG2,
    .mclk.refmpll.vco.ssd0RegAddr                       = LW_PFB_FBPA_REFMPLL_SSD0,
    .mclk.refmpll.vco.super.source                      = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,

    .mclk.swdiv.mux.super.pVirtual                      = &clkVirtual_Mux,
    .mclk.swdiv.mux.muxRegAddr                          = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,
    .mclk.swdiv.mux.muxValueMap                         = clkMuxValueMap_SwDiv4,
    .mclk.swdiv.mux.input                               = clkSchematicDag.mclk.swdiv.input,
    .mclk.swdiv.mux.count                               = CLK_INPUT_COUNT__SWDIV4,
    .mclk.swdiv.mux.glitchy                             = LW_FALSE,

    .mclk.swdiv.input[0]                                = &clkSchematicDag.xtal.super,
    .mclk.swdiv.input[1]                                = &clkSchematicDag.xtal.super,
    .mclk.swdiv.input[2]                                = &clkSchematicDag.mclk.swdiv.div2.super.super,
    .mclk.swdiv.input[3]                                = &clkSchematicDag.mclk.swdiv.div3.super.super,

    .mclk.swdiv.div2.super.super.pVirtual               = &clkVirtual_LdivV2,
    .mclk.swdiv.div2.super.pInput                       = &clkSchematicDag.defrost.readonly.super.super,
    .mclk.swdiv.div2.ldivRegAddr                        = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,

    .mclk.swdiv.div3.super.super.pVirtual               = &clkVirtual_LdivV2,
    .mclk.swdiv.div3.super.pInput                       = &clkSchematicDag.xtal.super,
    .mclk.swdiv.div3.ldivRegAddr                        = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,


    /* ------------------------ DEFROST ------------------------------------- */

    //
    // The following assumptions apply about 'defrost':
    // - It is not programmed after initialization (read-only);
    // - Fractional NDIV is not being used; and
    // - PLDIV is not being used.
    //
    // See hw/libs/common/analog/lwpu/doc/HPLL16G_SSD_DYN_A1.doc
    //
    // LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL is used because DEFROST_CLK is the
    // only PLL for UTILSCLK and PWRCLK.  For these domains, there is no
    // "bypass" per se.  Moreover, DEFROST_CLK is not used in any other domain.
    //
    .defrost.readonly.super.super.pVirtual      = &clkVirtual_ReadOnly,
    .defrost.readonly.super.pInput              = &clkSchematicDag.defrost.vco.super.super,

    .defrost.vco.super.super.pVirtual           = &clkVirtual_RoVco,
    .defrost.vco.super.pInput                   = &clkSchematicDag.xtal.super,
#ifdef LW_PTRIM_SYS_DEFROST_COEFF
    .defrost.vco.coeffRegAddr                   = LW_PTRIM_SYS_DEFROST_COEFF,
#else       // TODO: Delete once HW changes are checked in
    .defrost.vco.coeffRegAddr                   = LW_PTRIM_SYS_XTAL4X_COEFF,
#endif
    .defrost.vco.source                         = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,


    /* ------------------------ SPPLLs -------------------------------------- */

    //
    // 'sppll0' and 'sppll1' are not programmed after initialization (read-only)
    // except for the PDIVS in _COEFF2, which are managed in 'dispclk' and 'hubclk'.
    //
    // See hw/libs/common/analog/lwpu/doc/PLL27G_SSD_DYN_APESD_A1.doc
    //
    .sppll0.readonly.super.super.pVirtual       = &clkVirtual_ReadOnly,
    .sppll0.readonly.super.pInput               = &clkSchematicDag.sppll0.vco.super.super,

    .sppll0.vco.super.super.pVirtual            = &clkVirtual_RoVco,
    .sppll0.vco.super.pInput                    = &clkSchematicDag.xtal.super,
    .sppll0.vco.coeffRegAddr                    = LW_PVTRIM_SYS_SPPLL0_COEFF,
    .sppll0.vco.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL0,


    .sppll1.readonly.super.super.pVirtual       = &clkVirtual_ReadOnly,
    .sppll1.readonly.super.pInput               = &clkSchematicDag.sppll1.vco.super.super,

    .sppll1.vco.super.super.pVirtual            = &clkVirtual_RoVco,
    .sppll1.vco.super.pInput                    = &clkSchematicDag.xtal.super,
    .sppll1.vco.coeffRegAddr                    = LW_PVTRIM_SYS_SPPLL1_COEFF,
    .sppll1.vco.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL1,

    /* ------------------------ XTAL ---------------------------------------- */

    .xtal.super.pVirtual                        = &clkVirtual_Xtal,
    .xtal.freqKHz                               = 27000,
};

//
// In MCLK above, the number of elements in the input array must equal the
// number of elements in the mux value map, which is 4 in both cases.
//
ct_assert(CLK_INPUT_COUNT__SWDIV4 == sizeof(clkMuxValueMap_SwDiv4) / sizeof(*clkMuxValueMap_SwDiv4));
ct_assert(CLK_INPUT_COUNT__SWDIV4 == sizeof(clkSchematicDag.mclk.swdiv.input) / sizeof(*clkSchematicDag.mclk.swdiv.input));

/*!
 * @brief   Schematic Construction
 * @see     clkSchematicDag
 * @see     clkFreqDomainArray
 *
 * @details Construct the schematic dag for this litter.  Specifically, the
 *          private 'clkSchematicDag' structure is initialized for anything
 *          that can not be determined at compile time.  Most fields in this
 *          structure are initialized at compile time.
 *
 *          This function is called after all the ClkFreqSrc and ClkFreqDomain
 *          have been constructed.
 */
FLCN_STATUS
clkConstruct_SchematicDag
(
    void
)
{
    ClkSignal   signal;             // Discarded result from 'clkRead'
    FLCN_STATUS status = FLCN_OK;
    FLCN_STATUS s;

    status = clkScrubDomainList_HAL();

    //
    // Read all read-only objects in order to update their caches to match the
    // hardware as initialized by VBIOS/devinit.  If the read fails, continue
    // to read the other objects.  Report only the first failure.
    //
    s = clkRead_FreqSrc_VIP(&clkSchematicDag.defrost.readonly.super.super, &signal, LW_FALSE);
    if (status == FLCN_OK)
    {
        status = s;
    }
    
    //
    // SPPLL[01] are used only by these clock domains.  If they are both disabled
    // then there is no point in reading them.
    //
    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK)] ||
        clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK)])
    {
        s = clkRead_FreqSrc_VIP(&clkSchematicDag.sppll0.readonly.super.super, &signal, LW_FALSE);
        if (status == FLCN_OK)
        {
            status = s;
        }

        s = clkRead_FreqSrc_VIP(&clkSchematicDag.sppll1.readonly.super.super, &signal, LW_FALSE);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    //
    // Read simple frequency domains to fill phase zero cache.
    // Do not read the NAFLL domains since they depend on other BAORDOBJ initialization.
    // There is also point in reading volatile domains.
    //

    // Domains that are specific to chips with displays.
    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK)])
    {
        s = clkRead_FreqDomain_VIP(&clkSchematicDag.dispclk.super);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK)])
    {
        s = clkRead_FreqDomain_VIP(&clkSchematicDag.hubclk.super);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    // Other domains
    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK)])
    {
        s = clkRead_FreqDomain_VIP(&clkSchematicDag.utilsclk.super);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK)])
    {
        s = clkRead_FreqDomain_VIP(&clkSchematicDag.pwrclk.super);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    if (clkFreqDomainArray[BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)])
    {
        LwU32 ddrModeRegVal = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

        // Set @ref bDoubleOutputFreq if it's G6x
        if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_BROADCAST, _DDR_MODE, _GDDR6X, ddrModeRegVal))
        {
            clkSchematicDag.mclk.drampll.bDoubleOutputFreq  = LW_TRUE;
        }

        s = clkRead_FreqDomain_VIP(&clkSchematicDag.mclk.domain.super);
        if (status == FLCN_OK)
        {
            status = s;
        }
    }

    // Done
    return status;
}


/*!
 * @brief       Table of all domains in the schematic
 *
 * @details     It is essential that C99 designated initializers are used here.
 *              See doxygen flowerbox for this array in 'clkdomain.h' for details.
 */
ClkFreqDomain *clkFreqDomainArray[CLK_DOMAIN_ARRAY_SIZE]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkFreqDomainArray") =
{
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_GPCCLK)]     = &clkSchematicDag.gpcclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_XBARCLK)]    = &clkSchematicDag.xbarclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_SYSCLK)]     = &clkSchematicDag.sysclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_LWDCLK)]     = &clkSchematicDag.lwdclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK)]    = &clkSchematicDag.dispclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK)]     = &clkSchematicDag.hubclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK)]   = &clkSchematicDag.utilsclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK)]     = &clkSchematicDag.pwrclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)] = &clkSchematicDag.pciegenclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)]       = &clkSchematicDag.mclk.domain.super,
};


