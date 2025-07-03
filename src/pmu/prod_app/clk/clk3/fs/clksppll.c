/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */

/* ------------------------- System Includes -------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif

/* ------------------------- Application Includes --------------------------- */

#include "clk3/generic_dev_trim.h"
#include "clk3/fs/clksppll.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! The mux value map used for SPPLL0
const ClkFieldValue clkMuxValueMap_Sppll0[CLK_INPUT_COUNT__SPPLL]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_Sppll0") =
{
    [CLK_INPUT_VCO__SPPLL]       = CLK_DRF__FIELDVALUE(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL0, _VCO),       // 0  mode=0   VCO
    [CLK_INPUT_BYPASSCLK__SPPLL] = CLK_DRF__FIELDVALUE(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL0, _BYPASSCLK), // 1  mode=1   ONESOURCE
};

//! The mux value map used for SPPLL1
const ClkFieldValue clkMuxValueMap_Sppll1[CLK_INPUT_COUNT__SPPLL]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_Sppll1") =
{
    [CLK_INPUT_VCO__SPPLL]       = CLK_DRF__FIELDVALUE(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL1, _VCO),       // 0  mode=0   VCO
    [CLK_INPUT_BYPASSCLK__SPPLL] = CLK_DRF__FIELDVALUE(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL1, _BYPASSCLK), // 1  mode=1   ONESOURCE
};


/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */
