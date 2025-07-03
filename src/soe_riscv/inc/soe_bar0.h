/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_BAR0_H
#define SOE_BAR0_H

#include "lwoslayer.h"
#include "engine.h"
#include "regmacros.h"

/*!
 * @file soe_bar0.h
 * This file holds the inline definitions of BAR0 register read/write functions.
 * This header should only be included by heavy secure microcode (besides being
 * included by the HAL implementations that implement register read/writes). The
 * reason for inlining is detailed below.
 */

#define BAR0_REG_RD32(addr)                    bar0Read(addr)
#define BAR0_REG_WR32(addr, data)              bar0Write(addr, data)

#endif // SOE_BAR0_H
