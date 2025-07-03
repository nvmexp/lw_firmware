/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MAIN_H
#define MAIN_H

/*!
 * @file main.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "rmsoecmdif.h"
#include "soesw.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_soe-config_private.h"

/* ------------------------ Public Functions ------------------------------- */
int         main(int argc, char **ppArgv)
    GCC_ATTRIB_SECTION("imem_resident", "main");
void        InitSoeApp(void)
    GCC_ATTRIB_SECTION("imem_init", "InitSoeApp");
FLCN_STATUS InitSoeHS(void)
    GCC_ATTRIB_SECTION("imem_init", "InitSoeHS");

/* ------------------------ External definitions --------------------------- */

extern LwrtosQueueHandle SoeCmdMgmtCmdDispQueue;
extern LwrtosQueueHandle Disp2QWkrThd;
extern LwrtosQueueHandle Disp2QThermThd;

#endif // MAIN_H

