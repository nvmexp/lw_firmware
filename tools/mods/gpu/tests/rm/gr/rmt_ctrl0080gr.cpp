/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2016,2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080gr.cpp
//! \brief LW0080_CTRL_CMD GR tests
//!

#include "ctrl/ctrl0080.h"   // LW01_DEVICE_XX/LW03_DEVICE
#include "class/cl9097.h"    // FERMI_A
#include "class/cl9097sw.h"
#include "class/cl5097.h"    // LW50_TESLA
#include "class/cl906f.h"    // FERMI_A
#include "class/cla097.h"    // KEPLER_A
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cla06fsubch.h"
#include "class/cl5097sw.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputestc.h"
#include "core/include/refparse.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "rmt_ctrl0080gr.h"
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE                     (1024 * 4)
#define GR_OBJ_SUBCH                     LWA06F_SUBCHANNEL_3D

//
// Here, As we are changing the value of registers through instance memory so
// it is advisery to choose those register with which the risk of
// malfunctioning is low after updating register data.
//

//
// We are taking two different registers because
// LW_PGRAPH_PRI_SCC_DEBUG register is not present with FERMI
// and vice-versa.
//

//! \brief Ctrl0080GrTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl0080GrTest::Ctrl0080GrTest()
{
    SetName("Ctrl0080GrTest");

    m_hObject2 = 0;
    m_hDynMem = 0;
    m_pCh = NULL;
    m_hCh = 0;

    m_3dAlloc_First.SetOldestSupportedClass(LW50_TESLA);
    m_3dAlloc_First.SetNewestSupportedClass(LW50_TESLA);
    m_3dAlloc_Second.SetOldestSupportedClass(FERMI_A);
}

//! \brief Ctrl0080GrTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl0080GrTest::~Ctrl0080GrTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \want to test this on all chips
//-----------------------------------------------------------------------------
string Ctrl0080GrTest::IsTestSupported()
{
    // Bug 336820 : Test is not supported on chips < LW5X
    if ((m_3dAlloc_First.IsSupported(GetBoundGpuDevice()) || m_3dAlloc_Second.IsSupported(GetBoundGpuDevice())) && !GetBoundGpuSubdevice()->HasBug(336820))
        return RUN_RMTEST_TRUE;
    else
        return "Bug: 336820 or None of LW50_TESLA, FERMI+ classes is supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();
 
    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::Run()
{
    RC rc;
    RefManualRegister const *refManReg;
    RefManual* ref_man = GetBoundGpuSubdevice()->GetRefManual();
    if (m_3dAlloc_Second.IsSupported(GetBoundGpuDevice()))
    {
        refManReg = ref_man->FindRegister("LW_PGRAPH_PRI_BES_ZROP_DEBUG3");
        if (refManReg == NULL)
        {
            refManReg = ref_man->FindRegister("LW_PGRAPH_PRI_GPC0_ROPS_ZROP_DEBUG3");
        }

    }
    else
    {
        refManReg = ref_man->FindRegister("LW_PGRAPH_PRI_SCC_DEBUG");
    }
    CHECK_RC(TestSetContextOverride(refManReg));

    //
    // Test a register per ctxsw register list. This gives coverage for RM's ability
    // to correctly find a register's offset in the context buffer.
    //
#define TEST_CTX_OVERRIDE(regName) \
    refManReg = ref_man->FindRegister((regName)); \
    if (refManReg == NULL) { \
        Printf(Tee::PriHigh, "Could not find register %s\n", (regName)); \
        return RC::SOFTWARE_ERROR; \
    } \
    CHECK_RC(TestSetContextOverride(refManReg));

    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_FE_BUNDLE_BROADCAST_0");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_DS_DEBUG");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPC0_GCC_DBG");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPC0_GPM_PD_CONFIG");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPC0_TPCS_SM_MACHINE_ID0");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPCS_TPCS_PE_L2_EVICT_POLICY");
    TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPC0_PPCS_PES_TG");

    // The EGPC/ETPC register space exists only in pre-Hopper chips.
    refManReg = ref_man->FindRegister("LW_PGRAPH_PRI_EGPC0_ETPC0_SM0_DSM_PERF_COUNTER5");
    if (refManReg == NULL)
    {
        TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_GPC0_TPC0_SM0_DSM_PERF_COUNTER5");
    }
    else
    {
        TEST_CTX_OVERRIDE("LW_PGRAPH_PRI_EGPC0_ETPC0_SM0_DSM_PERF_COUNTER5");
    }

#undef TEST_CTX_OVERRIDE

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::Cleanup()
{
    // Handle case if test fails
    FreeCtxOverrideCh();

    return OK;
}

//! \brief Execute few methods of particular class on partolwlar channel
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::ExecClassMethods()
{
    RC rc = OK;
    // Clear notifier from the cpu
    m_notifier.Clear(0);

    if(m_3dAlloc_First.IsSupported(GetBoundGpuDevice()))
    {
        m_pCh->Write(0, LW5097_SET_OBJECT, m_3dAlloc_First.GetHandle());
        CHECK_RC(m_notifier.Instantiate(0, LW5097_SET_CONTEXT_DMA_NOTIFY));
        m_pCh->Write(0, LW5097_NOTIFY, LW5097_NOTIFY_TYPE_WRITE_ONLY);
        m_pCh->Write(0, LW5097_NO_OPERATION, 0);
    }
    else
    {
        CHECK_RC(m_pCh->SetObject(GR_OBJ_SUBCH, m_3dAlloc_Second.GetHandle()));
        CHECK_RC(m_notifier.Instantiate(GR_OBJ_SUBCH, LW9097_SET_NOTIFY_A, LW9097_SET_NOTIFY_B));
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_NOTIFY, LW9097_NOTIFY_TYPE_WRITE_ONLY);
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_NO_OPERATION, 0);
        m_pCh->Write(GR_OBJ_SUBCH, LW906F_SET_REFERENCE,0);// idle graphics
        m_pCh->Write(GR_OBJ_SUBCH, LW906F_FB_FLUSH, 0);    // sysmembar
    }

    m_pCh->Flush();

    // Wait for GPU notifier write
    CHECK_RC(m_notifier.Wait(0, m_TimeoutMs));
    if(m_notifier.IsSet(0) != true)
    {
        Printf(Tee::PriHigh, "Notifier read back through cpu did not match gpu write\n");
        rc  =  RC::NOTIFIER_TIMEOUT;
    }

    return rc;
}
//! \brief Free Allocated channel
//-----------------------------------------------------------------------------
void Ctrl0080GrTest::FreeCtxOverrideCh()
{
    LwRmPtr pLwRm;

    m_notifier.Free();

    m_3dAlloc_First.Free();
    m_3dAlloc_Second.Free();

    pLwRm->Free(m_hObject2);
    m_hObject2 = 0;

    m_TestConfig.FreeChannel(m_pCh);
}

//! \brief Create Channel For testing LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE
//! control cmd.
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::CreateCtxOverrideCh()
{
    RC rc = OK;
    LwRmPtr pLwRm;

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh,
                                          &m_hCh,
                                          LW2080_ENGINE_TYPE_GRAPHICS));

    if(m_3dAlloc_First.IsSupported(GetBoundGpuDevice()))
    {
        CHECK_RC(m_3dAlloc_First.Alloc(m_hCh, GetBoundGpuDevice()));
    }
    else
    {
        CHECK_RC(m_3dAlloc_Second.Alloc(m_hCh, GetBoundGpuDevice()));
        // Allocate object for FERMI_TWOD_A class
        CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObject2,
                              FERMI_TWOD_A, NULL));
    }

    // Allocate notifier
    m_notifier.Allocate(m_pCh, 1, GetTestConfiguration());

    return rc;
}
//!  \brief Test the SET_CONTEXT_OVERRIDE command
//-----------------------------------------------------------------------------
RC Ctrl0080GrTest::TestSetContextOverride(RefManualRegister const *refManReg)
{
    LwRmPtr pLwRm;
    RC rc = OK;
    AcrVerify lsFlcnState;
    UINT32 oldRegData = 0, newRegData = 0;
    LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE_PARAMS setContextOverrideParams = {0};
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    CHECK_RC(lsFlcnState.BindTestDevice(GetBoundTestDevice()));

    //
    // Get the initial value of register which we want to override.
    //
    setContextOverrideParams.regAddr = refManReg->GetOffset();
    oldRegData = pSubdev->RegRd32(setContextOverrideParams.regAddr);

    // get vaild read/write masks for test register used to setup value to write
    LwU32 wrMask = refManReg->GetReadableFieldMask();
    LwU32 rdMask = refManReg->GetWriteableFieldMask();
    LwU32 rwMask = wrMask & rdMask;

    //
    // newvalue = (oldvalue) & (~ andMask) | (orMask)
    //
    setContextOverrideParams.andMask = 0xFFFFFFFF;
    setContextOverrideParams.orMask  = rwMask & 0x12345678;

    //
    // Control cmd to override register value.
    //
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE,
                        &setContextOverrideParams,
                        sizeof (setContextOverrideParams)));

    //
    // Create a new channel (override value of register will update
    // only when if there is new context)
    //
    CHECK_RC(CreateCtxOverrideCh());

    // Check prod settings

    //
    // Execute allocated class method on created channel to make it active)
    //
    CHECK_RC(ExecClassMethods());

    newRegData = pSubdev->RegRd32(setContextOverrideParams.regAddr);

    if (newRegData != setContextOverrideParams.orMask)
    {
        Printf(Tee::PriHigh, "\tExpected addr=0x%08x 0x%08x, Got 0x%x\n",
               setContextOverrideParams.regAddr, setContextOverrideParams.orMask, newRegData);
        rc  =  RC::SOFTWARE_ERROR;
        goto cleanup_ctx;
    }

    // at this point, FECS and GPCS will be booted
    if(lsFlcnState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        RC flcnRc;

        Printf(Tee::PriHigh, "%s: ACR is supported. checking status of FECS and GPCCS Falcons\n",__FUNCTION__);

        //
        // check FECS status. If FECS is managed by LSFM then it should be booted
        // in selwred mode otherwise not. this function verifies that
        //
        CHECK_RC(lsFlcnState.Setup());
        flcnRc = lsFlcnState.VerifyFalconStatus(LSF_FALCON_ID_FECS, LW_TRUE);
        if(flcnRc != OK)
        {
            Printf(Tee::PriHigh, "%s: FECS failed to boot in expected mode\n",__FUNCTION__);
            rc = flcnRc;
            goto cleanup_ctx;
        }

        //
        // Checking selwred status of GPCCS falcon. If it is managed by LSFM
        // then it must be booted in selwred mode
        //
        flcnRc = lsFlcnState.VerifyFalconStatus(LSF_FALCON_ID_GPCCS, LW_TRUE);
        if(flcnRc != OK)
        {
            Printf(Tee::PriHigh, "%s: GPCCS failed to boot in expected mode\n",__FUNCTION__);
            rc = flcnRc;
            goto cleanup_ctx;
        }
    }

    //
    // Free the above allocated channel
    //
    FreeCtxOverrideCh();

    //
    // Retain the initial value of specified register
    //

    setContextOverrideParams.andMask = 0xFFFFFFFF;
    setContextOverrideParams.orMask  = oldRegData;

    //
    // Control cmd to override register value.
    //
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE,
                        &setContextOverrideParams,
                        sizeof (setContextOverrideParams)));

    //
    // Create a new channel (override value of register will update
    // only when if there is new context)
    //
    CHECK_RC(CreateCtxOverrideCh());

    //
    // Execute allocated class method on created channel to make it active)
    //
    CHECK_RC(ExecClassMethods());

    newRegData = pSubdev->RegRd32(setContextOverrideParams.regAddr);

    if (newRegData != oldRegData)
    {
        Printf(Tee::PriHigh, "\tExpected %d, Got %d \n", oldRegData, newRegData);
        rc  =  RC::SOFTWARE_ERROR;
        goto cleanup_ctx;
    }

    // at this point, FECS and GPCS will be booted
    if(lsFlcnState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        RC flcnRc;

        Printf(Tee::PriHigh, "%s: ACR is supported. checking status of FECS and GPCCS Falcons\n",__FUNCTION__);

        //
        // check FECS status. If FECS is managed by LSFM then it should be booted
        // in selwred mode otherwise not. this function verifies that
        //
        CHECK_RC(lsFlcnState.Setup());
        flcnRc = lsFlcnState.VerifyFalconStatus(LSF_FALCON_ID_FECS, LW_TRUE);
        if(flcnRc != OK)
        {
            Printf(Tee::PriHigh, "%s: FECS failed to boot in expected mode\n",__FUNCTION__);
            rc = flcnRc;
            goto cleanup_ctx;
        }

        //
        // Checking selwred status of GPCCS falcon. If it is managed by LSFM
        // then it must be booted in selwred mode
        //
        flcnRc = lsFlcnState.VerifyFalconStatus(LSF_FALCON_ID_GPCCS, LW_TRUE);
        if(flcnRc != OK)
        {
            Printf(Tee::PriHigh, "%s: GPCCS failed to boot in expected mode\n",__FUNCTION__);
            rc = flcnRc;
        }
    }

cleanup_ctx:
    //
    // Free the above allocated channel
    //
    FreeCtxOverrideCh();

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl0080GrTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0080GrTest, RmTest,
                 "Ctrl0080GrTest RM test.");
