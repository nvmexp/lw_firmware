
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include "lwtypes.h"

void *memcpy( void *s1, const void *s2, size_t n );
void *memset( void *s, int c, size_t n );
int memcmp( const void *s1, const void *s2, size_t n );

#endif // STRING_H
