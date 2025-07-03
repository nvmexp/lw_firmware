/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  seretval.h
 * @brief Defines various status codes that are colwenient to relay status
 *        information in functions.
 */

#ifndef SERETVAL_H
#define SERETVAL_H

#define SE_OK                           (0x00)
#define SE_ERROR                        (0xFF)

#define SE_ERR_NOT_SUPPORTED            (0x01)
#define SE_ERR_ILWALID_ARGUMENT         (0x02)
#define SE_ERR_ILLEGAL_OPERATION        (0x03)
#define SE_ERR_TIMEOUT                  (0x04)
#define SE_ERR_NO_FREE_MEM              (0x05)

#define SE_ERR_MUTEX_ACQUIRED           (0x10)
#define SE_ERR_MUTEX_ID_NOT_AVAILABLE   (0x11)

#define SE_ERR_MORE_PROCESSING_REQUIRED (0x30)

#endif // SERETVAL_H
