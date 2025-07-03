/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2 LWSR test.
//

#include "rmt_cl95a1sec2.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "lwsrMutexKeys.h"

#define LW_PSR_MUTEX_DIGEST_KEY_OFFS     0
#define LW_PSR_MUTEX_MSG_KEY_OFFS        (LW_PSR_MUTEX_DIGEST_KEY_OFFS + LW_PSR_MUTEX_DIGEST_KEY_SIZE)
#define LW_PSR_MUTEX_PRIV_KEY_BLOB_OFFS  (LW_PSR_MUTEX_MSG_KEY_OFFS + LW_PSR_MUTEX_MSG_KEY_SIZE)
#define LW_PSR_MUTEX_SEMA_OFFS           (LW_PSR_MUTEX_PRIV_KEY_BLOB_OFFS + LW_PSR_MUTEX_PRIV_KEY_BLOB_SIZE)
#define LW_PSR_MUTEX_SEMA_SIZE           4

/*!
 * Class95a1Sec2LwsrTest
 *
 * This test is used to verify the flow of the LWSR mutex acquirement between
 * SEC2 and RM.
 */
class Class95a1Sec2LwsrTest: public RmTest
{
public:
    Class95a1Sec2LwsrTest();
    virtual ~Class95a1Sec2LwsrTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC AllocateMem(Sec2MemDesc *, LwU32);
    virtual RC FreeMem(Sec2MemDesc*);
    virtual RC SendSemaMethod();
    virtual RC PollSemaMethod();
    virtual RC StartMethodStream(const char*, LwU32);
    virtual RC EndMethodStream(const char*);
    virtual RC MutexAcquire(LwU32, LwU32);

private:
    LwRm::Handle  m_hDev;
    LwRm::Handle  m_hDisplay;

    Sec2MemDesc   m_keyMemDesc;

    LwU32         m_runFromRm;

    LwRm::Handle  m_hObj;
    FLOAT64       m_TimeoutMs;

    GpuSubdevice *m_pSubdev;
};

/*!
 * Construction and desctructors
 */
Class95a1Sec2LwsrTest::Class95a1Sec2LwsrTest() :
    m_hDev(0),
    m_hDisplay(0),
    m_hObj(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_pSubdev(nullptr)
{
    SetName("Class95a1Sec2LwsrTest");

    m_runFromRm = 0;
}

Class95a1Sec2LwsrTest::~Class95a1Sec2LwsrTest()
{
}

/*!
 * IsTestSupported
 *
 * Need class 95A1 to be supported
 */
string Class95a1Sec2LwsrTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

RC Class95a1Sec2LwsrTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
{
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Coherent);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    pVidMemSurface->Alloc(GetBoundGpuDevice());
    pVidMemSurface->Map();

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return OK;
}

/*!
 * FreeMem
 *
 * Frees the above allocated memory
 */
RC Class95a1Sec2LwsrTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

/*!
 * Setup
 *
 * Sets up the timeout - Setting to max for emulation cases
 * Allocates Channel and object
 * Allocates necessary memory required for mutex acquirement
 */
RC Class95a1Sec2LwsrTest::Setup()
{
    LwRmPtr  pLwRm;
    RC       rc;

    m_hDev = pLwRm->GetDeviceHandle(GetBoundGpuDevice());

    CHECK_RC(InitFromJs());

    if (m_runFromRm)
    {
        CHECK_RC(pLwRm->Alloc(m_hDev, &m_hDisplay, LW04_DISPLAY_COMMON, NULL));

    }
    else
    {
        m_TimeoutMs = m_TestConfig.TimeoutMs();

        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

        CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
        m_pCh->SetObject(0, m_hObj);
    }

    // Allocate Memory for Main class
    AllocateMem(&m_keyMemDesc,
                (LW_PSR_MUTEX_DIGEST_KEY_SIZE +
                 LW_PSR_MUTEX_MSG_KEY_SIZE +
                 LW_PSR_MUTEX_PRIV_KEY_BLOB_SIZE +
                 LW_PSR_MUTEX_SEMA_SIZE));

    return rc;
}

/*!
 * SendSemaMethod
 *
 * Pushes semaphore methods into channel push buffer.
 * Uses 0x12345678 as the payload
 */
RC Class95a1Sec2LwsrTest::SendSemaMethod()
{
    m_pCh->Write(0, LW95A1_SEMAPHORE_A,
        DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (LwU32)((m_keyMemDesc.gpuAddrSysMem + LW_PSR_MUTEX_SEMA_OFFS) >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (LwU32)((m_keyMemDesc.gpuAddrSysMem + LW_PSR_MUTEX_SEMA_OFFS) & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C,
        DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));

    return OK;
}

/*!
 * PollSemaMethod
 *
 * Polls for the semaphore to be released by SEC2
 * Necessarily expects the payload 0x12345678 to be written by SEC2 into semaphore surface
 */
RC Class95a1Sec2LwsrTest::PollSemaMethod()
{
    RC            rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)((UINT08*)m_keyMemDesc.pCpuAddrSysMem + LW_PSR_MUTEX_SEMA_OFFS);
    Printf(Tee::PriHigh, "LWSR: Polling started for pAddr %p Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "LWSR: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

/*!
 * StartMethodStream
 *
 * Does startup tasks before starting a method stream
 * Also pushes needed methods which are always used
 */
RC Class95a1Sec2LwsrTest::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC  rc = OK;

    Printf(Tee::PriHigh, "LWSR: Starting method stream for %s \n", pId);
    MEM_WR32((UINT32*)((UINT08*)m_keyMemDesc.pCpuAddrSysMem + LW_PSR_MUTEX_SEMA_OFFS), 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_LWSR);
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

/*!
 * EndMethodStream
 *
 * Flushes the channel and polls on semaphore
 */
RC Class95a1Sec2LwsrTest::EndMethodStream(const char *pId)
{
    RC    rc           = OK;
    LwU64 startTimeMS  = 0;
    LwU64 elapsedTimeMS= 0;

    m_pSubdev = GetBoundGpuSubdevice();

    m_pCh->Write(0, LW95A1_EXELWTE, 0x1); // Notify on End
    m_pCh->Flush();

    startTimeMS = Platform::GetTimeMS();
    rc = PollSemaMethod();
    elapsedTimeMS = (Platform::GetTimeMS() - startTimeMS);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "LWSR: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "LWSR: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "LWSR: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

/* Encrypted Test Key Vector */
LwU8 g_Msg[]           = {0x52, 0x33, 0x15, 0x58, 0x03, 0xA1, 0x22, 0x90, 0x24, 0x6E, 0x60, 0x01};
// The first 16 bytes of key[] is the encrypted key and the second 32 bytes is the hash signature
// Decrypted data      = {0xC0, 0xD8, 0x32, 0x75, 0xA2, 0x56, 0xDD, 0x4D, 0x61, 0x82, 0xC3, 0xED, 0x76, 0x8F, 0x25, 0x7E,
//                        0xEC, 0xC8, 0xC8, 0x1A, 0x09, 0xBC, 0xB7, 0x9C, 0xB3, 0x41, 0x53, 0x80, 0x04, 0x86, 0x83, 0x48,
//                        0xD3, 0x67, 0x92, 0x82, 0x81, 0xBB, 0x96, 0xB9, 0x85, 0x62, 0x65, 0x9F, 0xEB, 0x63, 0xD7, 0xA2};
LwU8 g_Key[]           = {0x23, 0xA9, 0x8D, 0xCB, 0x8F, 0x90, 0x18, 0x00, 0x72, 0x42, 0x29, 0x65, 0x12, 0xD8, 0x1B, 0xDD,
                          0xDB, 0x48, 0x07, 0xDC, 0x54, 0xAD, 0xB2, 0x17, 0x33, 0x0B, 0x06, 0x0F, 0x41, 0xC0, 0xD5, 0x8E,
                          0x99, 0x88, 0xAA, 0x05, 0x4F, 0xB4, 0xB5, 0x09, 0xD0, 0xEE, 0x56, 0xA0, 0x97, 0x04, 0x2C, 0xE0};
// Result of HMAC-SHA1
LwU8 g_Mutex_HSHA1[]   = {0x4E, 0x77, 0x61, 0x01};

// Result of Feistel Cipher
LwU8 g_Mutex_FC[]      = {0xAD, 0xAC, 0x61, 0x9F};

// Result of Invalid Key
LwU8 g_Mutex_Ilwalid[] = {0xFF, 0xFF, 0xFF, 0xFF};

/*!
 * MutexAcquire
 *
 * Handles MutexAcquire method
 */
RC Class95a1Sec2LwsrTest::MutexAcquire(LwU32 algorithm, LwU32 negative)
{
    RC    rc              = OK;
    LwU8 *pMutexKeyOffs   = (LwU8 *)m_keyMemDesc.pCpuAddrSysMem + LW_PSR_MUTEX_DIGEST_KEY_OFFS;
    LwU8 *pMessageKeyOffs = (LwU8 *)m_keyMemDesc.pCpuAddrSysMem + LW_PSR_MUTEX_MSG_KEY_OFFS;
    LwU8 *pPrivKeyOffs    = (LwU8 *)m_keyMemDesc.pCpuAddrSysMem + LW_PSR_MUTEX_PRIV_KEY_BLOB_OFFS;
    LwU8 *pMutex          = g_Mutex_Ilwalid;

    memset(m_keyMemDesc.pCpuAddrSysMem, 0, (LW_PSR_MUTEX_DIGEST_KEY_SIZE + LW_PSR_MUTEX_MSG_KEY_SIZE + LW_PSR_MUTEX_PRIV_KEY_BLOB_SIZE));

    memcpy(pMessageKeyOffs, g_Msg, LW_PSR_MUTEX_MSG_KEY_SIZE);
    memcpy(pPrivKeyOffs, g_Key, LW_PSR_MUTEX_PRIV_KEY_BLOB_SIZE);

    if (negative)
    {
        // Ilwalidate the private key
        *(pPrivKeyOffs+0) = 0x00;
        *(pPrivKeyOffs+1) = 0x00;
        *(pPrivKeyOffs+2) = 0x00;
        *(pPrivKeyOffs+3) = 0x00;
    }

    Printf(Tee::PriHigh, "LWSR: Message = 0x%02x, 0x%02x, 0x%02x, 0x%02x!\n",
           *(pMessageKeyOffs+0),
           *(pMessageKeyOffs+1),
           *(pMessageKeyOffs+2),
           *(pMessageKeyOffs+3));

    Printf(Tee::PriHigh, "LWSR: Priv Key = 0x%02x, 0x%02x, 0x%02x, 0x%02x!\n",
           *(pPrivKeyOffs+0),
           *(pPrivKeyOffs+1),
           *(pPrivKeyOffs+2),
           *(pPrivKeyOffs+3));

    if (m_runFromRm)
    {
        LW0073_CTRL_SPECIFIC_LWSR_MUTEX_ACQUIRE_PARAMS mutexAcquireParams = {0};
        LwRmPtr pLwRm;

        mutexAcquireParams.stepIndex = LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE_STEP_INIT;

        CHECK_RC(pLwRm->Control(m_hDisplay,
                             LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE,
                             &mutexAcquireParams, sizeof (mutexAcquireParams)));

        mutexAcquireParams.stepIndex = LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE_STEP_ACQUIRE;
        mutexAcquireParams.pKeyBuf   = (LwU8*)m_keyMemDesc.pCpuAddrSysMem;
        mutexAcquireParams.algorithm = algorithm;

        CHECK_RC(pLwRm->Control(m_hDisplay,
                             LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE,
                             &mutexAcquireParams, sizeof (mutexAcquireParams)));
    }
    else
    {
        CHECK_RC(StartMethodStream("MutexAcquire", 1));

        m_pCh->Write(0, LW95A1_LWSR_MUTEX_ACQUIRE       , algorithm);
        m_pCh->Write(0, LW95A1_LWSR_MUTEX_ACQUIRE_KEYBUF, (LwU32)(m_keyMemDesc.gpuAddrSysMem >> 8));

        CHECK_RC(EndMethodStream("MutexAcquire"));
    }

    Printf(Tee::PriHigh, "LWSR: algorithm = %d, nagative = %d\n", algorithm, negative);
    if (!negative)
    {
        if (algorithm == LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_HMACSHA1)
        {
            pMutex = g_Mutex_HSHA1;
        }
        else if (algorithm == LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_FEISTELCIPHER)
        {
            pMutex = g_Mutex_FC;
        }
    }

    Printf(Tee::PriHigh, "LWSR: Mutex = 0x%02x, 0x%02x, 0x%02x, 0x%02x, expected = 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
           *(pMutexKeyOffs+0),
           *(pMutexKeyOffs+1),
           *(pMutexKeyOffs+2),
           *(pMutexKeyOffs+3),
           pMutex[0], pMutex[1], pMutex[2], pMutex[3]);

    if ((*(pMutexKeyOffs+0) != pMutex[0]) ||
        (*(pMutexKeyOffs+1) != pMutex[1]) ||
        (*(pMutexKeyOffs+2) != pMutex[2]) ||
        (*(pMutexKeyOffs+3) != pMutex[3]))
    {
        rc = !OK;
    }

    if (m_runFromRm)
    {
        LW0073_CTRL_SPECIFIC_LWSR_MUTEX_ACQUIRE_PARAMS mutexAcquireParams = {0};
        LwRmPtr  pLwRm;

        mutexAcquireParams.stepIndex = LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE_STEP_END;

        CHECK_RC(pLwRm->Control(m_hDisplay,
                                LW0073_CTRL_CMD_SPECIFIC_LWSR_MUTEX_ACQUIRE,
                                &mutexAcquireParams, sizeof (mutexAcquireParams)));
    }

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2LwsrTest::Run()
{
    RC rc = OK;

    CHECK_RC(MutexAcquire(LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_HMACSHA1, 0));
    CHECK_RC(MutexAcquire(LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_HMACSHA1, 1));
    CHECK_RC(MutexAcquire(LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_FEISTELCIPHER, 0));
    CHECK_RC(MutexAcquire(LW95A1_LWSR_MUTEX_ACQUIRE_ALGORITHM_FEISTELCIPHER, 1));

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2LwsrTest::Cleanup()
{
    RC       rc;
    RC       firstRc;
    LwRmPtr  pLwRm;

    FreeMem(&m_keyMemDesc);
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2LwsrTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2LwsrTest, RmTest,
                 "Class95a1Sec2LwsrTest RM test.");

