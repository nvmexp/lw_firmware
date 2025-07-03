/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_halstub.c
 */

#include <stddef.h>
#include "pmusw.h"
#if ((PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
#include "acr.h"
#include "g_acrlib_hal.h"
#else
#include "acr/pmu_acr.h"
#endif
#include "flcntypes.h"
#include "unit_api.h"
#include "lib/lib_pwm.h"
#include "pmu_objpg.h"
#include "pmu_timer.h"
#include "pmu_objfifo.h"
#include "pmgr/i2cdev.h"
#include "pmu_didle.h"
#include "pmu_objdi.h"
#include "pmu_obji2c.h"
#include "pmu_objspi.h"
#include "pmu_objvbios.h"
#include "pmu_objfb.h"
#include "objnne.h"
#include "gr/grtpc.h"
#include "config/g_hal_stubs.h"

