// Copied from //sw/dev/gpu_drv/chips_a/tools/restricted/pmu/os/OpenRTOS/Source/portable/FALCON/string.h

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _STRING_H_
#define _STRING_H_

#include "stdint.h"
void *memcpy( void *s1, const void *s2, size_t n );
void *memmove(void *dest, const void *src, size_t n);
void *memset( void *ptr, int value, size_t num );
void *memcmp( void *s1, const void *s2, size_t n );
#endif // _STRING_H_

