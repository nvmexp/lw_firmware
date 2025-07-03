/*
 * Lwidia_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl208ffbio.cpp
//! \brief LW208E_CTRL_CMD FBIO tests
//!
#include "ctrl/ctrl208f.h" // LW20_SUBDEVICE_DIAG
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

class Ctrl208fFbioTest: public RmTest
{
public:
    Ctrl208fFbioTest();
    virtual ~Ctrl208fFbioTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
};

//! \brief Ctrl208fFbioTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl208fFbioTest::Ctrl208fFbioTest()
{
    SetName("Ctrl208fFbioTest");
}

//! \brief Ctrl208fFbioTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl208fFbioTest::~Ctrl208fFbioTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl208fFbioTest::IsTestSupported()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW208F_CTRL_FBIO_GET_TRAINING_CAPS_PARAMS getTrainingParams;
    RC rc = pLwRm->Control(hSubdevice,
                           LW208F_CTRL_CMD_FBIO_GET_TRAINING_CAPS,
                           (void*)&getTrainingParams,
                           sizeof(getTrainingParams));
   if( (rc == OK) && (getTrainingParams.supported == 1) )
       return RUN_RMTEST_TRUE;
   Printf(Tee::PriHigh,"RC message = %s\n",rc.Message());
   return rc.Message();
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC Ctrl208fFbioTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl208fFbioTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    RC rc;
    LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_PARAMS setupTrainingParams;
    LW208F_CTRL_FBIO_RUN_TRAINING_EXP_PARAMS runTrainingParams;
    LwU32 part;

    // Issue null cmd to setup.  This should always succeed.
    setupTrainingParams.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_NULL;

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                            (void*)&setupTrainingParams,
                            sizeof(setupTrainingParams)));

    //
    // This cmd should fail as we attempt to set the # of mod slots too high.
    // Should fail with _ILWALID_LIMIT.  Note: this call can fail with NO_FREE_MEM as well.
    //
    setupTrainingParams.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_NUMBER_OF_MOD_SLOTS;
    setupTrainingParams.op.setNumberOfModSlots.modSlots = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_NUMBER_OF_MOD_SLOTS__MAX + 1;
    rc = pLwRm->Control(hSubdevice,
                        LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                        (void*)&setupTrainingParams,
                        sizeof(setupTrainingParams));
    if ( rc == RC::LWRM_ILWALID_LIMIT )
    {
        rc = OK;
    }
    else
        Printf(Tee::PriHigh, "Ctrl208ffbTest: set number of mod slots w/out-of-range value didnt' return expected rc, got 0x%x\n", (unsigned int)rc);
    CHECK_RC(rc);

    //
    // Setup very simple 3-op mod slot list. Note this list is setup backward;
    // just for grins.
    //
    setupTrainingParams.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_NUMBER_OF_MOD_SLOTS;
    setupTrainingParams.op.setNumberOfModSlots.modSlots = 3;
    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                            (void*)&setupTrainingParams,
                            sizeof(setupTrainingParams)));

    setupTrainingParams.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_MOD_SLOT;
    setupTrainingParams.op.setModSlot.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_MOD_SLOT_NULL;
    setupTrainingParams.op.setModSlot.seq = 2;
    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                            (void*)&setupTrainingParams,
                            sizeof(setupTrainingParams)));

    setupTrainingParams.op.setModSlot.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_MOD_SLOT_SET_REGISTER;
    setupTrainingParams.op.setModSlot.seq = 1;
    setupTrainingParams.op.setModSlot.op.setRegister.reg     = 0x1005A0;   // LW_PFB_TRAINING_DELAY
    setupTrainingParams.op.setModSlot.op.setRegister.andMask = ~(UINT32)0; // just use existing contents
    setupTrainingParams.op.setModSlot.op.setRegister.orMask  = 0;          // or in nothing.
    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                            (void*)&setupTrainingParams,
                            sizeof(setupTrainingParams)));

    setupTrainingParams.op.setModSlot.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_MOD_SLOT_DELAY;
    setupTrainingParams.op.setModSlot.seq = 0;
    setupTrainingParams.op.setModSlot.op.delay.usec = 0;
    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                            (void*)&setupTrainingParams,
                            sizeof(setupTrainingParams)));

    // this one should fail w/ILWALID_INDEX since it is out of range [0-2]
    setupTrainingParams.op.setModSlot.cmd = LW208F_CTRL_FBIO_SETUP_TRAINING_EXP_SET_MOD_SLOT_NULL;
    setupTrainingParams.op.setModSlot.seq = 4;
    rc = pLwRm->Control(hSubdevice,
                        LW208F_CTRL_CMD_FBIO_SETUP_TRAINING_EXP,
                        (void*)&setupTrainingParams,
                        sizeof(setupTrainingParams));
    if ( rc == RC::LWRM_ILWALID_INDEX )
    {
        rc = OK;
    }
    else
        Printf(Tee::PriHigh, "Ctrl208ffbTest: set out of range mod slots seq value didnt' return expected rc, got 0x%x\n", (unsigned int)rc);
    CHECK_RC(rc);

    // Compilation check for sizing
    runTrainingParams.risingPasses[LW208F_CTRL_FBIO_RUN_TRAINING_EXP_RESULT__SIZE -1] = 0;
    runTrainingParams.fallingPasses[LW208F_CTRL_FBIO_RUN_TRAINING_EXP_RESULT__SIZE -1] = 0;
    runTrainingParams.failingDebug[LW208F_CTRL_FBIO_RUN_TRAINING_EXP_RESULT__SIZE -1] = 0;
    runTrainingParams.partitionsValid = 0;
    runTrainingParams.bytelanesValid = 0;

    //
    // Run an experiment.  What we have(n't) setup (fully) could pass/fail as
    // an experiment.  However, it shouldn't fail API-wise.
    //
    if ( Platform::GetSimulationMode() != Platform::Fmodel )
    {
        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW208F_CTRL_CMD_FBIO_RUN_TRAINING_EXP,
                                (void*)&runTrainingParams,
                                sizeof(runTrainingParams)));
    }
    else
    {
        // Expecting a timeout failure on FModel at the moment.
        Printf(Tee::PriLow, "Ctrl208fFbioTest Fmodel timeout still happening when we run a simple experiment?!?!!\n");
    }

    // Print results
    Printf(Tee::PriHigh, "Ctrl208ffbTest: valid: partitions=0x%08x bytelanes=0x%08x\n",
        runTrainingParams.partitionsValid, runTrainingParams.bytelanesValid);

    for (part = 0; part < LW208F_CTRL_FBIO_RUN_TRAINING_EXP_RESULT__SIZE; part++)
    {
        Printf(Tee::PriHigh, "Ctrl208ffbTest: partition #%u: rising=0x%08x falling=0x%08x debug=0x%08x\n",
            part, runTrainingParams.risingPasses[part], runTrainingParams.fallingPasses[part], runTrainingParams.failingDebug[part]);
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl208fFbioTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl208fFbioTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl208fFbioTest, RmTest,
                 "Ctrl208fFbioTest RM test.");
