/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP_OBJHDCP_H
#define HDCP_OBJHDCP_H

/* ------------------------ Application includes --------------------------- */
#include "hdcp_cmn.h"
#include "config/g_hdcp_hal.h"
#include "config/hdcp-config.h"

/* ------------------------ External definitions --------------------------- */
/*
 * HDCP Object Definition
*/
typedef struct
{
    HDCP_HAL_IFACES    hal;
} OBJHDCP;

extern OBJHDCP Hdcp;

#endif // HDCP_OBJHDCP_H
