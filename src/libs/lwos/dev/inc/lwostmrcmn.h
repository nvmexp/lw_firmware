/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSTMRCMN_H
#define LWOSTMRCMN_H

/*!
 * @file lwostmrmn.h
 * @brief      Common timer functionalities.
 *
 * @details    This file and lwostmrcmn.c provide the ability to compare two
 *             timestamps, both for temporal order and measure the time elapsed
 *             between two readings.
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "osptimer.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/*!
 * @brief      Compares two time stamps.
 * @copydoc    osTmrIsBefore
 */
#define OS_TMR_IS_BEFORE(_tA, _tB)    osTmrIsBefore((LwU32)(_tA), (LwU32)(_tB))

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
LwBool  osTmrIsBefore        (LwU32 ticksA, LwU32 ticksB);
LwS32   osTmrGetTicksDiff    (LwU32 ticksA, LwU32 ticksB)
     GCC_ATTRIB_NOINLINE();

LwBool  osTmrCondWaitNs(OS_PTIMER_COND_FUNC fp, void *pArgs, LwU32 spinDelayNs, LwU32 totalDelayNs);
void    osTmrWaitNs(LwU32 delayNs);

/* ------------------------ Inline Functions -------------------------------- */

#endif // LWOSTMRCMN_H
