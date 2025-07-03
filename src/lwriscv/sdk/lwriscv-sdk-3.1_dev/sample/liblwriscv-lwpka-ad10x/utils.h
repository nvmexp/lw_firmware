/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UTILS_H
#define UTILS_H

#define TEST_MUTEX 1
#define DEBUG_ENABLE 0

#include <stdint.h>
#include <liblwriscv/shutdown.h>

#if(TEST_MUTEX)
    #define ACQUIRE_MUTEX()                 lwpka_acquire_mutex(engine)
    #define RELEASE_MUTEX()                 lwpka_release_mutex(engine)
#else
    #define ACQUIRE_MUTEX()                 NO_ERROR
    #define RELEASE_MUTEX()
#endif

#define CHECK_RET_AND_RELEASE_MUTEX_ON_FAILURE(retVal)  \
    if (retVal != NO_ERROR)                             \
    {                                                   \
        RELEASE_MUTEX();                                \
        riscvPanic();                                   \
    }

#define MIN(a,b)                            (((a) < (b)) ? (a) : (b))
#define MAX(a,b)                            (((a) > (b)) ? (a) : (b))

void    swapEndianness(void* pOutData, void* pInData, const uint32_t size);
uint8_t memCompare(void *a1, void *a2, uint32_t size);

#endif  // UTILS_H
