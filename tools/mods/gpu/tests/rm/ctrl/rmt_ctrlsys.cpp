/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrlsys.cpp
//! \brief Test for covering system-related control commands.
//!
//! This test allocates an LW01_ROOT (client) object and issues
//! issues a sequence of LwRmControl commands that validate both
//! correct operation as well as various error handling conditions.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"

#include "class/cl0000.h"
#include "ctrl/ctrl0000.h"
#include "core/include/memcheck.h"

class CtrlSysTest : public RmTest
{
public:
    CtrlSysTest();
    virtual ~CtrlSysTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC CpuInfoTest();
    void CpuInfoPrintType(UINT32 type);
    void CpuInfoPrintCaps(UINT32 caps);
    RC ChipsetInfoTest();

    LwRm::Handle m_hClient;
};

//! \brief CtrlSysTest basic constructor
//!
//! This is just the basic constructor for the CtrlSysTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CtrlSysTest::CtrlSysTest()
{
    SetName("CtrlSysTest");
}

//! \brief CtrlSysTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CtrlSysTest::~CtrlSysTest()
{

}

//! \brief IsSupported
//!
//! This test is supported on all elwironments.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CtrlSysTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC CtrlSysTest::Setup()
{
    RC rc;
    UINT32 status;

    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    CHECK_RC(RmApiStatusToModsRC(status));

    return rc;
}

//! \brief Run entry point
//!
//! Kick off system control command tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC CtrlSysTest::Run()
{
    RC rc;

    if (!m_hClient)
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }

    // run cpu_info test
    CHECK_RC(CpuInfoTest());

    // run chipset_info test
    CHECK_RC(ChipsetInfoTest());

    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding hClient (LW01_ROOT) resource.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC CtrlSysTest::Cleanup()
{
    LwRmFree(m_hClient, m_hClient, m_hClient);
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief CpuInfoTest routine
//!
//! This function covers the following LW01_ROOT control commands:
//!
//!  LW0000_CTRL_CMD_SYSTEM_GET_CPU_INFO
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlSysTest::CpuInfoTest()
{
    LW0000_CTRL_SYSTEM_GET_CPU_INFO_PARAMS cpuInfoParams;
    UINT32 status = 0;
    RC rc;

    Printf(Tee::PriLow, "CpuInfoTest: start\n");

    // zero out param structs
    memset(&cpuInfoParams, 0,
           sizeof (LW0000_CTRL_SYSTEM_GET_CPU_INFO_PARAMS));

    // get cpu info
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_SYSTEM_GET_CPU_INFO,
                         &cpuInfoParams, sizeof (cpuInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "CpuInfoTest: GET_CPU_INFO cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // print out info
    Printf(Tee::PriLow, "CPU INFO:\n");
    CpuInfoPrintType(cpuInfoParams.type);
    CpuInfoPrintCaps(cpuInfoParams.capabilities);
    Printf(Tee::PriLow, " clock: 0x%x MHz\n", (UINT32)cpuInfoParams.clock);
    Printf(Tee::PriLow, " l1size: 0x%x l2size: 0x%x dcachelinesize: 0x%x\n",
           (UINT32)cpuInfoParams.L1DataCacheSize,
           (UINT32)cpuInfoParams.L2DataCacheSize,
           (UINT32)cpuInfoParams.dataCacheLineSize);
    Printf(Tee::PriLow, " numLogical: 0x%x numPhysical: 0x%x\n",
           (UINT32)cpuInfoParams.numLogicalCpus,
           (UINT32)cpuInfoParams.numPhysicalCpus);
    Printf(Tee::PriLow, " name: %s\n", cpuInfoParams.name);

    Printf(Tee::PriLow, "CpuInfoTest: passed\n");

    return rc;
}

//! \brief CpuInfoPrintType routine
//!
//! This function decodes a LW0000_CTRL_SYSTEM_CPU_TYPE value.
//!
//! \return No return value.
void CtrlSysTest::CpuInfoPrintType(UINT32 type)
{
    Printf(Tee::PriLow, " type: 0x%x ", type);

    if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P5)
    {
        Printf(Tee::PriLow, " (P5)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P55)
    {
        Printf(Tee::PriLow, " (P55)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P6)
    {
        Printf(Tee::PriLow, " (P6)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P2)
    {
        Printf(Tee::PriLow, " (P2)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P2XC)
    {
        Printf(Tee::PriLow, " (P2XC)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_CELA)
    {
        Printf(Tee::PriLow, " (CELA)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P3)
    {
        Printf(Tee::PriLow, " (P3)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P3_INTL2)
    {
        Printf(Tee::PriLow, " (P3_INTL2)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_P4)
    {
        Printf(Tee::PriLow, " (P4)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K5)
    {
        Printf(Tee::PriLow, " (K5)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K6)
    {
        Printf(Tee::PriLow, " (K6)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K62)
    {
        Printf(Tee::PriLow, " (K62)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K63)
    {
        Printf(Tee::PriLow, " (K63)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K7)
    {
        Printf(Tee::PriLow, " (K7)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_K8)
    {
        Printf(Tee::PriLow, " (K8)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_C6)
    {
        Printf(Tee::PriLow, " (C6)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_C62)
    {
        Printf(Tee::PriLow, " (C62)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_GX)
    {
        Printf(Tee::PriLow, " (GX)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_M1)
    {
        Printf(Tee::PriLow, " (M1)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_M2)
    {
        Printf(Tee::PriLow, " (M2)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_MGX)
    {
        Printf(Tee::PriLow, " (MGX)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_TM_CRUSOE)
    {
        Printf(Tee::PriLow, " (TM_CRUSOE)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_PPC603)
    {
        Printf(Tee::PriLow, " (PPC603)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_PPC604)
    {
        Printf(Tee::PriLow, " (PPC604)\n");
    }
    else if (type == LW0000_CTRL_SYSTEM_CPU_TYPE_PPC750)
    {
        Printf(Tee::PriLow, " (PPC750)\n");
    }
    else
    {
        Printf(Tee::PriLow, " (unknown)\n");
    }

    return;
}

//! \brief CpuInfoPrintCaps routine
//!
//! This function decodes LW0000_CTRL_SYSTEM_CPU_CAP bits.
//!
//! \return No return value.
void CtrlSysTest::CpuInfoPrintCaps(UINT32 caps)
{
    Printf(Tee::PriLow, " caps: 0x%x\n", caps);

    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_MMX)
        Printf(Tee::PriLow, "   CPU_CAP_MMX\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_SSE)
        Printf(Tee::PriLow, "   CPU_CAP_SSE\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_3DNOW)
        Printf(Tee::PriLow, "   CPU_CAP_3DNOW\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_SSE2)
        Printf(Tee::PriLow, "   CPU_CAP_SSE2\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_SFENCE)
        Printf(Tee::PriLow, "   CPU_CAP_SFENCE\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_WRITE_COMBINING)
        Printf(Tee::PriLow, "   CPU_CAP_WRITE_COMBINING\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_ALTIVEC)
        Printf(Tee::PriLow, "   CPU_CAP_ALTIVEC\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_PUT_NEEDS_IO)
        Printf(Tee::PriLow, "   CPU_CAP_PUT_NEEDS_IO\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_NEEDS_WC_WORKAROUND)
        Printf(Tee::PriLow, "   CPU_CAP_NEEDS_WC_WORKAROUND\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_3DNOW_EXT)
        Printf(Tee::PriLow, "   CPU_CAP_3DNOW_EXT\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_MMX_EXT)
        Printf(Tee::PriLow, "   CPU_CAP_MMX_EXT\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_CMOV)
        Printf(Tee::PriLow, "   CPU_CAP_CMOV\n");
    if (caps & LW0000_CTRL_SYSTEM_CPU_CAP_CLFLUSH)
        Printf(Tee::PriLow, "   CPU_CAP_CLFLUSH\n");

    return;
}

//! \brief ChipsetInfoTest routine
//!
//! This function covers the following LW01_ROOT control commands:
//!
//!  LW0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO
//!
//! \return OK if the test passes, subtest-specific value indicating
//!         nature of failure otherwise
RC CtrlSysTest::ChipsetInfoTest()
{
    LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS chipsetInfoParams;
    UINT32 status = 0;
    RC rc;

    Printf(Tee::PriLow, "ChipsetInfoTest: start\n");

    // zero out param structs
    memset(&chipsetInfoParams, 0,
           sizeof (LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS));

    // get cpu info
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO,
                         &chipsetInfoParams, sizeof (chipsetInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriNormal,
               "ChipsetInfoTest: GET_CHIPSET_INFO cmd failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    // print out some interesting stuff
    Printf(Tee::PriLow, "CHIPSET INFO:\n");
    Printf(Tee::PriLow, " vendorId: 0x%x\n",
           chipsetInfoParams.vendorId);
    Printf(Tee::PriLow, " deviceId: 0x%x\n",
           chipsetInfoParams.deviceId);
    Printf(Tee::PriLow, " vendorName: %s\n",
           chipsetInfoParams.vendorNameString);
    Printf(Tee::PriLow, " chipsetName: %s\n",
           chipsetInfoParams.chipsetNameString);

    Printf(Tee::PriLow, "ChipsetInfoTest: passed\n");

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ CtrlSysTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(CtrlSysTest, RmTest,
                 "CtrlSysTest RM Test.");
