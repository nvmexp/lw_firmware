/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_psdl.cpp
//! \brief rmtest for PSDL
//!

#include "gpu/tests/rmtest.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "maxwell/gm204/dev_fb.h"
#include "maxwell/gm204/dev_fuse.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "rmpsdl.h"

class PsdlTest : public RmTest
{
public:
    PsdlTest();
    virtual ~PsdlTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(psdlStatus, LwU32);

private:
    LwU32 m_psdlStatus;
};

//! \brief PsdlTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
PsdlTest::PsdlTest()
{
    SetName("PsdlTest");
    m_psdlStatus = 0;
}

//! \brief PsdlTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PsdlTest::~PsdlTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string PsdlTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GM200))
        return "Test only supported on GM200 or later chips";
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC PsdlTest::Setup()
{
    return OK;
}

//! \brief Run the test
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC PsdlTest::Run()
{
    unsigned int mask;
    unsigned int regVal = GetBoundGpuSubdevice()->RegRd32(LW_FUSE_OPT_PRIV_SEC_EN);
    if (FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _NO, regVal))
    {
        Printf(Tee::PriHigh, "Error: Priv Sec disabled\n");
        return RC::SOFTWARE_ERROR;
    }
    if (m_psdlStatus != RM_PSDL_RC_OK)
    {
        Printf(Tee::PriHigh, "Error: PSDL loading failed with status 0x%x\n", (unsigned int)m_psdlStatus);
        return m_psdlStatus;
    }
    mask = GetBoundGpuSubdevice()->RegRd32(LW_PFB_PRI_MMU_PHYS_SELWRE__PRIV_LEVEL_MASK);
    if (mask != 0x77)
    {
        Printf(Tee::PriHigh, "Error: PSDL test failed. LW_PFB_PRI_MMU_PHYS_SELWRE__PRIV_LEVEL_MASK expected 0x77, actual 0x%02x\n", mask);
        Printf(Tee::PriHigh, "** Please check if the PSDL license has exceptions added for LW_PFB_PRI_MMU_PHYS_SELWRE__PRIV_LEVEL_MASK register ***");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC PsdlTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PsdlTest, RmTest, "PSDL Test. \n");

CLASS_PROP_READWRITE(PsdlTest, psdlStatus, UINT32, "PSDL status, default = 0");
