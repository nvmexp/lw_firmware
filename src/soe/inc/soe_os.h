/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_OS_H
#define SOE_OS_H

/*!
 * @file    soe_os.h
 * @copydoc soe_os.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwostask.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
LwBool vApplicationDmaNackCheckAndClear(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationDmaNackCheckAndClear");
LwBool vApplicationIsDmaIdxPermitted(LwU8 dmaIdx)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationIsDmaIdxPermitted");
void vApplicationFlcnPrivLevelSet(LwU8, LwU8)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelSet");
void vApplicationFlcnPrivLevelReset(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationFlcnPrivLevelReset");
void osEncryptDmemInit(void)
    GCC_ATTRIB_SECTION("imem_init", "osEncryptDmemInit");

/* ------------------------ Defines ----------------------------------------- */

#endif // SOE_OS_H

