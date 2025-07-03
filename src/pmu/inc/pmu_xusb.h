/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _PMU_XUSB_H
#define _PMU_XUSB_H

/*!
 * @file   pmu_xusb.h
 * @brief  Contains definitions for XUSB<->PMU message box content 
 */

/*!
 * @brief XUSB to PMU message box interface
 *
 * LW_XUSB2PMU_MSGBOX_CMD_*      indicates a message from XUSB to PMU
 *     _TYPE_PCIE_REQ
 *          used by XUSB to request PMU to unlock or lock to max genspeed 
 *     _PAYLOAD_PCIE_REQ_UNLOCK
 *     _PAYLOAD_PCIE_REQ_LOCK
 *          to be used with _TYPE_PCIE_REQ to request genspeed lock/unlock
 *
 * LW_XUSB2PMU_RDAT_REPLY_*      indicates a reply from PMU to XUSB in response to above message
 *     _TYPE_PMU_ACTIVE
 *          generic reply to XUSB to indicate that message/request was received by PMU
 */
#define LW_XUSB2PMU_MSGBOX_CMD_FULL                                         7:0
#define LW_XUSB2PMU_MSGBOX_CMD_TYPE                                         2:0
#define LW_XUSB2PMU_MSGBOX_CMD_TYPE_INIT                             0x00000000
#define LW_XUSB2PMU_MSGBOX_CMD_TYPE_PCIE_REQ                         0x00000001
#define LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD                                      7:3
#define LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD_INIT                          0x00000000
#define LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD_PCIE_REQ_UNLOCK               0x00000000
#define LW_XUSB2PMU_MSGBOX_CMD_PAYLOAD_PCIE_REQ_LOCK                 0x00000001

#define LW_XUSB2PMU_RDAT_REPLY_FULL                                         7:0
#define LW_XUSB2PMU_RDAT_REPLY_TYPE                                         2:0
#define LW_XUSB2PMU_RDAT_REPLY_TYPE_INIT                             0x00000000
#define LW_XUSB2PMU_RDAT_REPLY_TYPE_GENERIC_ACK                      0x00000001
#define LW_XUSB2PMU_RDAT_REPLY_PAYLOAD                                      7:3
#define LW_XUSB2PMU_RDAT_REPLY_PAYLOAD_INIT                          0x00000000

/*!
 * @brief PMU to XUSB message box interface
 *
 * LW_PMU2XUSB_MSGBOX_CMD_*      indicates a message from PMU to XUSB
 *     _TYPE_ISOCH_QUERY
 *          used by PMU to request current isoch traffic activity status from XUSB  
 *     _TYPE_PCIE_REQ_STATUS
 *          used by PMU to indicate the success status of PCIE request to XUSB 
 *     _PAYLOAD_PCIE_REQ_STATUS_FAIL
 *     _PAYLOAD_PCIE_REQ_STATUS_PASS
 *          to be used with _TYPE_PCIE_REQ_STATUS to indicate success/failure of request completion
 *
 * LW_PMU2XUSB_RDAT_REPLY_*      indicates a reply from XUSB to PMU in response to above message
 *     _TYPE_XUSB_ACTIVE
 *          generic reply to PMU to indicate that message/request was received by XUSB
 *     _TYPE_ISOCH_QUERY
 *          used by XUSB to indicate that reply is in response to isoch traffic query message from PMU
 *     _PAYLOAD_ISOCH_QUERY_IDLE
 *     _PAYLOAD_ISOCH_QUERY_ACTIVE  
 *          to be used with _TYPE_ISOCH_QUERY to indicate isoch traffic status on XUSB
 */
#define LW_PMU2XUSB_MSGBOX_CMD_FULL                                         7:0
#define LW_PMU2XUSB_MSGBOX_CMD_TYPE                                         2:0
#define LW_PMU2XUSB_MSGBOX_CMD_TYPE_INIT                             0x00000000
#define LW_PMU2XUSB_MSGBOX_CMD_TYPE_ISOCH_QUERY                      0x00000001
#define LW_PMU2XUSB_MSGBOX_CMD_TYPE_PCIE_REQ_STATUS                  0x00000002
#define LW_PMU2XUSB_MSGBOX_CMD_PAYLOAD                                      7:3
#define LW_PMU2XUSB_MSGBOX_CMD_PAYLOAD_INIT                          0x00000000
#define LW_PMU2XUSB_MSGBOX_CMD_PAYLOAD_PCIE_REQ_STATUS_PASS          0x00000001
#define LW_PMU2XUSB_MSGBOX_CMD_PAYLOAD_PCIE_REQ_STATUS_FAIL          0x00000000

#define LW_PMU2XUSB_RDAT_REPLY_FULL                                         7:0
#define LW_PMU2XUSB_RDAT_REPLY_TYPE                                         2:0
#define LW_PMU2XUSB_RDAT_REPLY_TYPE_INIT                             0x00000000
#define LW_PMU2XUSB_RDAT_REPLY_TYPE_GENERIC_ACK                      0x00000001
#define LW_PMU2XUSB_RDAT_REPLY_TYPE_ISOCH_QUERY                      0x00000002
#define LW_PMU2XUSB_RDAT_REPLY_PAYLOAD                                      7:3
#define LW_PMU2XUSB_RDAT_REPLY_PAYLOAD_INIT                          0x00000000
#define LW_PMU2XUSB_RDAT_REPLY_PAYLOAD_ISOCH_QUERY_IDLE              0x00000000
#define LW_PMU2XUSB_RDAT_REPLY_PAYLOAD_ISOCH_QUERY_ACTIVE            0x00000001

#endif // _PMU_XUSB_H
