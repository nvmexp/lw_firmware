/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OBJPR_H
#define SEC2_OBJPR_H

/*!
 * @file sec2_objpr.h
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "pr/sec2_pr.h"
#include "pr/pr_common.h"

#include "config/g_pr_hal.h"
#include "config/sec2-config.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern PR_HAL_IFACES PrHal;

/* ------------------------ Function Prototypes ---------------------------- */
void prGetDeviceCertTemplate(LwU8 **ppCert, LwU32 *pCertSize, void *pDevCertIndices);

#endif // SEC2_OBJPR_H
