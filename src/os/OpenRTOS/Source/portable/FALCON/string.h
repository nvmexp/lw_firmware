/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

void *memcpy( void *s1, const void *s2, size_t n );
void *memset( void *ptr, int value, size_t num );
int   memcmp( const void *s1, const void *s2, size_t n );

#endif // _STRING_H_

