/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: main.c : NS part of PUB in C, Will be moved to SPARK in coming instances
 */

//
// Includes
//
#include "pub.h"

// variable to store canary for SSP
void * __stack_chk_guard;

//
// Setup canary for SSP
// Here we are initializing NS canary to random generated number in NS
//
GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_NOINLINE()
void __canary_setup(void)
{
    __stack_chk_guard = (void *)RANDOM_CANARY_NS;
}
