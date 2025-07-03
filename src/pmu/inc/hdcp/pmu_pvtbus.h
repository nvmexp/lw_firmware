/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_PVTBUS_H
#define PMU_PVTBUS_H

/*!
 * @file    pmu_pvtbus.h
 * @brief   Provides access to the display private data bus.
 *
 * The bus allows the DPU/PMU to selwrely access some of the private values
 * from the display. These values are required during the HDCP authentication
 * process.
 */

#include "lwtypes.h"

/*! @brief Bus error code */
typedef LwU8 BUS_STATUS;

/*! @brief Successful operation */
#define BUS_SUCCESS         0x00

/*! @brief Read error */
#define BUS_READ_ERROR      0x11

/*! @brief Write error */
#define BUS_WRITE_ERROR     0x12

/*! @brief Timeout error */
#define BUS_TIMEOUT_ERROR   0x13


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


/* Function Prototypes */
BUS_STATUS hdcpReadM0(LwU64*);
BUS_STATUS hdcpReadStatus(LwU32, LwU32*);
BUS_STATUS hdcpWriteStatus(LwU32, LwU32);

#endif // PMU_PVTBUS_H
