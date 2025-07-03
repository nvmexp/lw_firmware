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
 * @file
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Antone Vogt-Varvak
 * @author      Onkar Bimalkhedkar
 */

#ifndef CLK3_DOM_FREQDOMAIN_SWDIVAPLL_H
#define CLK3_DOM_FREQDOMAIN_SWDIVAPLL_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkldiv.h"
#include "clk3/fs/clkpdiv.h"
#include "clk3/fs/clkadynramp.h"
#include "clk3/fs/clkswdiv4.h"


/* ------------------------ Macros ----------------------------------------- */


/*!
 * @brief       Inputs for the pll/xtal SW4 switch (ClkSwDivAPllFreqDomain::pllxtal.mux)
 * @memberof    ClkSwDivAPllFreqDomain
 * @protected
 *
 * @details     The preprocessor is used to make sure all domains using this
 *              class have the same value for the POR paths.  As of AD10X,
 *              they do, and the domains are DISPCLK and HUBCLK.
 *
 *              If ClkSwDivAPllFreqDomain is used for additional domains, they
 *              must be added to the '#if' conditionals to make sure they are
 *              equal.
 *
 *              If future chips use PLL path values that vary between any pair
 *              of domains, it will be necessary to add a const member to
 *              ClkSwDivAPllFreqDomain to use in place of these macros.
 */
/**@{*/
#if LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_XTAL_PLL_REFCLK ==   \
    LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_XTAL_PLL_REFCLK
#define CLK_INPUT_SWDIV_XTAL__SWDIVAPLLFREQDOMAIN CLK_INPUT_INDEX__SWDIV4(_PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _XTAL_PLL_REFCLK)
#endif

#if LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_DISPPLL ==           \
    LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_DISPHUBPLL
#define CLK_INPUT_SWDIV_PLL__SWDIVAPLLFREQDOMAIN CLK_INPUT_INDEX__SWDIV4(_PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _DISPPLL)
#endif

#if LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_SPPLL0 ==            \
    LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_SPPLL0
#define CLK_INPUT_SWDIV_SPPLL0_PDIV__SWDIVAPLLFREQDOMAIN CLK_INPUT_INDEX__SWDIV4(_PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _SPPLL0)
#endif

#if LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_SPPLL1 ==            \
    LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER_CLOCK_SOURCE_SEL_SPPLL1
#define CLK_INPUT_SWDIV_SPPLL1_PDIV__SWDIVAPLLFREQDOMAIN CLK_INPUT_INDEX__SWDIV4(_PVTRIM, _SYS_DISPCLK_SWITCH_DIVIDER, _SPPLL1)
#endif


/*!
 * @brief       Inputs for the pll/xtal SW2 switch (ClkSwDivAPllFreqDomain::pllxtal.mux)
 * @memberof    ClkSwDivAPllFreqDomain
 * @protected
 */
/**@{*/
#define CLK_INPUT_PLLXTAL_PLL__SWDIVAPLLFREQDOMAIN       0
#define CLK_INPUT_PLLXTAL_XTAL__SWDIVAPLLFREQDOMAIN      1
/**@}*/


/*!
 * @brief       Number of inputs for the pll/xtal SW2 switch (ClkSwDivAPllFreqDomain::pllxtal.mux)
 * @memberof    ClkSwDivAPllFreqDomain
 * @protected
 */
#define CLK_INPUT_PLLXTAL_COUNT__SWDIVAPLLFREQDOMAIN     2


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkSwDivAPllFreqDomain               ClkSwDivAPllFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkSwDivAPllFreqDomain_Virtual;


/*!
 * @class       ClkSwDivAPllFreqDomain
 * @extends     ClkProgrammableFreqDomain
 * @see         Perforce:  hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_Structure.vsdx
 * @protected
 *
 * @details     This class represents domains with an analog PLL (a.k.a. APLL),
 *              SWLDIV, and a mux, for example, DISPCLK and HUBCLK on Ada.
 *
 *              The block diagram of schema defined by this class is:
 *                                                       ______
 *                                                      |      |
 *        xtal ---+-------------------------------------|0     |
 *                |     ____________       ______       |      |
 *                |    |            |     |      |      |      |
 *                +----| domain PLL |-----|0     |      |      |
 *                |    |____________|     |  SW2 |------|1     |    ______
 *                +-----------------------|1     |      |      |   |      |
 *                            ______      |______|      |  SW4 |---| LDIV |---> OUTPUT
 *                           |      |                   |      |   |______|
 *      sppll0 --------------| PDIV |-------------------|2     |
 *                           |______|                   |      |
 *                                         ______       |      |
 *                                        |      |      |      |
 *      sppll1 ---------------------------| PDIV |------|3     |
 *                                        |______|      |______| SWDIV
 *
 *              Bug 2557340 dislwsses the use of the PDIVs.
 *
 *              The generic register/field names associated with each object are:
 *                  PLL:  PLL_CFG, PLL_COEFF, et al.
 *                  SW2:  PLL_CFG_BYPASSPLL
 *                  PDIV: SPPLLn_COEFF2_PDIVd
 *                  SW4:  SWITCH_DIVIDER_CLOCK_SOURCE_SEL
 *                  LDIV: SWITCH_DIVIDER_DIVIDER_SEL
 *
 *              SW2 is part of the PLL in hardware, but not handled in the
 *              ClkPll class.
 *
 *              Each SPPLL has two PDIVs: A for DISPCLK and B for HUBCLK.
 *
 *              SW4+LDIV are collectively the switch divider (SWDIV), which is
 *              handled by the ClkSwDiv class.
 *
 *              The LDIV does not support fractional divide.
 *
 *              This class may be used with or without a dynamic ramp PLL, which
 *              can be selected by initializing pll.super.super.super.pVirtual to
 *              one of the following:
 *                      &clkVirtual_DynRamp     (with dynamic ramp)
 *                      &clkVirtual_Pll         (without dynamic ramp)
 */
struct ClkSwDivAPllFreqDomain
{
    ClkProgrammableFreqDomain   super;          // domain
    ClkSwDiv4                   swdiv;          // output with SW4 and LDIV
    ClkADynRamp                 dyn;            // domain-specific PLL (e.g. DISPPLL)

    struct
    {
        ClkMux                  mux;            // SW2 selecting between PLL v XTAL
        ClkFreqSrc             *input[CLK_INPUT_PLLXTAL_COUNT__SWDIVAPLLFREQDOMAIN];
    } pllxtal;

    ClkPDiv                     pdiv0;          // PDIV for SPPLL0
    ClkPDiv                     pdiv1;          // PDIV for SPPLL1

    LwU32                       altPathMaxKHz;  // Use 'pll' only above this frequency
};


/* ------------------------ External Definitions --------------------------- */

extern const ClkSwDivAPllFreqDomain_Virtual clkVirtual_SwDivAPllFreqDomain;
const ClkFieldValue clkMuxValueMap_pllxtal_SwDivAPllFreqDomain[CLK_INPUT_PLLXTAL_COUNT__SWDIVAPLLFREQDOMAIN];


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkSwDivAPllFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_SwDivAPllFreqDomain      clkRead_ProgrammableFreqDomain      // Inherit
FLCN_STATUS clkConfig_SwDivAPllFreqDomain(ClkSwDivAPllFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_SwDivAPllFreqDomain");
#define     clkProgram_SwDivAPllFreqDomain   clkProgram_ProgrammableFreqDomain   // Inherit
#define     clkCleanup_SwDivAPllFreqDomain   clkCleanup_ProgrammableFreqDomain   // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_SWDIVAPLL_H

