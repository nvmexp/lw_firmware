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
 * @author      Eric Colter
 * @author      Antone Vogt-Varvak
 *
 * @details     The main purpose of this file is to declare and define the
 *              schematic dag for chips which
 *              - Are Hopper and later (using SWDIVs),
 *              - Are displayless, and
 *              - Use HBM (High-Bandwidth Memory).
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
#include "dev_fuse.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "clk3/generic_dev_trim.h"
#include "hwproject.h"

// Clock Frequency Domains
#include "clk3/dom/clkfreqdomain_bif.h"
#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"
#include "clk3/dom/clkfreqdomain_mclk.h"

// Frequency Source Objects
#include "clk3/fs/clkhpll.h"
#include "clk3/fs/clkldivv2.h"
#include "clk3/fs/clkmux.h"
#include "clk3/fs/clkreadonly.h"
#include "clk3/fs/clkropdiv.h"
#include "clk3/fs/clkrovco.h"
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

#if !PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_HBM_SUPPORTED)
#error This schematic is specific to HBM (High-Bandwidth Memory).
#endif

#if !PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS)
#error This schematic is specific to displayless chips.
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

    // MCLK on displayless GH100 is for HBM memory (not GDDR/SDDR).
    struct
    {
        ClkMclkFreqDomain               domain;     // mclk.domain
        struct
        {
            ClkRoPDiv                   pdiv;       // mclk.hbmpll.pdiv
            ClkRoVco                    vco;        // mclk.hbmpll.vco
        } hbmpll;

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
    } defrost;                                      // Sometimes called XTAL4X

    ClkXtal                             xtal;       // xtal

} ClkSchematicDag;

const ClkFieldValue clkMuxValueMap_pwrclk[4]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_pwrclk") =
{

    // 0: xtal 27MHz
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_0)
    },

    // 1: xtal 27MHz w/o BYPASS FSM
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _XTAL_1)
    },

    // 2: defrost divider w/o BYPASS_FSM (automatic)
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL),
        DRF_DEF(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _CLOCK_SOURCE_SEL, _DEFROST_CLK)
    },

    // 3: XTAL divider w/o BYPASS_FSM
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

    .utilsclk.swdiv.input[0u]                    = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.input[1u]                    = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.input[2u]                    = &clkSchematicDag.utilsclk.swdiv.div2.super.super,
    .utilsclk.swdiv.input[3u]                    = &clkSchematicDag.utilsclk.swdiv.div3.super.super,


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

    .pwrclk.super.pVirtual                       = &clkVirtual_FixedFreqDomain,
    .pwrclk.super.pRoot                          = &clkSchematicDag.pwrclk.swdiv.mux.super,

    .pwrclk.swdiv.div2.super.super.pVirtual      = &clkVirtual_LdivV2,
    .pwrclk.swdiv.div2.super.pInput              = &clkSchematicDag.defrost.readonly.super.super,
    .pwrclk.swdiv.div2.ldivRegAddr               = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,

    .pwrclk.swdiv.div3.super.super.pVirtual      = &clkVirtual_LdivV2,
    .pwrclk.swdiv.div3.super.pInput              = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.div3.ldivRegAddr               = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,

    .pwrclk.swdiv.mux.super.pVirtual             = &clkVirtual_Mux,
    .pwrclk.swdiv.mux.muxRegAddr                 = LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER,
    .pwrclk.swdiv.mux.muxValueMap                = clkMuxValueMap_pwrclk,
    .pwrclk.swdiv.mux.input                      = clkSchematicDag.pwrclk.swdiv.input,
    .pwrclk.swdiv.mux.count                      = LW_ARRAY_ELEMENTS(clkSchematicDag.pwrclk.swdiv.input),
    .pwrclk.swdiv.mux.glitchy                    = LW_FALSE,

    .pwrclk.swdiv.input[0u]                      = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.input[1u]                      = &clkSchematicDag.xtal.super,
    .pwrclk.swdiv.input[2u]                      = &clkSchematicDag.pwrclk.swdiv.div2.super.super,
    .pwrclk.swdiv.input[3u]                      = &clkSchematicDag.pwrclk.swdiv.div3.super.super,

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

    .gpcclk.nafll[7].id                         = LW2080_CTRL_CLK_NAFLL_ID_GPC7,
    .gpcclk.nafll[7].super.pVirtual             = &clkVirtual_Nafll,

// We must initialize an element in gpcclk.nafll for each GPC.
#if LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 != 8
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
     * The block diagram of schema defined for HBM (high-bandwisth memory) MCLK:
     *                  ____________
     *                 |     |      |
     *      XTAL ------| VCO | PDIV |------+
     *                 |_____|______|      |        ______
     *                     HPMBLL          |       |      | bypass in
     *                                     +-------|1     | PLL register
     *                                             |      |
     *                                  ______     |      |
     *      XTAL ------------------+   |      |    | SW2  |---> OUTPUT
     *               _______       +---|0     |    |      |
     *              |       |      |   |      |    |      |
     *   DEFROST ---| LDIV2 |---+  +---|1     |    |      |
     *              |_______|   |      |  SW4 |----|0     |
     *               _______    +------|2     |    |______|
     *              |       |          |      |
     *      XTAL ---| LDIV3 |----------|3     |
     *              |_______|          |______|
     *           \_____________________________/
     *                        SWDIV
     *
     * Per bug 2622250, we must use unicast registers for FBIO instead of broadcast.
     * As such, clkConstruct_SchematicDag sets the addresses based on floorsweeping.
     * See also bug 2369474.
     *
     * Starting with Hopper, SWDIVs (switch dividers) have a finite state machine
     * that orders the programming of the switch and dividers.  One feature of
     * the FSM is that ldiv2 and ldiv3 have the same divider value controlled
     * by the same register and field.  See bug 2991907/55 and in Perforce at
     * hw/doc/gpu/SOC/Clocks/Documentation/POR/GH100/Divider_switch.docx
     *
     * HBMCLK/HBMPLL are read-only.
     */
    .mclk.domain.super.pVirtual                 = &clkVirtual_MclkFreqDomain,
    .mclk.domain.super.pRoot                    = &clkSchematicDag.mclk.domain.mux.super,
    CLK_STATIC_INIT__DOMAIN(mclk.domain.super)

    .mclk.domain.input[CLK_INPUT_BYPASS__MCLK]  = &clkSchematicDag.mclk.swdiv.mux.super,
    .mclk.domain.input[CLK_INPUT_HBMPLL__MCLK]  = &clkSchematicDag.mclk.hbmpll.pdiv.super.super,

    .mclk.domain.mux.super.pVirtual             = &clkVirtual_Mux,
//  .mclk.domain.mux.muxRegAddr                 = LW_PFB_FBPA_i_FBIO_HBMPLL_CFG,    Based on floorsweeping
    .mclk.domain.mux.muxValueMap                = clkMuxValueMap_MclkFreqDomain,    // Either DDR or HBM
    .mclk.domain.mux.input                      = clkSchematicDag.mclk.domain.input,
    .mclk.domain.mux.count                      = CLK_INPUT_COUNT__MCLK,            // Either DDR or HBM
    .mclk.domain.mux.glitchy                    = LW_FALSE,

    .mclk.hbmpll.pdiv.super.super.pVirtual      = &clkVirtual_RoPDiv,
    .mclk.hbmpll.pdiv.super.pInput              = &clkSchematicDag.mclk.hbmpll.vco.super.super,
//  .mclk.hbmpll.pdiv.regAddr                   = LW_PFB_FBPA_i_FBIO_COEFF_CFG,    Based on floorsweeping
    .mclk.hbmpll.pdiv.base                      = DRF_BASE(LW_PFB_FBPA_FBIO_HBMPLL_COEFF_PLDIV),
    .mclk.hbmpll.pdiv.size                      = DRF_SIZE(LW_PFB_FBPA_FBIO_HBMPLL_COEFF_PLDIV),

    .mclk.hbmpll.vco.super.super.pVirtual       = &clkVirtual_RoVco,
    .mclk.hbmpll.vco.super.pInput               = &clkSchematicDag.xtal.super,
//  .mclk.hbmpll.vco.cfgRegAddr                 = LW_PFB_FBPA_i_FBIO_HBMPLL_CFG,    Based on floorsweeping
//  .mclk.hbmpll.vco.coeffRegAddr               = LW_PFB_FBPA_i_FBIO_HBMPLL_COEFF,  Based on floorsweeping
    .mclk.hbmpll.vco.source                     = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,

    .mclk.swdiv.mux.super.pVirtual              = &clkVirtual_Mux,
    .mclk.swdiv.mux.muxRegAddr                  = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,
    .mclk.swdiv.mux.muxValueMap                 = clkMuxValueMap_SwDiv4,
    .mclk.swdiv.mux.input                       = clkSchematicDag.mclk.swdiv.input,
    .mclk.swdiv.mux.count                       = CLK_INPUT_COUNT__SWDIV4,
    .mclk.swdiv.mux.glitchy                     = LW_FALSE,

    .mclk.swdiv.input[0]                        = &clkSchematicDag.xtal.super,
    .mclk.swdiv.input[1]                        = &clkSchematicDag.xtal.super,
    .mclk.swdiv.input[2]                        = &clkSchematicDag.mclk.swdiv.div2.super.super,
    .mclk.swdiv.input[3]                        = &clkSchematicDag.mclk.swdiv.div3.super.super,

    .mclk.swdiv.div2.super.super.pVirtual       = &clkVirtual_LdivV2,
    .mclk.swdiv.div2.super.pInput               = &clkSchematicDag.defrost.readonly.super.super,
    .mclk.swdiv.div2.ldivRegAddr                = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,

    .mclk.swdiv.div3.super.super.pVirtual       = &clkVirtual_LdivV2,
    .mclk.swdiv.div3.super.pInput               = &clkSchematicDag.xtal.super,
    .mclk.swdiv.div3.ldivRegAddr                = LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER,

    /* ------------------------ DEFROST ------------------------------------- */

    //
    // The following assumptions apply about 'defrost':
    // - It is not programmed after initialization (read-only);
    // - Fractional NDIV is not being used; and
    // - PLDIV is not being used.
    //
    // NOTE: November 2019: Unlike GA10x, LW_PTRIM_SYS_XTAL4X_CFG contains
    // a _PLLOUT_DIV field which is not lwrrently POR.  If it becomes POR,
    // a ClkPDiv object (or similar) would be necessary.
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
    .defrost.vco.coeffRegAddr                   = LW_PTRIM_SYS_XTAL4X_COEFF,
    .defrost.vco.source                         = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,

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
    // Read the read-only object in order to update its cache to match the
    // hardware as initialized by VBIOS/devinit.  If the read fails, continue
    // to read the other objects.  Report only the first failure.
    //
    s = clkRead_FreqSrc_VIP(&clkSchematicDag.defrost.readonly.super.super, &signal, LW_FALSE);
    if (status == FLCN_OK)
    {
        status = s;
    }
    
    //
    // Read simple frequency domains to fill phase zero cache.
    // Do not read the NAFLL domains since they depend on other BAORDOBJ initialization.
    // There is also point in reading volatile domains.
    //
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
        //
        // Per bug 2622250, we must use unicast registers for FBIO instead of broadcast.
        // We may choose any nonfloorswept registers assuming that all FBIOs have
        // been programmed the same by the FbFalcon ucode.  See also bug 2369474.
        // This issue is specific to HBM since DDR does not use FBIO.
        // See wiki.lwpu.com/gpuhwvoltaindex.php/GV100_HBM_Bringup#Serial_priv_bus
        //
#if LW_FUSE_STATUS_OPT_FBIO_IDX_ENABLE != 0 || LW_FUSE_STATUS_OPT_FBIO_IDX_DISABLE != 1
#error Logic below uses the ~ operator under the assumption that LW_FUSE_STATUS_OPT_FBIO_IDX_ENABLE is zero.
#endif
        // Choose the lowest-orderded FBIO that has not been floorswept.
        LwU32 fbioStatusData     = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_FBIO);
        LwU32 fbioFloorsweptMask = DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, fbioStatusData);
        LwU32 fbioValidMask      = LWBIT32(REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS)) - 1;
        LwU32 fbioActiveMask     = (~fbioFloorsweptMask) & fbioValidMask;
        LwU8  fbioToUse          = (LwU8) BIT_IDX_32(LOWESTBIT(fbioActiveMask));

        // Set the register addresses to use the chosen FBIO.
        clkSchematicDag.mclk.domain.mux.muxRegAddr      = LW_PFB_FBPA_UC_FBIO_HBMPLL_CFG(fbioToUse);
        clkSchematicDag.mclk.hbmpll.pdiv.regAddr        =
        clkSchematicDag.mclk.hbmpll.vco.coeffRegAddr    = LW_PFB_FBPA_UC_FBIO_HBMPLL_COEFF(fbioToUse);
        
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
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK)]   = &clkSchematicDag.utilsclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK)]     = &clkSchematicDag.pwrclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)] = &clkSchematicDag.pciegenclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)]       = &clkSchematicDag.mclk.domain.super,
};


