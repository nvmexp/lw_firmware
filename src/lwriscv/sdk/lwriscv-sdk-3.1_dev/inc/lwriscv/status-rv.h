/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_STATUS_RV_H
#define LWRISCV_STATUS_RV_H

#include <stdint.h>

typedef uint32_t LWRV_STATUS;

// Error numbers are copied from lwstatuscodes.h. New errors should be
// copied from there with matching numbers, or if they are lwriscv-specific,
// they should go into the 0x100+ range
#define LWRV_OK                            0
#define LWRV_ERR_IN_USE                    0x00000017
#define LWRV_ERR_INSUFFICIENT_RESOURCES    0x0000001A
#define LWRV_ERR_INSUFFICIENT_PERMISSIONS  0x0000001B
#define LWRV_ERR_ILWALID_ADDRESS           0x0000001E
#define LWRV_ERR_ILWALID_ARGUMENT          0x0000001F
#define LWRV_ERR_ILWALID_BASE              0x00000020
#define LWRV_ERR_ILWALID_DMA_SPECIFIER     0x00000027
#define LWRV_ERR_ILWALID_FLAGS             0x00000029
#define LWRV_ERR_ILWALID_INDEX             0x0000002C
#define LWRV_ERR_ILWALID_LOCK_STATE        0x0000002F
#define LWRV_ERR_ILWALID_OBJECT            0x00000031
#define LWRV_ERR_ILWALID_OBJECT_BUFFER     0x00000032
#define LWRV_ERR_ILWALID_OPERATION         0x00000038
#define LWRV_ERR_ILWALID_POINTER           0x0000003D
#define LWRV_ERR_ILWALID_REQUEST           0x0000003F
#define LWRV_ERR_ILWALID_STATE             0x00000040
#define LWRV_ERR_NOT_READY                 0x00000055
#define LWRV_ERR_NOT_SUPPORTED             0x00000056
#define LWRV_ERR_OUT_OF_RANGE              0x0000005B
#define LWRV_ERR_PROTECTION_FAULT          0x0000005F
#define LWRV_ERR_TIMEOUT                   0x00000065
#define LWRV_ERR_FEATURE_NOT_ENABLED       0x0000006D
#define LWRV_ERR_GENERIC                   0x0000FFFF
// Add more errors here as required

// lwriscv-specific error codes go in the 0x100+ range.
#define LWRV_ERR_DIO_DOC_CTRL_ERROR        0x00000100
#define LWRV_ERR_DMA_NACK                  0x00000101

// SHA driver specific errors.
#define LWRV_ERR_SHA_ENG_ERROR             0x00000200
#define LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT     0x00000201
#define LWRV_ERR_SHA_SW_RESET_TIMEOUT      0x00000202
#define LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED  0x00000203
#define LWRV_ERR_SHA_MUTEX_RELEASE_FAILED  0x00000204

#define LWRV_WARN_NOTHING_TO_DO            0x00010006
// Add more warnings here as required

#endif // LWRISCV_STATUS_RV_H
