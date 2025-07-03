/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_BAR0_HS_H
#define SEC2_BAR0_HS_H

#include "lwoslayer.h"
#include "engine.h"

/*!
 * @file sec2_bar0_hs.h
 * This file holds the inline definitions of BAR0 register read/write functions.
 * This header should only be included by heavy secure microcode (besides being
 * included by the HAL implementations that implement register read/writes). The
 * reason for inlining is detailed below.
 */

#define BAR0_REG_RD32(addr)                    bar0Read(addr)
#define BAR0_REG_WR32(addr, data)              bar0Write(addr, data)

#endif // SEC2_BAR0_HS_H
