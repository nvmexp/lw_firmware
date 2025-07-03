/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hkdretval.h
 * @brief Defines various status codes that are colwenient to relay status
 *        information in functions.
 */

#ifndef HKD_RETVAL_H
#define HKD_RETVAL_H

#define HKD_PASS                       (0xDEADBEEF)
#define HKD_START                      (0xDEAD0001)
#define HKD_ERROR_UNKNOWN              (0xFFFFFFFF)
#define HKD_ERROR_SEB_CTRL_RD_FAILED   (0xFFFF0002)
#define HKD_ERROR_SEB_CTRL_WR_FAILED   (0xFFFF0003)
#define HKD_ERROR_SEB_KEY0_WR_FAILED   (0xFFFF0004)
#define HKD_ERROR_SEB_KEY1_WR_FAILED   (0xFFFF0005)
#define HKD_ERROR_CTRL_WRITE_ERROR     (0xFFFF0006)
#define HKD_ERROR_1X_KEY_NOT_READY     (0xFFFF0007)
#define HKD_ERROR_2X_KEY_NOT_READY     (0xFFFF0008)
#define HKD_ERROR_2X_ALREADY_LOADED    (0xFFFF0009)
#define HKD_ERROR_BAR0_RD_FAILED       (0xFFFF000A)
#define HKD_ERROR_HDCP_FUSE_DISABLED   (0xFFFF000B)
#define HKD_ERROR_CHIP_NOT_ALLOWED     (0xFFFF000C)
#define HKD_ERROR_CSB_RD_FAILED        (0xFFFF000D)
#define HKD_ERROR_CSB_WR_FAILED        (0XFFFF000E)
#endif // HKD_RETVAL_H
