/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 */

/* ------------------------- System Includes -------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif

/* ------------------------- Application Includes --------------------------- */

#include "clk3/generic_dev_trim.h"
#include "clk3/fs/clkswdiv4automatic.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! The mux value map used for all 4-input SWDIVs
const ClkFieldValue clkMuxValueMap_SwDiv4Automatic[CLK_INPUT_COUNT__SWDIV4AUTOMATIC]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_SwDiv4Automatic") =
{

    // Override Source 0
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL) |
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE),
        DRF_DEF(_PTRIM, _SYS_SWDIV4, _CLOCK_SOURCE_SEL, _SOURCE(0)) |
        DRF_DEF(_PTRIM, _SYS_SWDIV, _CLOCK_SEL_OVERRIDE, _ENABLE)
    },

    // Override Source 1
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL) |
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE),
        DRF_DEF(_PTRIM, _SYS_SWDIV4, _CLOCK_SOURCE_SEL, _SOURCE(1)) |
        DRF_DEF(_PTRIM, _SYS_SWDIV, _CLOCK_SEL_OVERRIDE, _ENABLE)
    },


    // Override Source 2
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL) |
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE),
        DRF_DEF(_PTRIM, _SYS_SWDIV4, _CLOCK_SOURCE_SEL, _SOURCE(2)) |
        DRF_DEF(_PTRIM, _SYS_SWDIV, _CLOCK_SEL_OVERRIDE, _ENABLE)
    },

    // Override Source 3
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL) |
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE),
        DRF_DEF(_PTRIM, _SYS_SWDIV4, _CLOCK_SOURCE_SEL, _SOURCE(3)) |
        DRF_DEF(_PTRIM, _SYS_SWDIV, _CLOCK_SEL_OVERRIDE, _ENABLE)
    },

    // Automatic
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE),
        DRF_DEF(_PTRIM, _SYS_SWDIV, _CLOCK_SEL_OVERRIDE, _DISABLE)
    }
};


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */


