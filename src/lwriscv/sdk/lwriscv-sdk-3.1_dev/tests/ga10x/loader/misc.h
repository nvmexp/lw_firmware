/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef MISC_H
#define MISC_H

#include <stdio.h>
#include <lwtypes.h>
#define DEBUG_PRINTS

#include "engine-io.h"

// DBG_PRINT is intended for detailed status messages useful for debugging.
// ERR_PRINT is for error messages relevant to the user.
#ifdef DEBUG_PRINTS
#define DBG_PRINT(...) printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define ERR_PRINT(...) printf(__VA_ARGS__)

void dump_hex(char *data, unsigned size, unsigned offs);
LwBool imageRead(const char *filename, void **buf, LwLength *pSize);
LwBool dumpDmesg(Engine *pEngine);

#endif // MISC_H
