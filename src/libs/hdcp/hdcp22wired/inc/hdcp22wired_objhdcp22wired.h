/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_OBJHDCP22WIRED_H
#define HDCP22WIRED_OBJHDCP22WIRED_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "config/g_hdcp22wired_hal.h"
#include "config/hdcp22wired-config.h"

/* ------------------------ Type Definitions -------------------------------- */
/*
 * HDCP22WIRED Object Definition
*/
typedef struct
{
    HDCP22WIRED_HAL_IFACES    hal;
} OBJHDCP22WIRED;

/* ------------------------ External Definitions ---------------------------- */
extern OBJHDCP22WIRED Hdcp22wired;

#endif // HDCP22WIRED_OBJHDCP22WIRED_H
