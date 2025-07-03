/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _FUBUTILS_H_
#define _FUBUTILS_H_

#include <lwtypes.h>
#include <lwmisc.h>
#include "lwctassert.h"

/********************************Macros****************************************/

//
// The new SVEC field definition is ((num_blks << 24) | (e << 17) | (s << 16) | tag_val)
// assuming size is always multiple of 256, then (size << 16) equals (num_blks << 24)
//
#define SVEC_REG(base, size, s, e)  (((base) >> 8) | (((size) | (s)) << 16) | ((e) << 17))

// GCC attributes
#define ATTR_OVLY(ov)          __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)    __attribute__ ((aligned(align)))

#define CT_ASSERT(cond)  ct_assert(cond)

// Infinite loop handler
#define INFINITE_LOOP()  while(LW_TRUE)


#define FAIL_FUB_WITH_ERROR(status)          do { \
    fubReportStatus_HAL(pFub, status);            \
    if (g_canFalcHalt)                            \
    {                                             \
        falc_halt();                              \
    }                                             \
    else                                          \
    {                                             \
        INFINITE_LOOP();                          \
    }                                             \
} while (0)


#endif //_FUBUTILS_H_
