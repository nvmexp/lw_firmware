/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SSP_H
#define SSP_H

#include <lwtypes.h>

__attribute__((section(".stack_chk_guard"))) extern void *__stack_chk_guard;

#define SETUP_STACK_CANARY(value) (__stack_chk_guard = (void *) (value))

#endif // SSP_H
