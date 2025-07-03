/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file pmudriver.cpp
//! \brief ucode fuzz driver for SEC2 RTOS
//!
//! Note: copied mostly from SEC2 RTOS LS/HS switch test
//!

#include "rtosucodefuzzdriver.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "rmsec2cmdif.h"
#include "rmlsfm.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocsec2rtos.h"

class Sec2FuzzDriver : public RtosFlcnCmdFuzzDriver
{
public:
    Sec2FuzzDriver();
    virtual ~Sec2FuzzDriver() = default;

    virtual LwU32  GetUcodeId() override       { return LW2080_UCODE_FUZZER_SEC2; }

    virtual string IsTestSupported() override;
    virtual RC     Setup() override;
    virtual RC     Cleanup() override;
    virtual RC     PreRun(FilledDataTemplate* pData) override;

private:
    unique_ptr<UprocRtos> m_pSec2Rtos;
};

//! \brief Sec2FuzzDriver constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2FuzzDriver::Sec2FuzzDriver()
{
    SetName("Sec2FuzzDriver");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2FuzzDriver::IsTestSupported()
{
    RC rc;

    if (!IsClassSupported(MAXWELL_SEC2))
    {
        return "Sec2FuzzDriver: MAXWELL_SEC2 is not supported";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief PreRun: waits for sec2 to be ready for a command
//------------------------------------------------------------------------------
RC Sec2FuzzDriver::PreRun(FilledDataTemplate* pData)
{
    RC rc = m_pSec2Rtos->WaitUprocReady();
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Sec2FuzzDriver: Ucode OS is not ready. Skipping tests. \n");

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    CHECK_RC(RtosFlcnCmdFuzzDriver::PreRun(pData));
    return rc;
}

//! \brief Setup: Allocate UprocSec2Rtos instance
//------------------------------------------------------------------------------
RC Sec2FuzzDriver::Setup()
{
    RC rc;

    CHECK_RC(RtosFlcnCmdFuzzDriver::Setup());

    // Get SEC2 UPROCRTOS class instance
    m_pSec2Rtos = std::make_unique<UprocSec2Rtos>(GetBoundGpuSubdevice());
    if (m_pSec2Rtos == nullptr)
    {
        Printf(Tee::PriError, "Could not create UprocSec2Rtos instance.\n");
        return RC::SOFTWARE_ERROR;
    }
    rc = m_pSec2Rtos->Initialize();
    if (OK != rc)
    {
        m_pSec2Rtos->Shutdown();
        Printf(Tee::PriError, "SEC2 UPROCRTOS not supported\n");
        CHECK_RC(rc);
    }

    // Initialize RmTest handle
    m_pSec2Rtos->SetRmTestHandle(this);
    return rc;
}

//! \brief Cleanup, shuts down SEC2 test instance.
//------------------------------------------------------------------------------
RC Sec2FuzzDriver::Cleanup()
{
    m_pSec2Rtos->Shutdown();
    return RtosFlcnCmdFuzzDriver::Cleanup();
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2FuzzDriver, RmTest, "Sec2 Fuzzer Test");
DEFINE_RTOS_FLCN_UCODE_FUZZ_PROPS(Sec2FuzzDriver)

