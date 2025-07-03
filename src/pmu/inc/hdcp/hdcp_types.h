/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp_types.h
 * @brief  Header for 64 bit definition.
 *
 */

#ifndef HDCP_TYPES_H
#define HDCP_TYPES_H

// SRM field offsets and constants
#define LW_HDCPSRM_SRM_ID          0
#define LW_HDCPSRM_RESERVED1       1
#define LW_HDCPSRM_SRM_VERSION     2
#define LW_HDCPSRM_RESERVED2       4
#define LW_HDCPSRM_VRL_LENGTH      5
#define LW_HDCPSRM_VRL_LENGTH_HI   5
#define LW_HDCPSRM_VRL_LENGTH_MED  6
#define LW_HDCPSRM_VRL_LENGTH_LO   7
#define LW_HDCPSRM_NUM_DEVICES     8
#define LW_HDCPSRM_RESERVED3       0x80
#define LW_HDCPSRM_DEVICE_KSV_LIST 9
#define LW_HDCPSRM_SIGNATURE_SIZE  20
#define LW_HDCPSRM_MIN_LENGTH      48
#define LW_HDCP_SRM_MAX_ENTRIES    128

#define LW_HDCP_L_SIZE          20

#endif  // HDCP_TYPES_H
