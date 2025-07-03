/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJSPDM_H
#define SEC2_OBJSPDM_H

/*!
 * @file sec2_objspdm.h
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "spdm/sec2_spdm.h"
#include "config/g_spdm_hal.h"

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_SPDM))
#include "lw_apm_spdm_common.h"
#endif

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
extern SPDM_HAL_IFACES SpdmHal;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
SPDM_HAL_IFACES SpdmHal;

#endif // SEC2_OBJSPDM_H
