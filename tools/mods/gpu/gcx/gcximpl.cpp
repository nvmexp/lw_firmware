/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/****************************************************************************************
  This code implements the GC5 & GC6 power saving features of the GPU. To utilize this
  implementor your code should look something like this:
  GCxImpl * pGCx = m_pSubdev->GetGCx();
  pGCx->InitGC5()
  pGCx->InitGC6()
  if(pGCx && pGCx->IsSupported(pstate))
  {
    for(;;)
    {
        CHECK_RC(m_pGCx->DoEnterGc5(m_WakeupEvent,m_EnterDelayUsec))
        CHECK_RC(m_pGCx->DoExitGc5(m_ExitDelayUsec));
    }
    pGCx->Shutdown();
  }
  Where m_WakeupEvent can be one of the following events:
  Event                              Value   GC5 GC6     Description
  -------------------------------    ----    --- ---     ----------------------------------------------
  GC_CMN_WAKEUP_EVENT_gc6gpu         (1<<0)       x      // ask the EC to generate a gpu event
  GC_CMN_WAKEUP_EVENT_gc6i2c         (1<<1)       x      // read the therm sensor via I2C
  GC_CMN_WAKEUP_EVENT_gc6timer       (1<<2)       x      // program the SCI timer to wakeup the gpu
  GC_CMN_WAKEUP_EVENT_gc6hotplug     (1<<3)       x      // ask the EC to simulate a hot plug detect
  GC_CMN_WAKEUP_EVENT_gc6hotplugirq  (1<<4)       x      // ask the EC to simulate a hot plug detect IRQ
  GC_CMN_WAKEUP_EVENT_gc5rm          (1<<8)   x          // force wakeup via RM ctrl call.
  GC_CMN_WAKEUP_EVENT_gc5pex         (1<<9)   x          // read a GPU register to generate PEX traffic
  GC_CMN_WAKEUP_EVENT_gc5i2c         (1<<10)  x          // read the therm sensor via I2C
  GC_CMN_WAKEUP_EVENT_gc5timer       (1<<11)  x          // program the SCI timer to wakeup the gpu
  GC_CMN_WAKEUP_EVENT_gc5alarm       (1<<12)  x          // schedule a PTIMER event

  This implementor supports both DI-OS and DI-SSC power features exclusive. You specify which mode
  by setting the m_GC5Mode variable.
***************************************************************************************************/
#include "gpu/include/gcximpl.h"

#include "class/cl0005.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl2080/ctrl2080power.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "ctrl/ctrl2080/ctrl2080tmr.h"
#include "device/interface/pcie.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/fuse/fuse.h"
#include "gpu/gc6/ec_info.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/pcicfg.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/perfsub.h"
#include "gpu/utility/bglogger.h"
#include "hwref/maxwell/gm108/dev_gc6_island.h"
#include "lwRmReg.h"       // for LW_REG_STR_RM_POWER_FEATURES2
// For restoring Azalia on the GPU
#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrl.h"
#include "device/azalia/azactrlmgr.h"
#else
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "ctrl/ctrl208f/ctrl208fgpu.h"
#endif
#if !defined(LW_WINDOWS) && !defined(LW_MACINTOSH) || defined(WIN_MFG)
#include "device/smb/smbmgr.h"
#include "device/smb/smbport.h"
#else
#include "stub/smb_stub.h"
#endif

GCxImpl::GCxImpl(GpuSubdevice *pSubdev):
 m_pSubdev(pSubdev)
,m_pLwRm(nullptr)
,m_WakeupEvent(GC_CMN_WAKEUP_EVENT_gc5timer)
,m_WakeupReason(0)
,m_WakeupGracePeriodMs(5000)
,m_WakeupParams({})
,m_EnterTimeMs(0)
,m_EnterDelayUsec(0)
,m_ExitTimeMs(0)
,m_ExitDelayUsec(0)
,m_VerbosePri(Tee::PriLow)
,m_Gc5InitDone(false)
,m_Gc5Mode(gc5DI_OS)
,m_pGpuRdyMemNotifier(nullptr)
,m_pEventGpuReady(nullptr)
,m_hRmEventGpuReady(0)
,m_tmrCallback({})
,m_hRmEventTimer(0)
,m_PtimerAlarmFired(false)
,m_bSmbInitDone(false)
,m_pSmbPort(nullptr)
,m_bThermSensorFound(false)
,m_ThermAddr(0)
,m_Gc5DeepIdle({})
,m_Gc5DeepIdleStats({})
,m_Gc6InitDone(false)
,m_SkipSwFuseCheck(false)
,m_UseJTMgr(true)
,m_UseOptimus(false)
,m_Gc6ExitRequirePostProcessing(false)
,m_pJT(nullptr)
,m_EcFsmMode(0)
,m_Gc6Pstate(Perf::ILWALID_PSTATE)
,m_pPciCfgSpaceGpu(nullptr)
,m_PTimerEntry(0)
,m_PTimerExit(0)
,m_CfgRdyTimeoutMs(Tasker::NO_TIMEOUT)
,m_IsGc5Active(false)
{
    LwRmPtr RmPtr;
    m_pLwRm = RmPtr.Get();
}

//--------------------------------------------------------------------------------------------------
// Destructor
/* virtual */
GCxImpl::~GCxImpl()
{
}

//--------------------------------------------------------------------------------------------------
// Return true if the therm sensor was found on the SMBus
bool GCxImpl::IsThermSensorFound()
{
    if (!m_bSmbInitDone)
        InitializeSmbusDevice();
    return m_bThermSensorFound;
}

//--------------------------------------------------------------------------------------------------
// Check the fake thermal sensor enable status and if it is supported on this GPU.
RC GCxImpl::IsFakeThermSensorEnabled(bool *pbEnable, bool *pbSupported)
{
    RC rc;
    LW2080_CTRL_CMD_LPWR_FAKE_I2CS_GET_PARAMS params = {0};

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_LPWR_FAKE_I2CS_GET,
           &params,
           sizeof(params)));
    *pbEnable = !!params.bFakeI2csEnabled;
    *pbSupported = !!params.bFakeI2csSupported;
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Enable or disable the fake thermal sensor
RC GCxImpl::EnableFakeThermSensor(bool bEnable)
{
    LW2080_CTRL_CMD_LPWR_FAKE_I2CS_SET_PARAMS params = {bEnable};

    return m_pLwRm->ControlBySubdevice(m_pSubdev,
            LW2080_CTRL_CMD_LPWR_FAKE_I2CS_SET,
            &params,
            sizeof(params));
}

//--------------------------------------------------------------------------------------------------
// When RM sets a particular event it can also update the event notifier memory with one 16bit + one
// 32bit parameter. The contents of these parameters differ for each event. However you must
// allocate enough memory for *all* notifier events.
// We need to access the 16bit parameter for the GPU_READY_EVENT to get the reason for why the event
// was set (see GpuReadyMemNotifyHandler() above).
RC GCxImpl::SetGpuRdyEventMemNotifier()
{
    RC rc;

    // Setup the notifier memory
    // Setup the surface for the Event queues
    UINT32 eventMemSize = sizeof(LwNotification) * LW2080_NOTIFIERS_MAXCOUNT;
    eventMemSize = ALIGN_UP(eventMemSize,ColorUtils::PixelBytes(ColorUtils::VOID32));
    m_GpuRdyMemNotifier.SetWidth(eventMemSize / ColorUtils::PixelBytes(ColorUtils::VOID32));
    m_GpuRdyMemNotifier.SetPitch(eventMemSize);
    m_GpuRdyMemNotifier.SetHeight(1);
    m_GpuRdyMemNotifier.SetColorFormat(ColorUtils::VOID32);
    m_GpuRdyMemNotifier.SetLocation(Memory::Coherent);
    m_GpuRdyMemNotifier.SetAddressModel(Memory::Paging);
    m_GpuRdyMemNotifier.SetKernelMapping(true);
    CHECK_RC(m_GpuRdyMemNotifier.Alloc(m_pSubdev->GetParentDevice()));
    CHECK_RC(m_GpuRdyMemNotifier.Fill(0,m_pSubdev->GetSubdeviceInst()));
    CHECK_RC(m_GpuRdyMemNotifier.Map(m_pSubdev->GetSubdeviceInst()));

    // Update our internal pointer so we can read the wakeup event reasons
    m_pGpuRdyMemNotifier = (LwNotification *)m_GpuRdyMemNotifier.GetAddress();

    // set event notifer memory
    LW2080_CTRL_EVENT_SET_MEMORY_NOTIFIES_PARAMS memNotifiesParams = {0};
    memNotifiesParams.hMemory = m_GpuRdyMemNotifier.GetMemHandle();
    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                            LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES,
                            &memNotifiesParams,sizeof(memNotifiesParams)));
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Perform the necessary initialization for GC5 cycles.
RC GCxImpl::InitGc5()
{
    RC rc;
    if (!m_Gc5InitDone)
    {
        if (!m_pLwRm)
        {
            LwRmPtr RmPtr;
            m_pLwRm = RmPtr.Get();
        }

        // Verify up/down stream devices support L1 substate
        CHECK_RC(VerifyASPM());

        // Activate GC5 to prevent thrashing of the PMUs overlays.
        CHECK_RC(ActivateGc5(true));

        if (m_Gc5Mode == gc5DI_SSC)
        {
            CHECK_RC(SetGpuRdyEventMemNotifier());

            // Alloc and register for the LW2080_NOTIFIERS_GC5_GPU_READY event
            //
            const bool bManualReset = true;
            LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS   notifParams = {0};
            m_pEventGpuReady = Tasker::AllocEvent("GpuGc5EventGpuReady",bManualReset);
            CHECK_RC(m_pLwRm->AllocEvent(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                           &m_hRmEventGpuReady,
                           LW01_EVENT_OS_EVENT,
                           LW2080_NOTIFIERS_GC5_GPU_READY,
                           m_pEventGpuReady));

            // Set the event
            notifParams.event  = LW2080_NOTIFIERS_GC5_GPU_READY;
            notifParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;
            CHECK_RC(m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                                    LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                    &notifParams, sizeof(notifParams)));

            // Alloc and register for the LW2080_NOTIFIERS_TIMER event for the
            // PTimerAlarm event
            // Set a callback to fire immediately
            memset(&m_tmrCallback, 0, sizeof(m_tmrCallback));
            m_tmrCallback.func = PtimerAlarmHandler;
            m_tmrCallback.arg = this;

            LW0005_ALLOC_PARAMETERS         allocParams;
            memset(&allocParams, 0, sizeof(allocParams));
            allocParams.hParentClient = m_pLwRm->GetClientHandle();
            allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
            allocParams.notifyIndex   = LW2080_NOTIFIERS_TIMER;
            allocParams.data          = LW_PTR_TO_LwP64(&m_tmrCallback);

            CHECK_RC(m_pLwRm->Alloc(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                                  &m_hRmEventTimer,
                                  LW01_EVENT_KERNEL_CALLBACK_EX,
                                  &allocParams));
        }

        if (!m_bSmbInitDone)
            CHECK_RC(InitializeSmbusDevice());

        m_Gc5InitDone = true;
    }
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Shutdown GCx and free up internal resources.
RC GCxImpl::Shutdown()
{
    StickyRC rc;
    //TODO: If you call InitGc5() followed by Shutdown() without doing any GC5 cycles then we get a
    //      pmu halt. I need to discuss this with Devendra.
    if (m_IsGc5Active)
    {
        ActivateGc5(false);
    }

    if (m_bSmbInitDone)
    {
        SmbManager  *pSmbMgr = SmbMgr::Mgr();
        pSmbMgr->UninitializeAll();
        pSmbMgr->Forget();
        m_bSmbInitDone = false;
    }

    if (m_Gc5InitDone)
    {
        // Free the GPU_READY notifier
        if (m_hRmEventGpuReady)
        {
            // Disable the event
            //LwRmPtr pLwRm;
            LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS   notifParams = {0};
            notifParams.event  = LW2080_NOTIFIERS_GC5_GPU_READY;
            notifParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE;
            rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                                    LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                    &notifParams, sizeof(notifParams));

            m_pLwRm->Free(m_hRmEventGpuReady);
            m_hRmEventGpuReady = 0;

            Tasker::FreeEvent(m_pEventGpuReady);
            m_pEventGpuReady = nullptr;
        }

        if (m_pGpuRdyMemNotifier)
        {
            m_GpuRdyMemNotifier.Unmap();
            m_GpuRdyMemNotifier.Free();
            m_pGpuRdyMemNotifier = nullptr;
        }

        if (m_hRmEventTimer)
        {
            m_pLwRm->Free(m_hRmEventTimer);
            m_hRmEventTimer = 0;
        }
        m_Gc5InitDone = false;
    }

    if (m_Gc6InitDone)
    {
        // Reset GC6 specific resources
        m_Gc6ExitRequirePostProcessing = false;
        m_pJT->SetLwrrentFsmMode(m_OrigFsmMode);
        m_Gc6InitDone = false;
    }
    return rc;
}
//------------------------------------------------------------------------------
// Activate GC5 functionality. This prevents thrashing of the PMUs overlays.
//
RC GCxImpl::ActivateGc5( bool bActivate)
{
    RC rc;
    // Don't activate if mode == gc5DI_OS
    if (m_Gc5Mode == gc5DI_SSC)
    {
        LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS  pwrParams;
        memset(&pwrParams, 0, sizeof(pwrParams));
        pwrParams.action = bActivate ?
            LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ACTIVATE :
            LW2080_CTRL_GPU_POWER_ON_OFF_GC5_DEACTIVATE;
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                (UINT32)LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
                &pwrParams,sizeof (pwrParams)));
        m_IsGc5Active = bActivate;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Initialize the 1st Smb device found on any of the SMBUS ports.
RC GCxImpl::InitializeSmbusDevice()
{
    RC rc;

    if (m_pJT && !m_pJT->IsSMBusSupported())
    {
        return rc;
    }

    SmbManager  *pSmbMgr = SmbMgr::Mgr();
    SmbDevice   *pSmbDev = nullptr;

    m_bThermSensorFound = false;

    //This define is the same for all GPUs,however I don't want to pollute
    //GpuSubdevice with yet another API to get the internal sensors I2C address
    //and then implement that API for all GpuSubdevice families. So I'm stealing
    //the define from dev_therm.h and using it here.
    #define LW_THERM_I2CS_STATE             0x00020084 /* R-I4R */
    #define LW_THERM_I2CS_STATE_VALUE_SLAVE_ADR    6:0 /*       */

    m_ThermAddr = DRF_NUM(_THERM, _I2CS_STATE, _VALUE_SLAVE_ADR,
                          m_pSubdev->RegRd32(LW_THERM_I2CS_STATE));

    CHECK_RC(pSmbMgr->Find());
    CHECK_RC(pSmbMgr->InitializeAll());
    CHECK_RC(pSmbMgr->GetLwrrent((McpDev**)&pSmbDev));

    if (nullptr != pSmbDev)
    {
        // Search for SMBus EC on each port
        UINT32 numSubdev = 0;
        pSmbDev->GetNumSubDev(&numSubdev);
        for (UINT32 portNumber = 0; portNumber < numSubdev; portNumber++)
        {
            //Calling GetSubDev with portNumber since the ports are stored in order
            CHECK_RC(pSmbDev->GetSubDev(portNumber, &m_pSmbPort));

            // Read the internal therm sensor.If no errors are reported, then a
            // device exists at the target address.
            UINT08 value =0;
            rc = ReadThermSensor(&value,10);
            if (OK == rc)
            {
                Printf(m_VerbosePri,
                    "Therm sensor found on Port:%d Addr:0x%x Value:0x%x\n",
                    portNumber, m_ThermAddr, value);

                m_bThermSensorFound = true;
                break;
            }
            else
            {
                rc.Clear();
            }
        }
    }

    if (!m_bThermSensorFound)
    {
        Printf(m_VerbosePri,"Therm sensor at adddr:0x%x not found on Smbus\n", m_ThermAddr);
    }
    m_bSmbInitDone = true;
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Called by RM when the timer alarm is event is set.
void GCxImpl::PtimerAlarmHandler(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    GCxImpl *pGcx = (GCxImpl*)pArg;
    pGcx->m_PtimerAlarmFired = true;
}

//--------------------------------------------------------------------------------------------------
// Called by ExitGc5() to wait for the timer alarm to be set.
bool GCxImpl::PollPtimerAlarmFired(void *pArg)
{
    GCxImpl *pGcx = (GCxImpl*)pArg;
    return (pGcx->m_PtimerAlarmFired == true);
}

//! \brief GPU ready handler
//!
//! Ignore the Args struct because mods events don't support data with them.
//! Data is obtained from separate surface.
//--------------------------------------------------------------------------------------------------
void GCxImpl::GpuReadyMemNotifyHandler()
{
    UINT32 eventType =
        MEM_RD16(&m_pGpuRdyMemNotifier[LW2080_NOTIFIERS_GC5_GPU_READY].info16);
    Printf(m_VerbosePri,"GpuReadyMemNotifyHandler : Received EventType: %d\n", eventType);

    switch (eventType)
    {
        case LW2080_GC5_EXIT_COMPLETE:
            Printf(m_VerbosePri,"GpuReadyNotifyHandler : Received GC5 Exit complete notification\n");
            break;

        case LW2080_GC5_ENTRY_ABORTED:
            Printf(m_VerbosePri,"GpuReadyNotifyHandler : Received GC5 Entry Abort notification\n");
            break;

        default:
            Printf(m_VerbosePri,"GpuReadyNotifyHandler : Unknown gpu_rdy notification sent from RM\n");
            break;
    }
    m_WakeupReason = eventType;
    MEM_WR16(&m_pGpuRdyMemNotifier[LW2080_NOTIFIERS_GC5_GPU_READY].info16,0);
}

//--------------------------------------------------------------------------------------------------
// Verify ASPM states for both up/dn stream devices are L1
RC GCxImpl::VerifyASPM()
{
    auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
    PexDevice *pPexDev = NULL;
    UINT32 port;
    UINT32 state = 0;
    UINT32 gpuState = 0;
    bool bSupported = true;

    RC rc;
    // Verify the GPU's CYA bits are properly set
    gpuState = pGpuPcie->GetAspmCyaState();
    if (!(gpuState & Pci::ASPM_L1))
    {
        Printf(m_VerbosePri, "VerifyASPM: AspmCyaState:%d is not correct\n",gpuState);
        bSupported = false;
    }

    pGpuPcie->GetUpStreamInfo(&pPexDev, &port);
    MASSERT(pPexDev);
    while (pPexDev)
    {
        // Verify the L0/L1 state on both ends of the link
        CHECK_RC(pPexDev->GetDownStreamAspmState(&state, port));
        if (!(state & Pci::ASPM_L1))
        {
            Printf(m_VerbosePri,
                "VerifyASPM: DownstreamAspmState:%d on port:%d at depth:%d is not correct\n",
                state, port, pPexDev->GetDepth());
            bSupported = false;
        }

        if (pPexDev->IsRoot())
            break;

        CHECK_RC(pPexDev->GetUpStreamAspmState(&state));
        if (!(state & Pci::ASPM_L1))
        {
            Printf(m_VerbosePri, "VerifyASPM: UpstreamAspmState:%d at depth:%d is not correct\n",
                state,pPexDev->GetDepth());
            bSupported = false;
        }
        // Get the next upstream PexDev
        port = pPexDev->GetUpStreamIndex();
        pPexDev = pPexDev->GetUpStreamDev();
    }
    return (bSupported) ? OK : RC::HW_STATUS_ERROR;
}

//--------------------------------------------------------------------------------------------------
// Return true if GC5 is supported for the given pstate. If pstate == ILWALID_PSTATE then return
// true if GC5 is supported for any pstate, false otherwise
// Note: -power_feature2 0x30000 must be passed on the command line to support DI-SSC and it must
// not be there to support DI-OS.
// Some history here on why the code looks the way it does:
// DI-OS/DI-NH was present on earlier chips and the lw2080CtrlCmdGpuQueryDeepIdleSupport API was
// created to support that. Then later we introduced DI-SSC which required the new GC6+ islands and
// required a new set of APIs to determine if the islands are present and loaded. Then we made DI-OS
// use the islands as well but the original APIs remained. So if we are checking for DI-SSC we have
// one additional control call to make to determine if its supported.
// Although GC5 technically doesn't power down the GPU via these new islands we still utilize the
// SCI wakeup timer to generate a wakeup event, and the BSI to detect pex activity.
bool GCxImpl::IsGc5Supported(UINT32 pstate)
{
    RC rc;
    // Query RM for power features supported
    if (m_Gc5Mode == gc5DI_SSC)
    {
        LW2080_CTRL_POWER_FEATURES_SUPPORTED_PARAMS PwrFeaturesMask = {0};
        if (OK != m_pLwRm->ControlBySubdevice(m_pSubdev,
                                           LW2080_CTRL_CMD_POWER_FEATURES_SUPPORTED,
                                           &PwrFeaturesMask,
                                           sizeof(PwrFeaturesMask)))
        {
            Printf(m_VerbosePri, "GCxImpl::Failed to query RM Power Features !\n");
            return false;
        }

        //Check if RM supports GC5 power feature
        if (!FLD_TEST_DRF(2080_CTRL,
                          _POWER_FEATURES_SUPPORTED,
                          _GC5,
                          _TRUE,
                          PwrFeaturesMask.powerFeaturesSupported))
        {
            // GC5 power feature disabled in RM
            Printf(m_VerbosePri,"GCxImpl::RM power features don't support GC5 !\n");
            return false;
        }
    }

    // Verify the GC6PLUS island is present before checking to see if it is loaded
    LW2080_CTRL_POWER_FEATURE_GC6_INFO_PARAMS gc6InfoParam = {0};
    rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_POWER_FEATURE_GET_GC6_INFO,
           &gc6InfoParam,
           sizeof(gc6InfoParam));
    if ((OK != rc) || (FLD_TEST_DRF(2080_CTRL_POWER_FEATURE,
                        _GC6_INFO,
                        _PGISLAND_PRESENT,
                        _FALSE,
                        gc6InfoParam.gc6InfoMask)))
    {
        Printf(m_VerbosePri,"GCxImpl::GC6+ SCI/BSI islands are not present!\n");
        return false;
    }
    // Verify the GC6PLUS island is loaded in the BSI ram
    //
    LW2080_CTRL_GC6PLUS_IS_ISLAND_LOADED_PARAMS islandParam = {0};
    rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_GC6PLUS_IS_ISLAND_LOADED,
           &islandParam,
           sizeof(islandParam));
    if (OK != rc || !islandParam.bIsIslandLoaded)
    {
        Printf(m_VerbosePri,"GCxImpl::GC6+ SCI/BSI islands are not loaded !\n");
        return false;
    }

    // POR for GC5 is to operate in PStates 5 & 8, but not 0.
    memset(&m_Gc5DeepIdle, 0, sizeof(m_Gc5DeepIdle));
    rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
        LW2080_CTRL_CMD_GPU_QUERY_DEEP_IDLE_SUPPORT,
        &m_Gc5DeepIdle, sizeof(m_Gc5DeepIdle));

    if (OK == rc &&  (FLD_TEST_DRF(2080_CTRL_GPU,
                        _DEEP_IDLE,
                        _FLAGS_SUPPORT,
                        _YES,
                        m_Gc5DeepIdle.flags)))
    {
        for (int i=0; i<m_Gc5DeepIdle.numStates; i++)
        {   //we know we have atleast 1 pstate that is supported.
            if (m_Gc5DeepIdle.pstates[i] == pstate || pstate == Perf::ILWALID_PSTATE)
            {
                return(true);
            }
        }
    }

    Printf(m_VerbosePri,"GCxImpl::No supported pstates for GC5 !\n");
    return false;
}

//--------------------------------------------------------------------------------------------------
// Return true if the requested pstate is supported by GC5
bool GCxImpl::IsValidGc5Pstate(UINT32 pstate)
{
    for (int i=0; i<m_Gc5DeepIdle.numStates; i++)
    {
        if (m_Gc5DeepIdle.pstates[i] == pstate)
        {
            return(true);
        }
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
// Verify the requested wakeup event is valid for the current GC5 mode.
RC GCxImpl::ValidateWakeupEvent(UINT32 wakeupEvent)
{
    RC rc;
    switch (wakeupEvent)
    {
        case GC_CMN_WAKEUP_EVENT_gc5pex:
        case GC_CMN_WAKEUP_EVENT_gc5i2c:
            break;

        case GC_CMN_WAKEUP_EVENT_gc5rm:
        case GC_CMN_WAKEUP_EVENT_gc5timer:
        case GC_CMN_WAKEUP_EVENT_gc5alarm:
            if (m_Gc5Mode == gc5DI_OS)
                rc = RC::BAD_PARAMETER;
            break;

        default:
            rc = RC::BAD_PARAMETER;
    }
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Determine how to enter GC5 based on the desired wakeup event.
//
RC GCxImpl::DoEnterGc5(UINT32 pstate, UINT32 wakeupEvent, UINT32 enterDelayUsec)
{
    RC rc;
    bool bUseHwTimer = false;
    CHECK_RC(ValidateWakeupEvent(wakeupEvent));
    CHECK_RC(InitGc5());
    if (!IsValidGc5Pstate(pstate))
        return RC::BAD_PARAMETER;

    if (GetLwrPstate() != pstate)
    {
        Printf(m_VerbosePri,"Forcing PState:%d\n",pstate);
        CHECK_RC(m_pSubdev->GetPerf()->ForcePState(pstate,
                                                   LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }

    // Pause the thread which is collecting PCIE errors
    m_pPausePexErrThread.reset(new PexDevMgr::PauseErrorCollectorThread(
                (PexDevMgr*)DevMgrMgr::d_PexDevMgr));
    bool entryOK = false;
    DEFER
    {
        if (!entryOK)
        {
            m_pPausePexErrThread.reset(nullptr);
        }
    };

    m_WakeupEvent = wakeupEvent;
    m_EnterDelayUsec = enterDelayUsec;
    if (m_Gc5Mode == gc5DI_SSC)
    {
        if (!m_IsGc5Active)
        {
            ActivateGc5(true);
        }
        // Unconditionally cancel any pending timers to prevent incorrect wakeup and account for
        // case where the previous loop was a gc5timer/gc5alarm that aborted and left the timer
        // running.
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,LW2080_CTRL_CMD_TIMER_CANCEL,nullptr, 0));

        UINT64 freq = Xp::QueryPerformanceFrequency();
        UINT64 start = Xp::QueryPerformanceCounter();
        bUseHwTimer = (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5timer ||
                       m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm);
        bool bUseAutoHwTimer = (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm);
        if (bUseAutoHwTimer)
        {
            // program the alarm timer.
            LW2080_CTRL_CMD_TIMER_SCHEDULE_PARAMS params = {0};
            params.time_nsec = (LwU64)enterDelayUsec * 1000;
            params.flags = DRF_DEF(2080_CTRL,_TIMER_SCHEDULE,_FLAGS_TIME,_REL);
            CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                               LW2080_CTRL_CMD_TIMER_SCHEDULE,
                               &params,sizeof(params)));
        }
        // clear out current stats just before entering GC5
        CHECK_RC(GetDeepIdleStats(true,&m_Gc5DeepIdleStats));
        CHECK_RC(EnterGc5(bUseHwTimer,enterDelayUsec,bUseAutoHwTimer));

        m_EnterTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;
    }
    else
    {   // clear out the stats just before waiting for DI_OS to engage
        CHECK_RC(GetDeepIdleStats(true,&m_Gc5DeepIdleStats));
    }

    if (!bUseHwTimer)
    {
        Printf(m_VerbosePri, "Waiting %d usec before generating wakeup event.\n",enterDelayUsec);
        Platform::Delay(enterDelayUsec);
    }
    else
    {
        Printf(m_VerbosePri, "Waiting %d usec for timer/alarm event to occur.\n",enterDelayUsec);
    }

    entryOK = true;

    return rc;
}

//------------------------------------------------------------------------------
// I2C operations are not very reliable. We have seen multiple cases where a single
// I2C access would timeout but subsequent access's work fine. If the I2C is
// non-functional and we will fail all retries then return an error, otherwise
// return OK.
RC GCxImpl::ReadThermSensor(UINT08 *pValue, UINT32 retries)
{
    RC rc;
    UINT32 retry = 0;

    do{
        rc.Clear();
        rc = m_pSmbPort->RdByteCmd(m_ThermAddr,0,pValue);

        if(OK != rc)
        {
            Printf(m_VerbosePri,
                "Failed to read therm sensor from I2C addr:0x%x  rc=%d, retry=%d\n",
                m_ThermAddr, rc.Get(),retry);
            retry++;
            Tasker::Sleep(1);
        }
        else
        {
            Printf(m_VerbosePri,"Therm sensor at I2C addr:0x%x value:0x%x\n",m_ThermAddr,*pValue);
        }

    }while((OK != rc) && (retry < retries));

    return rc;
}
//------------------------------------------------------------------------------
// Exit GC5 based on the desired wakeup event.
RC GCxImpl::DoExitGc5(UINT32 exitDelayUsec, bool bRestorePstate)
{
    StickyRC rc;
    UINT64 start = 0;
    UINT64 freq = Xp::QueryPerformanceFrequency();
    m_ExitDelayUsec = exitDelayUsec;
    start = Xp::QueryPerformanceCounter();
    switch (m_WakeupEvent)
    {
        case GC_CMN_WAKEUP_EVENT_gc5rm:
            CHECK_RC(ExitGc5());
            break;

        // force a wakeup via register read
        case GC_CMN_WAKEUP_EVENT_gc5pex:
        {
            UINT32 value = 0x1234;
            value = m_pSubdev->RegRd32(0);
            Printf(m_VerbosePri, "GC5_Exit:Pex access via register 0 = 0x%x\n",value);
            break;
        }

        // force a wakeup via I2C
        case GC_CMN_WAKEUP_EVENT_gc5i2c:
        {
            UINT08 value = 0;
            CHECK_RC(ReadThermSensor(&value,10));
            break;
        }

        // just wait for the timer to expire
        case GC_CMN_WAKEUP_EVENT_gc5timer:
            break;

        // Force a wakeup using the timer alarm. Note we get two events in this
        // case. A GPU_READY event & TIMER_EVENT, however we are only checking
        // for the GPU_READY event.
        case GC_CMN_WAKEUP_EVENT_gc5alarm:
            break;

        default:
            MASSERT(!"Unknown wakeup event for GC5 exit!");
            return RC::SOFTWARE_ERROR;
    }

    UINT32 timeoutMs = m_WakeupGracePeriodMs; // add fudge factor.
    if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5timer ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm)
    {
        timeoutMs += (m_EnterDelayUsec/1000);
    }

    if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm)
    {
        CHECK_RC(POLLWRAP_HW(PollPtimerAlarmFired, this, timeoutMs));

        // get ready for next loop.
        m_PtimerAlarmFired = false;
    }
    // DI_OS doesn't generate any notifications or events, only DI_SSC does
    if (m_Gc5Mode == gc5DI_SSC)
    {
        rc = Tasker::WaitOnEvent(m_pEventGpuReady,timeoutMs);
        if (OK == rc.Get())
        {
            // read the reason for the event
            GpuReadyMemNotifyHandler();
            Tasker::ResetEvent(m_pEventGpuReady);
        }
        else  //(rc.Get() == RC::TIMEOUT_ERROR)
        { // Gpu probably didn't get the desired wakeup event.
            return rc;
        }

        if (m_WakeupReason != LW2080_GC5_EXIT_COMPLETE)
        {
            Printf(m_VerbosePri,"Unexpected wakeup reason= %d, expected:%d\n",
                    m_WakeupReason, LW2080_GC5_EXIT_COMPLETE);
        }

        m_ExitTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;

        // Get the wakeup reason from RM
        memset(&m_WakeupParams,0,sizeof(m_WakeupParams));
        m_WakeupParams.selectPowerState = LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5MINUS_SSC;
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                    (UINT32)LW2080_CTRL_CMD_GCX_GET_WAKEUP_REASON,
                    &m_WakeupParams,
                    sizeof (m_WakeupParams)));
    }

    CHECK_RC(GetDeepIdleStats(false,&m_Gc5DeepIdleStats));

    m_pPausePexErrThread.reset(nullptr);

    if (bRestorePstate == true)
    {
        CHECK_RC(m_pSubdev->GetPerf()->SetPerfPoint(m_LwrPerfPt));
    }
    // now just wait for a bit before going to next loop.
    Platform::Delay(m_ExitDelayUsec);

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Wrapper around the GpuSubdevice call to enter the GC5 low power state
//
RC GCxImpl::EnterGc5
(
    bool bUseHwTimer,       //
    UINT32 timeToWakeUs,    //
    bool bAutoSetupHwTimer  //
)
{
    RC rc;
    LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS  pwrParams;
    memset(&pwrParams, 0, sizeof(pwrParams));
    pwrParams.action             = LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ENTER;
    pwrParams.params.bUseHwTimer = bUseHwTimer ? LW_TRUE : LW_FALSE;
    pwrParams.params.timeToWakeUs      = timeToWakeUs;
    pwrParams.params.bAutoSetupHwTimer = bAutoSetupHwTimer ? LW_TRUE : LW_FALSE;

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
            (UINT32)LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
            &pwrParams,sizeof (pwrParams)));

    return rc;
}

//--------------------------------------------------------------------------------------------------
void GCxImpl::GetWakeupParams(LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS *pParms)
{
    memcpy(pParms,&m_WakeupParams,sizeof(m_WakeupParams));
}

//------------------------------------------------------------------------------
// Wrapper around the RM control call to exit GC5 state.
//
RC GCxImpl::ExitGc5()
{
    RC rc;
    LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS  pwrParams;
    memset(&pwrParams, 0, sizeof(pwrParams));
    pwrParams.action = LW2080_CTRL_GPU_POWER_ON_OFF_GC5_EXIT;

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
            (UINT32)LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
            &pwrParams,sizeof (pwrParams)));

    return rc;
}

bool GCxImpl::IsRTD3Supported(UINT32 pstate, bool bNativeACPI)
{
    UINT32 powerFeature = DRF_DEF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _GC6_RTD3, _TRUE);
    return IsGc6SupportedCommon(pstate, powerFeature, bNativeACPI);
}

bool GCxImpl::IsRTD3GCOffSupported(UINT32 pstate)
{
    UINT32 powerFeature = DRF_DEF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _GCOFF, _TRUE);
    return IsGc6SupportedCommon(pstate, powerFeature, true);
}

bool GCxImpl::IsGc6Supported(UINT32 pstate)
{
    UINT32 powerFeature = DRF_DEF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _GC6, _TRUE);
    return IsGc6SupportedCommon(pstate, powerFeature, false);
}

bool GCxImpl::IsFGc6Supported(UINT32 pstate)
{
    UINT32 powerFeature = DRF_DEF(2080_CTRL, _POWER_FEATURES_SUPPORTED, _FGC6, _TRUE);
    return IsGc6SupportedCommon(pstate, powerFeature, false);
}

//-------------------------------------------------------------------------------------------------
bool GCxImpl::IsGc6SupportedCommon(UINT32 pstate, UINT32 powerFeature, bool bNativeACPI)
{
    // Query RM for power features supported
    LW2080_CTRL_POWER_FEATURES_SUPPORTED_PARAMS pwrFeaturesMask = {0};
    if (OK != m_pLwRm->ControlBySubdevice(m_pSubdev,
                   LW2080_CTRL_CMD_POWER_FEATURES_SUPPORTED,
                   &pwrFeaturesMask,
                   sizeof(pwrFeaturesMask)))
    {
        Printf(m_VerbosePri, "GcxImpl - Failed to query RM Power Features !\n");
        return false;
    }

    if (!(powerFeature & pwrFeaturesMask.powerFeaturesSupported))
    {
        Printf(m_VerbosePri, "GcxImpl - Power feature disabled in RM!\n");
        return false;
    }

    if (GetGc6Pstate() != pstate)
    {
        Printf(m_VerbosePri, "GcxImpl - pstate %d is not supported!\n", pstate);
        return false;
    }

    Xp::JTMgr * pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    if (pJT == 0)
    {
        Printf(m_VerbosePri, "GcxImpl - JTMgr is not implemented!\n");
        return false;
    }

    UINT32 caps = pJT->GetCapabilities();
    if (!((caps & LW_JT_FUNC_CAPS_JT_ENABLED_TRUE) ||
        (bNativeACPI && pJT->IsNativeACPISupported())))
    {
        Printf(m_VerbosePri, "GcxImpl - JTCaps is not enabled!\n");
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void GCxImpl::SetDebugOptions(Xp::JTMgr::DebugOpts dbgOpts)
{
    if (!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    }
    MASSERT(m_pJT);
    m_pJT->SetDebugOptions(&dbgOpts);
}

//-------------------------------------------------------------------------------------------------
RC GCxImpl::GetDebugOptions(Xp::JTMgr::DebugOpts * pDbgOpts)
{
    if (!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
        MASSERT(m_pJT);
    }

    return m_pJT->GetDebugOptions(pDbgOpts);
}

//-------------------------------------------------------------------------------------------------
UINT32 GCxImpl::GetGc6Caps()
{
    if(!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    }
    MASSERT(m_pJT);
    return m_pJT->GetCapabilities();
}

//--------------------------------------------------------------------------------------------------
UINT32 GCxImpl::GetGc6FsmMode()
{
    if(!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    }
    MASSERT(m_pJT);
    m_pJT->GetLwrrentFsmMode((UINT08*)&m_EcFsmMode);
    return m_EcFsmMode;
}

//--------------------------------------------------------------------------------------------------
UINT32 GCxImpl::GetGc6Pstate()
{
    Perf *pPerf = m_pSubdev->GetPerf();
    UINT32 pstate = Perf::ILWALID_PSTATE;

    // need to query for the lowest PState instead of getting the deep idle pstate
    vector<UINT32> pstates;
    RC rc = pPerf->GetAvailablePStates(&pstates);
    if (OK == rc)
    {
        pstate=0;
        for (unsigned i=0; i<pstates.size(); i++)
        {
            pstate = max(pstate,pstates[i]);
        }
    }
    return pstate;
}

//--------------------------------------------------------------------------------------------------
// Initialize the implementor to perform GC6 cycles. This will also claim the pstates and force
// the pstate to the lowest possible performance.
RC GCxImpl::InitGc6(FLOAT64 timeoutMs)
{
    RC rc;
    m_CfgRdyTimeoutMs = timeoutMs;
    if (!m_pLwRm)
    {
        LwRmPtr RmPtr;
        m_pLwRm = RmPtr.Get();
    }

    // GC6 cycles don't restore the registers associated with floorsweeping.
    // Due to the power up sequence MODS can't perform this task at the correct
    // time. So make this test incompatible with floor sweeping. see B1173195
    FloorsweepImpl *pFs = m_pSubdev->GetFsImpl();

    if (pFs && pFs->GetFloorsweepingAffected() && m_pSubdev->HasBug(1173195))
    {
        Printf(Tee::PriHigh, "GC6 is not compatible with floorsweeping."\
            "Either remove the arguments or skip this test\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_pSubdev->GetFuse()->IsSwFusingAllowed() && !m_SkipSwFuseCheck)
    {
        Printf(Tee::PriHigh, "GC6 Test is not compatible with SW fusing.");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    MASSERT(m_pJT);
    m_pJT->GetLwrrentFsmMode(&m_OrigFsmMode);

    m_Gc6InitDone = true;

    if (!m_bSmbInitDone)
        InitializeSmbusDevice();

    m_Gc6Pstate = GetGc6Pstate();

    auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
    m_cfgSpaceList.clear();

    m_cfgSpaceList.push_back(m_pSubdev->AllocPciCfgSpace());
    m_cfgSpaceList.back()->Initialize(pGpuPcie->DomainNumber(),
                                  pGpuPcie->BusNumber(),
                                  pGpuPcie->DeviceNumber(),
                                  pGpuPcie->FunctionNumber());
    m_pPciCfgSpaceGpu = m_cfgSpaceList.back().get();

#ifdef INCLUDE_AZALIA
    AzaliaController* const pAzalia = m_pSubdev->GetAzaliaController();
    if (pAzalia)
    {

        SysUtil::PciInfo azaPciInfo = {0};
        CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

        m_cfgSpaceList.push_back(m_pSubdev->AllocPciCfgSpace());
        CHECK_RC(m_cfgSpaceList.back()->Initialize(azaPciInfo.DomainNumber,
                                               azaPciInfo.BusNumber,
                                               azaPciInfo.DeviceNumber,
                                               azaPciInfo.FunctionNumber));
    }
#endif

    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
        m_pSubdev->SupportsInterface<XusbHostCtrl>())
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(m_pSubdev->GetInterface<XusbHostCtrl>()->GetPciDBDF(&domain,
                                                                     &bus,
                                                                     &device,
                                                                     &function));

        m_cfgSpaceList.push_back(m_pSubdev->AllocPciCfgSpace());
        CHECK_RC(m_cfgSpaceList.back()->Initialize(domain, bus, device, function));
    }

    if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
        m_pSubdev->SupportsInterface<PortPolicyCtrl>())
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(m_pSubdev->GetInterface<PortPolicyCtrl>()->GetPciDBDF(&domain,
                                                                       &bus,
                                                                       &device,
                                                                       &function));

        m_cfgSpaceList.push_back(m_pSubdev->AllocPciCfgSpace());
        CHECK_RC(m_cfgSpaceList.back()->Initialize(domain, bus, device, function));
    }

    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(pGpuPcie->GetUpStreamInfo(&pPexDev, &port));
    MASSERT(pPexDev);
    if (Utility::ToUpperCase(m_PlatformType) == "TGL")
    {
        pPexDev->InitializeRootPortPowerOffsets(PexDevice::PlatformType::TGL);
    }
    else
    {
        pPexDev->InitializeRootPortPowerOffsets(PexDevice::PlatformType::CFL);
    }

    CHECK_RC(GetRtd3AuxRailSwitchSupport());

    return rc;
}

RC GCxImpl::GetRtd3AuxRailSwitchSupport()
{
    RC rc;
    LW2080_CTRL_CMD_GET_RTD3_INFO_PARAMS params = {};

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_GET_RTD3_INFO,
           &params,
           sizeof(params)));

    m_Rtd3AuxRailSwitchSupported = params.bIsRTD3VoltageSourceSwitchSupported;

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Save the Pci Config space to local memory for restoration later
RC GCxImpl::SaveCfgSpace()
{
    RC rc;
    #ifdef INCLUDE_AZALIA
    // Uninitialize Azalia device
    AzaliaController* const pAzalia = m_pSubdev->GetAzaliaController();
    if (pAzalia)
    {
        if (OK != (rc = pAzalia->Uninit()))
        {
            Printf(Tee::PriNormal, "pAzalia->Uninit rc=%d.\n",rc.Get());
            return rc;
        }
    }
    #endif

    // Save PCI config space of the GPU and other functions
    for (auto cfgSpaceIter = m_cfgSpaceList.begin();
         cfgSpaceIter != m_cfgSpaceList.end();
         ++cfgSpaceIter)
    {
        (*cfgSpaceIter)->Clear();
        CHECK_RC((*cfgSpaceIter)->Save());
    }

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Wait for the PCI config space to appear back on the PCI bus, then restore the it with a local
// copy of the Pci config spaces(including Azalia ).
RC GCxImpl::RestoreCfgSpace()
{
    RC rc;
    // Wait for PCI cfg space to become operational
    CHECK_RC(CheckCfgSpaceReady());

    for (auto cfgSpaceIter = m_cfgSpaceList.begin();
         cfgSpaceIter != m_cfgSpaceList.end();
        ++cfgSpaceIter)
    {
        PciCfgSpace* pCfgSpace = cfgSpaceIter->get();
        if (OK != (rc = pCfgSpace->Restore()))
        {
            Printf(Tee::PriError, "PciCfgSpace restore failed for %04x:%02x:%02x.%x, rc=%d.\n",
                        pCfgSpace->PciDomainNumber(),
                        pCfgSpace->PciBusNumber(),
                        pCfgSpace->PciDeviceNumber(),
                        pCfgSpace->PciFunctionNumber(),
                        rc.Get());
            return rc;
        }
    }

    #ifdef INCLUDE_AZALIA
    AzaliaController* const pAzalia = m_pSubdev->GetAzaliaController();

    // Resume Azalia
    if (pAzalia)
    {
        if (OK != (rc = pAzalia->Init()))
        {
            Printf(Tee::PriNormal, "pAzalia->Init: rc=%d\n",rc.Get());
            return rc;
        }
    }
    #endif
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Wait for the Pci Config space to dissappear from the bus.
RC GCxImpl::CheckCfgSpaceNotReady()
{
    return m_pPciCfgSpaceGpu->CheckCfgSpaceNotReady(m_CfgRdyTimeoutMs);
}

//--------------------------------------------------------------------------------------------------
// Wait for the Pci Config space to reappear on the bus.
RC GCxImpl::CheckCfgSpaceReady()
{
    return m_pPciCfgSpaceGpu->CheckCfgSpaceReady(m_CfgRdyTimeoutMs);
}

//--------------------------------------------------------------------------------------------------
RC GCxImpl::DoEnterGc6(UINT32 wakeupEvent, UINT32 enterDelayUsec)
{
    StickyRC rc;
    m_WakeupEvent = wakeupEvent;

    if(m_IsGc5Active)
    {
        ActivateGc5(false);
    }
    if (GetLwrPstate() != m_Gc6Pstate)
    {
        Printf(m_VerbosePri,"Forcing PState:%d\n",m_Gc6Pstate);
        CHECK_RC(m_pSubdev->GetPerf()->ForcePState(m_Gc6Pstate,LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }

    CHECK_RC(SaveCfgSpace());
    // now enter GC6
    UINT64 start = 0;
    UINT64 freq = Xp::QueryPerformanceFrequency();

    // Pause the thread which is collecting PCIE errors
    m_pPausePexErrThread.reset(new PexDevMgr::PauseErrorCollectorThread(
                (PexDevMgr*)DevMgrMgr::d_PexDevMgr));
    bool entryOK = false;
    DEFER
    {
        if (!entryOK)
        {
            m_pPausePexErrThread.reset(nullptr);
        }
    };

    Tasker::AcquireMutex(GpuPtr()->GetOneHzMutex());

    bool b1HzQuickRelease = true;
    DEFER
    {
        if (b1HzQuickRelease)
        {
            Tasker::ReleaseMutex(GpuPtr()->GetOneHzMutex());
        }
    };

    if (m_UseOptimus)
    {
        auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
        const UINT32 domain   = pGpuPcie->DomainNumber();
        const UINT32 bus      = pGpuPcie->BusNumber();
        const UINT32 device   = pGpuPcie->DeviceNumber();
        const UINT32 function = pGpuPcie->FunctionNumber();
        unique_ptr<Xp::OptimusMgr> pOptimus(Xp::GetOptimusMgr(domain, bus, device, function));
        if (pOptimus.get() != 0)
        {
            Printf(Tee::PriDebug, "Performing GC6 cycle using Optimus ACPI\n");
            BgLogger::PauseBgLogger DisableBgLogger; // To prevent accessing GPU by
                                                  // other threads during power off
            m_pSubdev->InGc6(true); // To prevent poll_interrutps accessing GPU
            Utility::CleanupOnReturnArg<GpuSubdevice,void,bool>
                disableInGc6(m_pSubdev, &GpuSubdevice::InGc6, false);

            // Enable dynamic power control, if available
            if (pOptimus->IsDynamicPowerControlSupported())
            {
                pOptimus->EnableDynamicPowerControl();
            }
            // Power off the GPU
            start = Xp::QueryPerformanceCounter();
            CHECK_RC(pOptimus->PowerOff());
            m_EnterTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;

            // If dynamic power control is available, wait for the
            // GPU to disappear from the PCI bus
            if (pOptimus->IsDynamicPowerControlSupported())
            {
                CHECK_RC(CheckCfgSpaceNotReady());
            }
            // we only want to disable InGc6 on any failure.
            disableInGc6.Cancel();
        }
    }
    else if (m_UseJTMgr)
    {
        m_PTimerEntry = GetPTimer();
        GC6EntryParams entryParams = {};
        CHECK_RC(FillGC6EntryParams(entryParams, enterDelayUsec));
        CHECK_RC(EnterGc6(entryParams, enterDelayUsec));
    }

    entryOK = true;
    b1HzQuickRelease = false;

    return rc;
}

//--------------------------------------------------------------------------------------------------
RC GCxImpl::DoExitGc6(UINT32 exitDelayUsec, bool bRestorePstate)
{
    StickyRC rc;
    UINT64 start = 0;
    UINT64 freq = Xp::QueryPerformanceFrequency();
    bool b1HzQuickRelease = true;

    DEFER
    {
        if (b1HzQuickRelease)
        {
            Tasker::ReleaseMutex(GpuPtr()->GetOneHzMutex());
        }
    };
    // exit GC6
    if (m_UseOptimus)
    {
        auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
        const UINT32 domain   = pGpuPcie->DomainNumber();
        const UINT32 bus      = pGpuPcie->BusNumber();
        const UINT32 device   = pGpuPcie->DeviceNumber();
        const UINT32 function = pGpuPcie->FunctionNumber();
        unique_ptr<Xp::OptimusMgr> pOptimus(Xp::GetOptimusMgr(domain, bus, device, function));
        if (pOptimus.get() != 0)
        {
            Utility::CleanupOnReturnArg<GpuSubdevice,void,bool>
                disableInGc6(m_pSubdev, &GpuSubdevice::InGc6, false);
            // Power on the GPU
            start = Xp::QueryPerformanceCounter();
            CHECK_RC(pOptimus->PowerOn());
            m_ExitTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;
        }
    }
    else if (m_UseJTMgr)
    {
        GC6ExitParams exitParams = {};
        CHECK_RC(FillGC6ExitParams(exitParams));

        rc = ExitGc6(exitParams, exitDelayUsec);
        // reset the actions and sequence
        m_UseRTD3 = false;
        m_UseFastGC6 = false;

        if (rc == RC::LWRM_TIMEOUT || rc == RC::LWRM_MORE_PROCESSING_REQUIRED)
        { // Gpu is not on the bus or possibly hung, so exit the hardway
            Utility::ExitMods(rc, Utility::ExitQuickAndDirty);
        }
        m_PTimerExit = GetPTimer();
        if (m_PTimerEntry > m_PTimerExit &&
            (FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _RTD3, m_EcFsmMode) ||
             FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _GC6M, m_EcFsmMode)))
        {
            Printf(Tee::PriWarn, "The PTIMER value has not been restored!\n"
                "This could cause asserts in subsequent OpenGL tests.\n");
        }
        if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotplugirq)
        {
            rc = m_pJT->AssertHpd(false);
        }
    }
    CHECK_RC(RestoreCfgSpace());

    Tasker::ReleaseMutex(GpuPtr()->GetOneHzMutex());
    b1HzQuickRelease = false;

    m_pPausePexErrThread.reset(nullptr);

    // Get the wakeup parameters from RM
    memset(&m_WakeupParams,0,sizeof(m_WakeupParams));
    if (FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _GC6M, m_EcFsmMode) ||
        FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _RTD3, m_EcFsmMode))
    {
        m_WakeupParams.selectPowerState = LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC6;
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                    (UINT32)LW2080_CTRL_CMD_GCX_GET_WAKEUP_REASON,
                    &m_WakeupParams,sizeof (m_WakeupParams)));
    }

    if (m_EnableLoadTimeCalc) 
    {
        // update engine load time
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                    LW2080_CTRL_CMD_GPU_GET_ENGINE_LOAD_TIMES,
                    &m_EngineLoadTimeParams,
                    sizeof(m_EngineLoadTimeParams)));
        
        // update engine ID <-> name mapping
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                    LW2080_CTRL_CMD_GPU_GET_ID_NAME_MAPPING,
                    &m_EngineIdNameMappingParams,
                    sizeof(m_EngineIdNameMappingParams)));
    }


    if (bRestorePstate == true)
    {
        CHECK_RC(m_pSubdev->GetPerf()->SetPerfPoint(m_LwrPerfPt));
    }

    // There are cases when GC6 exit in RM doesn't perform state load,
    // for e.g. on LWSR systems RM limit deferred perf state load
    // This control call is required to complete any pending task that
    // needs to be run after GC6 exit.
    if (m_Gc6ExitRequirePostProcessing)
    {
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
                    LW2080_CTRL_CMD_GPU_PROCESS_POST_GC6_EXIT_TASKS,
                    NULL, 0));
    }

    return rc;
}

RC GCxImpl::FillGC6EntryParams(GC6EntryParams &entry, UINT32 enterDelayUsec)
{
    entry.stepMask = BIT(LW2080_CTRL_GC6_STEP_ID_GPU_OFF);

    if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6timer ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer)
    {
        entry.params.bUseHwTimer = true;
        entry.params.timeToWakeUs = enterDelayUsec;
    }

    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_rtd3all)
    {
        m_UseRTD3 = true;
        entry.params.bIsRTD3CoreRailPowerLwt = m_Rtd3AuxRailSwitchSupported;
    }

    entry.params.bIsRTD3Transition = m_UseRTD3;
    if (m_pJT->IsJTACPISupported() && m_UseLWSR)
    {
        entry.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_LWSR;
        entry.stepMask |= BIT(LW2080_CTRL_GC6_STEP_ID_SR_ENTRY);
    }
    else if (!m_pJT->IsJTACPISupported() && m_UseRTD3)
    {
        entry.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
    }
    else
    {
        entry.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
    }

    return OK;
}

RC GCxImpl::FillGC6ExitParams(GC6ExitParams &exit)
{
    exit.params.bIsRTD3Transition = m_UseRTD3;
    if (m_pJT->IsJTACPISupported() && m_UseLWSR)
    {
        exit.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_LWSR;
    }
    else if (!m_pJT->IsJTACPISupported() && m_UseRTD3)
    {
        exit.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
    }
    else
    {
        exit.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
    }
    // UseGpuSelfWake action will tell RM to do EAGP on below events
    // Other events will perform EBPG sequence
    if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotplug ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotplugirq ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotunplug ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6i2c ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6timer ||
        m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer)
    {
        exit.params.bIsGpuSelfWake = true;
    }
    return OK;
}

//--------------------------------------------------------------------------------------------------
// Wrapper around the RM call to enter the GC6 deep idle state
// Note: Here is the call tree.
//   ------------------------ MODS ----------------------
// GpuGc6Test::EnterGc6()
// ->GpuSubdevice::EnterGc6()
//  ->LwRm::ControlBySubdevice(LW2080_CTRL_CMD_GC6_ENTRY)
//    -> LwRm::Control()
//     ------------------------RM Library ------------------
//       ->LwRmControl
//         -> Lw04Control()
//            -> RmControl()
//               -> _rmControl()
//                  -> lw2080CtrlCmdGc6Entry() via pRmCtrlParams->pRmControlEntry->pRmControl(pRmCtrlParams)
//                    -> _gc6EntryGpuPowerOff()
//                      -> gcxGc6GpuPowerOff_IMPL()
//                        -> gpuPowerOff_IMPL()
//      ------------------------ MODS ----------------------
//                           -> osGC6PowerControl()
//                              -> osCallACPI_DSM()
//                                -> ModsDrvCallACPIGeneric()
//                                  -> Platform::CallACPIGeneric()
//                                    -> Xp::CallACPIGeneric()
//                                      -> CallACPI_DSM_Eval()
//                                        -> LinuxJTMgr::DSM_Eval()
//                                          -> LinuxJTMgr::SMBusPowerControl() (if SMBus is suppored)
//                                            -> SmbEC::EnterGC6Power()
//                                          or
//                                          -> CallACPI_DSM_Eval() (if ACPI is supported)
//      ------------------------ Linux Kernel ----------------------
//                                            -> ioctl(s_KrnFd,...) (Linux Kernel Driver)
//
RC GCxImpl::EnterGc6(GC6EntryParams &entryParams, UINT32 enterDelayUsec)
{
    RC rc;

    UINT64 freq  = Xp::QueryPerformanceFrequency();
    UINT64 start = Xp::QueryPerformanceCounter();
    if (m_pJT->IsJTACPISupported())
    {
        Printf(Tee::PriLow, "Entering GC6 using JT ACPI\n");
    }
    else if (m_pJT->IsSMBusSupported())
    {
        Printf(Tee::PriLow, "Entering GC6 using SMBUS\n");
        if (m_UseRTD3)
        {
            CHECK_RC(m_pJT->SetLwrrentFsmMode(LW_EC_INFO_FW_CFG_FSM_MODE_RTD3));
        }
        else
        {
            CHECK_RC(m_pJT->SetLwrrentFsmMode(LW_EC_INFO_FW_CFG_FSM_MODE_GC6M));
        }
    }
    else if (m_pJT->IsNativeACPISupported())
    {
        Printf(Tee::PriLow, "Entering GC6 using native ACPI\n");
    }

    m_pJT->SetWakeupEvent(m_WakeupEvent);

    CHECK_RC(m_pSubdev->EnterGc6(entryParams));

    if (m_UseRTD3 && entryParams.flavorId == LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID && !m_UseD3Hot)
    {
        CHECK_RC(m_pJT->RTD3PowerCycle(Xp::PowerState::PowerOff));
    }

    m_EnterTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;

    Printf(m_VerbosePri,"Waiting %d usec before calling GC6Exit()\n",enterDelayUsec);
    Platform::Delay(enterDelayUsec);
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Wrapper around the GpuSubdevice call to exit the GC6 deep idle state
RC GCxImpl::ExitGc6(GC6ExitParams &exitParams, UINT32 exitDelayUsec)
{
    RC rc;

    UINT64 freq  = Xp::QueryPerformanceFrequency();
    UINT64 start = Xp::QueryPerformanceCounter();
    if (m_UseRTD3 && exitParams.flavorId == LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID && !m_UseD3Hot)
    {
        CHECK_RC(m_pJT->RTD3PowerCycle(Xp::PowerState::PowerOn));
        if (m_PciConfigSpaceRestoreDelayMs)
        {
            Printf(m_VerbosePri, "Waiting %ums for cfg space to restore\n",
                    m_PciConfigSpaceRestoreDelayMs);
            Platform::Delay(m_PciConfigSpaceRestoreDelayMs * 1000);
        }
    }
    CHECK_RC(m_pSubdev->ExitGc6(exitParams));

    m_ExitTimeMs = ((Xp::QueryPerformanceCounter()-start)*1000)/freq;

    Printf(m_VerbosePri, "Waiting %d usec after GC6 exit.\n",exitDelayUsec);
    Platform::Delay(exitDelayUsec); // in usec
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Get the current pstate
UINT32 GCxImpl::GetLwrPstate()
{
    m_pSubdev->GetPerf()->GetLwrrentPerfPoint(&m_LwrPerfPt);
    return (m_LwrPerfPt.PStateNum);
}
//--------------------------------------------------------------------------------------------------
// Get the GPUs PTIMER value.
UINT64 GCxImpl::GetPTimer()
{
    RC rc;

    LW2080_CTRL_TIMER_GET_TIME_PARAMS params = {0};
    rc = m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_TIMER_GET_TIME,
           &params,
           sizeof(params));
    if (rc == OK)
    {
        return params.time_nsec;
    }
    return 0;
}

//--------------------------------------------------------------------------------------------------
void GCxImpl::GetGc6CycleStats(Xp::JTMgr::CycleStats *pStats)
{
    if (!m_pJT)
    {
        m_pJT = Xp::GetJTMgr(m_pSubdev->GetGpuInst());
    }
    return(m_pJT->GetCycleStats(pStats));
}

//--------------------------------------------------------------------------------------------------
// Colwert the defined wakeup event to an ascii string.
string GCxImpl::WakeupEventToString
(
    UINT32 wakeupEvent //!< wakeup event to colwert
)
{
    switch (wakeupEvent)
    {
        case GC_CMN_WAKEUP_EVENT_gc5rm              : return "gc5RM";
        case GC_CMN_WAKEUP_EVENT_gc5pex             : return "gc5PEX";
        case GC_CMN_WAKEUP_EVENT_gc5i2c             : return "gc5I2C";
        case GC_CMN_WAKEUP_EVENT_gc5timer           : return "gc5TIMER";
        case GC_CMN_WAKEUP_EVENT_gc5alarm           : return "gc5ALARM";
        case GC_CMN_WAKEUP_EVENT_gc6hotplugirq      : return "gc6HPDirq";
        case GC_CMN_WAKEUP_EVENT_gc6hotplug         : return "gc6HPDplug";
        case GC_CMN_WAKEUP_EVENT_gc6hotunplug       : return "gc6HPDunplug";
        case GC_CMN_WAKEUP_EVENT_gc6timer           : return "gc6TIMER";
        case GC_CMN_WAKEUP_EVENT_gc6i2c             : return "gc6I2C";
        case GC_CMN_WAKEUP_EVENT_gc6i2c_bypass      : return "gc6I2Cbypass";
        case GC_CMN_WAKEUP_EVENT_gc6gpu             : return "gc6GPU";
        case GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer   : return "gc6RTD3GPUTimer";
        case GC_CMN_WAKEUP_EVENT_gc6rtd3_cpu        : return "gc6RTD3CPU";
        default                                     : return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
string GCxImpl::WakeupReasonToString
(
    const LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS &parms,
    decodeStrategy strategy
)
{
    string reason = "";
    if (strategy == decodeGc5)
    {
        if (parms.gc5ExitType == LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5_SSC_ABORT)
        {
            // we need to decode the abort reason, not the wakeup reason.
            UINT16 code = parms.gc5AbortCode;
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_DEEP_L1_ENTRY_TIMEOUT,_YES,code))
                reason += " ABORT_DEEP_L1_ENTRY_TIMEOUT |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_DEEP_L1_EXIT,_YES,code))
                reason += " ABORT_DEEP_L1_EXIT |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_MSCG_ABORT,_YES,code))
                reason += " ABORT_MSCG_ABORT |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_RTOS_ABORT,_YES,code))
                reason += " ABORT_RTOS_ABORT |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_AZALIA_ACTIVE,_YES,code))
                reason += " ABORT_AZALIA_ACTIVE |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_HOST_NOT_IDLE,_YES,code))
                reason += " ABORT_HOST_NOT_IDLE |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_PENDING_PMC_INTR,_YES,code))
                reason += " ABORT_PENDING_PMC_INTR |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_PENDING_SCI_INTR,_YES,code))
                reason += " ABORT_PENDING_SCI_INTR |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_THERM_I2CS_BUSY,_YES,code))
                reason += " ABORT_THERM_I2CS_BUSY |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_SPELWLATIVE_PTIMER_ALARM,_YES,code))
                reason += " ABORT_SPELWLATIVE_PTIMER_ALARM |";
            if (FLD_TEST_DRF(2080_CTRL_GCX_GET_WAKEUP_REASON,_GC5_SSC_ABORT,_LATE_DEEP_L1_EXIT,_YES,code))
                reason += " ABORT_LATE_DEEP_L1_EXIT |";

            if (reason.length() == 0)
                reason += "ABORT_UNKNOWN";
            else
                reason.erase(reason.end()-1,reason.end()); // remove last '|'
        }
        else if (parms.gc5ExitType == LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5_SSC_EXIT)
        {
            UINT32 intr0 = parms.sciIntr0;
            UINT32 intr1 = parms.sciIntr1;
            // decode the exit reasons on intr0
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_GPU_EVENT,_PENDING, intr0))
                reason += "GPU |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_I2CS,_PENDING,intr0))
                reason += "I2C |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_WAKE_TIMER,_PENDING,intr0))
                reason += "TIMER |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_BSI_EVENT,_PENDING,intr0))
                reason += "BSI |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_0,_PENDING,intr0))
                reason += "HPDIRQ0 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_1,_PENDING,intr0))
                reason += "HPDIRQ1 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_2,_PENDING,intr0))
                reason += "HPDIRQ2 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_3,_PENDING,intr0))
                reason += "HPDIRQ3 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_4,_PENDING,intr0))
                reason += "HPDIRQ4 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_5,_PENDING,intr0))
                reason += "HPDIRQ5 |";

            // Now check intr1
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_0, _PENDING,intr1))
                reason += "HPD0 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_1, _PENDING,intr1))
                reason += "HPD1 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_2, _PENDING,intr1))
                reason += "HPD2 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_3, _PENDING,intr1))
                reason += "HPD3 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_4, _PENDING,intr1))
                reason += "HPD4 |";
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_5, _PENDING,intr1))
                reason += "HPD5 |";
        }
    }
    else if (strategy == decodeGc6)
    {
        UINT32 intr0 = parms.sciIntr0;
        UINT32 intr1 = parms.sciIntr1;
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_GPU_EVENT,_PENDING, intr0))
            reason += "GPU |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_I2CS,_PENDING,intr0))
            reason += "I2C |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_WAKE_TIMER,_PENDING,intr0))
            reason += "TIMER |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_BSI_EVENT,_PENDING,intr0))
            reason += "BSI |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_0,_PENDING,intr0))
            reason += "HPDIRQ0 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_1,_PENDING,intr0))
            reason += "HPDIRQ1 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_2,_PENDING,intr0))
            reason += "HPDIRQ2 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_3,_PENDING,intr0))
            reason += "HPDIRQ3 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_4,_PENDING,intr0))
            reason += "HPDIRQ4 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0,_STATUS_LWRR_HPD_IRQ_5,_PENDING,intr0))
            reason += "HPDIRQ5 |";

        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_0, _PENDING,intr1))
            reason += "HPD0 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_1, _PENDING,intr1))
            reason += "HPD1 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_2, _PENDING,intr1))
            reason += "HPD2 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_3, _PENDING,intr1))
            reason += "HPD3 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_4, _PENDING,intr1))
            reason += "HPD4 |";
        if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR1,_STATUS_LWRR_HPD_5, _PENDING,intr1))
            reason += "HPD5 |";
    }

    if (reason.length() == 0)
        reason = "Unknown";
    else
        reason.erase(reason.end()-1,reason.end()); // remove last '|'

    return reason;
}

//--------------------------------------------------------------------------------------------------
string GCxImpl::Gc5ExitTypeToString
(
    UINT32 exitType //!< wakeup reason to colwert
)
{
    switch (exitType)
    {
        case LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5_SSC_EXIT  : return "SSC_EXIT";
        case LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5_SSC_ABORT : return "SSC_ABORT";
        default: return "Unknown GC5 exitType";
    }
}

//-----------------------------------------------------------------------------
//! \brief Static function to poll for deep idle statistics
//!
//! \param pvArgs : Pointer to deep idle statistics polling structure
//!
//! \return true if polling should terminate, false otherwise
//!
/* static */ bool GCxImpl::PollGetDeepIdleStats(void *pvArgs)
{
    PollDeepIdleStruct *pPollStruct = (PollDeepIdleStruct *)pvArgs;
    LwRm * pLwRm = pPollStruct->pGCx->m_pLwRm;
    GpuSubdevice * pSubdev = pPollStruct->pGCx->m_pSubdev;
    pPollStruct->pollRc.Clear();
    pPollStruct->pollRc =
        pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
                      LW2080_CTRL_CMD_GPU_READ_DEEP_IDLE_STATISTICS,
                      pPollStruct->pParams,
                      sizeof(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS));
    return (pPollStruct->pollRc == RC::LWRM_STATUS_ERROR_NOT_READY) ? false :true;
}

//-----------------------------------------------------------------------------
RC GCxImpl::GetDeepIdleStats
(
    UINT32 reset, // 0=no, 1=yes
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats
)
{
    RC rc;
    PollDeepIdleStruct pollStruct;
    memset(pStats, 0, sizeof(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS));
    UINT32 mode = (m_Gc5Mode == gc5DI_SSC) ?
        LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_SSC:
        LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_NH ;

    LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_PARAMS modeParams = {mode,reset};
    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                            LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_STATISTICS_MODE,
                            &modeParams,sizeof(modeParams)));

    LW2080_CTRL_GPU_INITIATE_DEEP_IDLE_STATISTICS_READ_PARAMS initiateParams = {0};
    CHECK_RC(m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(m_pSubdev),
                            LW2080_CTRL_CMD_GPU_INITIATE_DEEP_IDLE_STATISTICS_READ,
                            &initiateParams,sizeof(initiateParams)));

    pollStruct.pGCx    = this;
    pollStruct.pParams = pStats;
    CHECK_RC(POLLWRAP(PollGetDeepIdleStats,
                      &pollStruct,
                      DEEP_IDLE_STATS_TIMEOUT));
    return pollStruct.pollRc;
}

//--------------------------------------------------------------------------------------------------
// public interface. Just copy over the most current stats.
RC GCxImpl::GetDeepIdleStats(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats)
{
    memcpy(pStats, &m_Gc5DeepIdleStats, sizeof(m_Gc5DeepIdleStats));
    return OK;
}
//--------------------------------------------------------------------------------------------------
// Create a real GCx implementation
GCxImpl* CreateGCx(GpuSubdevice *pSubdev)
{
    return new GCxImpl(pSubdev);
}

//--------------------------------------------------------------------------------------------------
// Create a null implementation of GCx for GPUs that don't support it.
GCxImpl* CreateNullGCx(GpuSubdevice *pSubdev)
{
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
// Set D3 hot property in RM
RC GCxImpl::SetUseD3Hot(bool bUse)
{
    if (!bUse && !m_UseD3Hot)
        return OK;

    RC rc;
    LW2080_CTRL_CMD_SET_GC6_RTD3_INFO_PARAMS params = {};
    params.bIsRTD3SupportedBySystem = true;

    if (bUse)
    {
        params.RTD3D3HotTestParams =
            DRF_DEF(2080, _CTRL_SET_RTD3_TEST, _PARAM_ENABLE, _TRUE) |
            DRF_DEF(2080, _CTRL_SET_RTD3_TEST, _FORCE_D3HOT, _TRUE);
    }
    else
    {
        params.RTD3D3HotTestParams =
            DRF_DEF(2080, _CTRL_SET_RTD3_TEST, _PARAM_ENABLE, _FALSE) |
            DRF_DEF(2080, _CTRL_SET_RTD3_TEST, _FORCE_D3HOT, _FALSE);
    }
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pSubdev,
           LW2080_CTRL_CMD_SET_GC6_RTD3_INFO,
           &params,
           sizeof(params)));
    m_UseD3Hot = bUse;
    return rc;
}
