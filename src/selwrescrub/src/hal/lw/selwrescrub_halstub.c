/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file selwrescrub_halstub.c
 */

#include <stddef.h>
#include "selwrescrubtypes.h"
#include "lwuproc.h"
#include "selwrescrubutils.h"
#include "rmselwrescrubif.h"
#ifndef BOOT_FROM_HS_BUILD
#include "selwrescrub_static_inline_functions.h"
#endif // BOOT_FROM_HS_BUILD
#include "config/g_hal_stubs.h"
