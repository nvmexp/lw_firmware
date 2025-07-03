/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \rmt_acrtest.cpp
//! \Add Rmtest to verify ACR programming and to ensure ACM is setup properly
//! \Test includes verification for creation of Acr region and
//! \access to ACR region by Non-Secure(NS) and Light-Secure(LS) clients.
//! \While runnning this test pass argument acr_size
//!

#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "lwRmReg.h"
#include "core/include/registry.h"
#include "gpu/include/gpusbdev.h"
#include "lwstatus.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "ctrl/ctrl2080.h"
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER - For GM20X_and_later
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "ctrl/ctrl0080.h"
#include "core/utility/errloggr.h"
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER - For GM20X_and_later
#include "core/include/memcheck.h"

#define MEM_HANDLE 0xba40001

#define PRIV_LEVEL(x)      BIT(x)

typedef enum
{
    LSF_BSP,
    LSF_MSENC0,
    LSF_MSENC1,
} LSF_FALCON;

typedef struct
{
    Channel     *pCh;
    void        *pAlloc;
    UINT32      *m_cpuAddr;
    LwU32       subCh;
    LwU32       engineType;
    LwU64       m_gpuAddr;
    UINT32      falconBase;
} TestData;

const char* errString[3] = {
                            "MMU Fault : ENGINE_LWDEC FAULT_INFO_TYPE_REGION_VIOLATION",
                            "MMU Fault : ENGINE_LWENC0 FAULT_INFO_TYPE_REGION_VIOLATION",
                            "MMU Fault : ENGINE_LWENC1 FAULT_INFO_TYPE_REGION_VIOLATION"
                           };

class AcrTest : public RmTest
{
public:
    AcrTest();
    virtual ~AcrTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC VerifyNSAccess();
    RC VerifyLSAccess();
    RC WaitAndVerify(TestData *);
    RC AllocTestData(TestData *&, LSF_FALCON);
    void LwdecFalconWrite(TestData *);
    void LwencFalconWrite(TestData *);
    void FreeTestData(TestData *&);

private:
    LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS acrQueryRegionParam;

    LwRmPtr           pLwRm;

    FLOAT64           m_TimeoutMs;

    GpuSubdevice      *m_pSubdevice;
    GpuDevice         *pGpuDev;

    vector<TestData*> m_TestData;

    Surface2D        *m_pAcrSurf;

    LwBool nsFault;
};

//! \brief AcrTest constructor
//! \sa setup
//------------------------------------------------------------------------------

AcrTest::AcrTest()
{
    nsFault      = false;

    m_pSubdevice = nullptr;
    pGpuDev      = nullptr;

    m_pAcrSurf   = NULL;

    SetName("AcrTest");
}

//! \brief AcrTest destructor
//!
//! \sa cleanup
//------------------------------------------------------------------------------

AcrTest::~AcrTest()
{
    //nothing to be done
}

//! \brief Check for Test Support
//! \This function is to ensure that the test is supported only on GM20X and later chips
//! \return string
//------------------------------------------------------------------------------

string AcrTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsGM20XorBetter(chipName))
    {
        return RUN_RMTEST_TRUE;
    }
    return "Supported only on GM20X+ chips";
}

//! \brief Test Setup Function
//! \This setup is done for Verifying LS client access
//! \Here, Acr region is mapped to BAR1 for verification
//! \All the available engines(LS clients) are fetched
//! \and then data is allocated to each of them
//! \return RC
//------------------------------------------------------------------------------

RC AcrTest::Setup()
{
    RC       rc          = OK;
    UINT32   data;
    TestData *pNewTest;
    UINT32    i, numRegions = 0;
    m_pSubdevice = GetBoundGpuSubdevice();
    pGpuDev      = GetBoundGpuDevice();
    Gpu::LwDeviceId chipName = m_pSubdevice->DeviceId();

    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS  engineParams = {0};

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        LW2080_CTRL_CMD_GPU_GET_ENGINES_V2, &engineParams, sizeof(engineParams)));

    //
    // Parsing RMCreateAcrRegion1 regkey and checking read and write mask
    // If LSB of readmask or writemask is set then return error
    // And if parsing fails then also return error
    // User is expected to fill the Reg key through command line arguments
    // User can create ACR region using regkey
    //
    if (OK == Registry::Read("ResourceManager", "RMCreateAcrRegion1", &data))
    {
        LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS regionParams;

        Printf(Tee::PriHigh,"%s : AcrRegion = 0x%u PASS\n", __FUNCTION__, data);

        acrQueryRegionParam.clientReq.clientId     = LW2080_CTRL_CMD_FB_ACR_CLIENT_ID;
        acrQueryRegionParam.clientReq.regionSize   = DRF_VAL(_REG_STR_RM,
                                                             _CREATE_ACR_REGION,
                                                             _SIZE, data);
        acrQueryRegionParam.clientReq.reqReadMask  = (0x8) | DRF_VAL(_REG_STR_RM,
                                                                     _CREATE_ACR_REGION,
                                                                     _READMASK, data);
        acrQueryRegionParam.clientReq.reqWriteMask = (0x8) | DRF_VAL(_REG_STR_RM,
                                                                     _CREATE_ACR_REGION,
                                                                     _WRITEMASK, data);

        Printf(Tee::PriHigh,"ClientID: %d \tDatasize: %d \tReadMask: %d \tWriteMask: %d \n",
                acrQueryRegionParam.clientReq.clientId, acrQueryRegionParam.clientReq.regionSize,
                acrQueryRegionParam.clientReq.reqReadMask,
                acrQueryRegionParam.clientReq.reqWriteMask);

        m_pAcrSurf = new Surface2D();
        m_pAcrSurf->SetForceSizeAlloc(true);
        m_pAcrSurf->SetArrayPitch(1);
        m_pAcrSurf->SetArraySize(sizeof(UINT32) * engineParams.engineCount);
        m_pAcrSurf->SetColorFormat(ColorUtils::VOID32);
        m_pAcrSurf->SetAddressModel(Memory::Paging);
        m_pAcrSurf->SetLayout(Surface2D::Pitch);
        m_pAcrSurf->SetLocation(Memory::Fb);

        regionParams.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_REGION_PROPERTY;
        regionParams.acrRegionIdProp.regionId = 0;
        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
                                        &regionParams, sizeof(regionParams)));
        numRegions = regionParams.acrRegionIdProp.regionId;

        for (i = 1; i <= numRegions; i++)
        {
            regionParams.acrRegionIdProp.regionId = i;
            CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
                                    &regionParams, sizeof(regionParams)));
            if (regionParams.acrRegionIdProp.readMask == acrQueryRegionParam.clientReq.reqReadMask &&
                regionParams.acrRegionIdProp.writeMask == acrQueryRegionParam.clientReq.reqWriteMask &&
                regionParams.acrRegionIdProp.clientMask & BIT(acrQueryRegionParam.clientReq.clientId) &&
                regionParams.acrRegionIdProp.regionSize >= acrQueryRegionParam.clientReq.regionSize)
            {
                if (i == 1)
                {
                    m_pAcrSurf->SetAcrRegion1(true);
                }
                else
                {
                    m_pAcrSurf->SetAcrRegion2(true);
                }
                break;
            }
        }

        if (i > numRegions)
        {
            return RC::LWRM_ILWALID_REQUEST;
        }

        CHECK_RC(m_pAcrSurf->Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_pAcrSurf->Map());
    }
    else
    {
        Printf(Tee::PriHigh, "%s Specify arguments -acr[1|2]_size for setting"
                                " the reg key while running the test\n", __FUNCTION__);
        return RC::LWRM_ILWALID_REQUEST;
    }

    // Allocate Testdata to all engines
    for ( i = 0; i < engineParams.engineCount; i++ )
    {
        switch (engineParams.engineList[i])
        {
            case LW2080_ENGINE_TYPE_BSP:
                CHECK_RC(AllocTestData(pNewTest, LSF_BSP));
                break;
            case LW2080_ENGINE_TYPE_LWENC0:
                // LWENC is run in LS mode only on GPUs GM20X_thru_GP100
                if ((chipName >= Gpu::GM200) && (chipName <= Gpu::GP100))
                {
                    CHECK_RC(AllocTestData(pNewTest, LSF_MSENC0));
                }
                break;
            case LW2080_ENGINE_TYPE_LWENC1:
                // LWENC is run in LS mode only on GPUs GM20X_thru_GP100
                if ((chipName >= Gpu::GM200) && (chipName <= Gpu::GP100))
                {
                    CHECK_RC(AllocTestData(pNewTest, LSF_MSENC1));
                }
                break;
        }
    }

    if (chipName >= Gpu::TU102)
    {
        // After TU101 due to multiple LWDECs the error string changes from *LWDEC* to *LWDEC0*
        errString[0] = "MMU Fault : ENGINE_LWDEC0 FAULT_INFO_TYPE_REGION_VIOLATION";
    }

    StartRCTest();

    return rc;
}

//!
//! Test data allocation required for engine to write
//! on Acr region and then its verification
//!

RC AcrTest::AllocTestData(TestData* &pNewTest, LSF_FALCON engine)
{
    RC rc    = OK;
    pNewTest = new TestData;

    if (!pNewTest)
        return RC::SOFTWARE_ERROR;

    memset(pNewTest, 0, sizeof(TestData));

    pNewTest->m_gpuAddr  = 0LL;
    pNewTest->m_cpuAddr  = NULL;
    pNewTest->subCh      = 0;
    pNewTest->engineType = engine;

    // For writing on next location for each engine
    pNewTest->m_gpuAddr = m_pAcrSurf->GetCtxDmaOffsetGpu() + engine * sizeof(UINT32);
    pNewTest->m_cpuAddr = ((UINT32 *)m_pAcrSurf->GetAddress()) + engine;

    // Allocation of various engine's object and channel
    switch(pNewTest->engineType)
    {
        case LSF_BSP:
            CHECK_RC(m_TestConfig.AllocateChannel(&pNewTest->pCh, &m_hCh, LW2080_ENGINE_TYPE_BSP));
            pNewTest->pAlloc = new LwdecAlloc;
            CHECK_RC(((LwdecAlloc *)(pNewTest->pAlloc))->Alloc(m_hCh, pGpuDev));
            break;

        case LSF_MSENC0:
        case LSF_MSENC1:
            CHECK_RC(m_TestConfig.AllocateChannel(&pNewTest->pCh, &m_hCh, LW2080_ENGINE_TYPE_MSENC));
            pNewTest->pAlloc = new EncoderAlloc;
            CHECK_RC(((EncoderAlloc *)(pNewTest->pAlloc))->AllocOnEngine(m_hCh,
                                                LW2080_ENGINE_TYPE_LWENC(pNewTest->engineType - 1),
                                                GetBoundGpuDevice(),
                                                0));
            break;
    }
    m_TestData.push_back(pNewTest);

    return rc;
}

//! \brief Test Exelwtion Control Function.
//! \To verify both NS and LS clients for Acr region
//! \return RC
//------------------------------------------------------------------------------

RC AcrTest::Run()
{
    RC rc = OK;

    DISABLE_BREAK_COND(nobp, true);

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        Printf(Tee::PriHigh, "NS client access restrictions are not supported on fmodel\n");
    }
    else
    {
        CHECK_RC(VerifyNSAccess());
    }

    CHECK_RC(VerifyLSAccess());

    DISABLE_BREAK_END(nobp);

    return rc;
}

//!
//! VerifyNSAccess function tests whether ACR region is created properly or not and
//! whether the Non-Secure Client like RM Test can access the ACR region or not.
//! This is verified by writing a value from RM Test on Selwred ACR region.
//! Test fails if successful write.
//!
RC AcrTest::VerifyNSAccess()
{
    RC    rc = OK;
    LwU32 oldValue;
    LwU32 value;

    if ( !(acrQueryRegionParam.clientReq.reqReadMask & PRIV_LEVEL(0)) )
    {
         Printf(Tee::PriHigh, "%s: Non-Secure Client does not have read access"
                              " for Acr region", __FUNCTION__);
         rc = RC::LWRM_ERROR;
         return rc;
    }

    oldValue = MEM_RD32(m_pAcrSurf->GetAddress());

    // Generating a new value by toggling bits randomly
    value = oldValue ^ 0xffffffff;
    MEM_WR32(m_pAcrSurf->GetAddress(), value);

    // If value written in Acr region can be retrieved then failure otherwise success
    oldValue = MEM_RD32(m_pAcrSurf->GetAddress());

    if (oldValue == value)
    {
        // Success if NS client could access Acr region according to the permissions
        if (acrQueryRegionParam.clientReq.reqWriteMask & PRIV_LEVEL(0))
        {
            Printf(Tee::PriHigh, "%s: Success - Non-Secure Client has write access for Acr region"
                                 " and so could write on it\n", __FUNCTION__);
        }
        else
        {
            Printf(Tee::PriHigh, "%s: Failure - Non-Secure Client dont have the permisions"
                                 " but could write on Acr region\n", __FUNCTION__);
            rc = RC::LWRM_ERROR;
        }
    }
    else
    {
        if (acrQueryRegionParam.clientReq.reqWriteMask & PRIV_LEVEL(0))
        {
            Printf(Tee::PriHigh, "%s: Failure - Non-Secure Client has the permisions"
                                 " but couldn't write on Acr region\n", __FUNCTION__);
            rc = RC::LWRM_ERROR;
        }
        else
        {
            Printf(Tee::PriHigh, "%s: Success - Non-Secure Client does not have write access"
                                 " and couldn't write on Acr region\n", __FUNCTION__);
            Tasker::Sleep(1000);
            Printf(Tee::PriHigh, "%s: Had a fault - %s\n", __FUNCTION__, ErrorLogger::GetErrorString(0).c_str());
            nsFault = true;
        }
    }
    return rc;
}

//!
//! VerifyLSAccess function tests whether the Light-Secure Client
//! can access the ACR region or not.
//!

RC AcrTest::VerifyLSAccess()
{
    RC      rc    = OK;
    vector<TestData*>::iterator testIt;

    if (m_TestData.empty())
    {
        Printf(Tee::PriHigh, "%s: VIDEO Falcon is required to run this test\n", __FUNCTION__);

        return RC::LWRM_ILWALID_REQUEST;
    }

    // Write using available engines on Acr region
    for (testIt = m_TestData.begin(); testIt < m_TestData.end(); testIt++)
    {
        switch((*testIt)->engineType)
        {
            case LSF_BSP:
                LwdecFalconWrite(*testIt);
                break;
            case LSF_MSENC0:
            case LSF_MSENC1:
                LwencFalconWrite(*testIt);
                break;
            default:
                return RC::LWRM_ERROR;
        }
        Tasker::Sleep(1000);
    }

    // Verify if engines were able to write on Acr region
    for (testIt = m_TestData.begin(); testIt < m_TestData.end(); testIt++)
    {
        // Verify if engines were able to write on Acr region
        Printf(Tee::PriHigh,"%s: Verifying if engine %d could write on Acr region\n",
                            __FUNCTION__,(*testIt)->engineType);
        CHECK_RC(WaitAndVerify(*testIt));
    }
    return rc;
}

//!
//! LwEnc is a LS client. The semaphore method is used to write on ACR region
//!

void AcrTest::LwencFalconWrite(TestData* pNewTest)
{
    (pNewTest->pCh)->SetObject(pNewTest->subCh, ((EncoderAlloc *)(pNewTest->pAlloc))->GetHandle());

    (pNewTest->pCh)->Write(pNewTest->subCh, LWD0B7_SEMAPHORE_A,
                  DRF_NUM(D0B7, _SEMAPHORE_A, _UPPER, LwU64_HI32(pNewTest->m_gpuAddr)));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWD0B7_SEMAPHORE_B, LwU64_LO32(pNewTest->m_gpuAddr));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWD0B7_SEMAPHORE_C,
                  DRF_NUM(D0B7, _SEMAPHORE_C, _PAYLOAD, 0x12345678));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWD0B7_SEMAPHORE_D,
                  DRF_DEF(D0B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                  DRF_DEF(D0B7, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    (pNewTest->pCh)->Flush();
}

//!
//! LwDec is a LS client. The semaphore method is used to write on ACR region
//!

void AcrTest::LwdecFalconWrite(TestData* pNewTest)
{
    (pNewTest->pCh)->SetObject(pNewTest->subCh, ((LwdecAlloc *)(pNewTest->pAlloc))->GetHandle());

    (pNewTest->pCh)->Write(pNewTest->subCh, LWB0B0_SEMAPHORE_A,
                 DRF_NUM(B0B0, _SEMAPHORE_A, _UPPER, LwU64_HI32(pNewTest->m_gpuAddr)));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWB0B0_SEMAPHORE_B, LwU64_LO32(pNewTest->m_gpuAddr));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWB0B0_SEMAPHORE_C,
                  DRF_NUM(B0B0, _SEMAPHORE_C, _PAYLOAD, 0x12345678));
    (pNewTest->pCh)->Write(pNewTest->subCh, LWB0B0_SEMAPHORE_D,
                  DRF_DEF(B0B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                  DRF_DEF(B0B0, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));

    (pNewTest->pCh)->Flush();
}

//! \brief WaitAndVerify:: Wait and verify
//!
//! Wait for engine to finish and verify results
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC AcrTest::WaitAndVerify(TestData* pNewTest)
{
    RC     rc = OK;
    string error;

    pNewTest->pCh->WaitIdle(m_TimeoutMs);

    (pNewTest->pCh)->ClearError();

    if(ErrorLogger::HasErrors())
    {
        while(ErrorLogger::GetErrorCount() <= pNewTest->engineType)
        {
            Tasker::Sleep(1000);            // in milliseconds
        }

        if(nsFault)
        {
            error = ErrorLogger::GetErrorString(pNewTest->engineType + 1);
        }
        else
        {
            error = ErrorLogger::GetErrorString(pNewTest->engineType);
        }

        if(!(strncmp(error.c_str(), errString[pNewTest->engineType], strlen(errString[pNewTest->engineType]))))
        {
            Printf(Tee::PriHigh, "%s: PASS - LS client %d tried to write on ACR region, "
                                     "so had a MMU fault while writing which is expected\n",
                                     __FUNCTION__, pNewTest->engineType);
        }
        else
        {
            Printf(Tee::PriHigh, "%s: FAIL - Unexpected error, expecting a region violation"
                                 " MMU fault for particular engine\n", __FUNCTION__);
            rc = RC::LWRM_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%s: FAIL - LS client %d tried to write on ACR region, "
                             "but didn't have a MMU fault while writing\n", __FUNCTION__, pNewTest->engineType);
        rc = RC::LWRM_ERROR;
    }
    return rc;
}

//! \brief FreeTestData:: Free test data
//!
//! Free data used to run the test
//!
//! \return OK if the free passed, specific RC if failed
//------------------------------------------------------------------------
void AcrTest::FreeTestData(TestData* &pNewTest)
{
    if (!pNewTest)
        return;

    switch(pNewTest->engineType)
    {
        case LSF_BSP:
            if (pNewTest->pAlloc)
            {
                ((LwdecAlloc *)(pNewTest->pAlloc))->Free();
                delete ((LwdecAlloc *)(pNewTest->pAlloc));
            }
            break;
        case LSF_MSENC0:
        case LSF_MSENC1:
            if (pNewTest->pAlloc)
            {
                ((EncoderAlloc *)(pNewTest->pAlloc))->Free();
                delete ((EncoderAlloc *)(pNewTest->pAlloc));
            }
            break;
    }
    if (pNewTest->pCh)
        m_TestConfig.FreeChannel(pNewTest->pCh);

    delete pNewTest;
}

//! \brief Cleanup
//!
//! \return RC
//------------------------------------------------------------------------------

RC AcrTest::Cleanup()
{
    while (!m_TestData.empty())
    {
        FreeTestData(m_TestData.back());
        m_TestData.pop_back();
    }

    if (m_pAcrSurf)
    {
        m_pAcrSurf->Unmap();
        m_pAcrSurf->Free();
        delete m_pAcrSurf;
        m_pAcrSurf = NULL;
        EndRCTest();
    }

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(AcrTest, RmTest,"AcrTest RM test - Acr Sanity Test");
