/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_STATUS_LW_H
#define LWRISCV_STATUS_LW_H

#include <lwstatus.h>

typedef LW_STATUS LWRV_STATUS;

// Error numbers are copied from lwstatuscodes.h
#define LWRV_OK                            LW_OK
#define LWRV_ERR_IN_USE                    LW_ERR_IN_USE
#define LWRV_ERR_INSUFFICIENT_RESOURCES    LW_ERR_INSUFFICIENT_RESOURCES
#define LWRV_ERR_INSUFFICIENT_PERMISSIONS  LW_ERR_INSUFFICIENT_PERMISSIONS
#define LWRV_ERR_ILWALID_ADDRESS           LW_ERR_ILWALID_ADDRESS
#define LWRV_ERR_ILWALID_ARGUMENT          LW_ERR_ILWALID_ARGUMENT
#define LWRV_ERR_ILWALID_BASE              LW_ERR_ILWALID_BASE
#define LWRV_ERR_ILWALID_DMA_SPECIFIER     LW_ERR_ILWALID_DMA_SPECIFIER
#define LWRV_ERR_ILWALID_FLAGS             LW_ERR_ILWALID_FLAGS
#define LWRV_ERR_ILWALID_INDEX             LW_ERR_ILWALID_INDEX
#define LWRV_ERR_ILWALID_LOCK_STATE        LW_ERR_ILWALID_LOCK_STATE
#define LWRV_ERR_ILWALID_OBJECT            LW_ERR_ILWALID_OBJECT
#define LWRV_ERR_ILWALID_OBJECT_BUFFER     LW_ERR_ILWALID_OBJECT_BUFFER
#define LWRV_ERR_ILWALID_OPERATION         LW_ERR_ILWALID_OPERATION
#define LWRV_ERR_ILWALID_POINTER           LW_ERR_ILWALID_POINTER
#define LWRV_ERR_ILWALID_REQUEST           LW_ERR_ILWALID_REQUEST
#define LWRV_ERR_ILWALID_STATE             LW_ERR_ILWALID_STATE
#define LWRV_ERR_NOT_READY                 LW_ERR_NOT_READY
#define LWRV_ERR_NOT_SUPPORTED             LW_ERR_NOT_SUPPORTED
#define LWRV_ERR_OUT_OF_RANGE              LW_ERR_OUT_OF_RANGE
#define LWRV_ERR_PROTECTION_FAULT          LW_ERR_PROTECTION_FAULT
#define LWRV_ERR_TIMEOUT                   LW_ERR_TIMEOUT
#define LWRV_ERR_FEATURE_NOT_ENABLED       LW_ERR_FEATURE_NOT_ENABLED
#define LWRV_ERR_GENERIC                   LW_ERR_GENERIC

// liblwrsicv-specific errors. Map to the closest one from lwstatuscodes.h
#define LWRV_ERR_DIO_DOC_CTRL_ERROR        LW_ERR_ILWALID_STATE
#define LWRV_ERR_DMA_NACK                  LW_ERR_DMA_IN_USE

// SHA driver specific errors.
#define LWRV_ERR_SHA_ENG_ERROR             LW_ERR_FATAL_ERROR
#define LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT     LW_ERR_TIMEOUT
#define LWRV_ERR_SHA_SW_RESET_TIMEOUT      LW_ERR_TIMEOUT
#define LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED  LW_ERR_IN_USE
#define LWRV_ERR_SHA_MUTEX_RELEASE_FAILED  LW_ERR_IN_USE

#define LWRV_WARN_NOTHING_TO_DO            LW_WARN_NOTHING_TO_DO
#endif // LWRISCV_STATUS_LW_H
