/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef INCLUDED_SSP_H
#define INCLUDED_SSP_H
extern LwU64 __stack_chk_guard;

__attribute__((__optimize__("no-stack-protector"))) __attribute__((noinline)) void __canary_setup(void);

__attribute__((noreturn)) __attribute__((noinline)) void __stack_chk_fail(void);
#endif
