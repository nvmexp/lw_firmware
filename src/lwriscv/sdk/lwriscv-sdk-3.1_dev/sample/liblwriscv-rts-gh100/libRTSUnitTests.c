#include "libRTS.h"
#include "debug.h"

#define DEBUG_TEST_ENABLE 0

#if(DEBUG_TEST_ENABLE)
    #define TRACE_TEST_NN   TRACE_NN
    #define TRACE_TEST      TRACE
    #define DUMP_MEM32_TEST DUMP_MEM32
#else
    #define TRACE_TEST_NN(...)
    #define TRACE_TEST(...)
    #define DUMP_MEM32_TEST(...)
#endif

// TODO: make sure that when running these tests you are OK to overwrite/extend
// the measurements here. MSR allocation is defined in "hw_rts_msr_usage.h".
// We're using the 2 highest indices (which are lwrrently otherwise unallocated).
#define RTS_MIN_TEST_IDX    ((RTS_MAX_MSR_IDX)-2)
#define RTS_MAX_TEST_IDX    RTS_MAX_MSR_IDX // exclusive

#define READ_SHADOW         LW_TRUE
#define FLUSH_SHADOW        LW_TRUE
#define WAIT_COMPLETE       LW_TRUE

#define RTS_IDENTICAL_MSR(buf1, buf2) (memcmp(((uint8_t*) (buf1)), ((uint8_t*) (buf2)), RTS_MEASUREMENT_BYTES) == 0)

#define RTS_THROW_ERROR(ret, ...) do { \
    status = ret; \
    printf(__VA_ARGS__); \
    printf("\n"); \
    goto fail; \
} while(0)

#define RTS_ERR_CHECK(ret) do { \
    if(ret != LWRV_OK) { \
        goto fail; \
    } \
} while(0)


#define RTS_ERR_HANDLE(ret, expected, ...) do { \
    if(ret == expected) \
    { \
        printf("passed: %s\n", __func__); \
    } \
    else \
    { \
        printf("failed: %s; err = %d, expected = %d\n", __func__, ret, expected); \
    } \
} while(0)


#define READ(pErr, buf, pMsrCnt, idx, bReadShadow) do { \
    status = rtsReadMeasurement(pErr, buf, pMsrCnt, idx, bReadShadow); \
    RTS_ERR_CHECK(status); \
} while(0)

#define STORE(pErr, buf, idx, bFlushShadow) do { \
    status = rtsStoreMeasurement(pErr, buf, idx, bFlushShadow, WAIT_COMPLETE); \
    RTS_ERR_CHECK(status); \
} while(0)


static void simpleTest(void);
static void dumpAllMSRTest(void);
static void badIndexTest(void);
static void extendTest(void);
static void flushTest(void);


void libRTS_RunUnitTests(void)
{
    simpleTest();
    badIndexTest();
    extendTest();
    flushTest();
    dumpAllMSRTest();
}


static void simpleTest(void)
{
    LWRV_STATUS status = LWRV_OK;
    RTS_ERROR error;

    LwU32 inMeasurement[RTS_MEASUREMENT_WORDS] = {0x12345678, 0xaa, 0x9900, 0x880000, 0x77000000, 0x11, 0x2200, 0x330000, 0x44000000, 0x55, 0x66, 0x7700};

    LwU32 outMeasurement[RTS_MEASUREMENT_WORDS] = {0};

    LwU32 msrCnt = 0;

    // Write 1
    STORE(&error, inMeasurement, RTS_MIN_TEST_IDX, FLUSH_SHADOW);

    // Read MSR
    READ(&error, outMeasurement, &msrCnt, RTS_MIN_TEST_IDX, !READ_SHADOW);
    
    // Read MSRS
    READ(&error, outMeasurement, &msrCnt, RTS_MIN_TEST_IDX, READ_SHADOW);

fail:
    RTS_ERR_HANDLE(status, LWRV_OK);
    return;
}

/*
    RTS-REQ-3: Read measurement - Provide interface through which stored measurement from a particular MSR/MSRS index along with corresponding extension counter can be read out.
*/
static void dumpAllMSRTest(void)
{
    LWRV_STATUS status = LWRV_OK;
    RTS_ERROR error;
    LwU32 msrCnt = 0;
    LwU32 MSR[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 empty[RTS_MEASUREMENT_WORDS] = {0};

    // Empty (Zeroed) MSRs are skipped
    for(LwU32 i = RTS_MIN_TEST_IDX; i < RTS_MAX_TEST_IDX; i++)
    {
        READ(&error, MSR, &msrCnt, i, !READ_SHADOW);

        if(RTS_IDENTICAL_MSR(MSR, empty))
        {
            // no need to print or read MSRS for this since all will be 0s
            continue;
        }

        TRACE_TEST("[%d]:", i);
        TRACE_TEST_NN("MSR  (cnt: %d)", msrCnt);
        DUMP_MEM32_TEST("", MSR, RTS_MEASUREMENT_WORDS);
        
        READ(&error, MSR, &msrCnt, i, READ_SHADOW);
        TRACE_TEST_NN("MSRS (cnt: %d)", msrCnt);
        DUMP_MEM32_TEST("", MSR, RTS_MEASUREMENT_WORDS);
    }
fail:
    RTS_ERR_HANDLE(status, LWRV_OK);
    return;
}

static void badIndexTest(void)
{
    LWRV_STATUS status = LWRV_OK;
    RTS_ERROR error;
    LwU32 MSR[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 msrCnt = 0;
    READ(&error, MSR, &msrCnt, RTS_MAX_MSR_IDX, READ_SHADOW);

fail:
    RTS_ERR_HANDLE(status, LWRV_ERR_OUT_OF_RANGE);
    return;
}

/*
    RTS-REQ-1: Extend measurement - LibRTS shall provide interface to write measurement at given MSR index. RTS has extension FSM which computes SHA2-384 on the provided inputs and existing measurement.

    RTS-REQ-2: Shadow register extend
*/
static void extendTest(void)
{
    LWRV_STATUS status = LWRV_OK;
    RTS_ERROR error;
    LwU32 msrCnt = 0;
    LwU32 msrCntPrevious = 0;
    LwU32 msrsCntPrevious = 0;
    LwU32 empty[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 MSR[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 MSRPrevious[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 MSRSPrevious[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 m_defaultMeasurement[RTS_MEASUREMENT_WORDS] = {0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC};

    for(LwU32 i = RTS_MIN_TEST_IDX; i < RTS_MAX_TEST_IDX; i++)
    {
        // Read MSR
        READ(&error, MSRPrevious, &msrCntPrevious, i, !READ_SHADOW);
        if(RTS_IDENTICAL_MSR(MSR, empty))
        {
            // set some initial value
            STORE(&error, (LwU32*) m_defaultMeasurement, i, FLUSH_SHADOW);
            READ(&error, MSRPrevious, &msrCntPrevious, i, !READ_SHADOW);
        }

        // Read MSRS
        READ(&error, MSRSPrevious, &msrsCntPrevious, i, READ_SHADOW);

        // Extend
        STORE(&error, (LwU32*) m_defaultMeasurement, i, !FLUSH_SHADOW);

        // Read back updated MSR
        READ(&error, MSR, &msrCnt, i, !READ_SHADOW);
        if(RTS_IDENTICAL_MSR(MSRPrevious, MSR) || (msrCnt != msrCntPrevious+1))
        {
            RTS_THROW_ERROR(LWRV_ERR_GENERIC, "MSR[%d] failed to extend", i);
        }

        READ(&error, MSR, &msrCnt, i, READ_SHADOW);
        if( RTS_IDENTICAL_MSR(MSRSPrevious, MSR) || (msrCnt != msrsCntPrevious+1))
        {
            RTS_THROW_ERROR(LWRV_ERR_GENERIC, "MSRS[%d] failed to extend", i);
        }
    }
fail:
    RTS_ERR_HANDLE(status, LWRV_OK);
    return;
}

/*
    RTS-REQ-1: Extend measurement
        Verify MSR is not reset when shadow register is reset

    RTS-REQ-2: Shadow register reset
    Verify that Extended shadow register does not yield same MRV as Flushed shadow register
*/
static void flushTest(void)
{
    LWRV_STATUS status = LWRV_OK;
    RTS_ERROR error;
    LwU32 msrCntExtend = 0;
    LwU32 msrCntFlushed = 0;
    LwU32 MSRS_Extended[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 MSRS_Flushed[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 MSR_Flushed[RTS_MEASUREMENT_WORDS] = {0};
    LwU32 m_defaultMeasurement[RTS_MEASUREMENT_WORDS] = {0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555, 0x66666666, 0x77777777, 0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC};

    if(((RTS_MIN_TEST_IDX + 1) == RTS_MAX_TEST_IDX) || ((RTS_MIN_TEST_IDX+1) >= (RTS_MAX_MSR_IDX)))
    {
        RTS_THROW_ERROR(LWRV_ERR_GENERIC, "Need at least 2 valid test slots");
    }
    else
    {
        LwU32 idxReset = RTS_MIN_TEST_IDX;
        LwU32 idxExtend = RTS_MIN_TEST_IDX + 1;

        // Flush both shadow registers
        STORE(&error, (LwU32*) m_defaultMeasurement, idxReset, FLUSH_SHADOW);
        STORE(&error, (LwU32*) m_defaultMeasurement, idxExtend, FLUSH_SHADOW);

        // Verify MSRS are same and Cnt == 1
        READ(&error, MSRS_Flushed, &msrCntFlushed, idxReset, READ_SHADOW);
        READ(&error, MSRS_Extended, &msrCntExtend, idxExtend, READ_SHADOW);
        if( (!RTS_IDENTICAL_MSR(MSRS_Flushed, MSRS_Extended)) || 
            (msrCntFlushed != 1) ||
            (msrCntExtend != 1))
        {
            RTS_THROW_ERROR(LWRV_ERR_GENERIC, "Failed to Flush Shadow Register");
        }

        // Extend one
        STORE(&error, (LwU32*) m_defaultMeasurement, idxExtend, !FLUSH_SHADOW);

        // Flush the other
        STORE(&error, (LwU32*) m_defaultMeasurement, idxReset, FLUSH_SHADOW);

        // Verify that:
        // MSRS' differ
        // Extended MSRS cnt is 2
        // Flushed MSRS cnt is 1
        READ(&error, MSRS_Flushed, &msrCntFlushed, idxReset, READ_SHADOW);
        READ(&error, MSRS_Extended, &msrCntExtend, idxExtend, READ_SHADOW);
        if( (RTS_IDENTICAL_MSR(MSRS_Flushed, MSRS_Extended)) || 
            (msrCntFlushed != 1) ||
            (msrCntExtend != 2))
        {
            RTS_THROW_ERROR(LWRV_ERR_GENERIC, "Flush/Extend Comparison did not differ as expected");
        }

        // Verify that:
        // MSR is different than MSRS after flush
        // counter is not for flushed for MSR
        READ(&error, MSR_Flushed, &msrCntFlushed, idxReset, !READ_SHADOW);
        if( (RTS_IDENTICAL_MSR(MSRS_Flushed, MSR_Flushed)) || 
            (msrCntFlushed == 1))
        {
            RTS_THROW_ERROR(LWRV_ERR_GENERIC, "MSR was flushed when only MSRS should have been flushed");
        }
    }
fail:
    RTS_ERR_HANDLE(status, LWRV_OK);
    return;
}


/*
    TODO: need to find some way to verify RTS-REQ-6.

    RTS-REQ-6: No mutex - Though RTS is shared resource accessible from all SECHUB masters, HW RTS is intentionally designed to avoid using mutexes so that malicious code which does not release mutex should not prevent other engines from accessing RTS.

    Possibly we could create some multithreaded ucode which we intentionally interrupt in the middle of a Read/Store operation?

*/
