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
 *              schematic dag for Turing chips.
 * @details     The main purpose of this file is to declare and define the
 *              schematic dag for chips which
 *              - Are Turing (using OSMs),
 *              - Have display, and
 *              - Use DDR (Double Data Rate) memory.
 *
 *              More generally, everything specific to Turing in Clocks 3.x goes
 *              this file.  Nothing else should contain chip-specific data or
 *              logic since these data constructors contain flags to control
 *              specific behaviour.
 *
 *              In general, the schematic dag is declared and defined in
 *              "clk3/sd/clksd[chip].c", where [chip] is the first chip of
 *              the series of chips using that schematic.
 *
 * @note        Volta has essentially the same clocks schematic as Turing, so
 *              the logic in this file works with Volta as well.  In fact, it
 *              was developed on Volta.
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

// The manual
#include "dev_fbpa.h"
#include "dev_trim.h"
#include "clk3/generic_dev_trim.h"
#include "hwproject.h"

// Clock Frequency Domains
#include "clk3/dom/clkfreqdomain_apllldiv_disp.h"
#include "clk3/dom/clkfreqdomain_osm.h"
#include "clk3/dom/clkfreqdomain_bif.h"
#include "clk3/dom/clkfreqdomain_singlenafll.h"
#include "clk3/dom/clkfreqdomain_multinafll.h"
#include "clk3/dom/clkfreqdomain_mclk.h"

// Frequency Source Objects
#include "clk3/fs/clkldiv.h"
#include "clk3/fs/clkldivunit.h"
#include "clk3/fs/clkmultiplier.h"
#include "clk3/fs/clkmux.h"
#include "clk3/fs/clkosm.h"
#include "clk3/fs/clkapll.h"
#include "clk3/fs/clkreadonly.h"
#include "clk3/fs/clkasdm.h"
#include "clk3/fs/clksppll.h"
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
 *              'clkSchematicDag'.  Nothing than the initialization code in
 *              the source file references any of these symbols.
 *
 *              All other logic accesses these fields via 'clkFreqDomainArray'
 *              and/or pointers within these structures.
 *
 *              Why is this struct is "final" instead of "const"?  Because
 *              initialization logic in 'clkConstruct_SchematicDag' or elsewhere
 *              can modify it at runtime.
 *
 *              Why is this struct called 'ClkSchematicDag' instead of
 *              'ClkSchematicDag_[chip]'?  Because PMU builds are per-litter
 *              and there is one of these structures per litter.
 */
typedef struct
{
    //
    // Clock Frequency Domains
    //
    ClkDispFreqDomain                   dispclk;

    ClkOsmFreqDomain                    hubclk;
    ClkOsmFreqDomain                    utilsclk;
    ClkOsmFreqDomain                    pwrclk;

    ClkBifFreqDomain                    pciegenclk;

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

    // MCLK on Turing is for GDDR/SDDR (not HBM).
    struct
    {   ClkMclkFreqDomain               domain;     // mclk.domain
        ClkAPll                         drampll;    // mclk.drampll
        ClkASdm                         refmpll;    // mclk.refmpll
        ClkOsm                          refosm;     // mclk.refosm
        ClkOsm                          altosm;     // mclk.altosm
    } mclk;

    //
    // Shared among domains
    //
    struct // syspll
    {
        ClkReadOnly                     readonly;   // syspll.readonly
        ClkAPll                         pll;        // syspll.pll
    } syspll;

    ClkSppll                            sppll0;     // Typically 1620MHz
    ClkSppll                            sppll1;     // Typically 1620MHz
    ClkXtal                             xtal;       // Typically 27MHz
    ClkXtal                             xtal4x;     // Typically 108MHz

} ClkSchematicDag;


/***************************************************************
 * Global Schematic DAGs
 ***************************************************************/

/*!
 * @brief       Default PLL Info Table Entry for Slow PLLs
 * @see         Bug 200421380
 *
 * @details     In Turing (and Volta), this means DISPPLL, SPPLL0, and SPPLL1.
 *              These settings also apply to VPLLs, but they are not supported
 *              on the PMU in Clocks 3.x.
 *
 *              These values are meaningful during clkConfig and don't really
 *              matter for PLLs that we don't program.
 */
RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_SET pllInfoTableEntry_defaultSlow
    GCC_ATTRIB_SECTION("dmem_clk3x", "_pllInfoTableEntry_defaultSlow") =
{
    .MinRef     =   27,
    .MaxRef     =   27,
    .MilwCO     =  800,
    .MaxVCO     = 1620,
    .MinUpdate  =   13,
    .MaxUpdate  =   38,
    .MinM       =    1,
    .MaxM       =    1,
    .MinN       =   20,
    .MaxN       =  255,
    .MinPl      =    1,
    .MaxPl      =   31
};

/*!
 * @brief       Default PLL Info Table Entry for Fast PLLs
 *
 * @details     In Turing (and Volta), this means SYSPLL.
 *
 *              These values are meaningful during clkConfig and don't really
 *              matter for PLLs that we don't program.
 */
RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_SET pllInfoTableEntry_defaultFast
    GCC_ATTRIB_SECTION("dmem_clk3x", "_pllInfoTableEntry_defaultFast") =
{
    .MinRef     =  405,
    .MaxRef     =  405,
    .MilwCO     = 1750,
    .MaxVCO     = 3800,
    .MinUpdate  =   13,
    .MaxUpdate  =   38,
    .MinM       =   16,
    .MaxM       =   16,
    .MinN       =   40,
    .MaxN       =  255,
    .MinPl      =    1,
    .MaxPl      =   31
};


/*!
 * @brief   The schematic itself
 * @see     CLK_DEFINE_BOARDOBJ_EXTENDED_LEAF_CLASS
 *
 * @details The type of each object this structure is a struct whose name is
 *          'Clk[leafname]' where [leafname] is the base name of a leaf
 *          class such as 'Pll', 'Mux', 'ClkPllLdivFreqDomain' etc.
 *
 *          For the most part, these data do not change after initialization.
 *          Much of these data are initialized at compile-time.
 *
 *          This structure is used to build objects whose structures are named
 *          'RM_PMU_CLK_FREQ_DOMAIN_xxx' and 'LW2080_CTRL_CLK_FREQ_SOURCE_xxx'
 *          via BOARDOBJ, which are defined in 'common/inc/pmu/pmuifclk.h'.
 */
ClkSchematicDag clkSchematicDag
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkSchematicDag") =
{

    /* ------------------------ DISPCLK ------------------------------------- */

    .dispclk.super.pVirtual             = &clkVirtual_DispFreqDomain,
    .dispclk.super.pRoot                = &clkSchematicDag.dispclk.ldiv.mux.super,
    CLK_STATIC_INIT__DOMAIN(dispclk.super)

    //
    // WAR for bugs 1853081 (sw) and 1849902 (hw)
    //
    // Problem:  A superfluous pulse may occur on the DISPCLK or VCLK domains if:
    // SEL_VCO (dispclk.ldiv.mux) goes from 1 (VCO/PLL) to 0 (bypass), and
    // VCODIV (dispclk.ldiv.div1) is nonzero (i.e. divide by 1.0), and
    // BYPDIV (dispclk.ldiv.div0) is zero (i.e. divide by 1.0).
    //
    // Solution:  Disable the VCODIV keeping it at zero (i.e. divide by 1.0).
    // The PLDIV (disppll.pldiv) can be used to get the range we need.
    //
    // This problem applies only to Volta since the hardware folks intend to
    // remove the problematic 'redinsdel' optimization in Turing.  'redinsdel'
    // is the reduced insertion delay module.  However, there is no need to
    // program this LDIV, so there is no harm in leaving it disabled, which
    // makes 'clkConfig' run faster,  For that matter, we could also disable
    // 'dispclk.ldiv.div0'.
    //
    CLK_STATIC_INIT__LDIV(dispclk.ldiv, _PVTRIM, _SYS_SEL_VCO, _SYS_STATUS_SEL_VCO, _DISPCLK_OUT, _BYPASS, _SYS_DISPCLK_OUT)
    .dispclk.ldiv.div0.super.pInput             = &clkSchematicDag.dispclk.altosm.mux.super,
    .dispclk.ldiv.div0.maxLdivValue             = 63,
    .dispclk.ldiv.div0.maxFracDivValue          = 34,
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DISP_DYN_RAMP_SUPPORTED)
    .dispclk.ldiv.div1.super.pInput             = &clkSchematicDag.dispclk.dyn.super.super.super,
#else
    .dispclk.ldiv.div1.super.pInput             = &clkSchematicDag.dispclk.pll.super.super,
#endif
    .dispclk.ldiv.div1.maxLdivValue             =  2,       // Disable DISPCLK/DIV0 (VCO path) for Bug 1849902
    .dispclk.ldiv.div1.maxFracDivValue          =  2,       // Disable DISPCLK/DIV0 (VCO path) for Bug 1849902

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DISP_DYN_RAMP_SUPPORTED)
    .dispclk.dyn.super.super.super.pVirtual     = &clkVirtual_ADynRamp,
    .dispclk.dyn.super.super.pInput             = &clkSchematicDag.dispclk.refosm.mux.super,
    .dispclk.dyn.super.pPllInfoTableEntry       = &pllInfoTableEntry_defaultSlow,
    .dispclk.dyn.super.cfgRegAddr               = LW_PVTRIM_SYS_DISPPLL_CFG,
    .dispclk.dyn.super.coeffRegAddr             = LW_PVTRIM_SYS_DISPPLL_COEFF,
    .dispclk.dyn.cfg2RegAddr                    = LW_PVTRIM_SYS_DISPPLL_CFG2,
    .dispclk.dyn.ssd0RegAddr                    = LW_PVTRIM_SYS_DISPPLL_SSD0,
    .dispclk.dyn.dynRegAddr                     = LW_PVTRIM_SYS_DISPPLL_DYN,
    .dispclk.dyn.super.settleNs                 =  64000,
    .dispclk.dyn.super.slideStepDelayNs         =      0,   // Sliding is disabled.
    .dispclk.dyn.dynRampNs                      = 100000,   // Worst time as per Pranav Lodaya is 30us, will modify after testing on Silicon
    .dispclk.dyn.super.source                   = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .dispclk.dyn.super.bPldivEnable             = LW_TRUE,
    .dispclk.dyn.super.bDiv2Enable              = LW_FALSE,
#else
    .dispclk.pll.super.super.pVirtual           = &clkVirtual_APll,
    .dispclk.pll.super.pInput                   = &clkSchematicDag.dispclk.refosm.mux.super,
    .dispclk.pll.pPllInfoTableEntry             = &pllInfoTableEntry_defaultSlow,
    .dispclk.pll.cfgRegAddr                     = LW_PVTRIM_SYS_DISPPLL_CFG,
    .dispclk.pll.coeffRegAddr                   = LW_PVTRIM_SYS_DISPPLL_COEFF,
    .dispclk.pll.settleNs                       = 64000,
    .dispclk.pll.slideStepDelayNs               =     0,    // Sliding is disabled.
    .dispclk.pll.source                         = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .dispclk.pll.bPldivEnable                   = LW_TRUE,
    .dispclk.pll.bDiv2Enable                    = LW_FALSE,
#endif

    CLK_STATIC_INIT__OSM(dispclk.refosm,
        LW_PVTRIM_SYS_DISPCLK_REF_SWITCH,
        LW_PVTRIM_SYS_DISPCLK_REF_LDIV,
        LW_FALSE)

    CLK_STATIC_INIT__OSM(dispclk.altosm,
        LW_PVTRIM_SYS_DISPCLK_ALT_SWITCH,
        LW_PVTRIM_SYS_DISPCLK_ALT_LDIV,
        LW_FALSE)

    /* ------------------------ HUBCLK ------------------------------------- */

    .hubclk.super.pVirtual                      = &clkVirtual_OsmFreqDomain,
    .hubclk.super.pRoot                         = &clkSchematicDag.hubclk.osm.mux.super,
    CLK_STATIC_INIT__DOMAIN(hubclk.super)
    CLK_STATIC_INIT__OSM(hubclk.osm, LW_PTRIM_SYS_HUB2CLK_OUT_SWITCH, LW_PTRIM_SYS_HUB2CLK_OUT_LDIV, LW_TRUE)

    /* ------------------------ UTILSCLK ----------------------------------- */

    .utilsclk.super.pVirtual                    = &clkVirtual_OsmFreqDomain,
    .utilsclk.super.pRoot                       = &clkSchematicDag.utilsclk.osm.mux.super,
    CLK_STATIC_INIT__DOMAIN(utilsclk.super)
    CLK_STATIC_INIT__OSM(utilsclk.osm, LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, LW_PTRIM_SYS_UTILSCLK_OUT_LDIV, LW_TRUE)

    /* ------------------------ PWRCLK ------------------------------------- */

    .pwrclk.super.pVirtual                      = &clkVirtual_OsmFreqDomain,
    .pwrclk.super.pRoot                         = &clkSchematicDag.pwrclk.osm.mux.super,
    CLK_STATIC_INIT__DOMAIN(pwrclk.super)
    CLK_STATIC_INIT__OSM(pwrclk.osm, LW_PTRIM_SYS_PWRCLK_OUT_SWITCH, LW_PTRIM_SYS_PWRCLK_OUT_LDIV, LW_TRUE)

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

// We must initialize an element in gpcclk.nafll for each GPC.
#if LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1 != 6
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

    .mclk.domain.super.pVirtual                 = &clkVirtual_MclkFreqDomain,
    .mclk.domain.super.pRoot                    = &clkSchematicDag.mclk.domain.mux.super,
    CLK_STATIC_INIT__DOMAIN(mclk.domain.super)

    CLK_STATIC_INIT__OSM(mclk.altosm,
        LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH,
        LW_PTRIM_SYS_DRAMCLK_ALT_LDIV,
        LW_FALSE)

    .mclk.domain.input[CLK_INPUT_BYPASS__MCLK]  = &clkSchematicDag.mclk.altosm.mux.super,
    .mclk.domain.input[CLK_INPUT_REFMPLL__MCLK] = &clkSchematicDag.mclk.refmpll.super.super.super,
    .mclk.domain.input[CLK_INPUT_DRAMPLL__MCLK] = &clkSchematicDag.mclk.drampll.super.super,

    .mclk.domain.mux.super.pVirtual             = &clkVirtual_Mux,
    .mclk.domain.mux.muxRegAddr                 = LW_PTRIM_SYS_FBIO_MODE_SWITCH,
    .mclk.domain.mux.muxValueMap                = clkMuxValueMap_MclkFreqDomain,
    .mclk.domain.mux.input                      = clkSchematicDag.mclk.domain.input,
    .mclk.domain.mux.count                      = CLK_INPUT_COUNT__MCLK,
    .mclk.domain.mux.glitchy                    = LW_FALSE,

    .mclk.drampll.super.super.pVirtual          = &clkVirtual_APll,
    .mclk.drampll.super.pInput                  = &clkSchematicDag.mclk.refmpll.super.super.super,
    .mclk.drampll.cfgRegAddr                    = LW_PFB_FBPA_DRAMPLL_CFG,
    .mclk.drampll.coeffRegAddr                  = LW_PFB_FBPA_DRAMPLL_COEFF,
    .mclk.drampll.source                        = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .mclk.drampll.bPldivEnable                  = LW_TRUE,
    .mclk.drampll.bDiv2Enable                   = LW_TRUE,

    .mclk.refmpll.super.super.super.pVirtual    = &clkVirtual_ASdm,
    .mclk.refmpll.super.super.pInput            = &clkSchematicDag.mclk.refosm.mux.super,
    .mclk.refmpll.super.cfgRegAddr              = LW_PFB_FBPA_REFMPLL_CFG,
    .mclk.refmpll.super.coeffRegAddr            = LW_PFB_FBPA_REFMPLL_COEFF,
    .mclk.refmpll.cfg2RegAddr                   = LW_PFB_FBPA_REFMPLL_CFG2,
    .mclk.refmpll.ssd0RegAddr                   = LW_PFB_FBPA_REFMPLL_SSD0,
    .mclk.refmpll.super.source                  = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .mclk.refmpll.super.bPldivEnable            = LW_TRUE,
    .mclk.refmpll.super.bDiv2Enable             = LW_FALSE,

    CLK_STATIC_INIT__OSM(mclk.refosm,
        LW_PTRIM_SYS_REFCLK_REFMPLL_SWITCH,
        LW_PTRIM_SYS_REFCLK_REFMPLL_LDIV,
        LW_FALSE)

    /* ------------------------ SYSPLL -------------------------------------- */

    .syspll.readonly.super.super.pVirtual       = &clkVirtual_ReadOnly,
    .syspll.readonly.super.pInput               = &clkSchematicDag.syspll.pll.super.super,
    .syspll.readonly.cache.freqKHz              = 0,    // Ilwalidated cache

    .syspll.pll.super.super.pVirtual            = &clkVirtual_APll,
    .syspll.pll.super.pInput                    = &clkSchematicDag.xtal.super,
    .syspll.pll.cfgRegAddr                      = LW_PTRIM_SYS_SYSPLL_CFG,
    .syspll.pll.coeffRegAddr                    = LW_PTRIM_SYS_SYSPLL_COEFF,
    .syspll.pll.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
    .syspll.pll.bPldivEnable                    = LW_FALSE,


    /* ------------------------ SPPLLs -------------------------------------- */

    //
    // IMPORTANT:  On Turing (and prior), SPPLLs are always sourced via OSM.
    // As such, unlike subsequent chips, the 'SlkPll::source' field (below)
    // is set to _ONE_SOURCE, not _SPPLL[01].
    //

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
    .sppll0.pll.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE,
    .sppll0.pll.bPldivEnable                    = LW_FALSE,


    .sppll1.readonly.super.super.pVirtual       = &clkVirtual_ReadOnly,
    .sppll1.readonly.super.pInput               = &clkSchematicDag.sppll1.mux.super,
    .sppll1.readonly.cache.freqKHz              = 0,    // Ilwalidated cache

    .sppll1.mux.super.pVirtual                  = &clkVirtual_Mux,
    .sppll1.mux.muxRegAddr                      = LW_PVTRIM_SYS_BYPASSCTRL_DISP,
    .sppll1.mux.muxValueMap                     = clkMuxValueMap_Sppll1,
    .sppll1.mux.input                           = clkSchematicDag.sppll1.input,
    .sppll1.mux.count                           = CLK_INPUT_COUNT__SPPLL,
    .sppll1.mux.glitchy                         = LW_FALSE,

    .sppll1.input[CLK_INPUT_VCO__SPPLL]         = &clkSchematicDag.sppll1.pll.super.super,
    .sppll1.input[CLK_INPUT_BYPASSCLK__SPPLL]   = &clkSchematicDag.xtal.super,

    .sppll1.pll.super.super.pVirtual            = &clkVirtual_APll,
    .sppll1.pll.super.pInput                    = &clkSchematicDag.xtal.super,
    .sppll1.pll.cfgRegAddr                      = LW_PVTRIM_SYS_SPPLL1_CFG,
    .sppll1.pll.coeffRegAddr                    = LW_PVTRIM_SYS_SPPLL1_COEFF,
    .sppll1.pll.source                          = LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE,
    .sppll1.pll.bPldivEnable                    = LW_FALSE,

    .xtal.super.pVirtual                        = &clkVirtual_Xtal,
    .xtal.freqKHz                               = 27000,

    .xtal4x.super.pVirtual                      = &clkVirtual_Xtal,
    .xtal4x.freqKHz                             = 27000 * 4,
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

    //
    // Read all read-only objects in order to update their caches to match the
    // hardware as initialized by VBIOS/devinit.
    //
    status = clkRead_FreqSrc_VIP(&clkSchematicDag.syspll.readonly.super.super, &signal, LW_FALSE);
    s      = clkRead_FreqSrc_VIP(&clkSchematicDag.sppll0.readonly.super.super, &signal, LW_FALSE);
    if (status == FLCN_OK)
    {
        status = s;
    }
    s      = clkRead_FreqSrc_VIP(&clkSchematicDag.sppll1.readonly.super.super, &signal, LW_FALSE);
    if (status == FLCN_OK)
    {
        status = s;
    }

    //
    // Read most frequency domains to fill phase zero cache.
    // Do not read the NAFLL domains since they depend on other BAORDOBJ initialization.
    // There is also point in reading volatile domains.
    //
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
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK)]    = &clkSchematicDag.dispclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK)]     = &clkSchematicDag.hubclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK)]   = &clkSchematicDag.utilsclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK)]     = &clkSchematicDag.pwrclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)] = &clkSchematicDag.pciegenclk.super,
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK)]       = &clkSchematicDag.mclk.domain.super,
};


