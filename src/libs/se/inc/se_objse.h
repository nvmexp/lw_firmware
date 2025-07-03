/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SE_OBJSE_H
#define SE_OBJSE_H

/* ------------------------ Application includes --------------------------- */
#include "lwuproc.h"
#include "setypes.h"
#include "config/g_se_hal.h"

/* ------------------------ External definitions --------------------------- */
/*
 * SE Object Definition
 */
typedef struct
{
    SE_HAL_IFACES    hal;
    LwU8             flcnDefaultPrivilegeLevel;
} OBJSE;

extern OBJSE Se;

#endif // SE_OBJSE_H
