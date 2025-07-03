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

/* main.h */

#include <sys/param.h>

#define DEBUG           /* Log actions to stderr */

#if defined(DEBUG)
# define IFDEBUG(code)          code
#else
# define IFDEBUG(code)          /* no-op */
#endif
