/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080mc.cpp
//! \brief LW2080_CTRL_CMD MC tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/utility/errloggr.h"
#include "class/cl506f.h"
#include "class/cl502d.h"
#include "class/cl85b6.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

#define LW_PFIFO_ENG_HOLDOFF 0x00002540
#define LW_PFIFO_ENG_HOLDOFF_ENG1_MTHDCTX                         5:5
#define LW_PFIFO_ENG_HOLDOFF_ENG1_MTHDCTX_NOTBLOCKED           0x00000000
#define LW_PFIFO_ENG_HOLDOFF_ENG1_MTHDCTX_BLOCKED              0x00000001

class Ctrl2080McTest: public RmTest
{
public:
    Ctrl2080McTest();
    virtual ~Ctrl2080McTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    Notifier                       m_Notifier;
    LwRm::Handle                   m_objectHandle;
    Channel                        *m_pCh;
    FLOAT64                        m_TimeoutMs;

    RC allocObject();
    RC freeObject();
};

//! \brief Ctrl2080McTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080McTest::Ctrl2080McTest() :
    m_objectHandle(0),
    m_pCh(nullptr),
    m_TimeoutMs(Tasker::NO_TIMEOUT)
{
    SetName("Ctrl2080McTest");
}

//! \brief Ctrl2080McTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080McTest::~Ctrl2080McTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080McTest::IsTestSupported()
{
    if( (IsClassSupported(LW50_TWOD)) && (IsClassSupported(GT212_SUBDEVICE_PMU)) )
        return RUN_RMTEST_TRUE;
    return "Either LW50_TWOD or GT212_SUBDEVICE_PMU class is not supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl2080McTest::Setup()
{
    RC    rc;

    // TC1
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    // Allocate channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(allocObject());
    CHECK_RC(m_Notifier.Allocate(m_pCh, 1, &m_TestConfig));

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080McTest::Run()
{
    RC       rc;
    RC       errorRc;
    LwRmPtr  pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW2080_CTRL_MC_GATE_ENGINE_POWER_PARAMS   pwrGate = {0};
    UINT32   cnt;
    UINT32   holdOffReg;

    // TODO: Allocated video object and do the same.
    for (cnt = 0; cnt < 3; cnt++)
    {
        m_pCh->Write(0, LW502D_SET_OBJECT, m_objectHandle);

        CHECK_RC(m_Notifier.Instantiate(0, LW502D_SET_CONTEXT_DMA_NOTIFY));
        m_Notifier.Clear(0);
        CHECK_RC(m_pCh->Write(0, LW502D_NOTIFY,
                                 LW502D_NOTIFY_TYPE_WRITE_ONLY));

        m_pCh->Write(0, LW502D_NO_OPERATION, 0);
        m_pCh->Flush();
        // Waiting on subchannel 0
        CHECK_RC(m_Notifier.Wait(0, m_TimeoutMs));

        CHECK_RC(m_pCh->WaitIdle(m_TimeoutMs));

        // Check engine is in elpg_off state by checking the LW_PFIFO_ENG_HOLDOFF
        holdOffReg = GetBoundGpuSubdevice()->RegRd32(LW_PFIFO_ENG_HOLDOFF);
        if (DRF_VAL(_PFIFO, _ENG_HOLDOFF_, ENG1_MTHDCTX, holdOffReg) !=
            LW_PFIFO_ENG_HOLDOFF_ENG1_MTHDCTX_NOTBLOCKED)
        {
            Printf(Tee::PriHigh, "Engine is not powered on. Hold off reg = 0x%x\n",
                    holdOffReg);
            rc = RC::LWRM_ERROR;
            CHECK_RC(rc);
        }

        // Send the ELPG_ON message to trigger power gating routines.
        pwrGate.engineId = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_GRAPHICS;
        CHECK_RC(pLwRm->Control(hSubdevice,
                LW2080_CTRL_CMD_MC_GATE_ENGINE_POWER,
                (void*)&pwrGate,
                sizeof(pwrGate)));

        // Check engine is in elpg_on state by checking the LW_PFIFO_ENG_HOLDOFF
        holdOffReg = GetBoundGpuSubdevice()->RegRd32(LW_PFIFO_ENG_HOLDOFF);
        if (DRF_VAL(_PFIFO, _ENG_HOLDOFF_, ENG1_MTHDCTX, holdOffReg) !=
            LW_PFIFO_ENG_HOLDOFF_ENG1_MTHDCTX_BLOCKED)
        {
            Printf(Tee::PriHigh, "Engine is already not powered off. Hold off reg = 0x%x\n",
                    holdOffReg);
            rc = RC::LWRM_ERROR;
            CHECK_RC(rc);
        }

        // Lets leave the engine on.
        m_pCh->Write(0, LW502D_SET_OBJECT, m_objectHandle);
        m_pCh->Flush();
        m_pCh->Write(0, LW502D_NO_OPERATION, 0);
        m_pCh->Flush();
        CHECK_RC(m_pCh->WaitIdle(m_TimeoutMs));

    }

    // Check that there are no errors on the channel.
    errorRc = m_pCh->CheckError();

    CHECK_RC(freeObject());

    if (errorRc != OK)
    {
        return errorRc;
    }
    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080McTest::Cleanup()
{
    m_Notifier.Free();
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

RC Ctrl2080McTest::allocObject()
{
    RC          rc;
    LwRmPtr     pLwRm;

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_objectHandle, LW50_TWOD, NULL));
    return rc;
}

RC Ctrl2080McTest::freeObject()
{
    RC          rc;
    LwRmPtr     pLwRm;

    pLwRm->Free(m_objectHandle);
    m_objectHandle = 0;
    return rc;
}

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080McTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080McTest, RmTest,
                 "Ctrl2080McTest RM test.");
