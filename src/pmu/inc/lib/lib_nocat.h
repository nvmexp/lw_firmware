/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_nocat.h
 * @copydoc lib_nocat.c
 */

#ifndef LIB_NOCAT_H
#define LIB_NOCAT_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Public Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
LwU32       nocatBufferDmemPhysOffsetGet(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "nocatBufferDmemPhysOffsetGet");

void        nocatDiagBufferSet(LwU32 *pData, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "nocatDiagBufferSet");

FLCN_STATUS nocatDiagBufferLog(LwU32 *pData, LwU32 bufferSizeDWords, LwBool bRmAssert)
    GCC_ATTRIB_SECTION("imem_resident", "nocatDiagBufferLog");

#endif // LIB_SANDBAG_H
