/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP_PVTBUS_H
#define HDCP_PVTBUS_H

/*!
 * @file    hdcp_pvtbus.h
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the FLCN to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

#include "lwtypes.h"

/*
 * The following values are not defined in the manuals, and we do not wish to
 * make these values readily available.
 */
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_REP                1:1
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_REP_ILWALID        0x00000000
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_REP_VALID          0x00000001
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_UNPROT             3:3
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_UNPROT_VALID       0x00000000
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_UNPROT_ILWALID     0x00000001
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_SCOPE              13:13
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_SCOPE_FAILED       0x00000000
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_SCOPE_PASSED       0x00000001
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MASK               31:31
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MASK_NOT_READY     0x00000000
#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MASK_READY         0x00000001

#define LW_PDISP_PRIVATE_HDCP_STATUS_SOR_MAX                8

#endif // HDCP_PVTBUS_H
