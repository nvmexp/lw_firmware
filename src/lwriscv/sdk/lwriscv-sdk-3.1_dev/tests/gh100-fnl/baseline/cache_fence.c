/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/cache.h>
#include <lwriscv/fence.h>

#include "tests.h"
#include "util.h"

uint64_t testCache(void)
{
    // Cache operations are tested by Alon's UTF
    return PASSING_DEBUG_VALUE;

}

uint64_t testFence(void)
{
    // Fence operations are covered by Alon's UTF
    return PASSING_DEBUG_VALUE;
}
