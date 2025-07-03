/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_OS_H
#define SEC2_OS_H

/*!
 * @file    sec2_os.h
 * @copydoc sec2_os.c
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
void vApplicationIsLsOrHsModeSet_hs(LwBool *pBLsModeSet, LwBool *pBHsModeSet)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "vApplicationIsLsOrHsModeSet_hs");
FLCN_STATUS sec2DmemCrypt(LwU8 *pIv, LwU8 *ptr, LwU32 size, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_dmemCryptHs", "start");

/* ------------------------ Defines ----------------------------------------- */

#endif // SEC2_OS_H

