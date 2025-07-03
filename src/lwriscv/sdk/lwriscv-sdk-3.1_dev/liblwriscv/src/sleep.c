/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stddef.h>
#include <stdint.h>
#include <lwriscv/perfctr.h>

#include "liblwriscv/ptimer.h"

uint64_t delay(uint64_t cycles)
{
    uint64_t r = rdcycle() + cycles;
    uint64_t a = 0;

    while (a < r)
    {
        a = rdcycle();
    }

    return a - (r - cycles);
}

void usleep(uint64_t uSec)
{
    uint64_t r = ptimer_read() + (uSec * 1000);
    uint64_t a = 0;

    while (a < r)
    {
        a = ptimer_read();
    }
}
