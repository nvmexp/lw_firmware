/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_changrp.cpp
//! \brief To verify basic functionality of SCG group channel tagging
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <ctime>
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "random.h"

#include "class/cl9067.h"  // FERMI_CONTEXT_SHARE_A
#include "class/cl90f1.h"  // FERMI_VASPACE_A

// Must be last
#include "core/include/memcheck.h"

class SubcontextTest : public RmTest
{
    public:
    SubcontextTest();
    virtual ~SubcontextTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    private:
    RC BasicSubctxTest(ChannelGroup *pChGrp);
    RC NegativeSubctxTest(ChannelGroup *pChGrp0, ChannelGroup *pChGrp1);
#ifdef LW_VERIF_FEATURES
    RC SpecifiedSubctxTest(ChannelGroup *pChGrp);
#endif

    bool   m_bMultiVasInChanGrp;
    UINT32 m_maxSubctx;
    Random m_randomGenerator;
};

//! \brief SubcontextTest constructor
//!
//------------------------------------------------------------------------------
SubcontextTest::SubcontextTest() :
    m_bMultiVasInChanGrp(false),
    m_maxSubctx(0)
{
    SetName("SubcontextTest");
}

//! \brief SubcontextTest destructor
//!
//------------------------------------------------------------------------------
SubcontextTest::~SubcontextTest()
{
}

//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string SubcontextTest::IsTestSupported()
{
    LwRmPtr pLwRm;
    return (pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) ?
        RUN_RMTEST_TRUE : "Channel groups not supported");
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! \return corresponding RC if setup fails
//------------------------------------------------------------------------------
RC SubcontextTest::Setup()
{
    RC                               rc;
    LwRmPtr                          pLwRm;
    LwU8                             fifoCaps[LW0080_CTRL_FIFO_CAPS_TBL_SIZE];
    LW0080_CTRL_FIFO_GET_CAPS_PARAMS fifoCapsParams = {0};
    LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoInfoParams = {0};
    LwU32                            seed;

    CHECK_RC(InitFromJs());

    memset(fifoCaps, 0x0, LW0080_CTRL_FIFO_CAPS_TBL_SIZE);

    fifoCapsParams.capsTblSize = LW0080_CTRL_FIFO_CAPS_TBL_SIZE;
    fifoCapsParams.capsTbl     = (LwP64)fifoCaps;

    CHECK_RC(pLwRm->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_FIFO_GET_CAPS,
        &fifoCapsParams, sizeof(fifoCapsParams)));

    m_bMultiVasInChanGrp = !!LW0080_CTRL_FIFO_GET_CAP(fifoCaps, LW0080_CTRL_FIFO_CAPS_MULTI_VAS_PER_CHANGRP);

    if (m_bMultiVasInChanGrp && (GetBoundGpuDevice()->GetFamily() < GpuDevice::Volta))
    {
        Printf(Tee::PriHigh, "SubcontextTest: Multiple vaspaces in TSG not supported on this platform; but RM reports otherwise \n");
        return RC::SOFTWARE_ERROR;
    }

    fifoInfoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_SUBCONTEXT_PER_GROUP;
    fifoInfoParams.fifoInfoTblSize      = 1;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_FIFO_GET_INFO,
        (void*)&fifoInfoParams, sizeof(fifoInfoParams)));

    m_maxSubctx = fifoInfoParams.fifoInfoTbl[0].data;

    seed = (LwU32)time(NULL);
    m_randomGenerator.SeedRandom(seed);

    return OK;
}

//! \brief  Cleanup():
//!
//------------------------------------------------------------------------------
RC SubcontextTest::Cleanup()
{
    return OK;
}

//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC SubcontextTest::Run()
{
    RC rc = OK;

    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS channelGroupParams = {0};
    channelGroupParams.engineType = LW2080_ENGINE_TYPE_GR(0);

    // Allocate 2 TSGs
    ChannelGroup chGrp0(&m_TestConfig, &channelGroupParams);
    ChannelGroup chGrp1(&m_TestConfig, &channelGroupParams);
    chGrp0.SetUseVirtualContext(false);
    chGrp1.SetUseVirtualContext(false);

    CHECK_RC_CLEANUP(chGrp0.Alloc());
    CHECK_RC_CLEANUP(chGrp1.Alloc());

    // Run basic subctx test
    CHECK_RC_CLEANUP(BasicSubctxTest(&chGrp0));

    // Run subctx negative test
    CHECK_RC_CLEANUP(NegativeSubctxTest(&chGrp0, &chGrp1));

#ifdef LW_VERIF_FEATURES
    // Run SPECIFIED subctx test
    CHECK_RC_CLEANUP(SpecifiedSubctxTest(&chGrp0));
#else
    //
    // LW_VERIF_FEATURES not defined. Just print that this test
    // has been skipped due to LW_VERIF_FEATURES missing
    //
    Printf(Tee::PriHigh, "SubcontextTest::SpecifiedSubctxTest skipped "
           " because LW_VERIF_FEATURES is missing\n");
    return OK;
#endif

Cleanup:
    chGrp0.Free();
    chGrp1.Free();

    return rc;
}

RC SubcontextTest::BasicSubctxTest(ChannelGroup *pChGrp)
{
    RC                               rc        = OK;
    LwRmPtr                          pLwRm;
    LwRm::Handle                     hVASpace  = 0;
    LW_VASPACE_ALLOCATION_PARAMETERS vasParams = {0};
    Subcontext                      *pSubctx0  = NULL;
    Subcontext                      *pSubctx1  = NULL;

    // Test ASYNC subcontext allocation
    rc = pChGrp->AllocSubcontext(&pSubctx1, 0, Subcontext::ASYNC);
    if (rc == OK)
    {
        // ASYNC is not supported on pre-SCG chips; so return error
        if (m_maxSubctx <= LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC)
        {
            Printf(Tee::PriHigh, "SubcontextTest: ASYNC allocation not allowed for this chip; But it passed \n");
            CHECK_RC_CLEANUP(RC::LWRM_ERROR);
        }

#ifdef LW_VERIF_FEATURES
        if ((pSubctx1->GetId() < LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC) ||
           (pSubctx1->GetId() >= m_maxSubctx))
        {
            Printf(Tee::PriHigh, "SubcontextTest: ASYNC allocation passed; but returned an invalid id 0x%x \n",
                pSubctx1->GetId());
            CHECK_RC_CLEANUP(RC::LWRM_ERROR);
        }
#endif
    }
    else
    {
        // ASYNC is not supported on pre-SCG chips; so return OK
        if (m_maxSubctx <= LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC)
        {
            Printf(Tee::PriHigh, "SubcontextTest: ASYNC allocation not allowed for this chip; Failed as expected \n");
            rc.Clear();
        }

        goto Cleanup;
    }

    // Test SYNC subcontext allocation
    CHECK_RC_CLEANUP(pChGrp->AllocSubcontext(&pSubctx0, 0, Subcontext::SYNC));

#ifdef LW_VERIF_FEATURES
    if (pSubctx0->GetId() != LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC)
    {
        Printf(Tee::PriHigh, "SubcontextTest: SYNC allocation passed; but returned an invalid id 0x%x \n",
            pSubctx0->GetId());
        CHECK_RC_CLEANUP(RC::LWRM_ERROR);
    }
#endif

    pChGrp->FreeSubcontext(pSubctx0);
    pSubctx0 = NULL;

    // Test SYNC subcontext allocation on non-default vaspace
    CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()), &hVASpace, FERMI_VASPACE_A, &vasParams));

    DISABLE_BREAK_COND(nobp, true);
    rc = pChGrp->AllocSubcontext(&pSubctx0, hVASpace, Subcontext::SYNC);
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        if (!m_bMultiVasInChanGrp)
        {
            Printf(Tee::PriHigh, "SubcontextTest: Multiple vaspaces not supported on this chip; SYNC allocation passed \n");
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }

#ifdef LW_VERIF_FEATURES
        if (pSubctx0->GetId() != LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC)
        {
            CHECK_RC_CLEANUP(RC::LWRM_ERROR);
        }
#endif
    }
    else
    {
        if (!m_bMultiVasInChanGrp)
        {
            Printf(Tee::PriHigh, "SubcontextTest: Multiple vaspaces not supported on this chip; SYNC allocation failed as expected \n");
            rc.Clear();
        }
    }

    Printf(Tee::PriHigh, "BasicSubctxTest() completed\n");

Cleanup:
    if (pSubctx0)
    {
        pChGrp->FreeSubcontext(pSubctx0);
        pSubctx0 = NULL;
    }

    if (pSubctx1)
    {
        pChGrp->FreeSubcontext(pSubctx1);
        pSubctx1 = NULL;
    }

    if (hVASpace)
        pLwRm->Free(hVASpace);

    return rc;
}

RC SubcontextTest::NegativeSubctxTest(ChannelGroup *pChGrp0, ChannelGroup *pChGrp1)
{
    RC            rc        = OK;
    Subcontext   *pSubctx   = NULL;
    Channel      *pCh3D;
    LwU32         flags     = 0;

    // Allocate a subctx in TSG0
    CHECK_RC_CLEANUP(pChGrp0->AllocSubcontext(&pSubctx, 0, Subcontext::SYNC));

    // Allocate a channel in TSG1 with above subctx in TSG0
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, 0, flags);

    DISABLE_BREAK_COND(nobp, true);
    rc = pChGrp1->AllocChannelWithSubcontext(&pCh3D, pSubctx, flags);
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Negative test failed as expected\n");
        rc.Clear();
    }

Cleanup:
    if (pSubctx)
    {
        pChGrp0->FreeSubcontext(pSubctx);
        pSubctx = NULL;
    }

    return rc;
}

#ifdef  LW_VERIF_FEATURES
RC SubcontextTest::SpecifiedSubctxTest(ChannelGroup *pChGrp)
{

    RC            rc        = OK;
    Subcontext   *pSubctx0  = NULL;
    Subcontext   *pSubctx1  = NULL;
    LwU32         veid;

    // Choose random valid veid
    veid = m_randomGenerator.GetRandom(0, m_maxSubctx-1);

    // Allocate subcontext with SPECIFIED flag set and a valid veid
    CHECK_RC_CLEANUP(pChGrp->AllocSubcontextSpecified(&pSubctx0, 0, veid));

    // Negative test 1 : Try allocating subcontext with same valid veid - this should fail
    DISABLE_BREAK_COND(nobp, true);
    rc = pChGrp->AllocSubcontextSpecified(&pSubctx0, 0, veid);
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Negative 'SPECIFIED' flag test1 failed as expected\n");
        rc.Clear();
    }

    // Negative test 2 : Try allocating subcontext with invalid veid - this should fail
    DISABLE_BREAK_COND(nobp, true);
    rc = pChGrp->AllocSubcontextSpecified(&pSubctx1, 0, m_maxSubctx+2);
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Negative 'SPECIFIED' flag test2 failed as expected\n");
        rc.Clear();
    }

Cleanup:
    if (pSubctx0)
    {
        pChGrp->FreeSubcontext(pSubctx0);
        pSubctx0 = NULL;
    }

    if (pSubctx1)
    {
        pChGrp->FreeSubcontext(pSubctx1);
        pSubctx1 = NULL;
    }

    return rc;
}
#endif

JS_CLASS_INHERIT(SubcontextTest, RmTest,
    "SubcontextTest RMTEST that tests subcontextfunctionality");
