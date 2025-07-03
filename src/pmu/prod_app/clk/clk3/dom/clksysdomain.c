/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 */

#include "clk3/dom/clksysdomain.h"

/*******************************************************************************
    Virtual Table
*******************************************************************************/

CLK_DEFINE_VTABLE_CLK3__DOMAIN(SysDomain);

/*
 * @brief       Configure the clksysdomain to produce the target frequency
 */
FLCN_STATUS
clkConfig_SysDomain
(
    ClkSysDomain_Object *this,
    const ClkTargetSignal *pTarget
)
{
    // TODO: Implement
    PMU_HALT_COND(!"TODO: Implement");
    return FLCN_ERR_NOT_SUPPORTED;
}


