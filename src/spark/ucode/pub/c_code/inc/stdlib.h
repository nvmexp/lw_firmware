/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_


// Falcon instrinsics file provided by falcon tool chain.
#include <falcon-intrinsics.h>

// Falcon intrinsics file containing functions not provided by falcon tool chain.
#include "falcon-intrinsics-for-ccg.h"
#include "pub.h"
#include "pubutils.h"

// Defining our own custom Last chance handler.
#define GNAT_LAST_CHANCE_HANDLER(a, b) __lwstom_gnat_last_chance_handler()
extern void __lwstom_gnat_last_chance_handler(void) ATTR_OVLY(".imem_pub");

#endif // STDLIB_H
