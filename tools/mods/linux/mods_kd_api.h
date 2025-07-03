/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MODS_KD_API_H
#define INCLUDED_MODS_KD_API_H

#include "lwtypes.h"

/* In order to include <linux/types.h> necessary for user/kernel interface files
 * on ARM, it is required to define __EXPORTED_HEADERS__ in order to suppress
 * a warning about including linux kernel headers in a user space application */
#if defined(LWCPU_ARM) && !defined(__EXPORTED_HEADERS__)
    #define __EXPORTED_HEADERS__
    #define UNDEF_EXPORTED_HEADERS
#endif
#include "mods.h"
#ifdef UNDEF_EXPORTED_HEADERS
    #undef UNDEF_EXPORTED_HEADERS
    #undef __EXPORTED_HEADERS__
#endif

#endif /* INCLUDED_MODS_KD_API_H  */
