/*
 * Copyright 2008-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

#include <string.h>

void *memcpy( void *s1, const void *s2, size_t n )
{
   size_t index;
   for (index = 0; index < n; index++)
   {
      ((char*)s1)[index] = ((char*)s2)[index];
   }
   return s1;
}

void *memset( void *s, int c, size_t n )
{
   char *buf = s;
   while (n--)
   {
      *buf++ = (char)c;
   }
   return s;
}

int memcmp( const void *s1, const void *s2, size_t n )
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
