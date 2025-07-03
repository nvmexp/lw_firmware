/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#include <sdk/lwpu/inc/lwtypes.h>

#include "libos.h"

#define LIBOS_VALIDATE(x)                                                                                    \
    if (!(x))                                                                                                \
    {                                                                                                        \
        libosTaskPanic(LIBOS_TASK_PANIC_REASON_ASSERT_FAILED);                                               \
    }

#ifdef _DEBUG
#    define LIBOS_ASSERT LIBOS_VALIDATE
#else
#    define LIBOS_ASSERT(x)
#endif
