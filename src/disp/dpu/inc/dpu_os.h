/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_OS_H
#define DPU_OS_H

/*!
 * @file  dpu_os.h
 * @copydoc dpu_os.c
 */

/* ------------------------ System includes -------------------------------- */
#include "lwrtos.h"
#include "lwostask.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void        vApplicationFlcnPrivLevelSet(LwU8, LwU8)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelSet");
void        vApplicationFlcnPrivLevelReset(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelReset");
#ifdef FLCNDBG_ENABLED
void        vApplicationIntOnBreak(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationIntOnBreak");
#endif // FLCNDBG_ENABLED
LwBool      vApplicationIsDmaIdxPermitted(LwU8)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationIsDmaIdxPermitted");
FLCN_STATUS vApplicationEnterHS(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "vApplicationEnterHS");
void        vApplicationExitHS(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "vApplicationExitHS");
FLCN_STATUS vApplicationRttimerSetup(LwBool, LwU32, LwBool)
    GCC_ATTRIB_SECTION("imem_testrttimer", "vApplicationRttimerSetup");

/* ------------------------ Misc macro definitions ------------------------- */
#endif // DPU_OS_H

