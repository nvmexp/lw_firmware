/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_DUMMY_MCLK_SWITCH_H
#define FBFLCN_DUMMY_MCLK_SWITCH_H


#include "fbflcn_defines.h"
#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"

LwU32 doDummyMclkSwitch(LwU16 targetFreqMHz);

#endif
