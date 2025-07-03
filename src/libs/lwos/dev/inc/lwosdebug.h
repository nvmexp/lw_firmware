/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSDEBUG_H
#define LWOSDEBUG_H

/*!
 * @file    lwosdebug.h
 * @copydoc lwosdebug.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application includes --------------------------- */
#ifndef EXCLUDE_LWOSDEBUG
#include "lwoslayer.h"
#endif // EXCLUDE_LWOSDEBUG

#define FLCN_DEBUG_DEFAULT_VALUE   0xfeedfeed
#define FLCN_DEBUG_DEFAULT_TIMEOUT 1000U        /* Ns */

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#ifndef EXCLUDE_LWOSDEBUG
void osDebugISRStackInit(void)
    GCC_ATTRIB_SECTION("imem_init", "osDebugISRStackInit");
void osDebugESRStackInit(void)
    GCC_ATTRIB_SECTION("imem_init", "osDebugESRStackInit");
void osDebugStackPreFill(LwUPtr *pxStack, LwU16 usStackDepth)
    GCC_ATTRIB_SECTION("imem_init", "osDebugStackPreFill");
void osDebugISRStackOverflowDetect(void)
    GCC_ATTRIB_SECTION("imem_resident", "osDebugISRStackOverflowDetect");
void osDebugESRStackOverflowDetect(void)
    GCC_ATTRIB_SECTION("imem_resident", "osDebugESRStackOverflowDetect");

/* ------------------------- External definitions --------------------------- */
extern RM_RTOS_DEBUG_ENTRY_POINT OsDebugEntryPoint;
#endif // EXCLUDE_LWOSDEBUG
#endif // LWOSDEBUG_H

