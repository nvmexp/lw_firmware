/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "maxwell/gm107/dev_fuse.h"
#include "gpu/tests/rmtest.h"


#include "random.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "rmt_cl95a1sec2prsamples.h"
#include "rmt_cl95a1sec2pr44samples.h"

#define PR_METHOD(mid)                LW95A1_PR_FUNCTION_ID_DRM_TEE_##mid
#define IS_METHOD(mid)                (m_mid == PR_METHOD(mid)) || (m_mid == PR_METHOD(All))
#define GET_RESP_BUFFER_SIZE(pResp)   (pResp[22] * 0x100 + pResp[23])
typedef struct _pr_mthd_prop
{
    const char *pName;
    LwU8  *pReqMsg;
    LwU32 fid;
    LwU32 reqSize;
    LwU32 resSize;
    LwU32 resSizeExpected;
} PR_MTHD_PROP;

/*!
 * Class95a1Sec2PrTest
 *
 */
class Class95a1Sec2PrTest: public RmTest
{
public:
    Class95a1Sec2PrTest();
    virtual ~Class95a1Sec2PrTest();
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
    virtual RC RunMethod(LwU32 fid);
    virtual RC GetMthdProp(LwU32 fid, PR_MTHD_PROP *);
    virtual RC GetMthdProp44(LwU32 fid, PR_MTHD_PROP *);
    virtual RC ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd, const char *pStr,
                                       LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size);
    virtual RC ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd);

    SETGET_PROP(mid, LwU32);
    inline bool IsGA102orBetter();

private:
    Sec2MemDesc m_semMemDesc;

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;
    LwU32        m_mid;
    UINT32       m_gpuId;
    GpuSubdevice *m_pSubdev;
};

inline bool Class95a1Sec2PrTest::IsGA102orBetter()
{
    return m_gpuId >= Gpu::GA102;
}

/*!
 * Construction and desctructors
 */
Class95a1Sec2PrTest::Class95a1Sec2PrTest() :
    m_hObj(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT),
    m_mid(0),
    m_pSubdev(nullptr)
{
    SetName("Class95a1Sec2PrTest");
}

Class95a1Sec2PrTest::~Class95a1Sec2PrTest()
{
}

/*!
 * IsTestSupported
 *
 * Need class 95A1 to be supported
 */
string Class95a1Sec2PrTest::IsTestSupported()
{
    m_gpuId = GetBoundGpuSubdevice()->DeviceId();

    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

/*!
 * PrintBuf
 *
 * Helped routing to print buffers in hex format
 */
RC Class95a1Sec2PrTest::PrintBuf(const char *pStr, LwU8 *pBuf, LwU32 size)
{
    Printf(Tee::PriHigh, "PR: %s\n", pStr);
    for (LwU32 i=0; i < size; i++)
    {
        if (i && (!(i % 16)))
            Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "%02x ",(LwU8)pBuf[i]);
    }
    Printf(Tee::PriHigh, "\n");
    return OK;
}

RC Class95a1Sec2PrTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
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
RC Class95a1Sec2PrTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

/*!
 * Setup
 *
 * Sets up the timeout - Setting to max for emulation cases
 * Allocates Channel and object
 * Allocates necessary memory required for PR
 * Copies the necessary constants into allocated memory
 */
RC Class95a1Sec2PrTest::Setup()
{

    LwRmPtr      pLwRm;
    RC           rc;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
    m_pCh->SetObject(0, m_hObj);

    // Allocate Memory for Main class. Allocate big enough to support 1K/2K for both READ/GEN mode
    AllocateMem(&m_semMemDesc, 0x4);

    // Set default method ID to all
    m_mid = PR_METHOD(All);

    return rc;
}

/*!
 * SendSemaMethod
 *
 * Pushes semaphore methods into channel push buffer.
 * Uses 0x12345678 as the payload
 */
RC Class95a1Sec2PrTest::SendSemaMethod()
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
RC Class95a1Sec2PrTest::PollSemaMethod()
{
    RC              rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)m_semMemDesc.pCpuAddrSysMem;
    Printf(Tee::PriHigh, "PR: Polling started for pAddr %p Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "PR: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

/*!
 * StartMethodStream
 *
 * Does startup tasks before starting a method stream
 * Also pushes needed methods which are always used
 */
RC Class95a1Sec2PrTest::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC              rc = OK;

    Printf(Tee::PriHigh, "PR: Starting method stream for %s \n", pId);
    MEM_WR32(m_semMemDesc.pCpuAddrSysMem, 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_PR);
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

/*!
 * EndMethodStream
 *
 * Flushes the channel and polls on semaphore
 */
RC Class95a1Sec2PrTest::EndMethodStream(const char *pId)
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
        Printf(Tee::PriHigh, "PR: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "PR: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "PR: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

/*!
 * ErrorCheck
 *
 * Helper routing to check the retCode with the expected retCode and prints
 * error message based on the comparison
 */
RC Class95a1Sec2PrTest::ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd)
{
    RC   rc = OK;

    if (result != expected)
    {
        Printf(Tee::PriHigh, "PR: ERROR: '%s' FAILED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, (unsigned int) result, (unsigned int) expected);
        rc = ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "PR: '%s' PASSED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, result, expected);
    }
    return rc;
}

/*!
 * ErrorCheckWithComparison
 *
 * Helper routing to check the retcode and also compare the buffer with the expected buffer.
 */
RC Class95a1Sec2PrTest::ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd,
                                     const char *pStr,  LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size)
{
    RC rc = OK;
    CHECK_RC(ErrorCheck(expected, result, pMthd));

    //PrintBuf(pStr, pLwrrent, size);
    if (memcmp((char*)pLwrrent, (char*)pSrc, size))
    {
        rc = ERROR;
        Printf(Tee::PriHigh, "PR: ERROR: '%s %s' comparison FAILED\n", pMthd, pStr);
    }
    else
    {
        Printf(Tee::PriHigh, "PR: '%s %s' comparison PASSED\n", pMthd, pStr);
    }
    return rc;

}

RC Class95a1Sec2PrTest::GetMthdProp(LwU32 mid, PR_MTHD_PROP *pProp)
{
    pProp->fid = mid;
    switch (mid)
    {
        case PR_METHOD(BASE_AllocTEEContext):
            pProp->reqSize = g_DrmAllocTeeCtxReqMsgSize;
            pProp->resSize = g_DrmAllocTeeCtxRspMsgSize;
            pProp->resSizeExpected = g_DrmAllocTeeCtxRspMsgSizeExpected;
            pProp->pName = g_pDrmAllocTeeCtxName;
            pProp->pReqMsg = g_DrmAllocTeeCtxReqMsg;
            break;

        case PR_METHOD(LPROV_GenerateDeviceKeys):
            pProp->reqSize = g_DrmGenDevKeysReqMsgSize;
            pProp->resSize = g_DrmGenDevKeysRspMsgSize;
            pProp->resSizeExpected = g_DrmGenDevKeysRspMsgSizeExpected;
            pProp->pName = g_pDrmGenDevKeysName;
            pProp->pReqMsg = g_DrmGenDevKeysReqMsg;
            break;

        case PR_METHOD(SAMPLEPROT_PrepareSampleProtectionKey):
            pProp->reqSize = g_DrmPreSplProtKeyReqMsgSize;
            pProp->resSize = g_DrmPreSplProtKeyRspMsgSize;
            pProp->resSizeExpected = g_DrmPreSplProtKeyRspMsgSizeExpected;
            pProp->pName = g_pDrmPreSplProtkeyName;
            pProp->pReqMsg = g_DrmPreSplProtKeyReqMsg;
            break;

        case PR_METHOD(REVOCATION_IngestRevocationInfo):
            pProp->reqSize = g_DrmIngestRevInfoReqMsgSize;
            pProp->resSize = g_DrmIngestRevInfoRspMsgSize;
            pProp->resSizeExpected = g_DrmIngestRevInfoRspMsgSizeExpected;
            pProp->pName = g_pDrmIngestRevInfoName;
            pProp->pReqMsg = g_DrmIngestRevInfoReqMsg;
            break;

        case PR_METHOD(BASE_GenerateNonce):
            pProp->reqSize = g_DrmGenerateNonceReqMsgSize;
            pProp->resSize = g_DrmGenerateNonceRspMsgSize;
            pProp->resSizeExpected = g_DrmGenerateNonceRspMsgSizeExpected;
            pProp->pName = g_pDrmGenerateNonceName;
            pProp->pReqMsg = g_DrmGenerateNonceReqMsg;
            break;

        case PR_METHOD(BASE_CheckDeviceKeys):
            pProp->reqSize = g_DrmChkDevKeysReqMsgSize;
            pProp->resSize = g_DrmChkDevKeysRspMsgSize;
            pProp->resSizeExpected = g_DrmChkDevKeysRspMsgSizeExpected;
            pProp->pName = g_pDrmChkDevKeysName;
            pProp->pReqMsg = g_DrmChkDevKeysReqMsg;
            break;

        case PR_METHOD(SIGN_SignHash):
            pProp->reqSize = g_DrmSignHashReqMsgSize;
            pProp->resSize = g_DrmSignHashRspMsgSize;
            pProp->resSizeExpected = g_DrmSignHashRspMsgSizeExpected;
            pProp->pName = g_pDrmSignhashName;
            pProp->pReqMsg = g_DrmSignHashReqMsg;
            break;

        case PR_METHOD(BASE_GetSystemTime):
            pProp->reqSize = g_DrmGetSysTimeReqMsgSize;
            pProp->resSize = g_DrmGetSysTimeRspMsgSize;
            pProp->resSizeExpected = g_DrmGetSysTimeRspMsgSizeExpected;
            pProp->pName = g_pDrmGetSysTimeName;
            pProp->pReqMsg = g_DrmGetSysTimeReqMsg;
            break;

        case PR_METHOD(BASE_FreeTEEContext):
            pProp->reqSize = g_DrmFreeTeeCtxReqMsgSize;
            pProp->resSize = g_DrmFreeTeeCtxRspMsgSize;
            pProp->resSizeExpected = g_DrmFreeTeeCtxRspMsgSizeExpected;
            pProp->pName = g_pDrmFreeTeeCtxName;
            pProp->pReqMsg = g_DrmFreeTeeCtxReqMsg;
            break;

        case PR_METHOD(AES128CTR_PrepareToDecrypt):
            pProp->reqSize = g_DrmPrepToDecryptReqMsgSize;
            pProp->resSize = g_DrmPrepToDecryptRspMsgSize;
            pProp->resSizeExpected = g_DrmPrepToDecryptRspMsgSizeExpected;
            pProp->pName = g_pDrmPrepToDecryptName;
            pProp->pReqMsg = g_DrmPrepToDecryptReqMsg;
            break;

        case PR_METHOD(LICPREP_PackageKey):
            pProp->reqSize = g_DrmLicPrepPackKeyReqMsgSize;
            pProp->resSize = g_DrmLicPrepPackKeyRspMsgSize;
            pProp->resSizeExpected = g_DrmLicPrepPackKeyRspMsgSizeExpected;
            pProp->pName = g_pDrmLicPrepPackName;
            pProp->pReqMsg = g_DrmLicPrepPackKeyReqMsg;
            break;

        case PR_METHOD(DOM_PackageKeys):
            pProp->reqSize = g_DrmDomPackageKeysReqMsgSize;
            pProp->resSize = g_DrmDomPackageKeysRspMsgSize;
            pProp->resSizeExpected = g_DrmDomPackageKeysRspMsgSizeExpected;
            pProp->pName = g_pDrmDomPackageKeysName;
            pProp->pReqMsg = g_DrmDomPackageKeysReqMsg;
            break;

        case PR_METHOD(AES128CTR_CreateOEMBlobFromCDKB):
            pProp->reqSize = g_DrmCreateOemBlobReqMsgSize;
            pProp->resSize = g_DrmCreateOemBlobRspMsgSize;
            pProp->resSizeExpected = g_DrmCreateOemBlobRspMsgSizeExpected;
            pProp->pName = g_pDrmCreateOemBlobName;
            pProp->pReqMsg = g_DrmCreateOemBlobReqMsg;
            break;

        case PR_METHOD(BASE_SignDataWithSelwreStoreKey):
            pProp->reqSize = g_DrmSignWithSelwreStoreKeyReqMsgSize;
            pProp->resSize = g_DrmSignWithSelwreStoreKeyRspMsgSize;
            pProp->resSizeExpected = g_DrmSignWithSelwreStoreKeyRspMsgSizeExpected;
            pProp->pName = g_pDrmSignWithSelwreStoreKeyName;
            pProp->pReqMsg = g_DrmSignWithSelwreStoreKeyReqMsg;
            break;

        case PR_METHOD(BASE_GetDebugInformation):
            pProp->reqSize = g_DrmGetDebugInfoReqMsgSize;
            pProp->resSize = g_DrmGetDebugInfoRspMsgSize;
            pProp->resSizeExpected = g_DrmGetDebugInfoRspMsgSizeExpected;
            pProp->pName = g_pDrmGetDebugInfoName;
            pProp->pReqMsg = g_DrmGetDebugInfoReqMsg;
            break;

        default:
            Printf(Tee::PriHigh, "PR: Invalid method id while selecting property 0x%0x\n", mid);
            return ERROR;
    }

    return OK;
}

RC Class95a1Sec2PrTest::GetMthdProp44(LwU32 mid, PR_MTHD_PROP *pProp)
{
    pProp->fid = mid;
    switch (mid)
    {
    case PR_METHOD(BASE_AllocTEEContext):
        pProp->reqSize = g_DrmAllocTeeCtxReqMsgSize44;
        pProp->resSize = g_DrmAllocTeeCtxRspMsgSize44;
        pProp->resSizeExpected = g_DrmAllocTeeCtxRspMsgSizeExpected44;
        pProp->pName = g_pDrmAllocTeeCtxName44;
        pProp->pReqMsg = g_DrmAllocTeeCtxReqMsg44;
        break;

    case PR_METHOD(LPROV_GenerateDeviceKeys):
        pProp->reqSize = g_DrmGenDevKeysReqMsgSize44;
        pProp->resSize = g_DrmGenDevKeysRspMsgSize44;
        pProp->resSizeExpected = g_DrmGenDevKeysRspMsgSizeExpected44;
        pProp->pName = g_pDrmGenDevKeysName44;
        pProp->pReqMsg = g_DrmGenDevKeysReqMsg44;
        break;

    case PR_METHOD(SAMPLEPROT_PrepareSampleProtectionKey):
        pProp->reqSize = g_DrmPreSplProtKeyReqMsgSize44;
        pProp->resSize = g_DrmPreSplProtKeyRspMsgSize44;
        pProp->resSizeExpected = g_DrmPreSplProtKeyRspMsgSizeExpected44;
        pProp->pName = g_pDrmPreSplProtkeyName44;
        pProp->pReqMsg = g_DrmPreSplProtKeyReqMsg44;
        break;

    case PR_METHOD(REVOCATION_IngestRevocationInfo):
        pProp->reqSize = g_DrmIngestRevInfoReqMsgSize44;
        pProp->resSize = g_DrmIngestRevInfoRspMsgSize44;
        pProp->resSizeExpected = g_DrmIngestRevInfoRspMsgSizeExpected44;
        pProp->pName = g_pDrmIngestRevInfoName44;
        pProp->pReqMsg = g_DrmIngestRevInfoReqMsg44;
        break;

    case PR_METHOD(BASE_GenerateNonce):
        pProp->reqSize = g_DrmGenerateNonceReqMsgSize44;
        pProp->resSize = g_DrmGenerateNonceRspMsgSize44;
        pProp->resSizeExpected = g_DrmGenerateNonceRspMsgSizeExpected44;
        pProp->pName = g_pDrmGenerateNonceName44;
        pProp->pReqMsg = g_DrmGenerateNonceReqMsg44;
        break;

    case PR_METHOD(BASE_CheckDeviceKeys):
        pProp->reqSize = g_DrmChkDevKeysReqMsgSize44;
        pProp->resSize = g_DrmChkDevKeysRspMsgSize44;
        pProp->resSizeExpected = g_DrmChkDevKeysRspMsgSizeExpected44;
        pProp->pName = g_pDrmChkDevKeysName44;
        pProp->pReqMsg = g_DrmChkDevKeysReqMsg44;
        break;

    case PR_METHOD(SIGN_SignHash):
        pProp->reqSize = g_DrmSignHashReqMsgSize44;
        pProp->resSize = g_DrmSignHashRspMsgSize44;
        pProp->resSizeExpected = g_DrmSignHashRspMsgSizeExpected44;
        pProp->pName = g_pDrmSignhashName44;
        pProp->pReqMsg = g_DrmSignHashReqMsg44;
        break;

    case PR_METHOD(BASE_GetSystemTime):
        pProp->reqSize = g_DrmGetSysTimeReqMsgSize44;
        pProp->resSize = g_DrmGetSysTimeRspMsgSize44;
        pProp->resSizeExpected = g_DrmGetSysTimeRspMsgSizeExpected44;
        pProp->pName = g_pDrmGetSysTimeName44;
        pProp->pReqMsg = g_DrmGetSysTimeReqMsg44;
        break;

    case PR_METHOD(BASE_FreeTEEContext):
        pProp->reqSize = g_DrmFreeTeeCtxReqMsgSize44;
        pProp->resSize = g_DrmFreeTeeCtxRspMsgSize44;
        pProp->resSizeExpected = g_DrmFreeTeeCtxRspMsgSizeExpected44;
        pProp->pName = g_pDrmFreeTeeCtxName44;
        pProp->pReqMsg = g_DrmFreeTeeCtxReqMsg44;
        break;

    case PR_METHOD(AES128CTR_PrepareToDecrypt):
        pProp->reqSize = g_DrmPrepToDecryptReqMsgSize44;
        pProp->resSize = g_DrmPrepToDecryptRspMsgSize44;
        pProp->resSizeExpected = g_DrmPrepToDecryptRspMsgSizeExpected44;
        pProp->pName = g_pDrmPrepToDecryptName44;
        pProp->pReqMsg = g_DrmPrepToDecryptReqMsg44;
        break;

    case PR_METHOD(LICPREP_PackageKey):
        pProp->reqSize = g_DrmLicPrepPackKeyReqMsgSize44;
        pProp->resSize = g_DrmLicPrepPackKeyRspMsgSize44;
        pProp->resSizeExpected = g_DrmLicPrepPackKeyRspMsgSizeExpected44;
        pProp->pName = g_pDrmLicPrepPackName44;
        pProp->pReqMsg = g_DrmLicPrepPackKeyReqMsg44;
        break;

    case PR_METHOD(DOM_PackageKeys):
        pProp->reqSize = g_DrmDomPackageKeysReqMsgSize44;
        pProp->resSize = g_DrmDomPackageKeysRspMsgSize44;
        pProp->resSizeExpected = g_DrmDomPackageKeysRspMsgSizeExpected44;
        pProp->pName = g_pDrmDomPackageKeysName44;
        pProp->pReqMsg = g_DrmDomPackageKeysReqMsg44;
        break;

    case PR_METHOD(AES128CTR_CreateOEMBlobFromCDKB):
        pProp->reqSize = g_DrmCreateOemBlobReqMsgSize44;
        pProp->resSize = g_DrmCreateOemBlobRspMsgSize44;
        pProp->resSizeExpected = g_DrmCreateOemBlobRspMsgSizeExpected44;
        pProp->pName = g_pDrmCreateOemBlobName44;
        pProp->pReqMsg = g_DrmCreateOemBlobReqMsg44;
        break;

    case PR_METHOD(BASE_SignDataWithSelwreStoreKey):
        pProp->reqSize = g_DrmSignWithSelwreStoreKeyReqMsgSize44;
        pProp->resSize = g_DrmSignWithSelwreStoreKeyRspMsgSize44;
        pProp->resSizeExpected = g_DrmSignWithSelwreStoreKeyRspMsgSizeExpected44;
        pProp->pName = g_pDrmSignWithSelwreStoreKeyName44;
        pProp->pReqMsg = g_DrmSignWithSelwreStoreKeyReqMsg44;
        break;

    case PR_METHOD(BASE_GetDebugInformation):
        pProp->reqSize = g_DrmGetDebugInfoReqMsgSize44;
        pProp->resSize = g_DrmGetDebugInfoRspMsgSize44;
        pProp->resSizeExpected = g_DrmGetDebugInfoRspMsgSizeExpected44;
        pProp->pName = g_pDrmGetDebugInfoName44;
        pProp->pReqMsg = g_DrmGetDebugInfoReqMsg44;
        break;

    default:
        Printf(Tee::PriHigh, "PR: Invalid method id while selecting property 0x%0x\n", mid);
        return ERROR;
    }

    return OK;
}


/*!
 * Runmethod
 */
RC Class95a1Sec2PrTest::RunMethod(LwU32 fid)
{
    LwRmPtr       pLwRm;
    RC            rc         = OK;
    LwU32         rspMsgSize = 0;
    LwU8          *pRspBuf   = NULL;
    pr_stat_param *pStat     = NULL;
    LwU64         time = 0;
    PR_MTHD_PROP  mthdProp;

    if (IsGA102orBetter())
    {
        if ((rc = GetMthdProp44(fid, &mthdProp)) != OK)
        {
            return rc;
        }
    }
    else
    {
        if ((rc = GetMthdProp(fid, &mthdProp)) != OK)
        {
            return rc;
        }
    }

    Printf(Tee::PriHigh,"*******************************************************************\n");
    Printf(Tee::PriHigh, "PR: Running method %s (method ID %d)\n", mthdProp.pName, mthdProp.fid);
    Sec2MemDesc reqMemDesc;
    Sec2MemDesc resMemDesc;
    Sec2MemDesc statMemDesc;

    AllocateMem(&reqMemDesc, mthdProp.reqSize);
    AllocateMem(&resMemDesc, mthdProp.resSize);
    AllocateMem(&statMemDesc, 0x100); //Assuming 0x100 for now
    pRspBuf = (LwU8 *)resMemDesc.pCpuAddrSysMem;
    pStat = (pr_stat_param *)statMemDesc.pCpuAddrSysMem;

    // Copy request buffer
    memcpy((char*)reqMemDesc.pCpuAddrSysMem, mthdProp.pReqMsg, mthdProp.reqSize);
    Printf(Tee::PriHigh, "PR: Request size 0x%0x\n", mthdProp.reqSize);
    PrintBuf("Request: ", (LwU8 *)(reqMemDesc.pCpuAddrSysMem), mthdProp.reqSize);

    //memset
    memset(resMemDesc.pCpuAddrSysMem, 0, mthdProp.resSize);
    memset(statMemDesc.pCpuAddrSysMem, 0, 0x100);

    CHECK_RC(StartMethodStream(mthdProp.pName, 1));
    m_pCh->Write(0, LW95A1_PR_REQUEST_MESSAGE, (LwU32)(reqMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_PR_REQUEST_MESSAGE_SIZE, mthdProp.reqSize);
    m_pCh->Write(0, LW95A1_PR_RESPONSE_MESSAGE, (LwU32)(resMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_PR_RESPONSE_MESSAGE_SIZE, mthdProp.resSize);
    m_pCh->Write(0, LW95A1_PR_FUNCTION_ID, mthdProp.fid);
    m_pCh->Write(0, LW95A1_PR_STAT, (LwU32)(statMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream(mthdProp.pName));

    // reqMsgSize will be 8 for FreeTeeCtx which won't have rspSize returned.
    if (mthdProp.resSize > 8)
    {
        rspMsgSize = GET_RESP_BUFFER_SIZE(pRspBuf);
    }

    Printf(Tee::PriHigh, "PR: Response buffer size returned 0x%0x\n", rspMsgSize);
    PrintBuf("Response: ",pRspBuf, rspMsgSize);
    Printf(Tee::PriHigh, "\nPR: Stats\n");
    Printf(Tee::PriHigh, "PR: Total overlays loaded   : %u\n", pStat->totalOverlayLoads);
    Printf(Tee::PriHigh, "PR: Total overlays unloaded : %u\n", pStat->totalOverlayUnloads);
    Printf(Tee::PriHigh, "PR: Total code loaded       : %u\n", pStat->totalCodeLoaded);
    time = (((LwU64)pStat->timeElapsedHi) << 32) | (pStat->timeElapsedLo);
    time = (time / (1000 * 1000)); // Milliseconds
    if (!time)
        time = 1;
    Printf(Tee::PriHigh, "PR: Time (milliseconds)     : %llu\n", time);
    Printf(Tee::PriHigh, "\n");

    if (rspMsgSize != mthdProp.resSizeExpected)
    {
        Printf(Tee::PriHigh, "PR: ERROR - Rsp size 0x%0x doesnt match expected 0x%0x\n", rspMsgSize, mthdProp.resSizeExpected);
        rc = ERROR;
    }

    FreeMem(&reqMemDesc);
    FreeMem(&resMemDesc);

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2PrTest::Run()
{
    RC rc = OK;

    if (IS_METHOD(BASE_AllocTEEContext))
        CHECK_RC(RunMethod(PR_METHOD(BASE_AllocTEEContext)));

    if (IS_METHOD(BASE_GenerateNonce))
        CHECK_RC(RunMethod(PR_METHOD(BASE_GenerateNonce)));

    if (IS_METHOD(LPROV_GenerateDeviceKeys))
        CHECK_RC(RunMethod(PR_METHOD(LPROV_GenerateDeviceKeys)));

    if (IS_METHOD(SAMPLEPROT_PrepareSampleProtectionKey))
        CHECK_RC(RunMethod(PR_METHOD(SAMPLEPROT_PrepareSampleProtectionKey)));

    if (IS_METHOD(REVOCATION_IngestRevocationInfo))
        CHECK_RC(RunMethod(PR_METHOD(REVOCATION_IngestRevocationInfo)));

    if (IS_METHOD(BASE_GenerateNonce))
        CHECK_RC(RunMethod(PR_METHOD(BASE_GenerateNonce)));

    if (IS_METHOD(BASE_CheckDeviceKeys))
        CHECK_RC(RunMethod(PR_METHOD(BASE_CheckDeviceKeys)));

    if (IS_METHOD(SIGN_SignHash))
        CHECK_RC(RunMethod(PR_METHOD(SIGN_SignHash)));

    if (IS_METHOD(BASE_GetSystemTime))
        CHECK_RC(RunMethod(PR_METHOD(BASE_GetSystemTime)));

    if (IS_METHOD(BASE_FreeTEEContext))
        CHECK_RC(RunMethod(PR_METHOD(BASE_FreeTEEContext)));

    if (IS_METHOD(AES128CTR_PrepareToDecrypt))
        CHECK_RC(RunMethod(PR_METHOD(AES128CTR_PrepareToDecrypt)));

    if (IS_METHOD(LICPREP_PackageKey))
        CHECK_RC(RunMethod(PR_METHOD(LICPREP_PackageKey)));

    if (IS_METHOD(DOM_PackageKeys))
        CHECK_RC(RunMethod(PR_METHOD(DOM_PackageKeys)));

    if (IS_METHOD(AES128CTR_CreateOEMBlobFromCDKB))
      CHECK_RC(RunMethod(PR_METHOD(AES128CTR_CreateOEMBlobFromCDKB)));

    if (IS_METHOD(BASE_SignDataWithSelwreStoreKey))
        CHECK_RC(RunMethod(PR_METHOD(BASE_SignDataWithSelwreStoreKey)));

    if (IS_METHOD(BASE_GetDebugInformation))
        CHECK_RC(RunMethod(PR_METHOD(BASE_GetDebugInformation)));

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2PrTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeMem(&m_semMemDesc);
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2PrTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2PrTest, RmTest,
                 "Class95a1Sec2PrTest RM test.");
CLASS_PROP_READWRITE(Class95a1Sec2PrTest, mid, int,
                     "Method ID, default= 100 (all)");

