/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifdef _WIN32
// disable warning C4786: identifier was truncated to '255' characters in the debug information
#pragma warning (disable : 4786)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifndef _WIN32
#include <unistd.h>
#include <string.h>
#endif
#include <iostream>
#include <string>
#include <vector>

#include "mdiag/sysspec.h"
#include "core/include/cmdline.h"
