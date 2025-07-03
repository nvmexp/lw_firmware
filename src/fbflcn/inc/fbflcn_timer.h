/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_TIMER_H
#define FBFLCN_TIMER_H

/*!
 * @file    fbflcn_timer.h
 *          * To do - stefans: Short description about what this file is for*
 */

/* ------------------------ System includes --------------------------------- */
#include "fbflcn_defines.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------- Type definitions ------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
// Timer interrupt handler
LwU32 fbflcnTimerInterruptHandler(void);

#endif
