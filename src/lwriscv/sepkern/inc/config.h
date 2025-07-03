/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __SEPKERN_CONFIG_H_
#define __SEPKERN_CONFIG_H_

#define HALT_ON_SBI_FAILURE 0

// WAR for bug 2789594 - HW now thinks BTB provides marginal benefit.
#define BTB_DISABLE 0

// See https://confluence.lwpu.com/display/LW/LWWATCH+Debugging+and+Security+-+GA10X+POR
// 0 = locked down                          "Prod HS"
// 1 = "prod" (useful but minimized set)    "Prod LS"
// 2 = "debug" (maximum privilege)          "Debug"
#if DEBUG_BUILD
#define ICD_MODE 2
#else
#define ICD_MODE 0
#endif

// Constants
#define SBI_VERSION 0x00000000 /* TODO: What should this be? */
#define STACK_SIZE 0x300
// RV64
#define XREGSIZE 8

// Include manuals
#include "riscv_manuals.h"

#endif // __SEPKERN_CONFIG_H_
