/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "vpr.h"

#include "ampere/ga100/dev_fb.h"
#include "core/include/gpu.h"
#include "core/include/pool.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "pascal/gp100/dev_bus.h"
#include "pascal/gp100/dev_graphics_nobundle.h"
#include "pascal/gp100/dev_ram.h"
#include "sim/IChip.h"

#include <sstream>
#include <memory>

// RC helper macro, I wish some other file had this
#define CHECK_RC_MSG_ASSERT(f, msg)                                       \
do {                                                                      \
    if(OK != (rc = (f))) {                                                \
        ErrPrintf("%s: %s at %s %d\n",                                    \
                  msg, rc.Message(), __FILE__, __LINE__);                 \
        MASSERT(!"Generic MODS assert failure<refer to above message>."); \
    }                                                                     \
} while(0)

extern const ParamDecl vpr_params[] =
{
    PARAM_SUBDECL(lwgpu_basetest_ctx_params),
    STRING_PARAM("-interrupt_dir", "Specify the directory path to the interrupt file name used by the trace"),
    STRING_PARAM("-interrupt_file", "Specify the interrupt file name used by the trace."),
    SIMPLE_PARAM("-check_auto_fetch_disabled", "Enables check that auto_fetch_disable is high - Only works on SOC RTL with special fuse image"),
    SIMPLE_PARAM("-check_in_use_is_low", "Enables check that in_use is low - Only works on SOC RTL with special fuse image"),
    LAST_PARAM
};

vprTest::vprTest(ArgReader *reader) :
    //LWGpuSingleChannelTest(reader),
    LWGpuBaseTest(reader),
    lwgpu(NULL),
    m_buffInfo(NULL),
    m_pGpuSub(NULL),
    m_params(reader)
{
    m_buffInfo = new BuffInfo();

    if ( m_params->ParamPresent( "-interrupt_file" ) > 0 )
      m_vprIntFileName = m_pParams->ParamStr( "-interrupt_file" );
    if ( m_params->ParamPresent( "-interrupt_dir" ) > 0 )
      m_vprIntDir = m_pParams->ParamStr( "-interrupt_dir" );
    else
      m_vprIntDir = "./";
    if (m_vprIntDir[m_vprIntDir.size()-1] != '/')
      m_vprIntDir += '/';
    if (m_vprIntFileName.size() > 0 && m_vprIntFileName[0] != '/')
      m_vprIntFileName = m_vprIntDir + m_vprIntFileName;
    m_checkAutoFetchDisabled = (m_params->ParamPresent( "-check_auto_fetch_disabled" ) > 0);
    m_checkInUseIsLow = (m_params->ParamPresent( "-check_in_use_is_low" ) > 0);
}

vprTest::~vprTest(void)
{
    DestroyMembers();

    m_pGpuSub = NULL;
}

STD_TEST_FACTORY(vpr, vprTest)

// a little extra setup to be done
int vprTest::Setup(void)
{

    DebugPrintf("vpr: Setup() starts\n");

    lwgpu = FindGpuForClass(0, NULL);
    MASSERT(lwgpu != NULL);
    m_pGpuSub = lwgpu->GetGpuSubdevice();

    // Setup a state report, so we can pass gold
    getStateReport()->enable();

    // Get 4KB of VPR memory
    m_vprSurf.SetName("VPR Buffer");
    if (SetupSurface(&m_vprSurf, 0x1000, Memory::Fb, !m_checkInUseIsLow))
    {
        ErrPrintf("Cannot setup VPR surface");
        return 0;
    }

    // Get 4KB of non-VPR memory
    m_nolwprSurf.SetName("non-VPR Buffer");
    if (SetupSurface(&m_nolwprSurf, 0x1000, Memory::Fb, false))
    {
        ErrPrintf("Cannot setup non-VPR surface");
        return 0;
    }

    m_bar0WindowSave = 
        lwgpu->GetGpuSubdevice()->Regs().Read32(MODS_XAL_EP_BAR0_WINDOW);

    return (1);
}

void vprTest::CleanUp(void)
{
    DebugPrintf("vpr: CleanUp() starts\n");

    DestroyMembers();

    lwgpu->GetGpuSubdevice()->Regs().Write32(
        MODS_XAL_EP_BAR0_WINDOW, m_bar0WindowSave);

    DebugPrintf("vpr: CleanUp() done\n");
}

//! Run the test
//!
void vprTest::Run(void)
{
    if (m_pGpuSub->DeviceId() < Gpu::GA100)
    {
        return RunLegacy();
    }

    LwU32 temp;
    SetStatus(Test::TEST_INCOMPLETE);
    bool doFault = (m_vprIntFileName.size() > 0);

    InfoPrintf("vpr: Run() starts\n");

    RC rc;

    InfoPrintf("ErrorLogger::StartingTest\n");
    rc = ErrorLogger::StartingTest();
    if (rc != OK)
    {
        ErrPrintf("Error initializing ErrorLogger: %s\n", rc.Message());
        SetStatus(Test::TEST_FAILED);
        return;
    }

    const bool useContextReset = false;

    DisableBackdoor();
    ContextReset(300,useContextReset);

    InfoPrintf("Checking the VPR fetch is complete\n");
    for (int cnt = 0; cnt < 100000; cnt++)
    {
        Tasker::Yield();
        UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
        if ((vpr_info & DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_MODE_FETCH)) ==
                DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _FALSE))
        {
            break;
        }
    }
    UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
    if ((vpr_info & DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_MODE_FETCH)) ==
            DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _TRUE))
    {
        ErrPrintf("VPR fetch is stuck high: vpr_info %08x\n", vpr_info);
        SetStatus(Test::TEST_FAILED);
        return;
    }

    if (m_checkAutoFetchDisabled || m_checkInUseIsLow)
    {
        InfoPrintf("Triggering VPR fetch and checking that it is complete\n");
        UINT32 vpr_fetch = DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _TRUE);
        lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_MODE, vpr_fetch);
        for (int cnt = 0; cnt < 100000; cnt++)
        {
            Tasker::Yield();
            UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
            if ((vpr_info & DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_MODE_FETCH)) ==
                    DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _FALSE))
            {
                break;
            }
        }
        UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
        if ((vpr_info & DRF_SHIFTMASK(LW_PFB_PRI_MMU_VPR_MODE_FETCH)) ==
                DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _TRUE))
        {
            ErrPrintf("VPR fetch is stuck high: vpr_info %08x\n", vpr_info);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    UINT32 cya_reg = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_CYA_LO);
    InfoPrintf("VPR INFO CYA LO: 0x%08x\n",cya_reg);

    if ((cya_reg & DRF_DEF(_PFB_PRI, _MMU_VPR_CYA_LO, _IN_USE, _TRUE)) == 0)
        cya_reg = LW_PFB_PRI_MMU_VPR_CYA_TRUSTED;
    else
    {
        if (cya_reg & DRF_DEF(_PFB_PRI, _MMU_VPR_CYA_LO, _TRUST_DEFAULT, _DEAULT))
            cya_reg = DRF_DEF(_PFB_PRI, _MMU_VPR_CYA_LO, _TRUST_HOST, _DEFAULT);
        cya_reg = (cya_reg & ((2 << (1?LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_HOST))-1)) >> (0?LW_PFB_PRI_MMU_VPR_CYA_LO_TRUST_HOST);
    }
    InfoPrintf("HOST Trust Level: 0x%x\n",cya_reg);

    if (cya_reg != LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) //Check read and write of non-VPR, unless QUARANTINE
    {
        SetBar0Window(m_nolwprSurf, 0);
        lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xfeedface);
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if (temp != 0xfeedface)
        {
            ErrPrintf("Error on first accesses to non-VPR surface: temp %08x != 0xfeedface\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    SetBar0Window(m_vprSurf, 0); // Writes to VPR never cause an issue.
    lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xdeadbeef);
    if (doFault || (cya_reg == LW_PFB_PRI_MMU_VPR_CYA_TRUSTED))  // If generating a fault, or if TRUSTED read VPR.
    {
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if ((cya_reg != LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) && (temp != 0xdeadbeef)) //UNTRUSTED will fault on the read, and the data will be bad.
        {
            ErrPrintf("Error on accesses to VPR surface: temp %08x != 0xdeadbeef\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    if ((cya_reg == LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) || (cya_reg == LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS)) //Use non-VPR surface to wait for recovery to complete
    {
        if (cya_reg == LW_PFB_PRI_MMU_VPR_CYA_QUARANTINE) //Make sure context reset does not allow QUARANTINE to access non-vpr.
            ContextReset(300,useContextReset);
        if (doFault) // If QUARANTINE or GRAPHICS and testing faults, write outside of VPR to cause the fault.
        {
            SetBar0Window(m_nolwprSurf, 0);
            lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xf00dd00d);
            temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
            if (temp == 0xf00dd00d) //Make sure write did not happen.
            {
                ErrPrintf("Error on accesses to VPR surface (CONTENT LEAKED): temp %08x == 0xf00dd00d\n", temp);
                SetStatus(Test::TEST_FAILED);
                return;
            }
            SetBar0Window(m_vprSurf, 0); //Use VPR to wait for recovery.
        }
    }
    else
    {
        SetBar0Window(m_nolwprSurf, 0);  //Use non-VPR to wait for recovery.
    }
    for (int cnt = 0; cnt < 10000; cnt++)
    {
        Tasker::Yield();
        lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xfacecafe);
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if ((temp == 0xfacecafe) && (!doFault || (cnt > 2)))
            break;
    }
    if (temp != 0xfacecafe)
    {
        ErrPrintf("Error waiting for recovery: temp %08x != 0xfacecafe\n", temp);
        SetStatus(Test::TEST_FAILED);
        return;
    }
    ContextReset(300,useContextReset);
    if (cya_reg == LW_PFB_PRI_MMU_VPR_CYA_GRAPHICS) //Check that context reset worked
    {
        SetBar0Window(m_nolwprSurf, 0);  //Use non-VPR to wait for recovery.
        for (int cnt = 0; cnt < 10000; cnt++)
        {
            Tasker::Yield();
            lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xabcdabcd);
            temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
            if ((temp == 0xfacecafe) && (cnt > 2))
                break;
        }
        if (temp != 0xabcdabcd)
        {
            ErrPrintf("Error on accesses to non-VPR surface after context reset: temp %08x != 0xabcdabcd\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    EnableBackdoor();

    if (doFault)
    {
        if (CheckIntrPassFail() != TestEnums::TEST_SUCCEEDED)
        {
            InfoPrintf("frontdoor: Failed interrupt check\n");
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    SetStatus(Test::TEST_SUCCEEDED);
    InfoPrintf("ErrorLogger::TestCompleted\n");
}

void vprTest::RunLegacy(void)
{
    LwU32 temp;
    SetStatus(Test::TEST_INCOMPLETE);
    bool doFault = (m_vprIntFileName.size() > 0);

    InfoPrintf("vpr: Run() starts\n");

    RC rc;

    InfoPrintf("ErrorLogger::StartingTest\n");
    rc = ErrorLogger::StartingTest();
    if (rc != OK)
    {
        ErrPrintf("Error initializing ErrorLogger: %s\n", rc.Message());
        SetStatus(Test::TEST_FAILED);
        return;
    }

    GpuDevice *gpudev = lwgpu->GetGpuDevice();
    bool useContextReset = (gpudev->GetFamily() == GpuDevice::Kepler) || (gpudev->GetFamily() == GpuDevice::Maxwell);

    DisableBackdoor();
    ContextReset(300,useContextReset);

    InfoPrintf("Checking the VPR fetch is complete\n");
    for (int cnt = 0; cnt < 100000; cnt++)
    {
        Tasker::Yield();
        UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
        if ((vpr_info & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _FETCH, ~0)) == DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _FETCH, _FALSE))
        {
            break;
        }
    }
    UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
    if ((vpr_info & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _FETCH, ~0)) == DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _FETCH, _TRUE))
    {
        ErrPrintf("VPR fetch is stuck high: vpr_info %08x\n", vpr_info);
        SetStatus(Test::TEST_FAILED);
        return;
    }

    if (m_checkAutoFetchDisabled || m_checkInUseIsLow)
    {
        lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_INFO, 0);
        for (int cnt = 0; cnt < 4; cnt++)
        {
            UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
            if ((vpr_info & ~(DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _INDEX, ~0))) != 0)
            {
                ErrPrintf("Failed in_use or auto_fetch_disable check: vpr_info %08x\n", vpr_info);
                SetStatus(Test::TEST_FAILED);
                return;
            }
        }
        InfoPrintf("Triggering VPR fetch and checking that it is complete\n");
        UINT32 vpr_fetch = DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _FETCH, _TRUE);
        lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_INFO, vpr_fetch);
        for (int cnt = 0; cnt < 100000; cnt++)
        {
            Tasker::Yield();
            UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
            if ((vpr_info & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _FETCH, ~0)) == DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _FETCH, _FALSE))
            {
                break;
            }
        }
        UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
        if ((vpr_info & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _FETCH, ~0)) == DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _FETCH, _TRUE))
        {
            ErrPrintf("VPR fetch is stuck high: vpr_info %08x\n", vpr_info);
            SetStatus(Test::TEST_FAILED);
            return;
        }
        if (m_checkInUseIsLow)
        {
            lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_INFO, 0);
            for (int cnt = 0; cnt < 4; cnt++)
            {
                UINT32 vpr_info = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
                if ((vpr_info & ~(DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _INDEX, ~0))) != 0)
                {
                    ErrPrintf("Failed in_use check: vpr_info %08x\n", vpr_info);
                    SetStatus(Test::TEST_FAILED);
                    return;
                }
            }
        }
    }

    UINT32 cya_reg = DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _INDEX, _CYA_LO);
    lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_INFO, cya_reg);

    cya_reg = lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
    InfoPrintf("VPR INFO CYA LO: 0x%08x\n",cya_reg);

    if ((cya_reg & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _CYA_LO_IN_USE, 1)) == 0)
        cya_reg = LW_PFB_PRI_MMU_VPR_INFO_CYA_TRUSTED;
    else
    {
        if ((cya_reg & DRF_NUM(_PFB_PRI, _MMU_VPR_INFO, _CYA_LO_TRUST_DEFAULT, 1)) != 0)
            cya_reg = DRF_DEF(_PFB_PRI, _MMU_VPR_INFO, _CYA_LO_TRUST_HOST, _DEFAULT);
        cya_reg = (cya_reg & ((2 << (1?LW_PFB_PRI_MMU_VPR_INFO_CYA_LO_TRUST_HOST))-1)) >> (0?LW_PFB_PRI_MMU_VPR_INFO_CYA_LO_TRUST_HOST);
    }
    InfoPrintf("HOST Trust Level: 0x%x\n",cya_reg);

    if (cya_reg != LW_PFB_PRI_MMU_VPR_INFO_CYA_QUARANTINE) //Check read and write of non-VPR, unless QUARANTINE
    {
        SetBar0Window(m_nolwprSurf, 0);
        lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xfeedface);
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if (temp != 0xfeedface)
        {
            ErrPrintf("Error on first accesses to non-VPR surface: temp %08x != 0xfeedface\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    SetBar0Window(m_vprSurf, 0); // Writes to VPR never cause an issue.
    lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xdeadbeef);
    if (doFault || (cya_reg == LW_PFB_PRI_MMU_VPR_INFO_CYA_TRUSTED))  // If generating a fault, or if TRUSTED read VPR.
    {
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if ((cya_reg != LW_PFB_PRI_MMU_VPR_INFO_CYA_UNTRUSTED) && (temp != 0xdeadbeef)) //UNTRUSTED will fault on the read, and the data will be bad.
        {
            ErrPrintf("Error on accesses to VPR surface: temp %08x != 0xdeadbeef\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    if ((cya_reg == LW_PFB_PRI_MMU_VPR_INFO_CYA_QUARANTINE) || (cya_reg == LW_PFB_PRI_MMU_VPR_INFO_CYA_GRAPHICS)) //Use non-VPR surface to wait for recovery to complete
    {
        if (cya_reg == LW_PFB_PRI_MMU_VPR_INFO_CYA_QUARANTINE) //Make sure context reset does not allow QUARANTINE to access non-vpr.
            ContextReset(300,useContextReset);
        if (doFault) // If QUARANTINE or GRAPHICS and testing faults, write outside of VPR to cause the fault.
        {
            SetBar0Window(m_nolwprSurf, 0);
            lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xf00dd00d);
            temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
            if (temp == 0xf00dd00d) //Make sure write did not happen.
            {
                ErrPrintf("Error on accesses to VPR surface (CONTENT LEAKED): temp %08x == 0xf00dd00d\n", temp);
                SetStatus(Test::TEST_FAILED);
                return;
            }
            SetBar0Window(m_vprSurf, 0); //Use VPR to wait for recovery.
        }
    }
    else
    {
        SetBar0Window(m_nolwprSurf, 0);  //Use non-VPR to wait for recovery.
    }
    for (int cnt = 0; cnt < 10000; cnt++)
    {
        Tasker::Yield();
        lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xfacecafe);
        temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
        if ((temp == 0xfacecafe) && (!doFault || (cnt > 2)))
            break;
    }
    if (temp != 0xfacecafe)
    {
        ErrPrintf("Error waiting for recovery: temp %08x != 0xfacecafe\n", temp);
        SetStatus(Test::TEST_FAILED);
        return;
    }
    ContextReset(300,useContextReset);
    if (cya_reg == LW_PFB_PRI_MMU_VPR_INFO_CYA_GRAPHICS) //Check that context reset worked
    {
        SetBar0Window(m_nolwprSurf, 0);  //Use non-VPR to wait for recovery.
        for (int cnt = 0; cnt < 10000; cnt++)
        {
            Tasker::Yield();
            lwgpu->RegWr32(LW_PRAMIN_DATA008(0), 0xabcdabcd);
            temp = lwgpu->RegRd32(LW_PRAMIN_DATA008(0));
            if ((temp == 0xfacecafe) && (cnt > 2))
                break;
        }
        if (temp != 0xabcdabcd)
        {
            ErrPrintf("Error on accesses to non-VPR surface after context reset: temp %08x != 0xabcdabcd\n", temp);
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    EnableBackdoor();

    if (doFault)
    {
        if (CheckIntrPassFail() != TestEnums::TEST_SUCCEEDED)
        {
            InfoPrintf("frontdoor: Failed interrupt check\n");
            SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    SetStatus(Test::TEST_SUCCEEDED);
    InfoPrintf("ErrorLogger::TestCompleted\n");
}

//! \brief Enables the backdoor access
void vprTest::EnableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void vprTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}

//! Destroys all class members
void vprTest::DestroyMembers(void)
{
    if(m_buffInfo)
    {
        delete m_buffInfo;
        m_buffInfo = NULL;
    }

    if(m_vprSurf.GetSize())
    {
        m_vprSurf.Free();
    }

    if(m_nolwprSurf.GetSize())
    {
        m_nolwprSurf.Free();
    }

}

int vprTest::SetupSurface(MdiagSurf * buffer, UINT32 size,
                                 Memory::Location memLoc, bool bProtected)
{

    MASSERT(buffer != NULL);

    if(buffer == NULL)
    {
        ErrPrintf("vpr::SetupSurface - Buffer passed is null.\n");
        return 1;
    }

    if (m_pGpuSub->IsFbBroken() && (memLoc == Memory::Fb))
    {
        memLoc = Memory::NonCoherent;
    }

    buffer->SetWidth(size);
    buffer->SetHeight(1);
    // Note this is ignored if bProtected is TRUE
    buffer->SetLocation(memLoc);
    buffer->SetColorFormat(ColorUtils::Y8);
    buffer->SetProtect(Memory::ReadWrite);
    buffer->SetAlignment(0x10000);
    buffer->SetLayout(Surface::Pitch);
    buffer->SetPhysContig(true); // Want all memory physically contiguous if we're using it through BAR0 Window
    // VPR or not
    buffer->SetVideoProtected(bProtected);

    buffer->SetPageSize(4);

    if(OK != buffer->Alloc(lwgpu->GetGpuDevice()))
    {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("vpr: Cannot create dst buffer.\n");
        return 1;
    }

    DebugPrintf( "Surf2D allocated a surface:\n");
    DebugPrintf( "  Size = 0x%08x\n", buffer->GetSize());
    DebugPrintf( "  hMem = 0x%08x\n", buffer->GetMemHandle());
    DebugPrintf( "  hVirtMem = 0x%08x\n", buffer->GetVirtMemHandle());
    DebugPrintf( "  hCtxDmaGpu = 0x%08x\n", buffer->GetCtxDmaHandle());
    DebugPrintf( "  OffsetGpu = 0x%llx\n", buffer->GetCtxDmaOffsetGpu());
    DebugPrintf( "  hCtxDmaIso = 0x%08x\n", buffer->GetCtxDmaHandleIso());
    DebugPrintf( "  OffsetIso = 0x%llx\n", buffer->GetCtxDmaOffsetIso());
    DebugPrintf( "  Protected = %s\n", buffer->GetVideoProtected() ? "true" : "false");
    // print this only when we did a VidHeapAlloc
    Printf(Tee::PriDebug, "  VidMemOffset = 0x%08llx\n",
           buffer->GetVidMemOffset());

    // Add surface info to buffInfo
    m_buffInfo->SetRenderSurface(*buffer);

    return 0;
}

uint vprTest::GetBar0WindowTarget(const MdiagSurf& surface)
{

    uint target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
    switch(surface.GetLocation())
    {
    case Memory::Fb:
        target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
        break;
    case Memory::Coherent:
        target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT;
        break;
    case Memory::NonCoherent:
        target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT;
        break;
    case Memory::Optimal:
    default:
        MASSERT(0 && "Unknown memory location.");
        break;
    }

    return target;
}

void vprTest::SetBar0Window(const MdiagSurf& surface, LwU32 offset)
{
    PHYSADDR physAddr = 0;
    surface.GetPhysAddress(offset, &physAddr);
    RegHal& regHal = lwgpu->GetGpuSubdevice()->Regs();

    UINT32 writeValue = regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, 
        static_cast<UINT32>(
            physAddr >> regHal.LookupAddress(MODS_XAL_EP_BAR0_WINDOW_BASE_SHIFT)));

    if (lwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Hopper)
    {
        writeValue |= regHal.SetField(
            MODS_PBUS_BAR0_WINDOW_TARGET, GetBar0WindowTarget(surface));
    }
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, writeValue);
}

bool IntrErrorFilter(UINT64 intr, const char *lwrErr, const char *expectedErr);
TestEnums::TEST_STATUS vprTest::CheckIntrPassFail()
{
    TestEnums::TEST_STATUS result = TestEnums::TEST_SUCCEEDED;

    if(lwgpu->SkipIntrCheck())
    {
        InfoPrintf("vprTest::CheckIntrPassFail: Skipping intr check per commandline override!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
        return result;
    }

    const char *intr_file = m_vprIntFileName.c_str();
    FILE *wasFilePresent = NULL;

    if(ErrorLogger::HasErrors())
    {
        ErrorLogger::InstallErrorFilter(IntrErrorFilter);
        RC rc = ErrorLogger::CompareErrorsWithFile(intr_file, ErrorLogger::Exact);

        if(rc != OK)
        {
            if(Utility::OpenFile(intr_file, &wasFilePresent, "r") != OK)
            {
                ErrPrintf("vprTest::checkIntrPassFail: interrupts detected, but %s file not found.\n", intr_file);
                ErrorLogger::PrintErrors(Tee::PriNormal);
                ErrorLogger::TestUnableToHandleErrors();
            }
            result = TestEnums::TEST_FAILED_CRC;
        }
    }
    else
    {
        // Don't use Utility::OpenFile since it prints an error if it can't
        // find the file...confuses people
        wasFilePresent = fopen(intr_file, "r");
        if(wasFilePresent)
        {
            ErrPrintf("vprTest::checkIntrPassFail: NO error interrupts detected, but %s file exists!\n",
                intr_file);
            result = TestEnums::TEST_FAILED_CRC;
        }
    }

    if(wasFilePresent) fclose(wasFilePresent);

    return result;
}

void vprTest::ContextReset(int count, bool useContextReset)
{
#if (defined(LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE_PROTECTED_MODE) && \
    (defined(LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_PROTECTED_MODE) || defined(LW_PFB_PRI_MMU_VPR_MODE_VPR_PROTECTED_MODE)))
  if (useContextReset) {
#endif
    UINT32 ctxsw_reset = lwgpu->RegRd32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);
    ctxsw_reset &= ~DRF_NUM(_PGRAPH_PRI, _FECS_CTXSW_RESET_CTL, _SYS_CONTEXT_RESET, 1);
    lwgpu->RegWr32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL, ctxsw_reset | DRF_DEF(_PGRAPH_PRI, _FECS_CTXSW_RESET_CTL, _SYS_CONTEXT_RESET, _ENABLED));
    while (count > 0)
    {
        count--;
    }
    lwgpu->RegWr32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL, ctxsw_reset | DRF_DEF(_PGRAPH_PRI, _FECS_CTXSW_RESET_CTL, _SYS_CONTEXT_RESET, _DISABLED));
    if (m_pGpuSub->DeviceId() >= Gpu::GA100)
    {
        lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
    }
    else
    {
        lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
    }
#if (defined(LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE_PROTECTED_MODE) && \
    (defined(LW_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_PROTECTED_MODE) || defined(LW_PFB_PRI_MMU_VPR_MODE_VPR_PROTECTED_MODE)))
  } else {
        if (m_pGpuSub->DeviceId() >= Gpu::GA100)
        {
            lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_MODE,
                           DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _FETCH, _FALSE) |
                           DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _UPDATE_MODE, _TRUE) |
                           DRF_DEF(_PFB_PRI, _MMU_VPR_MODE, _VPR_PROTECTED_MODE, _FALSE));
            lwgpu->RegWr32(LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE,
                           DRF_DEF(_PGRAPH_PRI_GPCS, _MMU_VPR_WPR_WRITE, _PROTECTED_MODE, _FALSE));
            while (count > 0)
            {
                count--;
            }
            lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_MODE);
        }
        else
        {
            lwgpu->RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE,
                DRF_DEF(_PFB_PRI, _MMU_VPR_WPR_WRITE, _INDEX, _VPR_PROTECTED_MODE) |
                DRF_DEF(_PFB_PRI, _MMU_VPR_WPR_WRITE, _VPR_PROTECTED_MODE, _FALSE));
            lwgpu->RegWr32(LW_PGRAPH_PRI_GPCS_MMU_VPR_WPR_WRITE,
                           DRF_DEF(_PGRAPH_PRI_GPCS, _MMU_VPR_WPR_WRITE, _PROTECTED_MODE, _FALSE));
            while (count > 0)
            {
                count--;
            }
            lwgpu->RegRd32(LW_PFB_PRI_MMU_VPR_INFO);
        }
#endif
  }
}
