/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdarg.h>

#include "shared.h"
#include "sections.h"

/*
 * WARNING
 * This code is shared between tasks and kernel (and placed in IMEM).
 * Do not bloat it and do not create any global variables as by default
 * they may be not accessible to either kernel or userspace.
 *
 * This file is to be moved in LWOS in future, once its ready.
 */

sysSHARED_CODE void *memcpy( void *s1, const void *s2, size_t n )
{
   size_t index;
   for (index = 0; index < n; index++)
   {
      ((char*)s1)[index] = ((char*)s2)[index];
   }
   return s1;
}

sysSHARED_CODE void *memset( void *s, int c, size_t n )
{
   char *buf = s;
   while (n--)
   {
      *buf++ = (char)c;
   }
   return s;
}

sysSHARED_CODE int memcmp( const void *s1, const void *s2, size_t n )
{
    size_t index;
    for (index = 0; index < n; index++)
    {
        if ( ((unsigned char*)s1)[index] != ((unsigned char*)s2)[index] )
        {
            return (((unsigned char*)s1)[index] - ((unsigned char*)s2)[index]);
        }
    }
    return 0;
}
