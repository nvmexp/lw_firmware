/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "debug.h"
#include "dev_riscv_pri.h"
#include "dev_se_seb.h"
#include <liblwriscv/io_dio.h>
#include <liblwriscv/io.h>
#include <lwpu-sdk/lwmisc.h>

#include "libRTS.h"

// FIXME: need addition of the follwing to to dev_se_seb.h
// LW_SSE_RTS_ERROR_CTRL_RW (this likely belongs in LW_SSE_RTS_ERROR_CTRL in one of bits 1:7 since these aren't mapped to any function in dev_se_seb.h)
// LW_SSE_RTS_ERROR_CTRL_RW_WRITE 1
// LW_SSE_RTS_ERROR_CTRL_RW_READ 0
#define RTS_ERROR_TYPE_WRITE 1
#define RTS_ERROR_TYPE_READ  0

#define DEBUG_RTS_ENABLE 0

#if(DEBUG_RTS_ENABLE)
    #define TRACE_RTS_NN(...)   TRACE_NN(__VA_ARGS__)
    #define TRACE_RTS(...)      TRACE(__VA_ARGS__)
    #define DUMP_MEM32_RTS(...) DUMP_MEM32(__VA_ARGS__)
#else
    #define TRACE_RTS_NN(...)
    #define TRACE_RTS(...)
    #define DUMP_MEM32_RTS(...)
#endif

#define CHECK_PARAMS(pError, pMeasurement, pMsrCnt, msrIdx) do { \
    status = checkArguments(pError, pMeasurement, pMsrCnt, msrIdx); \
    if(status != LWRV_OK) \
    { \
        return status; \
    } \
} while (0)

#define IS_PTR_SANE(ptr)  (((void*) ptr) != NULL)
#define IS_MSRIDX_SANE(msrIdx) (msrIdx < RTS_MAX_MSR_IDX)

// FALCON_PTIMER0 has 32 ns resolution, so we have to do some rounding for 1uS
// 1uS is actually 31.25, but rounding up to guarantee that 1uS has passed
#define RTS_TIME_1_MICROSECOND  ((LwU64) 32)
#define RTS_TIME_1_MILLISECOND  ((LwU64) 31250)
#define RTS_DIO_TIMEOUT         RTS_TIME_1_MILLISECOND

#define RTS_TIMEOUT_EXPIRED(startTime) rtsTimeoutHit(startTime, RTS_DIO_TIMEOUT)

// Ignore return from rtsClearPendingDIOError since we want to return the error
// that was thrown originally
#define RTS_ERROR_CHECK(rvalue, emsg...)                \
    { if (LWRV_OK != (rvalue)) { emsg; goto fail; } }

/** @brief Read DIO ERROR_CTRL register to clear any pending error.
 *
 * @note: If err->bValid is false (no error) then the other parameters in the struct are ignored.
 *
 * @param err: pointer to error struct
 *
 * @return LWRV_STATUS
 */
static LWRV_STATUS rtsClearPendingDIOError(RTS_ERROR* err, bool isWrite);

/** @brief Input argument sanity checker for index and pointers provided to public functions
 *
 * @param pRtsError: pointer to error struct
 * @param pMeasurement: pointer to buffer containing (or which is to contain) the 384b measurement
 * @param msrIdx: MSR index
 *
 * @return LWRV_STATUS
 */
static LWRV_STATUS checkArguments(RTS_ERROR *pRtsError, LwU32* pMeasurement, LwU32 msrIdx);

/** @brief Determine if a blocking process (dio read/write) has exceeded the max
 * allowed time
 *
 * @param startTime: time when blocking process started
 * @param timeout: max time to allow blocking process to run before returning an error
 *
 * @return LWRV_STATUS
 */
static bool rtsTimeoutHit(LwU64 startTime, LwU64 timeout);

/** @brief Get current system time
 *
 * @param None
 *
 * @return LwU64
 */
static LwU64 rtsGetLwrrentTime(void);


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

LWRV_STATUS
rtsReadMeasurement(
    RTS_ERROR *pRtsError,
    LwU32 *pMeasurement,
    LwU32 *pMsrCnt,
    LwU32 msrIdx,
    LwBool bReadShadow
)
{
    LWRV_STATUS status = LWRV_OK;
    LwU32 regVal;
    LwU32 i;
    LwU64 startTime;

    // 1. Read RTS_ERROR_CTRL_VALID. If error is present, update pRtsError with
    // error details and clear error.
    status = checkArguments(pRtsError, pMeasurement, msrIdx);
    if(!IS_PTR_SANE(pMsrCnt))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
    }    
    RTS_ERROR_CHECK(status);

    // Clear any pre-existing error (if any)
    rtsClearPendingDIOError(pRtsError, RTS_ERROR_TYPE_READ);

    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;

    // 2. Poll MRR_READ == 0
    startTime = rtsGetLwrrentTime();
    do
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MRR, &regVal);
        RTS_ERROR_CHECK(status);
    } while ((!RTS_TIMEOUT_EXPIRED(startTime)) && (!(FLD_TEST_DRF(_SSE, _RTS_MRR, _READ, _INIT, regVal))));

    // 3. Write MRR_INDEX = correct index, MRR_READ = 1, MRR_READ_SHADOW = 0 or 1
    regVal = 0;
    regVal = FLD_SET_DRF_NUM(_SSE, _RTS_MRR, _INDEX, msrIdx, regVal);
    regVal = FLD_SET_DRF(_SSE, _RTS_MRR, _READ, _TRIGGER, regVal);
    regVal = FLD_SET_DRF_NUM(_SSE, _RTS_MRR_, READ_SHADOW, bReadShadow, regVal);
    status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_RTS_MRR, &regVal);
    RTS_ERROR_CHECK(status);

    // 4. Poll MRR_READ == 0
    startTime = rtsGetLwrrentTime();
    do 
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MRR, &regVal);
        RTS_ERROR_CHECK(status);
    } while ((!RTS_TIMEOUT_EXPIRED(startTime)) && (!(FLD_TEST_DRF(_SSE, _RTS_MRR, _READ, _INIT, regVal))));
    
    // 5. Read MRV[0] ~ MRV[11], MSRCNT
    for (i = 0; i < RTS_MEASUREMENT_WORDS; i++)
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MRV(i), (pMeasurement + i));
        RTS_ERROR_CHECK(status);
    }

    status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MSRCNT, pMsrCnt);
    RTS_ERROR_CHECK(status);

    // i = index, s = shadow register, c = msr count
    TRACE_RTS_NN("[%c][i: %2d][s: %d][c: %3d]", 'R', msrIdx, bReadShadow, *pMsrCnt);
    DUMP_MEM32_RTS("", pMeasurement, RTS_MEASUREMENT_WORDS);
fail:
    if(LWRV_OK != status)
    {
        rtsClearPendingDIOError(pRtsError, RTS_ERROR_TYPE_READ);
    }
    return status;
}

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
LWRV_STATUS
rtsStoreMeasurement(
    RTS_ERROR *pRtsError,
    LwU32 *pMeasurement,
    LwU32 msrIdx,
    LwBool bFlushShadow,
    LwBool bWaitForComplete
)
{
    LWRV_STATUS status = LWRV_OK;
    LwU32 regVal;
    LwU32 i;
    LwU64 startTime;

    // 1. Read RTS_ERROR_CTRL_VALID. If error is present, update pRtsError with
    // error details and clear error.
    status = checkArguments(pRtsError, pMeasurement, msrIdx);
    RTS_ERROR_CHECK(status);

    // Clear any pre-existing error (if any)
    rtsClearPendingDIOError(pRtsError, RTS_ERROR_TYPE_WRITE);

    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;

    // STEP 1: Poll _STORE_MEASUREMENT == 0
    startTime = rtsGetLwrrentTime();
    do
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MCR, &regVal);
        RTS_ERROR_CHECK(status);
    } while ((!RTS_TIMEOUT_EXPIRED(startTime)) && (!(FLD_TEST_DRF(_SSE, _RTS_MCR, _STORE_MEASUREMENT, _INIT, regVal))));

    // STEP 2: Write MER[0..11]
    for (i = 0; i < RTS_MEASUREMENT_WORDS; i++)
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_RTS_MER(i), (pMeasurement + i));
        RTS_ERROR_CHECK(status);
    }

    // STEP 3: Write MCR
    regVal = 0;
    regVal = FLD_SET_DRF_NUM(_SSE, _RTS_MCR, _INDEX, msrIdx, regVal);
    regVal = FLD_SET_DRF(_SSE, _RTS_MCR, _STORE_MEASUREMENT, _TRIGGER, regVal);
    regVal = FLD_SET_DRF_NUM(_SSE, _RTS_MCR_, FLUSH_SHADOW, bFlushShadow, regVal);
    status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_RTS_MCR, &regVal);
    RTS_ERROR_CHECK(status);

    // STEP 4: Poll TRIGGER_STATUS == 0
    startTime = rtsGetLwrrentTime();
    do {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MCR, &regVal);
        RTS_ERROR_CHECK(status);
    } while ((!RTS_TIMEOUT_EXPIRED(startTime)) && (!(FLD_TEST_DRF(_SSE, _RTS_MCR, _TRIGGER_STATUS, _TRIGGER_SUCCEEDED, regVal))));

    // STEP 5: Poll _STORE_MEASUREMENT == 0
    if (bWaitForComplete == LW_TRUE)
    {
        startTime = rtsGetLwrrentTime();
        do 
        {
            status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_MCR, &regVal);
            RTS_ERROR_CHECK(status);
        } while ((!RTS_TIMEOUT_EXPIRED(startTime)) && (!(FLD_TEST_DRF(_SSE, _RTS_MCR, _STORE_MEASUREMENT, _INIT, regVal))));
    }

    // i = index, f = flush, b = blocking wait
    TRACE_RTS_NN("[%c][i: %2d][f: %d][b: %d]  ", 'W', msrIdx, bFlushShadow, bWaitForComplete);
    DUMP_MEM32_RTS("", pMeasurement, RTS_MEASUREMENT_WORDS);
fail:
    if(LWRV_OK != status)
    {
        rtsClearPendingDIOError(pRtsError, RTS_ERROR_TYPE_WRITE);
    }
    return status;
}

/** @brief Read DIO ERROR_CTRL register to clear any pending error.
 *
 * @note: If err->bValid is false (no error) then the other parameters in the
 * struct are ignored.
 *
 * @param err: pointer to error struct
 *
 * @return LWRV_STATUS
 */
static LWRV_STATUS rtsClearPendingDIOError(RTS_ERROR* err, bool isWrite)
{
    LWRV_STATUS status = LWRV_OK;
    LwU32 regVal = 0;
    DIO_PORT dioPort;

    // Read Error
    status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_ERROR_CTRL, &regVal);
    RTS_ERROR_CHECK(status);

    err->bValid = DRF_VAL(_SSE, _RTS_ERROR_CTRL, _VALID, regVal);
    if(err->bValid)
    {
        status = dioReadWrite(dioPort, DIO_OPERATION_RD, LW_SSE_RTS_ERROR_INFO, &err->errInfo);
        RTS_ERROR_CHECK(status);

        err->errAddr = DRF_VAL(_SSE, _RTS_ERROR_CTRL, _ADDR, regVal);
        err->srcType = DRF_VAL(_SSE, _RTS_ERROR_CTRL, _MASTER, regVal);
        err->errType = isWrite; // TODO: update to "DRF_VAL(_SSE, _RTS_ERROR_CTRL, _RW, regVal);" once comment above RTS_ERROR_TYPE_WRITE is resolved
        TRACE_RTS("errCtrl = %08X", regVal);
        TRACE_RTS("errInfo = %08X", err->errInfo);
        TRACE_RTS("errType = %08X", err->errType);
        TRACE_RTS("errAddr = %08X", err->errAddr);
        TRACE_RTS("srcType = %08X", err->srcType);
        TRACE_RTS("bValid  = %s", err->bValid ? "true" : "false");

        // Clear Error
        regVal = FLD_SET_DRF(_SSE, _RTS_ERROR_CTRL, _VALID, _CLEAR, regVal);
        status = dioReadWrite(dioPort, DIO_OPERATION_WR, LW_SSE_RTS_ERROR_CTRL, &regVal);
        RTS_ERROR_CHECK(status);
    }
fail:
    return status;
}

/** @brief Input argument sanity checker for index and pointers provided to
 * public functions
 *
 * @param pRtsError: pointer to error struct
 * @param pMeasurement: pointer to buffer containing (or which is to contain) 
 * the 384b measurement
 * @param msrIdx: MSR index
 *
 * @return LWRV_STATUS
 */
static LWRV_STATUS checkArguments(RTS_ERROR *pRtsError, LwU32* pMeasurement, LwU32 msrIdx)
{
    LWRV_STATUS ret = LWRV_OK;
    if( !IS_PTR_SANE(pMeasurement) || !IS_PTR_SANE(pRtsError) )
    {
        ret = LWRV_ERR_ILWALID_ARGUMENT;
    }
    else if(!IS_MSRIDX_SANE(msrIdx))
    {
        ret = LWRV_ERR_OUT_OF_RANGE;
    }
    return ret;
}

/** @brief Determine if a blocking process (dio read/write) has exceeded the max
 * allowed time
 *
 * @param startTime: time when blocking process started
 * @param timeout: max time to allow blocking process to run before returning an error
 *
 * @return LWRV_STATUS
 */
static bool rtsTimeoutHit(LwU64 startTime, LwU64 timeout)
{
    LwU64 lwrrentTime = rtsGetLwrrentTime();
    return (lwrrentTime - startTime) > timeout;
}

/*
 This is a rare case when input PTIMER[55:0] toggles in between two CSB
    reads. The sequence is:
        PTIMER_GRAY is such a value that PTIME0 == all 1s, PTIMER1 == 0x0;
        CSB reads PTIMER0 and gets PTMER0 == all 1s;
        PTIMER_GRAY changes and new value evaluates to be PTIMER0 == all
        0s, PTIIMER1 == 0x1;
        CSB reads PTIMER1 and gets PTIMER1 == 0x1;
    In order to solve the above problem, SW can use below atomic sequence
    to detect PTIMER0 overflow.
        Read PTIMER1, get timer1_val0;
        Read PTIMER0, get timer0_val0;
        Read PTIMER1 again, get timer1_val1;
        if( timer1_val1 == time1_val0)
            Final result is (time1_val0 <<32) + timer0_val0
        else if (timer1_val1 > timer1_val0)
            Read PTIMER0 again, get timer0_val1
            Final result is (time1_val1 <<32) + timer0_val1
        else if (timer1_val1 == 0)
            56bit timer has overflowed. Special containment is in SW.
*/

/** @brief Get current system time
 *
 * @param None
 *
 * @return LwU64
 */
static LwU64 rtsGetLwrrentTime(void)
{
    LwU64 lwrrentTime = 0;

    // Read PTIMER1, get timer1_val0;
    LwU32 lwrrentTimeMSB = localRead(LW_PRGNLCL_FALCON_PTIMER1);

    // Read PTIMER0, get timer0_val0;
    LwU32 lwrrentTimeLSB = localRead(LW_PRGNLCL_FALCON_PTIMER0);

    // Read PTIMER1 again, get timer1_val1;
    LwU32 msbCheck = localRead(LW_PRGNLCL_FALCON_PTIMER1);

    if( lwrrentTimeMSB == msbCheck)
    {
        lwrrentTime = (LwU64)lwrrentTimeMSB;
        lwrrentTime <<= 32;
        lwrrentTime |= lwrrentTimeLSB;
    }
    else if (msbCheck > lwrrentTimeMSB)
    {
        // Read PTIMER0 again, get timer0_val1
        // Final result is (time1_val1 <<32) + timer0_val1
        lwrrentTimeLSB = localRead(LW_PRGNLCL_FALCON_PTIMER0);
        lwrrentTime = (LwU64)msbCheck;
        lwrrentTime <<= 32;
        lwrrentTime |= lwrrentTimeLSB;
    }
    else if (msbCheck == 0)
    {
        // 56bit timer has overflowed.
        lwrrentTime = 0;
    }
    return lwrrentTime;
}
