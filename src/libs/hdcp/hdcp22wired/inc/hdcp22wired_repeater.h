/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_REPEATER_H
#define HDCP22WIRED_REPEATER_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

#define HDCP22_SEQ_NUM_M_MAX                    (0xFFFFFF)
#define HDCP22_SEQ_NUM_V_MAX                    (0xFFFFFF)
#define HDCP22_RXINFO_DEVICE_COUNT_MASK         0xf0
#define HDCP22_RXINFO_MAX_DEV_EXCEED_MASK       0x08
#define HDCP22_RXINFO_MAX_DEPTH_EXCEED_MASK     0x04
#define HDCP22_RXINFO_HAS_1X_DEVICE_MASK        0x01
#define HDCP22_RXINFO_HAS_20_REPEATER_MASK      0x02

/* ------------------------ External Definitions --------------------------- */
FLCN_STATUS hdcp22HandleReadReceiverIdList(HDCP22_SESSION *pSession, LwU8 *pVBuffer, LwU32 vBufferSize)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22HandleReadReceiverIdList");
FLCN_STATUS hdcp22HandleVerifyM(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22HandleVerifyM");
FLCN_STATUS hdcp22WriteStreamManageMsg(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22WriteStreamManageMsg");
FLCN_STATUS hdcp22WriteReceiverIdAck(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredRepeater", "hdcp22WriteReceiverIdAck");
#endif //HDCP22WIRED_REPEATER_H
