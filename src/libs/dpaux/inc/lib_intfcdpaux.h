/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_intfcdpaux.h
 */

#ifndef LIB_INTFCDPAUX_H
#define LIB_INTFCDPAUX_H

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include <string.h>

/* ------------------------ Application includes --------------------------- */
#include "lib_dpaux.h"
/* ------------------------- Macros and Defines ---------------------------- */
/* ------------------------ External Functions ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
FLCN_STATUS dpauxAccess(DPAUX_CMD *pCmd, LwBool bIsReadAccess)
         GCC_ATTRIB_SECTION("imem_libDpaux", "dpauxAccess");

FLCN_STATUS dpauxChSetOwnership(LwU8 port, LwU32 newOwner)
         GCC_ATTRIB_SECTION("imem_libDpaux", "dpauxChSetOwnership");
#endif // LIB_INTFCDPAUX_H
