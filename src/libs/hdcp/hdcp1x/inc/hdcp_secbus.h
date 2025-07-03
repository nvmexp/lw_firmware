/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP_SECBUS_H
#define HDCP_SECBUS_H

/*!
 * @file    hdcp_secbus.h
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the FLCN to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

#include "lwtypes.h"
#include "dev_disp_seb.h"

#define LW_PDISP_SELWRE_HDCP_STATUS_SOR_MAX                8

#endif // HDCP_SECBUS_H
