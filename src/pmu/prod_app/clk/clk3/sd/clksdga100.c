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
 *              - Are Ampere (using SWDIVs),
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
#include "clk3/generic_dev_trim.h"
#include "hwproject.h"

// Clock Frequency Domains
#include "clk3/dom/clkfreqdomain_bif.h"
#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"
#include "clk3/dom/clkfreqdomain_mclk.h"

// Frequency Source Objects
#include "clk3/fs/clkxtal.h"
#include "clk3/fs/clkmux.h"
#include "clk3/fs/clkldivv2.h"
#include "clk3/fs/clkapll.h"
#include "clk3/fs/clkreadonly.h"
#include "clk3/fs/clksppll.h"

#include "clk3/fs/clkswdiv4.h"

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
    ClkSingleNafllFreqDomain            hostclk;

    // MCLK on displayless GA100 is for HBM memory (not GDDR/SDDR).
    struct
    {   ClkMclkFreqDomain               domain;     // mclk.domain
        ClkAPll                         hbmpll;     // mclk.hbmpll
        ClkSwDiv4                       swdiv;      // mclk.swdiv
    } mclk;

    //
    // Read-Only Domains
    //
    struct
    {
        ClkFixedFreqDomain              super;
        ClkSwDiv4                       swdiv;
    } utilsclk;

    struct
    {
        ClkFixedFreqDomain              super;
        ClkSwDiv4                       swdiv;
    } pwrclk;

    ClkSppll sppll0;        // Typically 2.7GHz
    ClkXtal  xtal;          // xtal
    ClkXtal  xtal4x;        // xtal4x
    ClkXtal  xtal16x;       // xtal16x

} ClkSchematicDag;


/***************************************************************
 * Global Schematic DAG
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
 *
 *          This structure is used to build objects whose structures are named
 *          'RM_PMU_CLK_FREQ_DOMAIN_xxx' via BOARDOBJ, which are defined in
 *          'common/inc/pmu/pmuifclk.h'.
 */
ClkSchematicDag clkSchematicDag
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkSchematicDag") =
{

    /* ------------------------ UTILSCLK ----------------------------------- */
    /*
     *                          |\
     *           xtal ----------|0\
     *                          |  \       ______       ______
     *         sppll0 ----------|1  |     |      |     |      |
     *                          |   |-----| DIV  |-----| ROOT |---- OUTPUT
     *         xtal4x ----------|2  |     |______|     |______|
     *                          |  /
     *    [pexrefclk] ----------|3/
     *                          |/
     */

    .utilsclk.super.pVirtual                        = &clkVirtual_FixedFreqDomain,
    .utilsclk.super.pRoot                           = &clkSchematicDag.utilsclk.swdiv.div.super.super,
    CLK_STATIC_INIT__DOMAIN(utilsclk.super)
    CLK_STATIC_INIT__SWDIV4(utilsclk.swdiv, LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH_DIVIDER)
    .utilsclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH_DIVIDER, _XTAL_PLL_REFCLK)] = &clkSchematicDag.xtal.super,
    .utilsclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH_DIVIDER, _SPPLL0)]         = &clkSchematicDag.sppll0.readonly.super.super,
    .utilsclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH_DIVIDER, _XTAL4X)]         = &clkSchematicDag.xtal4x.super,
    .utilsclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_UTILSCLK_OUT_SWITCH_DIVIDER, _PEX_REFCLK)]     = NULL,   // pexrefclk -- not used in production

    /* ------------------------ PWRCLK ------------------------------------- */
    /*
     *                          |\
     *         [xtal] ----------|0\
     *                          |  \       ______       ______
     *         sppll0 ----------|1  |     |      |     |      |
     *                          |   |-----| DIV  |-----| ROOT |---- OUTPUT
     *        xtal16x ----------|2  |     |______|     |______|
     *                          |  /
     *           xtal ----------|3/
     *                          |/
     */

    .pwrclk.super.pVirtual                          = &clkVirtual_FixedFreqDomain,
    .pwrclk.super.pRoot                             = &clkSchematicDag.pwrclk.swdiv.div.super.super,
    CLK_STATIC_INIT__DOMAIN(pwrclk.super)
    CLK_STATIC_INIT__SWDIV4(pwrclk.swdiv, LW_PTRIM_SYS_PWRCLK_OUT_SWITCH_DIVIDER)
    .pwrclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _XTAL_PLL_REFCLK)] = &clkSchematicDag.xtal.super,   // not explicit in dev_trim.h
    .pwrclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _SPPLL0)]         = &clkSchematicDag.sppll0.readonly.super.super,
    .pwrclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _XTAL16X)]        = &clkSchematicDag.xtal16x.super,
    .pwrclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_PWRCLK_OUT_SWITCH_DIVIDER, _XTAL_PLL_REFCLK)] = &clkSchematicDag.xtal.super,

    /* ------------------------ PCIEGENCLK ---------------------------------- */

    .pciegenclk.super.pVirtual                  = &clkVirtual_BifFreqDomain,
    .pciegenclk.super.pRoot                     = &clkSchematicDag.pciegenclk.bif.super,
    CLK_STATIC_INIT__DOMAIN(pciegenclk.super)
    .pciegenclk.bif.maxGenSpeed                 = RM_PMU_BIF_LINK_SPEED_GEN4PCIE,
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

    /* ------------------------ HOSTCLK ------------------------------------- */

    .hostclk.super.pVirtual                     = &clkVirtual_SingleNafllFreqDomain,
    .hostclk.super.pRoot                        = &clkSchematicDag.hostclk.nafll.super,
    CLK_STATIC_INIT__DOMAIN(hostclk.super)

    .hostclk.nafll.id                           = LW2080_CTRL_CLK_NAFLL_ID_HOST,
    .hostclk.nafll.super.pVirtual               = &clkVirtual_Nafll,

    /* ------------------------ MCLK --------------------------------------- */
    /*
     * The block diagram of schema defined for HBM MCLK:
     *                      ________
     *                     |        |
     *            XTAL ----| HBMPLL |-----------+
     *                     |________|           |    ______
     *                                          |   |      | bypass in PLL register
     *                      ______              +---|1     |
     *                     |      |                 |  SW2 |---> OUTPUT
     *             NULL ---|0     |             +---|0     |
     *                     |      |    ______   |   |______|
     *             NULL ---|1     |   |      |  |
     *                     |  SW4 |---| LDIV |--+
     *    SPPLL0 --- RO ---|2     |   |______|
     *                     |      |
     *             XTAL ---|3     |
     *                     |______| SWDIV
     *
     * Per bug 2622250, we must use unicast registers for FBIO instead of broadcast.
     * As such, clkConstruct_SchematicDag sets the addresses based on floorsweeping.
     * See also bug 2369474.
     */
    .mclk.domain.super.pVirtual                 = &clkVirtual_MclkFreqDomain,
    .mclk.domain.super.pRoot                    = &clkSchematicDag.mclk.domain.mux.super,
    CLK_STATIC_INIT__DOMAIN(mclk.domain.super)

    .mclk.domain.input[CLK_INPUT_BYPASS__MCLK]  = &clkSchematicDag.mclk.swdiv.div.super.super,
    .mclk.domain.input[CLK_INPUT_HBMPLL__MCLK]  = &clkSchematicDag.mclk.hbmpll.super.super,

    .mclk.domain.mux.super.pVirtual             = &clkVirtual_Mux,
//  .mclk.domain.mux.muxRegAddr                 = LW_PFB_FBPA_i_FBIO_HBMPLL_CFG,    Based on floorsweeping
    .mclk.domain.mux.muxValueMap                = clkMuxValueMap_MclkFreqDomain,    // Either DDR or HBM
    .mclk.domain.mux.input                      = clkSchematicDag.mclk.domain.input,
    .mclk.domain.mux.count                      = CLK_INPUT_COUNT__MCLK,            // Either DDR or HBM
    .mclk.domain.mux.glitchy                    = LW_FALSE,

    .mclk.hbmpll.super.super.pVirtual           = &clkVirtual_APll,
    .mclk.hbmpll.super.pInput                   = &clkSchematicDag.xtal.super,
//  .mclk.hbmpll.cfgRegAddr                     = LW_PFB_FBPA_i_FBIO_HBMPLL_CFG,    Based on floorsweeping
//  .mclk.hbmpll.coeffRegAddr                   = LW_PFB_FBPA_i_FBIO_HBMPLL_COEFF,  Based on floorsweeping
    .mclk.hbmpll.source                         = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .mclk.hbmpll.bPldivEnable                   = LW_TRUE,
    .mclk.hbmpll.bDiv2Enable                    = LW_TRUE,

    CLK_STATIC_INIT__SWDIV4(mclk.swdiv, LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER)

    // LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL
    .mclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_DRAMCLK_ALT_SWITCH_DIVIDER, _SPPLL0)] = &clkSchematicDag.sppll0.readonly.super.super,
    .mclk.swdiv.input[CLK_INPUT_INDEX__SWDIV4(_PTRIM, _SYS_DRAMCLK_ALT_SWITCH_DIVIDER, _XTAL_PLL_REFCLK)] = &clkSchematicDag.xtal.super,


    /* ------------------------ SPPLLs -------------------------------------- */

    // See hw/libs/common/analog/lwpu/doc/PLL16G_SSD_AESD_E.doc
    .sppll0.readonly.super.super.pVirtual       = &clkVirtual_ReadOnly,
    .sppll0.readonly.super.pInput               = &clkSchematicDag.sppll0.mux.super,
    .sppll0.readonly.cache.freqKHz              = 0,    // Ilwalidated cache

    .sppll0.mux.super.pVirtual                  = &clkVirtual_Mux,
    .sppll0.mux.muxRegAddr                      = LW_PVTRIM_SYS_BYPASSCTRL_DISP,
    .sppll0.mux.muxValueMap                     = clkMuxValueMap_Sppll0,
    .sppll0.mux.input                           = clkSchematicDag.sppll0.input,
    .sppll0.mux.count                           = CLK_INPUT_COUNT__SPPLL,
    .sppll0.mux.glitchy                         = LW_FALSE,

    .sppll0.input[CLK_INPUT_VCO__SPPLL]         = &clkSchematicDag.sppll0.pll.super.super,
    .sppll0.input[CLK_INPUT_BYPASSCLK__SPPLL]   = &clkSchematicDag.xtal.super,

    .sppll0.pll.super.super.pVirtual            = &clkVirtual_APll,
    .sppll0.pll.super.pInput                    = &clkSchematicDag.xtal.super,
    .sppll0.pll.cfgRegAddr                      = LW_PVTRIM_SYS_SPPLL0_CFG,
    .sppll0.pll.coeffRegAddr                    = LW_PVTRIM_SYS_SPPLL0_COEFF,
    .sppll0.pll.settleNs                        = 64000,
    .sppll0.pll.slideStepDelayNs                =   100,
    .sppll0.pll.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL0,
    .sppll0.pll.bPldivEnable                    = LW_FALSE,

    .xtal.super.pVirtual                        = &clkVirtual_Xtal,
    .xtal.freqKHz                               = 27000,

    .xtal4x.super.pVirtual                      = &clkVirtual_Xtal,
    .xtal4x.freqKHz                             = 27000 * 4,

    .xtal16x.super.pVirtual                     = &clkVirtual_Xtal,
    .xtal16x.freqKHz                            = 27000 * 16,
};


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
    // hardware as initialized by VBIOS/devinit.
    //
    s  = clkRead_FreqSrc_VIP(&clkSchematicDag.sppll0.readonly.super.super, &signal, LW_FALSE);
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
        clkSchematicDag.mclk.domain.mux.muxRegAddr  =
        clkSchematicDag.mclk.hbmpll.cfgRegAddr      = LW_PFB_FBPA_UC_FBIO_HBMPLL_CFG(fbioToUse);
        clkSchematicDag.mclk.hbmpll.coeffRegAddr    = LW_PFB_FBPA_UC_FBIO_HBMPLL_COEFF(fbioToUse);

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
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HOSTCLK)]    = &clkSchematicDag.hostclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK)]   = &clkSchematicDag.utilsclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK)]     = &clkSchematicDag.pwrclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)] = &clkSchematicDag.pciegenclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)]       = &clkSchematicDag.mclk.domain.super,
};
