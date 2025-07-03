/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2 test.
//

#include "rmt_cl95a1sec2.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"

#define HDCP_MODE_ILWALID   0
#define HDCP_MODE_NORMAL    1
#define HDCP_MODE_DEBUG     2
#define HDCP_MODE_DEBUG_MIN 3

/*!
 * Class95a1Sec2Test
 *
 * HDCP test works only with DEBUG fused chip
 */
class Class95a1Sec2Test: public RmTest
{
public:
    Class95a1Sec2Test();
    virtual ~Class95a1Sec2Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC PrintBuf(const char *pStr, LwU8 *pBuf, LwU32 size);
    virtual RC AllocateMem(Sec2MemDesc *, LwU32);
    virtual RC FreeMem(Sec2MemDesc*);
    virtual RC SendSemaMethod();
    virtual RC PollSemaMethod();
    virtual RC StartMethodStream(const char*, LwU32);
    virtual RC EndMethodStream(const char*);
    virtual RC ReadCaps();
    virtual RC Init();
    virtual RC CreateSession(HdcpReceiver *);
    virtual RC SetupReceiver(HdcpReceiver *, LwU32 , LwU32, LwU8);
    virtual RC FreeReceiver(HdcpReceiver *pReceiver);
    virtual RC VerifyCert(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC RevocationCheck(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC GenerateEkm(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC VerifyHprime(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC GenerateLcInit(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC GetRttChallenge(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC VerifyLprime(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC GenerateSkeInit(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC VerifyVprime(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC SessionCtrl(HdcpReceiver *pReceiver, LwU32 negative);
    virtual RC UpdateSession(HdcpReceiver *pReceiver, LwU32 updMask);
    virtual RC Encrypt(HdcpReceiver *pReceiver, LwU8 *pContent, LwU8 *pEncContent, LwU32 size);

    virtual RC ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd, const char *pStr,
                                       LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size);
    virtual RC ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd);

    SETGET_PROP(hdcpMode, LwU32);
    SETGET_PROP(hdcpNegative, bool);

private:
    Sec2MemDesc m_resMemDesc;
    Sec2MemDesc m_sbMemDesc;
    Sec2MemDesc m_semMemDesc;
    Sec2MemDesc m_srmMemDesc;
    Sec2MemDesc m_dcpMemDesc;

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;

    LwU32        m_scratchBufSize;

    // HdcpReceivers
    HdcpReceiver m_receivers[2] = {};

    GpuSubdevice *m_pSubdev;

    LwU32         m_hdcpMode;
    bool          m_hdcpNegative;
};

/*!
 * Construction and desctructors
 */
Class95a1Sec2Test::Class95a1Sec2Test() :
    m_hObj(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_scratchBufSize(0),
    m_pSubdev(nullptr)
{
    SetName("Class95a1Sec2Test");
    m_hdcpMode      = HDCP_MODE_DEBUG;
    m_hdcpNegative = false;
}

Class95a1Sec2Test::~Class95a1Sec2Test()
{
}

/*!
 * IsTestSupported
 *
 * Need class 95A1 to be supported
 */
string Class95a1Sec2Test::IsTestSupported()
{
    // Test takes too long to run on fmodel and so disabling it.
    if (IsClassSupported(LW95A1_TSEC) &&
       (Platform::GetSimulationMode() == Platform::Hardware))
    {
        return RUN_RMTEST_TRUE;
    }
    return "LW95A1 class is not supported on current platform";
}

/*!
 * PrintBuf
 *
 * Helped routing to print buffers in hex format
 */
RC Class95a1Sec2Test::PrintBuf(const char *pStr, LwU8 *pBuf, LwU32 size)
{
    Printf(Tee::PriHigh, "HDCP: %s\n", pStr);
    for (LwU32 i=0; i < size; i++)
    {
        if (i && (!(i % 16)))
            Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "%02x ",(LwU8)pBuf[i]);
    }
    Printf(Tee::PriHigh, "\n");
    return OK;
}

RC Class95a1Sec2Test::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
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
RC Class95a1Sec2Test::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

/*!
 * Setup
 *
 * Sets up the timeout - Setting to max for emulation cases
 * Allocates Channel and object
 * Allocates necessary memory required for HDCP
 * Copies the necessary constants into allocated memory
 */
RC Class95a1Sec2Test::Setup()
{

    LwRmPtr      pLwRm;
    RC           rc;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
    m_pCh->SetObject(0, m_hObj);

    // Allocate Memory for Main class
    AllocateMem(&m_semMemDesc, 0x4);
    AllocateMem(&m_resMemDesc, 0x100);
    AllocateMem(&m_srmMemDesc, 0x300);
    AllocateMem(&m_dcpMemDesc, 0x300);

    // Copy SRM and DCP key
    memcpy((char*)m_srmMemDesc.pCpuAddrSysMem, g_srm, g_srmSize);
    memcpy((char*)m_dcpMemDesc.pCpuAddrSysMem, g_dcpKey, g_dcpSize);

    Printf(Tee::PriHigh, "HDCP: Running in mode '%u'\n", (unsigned int)m_hdcpMode);
    Printf(Tee::PriHigh, "HDCP: Negative testing mode '%u'\n", m_hdcpNegative ? 1 : 0);

    return rc;
}

/*!
 * SendSemaMethod
 *
 * Pushes semaphore methods into channel push buffer.
 * Uses 0x12345678 as the payload
 */
RC Class95a1Sec2Test::SendSemaMethod()
{
    m_pCh->Write(0, LW95A1_SEMAPHORE_A,
        DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (LwU32)(m_semMemDesc.gpuAddrSysMem >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (LwU32)(m_semMemDesc.gpuAddrSysMem & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C,
        DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));

    return OK;
}

/*!
 * PollSemaMethod
 *
 * Polls for the semaphore to be released by TSEC
 * Necessarily expects the payload 0x12345678 to be written by TSEC into semaphore surface
 */
RC Class95a1Sec2Test::PollSemaMethod()
{
    RC              rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)m_semMemDesc.pCpuAddrSysMem;
    Printf(Tee::PriHigh, "HDCP: Polling started for pAddr %p Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "HDCP: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

/*!
 * StartMethodStream
 *
 * Does startup tasks before starting a method stream
 * Also pushes needed methods which are always used
 */
RC Class95a1Sec2Test::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC              rc = OK;

    Printf(Tee::PriHigh, "HDCP: Starting method stream for %s \n", pId);
    MEM_WR32(m_semMemDesc.pCpuAddrSysMem, 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_HDCP);
    if (sendSBMthd)
        m_pCh->Write(0, LW95A1_HDCP_SET_SCRATCH_BUFFER, (LwU32)(m_sbMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

/*!
 * EndMethodStream
 *
 * Flushes the channel and polls on semaphore
 */
RC Class95a1Sec2Test::EndMethodStream(const char *pId)
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
        Printf(Tee::PriHigh, "HDCP: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "HDCP: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "HDCP: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

/*!
 * ErrorCheck
 *
 * Helper routing to check the retCode with the expected retCode and prints
 * error message based on the comparison
 */
RC Class95a1Sec2Test::ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd)
{
    RC   rc = OK;

    PrintBuf("ResBuf ", (LwU8*)m_resMemDesc.pCpuAddrSysMem, 0x100);
    if (result != expected)
    {
        Printf(Tee::PriHigh, "HDCP: ERROR: '%s' FAILED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, (unsigned int) result, (unsigned int) expected);
        rc = ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "HDCP: '%s' PASSED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, result, expected);
    }
    return rc;
}

/*!
 * ErrorCheckWithComparison
 *
 * Helper routing to check the retcode and also compare the buffer with the expected buffer.
 */
RC Class95a1Sec2Test::ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd,
                                     const char *pStr,  LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size)
{
    RC rc = OK;
    CHECK_RC(ErrorCheck(expected, result, pMthd));

    PrintBuf(pStr, pLwrrent, size);
    if (memcmp((char*)pLwrrent, (char*)pSrc, size))
    {
        rc = ERROR;
        Printf(Tee::PriHigh, "HDCP: ERROR: '%s %s' comparison FAILED\n", pMthd, pStr);
    }
    else
    {
        Printf(Tee::PriHigh, "HDCP: '%s %s' comparison PASSED\n", pMthd, pStr);
    }
    return rc;

}

/*!
 * ReadCaps
 *
 * Handles ReadCaps method and allocates memory based on the capability.
 * Also populates the global data structure with the caps
 */
RC Class95a1Sec2Test::ReadCaps()
{
    RC                    rc        = OK;
    hdcp_read_caps_param *pReadCaps = (hdcp_read_caps_param *)m_resMemDesc.pCpuAddrSysMem;

    pReadCaps->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);

    CHECK_RC(StartMethodStream("ReadCaps", 0));
    m_pCh->Write(0, LW95A1_HDCP_READ_CAPS, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("ReadCaps"));

    CHECK_RC(ErrorCheck(HCLASS(ERROR_NONE), pReadCaps->retCode, "ReadCaps"));
    Printf(Tee::PriHigh, "Read caps Scratch Buffer size:  0x%0x\n", pReadCaps->scratchBufferSize);

    // Allocate scratch
    m_scratchBufSize = pReadCaps->scratchBufferSize;
    AllocateMem(&m_sbMemDesc, m_scratchBufSize);
    memset(m_sbMemDesc.pCpuAddrSysMem, 0, m_scratchBufSize);

    return rc;
}

/*!
 * Init
 *
 * Handles INIT method
 */
RC Class95a1Sec2Test::Init()
{
    RC              rc        = OK;
    hdcp_init_param *pInit = (hdcp_init_param *)m_resMemDesc.pCpuAddrSysMem;

    pInit->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);

    CHECK_RC(StartMethodStream("Init", 1));
    m_pCh->Write(0, LW95A1_HDCP_INIT, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("Init"));

    PrintBuf("Scratch Buf ", (LwU8*)m_sbMemDesc.pCpuAddrSysMem, m_scratchBufSize);
    CHECK_RC(ErrorCheck(HCLASS(ERROR_NONE), pInit->retCode, "Init"));
    return rc;

}

/*!
 * CreateSession
 *
 * Creates a session for known receiver
 */
RC Class95a1Sec2Test::CreateSession(HdcpReceiver *pReceiver)
{
    RC                        rc        = OK;
    hdcp_create_session_param *pCreate  = (hdcp_create_session_param *)m_resMemDesc.pCpuAddrSysMem;

    pCreate->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pCreate->noOfStreams = 2;

    CHECK_RC(StartMethodStream("CreateSession", 1));
    m_pCh->Write(0, LW95A1_HDCP_CREATE_SESSION, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("CreateSession"));

    //CHECK_RC(ErrorCheckWithComparison(HCLASS(ERROR_NONE), pCreate->retCode, "CreateSession", "RTX",
    //                                  (LwU8*)g_rtx[pReceiver->index], (LwU8*)&(pCreate->rtx), HCLASS(SIZE_RTX_8)));
    CHECK_RC(ErrorCheck(HCLASS(ERROR_NONE), pCreate->retCode, "CreateSession"));

    pReceiver->sessionID = pCreate->sessionID;
    pReceiver->rtx       = pCreate->rtx;

    Printf(Tee::PriHigh, "HDCP: CreateSession SessionID 0x%0x\n", pCreate->sessionID);
    PrintBuf("CreateSession RTX", (LwU8*)&(pCreate->rtx), HCLASS(SIZE_RTX_8));

    return rc;
}

/*!
 * VerifyCert
 *
 * Handles certificate verification. In the case of negative testing,
 * it corrupts the certificate and wait for corresponding ERROR code
 * from SEC.
 */
RC Class95a1Sec2Test::VerifyCert(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                        rc        = OK;
    hdcp_verify_cert_rx_param *pVerify  = (hdcp_verify_cert_rx_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                     expectedRetCode = HCLASS(ERROR_NONE);

    pVerify->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pVerify->sessionID = pReceiver->sessionID;
    pVerify->repeater = pReceiver->bIsRepeater;

    if (negative)
    {
        // Lets corrupt the cert
        char *p = (char *)pReceiver->m_certMemDesc.pCpuAddrSysMem;
        p[10]=0xa;
        p[11]=0xb;
        p[12]=0xc;
        p[13]=0xd;
        expectedRetCode = HCLASS(VERIFY_CERT_RX_ERROR_ILWALID_CERT);
    }

    CHECK_RC(StartMethodStream("VerifyCert", 1));
    m_pCh->Write(0, LW95A1_HDCP_SET_DCP_KPUB, (LwU32)(m_dcpMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_SET_CERT_RX, (LwU32)(pReceiver->m_certMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_VERIFY_CERT_RX, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("VerifyCert"));

    if (negative)
    {
        memcpy((char *)pReceiver->m_certMemDesc.pCpuAddrSysMem, g_certificates[0], HCLASS(SIZE_CERT_RX_8));
    }

    CHECK_RC(ErrorCheck(expectedRetCode, pVerify->retCode, "VerifyCert"));

    return rc;
}

/*!
 * RevocationCheck
 *
 * Handles revocation check. In negative testing, it corrupts the SRM
 * and waits for SRM validation error code from SEC.
 */
RC Class95a1Sec2Test::RevocationCheck(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                          rc        = OK;
    hdcp_revocation_check_param *pRevoke  = (hdcp_revocation_check_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                       expectedRetCode = HCLASS(ERROR_NONE);

    pRevoke->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pRevoke->transID.sessionID = pReceiver->sessionID;
    pRevoke->isVerHdcp2x       = 1;

    if (negative)
    {
        // Lets corrupt the SRM
        char *p = (char *)m_srmMemDesc.pCpuAddrSysMem;
        p[13]=0xa;
        p[14]=0xb;
        expectedRetCode = HCLASS(REVOCATION_CHECK_ERROR_SRM_VALD_FAILED);
    }

    CHECK_RC(StartMethodStream("RevocationCheck", 1));
    m_pCh->Write(0, LW95A1_HDCP_SET_SRM, (LwU32)(m_srmMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_REVOCATION_CHECK, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("RevocationCheck"));

    if (negative)
    {
        memcpy((char*)m_srmMemDesc.pCpuAddrSysMem, g_srm, g_srmSize);
    }

    CHECK_RC(ErrorCheck(expectedRetCode, pRevoke->retCode, "RevocationCheck"));
    return rc;
}

/*!
 * GenerateEkm
 *
 * Generates encrypted KM
 */
RC Class95a1Sec2Test::GenerateEkm(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                        rc      = OK;
    hdcp_generate_ekm_param   *pEkm   = (hdcp_generate_ekm_param *)m_resMemDesc.pCpuAddrSysMem;

    pEkm->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pEkm->sessionID = pReceiver->sessionID;

    CHECK_RC(StartMethodStream("GenerateEkm", 1));
    m_pCh->Write(0, LW95A1_HDCP_GENERATE_EKM, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GenerateEkm"));

    CHECK_RC(ErrorCheckWithComparison(HCLASS(ERROR_NONE), pEkm->retCode, "GenerateEkm", "EKM",
                                      (LwU8*)g_eKm[pReceiver->index], (LwU8*)pEkm->eKm, HCLASS(SIZE_E_KM_8)));

    return rc;
}

/*!
 * VerifyHprime
 *
 * Verifies HPRIME. In the case of negative testing, corrupts hprime and waits for
 * VPRIME validation failure error code to show up.
 */
RC Class95a1Sec2Test::VerifyHprime(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                        rc         = OK;
    hdcp_verify_hprime_param  *pHprime   = (hdcp_verify_hprime_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                     expectedRetCode = HCLASS(ERROR_NONE);

    pHprime->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pHprime->sessionID = pReceiver->sessionID;
    memcpy((char*)pHprime->hprime, pReceiver->hprime, LW95A1_HDCP_SIZE_HPRIME_8);

    if (negative)
    {
        pHprime->hprime[0] = 0xabcdef12;
        expectedRetCode = HCLASS(VERIFY_HPRIME_ERROR_HPRIME_VALD_FAILED);
    }

    CHECK_RC(StartMethodStream("VerifyHprime", 1));
    m_pCh->Write(0, LW95A1_HDCP_VERIFY_HPRIME, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GenerateEkm"));

    CHECK_RC(ErrorCheck(expectedRetCode, pHprime->retCode, "VerifyHprime"));

    return rc;
}

/*!
 * GenerateLcInit
 *
 * Generates RN
 */
RC Class95a1Sec2Test::GenerateLcInit(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                           rc     = OK;
    hdcp_generate_lc_init_param  *pLc   = (hdcp_generate_lc_init_param *)m_resMemDesc.pCpuAddrSysMem;

    pLc->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pLc->sessionID = pReceiver->sessionID;

    CHECK_RC(StartMethodStream("GenerateLcInit", 1));
    m_pCh->Write(0, LW95A1_HDCP_GENERATE_LC_INIT, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GenerateLcInit"));

    CHECK_RC(ErrorCheckWithComparison(HCLASS(ERROR_NONE), pLc->retCode, "GenerateLcInit", "RN",
                                      (LwU8*)g_rn[pReceiver->index], (LwU8*)&(pLc->rn), HCLASS(SIZE_RN_8)));

    return rc;
}

/*!
 * GetRttChallenge
 *
 * Handles RTT challenge
 */
RC Class95a1Sec2Test::GetRttChallenge(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                           rc      = OK;
    hdcp_get_rtt_challenge_param *pRtt   = (hdcp_get_rtt_challenge_param *)m_resMemDesc.pCpuAddrSysMem;

    pRtt->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pRtt->sessionID = pReceiver->sessionID;

    CHECK_RC(StartMethodStream("GetRttChallenge", 1));
    m_pCh->Write(0, LW95A1_HDCP_GET_RTT_CHALLENGE, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GetRttChallenge"));

    CHECK_RC(ErrorCheckWithComparison(HCLASS(ERROR_NONE), pRtt->retCode, "GetRttChallenge", "LPRIME",
                                      (LwU8*)g_lPrime[pReceiver->index], (LwU8*)pRtt->L128l, HCLASS(SIZE_LPRIME_8)/2));

    return rc;
}

/*!
 * VerifyLprime
 *
 * Verifies LPRIME. In the case of negative testing, corrupts LPRIME and
 * make sure it gets the right error code.
 */
RC Class95a1Sec2Test::VerifyLprime(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                       rc       = OK;
    hdcp_verify_lprime_param *pLprime = (hdcp_verify_lprime_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                    expectedRetCode = HCLASS(ERROR_NONE);

    pLprime->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pLprime->sessionID = pReceiver->sessionID;
    memcpy((char*)pLprime->lprime, (char *)pReceiver->lprime, LW95A1_HDCP_SIZE_LPRIME_8);

    if (negative)
    {
        pLprime->lprime[0]=0xabcdef65;
        expectedRetCode = HCLASS(VERIFY_LPRIME_ERROR_LPRIME_VALD_FAILED);
    }

    CHECK_RC(StartMethodStream("VerifyLprime", 1));
    m_pCh->Write(0, LW95A1_HDCP_VERIFY_LPRIME, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("VerifyLprime"));

    CHECK_RC(ErrorCheck(expectedRetCode, pLprime->retCode, "VerifyLprime"));

    return rc;
}

/*!
 * GenerateSkeInit
 *
 * Generaes RIV and EKS
 */
RC Class95a1Sec2Test::GenerateSkeInit(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                           rc       = OK;
    hdcp_generate_ske_init_param *pSke    = (hdcp_generate_ske_init_param *)m_resMemDesc.pCpuAddrSysMem;

    pSke->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pSke->sessionID = pReceiver->sessionID;

    CHECK_RC(StartMethodStream("GenerateSkeInit", 1));
    m_pCh->Write(0, LW95A1_HDCP_GENERATE_SKE_INIT, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GenerateSkeInit"));

    Printf(Tee::PriHigh, "HDCP: GenerateSkeInit RIV      : 0x%llx\n", pSke->riv);

    CHECK_RC(ErrorCheckWithComparison(HCLASS(ERROR_NONE), pSke->retCode, "GenerateSkeInit", "EKS",
                                      (LwU8*)g_eKs[pReceiver->index], (LwU8*)pSke->eKs, HCLASS(SIZE_E_KS_8)/2));

    return rc;
}

/*!
 * VerifyVprime
 */
RC Class95a1Sec2Test::VerifyVprime(HdcpReceiver *pReceiver, LwU32 negative)
{
    RC                           rc         = OK;
    hdcp_verify_vprime_param    *pVprime    = (hdcp_verify_vprime_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                       expectedRetCode = HCLASS(ERROR_NONE);

    pVprime->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pVprime->transID.sessionID = pReceiver->sessionID;

    if (negative)
    {
    }

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_HDCP);
    m_pCh->Write(0, LW95A1_HDCP_SET_SCRATCH_BUFFER, (LwU32)(m_sbMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_VERIFY_VPRIME, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_EXELWTE, 0x0);
    m_pCh->Flush();

    CHECK_RC(SendSemaMethod());

    if (negative)
    {
    //    memcpy((char *)pReceiver->m_certMemDesc.pCpuAddrSysMem, g_certificates[0], HCLASS(SIZE_CERT_RX_8));
    }

    CHECK_RC(ErrorCheck(expectedRetCode, pVprime->retCode, "VerifyVprime"));
    return rc;
}

/*!
 * SessionCtrl
 *
 * Handles session ctrl functionalities
 */
RC Class95a1Sec2Test::SessionCtrl(HdcpReceiver *pReceiver, LwU32 action)
{
    RC                           rc         = OK;
    hdcp_session_ctrl_param     *pSess    = (hdcp_session_ctrl_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                       expectedRetCode = HCLASS(ERROR_NONE);

    pSess->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pSess->sessionID = pReceiver->sessionID;
    pSess->ctrlFlag  = action;

    CHECK_RC(StartMethodStream("SessionCtrl", 1));
    m_pCh->Write(0, LW95A1_HDCP_SESSION_CTRL, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("SessionCtrl"));

    CHECK_RC(ErrorCheck(expectedRetCode, pSess->retCode, "SessionCtrl"));
    return rc;
}

/*!
 * Encrypt
 *
 * Content encryption function. Takes in both input and expected output.
 */
RC Class95a1Sec2Test::Encrypt(HdcpReceiver *pReceiver, LwU8 *pContent, LwU8 *pEncContent, LwU32 size)
{
    RC                     rc        = OK;
    hdcp_encrypt_param     *pEnc     = (hdcp_encrypt_param *)m_resMemDesc.pCpuAddrSysMem;
    Sec2MemDesc            contentMemDesc;
    Sec2MemDesc            outputMemDesc;

    pEnc->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pEnc->sessionID = pReceiver->sessionID;
    pEnc->noOfInputBlocks = (size/16);
    pEnc->streamID = 0;

    size = (pEnc->noOfInputBlocks) * 16;

    AllocateMem(&(contentMemDesc), size);
    memcpy((char *)contentMemDesc.pCpuAddrSysMem, pContent, size);
    AllocateMem(&(outputMemDesc), size);

    CHECK_RC(StartMethodStream("Encrypt", 1));
    m_pCh->Write(0, LW95A1_HDCP_SET_ENC_INPUT_BUFFER, (LwU32)(contentMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_SET_ENC_OUTPUT_BUFFER, (LwU32)(outputMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_HDCP_ENCRYPT, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("Encrypt"));

    PrintBuf("HDCP: Inp Buf ", (LwU8*)contentMemDesc.pCpuAddrSysMem, size);
    PrintBuf("HDCP: Enc Buf ", (LwU8*)outputMemDesc.pCpuAddrSysMem, size);

    rc = ErrorCheckWithComparison(HCLASS(ERROR_NONE), pEnc->retCode, "Encrypt", "OUTPUT",
                                      (LwU8*)pEncContent, (LwU8*)outputMemDesc.pCpuAddrSysMem, size);

    FreeMem(&outputMemDesc);
    FreeMem(&contentMemDesc);
    return rc;
}

/*!
 * UpdateSession
 *
 * Used for updating session details. To be improved.
 */
RC Class95a1Sec2Test::UpdateSession(HdcpReceiver *pReceiver, LwU32 updmask)
{
    RC                        rc        = OK;
    hdcp_update_session_param *pUpd     = (hdcp_update_session_param *)m_resMemDesc.pCpuAddrSysMem;
    LwU32                     expectedRetCode = HCLASS(ERROR_NONE);

    pUpd->retCode = 0;
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    pUpd->sessionID = pReceiver->sessionID;

    if (updmask & (1 << HCLASS(UPDATE_SESSION_MASK_RRX_PRESENT)))
    {

        memcpy((void*)&pUpd->rrx, (void*) &(pReceiver->rrx),
                                          HCLASS(SIZE_RRX_8));
        pUpd->updmask   |= (1 << HCLASS(UPDATE_SESSION_MASK_RRX_PRESENT));
    }

    if (updmask & (1 << HCLASS(UPDATE_SESSION_MASK_VERSION_PRESENT)))
    {
        pUpd->hdcpVer = pReceiver->hdcpVer;
        pUpd->updmask   |= (1 << HCLASS(UPDATE_SESSION_MASK_VERSION_PRESENT));
    }

    if (updmask & (1 << HCLASS(UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT)))
    {
        pUpd->bRecvPreComputeSupport = pReceiver->bRecvPreComputeSupport;
        pUpd->updmask   |= (1 << HCLASS(UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT));
    }

    CHECK_RC(StartMethodStream("UpdateSession", 1));
    m_pCh->Write(0, LW95A1_HDCP_UPDATE_SESSION, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("UpdateSession"));

    CHECK_RC(ErrorCheck(expectedRetCode, pUpd->retCode, "UpdateSession"));
    return rc;
}

/*!
 * SetupReceiver
 *
 * Sets up receiver details such as certificate and necessary known values
 */
RC Class95a1Sec2Test::SetupReceiver(HdcpReceiver *pReceiver, LwU32 index, LwU32 version, LwU8 computeSupport)
{
    AllocateMem(&(pReceiver->m_certMemDesc), HCLASS(SIZE_CERT_RX_8));
    memcpy((char *)pReceiver->m_certMemDesc.pCpuAddrSysMem, g_certificates[index], HCLASS(SIZE_CERT_RX_8));
    memcpy((char *)pReceiver->hprime, (char*)g_hPrime[index], LW95A1_HDCP_SIZE_HPRIME_8);
    memcpy((char *)pReceiver->lprime, (char*)g_lPrime[index], LW95A1_HDCP_SIZE_LPRIME_8);
    memcpy((char *)&pReceiver->rrx, g_rrx[index], LW95A1_HDCP_SIZE_RRX_8);

    pReceiver->bIsRepeater = g_bIsRepeater[index];
    pReceiver->hdcpVer = version;
    pReceiver->bRecvPreComputeSupport = computeSupport;
    pReceiver->index = index;

    return OK;
}

/*!
 * FreeReceiver
 *
 * Cleans up receiver state
 */
RC Class95a1Sec2Test::FreeReceiver(HdcpReceiver *pReceiver)
{
    FreeMem(&(pReceiver->m_certMemDesc));
    return OK;
}

#define HDCP2X_TEST_RECEIVER_1_INDEX      0
#define HDCP2X_TEST_RECEIVER_2_INDEX      1
#define HDCP2X_COMPUTE_SUPPORT_ABSENT     0
#define HDCP2X_COMPUTE_SUPPORT_PRESENT    1
//------------------------------------------------------------------------
RC Class95a1Sec2Test::Run()
{
    RC rc = OK;
    LwU32 updmask = 0;
    CHECK_RC(ReadCaps());

    CHECK_RC(Init());
    CHECK_RC(SetupReceiver(&(m_receivers[0]), HDCP2X_TEST_RECEIVER_2_INDEX, HCLASS(VERSION_21), HDCP2X_COMPUTE_SUPPORT_PRESENT));
    CHECK_RC(CreateSession(&(m_receivers[0])));

    if (m_hdcpMode == HDCP_MODE_DEBUG)
    {

        if (m_hdcpNegative)
            CHECK_RC(VerifyCert(&(m_receivers[0]), 1));
        CHECK_RC(VerifyCert(&(m_receivers[0]), 0));

        if (m_hdcpNegative)
            CHECK_RC(RevocationCheck(&(m_receivers[0]), 1));
        CHECK_RC(RevocationCheck(&(m_receivers[0]), 0));

        CHECK_RC(GenerateEkm(&(m_receivers[0]), 0));

        if (m_hdcpNegative)
            CHECK_RC(VerifyHprime(&(m_receivers[0]), 1));
        CHECK_RC(VerifyHprime(&(m_receivers[0]), 0));

        updmask = (1 << HCLASS(UPDATE_SESSION_MASK_RRX_PRESENT)) |
                  (1 << HCLASS(UPDATE_SESSION_MASK_VERSION_PRESENT)) |
                  (1 << HCLASS(UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT));
        CHECK_RC(UpdateSession(&(m_receivers[0]), updmask));

        CHECK_RC(GenerateLcInit(&(m_receivers[0]), 0));
        memcpy((void*)m_receivers[0].lprime, (void*)&(m_receivers[0].lprime[HCLASS(SIZE_LPRIME_64)/2]),
                                     HCLASS(SIZE_LPRIME_8)/2);

        CHECK_RC(GetRttChallenge(&(m_receivers[0]), 0));

        if (m_hdcpNegative)
            CHECK_RC(VerifyLprime(&(m_receivers[0]), 1));
        CHECK_RC(VerifyLprime(&(m_receivers[0]), 0));

        CHECK_RC(GenerateSkeInit(&(m_receivers[0]), 0));
        //    CHECK_RC(VerifyVprime(&(m_receivers[0]), 0));
        CHECK_RC(SessionCtrl(&(m_receivers[0]), LW95A1_HDCP_SESSION_CTRL_FLAG_ACTIVATE));
        CHECK_RC(Encrypt(&(m_receivers[0]), g_content, g_contentEnc, g_contentSize));
    }
    else if (m_hdcpMode == HDCP_MODE_DEBUG_MIN)
    {
        CHECK_RC(Encrypt(&(m_receivers[0]), g_content, g_contentEnc, g_contentSize));
    }
    else
    {
        Printf(Tee::PriHigh, "HDCP: Invalid exelwtion mode\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2Test::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeMem(&m_semMemDesc);
    FreeMem(&m_resMemDesc);
    FreeMem(&m_sbMemDesc);
    FreeMem(&m_dcpMemDesc);
    FreeMem(&m_srmMemDesc);
    FreeReceiver(&(m_receivers[0]));
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2Test object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2Test, RmTest,
                 "Class95a1Sec2 RM test.");
CLASS_PROP_READWRITE(Class95a1Sec2Test, hdcpMode, int,
                     "HdcpMode, default= 2");
CLASS_PROP_READWRITE(Class95a1Sec2Test, hdcpNegative, bool,
                     "HdcpNegative, default= false");

