/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/status-lw.h"

#define RTS_MAX_MSR_IDX         ((LwU32) 64)
#define RTS_SHA_384_BITS        ((LwU32) 384)
#define RTS_MEASUREMENT_BITS    ((LwU32) RTS_SHA_384_BITS) // using SHA-384; there is consideration to switch to 512 in the future
#define RTS_MEASUREMENT_BYTES   ((LwU32) RTS_MEASUREMENT_BITS >> 3)
#define RTS_MEASUREMENT_WORDS   ((LwU32) RTS_MEASUREMENT_BYTES >> 2)

/* 
 * errInfo is the value of RTS_ERROR_INFO register
 * errType is type of error (read/write). Reg field RTS_ERROR_CTRL_RW.
 * errAddr indicates the address at which the error happened. Reg field RTS_ERROR_CTRL_ADDR.
 * srcType is the source engine which caused RTS error. Reg field RTS_ERROR_CTRL_MASTER
 * bValid is a boolean which reads TRUE if valid error info is captured (RTS_ERROR_CTRL_VALID) and FALSE otherwise.
 */
typedef struct {
    LwU32   errInfo;
    LwU32   errType;
    LwU32   errAddr;
    LwU32   srcType;
    LwBool  bValid;
} RTS_ERROR;

/** @brief Read Stored Measurement
 *
 * @param pRtsError: pointer to RTS_ERROR which gives details of the error recorded by RTS.
 * @param pMeasurement: pointer to measurement LwU32 array
 * @param pMsrcnt: pointer to number of measurements extended since last flush (or power on).
 * @param msrIdx: index of MSR register from where the measurement is to be read
 * @param bReadShadow: defines whether to read from MSR or MSRS reg (TRUE = MSRS, FALSE = MSR)
 *
 * @return LWRV_STATUS
 */
LWRV_STATUS rtsReadMeasurement
(
    RTS_ERROR *pRtsError,
    LwU32     *pMeasurement,
    LwU32     *pMsrcnt,
    LwU32      msrIdx,
    LwBool     bReadShadow
);

/** @brief Store Measurement
 *
 * @param pRtsError: pointer to RTS_ERROR which gives details of the error recorded by RTS.
 * @param pMeasurement: pointer to measurement LwU32 array
 * @param msrIdx: index of MSR register to update
 * @param bFlushShadow: defines whether to flush MSRS (TRUE = Flush, FALSE = Extend)
 * @param bWaitForComplete: defines whether to wait for store to complete (TRUE = Blocking, FALSE = Non-Blocking)
 *
 * @return LWRV_STATUS
 */
LWRV_STATUS rtsStoreMeasurement
(
    RTS_ERROR *pRtsError,
    LwU32     *pMeasurement,
    LwU32      msrIdx,
    LwBool     bFlushShadow,
    LwBool     bWaitForComplete
);
