/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifdef LWRISCV_SDK
#include <libc/string.h>
#else /* LWRISCV_SDK */

#ifndef SHARED_H
#define SHARED_H
#include <stddef.h>
#include <stdarg.h>

/*
 * Forward declarations for those that don't want to pull std headers (kernel).
 * MK TODO: This doesn't belong to port. It should be moved to LWOS once it
 *          is supported.
 */

void *memcpy( void *s1, const void *s2, size_t n );
void *memset( void *ptr, int value, size_t num );
int   memcmp( const void *s1, const void *s2, size_t n );

#endif /* SHARED_H */
#endif /* LWRISCV_SDK */
