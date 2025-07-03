/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file soe_halstub.c
 */

#include <stddef.h>
#include "soesw.h"
#include "flcntypes.h"
#include "rmsoecmdif.h"
#include "soe_rttimer.h"
#include "unit_dispatch.h"
#include "soe_timer.h"
#include "soe_lwlink.h"
#include "spidev.h"
#include "tsec_drv.h"
#include "config/g_hal_stubs.h"
