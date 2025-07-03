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
#include "clk3/fs/clkosm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! The mux value map used for ALL OSMs
const ClkFieldValue clkMuxValueMap_Osm[CLK_INPUT_COUNT__OSM]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_Osm") =
{
    //  0  finalsel=2  miscclk=0    usually unconnected  formerly _PEX_xxxCLK_FAST
    [CLK_INPUT_MISCCLK0__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   0)
    },

    //  1  finalsel=2  miscclk=1    usually unconnected  formerly _HOSTCLK_DIV
    [CLK_INPUT_MISCCLK1__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   1)
    },

    //  2  finalsel=2  miscclk=2    usually unconnected  formerly _EXT_xxxCLK
    [CLK_INPUT_MISCCLK2__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   2)
    },

    //  3  finalsel=2  miscclk=3    usually unconnected  formerly _XTAL16X
    [CLK_INPUT_MISCCLK3__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   3)
    },

    //  4  finalsel=2  miscclk=4    usually unconnected  formerly _XTAL8X
    [CLK_INPUT_MISCCLK4__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   4)
    },

    //  5  finalsel=2  miscclk=5    usually unconnected  formerly _GPCPLL_OP
    [CLK_INPUT_MISCCLK5__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   5)
    },

    //  6  finalsel=2  miscclk=6    usually unconnected  formerly _SYSPLL_OP
    [CLK_INPUT_MISCCLK6__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   6)
    },

    //  7  finalsel=2  miscclk=7    usually unconnected  formerly _XBARPLL_OP
    [CLK_INPUT_MISCCLK7__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_MISCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _MISCCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _MISCCLK,   7)
    },

    //  8  finalsel=0  slowclk=0    usually XTAL       often not used
    [CLK_INPUT_SLOWCLK0__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _SLOWCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _SLOWCLK,   0)
    },

    //  9  finalsel=0  slowclk=1    usually XTALS      not generally used
    [CLK_INPUT_SLOWCLK1__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _SLOWCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _SLOWCLK,   1)
    },

    //  a  finalsel=0  slowclk=2    usually unconnected  formerly _SWCLK
    [CLK_INPUT_SLOWCLK2__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _SLOWCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _SLOWCLK,   2)
    },

    //  b  finalsel=0  slowclk=3    usually XTAL4X     often not used
    [CLK_INPUT_SLOWCLK3__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _SLOWCLK)       |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _SLOWCLK,   3)
    },

    //  c  finalsel=3  onesrcclk=0  usually SPPLL0
    [CLK_INPUT_ONESRCCLK0__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _ONESRCCLK)     |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _ONESRCCLK, 0)
    },

    //  d  finalsel=3  onesrcclk=1  usually SPPLL1 or SYSPLL selected by DIV1 mux
    [CLK_INPUT_ONESRCCLK1__OSM] =
    {   DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK)              |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL)             |
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK),
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _NO)             |
        DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _ONESRCCLK)     |
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _ONESRCCLK, 1)
    },

    // 0d  finalsel=3  onesrcclk=1  onesrcclk1=0  usually SPPLL1
    [CLK_INPUT_ONESRCCLK1_0__OSM] =
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK1_SELECT),
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _ONESRCCLK1_SELECT, 0)
    },

    // 1d  finalsel=3  onesrcclk=1  onesrcclk1=1  usually SYSPLL
    [CLK_INPUT_ONESRCCLK1_1__OSM] =
    {
        DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK1_SELECT),
        DRF_NUM(_PTRIM, _SYS_CLK_SWITCH, _ONESRCCLK1_SELECT, 1)
    },
};


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

