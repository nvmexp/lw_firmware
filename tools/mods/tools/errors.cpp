/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "stdio.h"

#undef  DEFINE_RC
#define DEFINE_RC(errno, code, message) errno,
int errnos[] =
{
   0,
   #define DEFINE_RETURN_CODES
   #include "core/error/errors.h"
};

#undef  DEFINE_RC
#define DEFINE_RC(errno, code, message) message,
static const char * Messages[] =
{
   "OK",
   #define DEFINE_RETURN_CODES
   #include "core/error/errors.h"
};

int main
(
   int    argc,
   char * argv[]
)
{
   FILE * File = fopen("errors.txt", "w");
   if (!File)
   {
      printf("Failed to open %s.\n", argv[1]);
      return 1;
   }

   for (int i = 0; i < sizeof(errnos) / sizeof(int); ++i)
   {
      fprintf(File, "%d\t%s\n", errnos[i], Messages[i]);
   }

   return 0;
}
