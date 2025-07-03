/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Implements actions and action blocks

#include "pmaction.h"
#include "core/include/channel.h"
#include "gpu/utility/chanwmgr.h"
#include "class/cl2080.h"       // LW2080_NOTIFIERS_*
#include "class/cl906f.h"       // GF100_CHANNEL_GPFIFO
#include "class/cl906fsw.h"     // LW906F_NOTIFIERS_*
#include "class/cl9297.h"       // FERMI_C
#include "class/cl91c0.h"       // FERMI_COMPUTE_B
#include "class/cla06f.h"       // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h"       // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h"       // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h"       // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h"       // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h"       // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h"       // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h"       // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h"       // HOPPER_CHANNEL_GPFIFO_A
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "class/cl85b5sw.h"     // LW85B5_ALLOCATION_PARAMETERS
#include "class/cla0b5sw.h"     // LWA0B5_ALLOCATION_PARAMETERS
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/clc365.h"
#include "ctrl/ctrl208f.h"      // LW208F_CTRL_CMD_FB_SET_STATE
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080/ctrl2080mc.h"  // LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED
#include "ctrl/ctrl2080/ctrl2080power.h"
#include "ctrl/ctrl90f1.h"
#include "ctrl/ctrla06c.h"      //LWA06C_CTRL_CMD_PREEMPT LWA06C_CTRL_PREEMPT_PARAMS
#include "ctrl/ctrlc365.h"      //LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_RESET_COUNTERS
#include "ctrl/ctrlc36f.h"
// LW_PFB_PRI_MMU_ILWALIDATE_*, LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR*
#include "turing/tu102/dev_fb.h"
#include "fermi/gf110/dev_pmgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "core/include/platform.h"
#include "pmchan.h"
#include "pmevent.h"
#include "pmgilder.h"
#include "pmparser.h"
#include "pmpotevt.h"
#include "pmsurf.h"
#include "pmtest.h"
#include "pmtrigg.h"
#include "gpu/perf/pmusub.h"
#include "pmutils.h"
#include "gpu/utility/runlist.h"
#include "core/include/refparse.h"
#include "core/include/fileholder.h"
#include "gpu/utility/semawrap.h"
#include "core/include/utility.h"
#include "gpu/utility/runlwrap.h"
#include "pmchwrap.h"
#include "gpu/tests/gputestc.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include <set>
#include <memory>
#include "mmu/gmmu_fmt.h"
#include "lwos.h"
#include "pascal/gp100/dev_fault.h"
#include "ampere/ga100/dev_runlist.h"
#include "mdiag/utils/utils.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"
#include "pmvaspace.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/advschd/pmvftest.h"
#include "mdiag/vmiopmods/vmiopmdiagelw.h"
#include "turing/tu102/dev_vm.h"
#include "core/include/registry.h"
#include "lwRmReg.h"
#include "core/include/chiplibtracecapture.h"
#include "pmsmcengine.h"
#include "mdiag/vgpu_migration/vgpu_migration.h"
#include "mdiag/utl/utl.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/vaspaceutils.h"

#define __MSGID_POLICYMANAGER__(blockName, actionName) \
        PolicyManagerMessageDebugger::Instance()->GetTeeModuleCode(blockName, actionName)

// Save/Restore CHRAM
#define CHRAM_FILE_HDR "Test name : Channel name : Subdevice : Channel RAM data"
#define CHRAM_FILE_FORMAT "%s %s %d %08x"
#define MAX_LINE 1024

const FLOAT64 NO_WAIT = 0.0; //!< Use this to not wait for an event.

static RC CreateUpdatePdeStructs
(
    const PmMemRanges *pRanges,
    PolicyManager::PageSize PageSize,
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS> *pParams,
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PAGE_TABLE_PARAMS*> *pPteBlocks,
    GpuDevices *pGpuDevices
);

static RC GetSubdevicesForSurfaceAccess
(
    PmTrigger         *pTrigger,
    const PmEvent     *pEvent,
    const PmGpuDesc   *pLocalDesc,
    const PmGpuDesc   *pRemoteDesc,
    GpuSubdevice     **ppLocalSubdevice,
    GpuSubdevice     **ppRemoteSubdevice
);

enum GpioDirection
{
    GPIO_UP,
    GPIO_DOWN,
};

static RC SetGpioDirection
(
    const PmChannelDesc *pChannelDesc,
    PmChannel *pChannel,
    const vector<UINT32> &ports,
    GpioDirection direction
);

///////////////////////////////////////////////////////
// PmAction Utility tools
// Note: Just work for PmAction
///////////////////////////////////////////////////////
namespace PmActionUtility
{
    class MatchingVaSpace
    {
    public:
        MatchingVaSpace
        (
           PmTest * pTest,
           const PmVaSpaceDesc * pVaSpaceDesc
        ): m_pTest(pTest),
           m_pVaSpaceDesc(pVaSpaceDesc)
        {
        }

        RC GetVaSpaces
        (
           PmTrigger * pTrigger,
           const PmEvent * pEvent,
           PmVaSpaces * pPmVaSpaces,
           const PmSmcEngineDesc *pSmcEngineDesc
        ) const
        {
            RC rc;
            PmVaSpaces vaspaces;
            MASSERT(pTrigger);
            MASSERT(pEvent);

            LwRm* pLwRm = nullptr;
            if (pSmcEngineDesc) // If Policy.SetSmcEngine specified
            {
                PmSmcEngines smcEngines = pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
                MASSERT(smcEngines.size() == 1);
                pLwRm = smcEngines[0]->GetLwRmPtr();
            }

            // Policy.SetVaSpace
            if (m_pVaSpaceDesc)
            {
                vaspaces = m_pVaSpaceDesc->GetMatchingVaSpaces(pTrigger, pEvent, pLwRm);
            }
            else
            {
                PmVaSpace * pVaSpace;
                // Use the test's VaSpace
                if(m_pTest)
                {
                    CHECK_RC(m_pTest->GetUniqueVAS(&pVaSpace));
                    vaspaces.push_back(pVaSpace);
                }
                // For can't deduce from the PmEvent test, use the default 0 vaspace
                else
                {
                    // If multiple Gpu Partitions used, then the  user has to specify
                    // Policy.SetSmcEngine to get the partiton's default VaSpace
                    if (PolicyManager::Instance()->MultipleGpuPartitionsExist())
                    {
                        if (pSmcEngineDesc)
                        {
                            pVaSpace = pTrigger->GetPolicyManager()->GetActiveVaSpace(0, pLwRm);
                            vaspaces.push_back(pVaSpace);
                        }
                        else // Policy.SetSmcEngine not specified
                        {
                            ErrPrintf("GetVaSpaces(): When multiple Gpu Partitions are enabled "
                                      "Policy.SetSmcEngine needs to be specified\n");
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                    else // There is only one default VaSpace- use that
                    {
                        pVaSpace = pTrigger->GetPolicyManager()->GetActiveVaSpace(0, pLwRm);
                        vaspaces.push_back(pVaSpace);
                    }
                }
            }

            for(PmVaSpaces::iterator it = vaspaces.begin();
                    it != vaspaces.end(); ++it)
            {
                pPmVaSpaces->push_back(*it);
            }

            return rc;
        }

    private:
        PmTest * m_pTest;
        const PmVaSpaceDesc * m_pVaSpaceDesc;
    };

    namespace
    {
        typedef std::set<std::tuple<GpuDevice*,PmChannel*>> L2FlushGpuDeviceSet;

        //----------------------------------------------------------------------
        //! \brief Performs either an in-band or out-of-band L2 flush
        //!
        RC L2Flush
        (
            GpuDevice const * const pGpuDevice,
            PmChannel * const pInbandChannel
        )
        {
            MASSERT(pInbandChannel != nullptr || pGpuDevice != nullptr);
            RC rc;

            if (pInbandChannel != nullptr)
            {
                PM_BEGIN_INSERTED_METHODS(pInbandChannel);
                switch (pInbandChannel->GetModsChannel()->GetClass())
                {
                    case MAXWELL_CHANNEL_GPFIFO_A:
                        CHECK_RC(pInbandChannel->Write(
                            0,
                            LWB06F_MEM_OP_C,
                            2,
                            0,
                            DRF_DEF(B06F, _MEM_OP_D, _OPERATION, _L2_FLUSH_DIRTY)));
                        break;
                    case PASCAL_CHANNEL_GPFIFO_A:
                    case VOLTA_CHANNEL_GPFIFO_A:
                    case TURING_CHANNEL_GPFIFO_A:
                    case AMPERE_CHANNEL_GPFIFO_A:
                    case HOPPER_CHANNEL_GPFIFO_A:
                        CHECK_RC(pInbandChannel->Write(
                            0,
                            LWC06F_MEM_OP_A,
                            4,
                            0,
                            0,
                            0,
                            DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _L2_FLUSH_DIRTY)));
                        break;
                    default:
                        CHECK_RC(pInbandChannel->Write(
                            0,
                            LW906F_MEM_OP_A,
                            2,
                            0,
                            DRF_DEF(906F, _MEM_OP_B, _OPERATION, _L2_FLUSH_DIRTY)));
                        break;
                }
                PM_END_INSERTED_METHODS();
                CHECK_RC(pInbandChannel->Flush());
            }
            else
            {
                LwRmPtr pLwRm;
                LW0080_CTRL_DMA_FLUSH_PARAMS params = {0};
                params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                            _L2, _ENABLE);
                CHECK_RC(pLwRm->ControlByDevice(pGpuDevice,
                                                LW0080_CTRL_CMD_DMA_FLUSH,
                                                &params, sizeof(params)));
            }
            return rc;
        }

        //----------------------------------------------------------------------
        //! \brief Performs an L2 Flush if the GPU supports Post L2 Compression
        //!
        RC FlushForPostL2Compression
        (
            const L2FlushGpuDeviceSet &gpuDevicesToFlush
        )
        {
            RC rc;

            for (const auto &gpuDeviceAndChannel : gpuDevicesToFlush)
            {
                auto pGpudevice = std::get<0>(gpuDeviceAndChannel);
                auto pInbandChannel = std::get<1>(gpuDeviceAndChannel);
                for (UINT32 i = 0; i < pGpudevice->GetNumSubdevices(); ++i)
                {
                    if (pGpudevice->GetSubdevice(i)->HasFeature(
                        Device::GPUSUB_SUPPORTS_POST_L2_COMPRESSION))
                    {
                        CHECK_RC(L2Flush(pGpudevice, pInbandChannel));
                        break;
                    }
                }
            }

            return rc;
        }

        //----------------------------------------------------------------------
        //! \brief Wrapper function around MemRange::MovePhysMem.
        //!        Updates tracking of which devices to do L2 flush.
        //!
        RC MovePhysMem
        (
            const PmMemRange &MemRange,
            Memory::Location Location,
            PolicyManager::MovePolicy MovePolicy,
            PolicyManager::LoopBackPolicy Loopback,
            bool DeferTlbIlwalidate,
            PmChannel *pInbandChannel,
            PolicyManager::AddressSpaceType TargetVaSpace,
            Surface2D::VASpace SurfAllocType,
            Surface2D::GpuSmmuMode SurfGpuSmmuMode,
            const PmMemRange * pAllocatedSrcRange,
            bool DisablePostL2Compression,
            L2FlushGpuDeviceSet &GpuDevicesToFlush
        )
        {
            RC rc;

            if (Utl::HasInstance())
            {
                if(Utl::Instance()->HasUpdateSurfaceInMmu(MemRange.GetMdiagSurf()->GetName()))
                {
                    ErrPrintf("PolicyManager: UTL side has been modified this surface %s.\n",
                            MemRange.GetMdiagSurf()->GetName().c_str());
                }
            }

            PolicyManager * pPolicyMgr = PolicyManager::Instance();
            pPolicyMgr->SetUpdateSurfaceInMmu(MemRange.GetMdiagSurf()->GetName());

            CHECK_RC(MemRange.MovePhysMem(
                    Location, MovePolicy, Loopback, DeferTlbIlwalidate,
                    pInbandChannel, TargetVaSpace, SurfAllocType,
                    SurfGpuSmmuMode, pAllocatedSrcRange,
                    DisablePostL2Compression));

            GpuDevicesToFlush.emplace(
                    MemRange.GetMdiagSurf()->GetGpuDev(), pInbandChannel);

            return rc;
        }

    }
}
//////////////////////////////////////////////////////////////////////
// PmActionBlock
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmActionBlock::PmActionBlock
(
    PmEventManager *pEventManager,
    const string &name
) :
    m_pEventManager(pEventManager), m_Name(name)
{
    MASSERT(pEventManager != NULL);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
//! Frees all actions that were added by AddAction()
//!
PmActionBlock::~PmActionBlock()
{
    for (size_t ii = 0; ii < m_Actions.size(); ++ii)
    {
        delete m_Actions[ii];
    }
    m_Actions.clear();
}

//--------------------------------------------------------------------
//! \brief Add a new action to the action block
//!
RC PmActionBlock::AddAction(PmAction *pAction)
{
    RC rc;

    pAction->SetActionBlock(this);
    m_Actions.push_back(pAction);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Check whether the ActionBlock is supported
//!
//! This method is called at parse-time, after all actions have been
//! added, to determine whether the action block is supported on this
//! hardware for the indicated trigger.  It basically just calls
//! PmAction::IsSupported() on all actions in the action block.
//!
RC PmActionBlock::IsSupported(const PmTrigger *pTrigger) const
{
    RC rc;

    for (size_t ii = 0; ii < m_Actions.size(); ++ii)
    {
        CHECK_RC(m_Actions[ii]->IsSupported(pTrigger));
    }
    return rc;
}

#ifndef SIM_BUILD
bool Platform::GetDumpPhysicalAccess() { return false; }
void Platform::SetDumpPhysicalAccess(bool flag) {};
#endif

//--------------------------------------------------------------------
//! \brief Execute the action block.
//!
//! \param pTrigger is the trigger that kicked off the action
//! \param pEvent is the event received that kicked off the action

RC PmActionBlock::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    MASSERT(pTrigger != NULL);
    MASSERT(pEvent != NULL);
    PolicyManager *pPolicyManager = GetPolicyManager();
    UINT32 teeCode = 0;
    RC rc;

    ChiplibOpScope newScope("Execute policy action block " + m_Name, NON_IRQ,
                             ChiplibOpScope::SCOPE_COMMON, NULL);

    // Make sure we release all mutexes when this function returns.
    //
    Utility::CleanupOnReturn<PmActionBlock> cleanup(this,
            &PmActionBlock::ReleaseMutexes);

    for (vector<PmAction*>::iterator ppAction = m_Actions.begin();
         ppAction != m_Actions.end();
         ppAction += (*ppAction)->GetNextAction())
    {
        PmAction *pAction = *ppAction;

        if (!pPolicyManager->InAnyTest())
        {
            break;  // Abort the action block if all tests end
        }

        // Special case OnActionBlock which pAction has no name.
        if (GetName() != "" || pAction->GetName() != "")
        {
            teeCode = __MSGID_POLICYMANAGER__(GetName(), pAction->GetName());
            if(pAction->GetName() != "")
            {
                DebugPrintf(teeCode, "PolicyManager: exelwting %s Begin\n", pAction->GetName().c_str());
            }
        }

        if (pPolicyManager->MutexDisabledInActions())
        {
            // PmMutexSaver temporarily releases the mutex
            PolicyManager::PmMutexSaver savedPmMutex(pPolicyManager);
            rc = pAction->Execute(pTrigger, pEvent);
        }
        else
        {
            pAction->SetTeeCode(teeCode);
            if(Tee::IsModuleEnabled(teeCode))
            {
                // If the -dump_physical_access comes with the -enable_message,
                // The priortiy is -dump_physical_access > -enable_message
                // or -d enable
                if(Platform::GetDumpPhysicalAccess() ||
                        ((Tee::LevDebug == CommandLine::ScreenLevel()) &&
                         (Tee::LevDebug == CommandLine::FileLevel()) &&
                         (Tee::LevDebug == CommandLine::DebuggerLevel())))
                {
                    rc = pAction->Execute(pTrigger, pEvent);
                }
                else
                {
                    if(pAction->GetName() != "")
                    {
                        Platform::SetDumpPhysicalAccess(true);
                        rc = pAction->Execute(pTrigger, pEvent);
                        Platform::SetDumpPhysicalAccess(false);
                    }
                    else
                    {
                        rc = pAction->Execute(pTrigger, pEvent);
                    }
                }
            }
            else
            {
                rc = pAction->Execute(pTrigger, pEvent);
            }
        }

        if(pAction->GetName() != "")
        {
            if (rc != OK)
            {
                ErrPrintf("PolicyManager: %s returned error %d %s\n",
                          pAction->GetName().c_str(), (INT32)rc, rc.Message());
            }
            else
            {
                DebugPrintf(teeCode, "PolicyManager: exelwting %s End\n", pAction->GetName().c_str());
            }
        }
        CHECK_RC(rc);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by PmEventManager::EndTest to do any cleanup
//!
RC PmActionBlock::EndTest()
{
    StickyRC firstRc;

    for (vector<PmAction*>::iterator ppAction = m_Actions.begin();
         ppAction != m_Actions.end();
         ppAction += (*ppAction)->GetNextAction())
    {
        firstRc = (*ppAction)->EndTest();
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Create mutex holder for actions which require locking for
//!        the duration of exelwtion of entire action block.
//!
Tasker::MutexHolder& PmActionBlock::CreateMutexHolder()
{
    m_MutexHolders.push_back(new Tasker::MutexHolder);
    return *m_MutexHolders.back();
}

//--------------------------------------------------------------------
//! \brief Release all mutexes locked by actions.
//!
void PmActionBlock::ReleaseMutexes()
{
    for (size_t i=0; i < m_MutexHolders.size(); i++)
    {
        delete m_MutexHolders[i];
    }
    m_MutexHolders.clear();
}

//////////////////////////////////////////////////////////////////////
// PmAction
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction::~PmAction()
{
}

//--------------------------------------------------------------------
//! \fn RC PmAction::IsSupported(const PmTrigger *pTrigger) const
//! \brief Check whether the action can be exelwted
//!
//! This is called at parse-time by PmActionBlock::IsSupported() to
//! determine whether the action is supported on this hardware for the
//! indicated trigger.
//!
//! \param pTrigger The PmTrigger that will trigger this action.
//!     This is used to check for things like a "remap faulting page"
//!     action getting bound to Trigger.Start().
//!
/* virtual RC PmAction::IsSupported(const PmTrigger *pTrigger) const = 0 */

//--------------------------------------------------------------------
//! Return how many actions to advance in the action block after
//! exelwting this action.  This method is designed to support
//! PmAction_Goto, which should be the only subclass to override this
//! method.  All other actions returns +1 (advance to the next
//! action).
//!
//! The return value may be negative, to step backward.  This
//! functionality could be used to implement loops.
//!
/* virtual */ INT32 PmAction::GetNextAction()
{
    return 1;
}

//--------------------------------------------------------------------
//! \brief Called by PmEventManager::EndTest to do any cleanup
//!
RC PmAction::EndTest()
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Get the channel to use for sending inband methods.
//!
PmChannel *PmAction::GetInbandChannel
(
    PmChannelDesc *pChannelDesc,
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    const PmMemRange *pMemRange
)
{
    PmChannel *pChannel = 0;
    PmChannels channelList;

    // If a valid channel descriptor was passed, use that to get the
    // list of possible channels to use for sending inband methods.
    if (pChannelDesc != 0)
    {
        channelList = pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    }

    // Otherwise get the list of active channels for the GPU
    // associated with the passed subsurface.
    else
    {
        MASSERT(pMemRange != 0);
        MdiagSurf *pSurface = pMemRange->GetMdiagSurf();
        GpuDevice *pGpuDevice = pSurface->GetGpuDev();
        channelList = GetPolicyManager()->GetActiveChannels(pGpuDevice);
    }

    // Grab the first channel of the list, if one exists.
    if (channelList.size() > 0)
    {
        pChannel = channelList[0];
    }

    return pChannel;
}

RC PmAction::GetEnginesList
(
    RegExp engineNames,
    PmChannel* pPmChannel,
    GpuDevice* pGpuDevice,
    vector<UINT32>* pEngines
)
{
    RC rc;
    vector<UINT32> allEngines;
    LwRm* pLwRm = pPmChannel->GetLwRmPtr();
    // Starting from Ampere, channel will be storing the engineId
    // associated with it
    if (!(pGpuDevice->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID)))
    {
        // Loop through all matching engines
        //
        CHECK_RC(pGpuDevice->GetEngines(&allEngines, pLwRm));
        for (vector<UINT32>::iterator pEngine = allEngines.begin();
                pEngine != allEngines.end(); ++pEngine)
        {
            if (engineNames.Matches(pGpuDevice->GetEngineName(*pEngine)))
            {
                (*pEngines).push_back(*pEngine);
            }
        }
    }
    else
    {
        (*pEngines).push_back(pPmChannel->GetEngineId());
    }

    return rc;
}

map<LwRm*, PmSmcEngines> PmAction::GetLwRmSmcEngines
(
    const PmTrigger *pTrigger,
    const PmEvent   *pEvent,
    const PmSmcEngineDesc *pSmcEngineDesc
) const
{
    map<LwRm*, PmSmcEngines> mapLwRmPmSmcEngines;
    PmSmcEngines smcEngines;

    if (pSmcEngineDesc)
    {
        smcEngines = pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
        MASSERT(smcEngines.size() != 0);
    }

    // Get all unique RM clients in case user has set more than one SmcEngine
    if (smcEngines.size())
    {
        for (auto smcEngine : smcEngines)
        {
            mapLwRmPmSmcEngines[smcEngine->GetLwRmPtr()].push_back(smcEngine);
        }
    }
    else
    {
        mapLwRmPmSmcEngines[LwRmPtr().Get()] = { };
    }

    return mapLwRmPmSmcEngines;
}

//////////////////////////////////////////////////////////////////////
// PmAction_Block
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_Block::PmAction_Block
(
    PmActionBlock *pActionBlock
) :
    PmAction("Action.Block"),
    m_pActionBlock(pActionBlock)
{
    MASSERT(m_pActionBlock != 0);
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting surface
//!
/* virtual */ RC PmAction_Block::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pActionBlock->IsSupported(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Print the string
//!
/* virtual */ RC PmAction_Block::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    return m_pActionBlock->Execute(pTrigger, pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmAction_Print
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_Print::PmAction_Print
(
    const string &printString
) :
    PmAction("Action.Print"),
    m_PrintString(printString)
{
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting surface
//!
/* virtual */ RC PmAction_Print::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if ((m_PrintString.find("%c%", 0) != string::npos) &&
        !pTrigger->HasChannel())

    {
        ErrPrintf("This trigger does not support this print string (missing channel)\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Impement pTrigger->HasRunlist() if there are triggers bound to runlist
    if (((m_PrintString.find("%rpe%", 0) != string::npos) ||
         (m_PrintString.find("%rre%", 0) != string::npos)) &&
        !pTrigger->HasChannel())

    {
        ErrPrintf("This trigger does not support this print string (missing channel or runlist)\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if ((m_PrintString.find("%m%", 0) != string::npos) &&
        !pTrigger->HasMethodCount())
    {
        ErrPrintf("This trigger does not support this print string (missing method counter)\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if ((m_PrintString.find("%s%", 0) != string::npos) &&
        !pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support this print string (missing faulting surfaces)\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print the string
//!
/* virtual */ RC PmAction_Print::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    string printString = m_PrintString;
    string subsurfaceNames;
    char methodStr[11];
    RC rc;

    string::size_type loc;

    while ((loc = printString.find("%c%", 0)) != string::npos)
    {
        PmChannels channels = pEvent->GetChannels();
        string strChannelName = " ";
        for (PmChannels::iterator iter = channels.begin();
             iter != channels.end(); ++iter)
        {
            strChannelName += (channels[0]->GetName() + " ");
        }
        printString.replace(loc, 3, strChannelName);
    }

    while ((loc = printString.find("%rpe%", 0)) != string::npos)
    {
        string strChannelName = " ";
        PmChannels channels = pEvent->GetChannels();
        for (PmChannels::iterator iter = channels.begin();
             iter != channels.end(); ++iter)
        {
            RunlistChannelWrapper *pRunlistWrapper =
                (*iter)->GetModsChannel()->GetRunlistChannelWrapper();
            if (pRunlistWrapper == NULL)
            {
                continue;
            }

            Runlist *pRunlist;
            CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));

            Runlist::Entries preemptEntries =
                GetPolicyManager()->GetPreemptRlEntries(pRunlist);
            for (UINT32 i = 0; i < preemptEntries.size() ; i++)
            {
                PmChannelWrapper* pmChWrap = preemptEntries[i].pChannel->GetPmChannelWrapper();
                if (pmChWrap && pmChWrap->GetPmChannel())
                    strChannelName += pmChWrap->GetPmChannel()->GetName() + " ";
            }
        }

        printString.replace(loc, 5, strChannelName);
    }

    while ((loc = printString.find("%rre%", 0)) != string::npos)
    {
        string strChannelName = " ";
        PmChannels channels = pEvent->GetChannels();
        for (PmChannels::iterator iter = channels.begin();
             iter != channels.end(); ++iter)
        {
            RunlistChannelWrapper *pRunlistWrapper =
                (*iter)->GetModsChannel()->GetRunlistChannelWrapper();
            if (pRunlistWrapper == NULL)
            {
                continue;
            }

            Runlist *pRunlist;
            CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));

            Runlist::Entries removedEntries =
                GetPolicyManager()->GetRemovedRlEntries(pRunlist);
            for (UINT32 i = 0; i < removedEntries.size() ; i++)
            {
                PmChannelWrapper* pmChWrap = removedEntries[i].pChannel->GetPmChannelWrapper();
                if (pmChWrap && pmChWrap->GetPmChannel())
                    strChannelName += pmChWrap->GetPmChannel()->GetName() + " ";
            }
        }

        printString.replace(loc, 5, strChannelName);
    }

    if (printString.find("%s%", 0) != string::npos)
    {
        PmSubsurfaces faultingSubsurfaces = pEvent->GetSubsurfaces();

        for (PmSubsurfaces::iterator iter = faultingSubsurfaces.begin();
             iter != faultingSubsurfaces.end(); iter++)
        {
            subsurfaceNames += (*iter)->GetName() + " ";
        }
    }
    while ((loc = printString.find("%s%", 0)) != string::npos)
    {
        printString.replace(loc, 3, subsurfaceNames);
    }

    while ((loc = printString.find("%m%", 0)) != string::npos)
    {
        sprintf(methodStr, "%d", pEvent->GetMethodCount());
        printString.replace(loc, 3, methodStr);
    }

    PrintMmuTable(&printString, "%mmu:");
    PrintMmuTable(&printString, "%mmubit:");
    PrintPhysicalRead(&printString);
    DumpSurface(&printString);
    DumpFaultBuffer(&printString, pEvent);

    Printf(Tee::PriAlways, "%s\n", printString.c_str());
    return OK;
}

void PmAction_Print::PrintMmuTable
(
    string * printString,
    const string & keyword
)
{
    // if keyword == "%mmu:"
    // format Action_Print("%mmu:<surface name>%");
    // print basic mmu information
    // if keyword == "%mmubit:"
    // format Action_Print("%mmubit:<surface name>%");
    // print each entry bit mmu information
    size_t start, end;
    if (((start = printString->find(keyword, 0)) != string::npos) &&
        ((end = printString->find("%", start + 1)) != string::npos))
    {
        if (printString->find(keyword, start + 1) != string::npos)
        {
            MASSERT(!"Action_Print too much %mmu: found.\n");
        }

        string name = printString->substr(start + keyword.size(),
                end - start - keyword.size());
        PolicyManager * pPolicyManager = GetPolicyManager();
        PmSubsurfaces subSurfaces = pPolicyManager->GetActiveSubsurfaces();
        for (PmSubsurfaces::iterator ppSubsurface = subSurfaces.begin();
                ppSubsurface != subSurfaces.end(); ++ ppSubsurface)
        {
            if ((*ppSubsurface)->GetName() == name)
            {
                printString->replace(start + keyword.size(),
                        end - start -keyword.size(), name);
                string mmuinfo;
                (*ppSubsurface)->GetSurface()->PrintMmuLevelSegmentInfo(&mmuinfo,
                        keyword == "%mmu:" ? false : true);
                (*printString) += "\n" + mmuinfo;
                return;
            }
        }
    }

}

void PmAction_Print::PrintPhysicalRead
(
    string * printString
)
{
    // format Action_Print("%physrd:<Aperture>:<address>:<size>%");
    const string keyword = "%physrd";
    size_t start, end;
    if (((start = printString->find(keyword, 0)) != string::npos) &&
        ((end = printString->find("%", start + 1)) != string::npos))
    {
        if (printString->find(keyword, start + 1) != string::npos)
        {
            MASSERT(!"Action_Print too much %physrd found.\n");
        }
        size_t apertureBegin = start + keyword.size() + 1; // size of ":"
        size_t apertureEnd = printString->find(":", apertureBegin);
        size_t physAddressBegin = apertureEnd + 1; // size of ":"
        size_t physAddressEnd = printString->find(":", physAddressBegin); // size of ":"
        size_t sizeBegin = physAddressEnd + 1; //sizeof ":"
        size_t sizeEnd = end; // size of "%"

        string aperture = printString->substr(apertureBegin, apertureEnd - apertureBegin);
        string address = printString->substr(physAddressBegin, physAddressEnd - physAddressBegin);
        UINT64 physAddress = static_cast<UINT64>(strtol(address.c_str(), NULL, 0));
        string size = printString->substr(sizeBegin, sizeEnd - sizeBegin);
        UINT32 realSize = static_cast<UINT32>(strtol(size.c_str(), NULL, 0));
        Memory::Location location;
        if ("Aperture.Framebuffer()" == aperture) location = Memory::Fb;
        else if ("Aperture.Coherent()" == aperture) location = Memory::Coherent;
        else if ("Aperture.NonCoherent()" == aperture) location = Memory::NonCoherent;
        else
        {
            WarnPrintf("Please double check the aperture which doesn't support in the function.\n");
            return;
        }
        printString->replace(start, end - start + 1, "phyrd " + size + " byte at aperture " + aperture +
                 " " +  address + " = ");
        string info;
        if (PraminPhysRd(physAddress, location, realSize, &info) != OK)
        {
            ErrPrintf("Failed to read physical address 0x%llx via PRAMIN\n", physAddress);
            MASSERT(0);
        }
        (*printString) += info;
        return;
    }
}

void PmAction_Print::DumpSurface
(
    string * printString
)
{
    GpuSubdevices gpuSubdevices = PolicyManager::Instance()->GetGpuSubdevices();

    // format Action_Print("%surfacedata:<surface name>:<offset>:<size>%");
    size_t start, end;
    const string keyword = "%surfacedata:";
    if (((start = printString->find(keyword, 0)) != string::npos) &&
        ((end = printString->find("%", start + 1)) != string::npos))
    {
        if (printString->find(keyword, start + 1) != string::npos)
        {
            MASSERT(!"Action_Print too much %surfacedata: found.\n");
        }
        size_t surfaceBegin, offsetBegin, sizeBegin;
        size_t surfaceEnd, offsetEnd, sizeEnd;
        surfaceBegin = start + keyword.size();
        sizeEnd = end;
        surfaceEnd = printString->find(":", surfaceBegin);
        offsetBegin = surfaceEnd + 1; // size of ":"
        offsetEnd = printString->find(":", offsetBegin);
        sizeBegin = offsetEnd + 1; // size of ":"

        string surfaceName = printString->substr(surfaceBegin, surfaceEnd - surfaceBegin);
        string offsetStr = printString->substr(offsetBegin, offsetEnd - offsetBegin);
        UINT64 offset = static_cast<UINT64>(strtol(offsetStr.c_str(), NULL, 0));
        string sizeStr = printString->substr(sizeBegin, sizeEnd - sizeBegin);
        UINT32 size = static_cast<UINT32>(strtol(sizeStr.c_str(), NULL, 0));

        printString->replace(start, end - start + 1, surfaceName + " at offset " + offsetStr + " size " + sizeStr);
        PolicyManager * pPolicyManager = PolicyManager::Instance();
        PmSubsurfaces pmSubsurfaces = pPolicyManager->GetActiveSubsurfaces();
        for (PmSubsurfaces::const_iterator ppSurface = pmSubsurfaces.begin();
            ppSurface != pmSubsurfaces.end(); ++ppSurface)
        {
            if ((*ppSurface)->GetName() == surfaceName)
            {
                for (GpuSubdevices::iterator pSubdev = gpuSubdevices.begin();
                    pSubdev != gpuSubdevices.end(); ++ pSubdev)
                {
                    vector<UINT08> data;
                    data.resize(size);
                    PmMemRange readRange((*ppSurface)->GetSurface(), offset + (*ppSurface)->GetOffset(), size);
                    readRange.Read(&data, *pSubdev);
                    UINT32 * pData = reinterpret_cast<UINT32 *>(&(data[0]));
                    for (size_t i = 0; i < data.size() / 4; ++i)
                    {
                        if (i%8 == 0) (*printString) += Utility::StrPrintf("\n");
                        (*printString) += Utility::StrPrintf("0x%08x ", pData[i]);
                    }

                    if (data.size() % 4 != 0)
                    {
                        size_t baseIndex = data.size() / 4 * 4;

                        for (size_t i = 0; i < data.size() % 4; ++ i)
                        {
                            (*printString) += Utility::StrPrintf("0x%02x", pData[baseIndex + i + 1]);
                        }
                    }

                    return;
                }
            }
        }
    }

}

void PmAction_Print::DumpFaultBuffer
(
    string * printString,
    const PmEvent * pEvent
)
{
    // format Action_Print("%faultTimeStamp%");
    const string keyword = "%faultTimeStamp%";
    size_t start;
    while (((start = printString->find(keyword, 0)) != string::npos))
    {
        if (pEvent->GetType() == PmEvent::REPLAYABLE_FAULT ||
            pEvent->GetType() == PmEvent::CE_RECOVERABLE_FAULT ||
            pEvent->GetType() == PmEvent::NON_REPLAYABLE_FAULT)
        {
           printString->replace(start, keyword.size(), "faultTimeStamp = ");
           (*printString) += Utility::StrPrintf("0x%016llx", pEvent->GetTimeStamp());
        }
        else
        {
            WarnPrintf("Fault type %d doesn't support faultTimeStamp!\n", pEvent->GetType());
            break;
        }
    }

}

//////////////////////////////////////////////////////////////////////
// PmAction_TriggerUtlUserEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_TriggerUtlUserEvent::PmAction_TriggerUtlUserEvent
(
    const vector<string> &data
) :
    PmAction("Action.TriggerUtlUserEvent"),
    m_Data(data)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted.
//!
/* virtual */ RC PmAction_TriggerUtlUserEvent::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Trigger user event, colwert the strings into map and send to UTL.
//!
/* virtual */ RC PmAction_TriggerUtlUserEvent::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    map<string, string> utlData;

    if (!Utl::HasInstance())
    {
        ErrPrintf("TriggerUtlUserEvent must co-run with UTL\n");
        return RC::SCRIPT_FAILED_TO_EXELWTE;
    }

    
    // Error message is printed in pmparser.cpp, add a check here in case m_Data gets modified
    MASSERT(!(m_Data.size() % 2));
    for (auto it = m_Data.cbegin(); it != m_Data.cend(); it += 2)
    {
        utlData.insert({*it, *(it + 1)});
    }

    Utl::Instance()->TriggerUserEvent(move(utlData));
    
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PreemptRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PreemptRunlist::PmAction_PreemptRunlist
(
    const PmRunlistDesc *pRunlistDesc,
    const PmSmcEngineDesc *pSmcEngineDesc
):
    PmAction("Action.PreemptRunlist"),
    m_pRunlistDesc(pRunlistDesc),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PreemptRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pRunlistDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief preempt the indicated runlist.
//!
/* virtual */ RC PmAction_PreemptRunlist::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        Runlists runlists;
        CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
        for (Runlists::iterator ppRunlist = runlists.begin();
             ppRunlist != runlists.end(); ++ppRunlist)
        {
            Runlist::Entries preemptEntries;
            Tasker::MutexHolder& lock = GetActionBlock()->CreateMutexHolder();
            CHECK_RC((*ppRunlist)->Preempt(&preemptEntries, &lock));
            GetPolicyManager()->AddPreemptRlEntries(*ppRunlist, preemptEntries);
        }
    }
    else if (m_pRunlistDesc->GetChannelDesc())
    {
        vector< tuple<GpuDevice*, LwRm*, UINT32> > deviceEngines;
        CHECK_RC(m_pRunlistDesc->GetChannelsEngines(pTrigger, pEvent, &deviceEngines));
        for (auto const & deviceEngine : deviceEngines)
        {
            GpuDevice* pGpuDev;
            LwRm* pLwRm;
            UINT32 engineId;

            tie(pGpuDev, pLwRm, engineId) = deviceEngine;
            CHECK_RC(pGpuDev->StopEngineRunlist(engineId, pLwRm));
        }
    }
    else
    {
        map<LwRm*, PmSmcEngines> mapLwRmPmSmcEngines = GetLwRmSmcEngines(pTrigger, pEvent, m_pSmcEngineDesc);

        // Get Matching engines per RM client and Resubmit them
        for (auto pairLwRmPmSmcEngines : mapLwRmPmSmcEngines)
        {
            vector< pair<GpuDevice*, UINT32> > deviceEngines;
            LwRm* pLwRm = pairLwRmPmSmcEngines.first;
            PmSmcEngines lwRmSmcEngines = pairLwRmPmSmcEngines.second;
            CHECK_RC(m_pRunlistDesc->GetMatchingEngines(pTrigger, pEvent, &deviceEngines, pLwRm, lwRmSmcEngines));
            for (vector< pair<GpuDevice*, UINT32> >::iterator deviceEngine = deviceEngines.begin();
                    deviceEngine != deviceEngines.end(); ++deviceEngine)
            {
                if (lwRmSmcEngines.size())
                {
                    InfoPrintf("Preempting %s engine in swizid %d\n",
                               deviceEngine->first->GetEngineName(deviceEngine->second), lwRmSmcEngines[0]->GetSwizzId());
                }
                else
                {
                    InfoPrintf("Preempting %s engine\n", deviceEngine->first->GetEngineName(deviceEngine->second));
                }
                CHECK_RC(deviceEngine->first->StopEngineRunlist(deviceEngine->second, pLwRm));
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PreemptChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PreemptChannel::PmAction_PreemptChannel
(
    const PmChannelDesc *pChannelDesc,
    bool  bPollPreemptComplete,
    FLOAT64 timeoutMs
):
    PmAction("Action.PreemptChannel"),
    m_pChannelDesc(pChannelDesc),
    m_PollPreemptComplete(bPollPreemptComplete),
    m_TimeoutMs(timeoutMs)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PreemptChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Polling function to wait for Action.PreemptChannel done on
//!        a GpuDevice
//!
/* static */ bool PmAction_PreemptChannel::PollPreemptDone(void* pvArgs)
{
    PmChannel *pPmChannel = static_cast<PmChannel*>(pvArgs);
    GpuDevice *pDevice = pPmChannel->GetGpuDevice();
    LwRm* pLwRm = pPmChannel->GetLwRmPtr();
    SmcEngine* pSmcEngine = pPmChannel->GetPmSmcEngine() ? pPmChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr;

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
    {

        // Begin from Ampere, LW_PFIFO_PREEMPT is replicated to
        // be per engine (MODS_RUNLIST_PREEMPT). each has a PRI base offser.

        GpuSubdevice *pSubdev = pDevice->GetSubdevice(subdev);
        LWGpuResource *pGpuRes = pPmChannel->GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);

        RegHal& regs = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

        if (pSubdev->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
        {
            if (regs.Test32(pPmChannel->GetEngineId(),
                        MODS_RUNLIST_PREEMPT_RUNLIST_PREEMPT_PENDING_TRUE))
            {
                return false;
            }
        }
        else
        {
            if (regs.Test32(
                        MODS_PFIFO_PREEMPT_PENDING_TRUE))
            {
                return false;
            }
        }
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief preempt the channel which does not belong to any tsg.
//!
RC PmAction_PreemptChannel::PreemptChannel
(
    PmChannel* pPmChannel
)
{
    RC rc;
    GpuDevice *pDevice = pPmChannel->GetGpuDevice();
    UINT32 channelId = pPmChannel->GetModsChannel()->GetChannelId();
    LwRm* pLwRm = pPmChannel->GetLwRmPtr();
    SmcEngine* pSmcEngine = pPmChannel->GetPmSmcEngine() ? pPmChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr;

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices();
         subdev++)
    {
        // Begin from Ampere, LW_PFIFO_PREEMPT is replicated to
        // be per engine (MODS_RUNLIST_PREEMPT). each has a PRI base offser.

        GpuSubdevice *pSubdev = pDevice->GetSubdevice(subdev);

        LWGpuResource *pGpuRes = GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);

        RegHal& regs = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

        if (pSubdev->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
        {
            regs.Write32(pPmChannel->GetEngineId(),
                            MODS_RUNLIST_PREEMPT, channelId);
        }
        else
        {
            regs.Write32(
                    MODS_PFIFO_PREEMPT_ID,
                    channelId);
        }
    }

    if (m_PollPreemptComplete)
    {
        // Polling to wait until the channel is preempted
        CHECK_RC(POLLWRAP_HW(PmAction_PreemptChannel::PollPreemptDone,
                             pPmChannel,
                             m_TimeoutMs));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief preempt tsg which contains the channel.
//!
RC PmAction_PreemptChannel::PreemptTsg
(
    PmChannel* pPmChannel
)
{
    RC rc;
    LwRm* pLwRm = pPmChannel->GetLwRmPtr();
    LWA06C_CTRL_PREEMPT_PARAMS preemptParams = {0};
    preemptParams.bWait = m_PollPreemptComplete;

    CHECK_RC(pLwRm->Control(pPmChannel->GetTsgHandle(),
             LWA06C_CTRL_CMD_PREEMPT, &preemptParams, sizeof(preemptParams)));

    return rc;
}

//--------------------------------------------------------------------
//! \brief preempt the indicated runlist.
//!
/* virtual */ RC PmAction_PreemptChannel::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    PmChannels channels;
    GpuSubdevices subdevices;
    RC rc;

    channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    if (channels.size() > 1)
    {
        ErrPrintf("Action.PreemptChannel: More than one channel found for preemption\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (channels[0]->GetTsgHandle())
    {
        CHECK_RC(PreemptTsg(channels[0]));
    }
    else
    {
        CHECK_RC(PreemptChannel(channels[0]));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemoveEntriesFromRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemoveEntriesFromRunlist::PmAction_RemoveEntriesFromRunlist
(
    const PmRunlistDesc *pRunlistDesc,
    UINT32 firstEntry,
    int numEntries
):
    PmAction("Action.RemoveEntriesFromRunlist"),
    m_pRunlistDesc(pRunlistDesc),
    m_FirstEntry(firstEntry),
    m_NumEntries(numEntries)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RemoveEntriesFromRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pRunlistDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Remove entries from the preempted runlist indicated by runlistDesc.
//!
/* virtual */ RC PmAction_RemoveEntriesFromRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        Runlist::Entries preemptEntries =
                         GetPolicyManager()->GetPreemptRlEntries(*ppRunlist);
        Runlist::Entries removedEntries =
                         GetPolicyManager()->GetRemovedRlEntries(*ppRunlist);

        if (preemptEntries.size() > 0)
        {
            // 0 means removing all entries
            int numEntries = m_NumEntries;
            if (m_NumEntries == 0)
                numEntries = (int)preemptEntries.size();

            int startInd = 0, endInd = 0; // [startInd, endInd)
            if (numEntries >= 0)
            {
                startInd = m_FirstEntry;
                endInd = startInd + numEntries;
            }
            else
            {
                endInd = m_FirstEntry + 1;
                startInd = endInd + numEntries;
            }
            startInd = startInd < 0 ? 0 : startInd;
            endInd = endInd > (int)preemptEntries.size()?
                              (int)preemptEntries.size() : endInd;

            // preemptEntries[0] is the oldest entry
            // preemptEntries[size-1] is the most recent entry
            // Adjust index to count from most recent to oldest
            int endInd_save = endInd;
            endInd = (int)preemptEntries.size() - startInd;
            startInd = (int)preemptEntries.size() - endInd_save;

            if (endInd > startInd)
            {
                removedEntries.insert(removedEntries.end(),
                                      preemptEntries.begin() + startInd,
                                      preemptEntries.begin() + endInd);
                GetPolicyManager()->SaveRemovedRlEntries(*ppRunlist,
                                                         removedEntries);

                preemptEntries.erase(preemptEntries.begin() + startInd,
                                     preemptEntries.begin() + endInd);
                GetPolicyManager()->AddPreemptRlEntries(*ppRunlist,
                                                        preemptEntries);
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RestoreEntriesToRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RestoreEntriesToRunlist::PmAction_RestoreEntriesToRunlist
(
    const PmRunlistDesc *pRunlistDesc,
    UINT32 insertPos,
    UINT32 firstEntry,
    int numEntries
):
    PmAction("Action.RestoreEntriesToRunlist"),
    m_pRunlistDesc(pRunlistDesc),
    m_InsertPos(insertPos),
    m_FirstEntry(firstEntry),
    m_NumEntries(numEntries)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RestoreEntriesToRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pRunlistDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Restore entries to the preempted runlist indicated by runlistDesc.
//!
/* virtual */ RC PmAction_RestoreEntriesToRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        Runlist::Entries preemptEntries =
                         GetPolicyManager()->GetPreemptRlEntries(*ppRunlist);
        Runlist::Entries removedEntries =
                         GetPolicyManager()->GetRemovedRlEntries(*ppRunlist);

        if (removedEntries.size() > 0)
        {
            // 0 means restoring all entries in removed list
            int numEntries = m_NumEntries;
            if (m_NumEntries == 0)
                numEntries = (int)removedEntries.size();

            int startInd = 0, endInd = 0; // [startInd, endInd)
            UINT32 insertPos;

            if (numEntries >= 0)
            {
                startInd = m_FirstEntry;
                endInd = startInd + numEntries;
            }
            else
            {
                endInd = m_FirstEntry + 1;
                startInd = endInd + numEntries;
            }
            startInd = startInd < 0 ? 0 : startInd;
            endInd = endInd > (int)removedEntries.size()?
                              (int)removedEntries.size() : endInd;
            insertPos = m_InsertPos > preemptEntries.size()?
                              (UINT32)preemptEntries.size() : m_InsertPos;

            // preemptEntries[0] is the oldest entry
            // preemptEntries[size-1] is the most recent entry
            // Adjust index to count from most recent to oldest
            int endInd_save = endInd;
            endInd = (int)removedEntries.size() - startInd;
            startInd = (int)removedEntries.size() - endInd_save;
            insertPos = (UINT32)preemptEntries.size() - insertPos;

            if (endInd > startInd)
            {
                preemptEntries.insert(preemptEntries.begin() + insertPos,
                                      removedEntries.begin() + startInd,
                                      removedEntries.begin() + endInd);
                GetPolicyManager()->AddPreemptRlEntries(*ppRunlist,
                                                        preemptEntries);

                removedEntries.erase(removedEntries.begin() + startInd,
                                     removedEntries.begin() + endInd);
                GetPolicyManager()->SaveRemovedRlEntries(*ppRunlist,
                                                         removedEntries);
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemoveChannelFromRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemoveChannelFromRunlist::PmAction_RemoveChannelFromRunlist
(
    const PmChannelDesc *pChannelDesc,
    UINT32 firstEntry,
    int numEntries
):
    PmAction("Action.RemoveChannelFromRunlist"),
    m_pChannelDesc(pChannelDesc),
    m_FirstEntry(firstEntry),
    m_NumEntries(numEntries)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RemoveChannelFromRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Remove the n'th to (n+numEntries)'s entry for the specified
//!        channel from preempted runlist.
//!
/* virtual */ RC PmAction_RemoveChannelFromRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        RunlistChannelWrapper *pRunlistWrapper =
            (*ppChannel)->GetModsChannel()->GetRunlistChannelWrapper();

        if (pRunlistWrapper != NULL)
        {
            Runlist *pRunlist;
            CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));

            Runlist::Entries preemptEntries =
                GetPolicyManager()->GetPreemptRlEntries(pRunlist);
            Runlist::Entries removedEntries =
                GetPolicyManager()->GetRemovedRlEntries(pRunlist);

            if (preemptEntries.size() > 0)
            {
                // 0 means all entries
                int numEntries = m_NumEntries;
                if (m_NumEntries == 0)
                    numEntries = (int)preemptEntries.size();

                //1. Find the first entry matching specified channel
                int i;
                UINT32 cn = m_FirstEntry;
                for (i = (int)preemptEntries.size() - 1; i >= 0 ; i--)
                {
                    if (preemptEntries[i].pChannel == (*ppChannel)->GetModsChannel())
                    {
                        if (cn == 0) break;
                        cn--;
                    }
                }
                if (i < 0) continue; // indicated channel not exist

                //2. Remove numEntries entries matching specified channel
                Channel *pChannel = preemptEntries[i].pChannel;
                Runlist::Entries matchingEntries;
                cn = abs(numEntries);
                while ( (i < (int)preemptEntries.size()) && (i >= 0))
                {
                    if (preemptEntries[i].pChannel == pChannel)
                    {
                        if (numEntries < 0)
                        {
                            matchingEntries.push_back(*(preemptEntries.begin()+i));
                            preemptEntries.erase(preemptEntries.begin()+i);
                        }
                        else
                        {
                            matchingEntries.push_front(*(preemptEntries.begin()+i));
                            preemptEntries.erase(preemptEntries.begin()+i);
                            i--; // move index since current one is deleted
                        }

                        if ( --cn == 0) break;
                    }
                    else
                    {
                        i = numEntries < 0? i + 1 : i - 1;
                    }
                }

                if (matchingEntries.size() > 0)
                {
                    // save removed entries at the end of removedEntries(most recent entry)
                    removedEntries.insert(removedEntries.end(),
                                          matchingEntries.begin(),
                                          matchingEntries.end());
                    GetPolicyManager()->SaveRemovedRlEntries(pRunlist,
                                                             removedEntries);

                    GetPolicyManager()->AddPreemptRlEntries(pRunlist,
                                                            preemptEntries);
                }
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RestoreChannelToRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RestoreChannelToRunlist::PmAction_RestoreChannelToRunlist
(
    const PmChannelDesc *pChannelDesc,
    UINT32 insertPos,
    UINT32 firstEntry,
    int numEntries
):
    PmAction("Action.RestoreChannelToRunlist"),
    m_pChannelDesc(pChannelDesc),
    m_InsertPos(insertPos),
    m_FirstEntry(firstEntry),
    m_NumEntries(numEntries)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RestoreChannelToRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Restore the n'th to (n+num)'th entry for the specified
//!        channel in indicated position.
//!
/* virtual */ RC PmAction_RestoreChannelToRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        RunlistChannelWrapper *pRunlistWrapper =
            (*ppChannel)->GetModsChannel()->GetRunlistChannelWrapper();

        if (pRunlistWrapper != NULL)
        {
            Runlist *pRunlist;
            CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));

            Runlist::Entries preemptEntries =
                GetPolicyManager()->GetPreemptRlEntries(pRunlist);
            Runlist::Entries removedEntries =
                GetPolicyManager()->GetRemovedRlEntries(pRunlist);

            if (removedEntries.size() > 0)
            {
                // 0 means all entries
                int numEntries = m_NumEntries;
                if (m_NumEntries == 0)
                    numEntries = (int)removedEntries.size();

                //1. Find the first entry matching specified channel
                int i;
                UINT32 cn = m_FirstEntry;
                for (i = (int)removedEntries.size() - 1; i >= 0; i--)
                {
                    if (removedEntries[i].pChannel == (*ppChannel)->GetModsChannel())
                    {
                        if (cn == 0) break;
                        cn --;
                    }
                }
                // continue if the indicated channel does not exist
                if (i < 0) continue;

                //2. Remstore numEntries entries matching specified channel
                Channel *pChannel = removedEntries[i].pChannel;
                Runlist::Entries matchingEntries;
                cn = abs(numEntries);
                while ( (i < (int)removedEntries.size()) && (i >=0))
                {
                    if (removedEntries[i].pChannel == pChannel)
                    {
                        if (numEntries < 0)
                        {
                            matchingEntries.push_back(*(removedEntries.begin()+i));
                            removedEntries.erase(removedEntries.begin()+i);
                        }
                        else
                        {
                            matchingEntries.push_front(*(removedEntries.begin()+i));
                            removedEntries.erase(removedEntries.begin()+i);
                            i--; // move index since current one is deleted
                        }

                        if ( --cn == 0) break;
                    }
                    else
                    {
                        i = numEntries < 0? i+1 : i-1;
                    }
                }

                if (matchingEntries.size() > 0)
                {
                    // Adjust index to count from most recent to oldest
                    // Insert removed entries to specified position
                    UINT32 insertPos = m_InsertPos > preemptEntries.size()?
                        0 : (UINT32)preemptEntries.size() - m_InsertPos;
                    preemptEntries.insert(preemptEntries.begin() + insertPos,
                                          matchingEntries.begin(),
                                          matchingEntries.end());
                    GetPolicyManager()->AddPreemptRlEntries(pRunlist,
                                                            preemptEntries);

                    GetPolicyManager()->SaveRemovedRlEntries(pRunlist,
                                                             removedEntries);
                }
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MoveEntryInRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MoveEntryInRunlist::PmAction_MoveEntryInRunlist
(
    const PmRunlistDesc *pRunlistDesc,
    UINT32 entryIndex,
    int relPri
):
    PmAction("Action.MoveEntryInRunlist"),
    m_pRunlistDesc(pRunlistDesc),
    m_EntryIndex(entryIndex),
    m_RelPri(relPri)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_MoveEntryInRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pRunlistDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Move entry m_EntryIndex in the runlist(s) by m_RelPri
//!
/* virtual */ RC PmAction_MoveEntryInRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        Runlist::Entries preemptEntries =
                         GetPolicyManager()->GetPreemptRlEntries(*ppRunlist);

        if (preemptEntries.size() > 0)
        {
            // 0 means the end of entry list
            int relPri = m_RelPri;
            if (m_RelPri == 0)
                relPri = (int)preemptEntries.size();

            int i = m_EntryIndex >= preemptEntries.size()?
                           0 : (int)(preemptEntries.size() - 1 - m_EntryIndex);

            Channel *pChannel = preemptEntries[i].pChannel;
            Runlist::Entries movedEntries;
            UINT32 cn = abs(relPri);
            while ( (i < (int)preemptEntries.size()) && (i >= 0))
            {
                if (preemptEntries[i].pChannel == pChannel)
                {
                    if (relPri < 0)
                    {
                        movedEntries.push_back(*(preemptEntries.begin()+i));
                        preemptEntries.erase(preemptEntries.begin()+i);
                    }
                    else
                    {
                        movedEntries.push_front(*(preemptEntries.begin()+i));
                        preemptEntries.erase(preemptEntries.begin()+i);
                        i--; // move index since current one is deleted
                    }
                }
                else
                {
                    i = relPri < 0? i + 1 : i - 1;
                    if ( --cn == 0) break;
                }
            }

            if (movedEntries.size() > 0)
            {
                if (relPri > 0) i++; //insert after i
                MASSERT((i >= 0) && (i <= (int)preemptEntries.size()));

                preemptEntries.insert(preemptEntries.begin() + i,
                                      movedEntries.begin(),
                                      movedEntries.end());
                GetPolicyManager()->AddPreemptRlEntries(*ppRunlist,
                                                        preemptEntries);
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MoveChannelInRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MoveChannelInRunlist::PmAction_MoveChannelInRunlist
(
    const PmChannelDesc *pChannelDesc,
    UINT32 entryIndex,
    int relPri
):
    PmAction("Action.MoveChannelInRunlist"),
    m_pChannelDesc(pChannelDesc),
    m_EntryIndex(entryIndex),
    m_RelPri(relPri)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_MoveChannelInRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Move n'th entry matching indicated channel by relPri.
//!
/* virtual */ RC PmAction_MoveChannelInRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        RunlistChannelWrapper *pRunlistWrapper =
            (*ppChannel)->GetModsChannel()->GetRunlistChannelWrapper();

        if (pRunlistWrapper != NULL)
        {
            Runlist *pRunlist;
            CHECK_RC(pRunlistWrapper->GetRunlist(&pRunlist));

            Runlist::Entries preemptEntries =
                GetPolicyManager()->GetPreemptRlEntries(pRunlist);

            if (preemptEntries.size() > 0)
            {
                // 0 means the end of entry list
                int relPri = m_RelPri;
                if (m_RelPri == 0)
                    relPri = (int)preemptEntries.size();

                //1. Find the entry to be moved
                int i;
                UINT32 cn = m_EntryIndex;
                for (i = (int)preemptEntries.size() - 1; i >= 0; i--)
                {
                    if (preemptEntries[i].pChannel == (*ppChannel)->GetModsChannel())
                    {
                        if (cn == 0) break;
                        cn --;
                    }
                }
                if (i < 0) continue; // indicated channel not exist

                //2. Move the entry forword or backword
                Channel *pChannel = preemptEntries[i].pChannel;
                Runlist::Entries movedEntries;
                cn = abs(relPri);
                while ( (i < (int)preemptEntries.size()) && (i >= 0))
                {
                    if (preemptEntries[i].pChannel == pChannel)
                    {
                        if (relPri < 0)
                        {
                            movedEntries.push_back(*(preemptEntries.begin()+i));
                            preemptEntries.erase(preemptEntries.begin()+i);
                        }
                        else
                        {
                            movedEntries.push_front(*(preemptEntries.begin()+i));
                            preemptEntries.erase(preemptEntries.begin()+i);
                            i--; // move index since current one is deleted
                        }
                    }
                    else
                    {
                        i = relPri < 0? i + 1 : i - 1;
                        if ( --cn == 0) break;
                    }
                }

                if (movedEntries.size() > 0)
                {
                    if (relPri > 0) i++; //insert after i
                    MASSERT((i >= 0) && (i <= (int)preemptEntries.size()));

                    preemptEntries.insert(preemptEntries.begin() + i,
                                          movedEntries.begin(),
                                          movedEntries.end());
                    GetPolicyManager()->AddPreemptRlEntries(pRunlist,
                                                            preemptEntries);
                }
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResubmitRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ResubmitRunlist::PmAction_ResubmitRunlist
(
    const PmRunlistDesc *pRunlistDesc,
    const PmSmcEngineDesc *pSmcEngineDesc
):
    PmAction("Action.ResubmitRunlist"),
    m_pRunlistDesc(pRunlistDesc),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! Check whether the trigger supports ResubmitRunlist.
//!
/* virtual */ RC PmAction_ResubmitRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pRunlistDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Resubmit the runlist to the faulting channel
//!
//!
/* virtual */ RC PmAction_ResubmitRunlist::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        Runlists runlists;
        CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
        for (Runlists::iterator ppRunlist = runlists.begin();
             ppRunlist != runlists.end(); ++ppRunlist)
        {
            Runlist::Entries preemptEntries =
                               GetPolicyManager()->GetPreemptRlEntries(*ppRunlist);
            if (preemptEntries.size() > 0)
            {
                CHECK_RC((*ppRunlist)->Resubmit(preemptEntries));
            }
            GetPolicyManager()->RemovePreemptRlEntries(*ppRunlist);
        }
    }
    else if (m_pRunlistDesc->GetChannelDesc())
    {
        vector< tuple<GpuDevice*, LwRm*, UINT32> > deviceEngines;
        CHECK_RC(m_pRunlistDesc->GetChannelsEngines(pTrigger, pEvent, &deviceEngines));
        for (auto const & deviceEngine : deviceEngines)
        {
            GpuDevice* pGpuDev;
            LwRm* pLwRm;
            UINT32 engineId;

            tie(pGpuDev, pLwRm, engineId) = deviceEngine;
            CHECK_RC(pGpuDev->StartEngineRunlist(engineId, pLwRm));
        }
    }
    else
    {
        map<LwRm*, PmSmcEngines> mapLwRmPmSmcEngines = GetLwRmSmcEngines(pTrigger, pEvent, m_pSmcEngineDesc);

        // Get Matching engines per RM client and Resubmit them
        for (auto pairLwRmPmSmcEngines : mapLwRmPmSmcEngines)
        {
            vector< pair<GpuDevice*, UINT32> > deviceEngines;
            LwRm* pLwRm = pairLwRmPmSmcEngines.first;
            PmSmcEngines lwRmSmcEngines = pairLwRmPmSmcEngines.second;
            CHECK_RC(m_pRunlistDesc->GetMatchingEngines(pTrigger, pEvent, &deviceEngines, pLwRm, lwRmSmcEngines));
            for (vector< pair<GpuDevice*, UINT32> >::iterator deviceEngine = deviceEngines.begin();
                    deviceEngine != deviceEngines.end(); ++deviceEngine)
            {
                if (lwRmSmcEngines.size())
                {
                    InfoPrintf("Resubmiting %s engine in swizid %d\n",
                               deviceEngine->first->GetEngineName(deviceEngine->second), lwRmSmcEngines[0]->GetSwizzId());
                }
                else
                {
                    InfoPrintf("Resubmiting %s engine\n", deviceEngine->first->GetEngineName(deviceEngine->second));
                }
                CHECK_RC(deviceEngine->first->StartEngineRunlist(deviceEngine->second, pLwRm));
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FreezeRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FreezeRunlist::PmAction_FreezeRunlist
(
    const PmRunlistDesc *pRunlistDesc
) :
    PmAction("Action.FreezeRunlist"),
    m_pRunlistDesc(pRunlistDesc)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_FreezeRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pRunlistDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Freeze the indicated runlists
//!
/* virtual */ RC PmAction_FreezeRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        CHECK_RC((*ppRunlist)->Freeze());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Make sure all runlists are unfrozen after the test
//!
/* virtual */ RC PmAction_FreezeRunlist::EndTest()
{
    StickyRC firstRc;

    GpuDevices gpuDevices = GetPolicyManager()->GetGpuDevices();
    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        Runlists runlists = (*ppGpuDevice)->GetAllocatedRunlists();
        for (Runlists::iterator ppRunlist = runlists.begin();
             ppRunlist != runlists.end(); ++ppRunlist)
        {
            firstRc = (*ppRunlist)->Unfreeze();
        }
    }

    return firstRc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnfreezeRunlist
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnfreezeRunlist::PmAction_UnfreezeRunlist
(
    const PmRunlistDesc *pRunlistDesc
) :
    PmAction("Action.UnfreezeRunlist"),
    m_pRunlistDesc(pRunlistDesc)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_UnfreezeRunlist::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pRunlistDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Unfreeze the indicated runlists
//!
/* virtual */ RC PmAction_UnfreezeRunlist::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        CHECK_RC((*ppRunlist)->Unfreeze());
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WriteRunlistEntries
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WriteRunlistEntries::PmAction_WriteRunlistEntries
(
    const PmRunlistDesc *pRunlistDesc,
    UINT32 NumEntries
) :
    PmAction("Action.WriteRunlistEntries"),
    m_pRunlistDesc(pRunlistDesc),
    m_NumEntries(NumEntries)
{
    MASSERT(pRunlistDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_WriteRunlistEntries::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pRunlistDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Write m_NumEntries entries to each of the indicated runlists
//!
//! This method only works on frozen runlists that have unwritten
//! entries.
//!
/* virtual */ RC PmAction_WriteRunlistEntries::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    Runlists runlists;
    RC rc;

    CHECK_RC(m_pRunlistDesc->GetMatchingRunlists(pTrigger, pEvent, &runlists));
    for (Runlists::iterator ppRunlist = runlists.begin();
         ppRunlist != runlists.end(); ++ppRunlist)
    {
        CHECK_RC((*ppRunlist)->WriteFrozenEntries(m_NumEntries));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_Flush
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_Flush::PmAction_Flush
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.Flush"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_Flush::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Flush the channel
//!
/* virtual */ RC PmAction_Flush::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        CHECK_RC((*ppChannel)->Flush());
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResetChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ResetChannel::PmAction_ResetChannel
(
    const PmChannelDesc *pChannelDesc,
    const string &EngineName
) :
    PmAction("Action.ResetChannel"),
    m_pChannelDesc(pChannelDesc),
    m_EngineName(EngineName)
{
    MASSERT(m_pChannelDesc != NULL);

    m_EngineName = PolicyManager::Instance()->ColwPm2GpuEngineName(EngineName);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ResetChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Reset the channel
//!
/* virtual */ RC PmAction_ResetChannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    // Loop through all the matching channels
    //
    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        PmChannel *pChannel = *ppChannel;
        GpuDevice *pGpuDevice = pChannel->GetGpuDevice();
        UINT32 engineToReset = LW2080_ENGINE_TYPE_NULL;

        // Starting from Ampere, channel will be storing the engineId
        // associated with it
        if (!(pGpuDevice->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID)))
        {
            // Find engine that matches m_EngineName
            //
            vector<UINT32> engines;
            CHECK_RC(pGpuDevice->GetEngines(&engines, pChannel->GetLwRmPtr()));
            for (vector<UINT32>::iterator pEngine = engines.begin();
                    pEngine != engines.end(); ++pEngine)
            {
                if (0 == Utility::strcasecmp(pGpuDevice->GetEngineName(*pEngine),
                                                m_EngineName.c_str()))
                {
                    engineToReset = *pEngine;
                    break;
                }
            }
        }
        else
        {
            engineToReset = pChannel->GetEngineId();
        }

        if (engineToReset == LW2080_ENGINE_TYPE_NULL)
        {
            ErrPrintf("Action.ResetChannel: No engine named '%s'\n", m_EngineName.c_str());
            return RC::BAD_PARAMETER;
        }

        DebugPrintf("%s: The channel (%s) with engine (%d) is being reset\n",
                    __FUNCTION__, pChannel->GetName().c_str(), engineToReset);

        // Send the appropriate RmControl call to reset the channel
        //
        CHECK_RC(pChannel->GetModsChannel()->RmcResetChannel(engineToReset));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResetEngine
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EnableEngine::PmAction_EnableEngine
(
    const PmGpuDesc *pGpuDesc,
    const string &EngineType,
    UINT32 hwinst,
    bool enable,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32>& RegIndexes,
    const string& RegSpace
) :
    PmAction_PriAccess(pGpuDesc, pSmcEngineDesc, pChannelDesc, RegName, RegIndexes, RegSpace, "Action.EnableEngine"),
    m_pGpuDesc(pGpuDesc),
    m_EngineType(EngineType),
    m_hwinst(hwinst),
    m_Enable(enable)
{
    m_EngineType = PolicyManager::Instance()->ColwPm2GpuEngineName(EngineType);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_EnableEngine::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Enable/disable the engine
//!
/* virtual */ RC PmAction_EnableEngine::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Can't find gpuSubdevice specified in Action.EnableEngine().\n");
        return RC::BAD_PARAMETER;
    }

    for (const auto& pSubdev : gpuSubdevices)
    {
        GpuSubdevice::HwDevType hwType = pSubdev->StringToHwDevType(m_EngineType);
        for (const auto& hwDevInfo : pSubdev->GetHwDevInfo())
        {
            if (hwDevInfo.bIsEngine && hwDevInfo.bResetIdValid &&
                (hwDevInfo.hwType == hwType) && (hwDevInfo.hwInstance == m_hwinst))
            {
                UINT32 enableBit = (hwDevInfo.resetId % 32);
                UINT32 andmask = 1 << enableBit;
                UINT32 val = (m_Enable ? 1 : 0) << enableBit;
                unique_ptr<PmRegister> pPmReg = GetPmRegister(pTrigger, pEvent, pSubdev);
                CHECK_RC(pPmReg->SetMasked(andmask, val));
            }
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnbindAndRebindChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnbindAndRebindChannel::PmAction_UnbindAndRebindChannel
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.UnbindAndRebindChannel"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_UnbindAndRebindChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Unbind and rebind each of the indicated channels
//!
/* virtual */ RC PmAction_UnbindAndRebindChannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        LwRm* pLwRm = (*ppChannel)->GetLwRmPtr();
        // Unbind/rebind the channel.
        //
        LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS Params = {0};
        Params.hChannel = (*ppChannel)->GetHandle();
        Params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_NOOP;
        Params.value = 0;

        CHECK_RC(pLwRm->ControlByDevice(
                     (*ppChannel)->GetGpuDevice(),
                     LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                     &Params, sizeof(Params)));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_DisableChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_DisableChannel::PmAction_DisableChannel
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.DisableChannel"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_DisableChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Disable each of the indicated channels
//!
/* virtual */ RC PmAction_DisableChannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        CHECK_RC((*ppChannel)->SetEnabled(false));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_BlockChannelFlush
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_BlockChannelFlush::PmAction_BlockChannelFlush
(
    const PmChannelDesc *pChannelDesc,
    bool  Blocked
) :
    PmAction("Action.BlockChannelFlush"),
    m_pChannelDesc(pChannelDesc),
    m_Blocked(Blocked)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check whether the action is supported
//!
/* virtual */ RC PmAction_BlockChannelFlush::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Block each of the indicated channels until user issues
// an unblock
//!
/* virtual */ RC PmAction_BlockChannelFlush::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        PmChannel *pCh = *ppChannel;
        PmChannelWrapper *pPmWrap = pCh->GetModsChannel()->GetPmChannelWrapper();
        CHECK_RC(pPmWrap->BlockFlush(m_Blocked));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ModifyPteFlagsInSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ModifyPteFlagsInSurfaces::PmAction_ModifyPteFlagsInSurfaces
(
    const string &Name,
    const PmSurfaceDesc *pSurfaceDesc,
    PolicyManager::PageSize PageSize,
    UINT32 PteFlags,
    UINT32 PteMask,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool DeferTlbIlwalidate
) :
    PmAction(Name),
    m_pSurfaceDesc(pSurfaceDesc),
    m_PageSize(PageSize),
    m_PteFlags(PteFlags),
    m_PteMask(PteMask),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(DeferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_TargetVASpace(TargetVaSpace)
{
    MASSERT(m_pSurfaceDesc != NULL);
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ModifyPteFlagsInSurfaces::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Modify the PTE flags in the indicated surface(s).
//!
/* virtual */ RC PmAction_ModifyPteFlagsInSurfaces::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        if (m_IsInband)
        {
            PmChannels channels;

            if (m_pInbandChannel)
            {
                channels =
                    m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
            }
            else
            {
                MdiagSurf *pSurface = (*ppSubsurface)->GetMdiagSurf();
                GpuDevice *pGpuDevice = pSurface->GetGpuDev();
                channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
            }

            if (channels.size() > 0)
            {
                PmChannel *pChannel = channels[0];

                CHECK_RC((*ppSubsurface)->ModifyPteFlags(m_PageSize,
                             m_PteFlags, m_PteMask, pChannel, m_TargetVASpace,
                             PolicyManager::LEVEL_PTE, m_DeferTlbIlwalidate));
            }
        }
        else
        {
            CHECK_RC((*ppSubsurface)->ModifyPteFlags(m_PageSize,
                         m_PteFlags, m_PteMask, 0, m_TargetVASpace,
                         PolicyManager::LEVEL_PTE, m_DeferTlbIlwalidate));
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ModifyMMULevelFlagsInPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ModifyMMULevelFlagsInPages::PmAction_ModifyMMULevelFlagsInPages
(
    const string &Name,
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::PageSize PageSize,
    UINT32 PteFlags,
    UINT32 PteMask,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    PolicyManager::Level LevelType,
    bool DeferTlbIlwalidate
) :
    PmAction(Name),
    m_PageDesc(pSurfaceDesc, Offset, Size),
    m_PageSize(PageSize),
    m_PteFlags(PteFlags),
    m_PteMask(PteMask),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(DeferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_TargetVASpace(TargetVaSpace),
    m_TargetMmuLevelType(LevelType)
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ModifyMMULevelFlagsInPages::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Modify the MMU Level flags in the indicated page(s).
//!
/* virtual */ RC PmAction_ModifyMMULevelFlagsInPages::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.ModifyMMULevelFlagsInPages: No matching surface ranges found\n");
        return rc;
    }

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end(); ++pRange)
    {
        if (m_IsInband)
        {
            PmChannels channels;

            if (m_pInbandChannel)
            {
                channels =
                    m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
            }
            else
            {
                MdiagSurf *pSurface = pRange->GetMdiagSurf();
                GpuDevice *pGpuDevice = pSurface->GetGpuDev();
                channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
            }

            if (channels.size() > 0)
            {
                PmChannel *pChannel = channels[0];

                CHECK_RC(pRange->ModifyPteFlags(m_PageSize, m_PteFlags,
                    m_PteMask, pChannel, m_TargetVASpace, m_TargetMmuLevelType,
                    m_DeferTlbIlwalidate));
            }
        }
        else
        {
            CHECK_RC(pRange->ModifyPteFlags(m_PageSize, m_PteFlags,
                m_PteMask, 0, m_TargetVASpace, m_TargetMmuLevelType,
                m_DeferTlbIlwalidate));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnmapSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnmapSurfaces::PmAction_UnmapSurfaces
(
    const PmSurfaceDesc *pSurfaceDesc,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyPteFlagsInSurfaces
    (
        "Action.UnmapSurfaces",
        pSurfaceDesc,
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        isInband,
        pInbandChannel,
        TargetVaSpace,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnmapPde
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnmapPde::PmAction_UnmapPde
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::Level levelType,
    bool isInband,
    PmChannelDesc * pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.UnmapPde",
        pSurfaceDesc,
        offset,
        size,
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        isInband,
        pInbandChannel,
        TargetVaSpace,
        levelType,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnmapPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnmapPages::PmAction_UnmapPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.UnmapPages",
         pSurfaceDesc,
         offset,
         size,
         PolicyManager::LWRRENT_PAGE_SIZE,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
         isInband,
         pInbandChannel,
         TargetVaSpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}
//////////////////////////////////////////////////////////////////////
// PmAction_SparsefyPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SparsifyPages::PmAction_SparsifyPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.SparsifyPages",
         pSurfaceDesc,
         offset,
         size,
         PolicyManager::LWRRENT_PAGE_SIZE,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _SPARSE, _TRUE) |
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SPARSE) |
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
         isInband,
         pInbandChannel,
         TargetVaSpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemapSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemapSurfaces::PmAction_RemapSurfaces
(
    const PmSurfaceDesc *pSurfaceDesc,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyPteFlagsInSurfaces
    (
        "Action.RemapSurfaces",
        pSurfaceDesc,
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        isInband,
        pInbandChannel,
        TargetVaSpace,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemapPde
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemapPde::PmAction_RemapPde
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::Level levelType,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.RemapPde",
        pSurfaceDesc,
        offset,
        size,
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        isInband,
        pInbandChannel,
        TargetVaSpace,
        levelType,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemapPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemapPages::PmAction_RemapPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.RemapPages",
        pSurfaceDesc,
        offset,
        size,
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        isInband,
        pInbandChannel,
        TargetVaSpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_RandomRemapIlwalidPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RandomRemapIlwalidPages::PmAction_RandomRemapIlwalidPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    const FancyPicker& percentagePicker,
    UINT32 randomSeed,
    bool deferTlbIlwalidate
) :
    PmAction("RandomRemapIlwalidPages"),
    m_pSurfaceDesc(pSurfaceDesc),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_TargetVASpace(TargetVaSpace),
    m_PercentagePicker(percentagePicker),
    m_RandomSeed(randomSeed)
{
    MASSERT(pSurfaceDesc != NULL);

    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    m_PercentagePicker.SetContext(&m_FpContext);

    m_Rand.SeedRandom(m_RandomSeed);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RandomRemapIlwalidPages::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Randomly remap a percentage of unmapped pages in specified surface.
//!
/* virtual */ RC PmAction_RandomRemapIlwalidPages::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    //
    // Map a perentage of unmapped pages
    //
    // Step1: Get the percentage number for random map
    UINT32 percentage = m_PercentagePicker.Pick();
    if (percentage == 0)
    {
        return rc;
    }
    else if (percentage > 100)
    {
        percentage = 100;
    }

    //
    // Step 2: Randomly select a list of ranges based on percentage number
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);
    PmMemRanges ranges = GetRandomMappingRanges(subsurfaces, percentage);

    //
    // Step3: Remap the randomly selected ranges
    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end(); ++pRange)
    {
        PmChannel *pChannel = NULL;
        if (m_IsInband)
        {
            PmChannels channels;

            if (m_pInbandChannel)
            {
                channels =
                    m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
            }
            else
            {
                MdiagSurf *pSurface = pRange->GetMdiagSurf();
                GpuDevice *pGpuDevice = pSurface->GetGpuDev();
                channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
            }

            if (channels.size() > 0)
                pChannel = channels[0];
            else
                return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(pRange->ModifyPteFlags(
            PolicyManager::LWRRENT_PAGE_SIZE,
            DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
            DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
            pChannel,
            m_TargetVASpace,
            PolicyManager::LEVEL_PTE,
            m_DeferTlbIlwalidate));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Select a list of ranges for random mapping
//!        (totalUnmpaedPages * percentage + 99)/100 pages will be selected;
//!        A random number between 0 and 100 is used to decide map or not
//!
PmMemRanges PmAction_RandomRemapIlwalidPages::GetRandomMappingRanges
(
    const PmSubsurfaces& subsurfaces,
    UINT32 percentage
)
{
    PmMemRanges matchingRanges;

    for (PmSubsurfaces::const_iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        PmSurface *pSurface = (*ppSubsurface)->GetSurface();
        PmMemMappings memMappings =
            pSurface->GetMemMappingsHelper()->GetMemMappings(NULL);

        //
        // Get total unmapped pages who are candidates for mapping
        UINT32 cntPagesCandidates = 0;
        UINT32 pteValidMask =
            DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID);

        for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
             ppMemMapping != memMappings.end(); ++ppMemMapping)
        {
            UINT64 pageSize = (*ppMemMapping)->GetPageSize();
            if (((*ppMemMapping)->GetPteFlags() & pteValidMask) ==
                DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE))
            {
                cntPagesCandidates += static_cast<UINT32>
                    (((*ppMemMapping)->GetSize() + pageSize - 1) / pageSize);
            }
        }

        //
        // callwlate the number of pages to be mapped
        UINT32 cntPagesToMap = (cntPagesCandidates * percentage + 99) / 100;
        if (cntPagesToMap && (cntPagesCandidates * percentage / 100 == 0))
        {
            // if it's less than 1 page, don't always map it.
            // randomly map or not
            cntPagesToMap = m_Rand.GetRandom(0, 100) <= percentage? 1 : 0;

            DebugPrintf(GetTeeCode(), "%s: Pages to be mapped = %d * %d%% less than 1. GetRandom(0, 100) <= %d? ==> %s it.\n",
                                      __FUNCTION__, cntPagesCandidates, percentage, percentage, cntPagesToMap == 1? "map":"not map");
        }
        else
        {
            DebugPrintf(GetTeeCode(), "%s: Pages to be mapped = %d * %d%% = %d\n",
                                      __FUNCTION__, cntPagesCandidates, percentage, cntPagesToMap);
        }

        if (cntPagesToMap == 0)
        {
            continue; // nothing to do with surface
        }

        for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
             ppMemMapping != memMappings.end(); ++ppMemMapping)
        {
            if (((*ppMemMapping)->GetPteFlags() & pteValidMask) ==
                DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE))
            {
                continue;
            }

            UINT64 pageSize = (*ppMemMapping)->GetPageSize();
            UINT64 pageCnt = ((*ppMemMapping)->GetSize() + pageSize - 1) / pageSize;
            for (UINT32 ii = 0; ii < pageCnt; ++ ii)
            {
                UINT32 randomNum = m_Rand.GetRandom(0, 100);

                if (randomNum <= cntPagesToMap * 100 / cntPagesCandidates)
                {
                    // map current page
                    UINT64 offset = (*ppMemMapping)->GetOffset() + pageSize * ii;
                    UINT64 size = (ii != pageCnt - 1)? pageSize :
                        (*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetSize() - offset;

                    // Joint it to previous range if they are adjacent
                    // Create a new range if not
                    PmMemRanges::reverse_iterator itLast = matchingRanges.rbegin();
                    if ((itLast != matchingRanges.rend()) &&
                        (itLast->GetOffset() + itLast->GetSize() == offset) &&
                        (itLast->GetSurface()->GetOrigMemHandle(itLast->GetOffset())
                         == (*ppMemMapping)->GetMemHandle()))
                    {
                        itLast->Set(itLast->GetSurface(), itLast->GetOffset(),
                                    itLast->GetSize() + size);
                    }
                    else
                    {
                        PmMemRange range((*ppSubsurface)->GetSurface(), offset, size);
                        matchingRanges.push_back(range);
                    }

                    cntPagesToMap --;

                    DebugPrintf(GetTeeCode(), "%s: Map surface %s - offset = 0x%llx size = 0x%llx\n",
                                              __FUNCTION__, (*ppMemMapping)->GetMdiagSurf()->GetName().c_str(), offset, size);

                    if (cntPagesToMap == 0)
                    {
                        break;
                    }
                }

                cntPagesCandidates --;
            }
        }
    }

    return matchingRanges;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ChangePageSize
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ChangePageSize::PmAction_ChangePageSize
(
    const PmSurfaceDesc *pSurfaceDesc,
    PolicyManager::PageSize targetPageSize,
    UINT64 offset,
    UINT64 size,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType targetVaSpace,
    bool deferTlbIlwalidate
):
    PmAction("Action.ChangePageSize"),
    m_PageDesc(pSurfaceDesc, offset, size),
    m_TargetPageSize(targetPageSize),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_TargetVASpace(targetVaSpace)
{
    // Only Gmmu supports Dual PageSize
    if ((m_TargetVASpace != PolicyManager::GmmuVASpace) &&
        (m_TargetVASpace != PolicyManager::DefaultVASpace))
    {
        MASSERT(!"Invalid targetVaSpace\n");
    }
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ChangePageSize::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Change page size in a specific range.
//!
/* virtual */ RC PmAction_ChangePageSize::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.ChangePageSize: No matching surface ranges found\n");
        return rc;
    }

    MdiagSurf *pSurface = ranges[0].GetMdiagSurf();
    GpuDevice *pGpuDevice = pSurface->GetGpuDev();

    //
    // Find the inband channel
    PmChannel *pInbandChannel = 0;
    if (m_IsInband)
    {
        PmChannels channels;

        if (m_pInbandChannel)
        {
            channels =
                m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
        }
        else
        {
            channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
        }

        if (channels.size() > 0)
        {
            pInbandChannel = channels[0];
        }
    }
    else
    {
        pInbandChannel = 0;
    }

    const UINT64 bigPageSizeInBytes = pGpuDevice->GetBigPageSize();
    MASSERT((bigPageSizeInBytes & (bigPageSizeInBytes - 1)) == 0);

    UINT64 targetPageSizeInBytes = 0;
    switch (m_TargetPageSize)
    {
    case PolicyManager::LWRRENT_PAGE_SIZE:
        return rc; // Nothing to do
    case PolicyManager::BIG_PAGE_SIZE:
        targetPageSizeInBytes = bigPageSizeInBytes;
        break;
    case PolicyManager::SMALL_PAGE_SIZE:
        targetPageSizeInBytes = PolicyManager::BYTES_IN_SMALL_PAGE;
        break;
    case PolicyManager::HUGE_PAGE_SIZE:
        targetPageSizeInBytes = PolicyManager::BYTES_IN_HUGE_PAGE;
        break;
    default:
        return RC::ILWALID_ARGUMENT;
    }

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end(); ++pRange)
    {
        //
        // Find pages to be changed
        //  split mappings on page boundary;
        //  no matter big->small or small->big case,
        //  split happens on big page size boundary.
        PmMemMappings memMappings;
        UINT64 alignedOffset = ALIGN_DOWN(pRange->GetOffset(), bigPageSizeInBytes);
        UINT64 alignedSize = ALIGN_UP(pRange->GetOffset() + pRange->GetSize(),
                                     bigPageSizeInBytes) - alignedOffset;
        if (alignedSize == 0)
        {
            continue;
        }

        PmMemRange alignedRange(pRange->GetSurface(), alignedOffset, alignedSize);
        memMappings = pRange->GetSurface()->GetMemMappingsHelper()->SplitMemMappings(&alignedRange);

        //
        // Change page size of those mappings to be changed
        for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
             ppMemMapping != memMappings.end(); ++ppMemMapping)
        {
            PmMemMapping *pMemMapping = *ppMemMapping;

            if (pMemMapping->GetPageSize() == targetPageSizeInBytes)
            {
                continue;
            }

            //
            // Change to target page size
            // 4 levels <==> 5 levels
            CHECK_RC(pMemMapping->ChangePageSize(targetPageSizeInBytes, pInbandChannel,
                m_DeferTlbIlwalidate));
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemapFaultingSurface
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemapFaultingSurface::PmAction_RemapFaultingSurface
(
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction("Action.RemapFaultingSurface"),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel)
{
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting surface
//!
/* virtual */ RC PmAction_RemapFaultingSurface::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support RemapFaultingSurface\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Remap the faulting surface.
//!
/* virtual */ RC PmAction_RemapFaultingSurface::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSurfaceDesc surfaceDesc("");
    surfaceDesc.SetFaulting(true);
    PmSubsurfaces subsurfaces =
        surfaceDesc.GetMatchingSubsurfaces(pTrigger, pEvent);

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        // Non-null channel pointer means use in-band (send methods).
        // Null channel pointer means out-of-band (call RM).
        PmChannel *pChannel = 0;

        if (m_IsInband)
        {
             pChannel = GetInbandChannel(m_pInbandChannel,
                 pTrigger, pEvent, *ppSubsurface);

             if (pChannel == 0)
             {
                 WarnPrintf("No channels found for Policy.SetInbandChannel. Ignoring %s\n",
                            GetName().c_str());
                 return rc;
             }
        }
        CHECK_RC((*ppSubsurface)->ModifyPteFlags(
            PolicyManager::LWRRENT_PAGE_SIZE,
            DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
            DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
            pChannel,
            PolicyManager::GmmuVASpace,
            PolicyManager::LEVEL_PTE,
            m_DeferTlbIlwalidate));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RemapFaultingPage
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RemapFaultingPage::PmAction_RemapFaultingPage
(
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction("Action.RemapFaultingPage"),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel)
{
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting page
//!
/* virtual */ RC PmAction_RemapFaultingPage::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support RemapFaultingPage\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Remap the faulting page(s).
//!
/* virtual */ RC PmAction_RemapFaultingPage::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Non-null channel pointer means use in-band (send methods).
    // Null channel pointer means out-of-band (call RM).
    PmChannel *pChannel = 0;

    if (m_IsInband)
    {
        pChannel = GetInbandChannel(m_pInbandChannel,
            pTrigger, pEvent, pEvent->GetMemRange());

        if (pChannel == 0)
        {
            WarnPrintf("No channels found for Policy.SetInbandChannel. Ignoring %s\n",
                       GetName().c_str());
            return rc;
        }
    }
    CHECK_RC(pEvent->GetMemRange()->ModifyPteFlags(
        PolicyManager::LWRRENT_PAGE_SIZE,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
        pChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        m_DeferTlbIlwalidate));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_CreateVaSpace
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_CreateVaSpace::PmAction_CreateVaSpace
(
    const string &vaSpaceName,
    const PmGpuDesc *pGpuDesc
) :
    PmAction("Action.CreateVaSpace"),
    m_VaSpaceName(vaSpaceName),
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_CreateVaSpace::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Move the indicated vaSpace(s).
//!
/* virtual */ RC PmAction_CreateVaSpace::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PolicyManager *pPolicyManager = GetPolicyManager();

    PmTest * pTest = pEvent->GetTest();
    if(pTest == NULL)
    {
        ErrPrintf("This Event doesn't support CreateVaSpace.");
        return RC::SOFTWARE_ERROR;
    }

    // Create a vaSpace with the indicated name on each
    // matching GpuDevice.
    //
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
            PmVaSpace * pVaSpace = new PmVaSpace_Mods(pPolicyManager, *ppGpuDevice,
                                        m_VaSpaceName, pTest->GetLwRmPtr());
            if(pPolicyManager->GetVaSpace(pVaSpace->GetHandle(), NULL) == NULL)
            {
                CHECK_RC(pPolicyManager->AddVaSpace(pVaSpace));
            }
            else
            {
                DebugPrintf("%s: The Vaspace %s has been registered before.\n",
                            __FUNCTION__, m_VaSpaceName.c_str());
                delete pVaSpace;
            }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_CreateSurface
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_CreateSurface::PmAction_CreateSurface
(
    const string &surfaceName,
    UINT64 size,
    const PmGpuDesc *pGpuDesc,
    bool clearFlag,
    Memory::Location location,
    bool physContig,
    UINT32 alignment,
    bool dualPageSize,
    PolicyManager::LoopBackPolicy loopback,
    Surface2D::GpuCacheMode cacheMode,
    PolicyManager::PageSize pageSize,
    Surface2D::GpuSmmuMode gpuSmmuMode,
    Surface2D::VASpace surfAllocType,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::PageSize physPageSize,
    const PmVaSpaceDesc *pVaSpaceDesc,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction("Action.CreateSurface"),
    m_SurfaceName(surfaceName),
    m_Size(size),
    m_pGpuDesc(pGpuDesc),
    m_ClearFlag(clearFlag),
    m_Location(location),
    m_PhysContig(physContig),
    m_Alignment(alignment),
    m_DualPageSize(dualPageSize),
    m_LoopBack(loopback),
    m_CacheMode(cacheMode),
    m_PageSize(pageSize),
    m_SurfaceAllocType(surfAllocType),
    m_GpuSmmuMode(gpuSmmuMode),
    m_IsInband(isInband),
    m_pInbandChannel(pInbandChannel),
    m_PhysPageSize(physPageSize),
    m_pVaSpaceDesc(pVaSpaceDesc),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(m_Size > 0);
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_CreateSurface::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Move the indicated surface(s).
//!
/* virtual */ RC PmAction_CreateSurface::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PolicyManager *pPolicyManager = GetPolicyManager();
    PmVaSpaces vaspaces;

    PmTest * pTest = pEvent->GetTest();
    PmActionUtility::MatchingVaSpace vaspaceObj(pTest, m_pVaSpaceDesc);
    CHECK_RC(vaspaceObj.GetVaSpaces(pTrigger, pEvent, &vaspaces, m_pSmcEngineDesc));

    if (m_pSmcEngineDesc && pTest)
    {
        if (!m_pSmcEngineDesc->SimpleMatch(pTest->GetSmcEngine()))
        {
            ErrPrintf("Action.CreateSurface: Policy.SetSmcEngine (Sys:%s) does not match Event's Test's SmcEngine (Sys:%d)\n",
                      m_pSmcEngineDesc->GetSysPipe().c_str(), pTest->GetSmcEngine()->GetSysPipe());
            MASSERT(0);
            return RC::SOFTWARE_ERROR;
        }
    }

    // Create a surface with the indicated name/size/location on each
    // matching GpuDevice.
    //
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        for (PmVaSpaces::iterator vaspaceIter = vaspaces.begin();
                vaspaceIter != vaspaces.end(); ++vaspaceIter)
        {
            LwRm* pLwRm = (*vaspaceIter)->GetLwRmPtr();
            ChiplibOpScope newScope("Alloc individual surface " + m_SurfaceName, NON_IRQ,
                                     ChiplibOpScope::SCOPE_COMMON, NULL);

            LwRm::Handle hVaSpace = (*vaspaceIter)->GetHandle();

            unique_ptr<MdiagSurf> pSurface2D(new MdiagSurf);
            bool bLoopback = (m_LoopBack == PolicyManager::LOOPBACK);
            bLoopback &= (Memory::Fb == m_Location);

            UINT64 PageSizeInBytes = 0;
            switch (m_PageSize)
            {
                case PolicyManager::SMALL_PAGE_SIZE:
                    PageSizeInBytes = PolicyManager::BYTES_IN_SMALL_PAGE;
                    break;
                case PolicyManager::BIG_PAGE_SIZE:
                    PageSizeInBytes = (*ppGpuDevice)->GetBigPageSize();
                    break;
                case PolicyManager::HUGE_PAGE_SIZE:
                    PageSizeInBytes = PolicyManager::BYTES_IN_HUGE_PAGE;
                    break;
                default:
                    break;  // Ignore other values
            }
            MASSERT((PageSizeInBytes % 1024) == 0);

            UINT32 PhysPageSizeInKB = 0;
            switch (m_PhysPageSize)
            {
                case PolicyManager::SMALL_PAGE_SIZE:
                    PhysPageSizeInKB= PolicyManager::BYTES_IN_SMALL_PAGE / 1024;
                    break;
                case PolicyManager::BIG_PAGE_SIZE:
                {
                    const UINT64 bigPageSizeInBytes = (*ppGpuDevice)->GetBigPageSize();
                    PhysPageSizeInKB = UNSIGNED_CAST(UINT32, bigPageSizeInBytes/1024);
                    break;
                }
                case PolicyManager::HUGE_PAGE_SIZE:
                    PhysPageSizeInKB = PolicyManager::BYTES_IN_HUGE_PAGE / 1024;
                    break;
                default:
                    break;  // Ignore other values
            }

            pSurface2D->SetName(m_SurfaceName);
            pSurface2D->SetWidth((UINT32)m_Size);
            pSurface2D->SetHeight(1);
            pSurface2D->SetColorFormat(ColorUtils::Y8);
            pSurface2D->SetLocation(m_Location);
            pSurface2D->SetPhysContig(m_PhysContig);
            if (m_Alignment)
                pSurface2D->SetAlignment(m_Alignment);
            pSurface2D->EnableDualPageSize(m_DualPageSize);
            pSurface2D->SetLoopBack(bLoopback);
            pSurface2D->SetGpuCacheMode(m_CacheMode);
            if (PageSizeInBytes)
                pSurface2D->SetPageSize(UNSIGNED_CAST(UINT32, PageSizeInBytes / 1024));
            pSurface2D->SetVASpace(m_SurfaceAllocType);
            pSurface2D->SetPhysAddrPageSize(PhysPageSizeInKB);
            pSurface2D->SetGpuVASpace(hVaSpace);

            CHECK_RC(pSurface2D->Alloc(*ppGpuDevice, pLwRm));

            // Fill the surfaces with 0, so that if the contents are moved
            // the moved contents can be CRC'd
            //
            // For inband Fill, because we are using PA, we have to create
            // mappings to get PA before sending CE fill methods. So surface will be added before
            // fill the surface.
            // For outofband Fill, create the mapping and then PmMemRange::Write() will map the
            // surface and keep the surface map until it's changed. Thus, polling thread can use the map
            // to read surface directly. It's a way to speed up the polling thread.(Bug 1796560)
            //
            //
            // Create PmSurface object outside PolicyManager::AddSurface() because the surface clear
            // need it early due to bug 1617440(more comments below)
            PmSurface *pPmSurface = new PmSurface(pPolicyManager, NULL, pSurface2D.release(), false);
            CHECK_RC(pPmSurface->AddSubsurface(0, m_Size, m_SurfaceName, "policyfile"));

            if (m_ClearFlag)
            {
                PmChannel *pInbandChannel = NULL;
                if (m_IsInband)
                {
                    PmMemRange range(pPmSurface, 0, pPmSurface->GetSize());
                    pInbandChannel = GetInbandChannel(m_pInbandChannel,
                            pTrigger, pEvent, &range);
                    for (UINT32 subdev = 0;
                            subdev < (*ppGpuDevice)->GetNumSubdevices(); subdev++)
                    {
                        CHECK_RC(pPmSurface->Fill(0, subdev, pInbandChannel));
                    }
                }
                else
                {
                    PmMemRange range(pPmSurface, 0, pPmSurface->GetSize());
                    vector<UINT08> clearValue(range.GetSize()); // 0s to fill`
                    for (UINT32 subdev = 0;
                            subdev < (*ppGpuDevice)->GetNumSubdevices(); subdev++)
                    {
                        GpuSubdevice* pGpuSubdev = (*ppGpuDevice)->GetSubdevice(subdev);
                        range.Write(&clearValue, pGpuSubdev);
                    }
                }
            }

            // See bug 1617440:
            // Fill the surface firstly is order to add the polling surface with an initialzed surface.
            // Otherwise, in emulation environment, if last surface value is same as this test and the image
            // has been reload. The polling thread will be triggered since the value is left from last test.
            // Before adding the surface to PolicyManager, this surface is invisible to polling thead(to be
            // accurate surface matcher by descriptor)
            CHECK_RC(pPolicyManager->AddSurface(NULL, pPmSurface->GetMdiagSurf(), false, &pPmSurface));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MoveSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MoveSurfaces::PmAction_MoveSurfaces
(
    const PmSurfaceDesc *pSurfaceDesc,
    Memory::Location location,
    PolicyManager::MovePolicy movePolicy,
    PolicyManager::LoopBackPolicy loopback,
    bool DeferTlbIlwalidate,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool MoveSurfInDefaultMMU,
    Surface2D::VASpace surfAllocType,
    Surface2D::GpuSmmuMode surfGpuSmmuMode,
    bool DisablePostL2Compression
) :
    PmAction("Action.MoveSurfaces"),
    m_pSurfaceDesc(pSurfaceDesc),
    m_Location(location),
    m_MovePolicy(movePolicy),
    m_LoopBack(loopback),
    m_DeferTlbIlwalidate(DeferTlbIlwalidate),
    m_IsInband(isInband),
    m_pInbandChannel(pInbandChannel),
    m_SrcSurfAllocType(surfAllocType),
    m_SrcSurfGpuSmmuMode(surfGpuSmmuMode),
    m_MoveSurfInDefaultMMU(MoveSurfInDefaultMMU),
    m_DisablePostL2Compression(DisablePostL2Compression)
{
    MASSERT(m_pSurfaceDesc != NULL);
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_MoveSurfaces::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Move the indicated surface(s).
//!
/* virtual */ RC PmAction_MoveSurfaces::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);
    PmActionUtility::L2FlushGpuDeviceSet gpuDevicesToFlush;

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
            ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        // Select target VASpace to be modified
        //
        PolicyManager::AddressSpaceType targetVASpace = PolicyManager::GmmuVASpace;

        if (m_IsInband)
        {
            PmChannel *pInbandChannel = GetInbandChannel(m_pInbandChannel,
                pTrigger, pEvent, *ppSubsurface);

            if (pInbandChannel)
            {
                CHECK_RC(PmActionUtility::MovePhysMem(
                    **ppSubsurface,
                    m_Location,
                    m_MovePolicy,
                    m_LoopBack,
                    m_DeferTlbIlwalidate,
                    pInbandChannel,
                    targetVASpace,
                    m_SrcSurfAllocType,
                    m_SrcSurfGpuSmmuMode,
                    nullptr,
                    m_DisablePostL2Compression,
                    gpuDevicesToFlush));
            }
        }
        else
        {
            CHECK_RC(PmActionUtility::MovePhysMem(
                **ppSubsurface,
                m_Location,
                m_MovePolicy,
                m_LoopBack,
                m_DeferTlbIlwalidate,
                nullptr /* no inband channel */,
                targetVASpace,
                m_SrcSurfAllocType,
                m_SrcSurfGpuSmmuMode,
                nullptr,
                m_DisablePostL2Compression,
                gpuDevicesToFlush));
        }
    }

    CHECK_RC(PmActionUtility::FlushForPostL2Compression(gpuDevicesToFlush));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MovePages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MovePages::PmAction_MovePages
(
    const string& name,
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 srcOffset,
    UINT64 size,
    Memory::Location location,
    PolicyManager::MovePolicy movePolicy,
    PolicyManager::LoopBackPolicy loopback,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool MoveSurfInDefaultMMU,
    Surface2D::VASpace surfAllocType,
    Surface2D::GpuSmmuMode surfGpuSmmuMode,
    const PmSurfaceDesc * pDestSurfaceDesc,
    UINT64 destOffset,
    bool deferTlbIlwalidate,
    bool disablePostL2Compression
) :
    PmAction(name),
    m_PageDesc(pSurfaceDesc, srcOffset, size),
    m_Location(location),
    m_MovePolicy(movePolicy),
    m_LoopBack(loopback),
    m_IsInband(isInband),
    m_pInbandChannel(pInbandChannel),
    m_SrcSurfAllocType(surfAllocType),
    m_SrcSurfGpuSmmuMode(surfGpuSmmuMode),
    m_MoveSurfInDefaultMMU(MoveSurfInDefaultMMU),
    m_DisablePostL2Compression(disablePostL2Compression)
{
    if (pDestSurfaceDesc != NULL)
        m_DestPageDesc = new PmPageDesc(pDestSurfaceDesc, destOffset, size);
    else
        m_DestPageDesc = NULL;
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_MovePages::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Move the indicated page(s).
//!
/* virtual */ RC PmAction_MovePages::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC   rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.MovePages: No matching surface ranges found\n");
        return rc;
    }

    PmActionUtility::L2FlushGpuDeviceSet gpuDevicesToFlush;

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end(); ++pRange)
    {
        // Select target VASpace to be modified
        //
        PolicyManager::AddressSpaceType targetVASpace = PolicyManager::GmmuVASpace;

        PmMemRange * pDestRange = NULL;
        PmMemRanges destRanges;
        if (m_DestPageDesc != NULL)
        {
            destRanges = m_DestPageDesc->GetMatchingRanges(pTrigger, pEvent);
            MASSERT(destRanges.size() == 1);

            pDestRange = &destRanges[0];
        }

        if (m_IsInband)
        {
            PmChannel *pInbandChannel = GetInbandChannel(m_pInbandChannel,
                    pTrigger, pEvent, &(*pRange));

            if (pInbandChannel != 0)
            {
                CHECK_RC(PmActionUtility::MovePhysMem(
                    *pRange,
                    m_Location,
                    m_MovePolicy,
                    m_LoopBack,
                    false,
                    pInbandChannel,
                    targetVASpace,
                    m_SrcSurfAllocType,
                    m_SrcSurfGpuSmmuMode,
                    pDestRange,
                    m_DisablePostL2Compression,
                    gpuDevicesToFlush));
            }
        }
        else
        {
            CHECK_RC(PmActionUtility::MovePhysMem(
                *pRange,
                m_Location,
                m_MovePolicy,
                m_LoopBack,
                false,
                nullptr /* no inband channel */,
                targetVASpace,
                m_SrcSurfAllocType,
                m_SrcSurfGpuSmmuMode,
                pDestRange,
                m_DisablePostL2Compression,
                gpuDevicesToFlush));
        }
    }

    CHECK_RC(PmActionUtility::FlushForPostL2Compression(gpuDevicesToFlush));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MoveFaultingSurface
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MoveFaultingSurface::PmAction_MoveFaultingSurface
(
    Memory::Location location,
    PolicyManager::MovePolicy movePolicy,
    PolicyManager::LoopBackPolicy loopback,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate,
    bool disablePostL2Compression
) :
    PmAction("Action.MoveFaultingSurface"),
    m_Location(location),
    m_MovePolicy(movePolicy),
    m_LoopBack(loopback),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_DisablePostL2Compression(disablePostL2Compression)
{
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting surface
//!
/* virtual */ RC PmAction_MoveFaultingSurface::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support MoveFaultingSurface\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Move the faulting surface.
//!
/* virtual */ RC PmAction_MoveFaultingSurface::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC   rc;
    PmSurfaceDesc surfaceDesc("");
    surfaceDesc.SetFaulting(true);
    PmSubsurfaces subsurfaces =
        surfaceDesc.GetMatchingSubsurfaces(pTrigger, pEvent);
    PmActionUtility::L2FlushGpuDeviceSet gpuDevicesToFlush;

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end(); ++ppSubsurface)
    {
        if (m_IsInband)
        {
            PmChannel *pInbandChannel = GetInbandChannel(m_pInbandChannel,
                pTrigger, pEvent, *ppSubsurface);

            if (pInbandChannel != 0)
            {
                CHECK_RC(PmActionUtility::MovePhysMem(
                    **ppSubsurface,
                    m_Location,
                    m_MovePolicy,
                    m_LoopBack,
                    m_DeferTlbIlwalidate,
                    pInbandChannel,
                    PolicyManager::GmmuVASpace,
                    Surface2D::DefaultVASpace,
                    Surface2D::GpuSmmuDefault,
                    nullptr,
                    m_DisablePostL2Compression,
                    gpuDevicesToFlush));
            }
        }
        else
        {
            CHECK_RC(PmActionUtility::MovePhysMem(
                **ppSubsurface,
                m_Location,
                m_MovePolicy,
                m_LoopBack,
                m_DeferTlbIlwalidate,
                nullptr /* no inband channel */,
                PolicyManager::GmmuVASpace,
                Surface2D::DefaultVASpace,
                Surface2D::GpuSmmuDefault,
                nullptr,
                m_DisablePostL2Compression,
                gpuDevicesToFlush));
        }
    }

    CHECK_RC(PmActionUtility::FlushForPostL2Compression(gpuDevicesToFlush));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MoveFaultingPage
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MoveFaultingPage::PmAction_MoveFaultingPage
(
    Memory::Location location,
    PolicyManager::MovePolicy movePolicy,
    PolicyManager::LoopBackPolicy loopback,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate,
    bool disablePostL2Compression
) :
    PmAction("Action.MoveFaultingPage"),
    m_Location(location),
    m_MovePolicy(movePolicy),
    m_LoopBack(loopback),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_DisablePostL2Compression(disablePostL2Compression)
{
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting page
//!
/* virtual */ RC PmAction_MoveFaultingPage::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support MoveFaultingPage\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Move the faulting page(s).
//!
/* virtual */ RC PmAction_MoveFaultingPage::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC   rc;
    PmActionUtility::L2FlushGpuDeviceSet gpuDevicesToFlush;

    if (m_IsInband)
    {
        PmChannel *pInbandChannel = GetInbandChannel(m_pInbandChannel,
            pTrigger, pEvent, pEvent->GetMemRange());

        if (pInbandChannel)
        {
            CHECK_RC(PmActionUtility::MovePhysMem(
                *pEvent->GetMemRange(),
                m_Location,
                m_MovePolicy,
                m_LoopBack,
                m_DeferTlbIlwalidate,
                pInbandChannel,
                PolicyManager::GmmuVASpace,
                Surface2D::DefaultVASpace,
                Surface2D::GpuSmmuDefault,
                nullptr,
                m_DisablePostL2Compression,
                gpuDevicesToFlush));
        }
    }
    else
    {
        CHECK_RC(PmActionUtility::MovePhysMem(
            *pEvent->GetMemRange(),
            m_Location,
            m_MovePolicy,
            m_LoopBack,
            m_DeferTlbIlwalidate,
            0 /* no inband channel */,
            PolicyManager::GmmuVASpace,
            Surface2D::DefaultVASpace,
            Surface2D::GpuSmmuDefault,
            nullptr,
            m_DisablePostL2Compression,
            gpuDevicesToFlush));
    }

    CHECK_RC(PmActionUtility::FlushForPostL2Compression(gpuDevicesToFlush));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_Goto
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_Goto::PmAction_Goto(const string& name) :
    PmAction(name),
    m_NextAction(1),
    m_Source(0),
    m_Target(0)
{
}

//--------------------------------------------------------------------
//! Call the subclass's DoExelwte funcion.  Then execute the "goto" if
//! the subclass sets pGoto, or continue to the next statement if not.
//!
//! Lwrrently, the "goto" is only used to emulate an "if" block, but
//! it could execute a "while" if we ever want to.
//!
RC PmAction_Goto::Execute(PmTrigger *pTrigger, const PmEvent* pEvent)
{
    bool doGoto = false;
    RC rc;

    CHECK_RC(DoExelwte(pTrigger, pEvent, &doGoto));
    DebugPrintf(GetTeeCode(), "PmAction_Goto(%s): condition %s\n",
                              GetName().c_str(), doGoto ? "false" : "true");
    m_NextAction = doGoto ? (INT32)(m_Target - m_Source) : +1;
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnTriggerCount
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnTriggerCount::PmAction_OnTriggerCount
(
    const FancyPicker& fancyPicker,
    bool               bUseSeedString,
    UINT32             randomSeed
) :
    PmAction_Goto("ActionBlock.OnTriggerCount"),
    m_FancyPicker(fancyPicker),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(randomSeed)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnTriggerCount::~PmAction_OnTriggerCount()
{
    for (PickCounterMap::iterator iter = m_Pickers.begin();
         iter != m_Pickers.end(); iter++)
    {
        delete iter->second;
    }
    m_Pickers.clear();
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnTriggerCount::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block unless the trigger-count matches.
//!
RC PmAction_OnTriggerCount::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    RC rc;

    // Get a pointer to the PmPickCounter class for this Trigger/action
    // pair.  Create one if it doesn't already exist.
    //
    PmPickCounter *pPickCounter;
    PickCounterMap::iterator iter = m_Pickers.find(pTrigger);
    if (iter != m_Pickers.end())
    {
        pPickCounter = iter->second;
    }
    else
    {
        pPickCounter = new PmPickCounter(&m_FancyPicker,
                                         m_bUseSeedString,
                                         m_RandomSeed,
                                         GetActionBlock()->GetName());
        m_Pickers[pTrigger] = pPickCounter;
    }

    // If the trigger count matches the PickCounter's trigger
    // count, then execute the conditional block and pick the next
    // trigger.  Otherwise, skip over the conditional block.
    //
    *pGoto = !pPickCounter->CheckCounter();

    pPickCounter->IncrCounter();

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnTestNum
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnTestNum::PmAction_OnTestNum
(
    const FancyPicker& fancyPicker,
    bool               bUseSeedString,
    UINT32             randomSeed
) :
    PmAction_Goto("ActionBlock.OnTestNum"),
    m_FancyPicker(fancyPicker),
    m_pPickCounter(NULL),
    m_bUseSeedString(bUseSeedString),
    m_RandomSeed(randomSeed)

{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnTestNum::~PmAction_OnTestNum()
{
    delete m_pPickCounter;
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnTestNum::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block unless the test number matches.
//!
RC PmAction_OnTestNum::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    RC rc;
    UINT32 TestNum = GetPolicyManager()->GetTestNum();

    // Create the PickCounter if it doesn't already exist.  This isn't
    // done in the constructor, because we don't have the ActionBlock
    // yet, which we use for the random-number seed.
    //
    if (!m_pPickCounter)
    {
        m_pPickCounter = new PmPickCounter(&m_FancyPicker,
                                           m_bUseSeedString,
                                           m_RandomSeed,
                                           GetActionBlock()->GetName());
    }

    // Increment the PickCounter until it matches the current test
    // number.
    //
    MASSERT(m_pPickCounter->GetCounter() <= TestNum);
    while (m_pPickCounter->GetCounter() < TestNum)
    {
        m_pPickCounter->IncrCounter();
    }

    // If the test number matches the PickCounter, then execute the
    // conditional block.  Else, goto the end.
    //
    *pGoto = !m_pPickCounter->CheckCounter();

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnTestId
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnTestId::PmAction_OnTestId
(
    UINT32 testId
) :
    PmAction_Goto("ActionBlock.OnTestId"),
    m_TestId(testId)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnTestId::~PmAction_OnTestId()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnTestId::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block unless the test number matches.
//!
RC PmAction_OnTestId::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    RC rc;

    *pGoto = !(pEvent->GetTest()->GetTestId() == m_TestId);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_Else
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_Else::PmAction_Else
(
) :
    PmAction_Goto("ActionBlock.Else")
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_Else::~PmAction_Else()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_Else::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Always skip the else block
//!
//! An else action is used to terminate a conditional block that is
//! followed by an else block.  This action should only be exelwted
//! if the condition was true, so that the else block is always skipped.
//!
RC PmAction_Else::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnSurfaceIsFaulting
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnSurfaceIsFaulting::PmAction_OnSurfaceIsFaulting
(
    const PmSurfaceDesc *pSurfaceDesc
) :
    PmAction_Goto("ActionBlock.OnSurfaceIsFaulting"),
    m_pSurfaceDesc(pSurfaceDesc)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnSurfaceIsFaulting::~PmAction_OnSurfaceIsFaulting()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnSurfaceIsFaulting::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_pSurfaceDesc->IsSupportedInAction(pTrigger));

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support OnSurfaceIsFaulting.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting page does not
//!        overlap any of the specified surfaces.
//!
RC PmAction_OnSurfaceIsFaulting::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
         ppSubsurface != subsurfaces.end();
         ++ppSubsurface)
    {
        if (pEvent->GetMemRange()->Overlaps(*ppSubsurface))
        {
            *pGoto = false;
            return OK;
        }
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnPageIsFaulting
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnPageIsFaulting::PmAction_OnPageIsFaulting
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size
) :
    PmAction_Goto("ActionBlock.OnPageIsFaulting"),
    m_PageDesc(pSurfaceDesc, offset, size)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnPageIsFaulting::~PmAction_OnPageIsFaulting()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnPageIsFaulting::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_PageDesc.IsSupportedInAction(pTrigger));

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support OnPageIsFaulting.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting page does not
//!        overlap any of the specified memory ranges.
//!
RC PmAction_OnPageIsFaulting::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.OnPageIsFaulting: No matching surface ranges found\n");
        *pGoto = true;
        return OK;
    }

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end();
         ++pRange)
    {
        if (pEvent->GetMemRange()->Overlaps(&(*pRange)))
        {
            *pGoto = false;
            return OK;
        }
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnFaultTypeMatchesFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnFaultTypeMatchesFault::PmAction_OnFaultTypeMatchesFault
(
    string FaultType
) :
    PmAction_Goto("ActionBlock.OnFaultTypeMatchesFault")
{
    ParseFaultType(FaultType);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnFaultTypeMatchesFault::~PmAction_OnFaultTypeMatchesFault()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnFaultTypeMatchesFault::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (!pTrigger->HasReplayFault())
    {
        ErrPrintf("This trigger does not support OnFaultTypeMatchesFault.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//-------------------------------------------------------------------
//! \parse the fault type
void PmAction_OnFaultTypeMatchesFault::ParseFaultType
(
    string faultType
)
{
    if      ("PDE" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_PDE;
    else if ("PDE_SIZE" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_PDE_SIZE;
    else if ("PTE" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_PTE;
    else if ("VA_LIMIT_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_VA_LIMIT_VIOLATION;
    else if ("UNBOUND_INST_BLOCK" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_UNBOUND_INST_BLOCK;
    else if ("PRIV_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_PRIV_VIOLATION;
    else if ("RO_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_RO_VIOLATION;
    else if ("WO_VIOLATION" == faultType)
    {
        ErrPrintf("Unsupported fault type in Action.OnFaultTypeMatchesFault.\n");
    }
    else if ("PITCH_MASK_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_PITCH_MASK_VIOLATION;
    else if ("WORK_CREATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_WORK_CREATION;
    else if ("UNSUPPORTED_APERTURE" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_UNSUPPORTED_APERTURE;
    else if ("COMPRESSION_FAILURE" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_COMPRESSION_FAILURE;
    else if ("UNSUPPORTED_KIND" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_UNSUPPORTED_KIND;
    else if ("REGION_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_REGION_VIOLATION;
    else if ("POISONED" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_POISONED;
    else if ("ATOMIC_VIOLATION" == faultType) m_faultType = LW_PFAULT_FAULT_TYPE_ATOMIC_VIOLATION;
    else
    {
        ErrPrintf("Unsupported fault type in Action.OnFaultTypeMatchesFault.\n");
    }
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting FaultType does not
//!        match the fault type in the fault buffer.
//!
RC PmAction_OnFaultTypeMatchesFault::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    if (pEvent->GetFaultType() == m_faultType)
    {
        *pGoto = false;
        return OK;
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnClientIDMatchesFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnClientIDMatchesFault::PmAction_OnClientIDMatchesFault
(
    UINT32 ClientID
) :
    PmAction_Goto("ActionBlock.OnClientIDMatchesFault"),
    m_clientID(ClientID)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnClientIDMatchesFault::~PmAction_OnClientIDMatchesFault()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnClientIDMatchesFault::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (!pTrigger->HasReplayFault())
    {
        ErrPrintf("This trigger does not support OnClientIDMatchesFault.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting ClientID does not
//!        match the ClientID in the fault buffer.
//!
RC PmAction_OnClientIDMatchesFault::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    if (pEvent->GetClientID() == m_clientID)
    {
        *pGoto = false;
        return OK;
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnGPCIDMatchesFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnGPCIDMatchesFault::PmAction_OnGPCIDMatchesFault
(
    UINT32 GPCID
) :
    PmAction_Goto("ActionBlock.OnGPCIDMatchesFault"),
    m_GPCID(GPCID)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnGPCIDMatchesFault::~PmAction_OnGPCIDMatchesFault()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnGPCIDMatchesFault::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (!pTrigger->HasReplayFault())
    {
        ErrPrintf("This trigger does not support OnGPCIDMatchesFault.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting GPCID does not
//!        match the GPCID in the faultbuffer.
//!
RC PmAction_OnGPCIDMatchesFault::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    if (pEvent->GetGPCID() == m_GPCID)
    {
        *pGoto = false;
        return OK;
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OlwEIDMatchesFault
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OlwEIDMatchesFault::PmAction_OlwEIDMatchesFault
(
    const string &tsgName,
    const UINT32 VEID,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction_Goto("ActionBlock.OlwEIDMatchesFault"),
    m_TsgName(tsgName),
    m_VEID(VEID),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OlwEIDMatchesFault::~PmAction_OlwEIDMatchesFault()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OlwEIDMatchesFault::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (!pTrigger->HasReplayFault())
    {
        ErrPrintf("ActionBlock.OlwEIDMatchesFault: This trigger only support replayable fault now.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting VEID does not
//!        match the VEID in the faultbuffer.
//!
RC PmAction_OlwEIDMatchesFault::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    if (pEvent->GetVEID() == m_VEID &&
        pEvent->GetTsgName() == m_TsgName)
    {
        if (m_pSmcEngineDesc)
        {
            PmSmcEngines smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
            MASSERT(smcEngines.size() <= 1);

            PmSmcEngine* pPmSmcEngine = smcEngines.size() ? smcEngines[0] : nullptr;
            PmChannels pmChannels = pEvent->GetChannels();
            if (pPmSmcEngine && !pmChannels.empty())
            {
                LwRm* pLwRm = pmChannels[0]->GetLwRmPtr();
                UINT32 grEngineId = pmChannels[0]->GetEngineId();

                if ((pPmSmcEngine->GetLwRmPtr() == pLwRm) &&
                    (MDiagUtils::GetGrEngineId(0) == grEngineId))
                {
                    *pGoto = false;
                    return OK;
                }
            }
        }
        else
        {
            *pGoto = false;
            return OK;
        }
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnNotificationFromPage
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnNotificationFromPage::PmAction_OnNotificationFromPage
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size
) :
    PmAction_Goto("ActionBlock.OnNotificationFromPage"),
    m_PageDesc(pSurfaceDesc, offset, size)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnNotificationFromPage::~PmAction_OnNotificationFromPage()
{
}

//--------------------------------------------------------------------
//! \brief Return OK
//!
/* virtual */ RC PmAction_OnNotificationFromPage::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_PageDesc.IsSupportedInAction(pTrigger));

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support OnNotificationFromPage.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Skip the conditional block if the faulting page does not
//!        overlap any of the specified memory ranges.
//!
RC PmAction_OnNotificationFromPage::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.OnNotificationFromPage: No matching surface ranges found\n");
        *pGoto = true;
        return OK;
    }

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end();
         ++pRange)
    {
        if (pEvent->GetMemRange()->Overlaps(&(*pRange)))
        {
            *pGoto = false;
            return OK;
        }
    }

    *pGoto = true;
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_AllowOverlappingTriggers
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_AllowOverlappingTriggers::PmAction_AllowOverlappingTriggers
(
    bool Allow
) :
    PmAction(""),
    m_Allow(Allow)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_AllowOverlappingTriggers::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Allow or disallow overlapping triggers during subsequent
//! actions
//!
/* virtual */ RC PmAction_AllowOverlappingTriggers::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    GetPolicyManager()->DisableMutexInActions(m_Allow);
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_NonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_NonStallInt::PmAction_NonStallInt
(
    const string &intName,
    const PmChannelDesc *pChannelDesc,
    bool enableSemaphore,
    bool enableInt
) :
    PmAction("Action.NonStallInt"),
    m_IntName(intName),
    m_pChannelDesc(pChannelDesc),
    m_EnableSemaphore(enableSemaphore),
    m_EnableInt(enableInt)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_NonStallInt::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Execute the non-stall interrupt on the specified channels.
//!
/* virtual */ RC PmAction_NonStallInt::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    PmEventManager *pEventManager = GetPolicyManager()->GetEventManager();
    RC rc;

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        PmChannel *pCh = *ppChannel;
        PmNonStallInt *pNonStallInt = pCh->GetNonStallInt(m_IntName);
        PM_BEGIN_INSERTED_METHODS(pCh);

        if (m_EnableSemaphore)
        {
            UINT64 semaphoreOffset;
            UINT64 payload;

            pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithTime);
            if (pCh->GetType() != PmChannel::PIO)
                CHECK_RC(pCh->WriteSetSubdevice(Channel::AllSubdevicesMask));
            CHECK_RC(pNonStallInt->PrepareInterruptRequest(&semaphoreOffset,
                                                           &payload));
            CHECK_RC(pCh->SetSemaphoreOffset(semaphoreOffset));
            CHECK_RC(pCh->SemaphoreRelease(payload));
        }
        if (m_EnableInt)
        {
            CHECK_RC(pCh->NonStallInterrupt());
        }

        switch (pCh->GetModsChannel()->GetClass())
        {
            case GF100_CHANNEL_GPFIFO:
                // Pre-kepler
                CHECK_RC(GetPolicyManager()->GetEventManager()->HookResmanEvent(
                             pCh->GetHandle(), LW906F_NOTIFIERS_NONSTALL,
                             PmNonStallInt::HandleResmanEvent,
                             &pNonStallInt, sizeof(pNonStallInt),
                             pCh->GetLwRmPtr()));
                break;
            default:
                // kepler on up
                for (UINT32 ii = 0;
                    ii < pCh->GetGpuDevice()->GetNumSubdevices(); ++ii)
                {
                    CHECK_RC(pEventManager->HookResmanEvent(
                        pCh->GetGpuDevice()->GetSubdevice(ii),
                        LW2080_NOTIFIERS_FIFO_EVENT_MTHD | LW01_EVENT_NONSTALL_INTR,
                        PmNonStallInt::HandleResmanEvent,
                        &pNonStallInt, sizeof(pNonStallInt),
                        pCh->GetLwRmPtr()));
                }
        }

        PM_END_INSERTED_METHODS();
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_EngineNonStallInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EngineNonStallInt::PmAction_EngineNonStallInt
(
    const string &intName,
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.EngineNonStallInt"),
    m_IntName(intName),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_EngineNonStallInt::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Execute the non-stall interrupt on the specified channels.
//!
/* virtual */ RC PmAction_EngineNonStallInt::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    PmEventManager *pEventManager = GetPolicyManager()->GetEventManager();
    RC rc;

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        PmNonStallInt *pNonStallInt = (*ppChannel)->GetNonStallInt(m_IntName);
        SemaphoreChannelWrapper *pSemWrap =
            (*ppChannel)->GetModsChannel()->GetSemaphoreChannelWrapper();
        if (pSemWrap != NULL && pSemWrap->SupportsBackend())
        {
            PmChannel *pCh = *ppChannel;
            UINT64 semaphoreOffset;
            UINT64 payload;
            UINT32 engineID = 0;

            PM_BEGIN_INSERTED_METHODS(pCh);
            if (pCh->GetType() != PmChannel::PIO)
                CHECK_RC(pCh->WriteSetSubdevice(Channel::AllSubdevicesMask));
            CHECK_RC(pNonStallInt->PrepareInterruptRequest(
                         &semaphoreOffset, &payload));

            UINT32 flags = (*ppChannel)->GetSemaphoreReleaseFlags();
            flags &= ~(Channel::FlagSemRelWithTime | Channel::FlagSemRelWithWFI);
            (*ppChannel)->SetSemaphoreReleaseFlags(flags);
            CHECK_RC(pSemWrap->ReleaseBackendSemaphoreWithTrap(
                         semaphoreOffset, payload, false, &engineID));

            for (UINT32 ii = 0;
                ii < pCh->GetGpuDevice()->GetNumSubdevices(); ++ii)
            {
                GpuSubdevice *pGpuSubDev = pCh->GetGpuDevice()->GetSubdevice(ii);
                UINT32 notifier =
                    MDiagUtils::GetRmNotifierByEngineType(engineID, pGpuSubDev, pCh->GetLwRmPtr());

                if (notifier < LW2080_NOTIFIERS_MAXCOUNT)
                {
                    CHECK_RC(pEventManager->HookResmanEvent(
                            pGpuSubDev,
                            notifier | LW01_EVENT_NONSTALL_INTR,
                            PmNonStallInt::HandleResmanEvent,
                            &pNonStallInt, sizeof(pNonStallInt),
                            pCh->GetLwRmPtr()));
                }
            }
            PM_END_INSERTED_METHODS();
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_InsertMethods
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor.
//!
PmAction_InsertMethods::PmAction_InsertMethods
(
    PmChannelDesc *pChannelDesc,
    UINT32 subch,
    UINT32 numMethods,
    const FancyPicker& methodPicker,
    const FancyPicker& dataPicker,
    UINT32 subdevMask
) :
    PmAction("Action.InsertMethods"),
    m_pChannelDesc(pChannelDesc),
    m_Subch(subch),
    m_NumMethods(numMethods),
    m_MethodPicker(methodPicker),
    m_DataPicker(dataPicker),
    m_SubdevMask(subdevMask)
{
    MASSERT(m_pChannelDesc != NULL);

    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    m_MethodPicker.SetContext(&m_FpContext);
    m_DataPicker.SetContext(&m_FpContext);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_InsertMethods::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Insert the fpicker-chosen methods on the specified channel(s)
//!
/* virtual */ RC PmAction_InsertMethods::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    RC rc;

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        UINT32 oldSubChannelNum = 0;
        
        SemaphoreChannelWrapper *pChWrap =
                (*ppChannel)->GetModsChannel()->GetSemaphoreChannelWrapper();

        if (pChWrap)
        {
            oldSubChannelNum = pChWrap->GetLwrrentSubchannel();
        }

        UINT32 subch = m_Subch;

        if (subch == LWRRENT_SUBCH)
        {
            subch = oldSubChannelNum;
        }

        PM_BEGIN_INSERTED_METHODS(*ppChannel);
        if ((*ppChannel)->GetType() != PmChannel::PIO)
            CHECK_RC((*ppChannel)->WriteSetSubdevice(m_SubdevMask));
        for (UINT32 ii = 0; ii < m_NumMethods; ++ii)
        {
            UINT32 method = m_MethodPicker.Pick();
            UINT32 data = m_DataPicker.Pick();
            ++m_FpContext.LoopNum;
            (*ppChannel)->Write(subch, method, data);
        }
        PM_END_INSERTED_METHODS();

        if (pChWrap)
        {
            pChWrap->SetLwrrentSubchannel(oldSubChannelNum);
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_MethodInt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_MethodInt::PmAction_MethodInt
(
    bool bWfiOnRelease,
    bool bWaitEventHandled
) :
    PmAction(""),
    m_WfiOnRelease(bWfiOnRelease),
    m_WaitEventHandled(bWaitEventHandled)
{
}

//--------------------------------------------------------------------
//! Check whether the trigger supports MethodInt
//!
/* virtual */ RC PmAction_MethodInt::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    // In order to insert a method interrupt, we must have a channel to
    // insert it on.
    //
    if (!pTrigger->HasChannel() ||
        !pTrigger->HasMethodCount())
    {
        ErrPrintf("This trigger does not support MethodInt\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Execute the method interrupt on the specified channels.
//!
/* virtual */ RC PmAction_MethodInt::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    MASSERT(pTrigger);
    MASSERT(pEvent);

    PmChannels channels = pEvent->GetChannels();
    MASSERT(channels.size() == 1);
    UINT32 method = pEvent->GetMethodCount();
    RC rc;

    PmMethodInt *pMethodInt = channels[0]->GetMethodInt();
    CHECK_RC(pMethodInt->RequestInterrupt(method,
                                          m_WfiOnRelease,
                                          m_WaitEventHandled));

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_TlbIlwalidate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_TlbIlwalidate::PmAction_TlbIlwalidate
(
    const string & name,
    const PmGpuDesc *pGpuDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    bool IlwalidateGpc,
    PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
    bool IlwalidateSysmembar,
    PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
    bool Inband,
    PmChannelDesc *pInbandChannel,
    UINT32 subdevMask,
    PolicyManager::AddressSpaceType TargetVASpace,
    FancyPicker GPCID,
    FancyPicker ClientUnitID,
    PmChannelDesc * pPdbTargetChannel,
    bool TlbIlwalidateBar1,
    bool TlbIlwalidateBar2,
    PolicyManager::Level TlbIlwalidateLevelNum,
    PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateIlwalScope
) :
    PmAction(name),
    m_pGpuDesc(pGpuDesc),
    m_pSurfaceDesc(pSurfaceDesc),
    m_IlwalidateGpc(IlwalidateGpc),
    m_IlwalidateReplay(IlwalidateReplay),
    m_IlwalidateSysmembar(IlwalidateSysmembar),
    m_IlwalidateAckType(IlwalidateAckType),
    m_Inband(Inband),
    m_pInbandChannel(pInbandChannel),
    m_SubdevMask(subdevMask),
    m_TargetVASpace(TargetVASpace),
    m_GPCID(GPCID),
    m_ClientUnitID(ClientUnitID),
    m_pPdbTargetChannel(pPdbTargetChannel),
    m_TlbIlwalidateBar1(TlbIlwalidateBar1),
    m_TlbIlwalidateBar2(TlbIlwalidateBar2),
    m_TlbIlwalidateLevelNum(TlbIlwalidateLevelNum),
    m_TlbIlwalidateIlwalScope(TlbIlwalidateIlwalScope),
    m_FaultingGpcId(~0x0),
    m_FaultingClientId(~0x0)
{
    m_GPCID.SetContext(&m_GPCFpContext);
    m_ClientUnitID.SetContext(&m_UnitFpContext);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_TlbIlwalidate::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    bool isSupported = true;

    if (isSupported)
    {
        if (m_pGpuDesc)
        {
            return m_pGpuDesc->IsSupportedInAction(pTrigger);
        }
        else if (m_pSurfaceDesc)
        {
            return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
        }
        else
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    else
        return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
//! \brief Ilwalidate the TLB entries
//!
/* virtual */ RC PmAction_TlbIlwalidate::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    if (m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED_FAULTING)
    {
        SetFaultingGpcId(pTrigger, pEvent);
        SetFaultingClientId(pTrigger, pEvent);
    }

    if (m_Inband)
        return ExelwteInband(pTrigger, pEvent);
    else 
        return ExelwteOutOfBand(pTrigger, pEvent);
}

//--------------------------------------------------------------------
//! This method is called by Execute() to do the work when m_Inband is
//! true.
//!
/* virtual */ RC PmAction_TlbIlwalidate::ExelwteInband
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

PmChannel * PmAction_TlbIlwalidate::GetIlwalidateChannel
(
    PmTrigger * pTrigger,
    const PmEvent * pEvent
)
{
    if (m_pPdbTargetChannel)
    {
        PmChannels channels = m_pPdbTargetChannel->GetMatchingChannels(pTrigger, pEvent);
        PmChannel * pChannel = channels[0];
        return pChannel;
    }
    else
    {
        return NULL;
    }
}

RC PmAction_TlbIlwalidate::SetInbandGPC
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD
 )
{
    RC rc;
    if (m_IlwalidateGpc)
    {
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_GPC, _ENABLE,
                              *memOpC);
    }
    else
    {
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_GPC, _DISABLE,
                              *memOpC);
    }

    return OK;
}

RC PmAction_TlbIlwalidate::SetInbandSysmemBar
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD
 )
{
    RC rc;

    if (m_IlwalidateSysmembar)
    {
        *memOpA = FLD_SET_DRF(C06F, _MEM_OP_A, _TLB_ILWALIDATE_SYSMEMBAR,
            _EN, *memOpA);
    }
    else
    {
        *memOpA = FLD_SET_DRF(C06F, _MEM_OP_A, _TLB_ILWALIDATE_SYSMEMBAR,
            _DIS, *memOpA);
    }

    return OK;
}

RC PmAction_TlbIlwalidate::SetInbandAckType
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD
 )
{
    RC rc;
    switch (m_IlwalidateAckType)
    {
    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_NONE:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_ACK_TYPE,
            _NONE, *memOpC);
        break;

    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_GLOBALLY:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_ACK_TYPE,
            _GLOBALLY, *memOpC);
        break;

    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_INTRANODE:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_ACK_TYPE,
            _INTRANODE, *memOpC);
        break;

    default:
        ErrPrintf("Action.TlbIlwalidate: Invalid ack type\n");
        return RC::BAD_PARAMETER;
    }

    return OK;
}

RC PmAction_TlbIlwalidate::GetPdbConfigure
(
    GpuSubdevice * pGpuSubDevice,
    PmChannel * pPdbTargetChannel,
    UINT64 * pdbPhyAddrAlign,
    PmSubsurface * pSurface,
    UINT32 * pdbAperture,
    LwRm* pLwRm
)
{

    RC rc;
    LwRm::Handle hVASpace;

    // if the following setting Policy_SetTlbIlwalidateChannelPdb(),
    // Policy_SetTlbIlwalidateBar1() or Policy_SetTlbIlwalidteBar2() hasn't been set,
    // the all pdb will be ilwalidate.
    if (pPdbTargetChannel != NULL)
    {
        hVASpace = pPdbTargetChannel->GetVaSpaceHandle();

        // It must be combined with followings when using the TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL.
        // 1. Make sure the vas which channel in is same as the surface.
        // 2. Policy.SetTlbIlwalidateChannelPdb(<Channel>);
        // 3. Action.TlbIlwalidateVA(Surface.Name(<name>), offset).
        if (PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL == m_IlwalidateReplay)
        {
            MASSERT(pSurface);

            if(hVASpace != pSurface->GetMdiagSurf()->GetGpuVASpace())
            {
                ErrPrintf("Action.TlbIlwalidate: The ilwalidated channel which vaspace is not same as the TlbIlwalidateVa surface's "
                          "vaspace at the TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL case. Please check the policy file configure.\n");
                return RC::SOFTWARE_ERROR;
            }
            if (pLwRm != pSurface->GetMdiagSurf()->GetLwRmPtr())
            {
                ErrPrintf("Action.TlbIlwalidate: The ilwalidated channel's RM client is not same as the TlbIlwalidateVa surface's RM "
                          "client at the TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL case. Please check the policy file configure.\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    else if (m_TlbIlwalidateBar1)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
            LW_VASPACE_ALLOCATION_INDEX_GPU_HOST,
            pLwRm->GetSubdeviceHandle(pGpuSubDevice),
            &hVASpace,
            LwRmPtr().Get())); // Since BAR1 is not partitioned, default RM client can be used
    }
    else if (m_TlbIlwalidateBar2)
    {
        //need rm support
        MASSERT(!"NOT Supported yet!");
        return RC::UNSUPPORTED_FUNCTION;
    }
    else if (pSurface)
    {
        hVASpace = pSurface->GetMdiagSurf()->GetGpuVASpace();
    }
    else
    {
        *pdbAperture = GMMU_APERTURE_ILWALID;
        *pdbPhyAddrAlign = ~0;
        WarnPrintf(GetTeeCode(), "Ilwalidate all pdb.\n");
        return rc;
    }

    if (hVASpace == 0)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
                    LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE,
                    pLwRm->GetDeviceHandle(pGpuSubDevice->GetParentDevice()),
                    &hVASpace,
                    pLwRm));
    }

    PolicyManager *pPolicyManager = GetPolicyManager();
    GpuDevice* gpuDevice = nullptr;
    if (pPolicyManager->GetGpuDevices().size() != 0)
    {
        gpuDevice = pPolicyManager->GetGpuDevices()[0];
    }

    CHECK_RC(VaSpaceUtils::GetVASpacePdbAddress(hVASpace,
         pLwRm->GetSubdeviceHandle(pGpuSubDevice), pdbPhyAddrAlign, pdbAperture, pLwRm, gpuDevice));

    *pdbPhyAddrAlign = (*pdbPhyAddrAlign) >> LW_PFB_PRI_MMU_ILWALIDATE_PDB_ADDR_ALIGNMENT; // 4K align

    return rc;
}

RC PmAction_TlbIlwalidate::SetInbandPdb
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD,
     GpuSubdevice * pGpuSubDevice,
     PmChannel * pPdbTargetChannel,
     PmSubsurface * pSurface,
     LwRm* pLwRm
)
{
    RC rc;
    // set pdb
    UINT64 pdbPhyAddrAlign = ~0;
    UINT32 pdbAperture = GMMU_APERTURE_ILWALID;

    CHECK_RC(GetPdbConfigure(pGpuSubDevice, pPdbTargetChannel, &pdbPhyAddrAlign, pSurface, &pdbAperture, pLwRm));

    if (pdbPhyAddrAlign != ~0uLL && pdbAperture != static_cast<UINT32>(GMMU_APERTURE_ILWALID))
    {
        UINT32 lowOffset = DRF_EXTENT(LWC06F_MEM_OP_C_TLB_ILWALIDATE_PDB_ADDR_LO) -
            DRF_BASE(LWC06F_MEM_OP_C_TLB_ILWALIDATE_PDB_ADDR_LO) + 1;

        // set pdb address
        *memOpC = FLD_SET_DRF_NUM(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PDB_ADDR_LO,
            static_cast<UINT32>(pdbPhyAddrAlign), *memOpC);
        *memOpD = FLD_SET_DRF_NUM(C06F, _MEM_OP_D, _TLB_ILWALIDATE_PDB_ADDR_HI,
            static_cast<UINT32>(pdbPhyAddrAlign >> lowOffset), *memOpD);

        // set pdb aperture
        switch(pdbAperture)
        {
        case GMMU_APERTURE_VIDEO:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_PDB_APERTURE, _VID_MEM,
                                  *memOpC);
            break;
        case GMMU_APERTURE_SYS_COH:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_PDB_APERTURE,
                                  _SYS_MEM_COHERENT, *memOpC);
            break;
        case GMMU_APERTURE_SYS_NONCOH:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_PDB_APERTURE,
                                  _SYS_MEM_NONCOHERENT, *memOpC);
            break;
        default:
            // invalid = 0
            // peer = 2
            ErrPrintf("Action.TargetedTlbIlwalidate: Invalid pdb aperture type.\n");
            return RC::BAD_PARAMETER;
        }

        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PDB, _ONE,
                              *memOpC);
    }
    else
    {
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PDB, _ALL,
                              *memOpC);
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetInbandReplay
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD
 )
{
    RC rc;

    switch (m_IlwalidateReplay)
    {
    case PolicyManager::TLB_ILWALIDATE_REPLAY_NONE:
        //
        // Used when class is MAXWELL_CHANNEL_GPFIFO_A.  This is safe since
        // _B is a superset of _A so this bit is unused.
        //
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY, \
            _NONE, *memOpC);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_START:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY,
            _START, *memOpC);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED:
    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED_FAULTING:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY,
            _CANCEL_TARGETED, *memOpC);
        break;
    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_GLOBAL:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY,
            _CANCEL_GLOBAL, *memOpC);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_START_ACK_ALL:
        *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY,
            _START_ACK_ALL, *memOpC);
        break;
    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL:
        break;
    default:
        ErrPrintf("Action.TlbIlwalidate: Invalid replay type.\n");
        return  RC::BAD_PARAMETER;
    }

    // set targeted GPC
    if (m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED)
    {
        UINT32 gpcID = m_GPCID.Pick();
        UINT32 clientUnitID = m_ClientUnitID.Pick();
        *memOpA = FLD_SET_DRF_NUM(C06F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_TARGET_GPC_ID,
                                  gpcID, *memOpA);
        *memOpA = FLD_SET_DRF_NUM(C06F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_TARGET_CLIENT_UNIT_ID,
                                  clientUnitID, *memOpA);
    }

    if (m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED_FAULTING)
    {
        *memOpA = FLD_SET_DRF_NUM(C06F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_TARGET_GPC_ID,
                                  m_FaultingGpcId, *memOpA);
        *memOpA = FLD_SET_DRF_NUM(C06F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_TARGET_CLIENT_UNIT_ID,
                                  m_FaultingClientId, *memOpA);
    }

    return OK;
}

void PmAction_TlbIlwalidate::SetFaultingGpcId
(
    PmTrigger * pTrigger,
    const PmEvent * pEvent
)
{
    if (pEvent->GetType() == PmEvent::REPLAYABLE_FAULT)
    {
        m_FaultingGpcId = pEvent->GetGPCID();
    }
}

void PmAction_TlbIlwalidate::SetFaultingClientId
(
    PmTrigger * pTrigger,
    const PmEvent * pEvent
)
{
    if (pEvent->GetType() == PmEvent::REPLAYABLE_FAULT)
    {
        m_FaultingClientId = pEvent->GetClientID();
    }
}

RC PmAction_TlbIlwalidate::CheckConfigureLevel
(
    GpuSubdevice * pGpuSubDevice,
    bool * isValid,
    LwRm* pLwRm
) const
{
    RC rc;
    LwRm::Handle hVASpace;

    if (m_TlbIlwalidateLevelNum != PolicyManager::LEVEL_MAX)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
                    LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE,
                    pLwRm->GetDeviceHandle(pGpuSubDevice->GetParentDevice()),
                    &hVASpace,
                    pLwRm));
        // Get top level mmu format
        //
        const struct GMMU_FMT * pGmmuFmt = NULL;
        CHECK_RC(VaSpaceUtils::GetVASpaceGmmuFmt(hVASpace,
                    pLwRm->GetSubdeviceHandle(pGpuSubDevice),
                    &pGmmuFmt,
                    pLwRm));

        if (NULL == pGmmuFmt)
        {
            WarnPrintf("Action.TlbIlwalidate: vmm is not enabled! RM API GET_PAGE_LEVEL_INFO can't work! Trying old API\n");
            return RC::SOFTWARE_ERROR;
        }
        INT32 maxLevel = 0;;
        switch (pGmmuFmt->version)
        {
            case GMMU_FMT_VERSION_1:
                maxLevel = 2;
                break;
            case GMMU_FMT_VERSION_2:
                maxLevel = 5;
                break;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            case GMMU_FMT_VERSION_3:
                maxLevel = 6;
                break;
#endif
            default:
                ErrPrintf("Action.TlbIlwalidate: Unsupported GMMU format version %d! Trying old API\n",
                          pGmmuFmt->version);
                return RC::SOFTWARE_ERROR;
                break;
        }

        if (m_TlbIlwalidateLevelNum > maxLevel)
        {
            WarnPrintf("Action.TlbIlwalidate: Invalid level number %d, since the max level RM provide is %d.\n",
                       m_TlbIlwalidateLevelNum, maxLevel);
            return RC::BAD_PARAMETER;
        }

        *isValid = true;
    }
    else
    {
        *isValid = false;
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetInbandIlwalScope
(
    UINT32 * memOpA,
    UINT32 * memOpB,
    UINT32 * memOpC,
    UINT32 * memOpD
)
{
    RC rc;

    if (m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_NONE)
    {
        switch (m_TlbIlwalidateIlwalScope)
        {
        case PolicyManager::TlbIlwalidateIlwalScope::ALL_TLBS:
            *memOpA = FLD_SET_DRF(C56F, _MEM_OP_A, _TLB_ILWALIDATE_ILWAL_SCOPE,
                _ALL_TLBS, *memOpA);
            break;
        case PolicyManager::TlbIlwalidateIlwalScope::LINK_TLBS:
            *memOpA = FLD_SET_DRF(C56F, _MEM_OP_A, _TLB_ILWALIDATE_ILWAL_SCOPE,
                _LINK_TLBS, *memOpA);
            break;
        case PolicyManager::TlbIlwalidateIlwalScope::NON_LINK_TLBS:
            *memOpA = FLD_SET_DRF(C56F, _MEM_OP_A, _TLB_ILWALIDATE_ILWAL_SCOPE,
                _NON_LINK_TLBS, *memOpA);
            break;
        default:
            ErrPrintf("Action.TlbIlwalidate: Invalid IlwalScope.\n");
            return RC::BAD_PARAMETER;
        }
    }

    return OK;
}

RC PmAction_TlbIlwalidate::SetInbandLevel
(
    UINT32 * memOpA,
    UINT32 * memOpB,
    UINT32 * memOpC,
    UINT32 * memOpD,
    GpuSubdevice * pGpuSubDevice,
    LwRm* pLwRm
)
{
    RC rc;
    bool isValid = false;

    CHECK_RC(CheckConfigureLevel(pGpuSubDevice, &isValid, pLwRm));

    if (isValid)
    {
        switch(m_TlbIlwalidateLevelNum)
        {
        case PolicyManager::LEVEL_ALL:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _ALL, *memOpC);
            break;
        case PolicyManager::LEVEL_PTE:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _PTE_ONLY, *memOpC);
            break;
        case PolicyManager::LEVEL_PDE0:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _UP_TO_PDE0, *memOpC);
            break;
        case PolicyManager::LEVEL_PDE1:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _UP_TO_PDE1, *memOpC);
            break;
        case PolicyManager::LEVEL_PDE2:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _UP_TO_PDE2, *memOpC);
            break;
        case PolicyManager::LEVEL_PDE3:
            *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PAGE_TABLE_LEVEL,
                _UP_TO_PDE3, *memOpC);
            break;
        case PolicyManager::LEVEL_MAX:
            ErrPrintf("Action.TlbIlwalidate: Invalid level number.\n");
            return RC::BAD_PARAMETER;
        }
    }

    return OK;
}

/*virtual*/ RC PmAction_TlbIlwalidate::SetOperation
(
     UINT32 * memOpC,
     UINT32 * memOpD
 )
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
//! This method is called by Execute() to do the work when m_Inband is
//! false.
//!
/*virtual*/RC PmAction_TlbIlwalidate::ExelwteOutOfBand
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC PmAction_TlbIlwalidate::RealExelwteOutOfBand
(
    GpuSubdevice * pGpuSubDevice,
    PmChannel * pPdbTargetChanne,
    PmSubsurface * pSubsurface
)
{
    RC rc;

    CHECK_RC(POLLWRAP_HW(&CallbackPollFunction, pGpuSubDevice,
        PolicyManager::PollWaitTimeLimit));

    UINT32 value = 0;

    LwRm* pLwRm = nullptr;

    if (pPdbTargetChanne)
        pLwRm = pPdbTargetChanne->GetLwRmPtr();
    else if (pSubsurface)
        pLwRm = pSubsurface->GetSurface()->GetLwRmPtr();
    else
        pLwRm = LwRmPtr().Get();

    CHECK_RC(SetOutOfBandVA(&value, pSubsurface, pGpuSubDevice));
    CHECK_RC(SetOutOfBandPdb(&value, pGpuSubDevice, pPdbTargetChanne, pSubsurface, pLwRm));
    CHECK_RC(SetOutOfBandSysmembar(&value, pGpuSubDevice));
    CHECK_RC(SetOutOfBandReplay(&value, pGpuSubDevice, pLwRm));
    CHECK_RC(SetOutOfBandGPC(&value));
    CHECK_RC(SetOutOfBandAckType(&value));
    CHECK_RC(SetOutOfBandLevel(pGpuSubDevice, &value, pLwRm));
    CHECK_RC(SetOutOfBandLinkTlb(pGpuSubDevice, &value));

    SetOutOfBandTrigger(&value);

    DebugPrintf(GetTeeCode(), "Register value which is set in PmAction_TlbIlwalidate().\n"
                              "Register: LW_PFB_PRI_MMU_ILWALIDATE value: 0x%x\n", value);

    // write register LW_PFB_PRI_MMU_ILWALIDATE
    pGpuSubDevice->Regs().Write32(MODS_PFB_PRI_MMU_ILWALIDATE, value);

    // Add this polling to ensure the tlbilwalidate has been finished
    CHECK_RC(POLLWRAP_HW(&CallbackPollFifoEmptyFunction, pGpuSubDevice,
        PolicyManager::PollWaitTimeLimit));

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandVA
(
    UINT32 * value,
    PmSubsurface * pSurface,
    GpuSubdevice * pGpuSubdevice
)
{
    const RegHal &regs = pGpuSubdevice->Regs();

    if (pSurface != NULL)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ALL_VA_TRUE);
    return OK;
}

RC PmAction_TlbIlwalidate::SetOutOfBandLevel
(
    GpuSubdevice * pGpuSubDevice,
    UINT32 * value,
    LwRm* pLwRm
)
{
    RC rc;
    bool isValid = false;
    const RegHal &regs = pGpuSubDevice->Regs();

    CHECK_RC(CheckConfigureLevel(pGpuSubDevice, &isValid, pLwRm));

    if (isValid)
    {
        switch(m_TlbIlwalidateLevelNum)
        {
        case PolicyManager::LEVEL_ALL:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_ALL);
            break;
        case PolicyManager::LEVEL_PTE:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_PTE_ONLY);
            break;
        case PolicyManager::LEVEL_PDE0:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_UP_TO_PDE0);
            break;
        case PolicyManager::LEVEL_PDE1:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_UP_TO_PDE1);
            break;
        case PolicyManager::LEVEL_PDE2:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_UP_TO_PDE2);
            break;
        case PolicyManager::LEVEL_PDE3:
            regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_CACHE_LEVEL_UP_TO_PDE3);
            break;
        case PolicyManager::LEVEL_MAX:
            ErrPrintf("Action.TlbIlwalidate: Invalid level number.\n");
            return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandLinkTlb
(
    GpuSubdevice * pGpuSubDevice,
    UINT32 * value
)
{
    RC rc;
    const RegHal &regs = pGpuSubDevice->Regs();

    if (m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_NONE)
    {
        switch (m_TlbIlwalidateIlwalScope)
        {
        case PolicyManager::TlbIlwalidateIlwalScope::ALL_TLBS:
            if (regs.IsSupported(MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_ALL_TLBS))
            {
                regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_ALL_TLBS);
            }
            break;
        case PolicyManager::TlbIlwalidateIlwalScope::LINK_TLBS:
            if (regs.IsSupported(MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_LINK_TLBS))
            {
                regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_LINK_TLBS);
            }
            break;
        case PolicyManager::TlbIlwalidateIlwalScope::NON_LINK_TLBS:
            if (regs.IsSupported(MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_NON_LINK_TLBS))
            {
                regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ILWAL_SCOPE_NON_LINK_TLBS);
            }
            break;
        default:
            ErrPrintf("Action.TlbIlwalidate: Invalid IlwalScope.\n");
            return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandPdb
(
    UINT32 * valueCommand,
    GpuSubdevice * pGpuSubDevice,
    PmChannel * pPdbTargetChannel,
    PmSubsurface * pSurface,
    LwRm* pLwRm
)
{
    RC rc;
    RegHal &regs = pGpuSubDevice->Regs();

    UINT32 value = 0;

    UINT64 pdbPhyAddrAlign = ~0;
    UINT32 pdbAperture = GMMU_APERTURE_ILWALID;

    CHECK_RC(GetPdbConfigure(pGpuSubDevice, pPdbTargetChannel, &pdbPhyAddrAlign, pSurface, &pdbAperture, pLwRm));

    if (pdbPhyAddrAlign != ~0uLL && pdbAperture != static_cast<UINT32>(0))
    {
        // set pdb aperture
        switch(pdbAperture)
        {
        case GMMU_APERTURE_VIDEO:
            regs.SetField(&value, MODS_PFB_PRI_MMU_ILWALIDATE_PDB_APERTURE_VID_MEM);
            break;
        case GMMU_APERTURE_SYS_COH:
        case GMMU_APERTURE_SYS_NONCOH:
            regs.SetField(&value, MODS_PFB_PRI_MMU_ILWALIDATE_PDB_APERTURE_SYS_MEM);
            break;
        default:
            // invalid = 0
            // peer = 2
            ErrPrintf("Action.TlbIlwalidate: Invalid pdb aperture type.\n");
            return RC::BAD_PARAMETER;
        }

        // set pdb low address
        regs.SetField(&value, MODS_PFB_PRI_MMU_ILWALIDATE_PDB_ADDR, pdbPhyAddrAlign);

        DebugPrintf(GetTeeCode(), "Register value which is set in PmAction_TlbIlwalidate().\n"
                                  "Register: LW_PFB_PRI_MMU_ILWALIDATE_PDB value: 0x%08x\n", value);

        // write register LW_PFB_PRI_MMU_ILWALIDATE_PDB
        regs.Write32(MODS_PFB_PRI_MMU_ILWALIDATE_PDB, value);

        value = 0;
        UINT32 lowOffset = DRF_SIZE(LWC06F_MEM_OP_C_TLB_ILWALIDATE_PDB_ADDR_LO);
        // set pdb high address
        regs.SetField(&value, MODS_PFB_PRI_MMU_ILWALIDATE_UPPER_PDB_ADDR,
                      pdbPhyAddrAlign >> lowOffset);

        DebugPrintf(GetTeeCode(), "Register value which is set in PmAction_TlbIlwalidate().\n"
                                  "Register: LW_PFB_PRI_MMU_ILWALIDATE_UPPER_PDB value: 0x%08x\n", value);

        // write register LW_PFB_PRI_MMU_ILWALIDATE_UPPER_PDB
        regs.Write32(MODS_PFB_PRI_MMU_ILWALIDATE_UPPER_PDB, value);

        regs.SetField(valueCommand, MODS_PFB_PRI_MMU_ILWALIDATE_ALL_PDB_FALSE);
    }
    else
    {
        regs.SetField(valueCommand, MODS_PFB_PRI_MMU_ILWALIDATE_ALL_PDB_TRUE);
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandSysmembar
(
    UINT32 * value,
    GpuSubdevice * pGpuSubDevice
)
{
    const RegHal &regs = pGpuSubDevice->Regs();

    // bar
    if (m_IlwalidateSysmembar)
    {
        regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_SYS_MEMBAR_TRUE);
    }
    else
    {
        regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_SYS_MEMBAR_FALSE);
    }

    return OK;
}

RC PmAction_TlbIlwalidate::SetOutOfBandReplay
(
    UINT32 * value,
    GpuSubdevice * pGpuSubDevice,
    LwRm* pLwRm
)
{
    RC rc;

    // replay
    switch (m_IlwalidateReplay)
    {
    case PolicyManager::TLB_ILWALIDATE_REPLAY_NONE:
        //
        // Used when class is MAXWELL_CHANNEL_GPFIFO_A.  This is safe since
        // _B is a superset of _A so this bit is unused.
        //
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _NONE, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_START:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _START, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_GLOBAL:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _CANCEL_GLOBAL, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _CANCEL_TARGETED, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_START_ACK_ALL:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _START_ACK_ALL, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL:
        break;

    default:
        ErrPrintf("Action.TlbIlwalidate: Invalid replay type.\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandGPC
(
    UINT32 * value
)
{
    if (m_IlwalidateGpc)
    {
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_HUBTLB_ONLY, _FALSE, *value);
    }
    else
    {
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_HUBTLB_ONLY, _TRUE, *value);
    }

    return OK;
}

RC PmAction_TlbIlwalidate::SetOutOfBandAckType
(
    UINT32 * value
)
{
    RC rc;

    switch (m_IlwalidateAckType)
    {
    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_NONE:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_ACK, _NONE_REQUIRED, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_GLOBALLY:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_ACK, _GLOBALLY, *value);
        break;

    case PolicyManager::TLB_ILWALIDATE_ACK_TYPE_INTRANODE:
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_ACK, _INTRANODE, *value);
        break;

    default:
        ErrPrintf("Action.TlbIlwalidate: Invalid ack type.\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

RC PmAction_TlbIlwalidate::SetOutOfBandTrigger
(
    UINT32 * value
)
{
    // trigger the tlb ilwalidate
    *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_TRIGGER, _TRUE, *value);

    return OK;
}

/*static*/ bool PmAction_TlbIlwalidate::CallbackPollFifoEmptyFunction
(
    void * pvArgs
)
{
    GpuSubdevice * pGpuSubdevice = static_cast<GpuSubdevice *>(pvArgs);
    const RegHal &regs = pGpuSubdevice->Regs();

    // polling to make sure ilwalidate is done
    //
    // VF mods:
    //   LW_PFB_PRI_MMU_CTRL_PRI_FIF is not virtualized;
    //   Poll MMU_ILWALIDATE_TRIGGER bit to promise no pending TlbIlwlidate operation
    // PF/default mods:
    //   Poll fifo empty to promise it's not queued
    return Platform::IsVirtFunMode()?
        regs.Test32(MODS_PFB_PRI_MMU_ILWALIDATE_TRIGGER_FALSE) :
        regs.Test32(MODS_PFB_PRI_MMU_CTRL_PRI_FIFO_EMPTY_TRUE);
}

/*static*/ bool PmAction_TlbIlwalidate::CallbackPollFunction
(
    void * pvArgs
)
{
    GpuSubdevice * pGpuSubdevice = static_cast<GpuSubdevice *>(pvArgs);
    const RegHal &regs = pGpuSubdevice->Regs();

    // VF mods:
    //   LW_PFB_PRI_MMU_CTRL_PRI_FIF is not virtualized;
    //   Poll MMU_ILWALIDATE_TRIGGER bit to promise no pending TlbIlwlidate operation
    // PF/default mods:
    //   Poll until the pri fifo queue has at least one space to hold the priv write
    return Platform::IsVirtFunMode()?
        regs.Test32(MODS_PFB_PRI_MMU_ILWALIDATE_TRIGGER_FALSE) :
        (regs.Read32(MODS_PFB_PRI_MMU_CTRL_PRI_FIFO_SPACE) != 0);
}

//////////////////////////////////////////////////////////////////////
// PmAction_TargetedTlbIlwalidate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
PmAction_TargetedTlbIlwalidate::PmAction_TargetedTlbIlwalidate
(
    const PmGpuDesc *pGpuDesc,
    bool IlwalidateGpc,
    PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
    bool IlwalidateSysmembar,
    PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
    bool Inband,
    PmChannelDesc *pInbandChannel,
    UINT32 subdevMask,
    PolicyManager::AddressSpaceType TargetVASpace,
    FancyPicker GPCID,
    FancyPicker ClientUnitID,
    PmChannelDesc * pPdbTargetChannel,
    bool TlbIlwalidateBar1,
    bool TlbIlwalidateBar2,
    PolicyManager::Level TlbIlwalidateLevelNum,
    PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateIlwalScope
) :
    PmAction_TlbIlwalidate("Action_TlbIlwalidate",
                           pGpuDesc,
                           NULL,
                           IlwalidateGpc,
                           IlwalidateReplay,
                           IlwalidateSysmembar,
                           IlwalidateAckType,
                           Inband,
                           pInbandChannel,
                           subdevMask,
                           TargetVASpace,
                           GPCID,
                           ClientUnitID,
                           pPdbTargetChannel,
                           TlbIlwalidateBar1,
                           TlbIlwalidateBar2,
                           TlbIlwalidateLevelNum,
                           TlbIlwalidateIlwalScope)
{
    MASSERT(m_pGpuDesc != NULL);
}

/*virtual*/ RC PmAction_TargetedTlbIlwalidate::ExelwteInband
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices GpuSubdevices = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    for (GpuSubdevices::iterator ppGpuSubDevice = GpuSubdevices.begin();
        ppGpuSubDevice != GpuSubdevices.end(); ++ppGpuSubDevice)
    {
        PmChannels channels;

        if (m_pInbandChannel)
        {
            channels = m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
        }
        else
        {
            channels = GetPolicyManager()->GetActiveChannels((*ppGpuSubDevice)->GetParentDevice());
        }

        PmChannel * pPdbTargetChannel = GetIlwalidateChannel(pTrigger, pEvent);

        if (channels.size() > 0)
        {
            PmChannel *pChannel = channels[0];

            switch (pChannel->GetModsChannel()->GetClass())
            {
            case GF100_CHANNEL_GPFIFO:
            case KEPLER_CHANNEL_GPFIFO_A:
            case KEPLER_CHANNEL_GPFIFO_B:
            case KEPLER_CHANNEL_GPFIFO_C:
                // Call the legacy function for Kepler and
                // older chips.
                return SendTlbIlwalidateMethodsLegacy(pChannel);

            case MAXWELL_CHANNEL_GPFIFO_A:
                return SendTlbIlwalidateMethodsMaxwell(pChannel);

            default:
                // Call the Pascal function for Pascal and
                // newer chips.
                return SendTlbIlwalidateMethodsPascal(
                    pChannel,
                    pPdbTargetChannel,
                    (*ppGpuSubDevice));
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! This function sends the appropriate TLB ilwalidate methods
//! for pre-Maxwell classes.
//!
RC PmAction_TargetedTlbIlwalidate::SendTlbIlwalidateMethodsLegacy
(
     PmChannel *pChannel
)
{
    RC rc;
    UINT32 MemOpB =
        DRF_DEF(906F, _MEM_OP_B, _OPERATION, _MMU_TLB_ILWALIDATE) |
        DRF_DEF(906F, _MEM_OP_B, _MMU_TLB_ILWALIDATE_PDB, _ALL)   |
        (m_IlwalidateGpc
        ? DRF_DEF(906F, _MEM_OP_B, _MMU_TLB_ILWALIDATE_GPC, _ENABLE)
        : DRF_DEF(906F, _MEM_OP_B, _MMU_TLB_ILWALIDATE_GPC, _DISABLE));

    PM_BEGIN_INSERTED_METHODS(pChannel);

    if (pChannel->GetType() != PmChannel::PIO)
        CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

    CHECK_RC(pChannel->Write(
        0,
        LW906F_MEM_OP_A,
        2,
        0,
        MemOpB));

    PM_END_INSERTED_METHODS();
    CHECK_RC(pChannel->Flush());

    return rc;
}

//--------------------------------------------------------------------
//! This function sends the appropriate TLB ilwalidate methods
//! for Maxwell and later classes.
//!
RC PmAction_TargetedTlbIlwalidate::SendTlbIlwalidateMethodsMaxwell
(
     PmChannel *pChannel
 )
{
    RC rc;
    UINT32 memOpC = 0;
    UINT32 memOpD = 0;

    memOpC = FLD_SET_DRF(B06F, _MEM_OP_C, _TLB_ILWALIDATE_PDB, _ALL, memOpC);

    if (m_IlwalidateGpc)
    {
        memOpC = FLD_SET_DRF(B06F, _MEM_OP_C, _TLB_ILWALIDATE_GPC, _ENABLE,
                             memOpC);
    }
    else
    {
        memOpC = FLD_SET_DRF(B06F, _MEM_OP_C, _TLB_ILWALIDATE_GPC, _DISABLE,
                             memOpC);
    }

    memOpD = FLD_SET_DRF(B06F, _MEM_OP_D, _OPERATION, _MMU_TLB_ILWALIDATE,
                         memOpD);

    PM_BEGIN_INSERTED_METHODS(pChannel);

    if (pChannel->GetType() != PmChannel::PIO)
        CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

    CHECK_RC(pChannel->Write(
        0,
        LWB06F_MEM_OP_C,
        2,
        memOpC,
        memOpD));

    PM_END_INSERTED_METHODS();

    CHECK_RC(pChannel->Flush());

    return rc;
}

//--------------------------------------------------------------------
//! This function sends the appropriate TLB ilwalidate methods
//! for Maxwell and later classes.
//!
RC PmAction_TargetedTlbIlwalidate::SendTlbIlwalidateMethodsPascal
(
     PmChannel * pChannel,
     PmChannel * pPdbTargetChannel,
     GpuSubdevice * pGpuSubDevice
 )
{
    RC rc;
    UINT32 memOpA = 0;
    UINT32 memOpB = 0;
    UINT32 memOpC = 0;
    UINT32 memOpD = 0;

    LwRm* pLwRm = pPdbTargetChannel ? pPdbTargetChannel->GetLwRmPtr() : LwRmPtr().Get();

    CHECK_RC(SetInbandSysmemBar(&memOpA, &memOpB, &memOpC, &memOpD));
    CHECK_RC(SetInbandGPC(&memOpA, &memOpB, &memOpC, &memOpD));
    CHECK_RC(SetInbandReplay(&memOpA, &memOpB, &memOpC, &memOpD));
    CHECK_RC(SetInbandAckType(&memOpA, &memOpB, &memOpC, &memOpD));
    // if every channel has its own va space, SetInbandPdb will need input the pPdbTargetChannel
    CHECK_RC(SetInbandPdb(&memOpA, &memOpB, &memOpC, &memOpD, pGpuSubDevice, pPdbTargetChannel, NULL, pLwRm));
    CHECK_RC(SetInbandLevel(&memOpA, &memOpB, &memOpC, &memOpD, pGpuSubDevice, pLwRm));
    if (pChannel->GetModsChannel()->GetClass() >= HOPPER_CHANNEL_GPFIFO_A)
    {
        CHECK_RC(SetInbandIlwalScope(&memOpA, &memOpB, &memOpC, &memOpD));
    }
    CHECK_RC(SetOperation(&memOpC, &memOpD));

    DebugPrintf(GetTeeCode(), "Method which is send to PB in PmAction_TlbIlwalidate().\n"
                              "MemOpA: 0x%08x\n"
                              "MemOpB: 0x%08x\n"
                              "MemOpC: 0x%08x\n"
                              "MemOpD: 0x%08x\n",
                              memOpA, memOpB, memOpC, memOpD);

    PM_BEGIN_INSERTED_METHODS(pChannel);

    if (pChannel->GetType() != PmChannel::PIO)
        CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

    CHECK_RC(pChannel->Write(
        0,
        LWC06F_MEM_OP_A,
        4,
        memOpA,
        memOpB,
        memOpC,
        memOpD));

    PM_END_INSERTED_METHODS();

    CHECK_RC(pChannel->Flush());

    return rc;
}

/*virtual*/ RC PmAction_TargetedTlbIlwalidate::SetOperation
(
 UINT32 * memOpC,
 UINT32 * memOpD
 )
{
    RC rc;

    *memOpD = FLD_SET_DRF(C06F, _MEM_OP_D, _OPERATION, _MMU_TLB_ILWALIDATE, *memOpD);

    return OK;
}

RC PmAction_TargetedTlbIlwalidate::ExelwteOutOfBand
(
 PmTrigger *pTrigger,
 const PmEvent *pEvent
 )
{
    RC rc;

    GpuSubdevices gpuSubdevices = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.TlbIlwalidate: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    PmChannel * pPdbTargetChannel = GetIlwalidateChannel(pTrigger, pEvent);
    for (GpuSubdevices::iterator ppGpuSubDevices = gpuSubdevices.begin();
        ppGpuSubDevices != gpuSubdevices.end(); ++ppGpuSubDevices)
    {
        // ToDo: m_SubdevMask doesn't work for outband
        RealExelwteOutOfBand(*ppGpuSubDevices, pPdbTargetChannel, NULL);
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_TlbIlwalidateVA
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_TlbIlwalidateVA::PmAction_TlbIlwalidateVA
(
    const PmSurfaceDesc *pSurfaceDesc,
    const UINT64 aOffset,
    bool Inband,
    UINT32 subdevMask,
    PmChannelDesc *pInbandChannel,
    bool IlwalidateGpc,
    PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
    bool IlwalidateSysmembar,
    PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
    PolicyManager::AddressSpaceType TargetVASpace,
    PmChannelDesc * pPdbTargetChannel,
    bool TlbIlwalidateBar1,
    bool TlbIlwalidateBar2,
    PolicyManager::Level TlbIlwalidateLevelNum,
    PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateIlwalScope,
    FancyPicker GPCID,
    FancyPicker ClientUnitID,
    PolicyManager::TlbIlwalidateAccessType TlbIlwalidateAccessType,
    UINT32  VEID,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction_TlbIlwalidate("Action.TlbIlwalidateVA",
                           NULL,
                           pSurfaceDesc,
                           IlwalidateGpc,
                           IlwalidateReplay,
                           IlwalidateSysmembar,
                           IlwalidateAckType,
                           Inband,
                           pInbandChannel,
                           subdevMask,
                           TargetVASpace,
                           GPCID,
                           ClientUnitID,
                           pPdbTargetChannel,
                           TlbIlwalidateBar1,
                           TlbIlwalidateBar2,
                           TlbIlwalidateLevelNum,
                           TlbIlwalidateIlwalScope),
    m_Offset(aOffset),
    m_VEID(VEID),
    m_pSmcEngineDesc(pSmcEngineDesc),
    m_MMUEngineId(UNITIALIZED_MMU_ENGINE_ID),
    m_AccessType(TlbIlwalidateAccessType)
{
    MASSERT(m_pSurfaceDesc != NULL);
    m_pPmSmcEngine = nullptr;
}

//--------------------------------------------------------------------
//! \brief tlb ilwalidate targeted virtual address
//!
/* virtual */ RC PmAction_TlbIlwalidateVA::ExelwteInband
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSubsurfaces surfaceVec = m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);
    PmSmcEngines smcEngines;
    if (m_pSmcEngineDesc)
        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
    MASSERT(smcEngines.size() <= 1);

    m_pPmSmcEngine = smcEngines.size() ? smcEngines[0] : nullptr;

    for (PmSubsurfaces::iterator pSurface = surfaceVec.begin();
        pSurface != surfaceVec.end(); ++pSurface)
    {
        GpuDevice *pGpuDevice = (*pSurface)->GetMdiagSurf()->GetGpuDev();
        for (UINT32 i = 0; i < pGpuDevice->GetNumSubdevices(); i++)
        {
            GpuSubdevice * pGpuSubdevice = pGpuDevice->GetSubdevice(i);

            PmChannels channels;
            if (m_pInbandChannel)
            {
                channels = m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
            }
            else
            {
                channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
            }

            PmChannel * pPdbTargetChannel = GetIlwalidateChannel(pTrigger, pEvent);

            if (channels.size() > 0)
            {
                PmChannel * pChannel = channels[0];
                if ((pChannel->GetModsChannel()->GetClass() == PASCAL_CHANNEL_GPFIFO_A) ||
                    (pChannel->GetModsChannel()->GetClass() == VOLTA_CHANNEL_GPFIFO_A) ||
                    (pChannel->GetModsChannel()->GetClass() == TURING_CHANNEL_GPFIFO_A) ||
                    (pChannel->GetModsChannel()->GetClass() == AMPERE_CHANNEL_GPFIFO_A ) ||
                    (pChannel->GetModsChannel()->GetClass() == HOPPER_CHANNEL_GPFIFO_A ))
                {

                    UINT32 memOpA = 0;
                    UINT32 memOpB = 0;
                    UINT32 memOpC = 0;
                    UINT32 memOpD = 0;

                    LwRm* pLwRm = pPdbTargetChannel ? pPdbTargetChannel->GetLwRmPtr() : LwRmPtr().Get();

                    SetInbandPdb(&memOpA, &memOpB, &memOpC, &memOpD, pGpuSubdevice, pPdbTargetChannel, *pSurface, pLwRm);
                    SetInbandAckType(&memOpA, &memOpB, &memOpC, &memOpD);
                    SetInbandGPC(&memOpA, &memOpB, &memOpC, &memOpD);
                    SetInbandReplay(&memOpA, &memOpB, &memOpC, &memOpD, pGpuSubdevice, pChannel);
                    SetInbandSysmemBar(&memOpA, &memOpB, &memOpC, &memOpD);
                    SetInbandLevel(&memOpA, &memOpB, &memOpC, &memOpD, pGpuSubdevice, pLwRm);
                    if (pChannel->GetModsChannel()->GetClass() >= AMPERE_CHANNEL_GPFIFO_A)
                    {
                        SetInbandIlwalScope(&memOpA, &memOpB, &memOpC, &memOpD);
                    }
                    SetTargetedVA(&memOpA, &memOpB, &memOpC, &memOpD, *pSurface);

                    SetOperation(&memOpC, &memOpD);

                    DebugPrintf(GetTeeCode(), "Method which is send to PB in PmAction_TlbIlwalidateVA().\n"
                                              "MemOpA: 0x%08x\n"
                                              "MemOpB: 0x%08x\n"
                                              "MemOpC: 0x%08x\n"
                                              "MemOpD: 0x%08x\n",
                                              memOpA, memOpB, memOpC, memOpD);

                    PM_BEGIN_INSERTED_METHODS(pChannel);

                    CHECK_RC(pChannel->Write(
                        0,
                        LWC06F_MEM_OP_A,
                        4,
                        memOpA,
                        memOpB,
                        memOpC,
                        memOpD));

                    PM_END_INSERTED_METHODS();

                    CHECK_RC(pChannel->Flush());
                }
            }
        }
    }

    return rc;
}

RC PmAction_TlbIlwalidateVA::SetInbandReplay
(
    UINT32 * memOpA,
    UINT32 * memOpB,
    UINT32 * memOpC,
    UINT32 * memOpD,
    GpuSubdevice * pGpuSubdevice,
    PmChannel * pChannel
)
{
    RC rc;

    PmAction_TlbIlwalidate::SetInbandReplay(memOpA, memOpB, memOpC, memOpD);

    if(m_IlwalidateReplay == PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL)
    {
        *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C, _TLB_ILWALIDATE_REPLAY,
            _CANCEL_VA_GLOBAL, *memOpC);

        CHECK_RC(GetMMUEngineId(pGpuSubdevice));
        if (pChannel->GetModsChannel()->GetClass() >= HOPPER_CHANNEL_GPFIFO_A)
        {
            *memOpA = FLD_SET_DRF_NUM(C86F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_MMU_ENGINE_ID,
                        m_MMUEngineId, *memOpA);
        }
        else
        {
            *memOpA = FLD_SET_DRF_NUM(C36F, _MEM_OP_A, _TLB_ILWALIDATE_CANCEL_MMU_ENGINE_ID,
                        m_MMUEngineId, *memOpA);
        }

        switch(m_AccessType)
        {
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_READ:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE, _VIRT_READ,
                                  *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE, _VIRT_WRITE,
                                  *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_STRONG:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE,
                                  _VIRT_ATOMIC_STRONG, *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_RSVRVD:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE, _VIRT_RSVRVD,
                                  *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_WEAK:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE,
                                  _VIRT_ATOMIC_WEAK, *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_ALL:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE,
                                  _VIRT_ATOMIC_ALL, *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE_AND_ATOMIC:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE,
                                  _VIRT_WRITE_AND_ATOMIC, *memOpC);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ALL:
            *memOpC = FLD_SET_DRF(C36F, _MEM_OP_C,
                                  _TLB_ILWALIDATE_ACCESS_TYPE, _VIRT_ALL,
                                  *memOpC);
            break;
        default:
            ErrPrintf("Action.TlbIlwalidateVA: Not supported Access Type.\n");
        }
    }

    return rc;
}

RC PmAction_TlbIlwalidateVA::GetMMUEngineId(GpuSubdevice * pGpuSubdevice)
{
    RC rc;

    if(m_MMUEngineId == UNITIALIZED_MMU_ENGINE_ID)
    {
        PolicyManager * pPolicyManager = PolicyManager::Instance();
        LwRm* pLwRm = m_pPmSmcEngine ? m_pPmSmcEngine->GetLwRmPtr() : LwRmPtr().Get();
        UINT32 grEngineId = MDiagUtils::GetGrEngineId(0);

        m_MMUEngineId = pPolicyManager->VEIDToMMUEngineId(m_VEID, pGpuSubdevice, pLwRm, grEngineId);
    }

    return rc;
}

RC PmAction_TlbIlwalidateVA::GetVAConfigure
(
    PmSubsurface * pSurface,
    UINT64 * virtualAddressAlign
)
{
    UINT64 virtualAddress = pSurface->GetGpuAddr() + m_Offset;
    *virtualAddressAlign = virtualAddress >> LW_PFB_PRI_MMU_ILWALIDATE_VADDR_ALIGNMENT; //align to 4k

    return OK;
}

RC PmAction_TlbIlwalidateVA::SetTargetedVA
(
     UINT32 * memOpA,
     UINT32 * memOpB,
     UINT32 * memOpC,
     UINT32 * memOpD,
     PmSubsurface * pSubSurface
 )
{
    RC rc;
    UINT64 virtualAddressAlign = 0;
    CHECK_RC(GetVAConfigure(pSubSurface, &virtualAddressAlign));

    *memOpA = FLD_SET_DRF_NUM(C06F, _MEM_OP_A, _TLB_ILWALIDATE_TARGET_ADDR_LO,
        static_cast<UINT32>(virtualAddressAlign), *memOpA);
    *memOpB = FLD_SET_DRF_NUM(C06F, _MEM_OP_B, _TLB_ILWALIDATE_TARGET_ADDR_HI,
        static_cast<UINT32>(virtualAddressAlign >> 20), *memOpB);

    return OK;
}

/*virtual*/ RC PmAction_TlbIlwalidateVA::SetOperation
(
    UINT32 * memOpC,
    UINT32 * memOpD
)
{
    RC rc;

    // When the operation is MMU_TLB_ILWALIDATE_TARGETED,
    // MEM_OP_C_TLB_ILWALIDATE_PDB must be ONE
    *memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _TLB_ILWALIDATE_PDB, _ONE, *memOpC);
    *memOpD = FLD_SET_DRF(C06F, _MEM_OP_D, _OPERATION, _MMU_TLB_ILWALIDATE_TARGETED, *memOpD);

    return OK;
}

RC PmAction_TlbIlwalidateVA::ExelwteOutOfBand
(
    PmTrigger * PmTrigger,
    const PmEvent * PmEvent
)
{
    RC rc;
    PmSubsurfaces surfaceVec = m_pSurfaceDesc->GetMatchingSubsurfaces(PmTrigger, PmEvent);
    PmChannel * pPdbTargetChannel = GetIlwalidateChannel(PmTrigger, PmEvent);
    PmSmcEngines smcEngines;
    if (m_pSmcEngineDesc)
        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(PmTrigger, PmEvent);
    MASSERT(smcEngines.size() <= 1);

    m_pPmSmcEngine = smcEngines.size() ? smcEngines[0] : nullptr;

    for (PmSubsurfaces::iterator pSurface = surfaceVec.begin();
        pSurface != surfaceVec.end(); ++pSurface)
    {
        GpuDevice *pGpuDevice = (*pSurface)->GetMdiagSurf()->GetGpuDev();
        for (UINT32 i = 0; i < pGpuDevice->GetNumSubdevices(); i++)
        {
            GpuSubdevice * pGpuSubdevice = pGpuDevice->GetSubdevice(i);

            CHECK_RC(RealExelwteOutOfBand(pGpuSubdevice, pPdbTargetChannel, *pSurface));
        }
    }

    return rc;
}

RC PmAction_TlbIlwalidateVA::SetOutOfBandReplay
(
    UINT32 * value,
    GpuSubdevice * pGpuSubdevice,
    LwRm* pLwRm
)
{
    RC rc;

    PmAction_TlbIlwalidate::SetOutOfBandReplay(value, pGpuSubdevice, pLwRm);

    if (PolicyManager::TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL == m_IlwalidateReplay)
    {
        *value = FLD_SET_DRF(, _PFB_PRI, _MMU_ILWALIDATE_REPLAY, _CANCEL_VA_GLOBAL, *value);

        // set mmu engine id
        CHECK_RC(GetMMUEngineId(pGpuSubdevice));
        *value = FLD_SET_DRF_NUM(, _PFB_PRI, _MMU_ILWALIDATE_CANCEL_CLIENT_ID, m_MMUEngineId, *value);

        switch(m_AccessType)
        {
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_READ:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_READ, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_WRITE, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_STRONG:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_ATOMIC_STRONG, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_RSVRVD:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_RSVRVD, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_WEAK:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_ATOMIC_WEAK, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_ALL:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_ATOMIC_ALL, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE_AND_ATOMIC:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_WRITE_AND_ATOMIC, *value);
            break;
        case PolicyManager::TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ALL:
            *value = FLD_SET_DRF( , _PFB_PRI, _MMU_ILWALIDATE_CACHE_LEVEL,
                        _CANCEL_ALL, *value);
            break;
        default:
            ErrPrintf("Action.TlbIlwalidateVA: Not supported Access Type.\n");
        }

    }

    return rc;
}

RC PmAction_TlbIlwalidateVA::SetOutOfBandVA
(
    UINT32 * value,
    PmSubsurface * PmSubsurface,
    GpuSubdevice * pGpuSubdevice
)
{
    RegHal &regs = pGpuSubdevice->Regs();
    RC rc;

    if (PmSubsurface == NULL)
    {
        regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ALL_VA_TRUE);
        return rc;
    }

    UINT32 valueLow = 0;
    UINT32 valueHigh = 0;
    UINT64 virtualAddressAlign = 0;

    CHECK_RC(GetVAConfigure(PmSubsurface, &virtualAddressAlign));

    // set va low address
    regs.SetField(&valueLow, MODS_PFB_PRI_MMU_ILWALIDATE_VADDR_BITS,
                  static_cast<UINT32>(virtualAddressAlign));

    DebugPrintf(GetTeeCode(), "Register value which is set in PmAction_TlbIlwalidateVA().\n"
                              "Register: LW_PFB_PRI_MMU_ILWALIDATE_VADDR value: 0x%x\n", valueLow);

    // write register LW_PFB_PRI_MMU_ILWALIDATE_VADDR
    regs.Write32(MODS_PFB_PRI_MMU_ILWALIDATE_VADDR, valueLow);

    const UINT32 lowOffset =
        regs.LookupMaskSize(MODS_PFB_PRI_MMU_ILWALIDATE_VADDR_BITS);

    // set va high address
    regs.SetField(&valueHigh, MODS_PFB_PRI_MMU_ILWALIDATE_UPPER_VADDR_BITS,
                  static_cast<UINT32>(virtualAddressAlign >> lowOffset));

    DebugPrintf(GetTeeCode(), "Register value which is set in PmAction_TlbIlwalidateVA().\n"
                              "Register: LW_PFB_PRI_MMU_ILWALIDATE_UPPER_VADDR value: 0x%x\n", valueHigh);

    // write register LW_PFB_PRI_MMU_ILWALIDATE_VADDR
    regs.Write32(MODS_PFB_PRI_MMU_ILWALIDATE_UPPER_VADDR, valueHigh);

    regs.SetField(value, MODS_PFB_PRI_MMU_ILWALIDATE_ALL_VA_FALSE);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2Flush
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_L2Flush::PmAction_L2Flush
(
    const PmGpuDesc *pGpuDesc,
    bool inband,
    UINT32 subdevMask
) :
    PmAction("Action.L2Flush"),
    m_pGpuDesc(pGpuDesc),
    m_Inband(inband),
    m_SubdevMask(subdevMask)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_L2Flush::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Flush the L2 cache
//!
/* virtual */ RC PmAction_L2Flush::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    LwRmPtr pLwRm;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        if (m_Inband)
        {
            PmChannels channelsOnGpu =
                GetPolicyManager()->GetActiveChannels(*ppGpuDevice);
            if (channelsOnGpu.empty())
            {
                ErrPrintf("Action.L2Flush: Cannot find channel\n");
                CHECK_RC(RC::ILWALID_CHANNEL);
            }
            PmChannel *pChannel = channelsOnGpu[0];
            PM_BEGIN_INSERTED_METHODS(pChannel);

            if (pChannel->GetType() != PmChannel::PIO)
                CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

            PM_END_INSERTED_METHODS();
            CHECK_RC(PmActionUtility::L2Flush(*ppGpuDevice, pChannel));
        }
        else
        {
            CHECK_RC(PmActionUtility::L2Flush(*ppGpuDevice, nullptr));
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2SysmemIlwalidate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_L2SysmemIlwalidate::PmAction_L2SysmemIlwalidate
(
    const PmGpuDesc *pGpuDesc,
    bool inband,
    UINT32 subdevMask
) :
    PmAction("Action.L2SysmemIlwalidate"),
    m_pGpuDesc(pGpuDesc),
    m_Inband(inband),
    m_SubdevMask(subdevMask)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_L2SysmemIlwalidate::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Ilwalidate the L2 sysmem cache
//!
/* virtual */ RC PmAction_L2SysmemIlwalidate::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    LwRmPtr pLwRm;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        if (m_Inband)
        {
            PmChannels channelsOnGpu =
                GetPolicyManager()->GetActiveChannels(*ppGpuDevice);
            if (channelsOnGpu.empty())
            {
                ErrPrintf("Action.L2SysmemIlwalidate: Cannot find channel\n");
                CHECK_RC(RC::ILWALID_CHANNEL);
            }
            PmChannel *pChannel = channelsOnGpu[0];
            PM_BEGIN_INSERTED_METHODS(pChannel);

            if (pChannel->GetType() != PmChannel::PIO)
                CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

            switch (pChannel->GetModsChannel()->GetClass())
            {
                case MAXWELL_CHANNEL_GPFIFO_A:
                    CHECK_RC(pChannel->Write(
                        0,
                        LWB06F_MEM_OP_C,
                        2,
                        0,
                        DRF_DEF(B06F, _MEM_OP_D, _OPERATION,
                            _L2_SYSMEM_ILWALIDATE)));
                    break;
                case PASCAL_CHANNEL_GPFIFO_A:
                case VOLTA_CHANNEL_GPFIFO_A:
                case TURING_CHANNEL_GPFIFO_A:
                case AMPERE_CHANNEL_GPFIFO_A:
                case HOPPER_CHANNEL_GPFIFO_A:
                    CHECK_RC(pChannel->Write(
                        0,
                        LWC06F_MEM_OP_A,
                        4,
                        0,
                        0,
                        0,
                        DRF_DEF(C06F, _MEM_OP_D, _OPERATION,
                            _L2_SYSMEM_ILWALIDATE)));
                    break;
                default:
                    CHECK_RC(pChannel->Write(
                        0,
                        LW906F_MEM_OP_A,
                        2,
                        0,
                        DRF_DEF(906F, _MEM_OP_B, _OPERATION,
                            _L2_SYSMEM_ILWALIDATE)));
                    break;
            }
            PM_END_INSERTED_METHODS();
            CHECK_RC(pChannel->Flush());
        }
        else
        {
            LW0080_CTRL_DMA_FLUSH_PARAMS params = {0};
            params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _L2_ILWALIDATE, _SYSMEM);
            CHECK_RC(pLwRm->ControlByDevice(*ppGpuDevice,
                                            LW0080_CTRL_CMD_DMA_FLUSH,
                                            &params, sizeof(params)));
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2PeermemIlwalidate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_L2PeermemIlwalidate::PmAction_L2PeermemIlwalidate
(
    const PmGpuDesc *pGpuDesc,
    bool inband,
    UINT32 subdevMask
) :
    PmAction("Action.L2PeermemIlwalidate"),
    m_pGpuDesc(pGpuDesc),
    m_Inband(inband),
    m_SubdevMask(subdevMask)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_L2PeermemIlwalidate::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Ilwalidate the L2 peermem cache
//!
/* virtual */ RC PmAction_L2PeermemIlwalidate::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    LwRmPtr pLwRm;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        if (m_Inband)
        {
            PmChannels channelsOnGpu =
                GetPolicyManager()->GetActiveChannels(*ppGpuDevice);
            if (channelsOnGpu.empty())
            {
                ErrPrintf("Action.L2PeermemIlwalidate: Cannot find channel\n");
                return RC::ILWALID_CHANNEL;
            }
            PmChannel *pChannel = channelsOnGpu[0];
            PM_BEGIN_INSERTED_METHODS(pChannel);

            if (pChannel->GetType() != PmChannel::PIO)
                CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

            switch (pChannel->GetModsChannel()->GetClass())
            {
                case MAXWELL_CHANNEL_GPFIFO_A:
                    CHECK_RC(pChannel->Write(
                        0,
                        LWB06F_MEM_OP_C,
                        2,
                        0,
                        DRF_DEF(B06F, _MEM_OP_D, _OPERATION,
                            _L2_PEERMEM_ILWALIDATE)));
                    break;
                case PASCAL_CHANNEL_GPFIFO_A:
                case VOLTA_CHANNEL_GPFIFO_A:
                case TURING_CHANNEL_GPFIFO_A:
                case AMPERE_CHANNEL_GPFIFO_A:
                case HOPPER_CHANNEL_GPFIFO_A:
                    CHECK_RC(pChannel->Write(
                        0,
                        LWC06F_MEM_OP_A,
                        4,
                        0,
                        0,
                        0,
                        DRF_DEF(C06F, _MEM_OP_D, _OPERATION,
                            _L2_PEERMEM_ILWALIDATE)));
                    break;
                default:
                    CHECK_RC(pChannel->Write(
                        0,
                        LW906F_MEM_OP_A,
                        2,
                        0,
                        DRF_DEF(906F, _MEM_OP_B, _OPERATION,
                            _L2_PEERMEM_ILWALIDATE)));
                    break;
            }
            PM_END_INSERTED_METHODS();
            CHECK_RC(pChannel->Flush());
        }
        else
        {
            LW0080_CTRL_DMA_FLUSH_PARAMS params = {0};
            params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT,
                                        _L2_ILWALIDATE, _PEERMEM);
            CHECK_RC(pLwRm->ControlByDevice(*ppGpuDevice,
                                            LW0080_CTRL_CMD_DMA_FLUSH,
                                            &params, sizeof(params)));
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2Operate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief L2 operation ctor
//!
PmAction_L2Operation::PmAction_L2Operation
(
    const string& name,
    const PmGpuDesc *pGpuDesc,
    UINT32 flags,
    bool inband,
    UINT32 subdevMask
) :
    PmAction(name),
    m_pGpuDesc(pGpuDesc),
    m_SubdevMask(subdevMask),
    m_Inband(inband)
{
    MASSERT(m_pGpuDesc != NULL);

    /*
    L2_ILWALIDATE_CLEAN = 0x1, 0001b,  //bit 0: 0= ilwalidate, 1= evict
    L2_FB_FLUSH         = 0x2, 0010b //bit 1: 0= no FB flush, 1= FB flush
    L2_ILWALIDATE_CLEAN_FB_FLUSH = 0x3, 0011b
    */
    m_Flags = 0;
    // L2_ILWALIDATE_CLEAN_FB_FLUSH == L2_ILWALIDATE_CLEAN | L2_FB_FLUSH, so can
    // skip the test for L2_ILWALIDATE_CLEAN_FB_FLUSH.
    if (GpuSubdevice::L2_ILWALIDATE_CLEAN  & flags)
    {
        m_Flags |= GpuSubdevice::L2_ILWALIDATE_CLEAN;
    }
    if (GpuSubdevice::L2_FB_FLUSH & flags)
    {
        m_Flags |= GpuSubdevice::L2_FB_FLUSH;
    }
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_L2Operation::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Ilwalidate or flush the L2 cache
//!
/* virtual */ RC PmAction_L2Operation::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    if (m_Inband)
    {
        ErrPrintf("Current PmAction_L2Operation only support out-of-band operations.\n");
        MASSERT(0);
        return RC::ILWALID_ARGUMENT;
    }

    RC rc;
    LwRmPtr pLwRm;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        for (UINT32 i = 0; i < (*ppGpuDevice)->GetNumSubdevices(); i++)
        {
            (*ppGpuDevice)->GetSubdevice(i)->IlwalidateL2Cache(m_Flags);
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2VidmemIlwalidate
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_L2VidmemIlwalidate::PmAction_L2VidmemIlwalidate
(
    const PmGpuDesc *pGpuDesc,
    bool inband,
    UINT32 subdevMask
) :
    PmAction_L2Operation("Action.L2VidmemIlwalidate",
        pGpuDesc,
        GpuSubdevice::L2_ILWALIDATE_CLEAN,
        inband,
        subdevMask)
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_HostMembar
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_HostMembar::PmAction_HostMembar
(
    const PmGpuDesc *pGpuDesc,
    bool isMembar,
    PmChannelDesc *pInbandChannel,
    bool inband
) :
    PmAction("Action.HostMembar"),
    m_pGpuDesc(pGpuDesc),
    m_isMembar(isMembar),
    m_pInbandChannel(pInbandChannel),
    m_Inband(inband)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_HostMembar::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Wait for all write finish
//!
/* virtual */ RC PmAction_HostMembar::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    UINT32 memOpA = 0;
    UINT32 memOpB = 0;
    UINT32 memOpC = 0;

    if (m_isMembar)
    {
        memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _MEMBAR_TYPE, _MEMBAR, memOpC);
    }
    else
    {
        memOpC = FLD_SET_DRF(C06F, _MEM_OP_C, _MEMBAR_TYPE, _SYS_MEMBAR, memOpC);
    }

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        if (m_Inband)
        {
            PmChannels channels;
            if (m_pInbandChannel)
            {
                channels = m_pInbandChannel->GetMatchingChannels(pTrigger, pEvent);
            }
            else
            {
                channels = GetPolicyManager()->GetActiveChannels(*ppGpuDevice);
            }

            if (channels.empty())
            {
                ErrPrintf("Action.HostMembar: Cannot find channel\n");
                return RC::ILWALID_CHANNEL;
            }
            else
            {
                PmChannel *pChannel = channels[0];

                PM_BEGIN_INSERTED_METHODS(pChannel);

                switch (pChannel->GetModsChannel()->GetClass())
                {
                    case PASCAL_CHANNEL_GPFIFO_A:
                    case VOLTA_CHANNEL_GPFIFO_A:
                    case TURING_CHANNEL_GPFIFO_A:
                    case AMPERE_CHANNEL_GPFIFO_A:
                    case HOPPER_CHANNEL_GPFIFO_A:
                        CHECK_RC(pChannel->Write(
                                    0,
                                    LWC06F_MEM_OP_A,
                                    4,
                                    memOpA,
                                    memOpB,
                                    memOpC,
                                    DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _MEMBAR)));
                        break;
                    default:
                        ErrPrintf("Action.HostMembar: Unknown channel class 0x%x\n",
                                  pChannel->GetModsChannel()->GetClass());
                        return RC::UNSUPPORTED_FUNCTION;
                        break;
                }

                PM_END_INSERTED_METHODS();

                CHECK_RC(pChannel->Flush());
            }
        }
        else
        {
            for (UINT32 i = 0; i < (*ppGpuDevice)->GetNumSubdevices(); i++)
            {
                (*ppGpuDevice)->GetSubdevice(i)->FbFlush(
                        PolicyManager::PollWaitTimeLimit);
            }
        }

    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ChannelWfi
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ChannelWfi::PmAction_ChannelWfi
(
    const PmChannelDesc *pChannelDesc,
    PolicyManager::Wfi wfiType
 ) :
    PmAction("Action.ChannelWfi"),
    m_pChannelDesc(pChannelDesc),
    m_wfiType(wfiType)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ChannelWfi::IsSupported
(
 const PmTrigger *pTrigger
 ) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Wait for all write finish
//!
/* virtual */ RC PmAction_ChannelWfi::Execute
(
 PmTrigger *pTrigger,
 const PmEvent *pEvent
 )
{
    RC rc;
    if (m_pChannelDesc == NULL)
    {
        ErrPrintf("Action.ChannelWfi: Wrong channel description.\n");
        return RC::ILWALID_ARGUMENT;
    }

    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
        ppChannel != channels.end(); ++ppChannel)
    {
        SemaphoreChannelWrapper * pChWrap =
            (*ppChannel)->GetModsChannel()->GetSemaphoreChannelWrapper();
        UINT32 subch = pChWrap ? pChWrap->GetLwrrentSubchannel() : 0;

        // Channel * pChannel = NULL;
        switch(m_wfiType)
        {
            case PolicyManager::WFI_POLL_HOST_SEMAPHORE:
                // according to Dan suggestiong, not support now
                // pChannel = pLwRm->GetChannel((*ppChannel)->GetHandle());
                // CHECK_RC(pChannel->WaitIdle(static_cast<FLOAT64>(10000)));
                break;
            case PolicyManager::WFI_SCOPE_ALL:
                PM_BEGIN_INSERTED_METHODS(*ppChannel);
                (*ppChannel)->Write(subch, LWB06F_WFI, LWB06F_WFI_SCOPE_ALL);
                PM_END_INSERTED_METHODS();
                break;
            case PolicyManager::WFI_SCOPE_LWRRENT_SCG_TYPE:
                PM_BEGIN_INSERTED_METHODS(*ppChannel);
                (*ppChannel)->Write(subch, LWB06F_WFI, LWB06F_WFI_SCOPE_LWRRENT_SCG_TYPE);
                PM_END_INSERTED_METHODS();
                break;
            default:
                MASSERT(!"unsupport wfi type");
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_L2WaitForSysPendingReads
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_L2WaitForSysPendingReads::PmAction_L2WaitForSysPendingReads
(
    const PmGpuDesc *pGpuDesc,
    bool inband,
    UINT32 subdevMask
) :
    PmAction("Action.L2WaitForSysPendingReads"),
    m_pGpuDesc(pGpuDesc),
    m_Inband(inband),
    m_SubdevMask(subdevMask)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_L2WaitForSysPendingReads::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Flush the L2 cache
//!
/* virtual */ RC PmAction_L2WaitForSysPendingReads::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        if ((*ppGpuDevice)->GetFamily() >= GpuDevice::Ampere)
        {
            ErrPrintf("Action.L2WaitForSysPendingReads: Not supported from Ampere.\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (m_Inband)
        {
            PmChannels channelsOnGpu =
                GetPolicyManager()->GetActiveChannels(*ppGpuDevice);
            if (channelsOnGpu.empty())
            {
                ErrPrintf("Action.L2WaitForSysPendingReads: Cannot find channel\n");
                return RC::ILWALID_CHANNEL;
            }
            PmChannel *pChannel = channelsOnGpu[0];
            PM_BEGIN_INSERTED_METHODS(pChannel);

            if (pChannel->GetType() != PmChannel::PIO)
                CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));

            switch (pChannel->GetModsChannel()->GetClass())
            {
                case PASCAL_CHANNEL_GPFIFO_A:
                case VOLTA_CHANNEL_GPFIFO_A:
                case TURING_CHANNEL_GPFIFO_A:
                case AMPERE_CHANNEL_GPFIFO_A:
                case HOPPER_CHANNEL_GPFIFO_A:
                    CHECK_RC(pChannel->Write(
                        0,
                        LWC06F_MEM_OP_A,
                        4,
                        0,
                        0,
                        0,
                        DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _L2_WAIT_FOR_SYS_PENDING_READS)));
                    break;
                default:
                    break;
            }
            PM_END_INSERTED_METHODS();
            CHECK_RC(pChannel->Flush());
        }
        else
        {
            // Issue flush
            for (UINT32 i = 0; i < (*ppGpuDevice)->GetNumSubdevices(); i++)
            {
                (*ppGpuDevice)->GetSubdevice(i)->Regs().Write32(
                        MODS_UFLUSH_L2_WAIT_FOR_SYS_PENDING_READS_PENDING_ISSUE);
            }

            // Wait flush done
            CHECK_RC(POLLWRAP_HW(
                PmAction_L2WaitForSysPendingReads::PollSysPendingRendsFlushDone,
                *ppGpuDevice,
                PolicyManager::PollWaitTimeLimit));
        }
    }
    return rc;
}

/*static*/bool PmAction_L2WaitForSysPendingReads::PollSysPendingRendsFlushDone(void* pPollArgs)
{
    GpuDevice  *pDevice = static_cast<GpuDevice *>(pPollArgs);

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
    {
        RegHal &regs = pDevice->GetSubdevice(subdev)->Regs();
        const UINT32 regValue =
            regs.Read32(MODS_UFLUSH_L2_WAIT_FOR_SYS_PENDING_READS);

        if (regs.TestField(regValue, MODS_UFLUSH_L2_WAIT_FOR_SYS_PENDING_READS_PENDING_BUSY) ||
            regs.TestField(regValue, MODS_UFLUSH_L2_WAIT_FOR_SYS_PENDING_READS_OUTSTANDING_TRUE))
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetReadOnly
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetReadOnly::PmAction_SetReadOnly
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.SetReadOnly",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _READ_ONLY, _TRUE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_READ_ONLY),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearReadOnly
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearReadOnly::PmAction_ClearReadOnly
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.ClearReadOnly",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _READ_ONLY, _FALSE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_READ_ONLY),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetAtomicDisable
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetAtomicDisable::PmAction_SetAtomicDisable
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.SetAtomicDisable",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _ATOMIC_DISABLE, _TRUE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_ATOMIC_DISABLE),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearAtomicDisable
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearAtomicDisable::PmAction_ClearAtomicDisable
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.ClearAtomicDisable",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _ATOMIC_DISABLE, _FALSE),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_ATOMIC_DISABLE),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetShaderDefaultAccess
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetShaderDefaultAccess::PmAction_SetShaderDefaultAccess
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
         "Action.SetShaderDefaultAccess",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                 _SHADER_ACCESS, _DEFAULT),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SHADER_ACCESS),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetShaderReadOnly
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetShaderReadOnly::PmAction_SetShaderReadOnly
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.SetShaderReadOnly",
         pSurfaceDesc,
         offset,
         size,
         PageSize,
         DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                 _SHADER_ACCESS, _READ_ONLY),
         DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SHADER_ACCESS),
         isInband,
         pInbandChannel,
         PolicyManager::GmmuVASpace,
         PolicyManager::LEVEL_PTE,
         deferTlbIlwalidate
     )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetShaderWriteOnly
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetShaderWriteOnly::PmAction_SetShaderWriteOnly
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.SetShaderWriteOnly",
        pSurfaceDesc,
        offset,
        size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _SHADER_ACCESS, _WRITE_ONLY),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SHADER_ACCESS),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetShaderReadWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetShaderReadWrite::PmAction_SetShaderReadWrite
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.SetShaderReadWrite",
        pSurfaceDesc,
        offset,
        size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _SHADER_ACCESS, _READ_WRITE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SHADER_ACCESS),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPdeSize
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPdeSize::PmAction_SetPdeSize
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    FLOAT64 PdeSize
) :
    PmAction("Action.SetPdeSize"),
    m_PageDesc(pSurfaceDesc, Offset, Size),
    m_PdeSize(PdeSize)
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetPdeSize::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_PageDesc.IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Set the PDE's "Size" field
//!
/* virtual */ RC PmAction_SetPdeSize::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Call LW0080_CTRL_CMD_DMA_UPDATE_PDE on each PDE we need to update
    //
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS> params;
    GpuDevices gpuDevices;

    CHECK_RC(CreateUpdatePdeStructs(&ranges, PolicyManager::ALL_PAGE_SIZES,
                                    &params, NULL, &gpuDevices));

    LwRm* pLwRm = ranges[0].GetSurface()->GetLwRmPtr();

    for (UINT32 ii = 0; ii < params.size(); ++ii)
    {
        if (m_PdeSize == 1.0)
        {
            params[ii].flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2, _FLAGS,
                _PDE_SIZE, _FULL, params[ii].flags);
        }
        else if (m_PdeSize == 0.5)
        {
            params[ii].flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2, _FLAGS,
                _PDE_SIZE, _HALF, params[ii].flags);
        }
        else if (m_PdeSize == 0.25)
        {
            params[ii].flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2, _FLAGS,
                _PDE_SIZE, _QUARTER, params[ii].flags);
        }
        else if (m_PdeSize == 0.125)
        {
            params[ii].flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2, _FLAGS,
                _PDE_SIZE, _EIGHTH, params[ii].flags);
        }
        else
        {
            MASSERT(!"Illegal size in Action.SetPdeSize");
        }

        CHECK_RC(pLwRm->ControlByDevice(gpuDevices[ii],
                                        LW0080_CTRL_CMD_DMA_UPDATE_PDE_2,
                                        &params[ii], sizeof(params[ii])));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPdeAperture
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPdeAperture::PmAction_SetPdeAperture
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::Aperture Aperture,
    PolicyManager::PageSize PageSize
) :
    PmAction("Action.SetPdeAperture"),
    m_PageDesc(pSurfaceDesc, Offset, Size),
    m_Aperture(Aperture),
    m_PageSize(PageSize)
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetPdeAperture::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_PageDesc.IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Set the PDE's "Aperture" field
//!
/* virtual */ RC PmAction_SetPdeAperture::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Select the pteTableAperture we will pass to
    // LW0080_CTRL_CMD_DMA_UPDATE_PDE_2
    //
    UINT32 pteTableAperture =
        LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_ILWALID;
    switch (m_Aperture)
    {
        case PolicyManager::APERTURE_FB:
            pteTableAperture =
                LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_VIDEO_MEMORY;
            break;
        case PolicyManager::APERTURE_COHERENT:
            pteTableAperture =
                LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_SYSTEM_COHERENT_MEMORY;
            break;
        case PolicyManager::APERTURE_NONCOHERENT:
            pteTableAperture =
                LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_SYSTEM_NON_COHERENT_MEMORY;
            break;
        case PolicyManager::APERTURE_ILWALID:
            pteTableAperture =
                LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_ILWALID;
            break;
        default:
            MASSERT(!"Illegal aperture in Action.SetPdeAperture");
    }

    // Call LW0080_CTRL_CMD_DMA_UPDATE_PDE_2 on each PDE we need to update
    //
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS> params;
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PAGE_TABLE_PARAMS*> pteBlocks;
    GpuDevices gpuDevices;

    CHECK_RC(CreateUpdatePdeStructs(&ranges, m_PageSize,
                                    &params, &pteBlocks, &gpuDevices));

    for (UINT32 ii = 0; ii < pteBlocks.size(); ++ii)
    {
        pteBlocks[ii]->aperture = pteTableAperture;
    }

    LwRm* pLwRm = ranges[0].GetSurface()->GetLwRmPtr();
    for (UINT32 ii = 0; ii < params.size(); ++ii)
    {
        CHECK_RC(pLwRm->ControlByDevice(gpuDevices[ii],
                                        LW0080_CTRL_CMD_DMA_UPDATE_PDE_2,
                                        &params[ii], sizeof(params[ii])));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SparsifyMmuLevel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SparsifyMmuLevel::PmAction_SparsifyMmuLevel
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVASpace,
    PolicyManager::Level targetMmuLevelType,
    bool deferTlbIlwalidate
) :
    PmAction("Action.SparsifyMmuLevel"),
    m_PageDesc(pSurfaceDesc, Offset, Size),
    m_PageSize(PageSize),
    m_IsInband(isInband),
    m_DeferTlbIlwalidate(deferTlbIlwalidate),
    m_pInbandChannel(pInbandChannel),
    m_TargetVASpace(TargetVASpace),
    m_TargetMmuLevelType(targetMmuLevelType)
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SparsifyMmuLevel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_PageDesc.IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Sparsify the MMU level
//!
/* virtual */ RC PmAction_SparsifyMmuLevel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        ErrPrintf("Action.SparsifyMmuLevel: No matching surface ranges found\n");
        return RC::ILWALID_ARGUMENT;
    }

    //
    // New MMU format using MMU level API
    //
    // Invalid + Sparse(Vol)
    UINT32 pteFlags =
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _SPARSE, _TRUE) |
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE);
    UINT32 pteMask =
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_SPARSE) |
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID);

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end(); ++pRange)
    {
        if (m_IsInband)
        {
            PmChannel *pInbandChannel = GetInbandChannel(m_pInbandChannel,
                pTrigger, pEvent, &(*pRange));
            MASSERT (NULL != pInbandChannel);

            CHECK_RC(pRange->ModifyPteFlags(m_PageSize, pteFlags,
                    pteMask, pInbandChannel, m_TargetVASpace,
                    m_TargetMmuLevelType, m_DeferTlbIlwalidate));
        }
        else
        {
            CHECK_RC(pRange->ModifyPteFlags(m_PageSize, pteFlags,
                pteMask, 0, m_TargetVASpace, m_TargetMmuLevelType,
                m_DeferTlbIlwalidate));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPteAperture
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPteAperture::PmAction_SetPteAperture
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::PageSize PageSize,
    PolicyManager::Aperture Aperture,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.SetPteAperture",
        pSurfaceDesc,
        Offset,
        Size,
        PageSize,
        GetPteFlagsFromAperture(Aperture),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//--------------------------------------------------------------------
//! Colwert an aperture into the corresponding
//! LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE value
//!
/* static */ UINT32 PmAction_SetPteAperture::GetPteFlagsFromAperture
(
    PolicyManager::Aperture Aperture
)
{
    switch (Aperture)
    {
        case PolicyManager::APERTURE_FB:
            return DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                           _APERTURE, _VIDEO_MEMORY);
        case PolicyManager::APERTURE_COHERENT:
            return DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                           _APERTURE, _SYSTEM_COHERENT_MEMORY);
        case PolicyManager::APERTURE_NONCOHERENT:
            return DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                           _APERTURE, _SYSTEM_NON_COHERENT_MEMORY);
        case PolicyManager::APERTURE_PEER:
            return DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                           _APERTURE, _PEER_MEMORY);
        default:
            MASSERT(!"Illegal aperture in Action.SetPteAperture");
    }

    return 0;  // Shouldn't get here
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetVolatile
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetVolatile::PmAction_SetVolatile
(
    string name,
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        name,
        pSurfaceDesc,
        Offset,
        Size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _GPU_CACHED, _FALSE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_GPU_CACHED),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearVolatile
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearVolatile::PmAction_ClearVolatile
(
    string name,
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        name,
        pSurfaceDesc,
        Offset,
        Size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _GPU_CACHED,_TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_GPU_CACHED),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetKind
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetKind::PmAction_SetKind
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 Offset,
    UINT64 Size,
    UINT32 Kind
) :
    PmAction("Action.SetKind"),
    m_PageDesc(pSurfaceDesc, Offset, Size),
    m_Kind(Kind)
{
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetKind::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_PageDesc.IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Set the PTE's "Kind" field
//!
/* virtual */ RC PmAction_SetKind::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);
    vector<LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS> params;
    GpuDevices gpuDevices;
    vector<LwRm*> lwRms;

    // Save surface PteKind to restore it at the end of test (before CRCCheck)
    PolicyManager::Instance()->SavePteKind(ranges);

    CHECK_RC(PmMemRange::CreateSetPteInfoStructs(
        &ranges, &params, &gpuDevices, &lwRms));

    for (UINT32 ii = 0; ii < params.size(); ++ii)
    {
        for (UINT32 jj = 0; jj < LW0080_CTRL_DMA_SET_PTE_INFO_PTE_BLOCKS; ++jj)
        {
            if (params[ii].pteBlocks[jj].pageSize != 0 &&
                FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                    _TRUE, params[ii].pteBlocks[jj].pteFlags))
            {
                params[ii].pteBlocks[jj].kind = m_Kind;
            }
        }
        CHECK_RC(lwRms[ii]->ControlByDevice(gpuDevices[ii],
                                        LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                                        &params[ii], sizeof(params[ii])));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_IlwalidatePdbTarget
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmAction_IlwalidatePdbTarget::PmAction_IlwalidatePdbTarget
(
    PmChannelDesc *pChannelDesc
) :
    PmAction("Action.IlwalidatePdbTarget"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting page
//!
/* virtual */ RC PmAction_IlwalidatePdbTarget::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Reset the requested engine context(s)
//!
/* virtual */ RC PmAction_IlwalidatePdbTarget::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        LwRm* pLwRm = (*ppChannel)->GetLwRmPtr();
        // Ilwalidate the target field on the PDB associated with this
        // channel.
        //
        LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS Params = {0};
        Params.hChannel = (*ppChannel)->GetHandle();
        Params.property =
            LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_ILWALIDATE_PDB_TARGET;
        Params.value = 0;

        CHECK_RC(pLwRm->ControlByDevice(
                     (*ppChannel)->GetGpuDevice(),
                     LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                     &Params, sizeof(Params)));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResetEngineContext
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmAction_ResetEngineContext::PmAction_ResetEngineContext
(
    PmChannelDesc *pChannelDesc,
    const RegExp &EngineNames
) :
    PmAction("Action.ResetEngineContext"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);

    string enginePmName = EngineNames.ToString();
    m_EngineNames.Set(PolicyManager::Instance()->ColwPm2GpuEngineName(enginePmName),
                      EngineNames.GetFlags());
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting page
//!
/* virtual */ RC PmAction_ResetEngineContext::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Reset the requested engine context(s)
//!
/* virtual */ RC PmAction_ResetEngineContext::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        GpuDevice *pGpuDevice = (*ppChannel)->GetGpuDevice();
        vector<UINT32> Engines;
        LwRm* pLwRm = (*ppChannel)->GetLwRmPtr();

        CHECK_RC(GetEnginesList(m_EngineNames, *ppChannel, pGpuDevice, &Engines));
        MASSERT(Engines.size() != 0);

        for (auto engine : Engines)
        {
            // Reset the engine on the channel
            LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS Params = {0};
            Params.hChannel = (*ppChannel)->GetHandle();
            Params.property =
                LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_RESETENGINECONTEXT;
            Params.value = engine;

            CHECK_RC(pLwRm->ControlByDevice(
                     pGpuDevice,
                     LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                     &Params, sizeof(Params)));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResetEngineContextNoPreempt
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmAction_ResetEngineContextNoPreempt::PmAction_ResetEngineContextNoPreempt
(
    PmChannelDesc *pChannelDesc,
    const RegExp &EngineNames
) :
    PmAction("Action.ResetEngineContextNoPreempt"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);

    string enginePmName = EngineNames.ToString();
    m_EngineNames.Set(PolicyManager::Instance()->ColwPm2GpuEngineName(enginePmName),
                      EngineNames.GetFlags());
}

//--------------------------------------------------------------------
//! \brief Check to make sure the trigger would have a faulting page
//!
/* virtual */ RC PmAction_ResetEngineContextNoPreempt::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Reset the requested engine context(s)
//!
/* virtual */ RC PmAction_ResetEngineContextNoPreempt::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Loop through all matching channels
    //
    PmChannels Channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (PmChannels::iterator ppChannel = Channels.begin();
         ppChannel != Channels.end(); ++ppChannel)
    {
        GpuDevice *pGpuDevice = (*ppChannel)->GetGpuDevice();
        vector<UINT32> Engines;
        LwRm* pLwRm = (*ppChannel)->GetLwRmPtr();

        CHECK_RC(GetEnginesList(m_EngineNames, *ppChannel, pGpuDevice, &Engines));
        MASSERT(Engines.size() != 0);

        for (auto engine : Engines)
        {
            // Reset the engine on the channel
            LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS Params = {0};
            Params.hChannel = (*ppChannel)->GetHandle();
            Params.property =
                LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_RESETENGINECONTEXT_NOPREEMPT;
            Params.value = engine;

            CHECK_RC(pLwRm->ControlByDevice(
                     pGpuDevice,
                     LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                     &Params, sizeof(Params)));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPrivSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPrivSurfaces::PmAction_SetPrivSurfaces
(
    const PmSurfaceDesc *pSurfaceDesc,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyPteFlagsInSurfaces
    (
        "Action.SetPrivSurfaces",
        pSurfaceDesc,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _PRIV, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_PRIV),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearPrivSurfaces
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearPrivSurfaces::PmAction_ClearPrivSurfaces
(
    const PmSurfaceDesc *pSurfaceDesc,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool DeferTlbIlwalidate
) :
    PmAction_ModifyPteFlagsInSurfaces
    (
        "Action.ClearPrivSurfaces",
        pSurfaceDesc,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _PRIV, _FALSE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_PRIV),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        DeferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPrivPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPrivPages::PmAction_SetPrivPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.SetPrivPages",
        pSurfaceDesc,
        offset,
        size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _PRIV, _TRUE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_PRIV),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearPrivPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearPrivPages::PmAction_ClearPrivPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    PolicyManager::PageSize PageSize,
    bool isInband,
    PmChannelDesc *pInbandChannel,
    bool deferTlbIlwalidate
) :
    PmAction_ModifyMMULevelFlagsInPages
    (
        "Action.ClearPrivPages",
        pSurfaceDesc,
        offset,
        size,
        PageSize,
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _PRIV, _FALSE),
        DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_PRIV),
        isInband,
        pInbandChannel,
        PolicyManager::GmmuVASpace,
        PolicyManager::LEVEL_PTE,
        deferTlbIlwalidate
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPrivChannels
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPrivChannels::PmAction_SetPrivChannels
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.SetPrivChannels"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetPrivChannels::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Set the priv bit in the indicated channels
//!
/* virtual */ RC PmAction_SetPrivChannels::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        if ((*ppChannel)->GetType() == PmChannel::GPFIFO)
        {
            PM_BEGIN_INSERTED_METHODS(*ppChannel);
            CHECK_RC((*ppChannel)->SetPrivEnable(true));
            PM_END_INSERTED_METHODS();
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearPrivChannels
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearPrivChannels::PmAction_ClearPrivChannels
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction("Action.ClearPrivChannels"),
    m_pChannelDesc(pChannelDesc)
{
    MASSERT(m_pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ClearPrivChannels::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Clear the priv bit in the indicated channels
//!
/* virtual */ RC PmAction_ClearPrivChannels::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        if ((*ppChannel)->GetType() == PmChannel::GPFIFO)
        {
            PM_BEGIN_INSERTED_METHODS(*ppChannel);
            CHECK_RC((*ppChannel)->SetPrivEnable(false));
            PM_END_INSERTED_METHODS();
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_HostSemaphoreOperation
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_HostSemaphoreOperation::PmAction_HostSemaphoreOperation
(
    const string        &Name,
    const PmChannelDesc *pChannelDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker&   offsetPicker,
    const FancyPicker&   payloadPicker,
    UINT32               subdevMask,
    const PmGpuDesc     *pRemoteGpuDesc
) :
    PmAction(Name),
    m_pChannelDesc(pChannelDesc),
    m_pSurfaceDesc(pSurfaceDesc),
    m_OffsetPicker(offsetPicker),
    m_PayloadPicker(payloadPicker),
    m_SubdevMask(subdevMask),
    m_pRemoteGpuDesc(pRemoteGpuDesc)
{
    MASSERT(pChannelDesc != NULL);
    MASSERT(pSurfaceDesc != NULL);

    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    m_OffsetPicker.SetContext(&m_FpContext);
    m_PayloadPicker.SetContext(&m_FpContext);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_HostSemaphoreOperation::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_pChannelDesc->IsSupportedInAction(pTrigger));
    CHECK_RC(m_pSurfaceDesc->IsSupportedInAction(pTrigger));
    if (m_pRemoteGpuDesc)
    {
        CHECK_RC(m_pRemoteGpuDesc->IsSupportedInAction(pTrigger));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a host semaphore operation to the indicated channels
//!
/* virtual */ RC PmAction_HostSemaphoreOperation::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC             rc;
    UINT64         offset = m_OffsetPicker.Pick64();
    UINT64         payload = m_PayloadPicker.Pick64();
    PmPageDesc     pageDesc(m_pSurfaceDesc, offset, 4);
    PmMemRanges    ranges = pageDesc.GetMatchingRanges(pTrigger, pEvent);
    GpuSubdevice  *pLocalSubdev = NULL;
    GpuSubdevice  *pRemoteSubdev = NULL;
    PmChannels     channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    ++m_FpContext.LoopNum;
    CHECK_RC(GetSubdevicesForSurfaceAccess(pTrigger,
                                           pEvent,
                                           m_pSurfaceDesc->GetGpuDesc(),
                                           m_pRemoteGpuDesc,
                                           &pLocalSubdev,
                                           &pRemoteSubdev));

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        PmChannel *pChannel = *ppChannel;

        PM_BEGIN_INSERTED_METHODS(pChannel);

        SetSemaphoreFlags(pChannel);
        if (pChannel->GetType() != PmChannel::PIO)
            CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));
        for (PmMemRanges::iterator pRange = ranges.begin();
             pRange != ranges.end(); ++pRange)
        {
            if ((!pRemoteSubdev &&
                 (pChannel->GetGpuDevice() == pRange->GetMdiagSurf()->GetGpuDev())) ||
                (pRemoteSubdev &&
                 (pChannel->GetGpuDevice() == pRemoteSubdev->GetParentDevice())))
            {
                if (pRemoteSubdev)
                {
                    CHECK_RC(pRange->MapPeer(pLocalSubdev, pRemoteSubdev));
                }
                CHECK_RC(pChannel->SetSemaphoreOffset(pRange->GetGpuAddr(pLocalSubdev,
                                                                         pRemoteSubdev)));

                if (pChannel->Has64bitSemaphores())
                {
                    Channel::SemaphorePayloadSize size = pChannel->GetSemaphorePayloadSize();
                    Utility::CleanupOnReturnArg<PmChannel, RC, Channel::SemaphorePayloadSize>
                        restoreSemaPayload(pChannel, &PmChannel::SetSemaphorePayloadSize, size);

                    CHECK_RC(pChannel->SetSemaphorePayloadSize(
                        m_PayloadPicker.Type() == FancyPicker::T_INT64?
                            Channel::SEM_PAYLOAD_SIZE_64BIT :
                            Channel::SEM_PAYLOAD_SIZE_32BIT));
                    CHECK_RC(DoSemaphoreOperation(pChannel, payload));
                }
                else
                {
                    if (payload >> 32)
                    {
                        return RC::BAD_PARAMETER;
                    }

                    CHECK_RC(DoSemaphoreOperation(pChannel, payload));
                }
            }
        }
        PM_END_INSERTED_METHODS();
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_HostSemaphoreAcquire
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_HostSemaphoreAcquire::PmAction_HostSemaphoreAcquire
(
    const PmChannelDesc *pChannelDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker&   offsetPicker,
    const FancyPicker&   payloadPicker,
    UINT32               subdevMask,
    const PmGpuDesc     *pRemoteGpuDesc
) :
    PmAction_HostSemaphoreOperation("Action.HostSemaphoreAcquire",
                                    pChannelDesc,
                                    pSurfaceDesc,
                                    offsetPicker,
                                    payloadPicker,
                                    subdevMask,
                                    pRemoteGpuDesc)
{
}

//--------------------------------------------------------------------
//! \brief Virtual method called by Execute() to do the acquire
//!
/* virtual */ RC PmAction_HostSemaphoreAcquire::DoSemaphoreOperation
(
    PmChannel *pChannel,
    UINT64 Payload
)
{
    return pChannel->SemaphoreAcquire(Payload);
}

//////////////////////////////////////////////////////////////////////
// PmAction_HostSemaphoreRelease
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_HostSemaphoreRelease::PmAction_HostSemaphoreRelease
(
    const PmChannelDesc *pChannelDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker&   offsetPicker,
    const FancyPicker&   payloadPicker,
    UINT32               subdevMask,
    UINT32               flags,
    const PmGpuDesc     *pRemoteGpuDesc
) :
    PmAction_HostSemaphoreOperation("Action.HostSemaphoreRelease",
                                    pChannelDesc,
                                    pSurfaceDesc,
                                    offsetPicker,
                                    payloadPicker,
                                    subdevMask,
                                    pRemoteGpuDesc),
    m_SemRelFlags(flags)
{
}

//--------------------------------------------------------------------
//! \brief Virtual method called by Execute() to set the flags
//!
/* virtual */ void PmAction_HostSemaphoreRelease::SetSemaphoreFlags
(
    PmChannel *pChannel
)
{
    pChannel->SetSemaphoreReleaseFlags(m_SemRelFlags);
}

//--------------------------------------------------------------------
//! \brief Virtual method called by Execute() to do the release
//!
/* virtual */ RC PmAction_HostSemaphoreRelease::DoSemaphoreOperation
(
    PmChannel *pChannel,
    UINT64 Payload
)
{
    return pChannel->SemaphoreRelease(Payload);
}

//////////////////////////////////////////////////////////////////////
// PmAction_HostSemaphoreReduction
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_HostSemaphoreReduction::PmAction_HostSemaphoreReduction
(
    const PmChannelDesc *pChannelDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker&   offsetPicker,
    const FancyPicker&   payloadPicker,
    Channel::Reduction   reduction,
    Channel::ReductionType  reductionType,
    UINT32               subdevMask,
    UINT32               flags,
    const PmGpuDesc     *pRemoteGpuDesc
) :
    PmAction_HostSemaphoreOperation("Action.HostSemaphoreReduction",
                                    pChannelDesc,
                                    pSurfaceDesc,
                                    offsetPicker,
                                    payloadPicker,
                                    subdevMask,
                                    pRemoteGpuDesc),
    m_Reduction(reduction),
    m_ReductionType(reductionType),
    m_SemRelFlags(flags)
{
}

//--------------------------------------------------------------------
//! \brief Virtual method called by Execute() to set the flags
//!
/* virtual */ void PmAction_HostSemaphoreReduction::SetSemaphoreFlags
(
    PmChannel *pChannel
)
{
    pChannel->SetSemaphoreReleaseFlags(m_SemRelFlags);
}

//--------------------------------------------------------------------
//! \brief Virtual method called by Execute() to do the reduction
//!
/* virtual */ RC PmAction_HostSemaphoreReduction::DoSemaphoreOperation
(
    PmChannel *pChannel,
    UINT64 Payload
)
{
    return pChannel->SemaphoreReduction(m_Reduction, m_ReductionType, Payload);
}

//////////////////////////////////////////////////////////////////////
// PmAction_EngineSemaphoreOperation
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EngineSemaphoreOperation::PmAction_EngineSemaphoreOperation
(
    const PmChannelDesc *pChannelDesc,
    const PmSurfaceDesc *pSurfaceDesc,
    const FancyPicker&   offsetPicker,
    const FancyPicker&   payloadPicker,
    UINT32               subdevMask,
    UINT32               flags,
    const PmGpuDesc     *pRemoteGpuDesc,
    SemaphoreOpType      operationType,
    const string&        name
) :
    PmAction(name),
    m_pChannelDesc(pChannelDesc),
    m_pSurfaceDesc(pSurfaceDesc),
    m_OffsetPicker(offsetPicker),
    m_PayloadPicker(payloadPicker),
    m_SubdevMask(subdevMask),
    m_SemRelFlags(flags),
    m_pRemoteGpuDesc(pRemoteGpuDesc),
    m_SemaphoreOpType(operationType)
{
    MASSERT(pChannelDesc   != NULL);
    MASSERT(pSurfaceDesc   != NULL);

    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;
    m_OffsetPicker.SetContext(&m_FpContext);
    m_PayloadPicker.SetContext(&m_FpContext);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_EngineSemaphoreOperation::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_pChannelDesc->IsSupportedInAction(pTrigger));
    CHECK_RC(m_pSurfaceDesc->IsSupportedInAction(pTrigger));
    if (m_pRemoteGpuDesc)
    {
        CHECK_RC(m_pRemoteGpuDesc->IsSupportedInAction(pTrigger));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write an engine semaphore release/acquire to the indicated channels
//!
/* virtual */ RC PmAction_EngineSemaphoreOperation::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC             rc;
    UINT32         offset = m_OffsetPicker.Pick();
    UINT64         payload = m_PayloadPicker.Pick64();
    PmPageDesc     pageDesc(m_pSurfaceDesc, offset, 4);
    PmMemRanges    ranges = pageDesc.GetMatchingRanges(pTrigger, pEvent);
    GpuSubdevice  *pLocalSubdev = NULL;
    GpuSubdevice  *pRemoteSubdev = NULL;
    PmChannels     channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    ++m_FpContext.LoopNum;
    CHECK_RC(GetSubdevicesForSurfaceAccess(pTrigger,
                                           pEvent,
                                           m_pSurfaceDesc->GetGpuDesc(),
                                           m_pRemoteGpuDesc,
                                           &pLocalSubdev,
                                           &pRemoteSubdev));

    for (PmChannels::iterator ppChannel = channels.begin();
         ppChannel != channels.end(); ++ppChannel)
    {
        PmChannel *pChannel = *ppChannel;
        SemaphoreChannelWrapper *pSemWrap =
            pChannel->GetModsChannel()->GetSemaphoreChannelWrapper();

        PM_BEGIN_INSERTED_METHODS(pChannel);
        if (pChannel->GetType() != PmChannel::PIO)
            CHECK_RC(pChannel->WriteSetSubdevice(m_SubdevMask));
        if ( pSemWrap != NULL && pSemWrap->SupportsBackend() )
        {
            pChannel->SetSemaphoreReleaseFlags(m_SemRelFlags);

            for (PmMemRanges::iterator pRange = ranges.begin();
                 pRange != ranges.end(); ++pRange)
            {
                if ((!pRemoteSubdev &&
                     (pChannel->GetGpuDevice() == pRange->GetMdiagSurf()->GetGpuDev())) ||
                    (pRemoteSubdev &&
                     (pChannel->GetGpuDevice() == pRemoteSubdev->GetParentDevice())))
                {
                    if (pRemoteSubdev)
                    {
                        CHECK_RC(pRange->MapPeer(pLocalSubdev, pRemoteSubdev));
                    }

                    if (m_SemaphoreOpType == SEMAPHORE_OPERATION_RELEASE)
                    {
                        CHECK_RC(pSemWrap->ReleaseBackendSemaphore(
                            pRange->GetGpuAddr(pLocalSubdev, pRemoteSubdev),
                            payload, false, NULL));
                    }
                    else if (m_SemaphoreOpType == SEMAPHORE_OPERATION_ACQUIRE)
                    {
                        CHECK_RC(pSemWrap->AcquireBackendSemaphore(
                            pRange->GetGpuAddr(pLocalSubdev, pRemoteSubdev),
                            payload, false, NULL));
                    }
                    else
                    {
                        MASSERT(!"Unkown Semaphore operation.");
                    }
                }
            }
        }
        PM_END_INSERTED_METHODS();
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PowerWait
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PowerWait::PmAction_PowerWait
(
    const UINT32 modelWaitUS,
    const UINT32 rtlWaitUS,
    const UINT32 hwWaitUS,
    const bool   bBusyWait
) :
    PmAction("Action.PowerWait"),
    m_ModelWaitUS(modelWaitUS),
    m_RTLWaitUS(rtlWaitUS),
    m_HWWaitUS(hwWaitUS),
    m_bBusyWait(bBusyWait)
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmAction_PowerWait::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Sleep for some number of milliseconds
//!
/* virtual */ RC PmAction_PowerWait::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    UINT32 waitTime = 0;

    switch(Platform::GetSimulationMode())
    {
        case Platform::Fmodel:
        case Platform::Amodel:
            waitTime = m_ModelWaitUS;
            break;
        case Platform::RTL:
            waitTime = m_RTLWaitUS;
            break;
        case Platform::Hardware:
            waitTime = m_HWWaitUS;
            break;
    }

    if (m_bBusyWait)
        Platform::Delay(waitTime);
    else
        Platform::SleepUS(waitTime);

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_EscapeRead
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EscapeRead::PmAction_EscapeRead
(
    const PmGpuDesc *pGpuDesc,
    const string &path,
    UINT32        index,
    UINT32        size
) :
    PmAction(pGpuDesc ? "Action.GpuEscapeRead" : "Action.EscapeRead"),
    m_pGpuDesc(pGpuDesc),
    m_Path(path),
    m_Index(index),
    m_Size(size)
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmAction_EscapeRead::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (m_pGpuDesc != NULL)
        return m_pGpuDesc->IsSupportedInAction(pTrigger);

    return OK;
}

//--------------------------------------------------------------------
//! \brief Perform an escape read
//!
/* virtual */ RC PmAction_EscapeRead::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    // Need to check the simulation mode here because IsSupported is called
    // when PolicyManager parses the policy file which is prior to platform
    // initialization
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        UINT32 value;
        RC rc;
        string escapeGildStr;

        // If no Gpu was specified, then do a generic escape read
        if (m_pGpuDesc == NULL)
        {
            // Read the data into a variable and record the results via the
            // policy manager gilder
            Platform::EscapeRead(m_Path.c_str(), m_Index, m_Size, &value);
            escapeGildStr = Utility::StrPrintf("ESCAPE_READ %s 0x%08x",
                                               m_Path.c_str(), value);
            PmEvent_GildString escapeGildEvent(escapeGildStr);
            return GetPolicyManager()->GetEventManager()->HandleEvent(&escapeGildEvent);
        }

        GpuSubdevices  gpuSubdevs;

        gpuSubdevs = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

        // Loop through all subdevices
        for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevs.begin();
             ppGpuSubdevice != gpuSubdevs.end(); ++ppGpuSubdevice)
        {
            // Read the data into a variable and record the results via the
            // policy manager gilder
            (*ppGpuSubdevice)->EscapeRead(m_Path.c_str(), m_Index,
                                          m_Size, &value);

            escapeGildStr =
                Utility::StrPrintf("GPU_ESCAPE_READ %d:%d %s 0x%08x",
                         (*ppGpuSubdevice)->GetParentDevice()->GetDeviceInst(),
                         (*ppGpuSubdevice)->GetSubdeviceInst(),
                         m_Path.c_str(), value);
            PmEvent_GildString escapeGildEvent(escapeGildStr);
            CHECK_RC(GetPolicyManager()->GetEventManager()->
                             HandleEvent(&escapeGildEvent));
        }

        return rc;
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//////////////////////////////////////////////////////////////////////
// PmAction_EscapeWrite
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EscapeWrite::PmAction_EscapeWrite
(
    const PmGpuDesc *pGpuDesc,
    const string &path,
    UINT32        index,
    UINT32        size,
    UINT32        data
) :
    PmAction(pGpuDesc ? "Action.GpuEscapeWrite" : "Action.EscapeWrite"),
    m_pGpuDesc(pGpuDesc),
    m_Path(path),
    m_Index(index),
    m_Size(size),
    m_Data(data)
{
}

//--------------------------------------------------------------------
//! \brief Return OK.
//!
/* virtual */ RC PmAction_EscapeWrite::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (m_pGpuDesc != NULL)
        return m_pGpuDesc->IsSupportedInAction(pTrigger);

    return OK;
}

//--------------------------------------------------------------------
//! \brief Perform an escape write
//!
/* virtual */ RC PmAction_EscapeWrite::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    // Need to check the simulation mode here because IsSupported is called
    // when PolicyManager parses the policy file which is prior to platform
    // initialization
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // If no Gpu was specified, then do a generic escape write
        if (m_pGpuDesc == NULL)
        {
            Platform::EscapeWrite(m_Path.c_str(), m_Index, m_Size, m_Data);
        }
        else
        {
            GpuSubdevices  gpuSubdevs;

            gpuSubdevs = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

            // Loop through all subdevices
            for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevs.begin();
                 ppGpuSubdevice != gpuSubdevs.end(); ++ppGpuSubdevice)
            {
                (*ppGpuSubdevice)->EscapeWrite(m_Path.c_str(), m_Index,
                                               m_Size, m_Data);
            }
        }
        return OK;
    }

    // Silently ignore BACKDOOR_ACCESS escape-write on emulation/silicon as 
    // those models always use a frontdoor memory access path anyway. This will 
    // remove the need to create a different version of the policy file for 
    // emulation/silicon.
    else if (m_Path == "BACKDOOR_ACCESS")
    {
        InfoPrintf("Ignoring Action.EscapeWrite BACKDOOR_ACCESS on emulation/silicon.\n");
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPState
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPState::PmAction_SetPState
(
    const PmGpuDesc *pGpuDesc,
    UINT32           pstate,
    UINT32           fallback
) :
    PmAction("Action.SetPState"),
    m_pGpuDesc(pGpuDesc),
    m_PState(pstate),
    m_Fallback(fallback)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetPState::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Set the PState.
//!
/* virtual */ RC PmAction_SetPState::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC             rc = OK;
    GpuSubdevices  gpuSubdevs;
    Perf          *pPerf;
    bool           bValidPState;

    gpuSubdevs = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    // Loop through all subdevices
    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevs.begin();
         ppGpuSubdevice != gpuSubdevs.end(); ++ppGpuSubdevice)
    {
        pPerf = (*ppGpuSubdevice)->GetPerf();
        CHECK_RC(pPerf->DoesPStateExist(m_PState, &bValidPState));
        if (bValidPState)
        {
            UINT32 actualPState;

            CHECK_RC(pPerf->ForcePState(m_PState, m_Fallback));
            CHECK_RC(pPerf->GetLwrrentPState(&actualPState));

            InfoPrintf("Set PState on subdevice %d:%d : Desired=%d, Actual=%d\n",
                       (*ppGpuSubdevice)->GetParentDevice()->GetDeviceInst(), (*ppGpuSubdevice)->GetSubdeviceInst(), m_PState, actualPState);
        }
        else
        {
            ErrPrintf("Action.SetPState: Invalid PState %d on GPU %d:%d\n",
                      m_PState, (*ppGpuSubdevice)->GetParentDevice()->GetDeviceInst(), (*ppGpuSubdevice)->GetSubdeviceInst());
            return RC::BAD_PARAMETER;
        }
    }

    // Put a message in the log if there were no matches on the current regular
    // expression for subdevices
    if (gpuSubdevs.empty())
        InfoPrintf("No matching subdevices found for PState change (re: %s)\n",
                   m_pGpuDesc->ToString().c_str());

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_AbortTests
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_AbortTests::PmAction_AbortTests(PmTestDesc *pTestDesc) :
    PmAction("Action.AbortTests"),
    m_pTestDesc(pTestDesc)
{
    MASSERT(pTestDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_AbortTests::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pTestDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Abort the test associated with the channel.
//!
/* virtual */ RC PmAction_AbortTests::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmTests tests = m_pTestDesc->GetMatchingTests(pTrigger, pEvent, nullptr);
    RC rc;

    for (PmTests::iterator ppTest = tests.begin();
         ppTest != tests.end(); ++ppTest)
    {
        CHECK_RC((*ppTest)->Abort());
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_AbortTest
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_AbortTest::PmAction_AbortTest() :
    PmAction("Action.AbortTest")
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_AbortTest::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    if (!pTrigger->HasChannel())
    {
        ErrPrintf("Action.AbortTest: Can't use with this trigger (faulting channel required).\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Abort the test associated with the channel.
//!
/* virtual */ RC PmAction_AbortTest::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels = pEvent->GetChannels();
    GpuSubdevice * pGpuSubdevice = pEvent->GetGpuSubdevice();

    MASSERT(!channels.empty() || pGpuSubdevice);

    if (!channels.empty() )
    {
        bool aborted = false;
        for (PmChannels::iterator iter = channels.begin();
             iter != channels.end(); ++iter)
        {
            PmTest *pTest = (*iter)->GetTest();
            // No harmful to set abort to same test multiple times
            if (pTest)
            {
                CHECK_RC(pTest->Abort());
                aborted = true;
            }

        }
        if (aborted)
        {
            return rc;
        }
        else
        {
            for (PmChannels::iterator iter = channels.begin();
                 iter != channels.end(); ++iter)
            {
                InfoPrintf("No matching test found for channel %s\n",
                           (*iter)->GetName().c_str());
            }
        }
    }

    if (pGpuSubdevice)
    {
        // physical page fault which RM just know which subdevice hit the fault instead
        // of which channel hit this.
        PolicyManager * pPolicyManager = PolicyManager::Instance();
        PmTests pmTests = pPolicyManager->GetActiveTests();
        for (PmTests::iterator ppTest = pmTests.begin();
                ppTest != pmTests.end(); ++ppTest)
        {
            GpuDevice * pGpuDevice = (*ppTest)->GetGpuDevice();
            for (UINT32 i = 0; i < pGpuDevice->GetNumSubdevices(); ++i)
            {
                if (pGpuDevice->GetSubdevice(i) == pGpuSubdevice)
                {
                    return (*ppTest)->Abort();
                }
            }
        }

        InfoPrintf("No matching test found for sub device\n");
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WaitForSemaphoreRelease
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WaitForSemaphoreRelease::PmAction_WaitForSemaphoreRelease
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT32 offset,
    UINT64 payload,
    FLOAT64 timeoutMs
) :
    PmAction("Action.WaitForSemaphoreRelease"),
    m_PageDesc(pSurfaceDesc, offset, 4),
    m_Payload(payload),
    m_TimeoutMs(timeoutMs)
{
    MASSERT(pSurfaceDesc != NULL);
    m_pGpuDesc = pSurfaceDesc->GetGpuDesc();
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_WaitForSemaphoreRelease::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;
    if (m_pGpuDesc)
    {
        CHECK_RC(m_pGpuDesc->IsSupportedInAction(pTrigger));
    }
    CHECK_RC(m_PageDesc.IsSupportedInAction(pTrigger));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Wait for a an offset within the surfaces to be a specific
//!        value (i.e. semaphore release has oclwrred)
//!
/* virtual */ RC PmAction_WaitForSemaphoreRelease::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PollSemReleaseData semReleaseData;

    semReleaseData.memRanges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);
    if (m_pGpuDesc == NULL)
    {
        semReleaseData.subdevices = GetPolicyManager()->GetGpuSubdevices();
    }
    else
    {
        semReleaseData.subdevices = m_pGpuDesc->GetMatchingSubdevices(pTrigger,
                                                                      pEvent);
    }
    semReleaseData.payload      = m_Payload;

    CHECK_RC(POLLWRAP(PmAction_WaitForSemaphoreRelease::PollSemRelease,
                      &semReleaseData,
                      m_TimeoutMs));
    return semReleaseData.waitRc;
}

//--------------------------------------------------------------------
//! \brief Polling function to wait for a semaphore release on a surface
//!
/* static */ bool PmAction_WaitForSemaphoreRelease::PollSemRelease
(
    void *pvArgs
)
{
    PollSemReleaseData *pSemReleaseData =
        static_cast<PollSemReleaseData *>(pvArgs);
    PmMemRanges::iterator pMemRange = pSemReleaseData->memRanges.begin();

    while (pMemRange != pSemReleaseData->memRanges.end())
    {
        GpuSubdevices::iterator ppGpuSubdevice;

        ppGpuSubdevice = pSemReleaseData->subdevices.begin();
        while (ppGpuSubdevice != pSemReleaseData->subdevices.end())
        {
            GpuSubdevice *pSubdev = *ppGpuSubdevice;

            // Since (at least theoretically) there could be surfaces
            // allocated on different GpuDevices in the matching surfaces
            // it is necessary to ensure that the surface is present on
            // the current subdevice
            if (pSubdev->GetParentDevice() ==
                pMemRange->GetMdiagSurf()->GetGpuDev())
            {
                vector<UINT08> data;

                pSemReleaseData->waitRc = pMemRange->Read(&data, pSubdev);

                if (pSemReleaseData->waitRc != OK)
                    return true;

                UINT64 data64 = 0;
                // See bug 1937568. Sem surface or payload size can be
                // 4-byte or 8-byte.
                if (data.size() >= 8)
                {
                    data64 = *reinterpret_cast<UINT64*>(&data[0]);
                }
                else if (data.size() >= 4)
                {
                    data64 = *reinterpret_cast<UINT32*>(&data[0]);
                }
                else
                {
                    MASSERT(!"PollSemRelease, semaphore payload should be 4 or 8 bytes.\n");
                    return false;
                }

                if (data64 != pSemReleaseData->payload)
                    return false;
            }
            ++ppGpuSubdevice;
        }

        // Made it through the check for this PmMemRange so the data in this
        // range must be correct on all subdevices, go ahead and delete
        // it from the vector of ranges to be checked.  No need to increment
        // ppMemRange since erase returns an iterator to the next element
        pMemRange = pSemReleaseData->memRanges.erase(pMemRange);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FreezeHost
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FreezeHost::PmAction_FreezeHost
(
    const PmGpuDesc *pGpuDesc,
    FLOAT64 timeoutMs
):
    PmAction("Action.FreezeHost"),
    m_pGpuDesc(pGpuDesc),
    m_TimeoutMs(timeoutMs)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_FreezeHost::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Polling function to wait for Action.FreezeHost done on subGpuDevice
//!
/* static */ bool PmAction_FreezeHost::PollFreezeDone
(
    void* pvArgs
)
{
    GpuSubdevices  *pGpuSubdevices = static_cast<GpuSubdevices *>(pvArgs);

    while (!pGpuSubdevices->empty())
    {
        RegHal &regs = pGpuSubdevices->back()->Regs();
        if (regs.Test32(MODS_PFIFO_FREEZE_PENDING_TRUE))
        {
            return false;
        }
        pGpuSubdevices->pop_back();
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief freeze host via the LW_PFIFO_FREEZE register.
//!
/* virtual */ RC PmAction_FreezeHost::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.FreezeHost: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    // Freeze all matched gpuSubDevice
    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
        ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        (*ppGpuSubdevice)->Regs().Write32(MODS_PFIFO_FREEZE_HOST_ENABLED);
    }

    // Polling to wait all gpuSubDevice frozen
    CHECK_RC(POLLWRAP_HW(PmAction_FreezeHost::PollFreezeDone,
             &gpuSubdevices,
             m_TimeoutMs));

    return OK;
}

//--------------------------------------------------------------------
//! \brief Make sure all hosts are unfrozen after the test
//!
/* virtual */ RC PmAction_FreezeHost::EndTest()
{
    RC rc;

    GpuSubdevices gpuSubdevices = GetPolicyManager()->GetGpuSubdevices();
    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
         ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        (*ppGpuSubdevice)->Regs().Write32(MODS_PFIFO_FREEZE_HOST_DISABLED);
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UnFreezeHost
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UnFreezeHost::PmAction_UnFreezeHost
(
    const PmGpuDesc *pGpuDesc
):
    PmAction("Action.UnfreezeHost"),
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_UnFreezeHost::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief freeze host via the LW_PFIFO_FREEZE register.
//!
/* virtual */ RC PmAction_UnFreezeHost::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.UnFreezeHost: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
        ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        RegHal &regs = (*ppGpuSubdevice)->Regs();
        UINT32 regValue = regs.Read32(MODS_PFIFO_FREEZE);
        if (!regs.TestField(regValue, MODS_PFIFO_FREEZE_HOST_ENABLED))
        {
            ErrPrintf("FREEZE_HOST != ENABLED before Action.UnFreezeHost().\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        regs.SetField(&regValue, MODS_PFIFO_FREEZE_HOST_DISABLED);
        regs.Write32(MODS_PFIFO_FREEZE, regValue);
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ResetGpuDevice
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ResetGpuDevice::PmAction_ResetGpuDevice
(
    const string &Name,
    const PmGpuDesc *pGpuDesc,
    UINT32 lw0080CtrlBifResetType
) :
    PmAction(Name),
    m_pGpuDesc(pGpuDesc),
    m_lw0080CtrlBifResetType(lw0080CtrlBifResetType)
{
    MASSERT(m_pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ResetGpuDevice::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Call LW0080_CTRL_CMD_BIF_RESET on the GpuDevice
//!
/* virtual */ RC PmAction_ResetGpuDevice::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    RC rc;

    for (GpuDevices::iterator ppGpuDevice = gpuDevices.begin();
         ppGpuDevice != gpuDevices.end(); ++ppGpuDevice)
    {
        CHECK_RC((*ppGpuDevice)->Reset(m_lw0080CtrlBifResetType));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PhysWr32
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PhysWr32::PmAction_PhysWr32
(
    UINT64 physAddr,
    UINT32 data
):
    PmAction("Action.PhysWr32"),
    m_PhysAddr(physAddr),
    m_Data(data)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PhysWr32::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief write data to PRI registers.
//!
/* virtual */ RC PmAction_PhysWr32::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc = Platform::PhysWr(m_PhysAddr, &m_Data, sizeof(m_Data));

    Platform::FlushCpuWriteCombineBuffer();

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PriAccess
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PriAccess::PmAction_PriAccess
(
    const PmGpuDesc *pGpuDesc,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32> &RegIndexes,
    const string& RegSpace,
    const string& ActionName
) :
    PmAction(ActionName),
    m_pGpuDesc(pGpuDesc),
    m_pSmcEngineDesc(pSmcEngineDesc),
    m_pChannelDesc(pChannelDesc),
    m_PriRegName(RegName),
    m_PriRegIndexes(RegIndexes),
    m_PriRegSpace(RegSpace)
{
}

unique_ptr<PmRegister> PmAction_PriAccess::GetPmRegister
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    GpuSubdevice *pSubdev
)
{
    LWGpuResource *pGpuRes = GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);
    MASSERT(pGpuRes);

    UINT32 subdev = pSubdev->GetSubdeviceInst();

    PmSmcEngines smcEngines;
    if (m_pSmcEngineDesc)
        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
    MASSERT(smcEngines.size() <= 1);

    PmChannels channels;
    RegHalDomain domain = MDiagUtils::GetDomain(m_PriRegName);
    bool isPerRunlistReg = domain == RegHalDomain::RUNLIST || domain == RegHalDomain::CHRAM ;

    PmSmcEngine *pPmSmcEngine = smcEngines.size() ? smcEngines[0] : nullptr;

    // Deduce register space from channel
    if (m_PriRegSpace == "" && isPerRunlistReg)
    {
        if (!m_pChannelDesc)
        {
            ErrPrintf("For per runlist registers, either regSpace argument or Policy.SetTargetRunlist is needed.\n");
            MASSERT(0);
        }
        channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
        if (channels.size() != 1)
        {
            ErrPrintf("There should be only one matching channel for Policy.SetTargetRunlist.\n");
            MASSERT(0);
        }
        UINT32 engineId  = channels[0]->GetEngineId();
        m_PriRegSpace = "LW_ENGINE_" + MDiagUtils::Eng2Str(engineId);

        if (!pPmSmcEngine &&
            pGpuRes->IsSMCEnabled() &&
            MDiagUtils::IsGraphicsEngine(engineId))
        {
            pPmSmcEngine = GetPolicyManager()->GetMatchingSmcEngine(engineId, channels[0]->GetLwRmPtr());
            MASSERT(pPmSmcEngine);
        }
    }

    return make_unique<PmRegister>(pGpuRes, subdev, m_PriRegName, m_PriRegIndexes, pPmSmcEngine, m_PriRegSpace);
}

//////////////////////////////////////////////////////////////////////
// PmAction_PriWriteReg32
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PriWriteReg32::PmAction_PriWriteReg32
(
    const PmGpuDesc *pGpuDesc,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32>& RegIndexes,
    UINT32 RegVal,
    const string& RegSpace
):
    PmAction_PriAccess(pGpuDesc, pSmcEngineDesc, pChannelDesc, RegName, RegIndexes, RegSpace, "Action.PriWriteReg32"),
    m_RegVal(RegVal)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PriWriteReg32::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief write data to PRI registers.
//!
/* virtual */ RC PmAction_PriWriteReg32::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.PriWriteReg32: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    for (const auto& pSubdev : gpuSubdevices)
    {
        unique_ptr<PmRegister> pPmReg = GetPmRegister(pTrigger, pEvent, pSubdev);
        pPmReg->SetRegValue(m_RegVal);
        CHECK_RC(pPmReg->Write());
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PriWriteField32
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PriWriteField32::PmAction_PriWriteField32
(
    const PmGpuDesc *pGpuDesc,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32>& RegIndexes,
    string RegFieldName,
    UINT32 RegVal,
    const string& RegSpace
):
    PmAction_PriAccess(pGpuDesc, pSmcEngineDesc, pChannelDesc, RegName, RegIndexes, RegSpace, "Action.PriWriteField32"),
    m_FieldName(RegName+RegFieldName),
    m_FieldVal(RegVal)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PriWriteField32::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief write data to PRI registers field.
//!
/* virtual */ RC PmAction_PriWriteField32::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.PriWriteField32: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    for (const auto& pSubdev : gpuSubdevices)
    {
        unique_ptr<PmRegister> pPmReg = GetPmRegister(pTrigger, pEvent, pSubdev);
        CHECK_RC(pPmReg->SetField(m_FieldName, m_FieldVal));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PriWriteMask32
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_PriWriteMask32::PmAction_PriWriteMask32
(
    const PmGpuDesc *pGpuDesc,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32>& RegIndexes,
    UINT32 AndMask,
    UINT32 RegVal,
    const string& RegSpace
):
    PmAction_PriAccess(pGpuDesc, pSmcEngineDesc, pChannelDesc, RegName, RegIndexes, RegSpace, "Action.PriWriteField32"),
    m_AndMask(AndMask),
    m_RegVal(RegVal)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_PriWriteMask32::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief write data to PRI registers field.
//!
/* virtual */ RC PmAction_PriWriteMask32::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.PriWriteMask32: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    for (const auto& pSubdev : gpuSubdevices)
    {
        unique_ptr<PmRegister> pPmReg = GetPmRegister(pTrigger, pEvent, pSubdev);
        CHECK_RC(pPmReg->SetMasked(m_AndMask, m_RegVal));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WaitPriReg32
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WaitPriReg32::PmAction_WaitPriReg32
(
    const PmGpuDesc *pGpuDesc,
    const PmSmcEngineDesc *pSmcEngineDesc,
    const PmChannelDesc *pChannelDesc,
    const string& RegName,
    const vector<UINT32>& RegIndexes,
    UINT32 RegMask,
    UINT32 RegVal,
    FLOAT64 TimeoutMs,
    const string& RegSpace
):
    PmAction_PriAccess(pGpuDesc, pSmcEngineDesc, pChannelDesc, RegName, RegIndexes, RegSpace, "Action.WaitPriReg32"),
    m_RegMask(RegMask),
    m_RegVal(RegVal),
    m_TimeoutMs(TimeoutMs)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_WaitPriReg32::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief write data to PRI registers field.
//!
/* virtual */ RC PmAction_WaitPriReg32::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.WaitPriReg32: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }


    for (const auto& pSubdev : gpuSubdevices)
    {
        unique_ptr<PmRegister> pPmReg = GetPmRegister(pTrigger, pEvent, pSubdev);
        if (pPmReg->IsReadable())
        {
            m_WaitData.push_back(make_tuple(pPmReg->m_pGpuRes, pPmReg->m_Subdev, pPmReg->m_RegOffset, pPmReg->m_pPmSmcEngine));
        }
        else
        {
            ErrPrintf("Action.WaitPriReg32: Register %s is not readable.\n", m_PriRegName.c_str());
            return RC::BAD_PARAMETER;
        }
    }

    if (m_TimeoutMs == NO_WAIT)
    {
        if (PollRegs(this))
        {
            return OK;
        }
        else
        {
            return RC::TIMEOUT_ERROR;
        }
    }
    else
    {
        return POLLWRAP_HW(PmAction_WaitPriReg32::PollRegs, this, m_TimeoutMs);
    }
}

/* static */ bool PmAction_WaitPriReg32::PollRegs(void *pvArgs)
{
    UINT32                 lwrValue;
    PmAction_WaitPriReg32 *pThis;

    pThis = static_cast<PmAction_WaitPriReg32 *>(pvArgs);

    auto pWaitEntry = pThis->m_WaitData.begin();

    while (pWaitEntry != pThis->m_WaitData.end())
    {
        LWGpuResource *pGpuRes = nullptr;
        UINT32 subdev = 0;
        UINT32 regOffset = 0;
        PmSmcEngine *pPmSmcEngine = nullptr;
        std::tie(pGpuRes, subdev, regOffset, pPmSmcEngine) = *pWaitEntry;

        SmcEngine *pSmcEngine = pPmSmcEngine ? pPmSmcEngine->GetSmcEngine() : nullptr;
        lwrValue = pGpuRes->RegRd32(regOffset, subdev, pSmcEngine);

        if ((lwrValue & pThis->m_RegMask) == pThis->m_RegVal)
        {
            pWaitEntry = pThis->m_WaitData.erase(pWaitEntry);
        }
        else
        {
            pWaitEntry++;
        }
    }

    return pThis->m_WaitData.empty();
}

//////////////////////////////////////////////////////////////////////
// PmAction_StartTimer
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_StartTimer::PmAction_StartTimer
(
    const string &TimerName
):
    PmAction("Action.StartTimer"),
    m_TimerName(TimerName)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_StartTimer::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Start the indicated timer
//!
/* virtual */ RC PmAction_StartTimer::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PolicyManager *pPolicyManager = GetPolicyManager();
    RC rc;

    pPolicyManager->StartTimer(m_TimerName);

    PmEvent_StartTimer event(m_TimerName);
    CHECK_RC(pPolicyManager->GetEventManager()->HandleEvent(&event));

    return rc;
}

namespace
{
    //////////////////////////////////////////////////////////////////////
    // AliasPagesCheckSize
    //////////////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------
    //! \brief Passing NULL to pDstSurf when PmAction_AliasPages calling this.
    // pDstSurf is required by PmAction_AliasPagesFromSurface only.
    //!
    void AliasPagesCheckSize(UINT64 srcOffset, UINT64 dstOffset,
        UINT64 mapSize, PmSubsurface* pSrcSurf, PmSubsurface* pDstSurf)
    {
        bool validArg = true;

        MASSERT(pSrcSurf);

        // Check if offset valid.

        if (srcOffset >= pSrcSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid source offset 0x%llx. It isn't within the related sub surface which has size 0x%llx.\n",
                      srcOffset, pSrcSurf->GetSize());
            validArg = false;
        }
        if ((pDstSurf && dstOffset >= pDstSurf->GetSize()) ||
            dstOffset >= pSrcSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid dest offset 0x%llx. It isn't within the related sub surface which has size 0x%llx.\n",
                      dstOffset,
                      pDstSurf ? pDstSurf->GetSize() : pSrcSurf->GetSize());
            validArg = false;
        }

        // Check if mapping size smaller than surface size.

        if (mapSize > pSrcSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid aliasing size 0x%llx which is bigger than the source surface size 0x%llx.\n",
                      mapSize, pSrcSurf->GetSize());
            validArg = false;
        }
        if (pDstSurf && mapSize > pDstSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid aliasing size 0x%llx which is bigger than the dest sourface size 0x%llx.\n",
                      mapSize, pDstSurf->GetSize());
            validArg = false;
        }

        // Check if mapping size or any surface size is less than 1 page.

        PmMemRange memRange(pSrcSurf->GetSurface(),
            pSrcSurf->GetOffset(), pSrcSurf->GetSize());
        PmMemMappingsHelper *pHelper = memRange.GetSurface()->GetMemMappingsHelper();
        PmMemMappings mappings = pHelper->GetMemMappings(&memRange);
        UINT64 pageSize = mappings[0]->GetPageSize();
        if (pDstSurf)
        {
            memRange.Set(pDstSurf->GetSurface(),
                pDstSurf->GetOffset(), pDstSurf->GetSize());
            pHelper = memRange.GetSurface()->GetMemMappingsHelper();
            mappings.clear();
            mappings = pHelper->GetMemMappings(&memRange);
            pageSize = std::min(mappings[0]->GetPageSize(), pageSize);
        }

        if (pageSize > mapSize)
        {
            ErrPrintf("AliasPages: Aliasing size 0x%llx is invalid. It can't be less than 1 page (page size 0x%llx).\n",
                      mapSize, pageSize);
            validArg = false;
        }
        if (pageSize > pSrcSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid source surface size 0x%llx. Surface can't be smaller than 1 page (page size 0x%llx).\n",
                      pSrcSurf->GetSize(), pageSize);
            validArg = false;
        }
        if (pDstSurf && pageSize > pDstSurf->GetSize())
        {
            ErrPrintf("AliasPages: Invalid dest sourface size 0x%llx. Surface can't be smaller than 1 page (page size 0x%llx).\n",
                      pDstSurf->GetSize(), pageSize);
            validArg = false;
        }
        MASSERT(validArg);
        // The following stops compiler's complain:
        //      "error: variable 'validArg' set but not used [-Werror=unused-but-set-variable]"
        // in release build.
        (void)validArg;
    }
}

//////////////////////////////////////////////////////////////////////
// PmAction_AliasPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_AliasPages::PmAction_AliasPages
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 aliasDstOffset,
    UINT64 size,
    UINT64 aliasSrcOffset,
    bool deferTlbIlwalidate
) :
    PmAction("Action.AliasPages"),
    m_pSurfaceDesc(pSurfaceDesc),
    m_AliasDstOffset(aliasDstOffset),
    m_AliasSrcOffset(aliasSrcOffset),
    m_Size(size),
    m_DeferTlbIlwalidate(deferTlbIlwalidate)
{
    MASSERT(m_pSurfaceDesc != 0);
    MASSERT(m_AliasSrcOffset != m_AliasDstOffset);
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(pSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_AliasPages::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}
//--------------------------------------------------------------------
//! \brief Remap the page table entries for the alias destination
// to point to the physical memory lwrrently mapped to the alias source
//!
/* virtual */ RC PmAction_AliasPages::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc = OK;

    PmSubsurfaces subsurfaces = m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    // For each matching subsurface, retrieve memory mappings for the range
    // to be remapped and the alias source range whose mappings will be matched
    // Iterate through mappings, splitting the alias destination mapping where
    // necessary and remapping the pte's
    for ( PmSubsurfaces::iterator pSubsurface = subsurfaces.begin();
         pSubsurface != subsurfaces.end(); pSubsurface++ )
    {
        AliasPagesCheckSize(m_AliasSrcOffset, m_AliasDstOffset,
            m_Size, *pSubsurface, NULL);

        CHECK_RC(PmMemRange::AliasPhysMem(*pSubsurface, *pSubsurface,
                m_AliasSrcOffset, m_AliasDstOffset, m_Size, m_DeferTlbIlwalidate));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_AliasPagesFromSurface
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_AliasPagesFromSurface::PmAction_AliasPagesFromSurface
(
    const PmSurfaceDesc *pDestSurfaceDesc,
    UINT64 aliasDstOffset,
    UINT64 size,
    const PmSurfaceDesc *pSourceSurfaceDesc,
    UINT64 aliasSrcOffset,
    bool deferTlbIlwalidate
) :
    PmAction("Action.AliasPagesFromSurface"),
    m_pDestSurfaceDesc(pDestSurfaceDesc),
    m_AliasDstOffset(aliasDstOffset),
    m_pSourceSurfaceDesc(pSourceSurfaceDesc),
    m_AliasSrcOffset(aliasSrcOffset),
    m_Size(size),
    m_DeferTlbIlwalidate(deferTlbIlwalidate)
{
    MASSERT(m_pDestSurfaceDesc != 0);
    MASSERT(m_pSourceSurfaceDesc != 0);
    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    pPolicyMgr->SetUpdateSurfaceInMmu(m_pDestSurfaceDesc->GetRegName());
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_AliasPagesFromSurface::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_pDestSurfaceDesc->IsSupportedInAction(pTrigger));
    CHECK_RC(m_pSourceSurfaceDesc->IsSupportedInAction(pTrigger));

    return rc;
}
//--------------------------------------------------------------------
//! \brief Remap the page table entries for the alias destination
// to point to the physical memory lwrrently mapped to the alias source
//!
/* virtual */ RC PmAction_AliasPagesFromSurface::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc = OK;

    PmSubsurfaces sourceSubsurfaces =
        m_pSourceSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);
    PmSubsurfaces destSubsurfaces =
        m_pDestSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    if (destSubsurfaces.size() == 0 || sourceSubsurfaces.size() == 0)
    {
        WarnPrintf("Can't find the surface in Action.AliasPagesFromSurface.Ignoring action.\n");
    }

    if ((sourceSubsurfaces.size() != destSubsurfaces.size()) &&
        (sourceSubsurfaces.size() != 1))
    {
        ErrPrintf("Action.AliasPagesFromSurface: The source surface list of this action must either have the same "
                  "number of surfaces has the destination surface list or it must have only one surface.");
        return RC::BAD_PARAMETER;
    }

    PmSubsurfaces::iterator pSourceSubsurface = sourceSubsurfaces.begin();
    PmSubsurfaces::iterator pDestSubsurface = destSubsurfaces.begin();

    // For each matching subsurface, retrieve memory mappings for the range
    // to be remapped and the alias source range whose mappings will be matched
    // Iterate through mappings, splitting the alias destination mapping where
    // necessary and remapping the pte's
    while (pDestSubsurface != destSubsurfaces.end())
    {
        AliasPagesCheckSize(m_AliasSrcOffset, m_AliasDstOffset,
            m_Size, *pSourceSubsurface, *pDestSubsurface);

        CHECK_RC(PmMemRange::AliasPhysMem(*pSourceSubsurface, *pDestSubsurface,
                m_AliasSrcOffset, m_AliasDstOffset, m_Size, m_DeferTlbIlwalidate));

        ++pSourceSubsurface;
        ++pDestSubsurface;

        // If the list of source subsurfaces is shorter than the list
        // of destination subsurfaces, wrap around to the beginning
        // of the list of source subsurfaces.  This is so we can handle
        // two valid test cases.  1) Aliasing a list of N subsurfaces
        // to a list of N other subsurfaces, one-to-one; and 2) aliasing
        // a list of N subsurfaces to 1 subsurface.
        if (pSourceSubsurface == sourceSubsurfaces.end())
        {
            pSourceSubsurface = sourceSubsurfaces.begin();
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FillSurface
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FillSurface::PmAction_FillSurface
(
    PmSurfaceDesc     *pSurfaceDesc,
    const FancyPicker& offset,
    UINT32             size,
    const FancyPicker& data,
    bool               isInband,
    PmChannelDesc     *pInbandChannel
) :
    PmAction("Action.FillSurface"),
    m_pSurfaceDesc(pSurfaceDesc),
    m_OffsetPicker(offset),
    m_Size(size),
    m_DataPicker(data),
    m_IsInband(isInband),
    m_pInbandChannel(pInbandChannel)
{
    MASSERT(m_pSurfaceDesc != NULL);
    MASSERT((m_Size & 0x03) == 0);

    m_FpContext.LoopNum    = 0;
    m_FpContext.RestartNum = 0;
    m_FpContext.pJSObj     = 0;

    m_DataPicker.SetContext(&m_FpContext);
    m_OffsetPicker.SetContext(&m_FpContext);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_FillSurface::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    CHECK_RC(m_pSurfaceDesc->IsSupportedInAction(pTrigger));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Add entries to the indicated surface table
//!
/* virtual */ RC PmAction_FillSurface::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSubsurfaces surfaceVec = m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);
    GpuSubdevices gpuSubdevices = GetPolicyManager()->GetGpuSubdevices();

    UINT32 value = m_DataPicker.Pick();
    UINT32 offset = m_OffsetPicker.Pick();

    for (PmSubsurfaces::iterator pSurface = surfaceVec.begin();
         pSurface != surfaceVec.end(); ++pSurface)
    {
        // Fill data vector from picker
        vector<UINT08> dataVec;
        for (UINT32 i = 0; i < (m_Size / sizeof(UINT32)); i++)
        {
            dataVec.push_back((UINT08)value);
            dataVec.push_back((UINT08)(value >> 8));
            dataVec.push_back((UINT08)(value >> 16));
            dataVec.push_back((UINT08)(value >> 24));
        }

        for (GpuSubdevices::iterator pSubdev = gpuSubdevices.begin();
             pSubdev != gpuSubdevices.end(); ++pSubdev)
        {
            PmMemRange writeRange((*pSurface)->GetSurface(), offset + (*pSurface)->GetOffset(), m_Size);

            if (m_IsInband)
            {
                PmChannel *pInbandChannel =
                    GetInbandChannel(m_pInbandChannel, pTrigger, pEvent, &writeRange);
                writeRange.GpuInbandPhysFill(value, (*pSubdev)->GetSubdeviceInst(), pInbandChannel);
            }
            else
            {
                writeRange.Write(&dataVec, *pSubdev);
            }
        }
    }

    ++m_FpContext.LoopNum;

    // Flush the write combine buffer to immediately write value to memory
    Platform::FlushCpuWriteCombineBuffer();

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetCpuMappingMode
//////////////////////////////////////////////////////////////////////

PmAction_SetCpuMappingMode::PmAction_SetCpuMappingMode
(
    const string &Name,
    const PmSurfaceDesc *pSurfaceDesc,
    PolicyManager::CpuMappingMode mappingMode
) :
    PmAction(Name),
    m_pSurfaceDesc(pSurfaceDesc),
    m_CpuMappingMode(mappingMode)
{
}

RC PmAction_SetCpuMappingMode::IsSupported(const PmTrigger *pTrigger) const
{
    RC rc;

    CHECK_RC(m_pSurfaceDesc->IsSupportedInAction(pTrigger));

    return rc;
}

RC PmAction_SetCpuMappingMode::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    PmSubsurfaces surfaceVec = m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger, pEvent);

    for (PmSubsurfaces::iterator pSurface = surfaceVec.begin();
        pSurface != surfaceVec.end(); ++pSurface)
    {
        PmSurface *pPmSurface = (*pSurface)->GetSurface();
        pPmSurface->SetCpuMappingMode(m_CpuMappingMode);
    }

    return OK;
}

PmAction_CpuMapSurfacesDirect::PmAction_CpuMapSurfacesDirect
(
    const PmSurfaceDesc *pSurfaceDesc
) :
    PmAction_SetCpuMappingMode
    (
        "Action.CpuMapSurfacesDirect",
        pSurfaceDesc,
        PolicyManager::CpuMapDirect
    )
{
}

PmAction_CpuMapSurfacesReflected::PmAction_CpuMapSurfacesReflected
(
    const PmSurfaceDesc *pSurfaceDesc
) :
    PmAction_SetCpuMappingMode
    (
        "Action.CpuMapSurfacesReflected",
        pSurfaceDesc,
        PolicyManager::CpuMapReflected
    )
{
}

PmAction_CpuMapSurfacesDefault::PmAction_CpuMapSurfacesDefault
(
    const PmSurfaceDesc *pSurfaceDesc
) :
    PmAction_SetCpuMappingMode
    (
        "Action.CpuMapSurfacesDefault",
        pSurfaceDesc,
        PolicyManager::CpuMapDefault
    )
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_GpuSetClock
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_GpuSetClock::PmAction_GpuSetClock
(
    const PmGpuDesc *pGpuDesc,
    string           clkdmnName,
    UINT32           targetHz
):
    PmAction("Action.GpuSetClock"),
    m_pGpuDesc(pGpuDesc),
    m_TargetHz(targetHz)
{
    MASSERT(pGpuDesc != 0);
    m_ClkdmnName.Set(clkdmnName, RegExp::FULL | RegExp::ICASE);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_GpuSetClock::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Change clk in specified clk domain
//!
/* virtual */ RC PmAction_GpuSetClock::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
    Perf          *pPerf;
    UINT32         actualPState;

    // Loop all matched gpuSubDevice and clkdomain
    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
        ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        ClkDomains matchingClkDmns = GetMatchingClkDomains(*ppGpuSubdevice);

        if ( matchingClkDmns.size() == 0)
        {
            WarnPrintf("Action.GpuSetClock: No matched clkdomains found on the gpu.\n");
        }

        pPerf = (*ppGpuSubdevice)->GetPerf();

        for (UINT32 i = 0; i < matchingClkDmns.size(); i++)
        {
            CHECK_RC(pPerf->GetLwrrentPState(&actualPState));
            CHECK_RC((*ppGpuSubdevice)->SetClock(matchingClkDmns[i],
                m_TargetHz,
                actualPState,
                0));
        }
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Return a list of Gpu::ClkDomain on specified gpusubdev
//!  whose name matches the regular expresion of m_ClkdmnName
//!
PmAction_GpuSetClock::ClkDomains PmAction_GpuSetClock::GetMatchingClkDomains
(
    const GpuSubdevice* pGpuSubdevice
)
{
    ClkDomains matchingClkDomains;
    for (UINT32 i = Gpu::ClkUnkUnsp; i < Gpu::ClkDomain_NUM; i++)
    {
        Gpu::ClkDomain cd = (Gpu::ClkDomain)i;
        const char* pClkDmnName = PerfUtil::ClkDomainToStr(cd);
        if (pClkDmnName && m_ClkdmnName.Matches(pClkDmnName) &&
            pGpuSubdevice->HasDomain(cd))
        {
            matchingClkDomains.push_back(cd);
        }
    }

    return matchingClkDomains;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbSetState
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbSetState::PmAction_FbSetState
(
    const string    &Name,
    const PmGpuDesc *pGpuDesc,
    UINT32           state
) :
    PmAction(Name)
    #ifdef LW_VERIF_FEATURES
        ,
        m_pGpuDesc(pGpuDesc),
        m_State(state)
    #endif
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_FbSetState::IsSupported
(
    const PmTrigger *pTrigger
) const
{
#ifdef LW_VERIF_FEATURES
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
#else
    ErrPrintf("Action.StopFb/RestartFb: Require LW_VERIF_FEATURES\n");
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//--------------------------------------------------------------------
//! \brief Put the fb engine in the requested state
//!
/* virtual */ RC PmAction_FbSetState::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
#ifdef LW_VERIF_FEATURES
    LwRmPtr pLwRm;
    RC rc;

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
         ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        LW208F_CTRL_FB_SET_STATE_PARAMS params = {0};
        params.state = m_State;

        CHECK_RC(pLwRm->Control((*ppGpuSubdevice)->GetSubdeviceDiag(),
                            LW208F_CTRL_CMD_FB_SET_STATE,
                            &params,
                            sizeof(params)));
    }

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//////////////////////////////////////////////////////////////////////
// PmAction_EnableElpg
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_EnableElpg::PmAction_EnableElpg
(
    const string &Name,
    const PmGpuDesc *pGpuDesc,
    UINT32 EngineId,
    bool ToEnableElpg
) :
    PmAction(Name),
    m_pGpuDesc(pGpuDesc),
    m_EngineId(EngineId),
    m_ToEnableElpg(ToEnableElpg)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_EnableElpg::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Put the fb engine in the requested state
//!
/* virtual */ RC PmAction_EnableElpg::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    GpuSubdevices gpuSubdev = m_pGpuDesc->GetMatchingSubdevices(pTrigger,
                                                                pEvent);
    for (GpuSubdevices::iterator ppGpuSubdev = gpuSubdev.begin();
         ppGpuSubdev != gpuSubdev.end(); ++ppGpuSubdev)
    {
        PMU *pPmu = NULL;
        CHECK_RC((*ppGpuSubdev)->GetPmu(&pPmu));

        vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
        LW2080_CTRL_POWERGATING_PARAMETER Param = {0};
        Param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        Param.parameterExtended = m_EngineId;
        UINT32 ParamValue = 0;
        if (m_ToEnableElpg)
            ParamValue = 1;
        Param.parameterValue = ParamValue;
        pgParams.push_back(Param);
        UINT32 Flags = 0;
        Flags = FLD_SET_DRF(2080, _CTRL_MC_SET_POWERGATING_THRESHOLD,
                            _FLAGS_BLOCKING,
                            _TRUE, Flags);
        CHECK_RC(pPmu->SetPowerGatingParameters(&pgParams, Flags));
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_GpuRailGateAction
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_GpuRailGateAction::PmAction_GpuRailGateAction
(
    const string    &Name,
    const PmGpuDesc *pGpuDesc,
    UINT32           action
) :
    PmAction(Name)
    #ifdef LW_VERIF_FEATURES
        ,
        m_pGpuDesc(pGpuDesc),
        m_Action(action)
    #endif
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_GpuRailGateAction::IsSupported
(
    const PmTrigger *pTrigger
) const
{
#ifdef LW_VERIF_FEATURES
    if ((m_Action != LW2080_CTRL_GPU_POWER_ON_OFF_RG_RESTORE) &&
        (m_Action != LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE))
    {
        ErrPrintf("Action.RailGateGpu/UnRailGateGpu: Unknown action(%d), neither railgate nor unrailgate",
                  m_Action);
        return RC::UNSUPPORTED_FUNCTION;
    }

    return m_pGpuDesc->IsSupportedInAction(pTrigger);
#else
    ErrPrintf("Action.RailGateGpu/UnRailGateGpu: Require LW_VERIF_FEATURES\n");
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//--------------------------------------------------------------------
//! \brief perform railgate on gpu
//!
RC PmAction_GpuRailGateAction::RailGateGpu(GpuSubdevice *pGpuSubdevice)
{
    //
    // T124 fullchip has different interface to do railgate as gk20a
    // standalone does.
    //
    if (pGpuSubdevice->IsSOC())
    {
        CheetAh::SocPtr()->DisableModuleClocks(CheetAh::CLK_MODULE_3D);
    }
    else
    {
        // GK20a standalone
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief perform unrailgate on gpu
//!
RC PmAction_GpuRailGateAction::UnRailGateGpu(GpuSubdevice *pGpuSubdevice)
{
    //
    // T124 fullchip has different interface to do unrailgate as gk20a
    // standalone does.
    //
    if (pGpuSubdevice->IsSOC())
    {
        CheetAh::SocPtr()->EnableModuleClocks(CheetAh::CLK_MODULE_3D);
    }
    else
    {
        // GK20a standalone
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief perform ctx save/restore and railgate/unrailgate on gpu
//!
/* virtual */ RC PmAction_GpuRailGateAction::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
#ifdef LW_VERIF_FEATURES
    LwRmPtr pLwRm;
    RC rc;

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
         ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        // Un-railgate before RM restore
        if (m_Action == LW2080_CTRL_GPU_POWER_ON_OFF_RG_RESTORE)
        {
            CHECK_RC(UnRailGateGpu(*ppGpuSubdevice));
        }

        // Send ctx save/restore command to RM
        LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS params = {0};
        params.action = m_Action;

        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(*ppGpuSubdevice),
                            LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
                            &params,
                            sizeof(params)));

        // Railgate after RM save
        if (m_Action == LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE)
        {
            CHECK_RC(RailGateGpu(*ppGpuSubdevice));
        }
    }

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetGpioUp
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetGpioUp::PmAction_SetGpioUp
(
    const PmChannelDesc *pChannelDesc,
    const vector<UINT32> gpioPorts
) :
    PmAction("Action.SetGpioUp"),
    m_pChannelDesc(pChannelDesc),
    m_GpioPorts(gpioPorts)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetGpioUp::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Set gpio ports up
//!
/* virtual */ RC PmAction_SetGpioUp::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
        ppChannel != channels.end(); ++ppChannel)
    {
        PM_BEGIN_INSERTED_METHODS(*ppChannel);
        CHECK_RC(SetGpioDirection(m_pChannelDesc, *ppChannel, m_GpioPorts, GPIO_UP));
        PM_END_INSERTED_METHODS();
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetGpioDown
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetGpioDown::PmAction_SetGpioDown
(
    const PmChannelDesc *pChannelDesc,
    const vector<UINT32> gpioPorts
) :
    PmAction("Action.SetGpioDown"),
    m_pChannelDesc(pChannelDesc),
    m_GpioPorts(gpioPorts)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetGpioDown::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Set gpio ports down
//!
/* virtual */ RC PmAction_SetGpioDown::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmChannels channels =
        m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    for (PmChannels::iterator ppChannel = channels.begin();
        ppChannel != channels.end(); ++ppChannel)
    {
        PM_BEGIN_INSERTED_METHODS(*ppChannel);
        CHECK_RC(SetGpioDirection(m_pChannelDesc, *ppChannel, m_GpioPorts, GPIO_DOWN));
        PM_END_INSERTED_METHODS();
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_CreateChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_CreateChannel::PmAction_CreateChannel
(
    const string &channelName,
    const PmGpuDesc *pGpuDesc,
    const PmTestDesc *pTestDesc,
    const PmVaSpaceDesc *pVaSpaceDesc,
    const string &engineName,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction("Action.CreateChannel"),
    m_Name(channelName),
    m_pGpuDesc(pGpuDesc),
    m_pTestDesc(pTestDesc),
    m_pVaSpaceDesc(pVaSpaceDesc),
    m_EngineName(engineName),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(m_Name.size() > 0);
    MASSERT(m_pGpuDesc != 0);
    MASSERT(m_pTestDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_CreateChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Create the indicated channel(s).
//!
/* virtual */ RC PmAction_CreateChannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmVaSpaces vaspaces;

    PolicyManager *pPolicyManager = GetPolicyManager();
    GpuDevices gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);
    PmTests tests = m_pTestDesc->GetMatchingTests(pTrigger, pEvent, m_pSmcEngineDesc);

    for (PmTests::iterator testIter = tests.begin();
            testIter != tests.end();
            ++testIter)
    {
        PmActionUtility::MatchingVaSpace vaspaceObj(*testIter, m_pVaSpaceDesc);
        CHECK_RC(vaspaceObj.GetVaSpaces(pTrigger, pEvent, &vaspaces, m_pSmcEngineDesc));
    }

    for (GpuDevices::iterator gpuDeviceIter = gpuDevices.begin();
            gpuDeviceIter != gpuDevices.end();
            ++gpuDeviceIter)
    {
        for (PmVaSpaces::iterator vaspaceIter = vaspaces.begin();
                vaspaceIter != vaspaces.end(); ++vaspaceIter)
        {
            LwRm* pLwRm = (*vaspaceIter)->GetLwRmPtr();

            auto pPmChannel = new PmChannel_Mods(
                    pPolicyManager, NULL /*channel created by PM is shared among all pmtests*/,
                    *gpuDeviceIter, m_Name,
                    true, *vaspaceIter, pLwRm);

            CHECK_RC(pPolicyManager->AddChannel(pPmChannel));

            //TODO: support multiple sub device
            MASSERT((*gpuDeviceIter)->GetNumSubdevices() == 1);
            UINT32 engineId = LW2080_ENGINE_TYPE_NULL;

            // If before Ampere then engineId not required for channel creation
            if (!((*gpuDeviceIter)->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID)))
            {
                CHECK_RC(pPmChannel->RealCreateChannel(engineId));
            }
            // Else if the user has provided a hint for the channel type (channel w/o subchannels)
            // then get the engineId from the hint
            else if (!m_EngineName.empty())
            {
                if (m_EngineName == "graphics")
                {
                    PmSmcEngines smcEngines;
                    if (m_pSmcEngineDesc)
                    {
                        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
                        MASSERT(smcEngines.size() != 0);
                    }
                    MASSERT(smcEngines.size() <= 1);
                    engineId = MDiagUtils::GetGrEngineId(0);
                    pPmChannel->SetPmSmcEngine(smcEngines.size() ? smcEngines[0] : nullptr);
                }
                else if (m_EngineName.compare(0, 4, "copy") == 0)
                    CHECK_RC(PolicyManager::Instance()->GetCEEngineTypeByIndex(0, true, &engineId));
                else
                    MASSERT(!"Only Graphics and CE channels creation supported in Policy Manager");
                CHECK_RC(pPmChannel->RealCreateChannel(engineId));
            }
            // else defer the channel creation to subchannel since then is when engineId
            // can be deciphered
            {
                DebugPrintf("%s: The channel's (%s) creation has been deferred to subchannel to deduce the engineId\n",
                            __FUNCTION__, m_Name.c_str());
            }
        }
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_CreateSubchannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_CreateSubchannel::PmAction_CreateSubchannel
(
    const PmChannelDesc *pChannelDesc,
    const UINT32 hwClass,
    const UINT32 subchannelNumber,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction("Action.CreateSubchannel"),
    m_pChannelDesc(pChannelDesc),
    m_HwClass(hwClass),
    m_SubchannelNumber(subchannelNumber),
    m_pSmcEngineDesc(pSmcEngineDesc)

{
    MASSERT(pChannelDesc != 0);
}

PmAction_CreateSubchannel::PmAction_CreateSubchannel
(
    const PmChannelDesc *pChannelDesc,
    const string& actionName,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction(actionName),
    m_pChannelDesc(pChannelDesc),
    m_HwClass(~0),
    m_SubchannelNumber(~0),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(pChannelDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_CreateSubchannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Create the indicated channel(s).
//!
/* virtual */ RC PmAction_CreateSubchannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger,
                                                              pEvent);

    PmSmcEngines smcEngines;
    if (m_pSmcEngineDesc)
    {
        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
        MASSERT(smcEngines.size() != 0);
    }
    MASSERT(smcEngines.size() <= 1);

    for (PmChannels::iterator channelIter = channels.begin();
         channelIter != channels.end();
         ++channelIter)
    {
        MASSERT((*channelIter)->GetSubchannelClass(m_SubchannelNumber) == 0);
        if (!(*channelIter)->GetModsChannel())
        {
            UINT32 engineId = MDiagUtils::GetGrEngineId(0);
            (*channelIter)->SetPmSmcEngine(smcEngines.size() ? smcEngines[0] : nullptr);
            (*channelIter)->RealCreateChannel(engineId);
        }
        CHECK_RC(CreateSubchannel(*channelIter, NULL));
    }

    return rc;
}

RC PmAction_CreateSubchannel::CreateSubchannel
(
    PmChannel* pPmChannel,
    void * objParams
)
{
    RC rc;

    MASSERT((UINT32)~0 != m_HwClass);
    MASSERT((UINT32)~0 != m_SubchannelNumber);

    Channel *pChannel = pPmChannel->GetModsChannel();
    LwRm::Handle subchannelHandle;

    const string scopeName = Utility::StrPrintf("%s Alloc subchannel number %d class 0x%x on channel 0x%x",
                                                pPmChannel->GetTest() ? pPmChannel->GetTest()->GetTestName() : "",
                                                m_SubchannelNumber, m_HwClass, pChannel->GetChannelId());
    ChiplibOpScope newScope(scopeName, NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    rc = pPmChannel->GetLwRmPtr()->Alloc(pChannel->GetHandle(), &subchannelHandle,
                      m_HwClass, objParams);

    if (OK != rc)
    {
        ErrPrintf("Action.CreateSubchannel: Unable to allocate subchannel class 0x%x for channel %u: %s\n",
                  m_HwClass, pChannel->GetChannelId(), rc.Message());
        return rc;
    }

    pPmChannel->AddSubchannelHandle(subchannelHandle);

    rc = pChannel->ScheduleChannel(true);

    if (OK != rc)
    {
        ErrPrintf("Action.CreateSubchannel: Failed to schedule channel %u.\n",
                  pChannel->GetChannelId());
        return rc;
    }

    rc = pChannel->SetObject(m_SubchannelNumber, subchannelHandle);

    if (OK != rc)
    {
        ErrPrintf("Action.CreateSubchannel: Can't set subchannel number %u on subchannel with class 0x%x.\n",
                  m_SubchannelNumber, m_HwClass);
        return rc;
    }

    CHECK_RC(pPmChannel->SetSubchannelClass(m_SubchannelNumber, m_HwClass));

    return OK;
}

PmAction_CreateCESubchannel::PmAction_CreateCESubchannel
(
     const PmChannelDesc       *pChannelDesc,
     UINT32                     ceAllocInstNum,
     PolicyManager::CEAllocMode allocMode,
     const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction_CreateSubchannel(pChannelDesc, "Action.CreateCESubchannel", pSmcEngineDesc),
    m_CEAllocInstNum(ceAllocInstNum),
    m_CEAllocMode(allocMode)
{
}

RC PmAction_CreateCESubchannel::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    PmChannels channels = GetPmChannelDesc()->GetMatchingChannels(pTrigger,
                                                                  pEvent);

    PmSmcEngines smcEngines;
    if (m_pSmcEngineDesc)
    {
        smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
        MASSERT(smcEngines.size() != 0);
    }
    MASSERT(smcEngines.size() <= 1);

    for (PmChannels::iterator channelIter = channels.begin();
         channelIter != channels.end();
         ++channelIter)
    {
        LwRm* pLwRm = (*channelIter)->GetLwRmPtr();
        // get subchannel class

        UINT32 subChClass;
        EngineClasses::GetFirstSupportedClass(
                pLwRm,
                (*channelIter)->GetGpuDevice(),
                "Ce",
                &subChClass);

        // get subchannel number and object parameters
        UINT32 subChNum = 0;
        vector<UINT08> objParameters;
        UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
        bool IsGrCE = false;
        CHECK_RC(GetSubchCreationParams(*channelIter, &objParameters,
                                        subChClass, &subChNum,
                                        &engineId, &IsGrCE));

        if (!(*channelIter)->GetModsChannel())
        {
            if (IsGrCE)
            {
                UINT32 engineId = MDiagUtils::GetGrEngineId(0);
                (*channelIter)->SetPmSmcEngine(smcEngines.size() ? smcEngines[0] : nullptr);
                (*channelIter)->RealCreateChannel(engineId);
            }
            else
            {
                MASSERT(engineId != LW2080_ENGINE_TYPE_NULL);
                (*channelIter)->RealCreateChannel(engineId);
            }
        }

        void *params = objParameters.size() > 0?
                       (void*)(&objParameters[0]): NULL;

        // check this subChNum in this channel whether has been set
        if((*channelIter)->GetSubchannelClass(subChNum) == subChClass)
        {
            DebugPrintf("%s: This channel %s which subChannel %d has been registeted with class 0x%08x.\n",
                        __FUNCTION__, (*channelIter)->GetName().c_str(), subChNum, subChClass);
            continue;
        }

        m_SubchannelNumber = subChNum;
        m_HwClass = subChClass;
        CHECK_RC(CreateSubchannel(*channelIter, params));
    }

    return rc;
}

RC PmAction_CreateCESubchannel::GetSubchCreationParams
(
    PmChannel *pPmChannel,
    vector<UINT08> *objParams,
    UINT32 subchClass,
    UINT32 *pSubChNum,
    UINT32 *engineId,
    bool *IsGrCE
)
{
    RC rc;

    if (NULL == objParams)
    {
        return RC::ILWALID_ARGUMENT;
    }

    objParams->clear();

    if (EngineClasses::IsClassType("Ce", subchClass))
    {
        if (pPmChannel->HasGraphicObjClass() ||
            pPmChannel->HasComputeObjClass())
        {
            objParams->resize(sizeof(LWA0B5_ALLOCATION_PARAMETERS));
            LWA0B5_ALLOCATION_PARAMETERS* pClassParam =
                reinterpret_cast<LWA0B5_ALLOCATION_PARAMETERS*>(&(*objParams)[0]);

            // Get first GrCE
            pClassParam->version = LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
            CHECK_RC(PolicyManager::Instance()->
                GetCEEngineTypeByIndex(0, false, &pClassParam->engineType));

            *pSubChNum = LWA06F_SUBCHANNEL_COPY_ENGINE;
            *IsGrCE = true;
        }
        else
        {
            UINT32 ceInstance = 0;
            *pSubChNum = 0; // any number between 0 ~ 4
            if (m_CEAllocMode == PolicyManager::CEAllocAsync)
            {
                ceInstance = m_CEAllocInstNum;
            }
            CHECK_RC(PolicyManager::Instance()->GetCEEngineTypeByIndex(
                ceInstance, true, engineId));
            objParams->resize(sizeof(LW85B5_ALLOCATION_PARAMETERS));
            LW85B5_ALLOCATION_PARAMETERS* pClassParam =
                reinterpret_cast<LW85B5_ALLOCATION_PARAMETERS*>(&(*objParams)[0]);
            pClassParam->engineInstance = MDiagUtils::GetCopyEngineOffset(*engineId);
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetUpdateFaultBufferGetPointer
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetUpdateFaultBufferGetPointer::PmAction_SetUpdateFaultBufferGetPointer
(
    bool flag
)
:
    PmAction("Action.SetUpdateFaultBufferGetPointer"),
    m_Flag(flag)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetUpdateFaultBufferGetPointer::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Enable/disable the updating of the fault event buffer get pointer.
//!
/* virtual */ RC PmAction_SetUpdateFaultBufferGetPointer::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    GetPolicyManager()->SetUpdateFaultBufferGetPointer(m_Flag);
    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearFaultBuffer
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearFaultBuffer::PmAction_ClearFaultBuffer
(
    const PmGpuDesc *pGpuDesc
) :
    PmAction("Action.ClearFaultBuffer"),
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ClearFaultBuffer::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pGpuDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Enable/disable the updating of the fault event buffer get pointer.
//!
/* virtual */ RC PmAction_ClearFaultBuffer::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    for (GpuSubdevices::iterator gpuSubdeviceIter = gpuSubdevices.begin();
         gpuSubdeviceIter != gpuSubdevices.end();
         ++gpuSubdeviceIter)
    {
        GetPolicyManager()->ClearFaultBuffer(*gpuSubdeviceIter);
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetAtsPermission
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetAtsPermission::PmAction_SetAtsPermission
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    string permissionType,
    bool permissiolwalue
) :
    PmAction("Action.SetAtsPermission"),
    m_PageDesc(pSurfaceDesc, offset, size),
    m_PermissionType(permissionType),
    m_Permissiolwalue(permissiolwalue)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetAtsPermission::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Set the permission for the indicated ATS page(s).
//!
/* virtual */ RC PmAction_SetAtsPermission::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.SetAtsPermission: No matching surface ranges found\n");
        return rc;
    }

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end();
         ++pRange)
    {
        MdiagSurf *surface = pRange->GetMdiagSurf();

        if (!surface->IsAtsMapped())
        {
            ErrPrintf("Action.SetAtsPermission: Called on non-ATS surface '%s'.\n",
                      surface->GetName().c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        UINT32 atsPageSize = surface->GetAtsPageSize() << 10;
        MASSERT(atsPageSize > 0);
        UINT64 size = pRange->GetSize();
        UINT64 virtAddr = pRange->GetGpuAddr();
        UINT64 startVirtAddr = ALIGN_DOWN(virtAddr, atsPageSize);
        UINT64 endVirtAddr = ALIGN_UP(virtAddr + size, atsPageSize);

        for (virtAddr = startVirtAddr;
             virtAddr < endVirtAddr;
             virtAddr += atsPageSize)
        {
            CHECK_RC(surface->UpdateAtsPermission(virtAddr, m_PermissionType,
                m_Permissiolwalue));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnAtsIsEnabledOnChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_OnAtsIsEnabledOnChannel::PmAction_OnAtsIsEnabledOnChannel
(
    const PmChannelDesc *pChannelDesc
) :
    PmAction_Goto("ActionBlock.OnAtsIsEnabledOnChannel"),
    m_pChannelDesc(pChannelDesc)
{
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_OnAtsIsEnabledOnChannel::~PmAction_OnAtsIsEnabledOnChannel()
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_OnAtsIsEnabledOnChannel::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pChannelDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief check if ATS is enabled on channel.
//!

RC PmAction_OnAtsIsEnabledOnChannel::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent* pEvent,
    bool *pGoto
)
{
    RC rc;

    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    if (channels.empty())
    {
        ErrPrintf("ActionBlock.OnAtsIsEnabledOnChannel: Cannot find channel\n");
        return RC::ILWALID_CHANNEL;
    }

    bool atsEnabled = false;
    for (PmChannels::iterator channelIter = channels.begin();
         channelIter != channels.end();
         ++channelIter)
    {
        atsEnabled |= (*channelIter)->IsAtsEnabled();
    }
    *pGoto = !atsEnabled;
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UpdateAtsMapping
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UpdateAtsMapping::PmAction_UpdateAtsMapping
(
    const string &name,
    IommuDrv::MappingType updateType
) :
    PmAction(name),
    m_UpdateType(updateType)
{
}

//--------------------------------------------------------------------
//! \brief Modify the mapped status of ATS pages.
//!
/* virtual */ RC PmAction_UpdateAtsMapping::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = GetUpdateRanges(pTrigger, pEvent);

    for (PmMemRanges::iterator pRange = ranges.begin();
         pRange != ranges.end();
         ++pRange)
    {
        MdiagSurf *surface = pRange->GetMdiagSurf();

        if (!surface->IsAtsMapped())
        {
            ErrPrintf("Action.UpdateAtsMapping: Called on non-ATS surface '%s'.\n",
                      surface->GetName().c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        PmMemMappingsHelper *mappingsHelper =
            pRange->GetSurface()->GetMemMappingsHelper();

        UINT64 atsPageSize = surface->GetAtsPageSize() << 10;
        MASSERT(atsPageSize > 0);
        UINT64 offset = ALIGN_DOWN(pRange->GetOffset(), atsPageSize);

        // Loop over every ATS page in the current range.
        for (; offset < pRange->GetSize(); offset += atsPageSize)
        {
            // Get all of the GMMU mappings that overlap the current ATS page.

            UINT64 size = min(atsPageSize, pRange->GetSurface()->GetSize() - offset);
            PmMemRange atsPageRange(pRange->GetSurface(), offset, size);
            PmMemMappings mappings =
                mappingsHelper->GetMemMappings(&atsPageRange);
            PmMemMappings::iterator mappingIter = mappings.begin();

            if (mappingIter == mappings.end())
            {
                ErrPrintf("Action.UpdateAtsMapping: Can't find matching GMMU mapping for ATS page.\n");
                return RC::SOFTWARE_ERROR;
            }

            // Make sure the first GMMU mapping actually overlaps the
            // current ATS page offset.
            MASSERT(offset >= (*mappingIter)->GetOffset());
            MASSERT(offset < (*mappingIter)->GetOffset() + (*mappingIter)->GetSize());

            // Get the virtual and physical addresses from the GMMU mapping.
            // The GMMU page size size might be larger than the ATS page size,
            // so account for the potential difference in offset.

            UINT64 virtAddr = (*mappingIter)->GetVirtAddr() +
                offset - (*mappingIter)->GetOffset();

            UINT64 physAddr = (*mappingIter)->GetPhysAddr() +
                offset - (*mappingIter)->GetOffset();

            string aperture;

            switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _APERTURE, (*mappingIter)->GetPteFlags()))
            {
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
                    aperture = "Local";
                    break;

                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_PEER_MEMORY:

                    ErrPrintf("Action.UpdateAtsMapping: Not support peer aperture lwrrently.\n");
                    return RC::SOFTWARE_ERROR;

                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
                    aperture = "Sysmem";
                    break;

                default:
                    ErrPrintf("Action.UpdateAtsMapping: Unrecognized ATS page aperture.\n");
                    return RC::SOFTWARE_ERROR;
            }

            CHECK_RC(surface->UpdateAtsMapping(m_UpdateType, virtAddr, physAddr, aperture));

            // If there are any more GMMU mappings (which can occur if the
            // GMMU page size is less than the ATS page size), check to make
            // sure they are all physically contiguous.
            while (mappingIter != mappings.end())
            {
                if (physAddr != (*mappingIter)->GetPhysAddr() + offset -
                    (*mappingIter)->GetOffset())
                {
                    ErrPrintf("Action.UpdateAtsMapping: Discontiguous GMMU pages found within an ATS page.\n");
                    return RC::SOFTWARE_ERROR;
                }
                ++mappingIter;
            }
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_UpdateAtsPages
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UpdateAtsPages::PmAction_UpdateAtsPages
(
    const string &name,
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    IommuDrv::MappingType updateType
) :
    PmAction_UpdateAtsMapping(name, updateType),
    m_PageDesc(pSurfaceDesc, offset, size)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_UpdateAtsPages::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Get the virtual address range(s) to be updated.
//!
/* virtual */ PmMemRanges PmAction_UpdateAtsPages::GetUpdateRanges
(
    const PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    return m_PageDesc.GetMatchingRanges(pTrigger, pEvent);
}

//////////////////////////////////////////////////////////////////////
// PmAction_UpdateFaultingAtsPage
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_UpdateFaultingAtsPage::PmAction_UpdateFaultingAtsPage
(
    const string &name,
    IommuDrv::MappingType updateType
) :
    PmAction_UpdateAtsMapping(name, updateType)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_UpdateFaultingAtsPage::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    RC rc;

    if (!pTrigger->HasMemRange())
    {
        ErrPrintf("This trigger does not support %s\n", GetName().c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the virtual address range(s) to be updated.
//!
/* virtual */ PmMemRanges PmAction_UpdateFaultingAtsPage::GetUpdateRanges
(
    const PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmMemRanges ranges;
    ranges.push_back(*pEvent->GetMemRange());
    return ranges;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SetPhysAddrBits
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SetPhysAddrBits::PmAction_SetPhysAddrBits
(
    const PmSurfaceDesc *pSurfaceDesc,
    UINT64 offset,
    UINT64 size,
    UINT64 addrValue,
    UINT64 addrMask,
    bool isInband,
    PmChannelDesc *inbandChannelDesc,
    bool deferTlbIlwalidate
) :
    PmAction("Action.SetPhysAddrBits"),
    m_PageDesc(pSurfaceDesc, offset, size),
    m_AddrValue(addrValue),
    m_AddrMask(addrMask),
    m_IsInband(isInband),
    m_InbandChannelDesc(inbandChannelDesc),
    m_DeferTlbIlwalidate(deferTlbIlwalidate)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SetPhysAddrBits::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_PageDesc.IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Modify the upper bits of the physical addresses in each of
//!        the user specified pages.
//!
/* virtual */ RC PmAction_SetPhysAddrBits::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmMemRanges ranges = m_PageDesc.GetMatchingRanges(pTrigger, pEvent);

    if (ranges.size() == 0)
    {
        DebugPrintf("Action.SetPhysAddrBits: No matching surface ranges found\n");
        return rc;
    }

    for (PmMemRanges::iterator rangeIter = ranges.begin();
         rangeIter != ranges.end();
         ++rangeIter)
    {
        PmChannel *inbandChannel = 0;

        if (m_IsInband)
        {
            PmChannels channels;

            if (m_InbandChannelDesc != 0)
            {
                channels = m_InbandChannelDesc->GetMatchingChannels(
                    pTrigger, pEvent);
            }
            else
            {
                MdiagSurf *pSurface = rangeIter->GetSurface()->GetMdiagSurf();
                GpuDevice *pGpuDevice = pSurface->GetGpuDev();
                channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
            }

            if (channels.size() > 0)
            {
                inbandChannel = channels[0];
            }
        }

        PmMemMappingsHelper *mappingsHelper =
            rangeIter->GetSurface()->GetMemMappingsHelper();
        PmMemMappings mappings =
            mappingsHelper->SplitMemMappings(&(*rangeIter));

        for (PmMemMappings::iterator memMappingIter = mappings.begin();
             memMappingIter != mappings.end();
             ++memMappingIter)
        {
            PmMemMapping *memMapping = *memMappingIter;

            memMapping->SetPhysAddrBits(m_AddrValue, m_AddrMask,
                m_DeferTlbIlwalidate, inbandChannel);
        }

        mappingsHelper->JoinMemMappings(&(*rangeIter));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// Static Methods
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! This function is called by CreateUpdatePdeStructs to create one
//! LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS.
//!
//! This function uses LW0080_CTRL_CMD_DMA_GET_PDE_INFO to query for
//! the info in a PDE, and writes a LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS
//! that should (in theory) write the same info back to the PDE.
//!
//! \param pGpuDevice The device that contains the PDE.
//! \param GpuAddr Any virtual address that is addressed by the PDE.
//! \param PdeCoverage The amount of virtual address space that is
//!     addressed by each PDE.
//! \param[out] pParams On exit, contains the info for the PDE.
//!
static RC CreateOneUpdatePdeStruct
(
    GpuDevice *pGpuDevice,
    UINT64 GpuAddr,
    UINT64 PdeCoverage,
    LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS *pParams,
    LwRm* pLwRm
)
{
    MASSERT(pGpuDevice != NULL);
    MASSERT(PdeCoverage != 0);
    MASSERT(pParams != NULL);
    RC rc;

    // Query for the current PDE info
    //
    LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS GetParams = {0};
    GetParams.gpuAddr = GpuAddr;
    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice,
                                    LW0080_CTRL_CMD_DMA_GET_PDE_INFO,
                                    &GetParams, sizeof(GetParams)));

    // Initialize the fields of pParams that are not dependant on page
    // size.
    //
    memset(pParams, 0, sizeof(*pParams));
    pParams->pdeIndex = (UINT32)(GpuAddr / PdeCoverage);
    pParams->flags = DRF_DEF(0080_CTRL_DMA_UPDATE_PDE_2, _FLAGS,
        _FORCE_OVERRIDE, _TRUE);

    switch(GetParams.pdeSize)
    {
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_SIZE_FULL:
            pParams->flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2,
                _FLAGS, _PDE_SIZE, _FULL, pParams->flags);
            break;
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_SIZE_HALF:
            pParams->flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2,
                _FLAGS, _PDE_SIZE, _HALF, pParams->flags);
            break;
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_SIZE_QUARTER:
            pParams->flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2,
                _FLAGS, _PDE_SIZE, _QUARTER, pParams->flags);
            break;
        case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PDE_SIZE_EIGHTH:
            pParams->flags = FLD_SET_DRF(0080_CTRL_DMA_UPDATE_PDE_2,
                _FLAGS, _PDE_SIZE, _EIGHTH, pParams->flags);
            break;
        default:
            MASSERT(!"Unknown PDE_SIZE in CreateOneUpdatePdeStruct()");
    }

    // Initialize the fields of pParams that depend on page size.
    //
    for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_PDE_INFO_PTE_BLOCKS; ++ii)
    {
        LW0080_CTRL_DMA_PDE_INFO_PTE_BLOCK *pGetPteBlock =
            &GetParams.pteBlocks[ii];

        if (pGetPteBlock->pageSize != 0)
        {
            LW0080_CTRL_DMA_UPDATE_PDE_2_PAGE_TABLE_PARAMS *pSetPteBlock;

            if (GetParams.pteBlocks[ii].pageSize ==
                PolicyManager::BYTES_IN_SMALL_PAGE)
            {
                pSetPteBlock = &pParams->ptParams[
                    LW0080_CTRL_DMA_UPDATE_PDE_2_PT_IDX_SMALL];
            }
            else
            {
                pSetPteBlock = &pParams->ptParams[
                    LW0080_CTRL_DMA_UPDATE_PDE_2_PT_IDX_BIG];
            }

            pSetPteBlock->physAddr = pGetPteBlock->ptePhysAddr;
            pSetPteBlock->numEntries = (pGetPteBlock->pdeVASpaceSize /
                pGetPteBlock->pageSize);

            switch (pGetPteBlock->pteAddrSpace)
            {
                case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PTE_ADDR_SPACE_VIDEO_MEMORY:
                    pSetPteBlock->aperture =
                        LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_VIDEO_MEMORY;
                    break;

                case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PTE_ADDR_SPACE_SYSTEM_COHERENT_MEMORY:
                    pSetPteBlock->aperture =
                        LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_SYSTEM_COHERENT_MEMORY;
                    break;

                case LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS_PTE_ADDR_SPACE_SYSTEM_NON_COHERENT_MEMORY:
                    pSetPteBlock->aperture =
                        LW0080_CTRL_DMA_UPDATE_PDE_2_PT_APERTURE_SYSTEM_NON_COHERENT_MEMORY;
                    break;

                default:
                    MASSERT(!"Unknown aperture in CreateOneUpdatePdeStruct()");
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! This function is called by CreateUpdatePdeStructs to extract the
//! big-page/small-page info from one LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS
//! struct.
//!
//! This function uses the PageSize argument to determine whether we
//! want to modify the big-page data, small-page data, or both.  Then
//! it appends the ptParams[] entries to the pPteBlocks vector.
//!
//! \param pParams The LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS struct.
//! \param pGpuDevice The device that contains the PDE.
//! \param PageSize Indicates which big-page/small-page data to extract.
//! \param PdeCoverage The amount of virtual address space that is
//!     addressed by each PDE.
//! \param pRanges All the memory ranges that the caller is modifying.
//!     Used when PageSize is LWRRENT_PAGE_SIZE, so that we can
//!     determine which page size(s) are lwrrently used by the PDE.
//! \param[out] pPteBlocks On exit, pointers to the big-page/small-page
//!     blocks in pParams are appended to this vector.
//!
static void ExtractPteTableBlocks
(
    LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS *pParams,
    GpuDevice *pGpuDevice,
    PolicyManager::PageSize PageSize,
    UINT64 PdeCoverage,
    const PmMemRanges *pRanges,
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PAGE_TABLE_PARAMS*> *pPteBlocks
)
{
    MASSERT(pParams != NULL);
    MASSERT(pGpuDevice != NULL);
    MASSERT(PdeCoverage != 0);
    MASSERT(pRanges != NULL);
    MASSERT(pPteBlocks != NULL);

    static const UINT32 BYTES_IN_SMALL_PAGE =
        PolicyManager::BYTES_IN_SMALL_PAGE;
    UINT64 MinPdeAddr = PdeCoverage * pParams->pdeIndex;
    UINT64 MaxPdeAddr = PdeCoverage * (pParams->pdeIndex + 1);
    bool useBigPage = false;
    bool useSmallPage = false;

    // Set useBigPage and/or useSmallPage, depending on whether we
    // want to keep the big-page info, small-page info, or both.
    //
    switch (PageSize)
    {
        case PolicyManager::ALL_PAGE_SIZES:
            useBigPage = true;
            useSmallPage = true;
            break;
        case PolicyManager::BIG_PAGE_SIZE:
            useBigPage = true;
            break;
        case PolicyManager::SMALL_PAGE_SIZE:
            useSmallPage = true;
            break;
        case PolicyManager::HUGE_PAGE_SIZE:
        case PolicyManager::PAGE_SIZE_512MB:
            MASSERT(!"NOT support yet!");
            break;
        case PolicyManager::LWRRENT_PAGE_SIZE:
            for (PmMemRanges::const_iterator pRange = pRanges->begin();
                 pRange != pRanges->end(); ++pRange)
            {
                if (pRange->GetSurface()->GetMdiagSurf()->GetGpuDev() !=
                    pGpuDevice)
                {
                    continue;
                }
                PmMemMappings mappings;
                PmMemMappingsHelper *pMappingsHelper;
                pMappingsHelper = pRange->GetSurface()->GetMemMappingsHelper();
                mappings = pMappingsHelper->GetMemMappings(&*pRange);
                for (PmMemMappings::iterator ppMapping = mappings.begin();
                     ppMapping != mappings.end(); ++ppMapping)
                {
                    if (MinPdeAddr < ((*ppMapping)->GetGpuAddr() +
                                      (*ppMapping)->GetSize()) &&
                        MaxPdeAddr > (*ppMapping)->GetGpuAddr())
                    {
                        if ((*ppMapping)->GetPageSize() == BYTES_IN_SMALL_PAGE)
                            useSmallPage = true;
                        else
                            useBigPage = true;
                    }
                }
            }
            break;
    }

    // Append the desired entries to pPteBlocks
    //
    if (useSmallPage)
    {
        pPteBlocks->push_back(
            &pParams->ptParams[LW0080_CTRL_DMA_UPDATE_PDE_2_PT_IDX_SMALL]);
    }
    if (useBigPage)
    {
        pPteBlocks->push_back(
            &pParams->ptParams[LW0080_CTRL_DMA_UPDATE_PDE_2_PT_IDX_BIG]);
    }
}

//--------------------------------------------------------------------
//! \brief Create UPDATE_PDE structs for read-modify-write operations
//!
//! This method is used by actions that do read-modify-write
//! operations on PDEs, using LW0080_CTRL_CMD_DMA_GET_PDE_INFO to read
//! and LW0080_CTRL_CMD_DMA_UPDATE_PDE_2 to write.
//!
//! Given a set of PmMemRanges, this function creates a set of
//! UPDATE_PDE structs in which the fields are all set to the current
//! values in the PDEs.  It's up to the caller to modify any fields he
//! wants to change before sending the structs to the RM.
//!
//! \param pRanges The ranges that the caller wants to modify the PDEs
//!     for.
//! \param PageSize The page size(s) the caller wants to modify.
//!     Ignored if pPteBlocks is NULL.
//! \param[out] pParams The UPDATE_PDE structs returned to the caller.
//! \param[out] pPteBlocks Each UPDATE_PDE struct contains blocks for several
//!     page tables.  If non-NULL, this parameter returns all the blocks
//!     that match PageSize.
//! \param[out] pGpuDevices The GpuDevice for each struct in pParams.
//!
/* static */ RC CreateUpdatePdeStructs
(
    const PmMemRanges *pRanges,
    PolicyManager::PageSize PageSize,
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS> *pParams,
    vector<LW0080_CTRL_DMA_UPDATE_PDE_2_PAGE_TABLE_PARAMS*> *pPteBlocks,
    GpuDevices *pGpuDevices
)
{
    MASSERT(pRanges != NULL);
    MASSERT(pParams != NULL);
    MASSERT(pGpuDevices != NULL);

    RC rc;
    map<GpuDevice*, UINT64> PdeCoverage;
    map<GpuDevice*, set<UINT32> > PdesProcessed;

    pParams->clear();
    pGpuDevices->clear();

    for (PmMemRanges::const_iterator pRange = pRanges->begin();
         pRange != pRanges->end(); ++pRange)
    {
        GpuDevice *pGpuDevice = pRange->GetMdiagSurf()->GetGpuDev();
        LwRm* pLwRm = pRange->GetSurface()->GetLwRmPtr();

        if (PdeCoverage.count(pGpuDevice) == 0)
        {
            LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS capsParams = {0};
            CHECK_RC(pLwRm->ControlByDevice(
                         pGpuDevice, LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
                         &capsParams, sizeof(capsParams)));
            PdeCoverage[pGpuDevice] =
                ((UINT64)1) << capsParams.pdeCoverageBitCount;
        }

        for (UINT64 GpuAddr = pRange->GetGpuAddr();
             GpuAddr < pRange->GetGpuAddr() + pRange->GetSize();
             GpuAddr = ALIGN_UP(GpuAddr + 1, PdeCoverage[pGpuDevice]))
        {
            UINT32 PdeIndex = (UINT32)(GpuAddr / PdeCoverage[pGpuDevice]);
            if (PdesProcessed[pGpuDevice].count(PdeIndex) == 0)
            {
                LW0080_CTRL_DMA_UPDATE_PDE_2_PARAMS updateParams;
                CHECK_RC(CreateOneUpdatePdeStruct(pGpuDevice, GpuAddr,
                                                  PdeCoverage[pGpuDevice],
                                                  &updateParams,
                                                  pLwRm));
                pParams->push_back(updateParams);
                pGpuDevices->push_back(pGpuDevice);
                PdesProcessed[pGpuDevice].insert(PdeIndex);
            }
        }
    }

    if (pPteBlocks != NULL)
    {
        pPteBlocks->clear();
        for (UINT32 ii = 0; ii < pParams->size(); ++ii)
        {
            ExtractPteTableBlocks(&(*pParams)[ii], (*pGpuDevices)[ii],
                                  PageSize, PdeCoverage[(*pGpuDevices)[ii]],
                                  pRanges, pPteBlocks);
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the local and remote subdevices for surface access
//!
//! This method is used by actions access surfaces via the GPU to
//! determine the local and remote subdevices used when accessing
//! the surface.
//!
//! Given the trigger and event, as well as a descriptor for the
//! local and remote gpus, a subdevice pointer for each is returned.
//! If the descriptors indicate that no remote access is necessary
//! both of the returned subdevice pointers will be NULL.If no
//! remote descriptor is present (the parameter is NULL) then
//! surface access will never be remote).
//!
//! \param pTrigger The trigger of the action.
//! \param pEvent The event that caused the trigger.
//! \param pLocalDesc The GPU descriptor for the local GPU.
//! \param pRemoteDesc The GPU descriptor for the remote GPU.
//! \param[out] ppLocalSubdevice The local subdevice returned to the caller
//! \param[out] ppRemoteSubdevice The remote subdevice returned to the caller
//!
/* static */ RC GetSubdevicesForSurfaceAccess
(
    PmTrigger         *pTrigger,
    const PmEvent     *pEvent,
    const PmGpuDesc   *pLocalDesc,
    const PmGpuDesc   *pRemoteDesc,
    GpuSubdevice     **ppLocalSubdevice,
    GpuSubdevice     **ppRemoteSubdevice
)
{
    MASSERT(ppLocalSubdevice);
    MASSERT(ppRemoteSubdevice);

    *ppLocalSubdevice = NULL;
    *ppRemoteSubdevice = NULL;

    // If there is no remote subdevice Gpu description, then there is no peer
    // access, return OK with NULL subdevices
    if (!pRemoteDesc)
        return OK;

    if (!pLocalDesc)
    {
        ErrPrintf("GetSubdevicesForSurfaceAccess : Remote gpu description without a local gpu description!\n");
        return RC::SOFTWARE_ERROR;
    }

    GpuSubdevices localSubdev;
    GpuSubdevices remoteSubdev;

    // Match specifically against all subdevices when getting the remote
    // subdevice to use since a remote subdevice may be on a device not
    // from the triggering event/trigger
    remoteSubdev =
        pRemoteDesc->GetMatchingSubdevicesInTrigger(
            pTrigger->GetPolicyManager());
    localSubdev =
        pLocalDesc->GetMatchingSubdevices(pTrigger, pEvent);

    if ((remoteSubdev.size() != 1) || (localSubdev.size() != 1))
    {
        ErrPrintf("GetSubdevicesForSurfaceAccess : Gpus found - %d local, %d remote.  Exactly one of each must be found\n",
                  (UINT32)localSubdev.size(), (UINT32)remoteSubdev.size());
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        *ppRemoteSubdevice = remoteSubdev[0];
        *ppLocalSubdevice = localSubdev[0];
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Set the gpio ports to the specified direction
//!
//! This method sets the gpio ports up or down by inserting
//! falcon methods via subchannels.
//! We first find if there are matching subchannels specified by user.
//! If not, we pick the first supported subchannel,
//! which will be either a graphics or compute subchannel.
//! If no supported subchannel found, returns an error.
//!
//! \param pChannelDesc The PmChannelDesc to get valid subchannels.
//! \param pChannel The PmChannel to insert falcon methods.
//! \param ports Gpio ports to set.
//! \param direction The direction to set, up or down.
//!
/* static */ RC SetGpioDirection
(
    const PmChannelDesc *pChannelDesc,
    PmChannel *pChannel,
    const vector<UINT32> &ports,
    GpioDirection direction
)
{
    MASSERT(pChannel != NULL);

    // Set up list of supported graphics and compute classes
    //
    ThreeDAlloc graphicsClasses;
    graphicsClasses.SetOldestSupportedClass(FERMI_C);

    ComputeAlloc computeClasses;
    computeClasses.SetOldestSupportedClass(FERMI_COMPUTE_B);

    // If there are subchannels matching the name specified by user,
    // pick them and insert methods through them only.
    vector<UINT32> subchs =
        pChannelDesc->GetMatchingSubchannels(pChannel, NULL, NULL);
    // Else we pick the first subchannel whose class is either graphics or compute.
    if (subchs.empty())
    {
        DebugPrintf("SetGpioDirection: No matching subchannel found in channel %s. Will find the first supported one.\n",
                    pChannel->GetName().c_str());
        for (UINT32 iSubch = 0; iSubch < Channel::NumSubchannels; ++iSubch)
        {
            UINT32 subchClass = pChannel->GetSubchannelClass(iSubch);
            if (graphicsClasses.HasClass(subchClass) ||
                computeClasses.HasClass(subchClass))
            {
                // Only pick the first supported subchannel.
                subchs.push_back(iSubch);
                break;
            }
        }
    }
    // No supported subchannel found. Return an error.
    if (subchs.empty())
    {
        ErrPrintf("SetGpioDirection: No supported subchannel found in channel %s. Gpio falcon methods not inserted.\n",
                  pChannel->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    bool methodInserted = false;
    for (UINT32 iSubch = 0; iSubch < subchs.size(); ++iSubch)
    {
        UINT32 scratch0 = 0;
        UINT32 scratch1 = 0;
        UINT32 falcon01 = 0;
        UINT32 nop = 0;
        UINT32 subch = subchs[iSubch];
        UINT32 subchClass = pChannel->GetSubchannelClass(subch);
        bool classSupported = false;

        if (graphicsClasses.HasClass(subchClass))
        {
            scratch0 = LW9297_SET_MME_SHADOW_SCRATCH(0);
            scratch1 = LW9297_SET_MME_SHADOW_SCRATCH(1);
            falcon01 = LW9297_SET_FALCON01;
            nop = LW9297_NO_OPERATION;
            classSupported = true;
        }
        else if (computeClasses.HasClass(subchClass))
        {
            scratch0 = LW91C0_SET_MME_SHADOW_SCRATCH(0);
            scratch1 = LW91C0_SET_MME_SHADOW_SCRATCH(1);
            falcon01 = LW91C0_SET_FALCON01;
            nop = LW91C0_NO_OPERATION;
            classSupported = true;
        }
        if (classSupported)
        {
            for (UINT32 iPort = 0; iPort < ports.size(); ++iPort)
            {
                UINT32 port = ports[iPort];
                if (port > 31)
                {
                    ErrPrintf("SetGpioDirection: Invalid gpio port %u! It should be in range of 0 to 31.\n", port);
                    return RC::SOFTWARE_ERROR;
                }
                // noops count is hard-coded
                UINT32 nopNum = FALCON_NOP_COUNT;
                pChannel->Write(subch, scratch0, 0);
                pChannel->Write(subch, scratch1,
                                (direction == GPIO_UP ?
                                 DRF_DEF(_PMGR_GPIO_0_OUTPUT_CNTL, _IO, _OUTPUT, _1) :
                                 DRF_DEF(_PMGR_GPIO_0_OUTPUT_CNTL, _IO, _OUTPUT, _0)) |
                                DRF_DEF(_PMGR_GPIO_0_OUTPUT_CNTL, _IO, _OUT_EN, _YES));
                pChannel->Write(subch, falcon01, LW_PMGR_GPIO_0_OUTPUT_CNTL + (port * 4));
                for (UINT32 iNop = 0; iNop < nopNum; ++iNop)
                {
                    pChannel->Write(subch, nop, 0);
                }
                pChannel->Write(subch, scratch0, 0);
                pChannel->Write(subch, scratch1, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
                pChannel->Write(subch, falcon01, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
                for (UINT32 iNop = 0; iNop < nopNum; ++iNop)
                {
                    pChannel->Write(subch, nop, 0);
                }
            }
            methodInserted = true;
        }
        else
        {
            WarnPrintf("SetGpioDirection: Unsupported subchannel %s in channel %s. Gpio falcon methods not inserted via this subchannel.\n",
                       pChannel->GetSubchannelName(subch).c_str(), pChannel->GetName().c_str());
        }
    }

    // Method not inserted. Return an error.
    if (!methodInserted)
    {
        ErrPrintf("SetGpioDirection: No supported subchannel found in channel %s. Gpio falcon methods not inserted.\n",
                  pChannel->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SendTraceEvent
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SendTraceEvent::PmAction_SendTraceEvent
(
    const string &eventName,
    const PmSurfaceDesc *pSurfaceDesc
):
    PmAction("Action.SendTraceEvent"),
    m_EventName(eventName),
    m_pSurfaceDesc(pSurfaceDesc)
{
    MASSERT(m_pSurfaceDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SendTraceEvent::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return m_pSurfaceDesc->IsSupportedInAction(pTrigger);
}

//--------------------------------------------------------------------
//! \brief Send the indicated trace3d event to plugin.
//!
/* virtual */ RC PmAction_SendTraceEvent::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PmSubsurfaces subsurfaces =
        m_pSurfaceDesc->GetMatchingSubsurfaces(pTrigger,pEvent);

    PolicyManager *pPolicyManager = GetPolicyManager();

    for (PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
            ppSubsurface != subsurfaces.end();++ppSubsurface)
    {
        MdiagSurf *pSurface2D = (*ppSubsurface)->GetSurface()->GetMdiagSurf();
        PmTests tests = pPolicyManager->GetActiveTests();
        for (PmTests::iterator ppTest = tests.begin();
                ppTest != tests.end();++ppTest)
        {
            (*ppTest)->SendTraceEvent(m_EventName.c_str(),pSurface2D->GetName().c_str());
        }
    }
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_PmAction_RestartFaultedChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RestartFaultedChannel::PmAction_RestartFaultedChannel
(
        const string& actionName,
        const PmChannelDesc *pChannelDesc,
        bool  isInBand,
        PmChannelDesc *pInbandChannelDesc,
        FaultType type
):
    PmAction(actionName),
    m_pChannelDesc(pChannelDesc),
    m_IsInband(isInBand),
    m_pInbandChannelDesc(pInbandChannelDesc),
    m_FaultType(type)
{
    MASSERT(pChannelDesc != NULL);
    MASSERT(type != UNKNOWN);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_RestartFaultedChannel::~PmAction_RestartFaultedChannel()
{}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
RC PmAction_RestartFaultedChannel::IsSupported(const PmTrigger *pTrigger) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief clear LW_PCCSR_CHANNEL_ENG_FAULTED
//!
RC PmAction_RestartFaultedChannel::Execute(PmTrigger *pTrigger, const PmEvent *pEvent)
{
    PmChannels channels;
    GpuSubdevices subdevices;
    RC rc;

    channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);

    if (channels.empty())
    {
         WarnPrintf("No channels found for %s. Ignoring\n", GetName().c_str());
         return rc;
    }
    if (m_IsInband)
    {
        PmChannel *pInbandChannel = GetInbandChannel(pTrigger, pEvent,
            channels[0]->GetGpuDevice());
        MASSERT(pInbandChannel);
        CHECK_RC(InbandClearFault(channels, pInbandChannel));
    }
    else
    {
        CHECK_RC(OutOfBandClearFault(channels));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief in band clear fault
//!
RC PmAction_RestartFaultedChannel::InbandClearFault
(
    const PmChannels& pmChannels,
    PmChannel *pInbandChannel
)
{
    RC rc;

    for (PmChannels::const_iterator iter = pmChannels.begin(); iter != pmChannels.end(); ++iter)
    {
        Channel *pChannel = pInbandChannel->GetModsChannel();
        switch (pChannel->GetClass())
        {
            case VOLTA_CHANNEL_GPFIFO_A:
            case TURING_CHANNEL_GPFIFO_A:
            case AMPERE_CHANNEL_GPFIFO_A:
            case HOPPER_CHANNEL_GPFIFO_A:
                CHECK_RC(SendMethodsForVolta(*iter, pInbandChannel));
                break;
            default:
                ErrPrintf("%s doesn't support host class 0x%x\n",
                          GetName().c_str(), pChannel->GetClass());
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief out-of-band clear fault for Ampere
//!
RC PmAction_RestartFaultedChannel::OutOfBandClearFaultAmpere(PmChannel *pPmChannel)
{
    RC rc;
    GpuDevice *pDevice = pPmChannel->GetGpuDevice();
    const UINT32 chId = pPmChannel->GetModsChannel()->GetChannelId();
    const UINT32 engineId = pPmChannel->GetEngineId();

    LwRm* pLwRm = pPmChannel->GetLwRmPtr();

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
    {
        GpuSubdevice *pSubdev = pDevice->GetSubdevice(subdev);
        LW2080_CTRL_CMD_FIFO_CLEAR_FAULTED_BIT_PARAMS params = {0};
        params.vChid = chId;
        params.engineType = engineId;

        switch (m_FaultType)
        {
            case ENGINE:
                params.faultType = LW2080_CTRL_FIFO_CLEAR_FAULTED_BIT_FAULT_TYPE_ENGINE;
                DebugPrintf(GetTeeCode(), "Clear LW_CHRAM_CHANNEL_ENG_FAULTED for subdevice(%d) in %s.\n", subdev, GetName().c_str());
                break;
            case PBDMA:
                params.faultType = LW2080_CTRL_FIFO_CLEAR_FAULTED_BIT_FAULT_TYPE_PBDMA;
                DebugPrintf(GetTeeCode(), "Clear LW_CHRAM_CHANNEL_PBDMA_FAULTED for subdevice(%d) in %s.\n", subdev, GetName().c_str());
                break;
            default:
                MASSERT(!"unknown fault type!");
        }

        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
                            LW2080_CTRL_CMD_FIFO_CLEAR_FAULTED_BIT,
                            &params,
                            sizeof(params)));
    }

    return rc;
}

RC PmAction_RestartFaultedChannel::OutOfBandClearFault(PmChannel *pPmChannel)
{
    RC rc;
    // seeing //sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/volta/gv000/dev_fifo.h
    //#define LW_PCCSR_CHANNEL(i)                      (0x00800004+(i)*8) /* RW-4A */
    //#define LW_PCCSR_CHANNEL__SIZE_1              LW_CHIP_HOST_CHANNELS /*       */
    //
    //#define LW_PCCSR_CHANNEL_PBDMA_FAULTED                        22:22 /* RWIVF */
    //#define LW_PCCSR_CHANNEL_PBDMA_FAULTED_FALSE             0x00000000 /* R-I-V */
    //#define LW_PCCSR_CHANNEL_PBDMA_FAULTED_TRUE              0x00000001 /* R---V */
    //#define LW_PCCSR_CHANNEL_PBDMA_FAULTED_RESET             0x00000001 /* -W--T */

    //#define LW_PCCSR_CHANNEL_ENG_FAULTED                          23:23 /* RWIVF */
    //#define LW_PCCSR_CHANNEL_ENG_FAULTED_FALSE               0x00000000 /* R-I-V */
    //#define LW_PCCSR_CHANNEL_ENG_FAULTED_TRUE                0x00000001 /* R---V */
    //#define LW_PCCSR_CHANNEL_ENG_FAULTED_RESET               0x00000001 /* -W--T */


    const UINT32 chId = pPmChannel->GetModsChannel()->GetChannelId();
    GpuDevice *pDevice = pPmChannel->GetGpuDevice();

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices();
            subdev++)
    {
        GpuSubdevice *pSubDevice = pDevice->GetSubdevice(subdev);
        RegHal &regs = pSubDevice->Regs();
        UINT32 regVal = regs.Read32(MODS_PCCSR_CHANNEL, chId);

        switch(m_FaultType)
        {
            case ENGINE:
                if (!regs.TestField(regVal, MODS_PCCSR_CHANNEL_ENG_FAULTED_TRUE))
                {
                    WarnPrintf("LW_PCCSR_CHANNEL_ENG_FAULTED != TRUE before %s.\n", GetName().c_str());
                }
                DebugPrintf(GetTeeCode(), "Clear LW_PCCSR_CHANNEL_ENG_FAULTED for subdevice(%d) in %s.\n", subdev, GetName().c_str());
                regs.SetField(&regVal, MODS_PCCSR_CHANNEL_ENG_FAULTED_RESET);
                break;
            case PBDMA:
                if (!regs.TestField(regVal, MODS_PCCSR_CHANNEL_PBDMA_FAULTED_TRUE))
                {
                    WarnPrintf("LW_PCCSR_CHANNEL_PBDMA_FAULTED != TRUE before %s.\n", GetName().c_str());
                }
                DebugPrintf(GetTeeCode(), "Clear LW_PCCSR_CHANNEL_PBDMA_FAULTED for subdevice(%d) in %s.\n", subdev, GetName().c_str());
                regs.SetField(&regVal, MODS_PCCSR_CHANNEL_PBDMA_FAULTED_RESET);
                break;
            default:
                MASSERT(!"unknown fault type!");
        }

        regs.Write32(MODS_PCCSR_CHANNEL, chId, regVal);
    }

    return rc;
}

//! \brief out-of-band clear fault
//!
RC PmAction_RestartFaultedChannel::OutOfBandClearFault(const PmChannels& pmChannels)
{
    RC rc;

    for (const auto& pPmChannel : pmChannels)
    {
        GpuDevice *pDevice = pPmChannel->GetGpuDevice();

        rc = pDevice->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID) ?
                OutOfBandClearFaultAmpere(pPmChannel) : OutOfBandClearFault(pPmChannel);
        if (rc != OK)
            break;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the channel to use for sending inband methods.
//!
PmChannel* PmAction_RestartFaultedChannel::GetInbandChannel
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    GpuDevice *pGpuDevice
)
{
    MASSERT(m_IsInband);

    RC rc;

    PmChannels channels;
    if (m_pInbandChannelDesc)
    {
        channels = m_pInbandChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    }
    else
    {
        channels = GetPolicyManager()->GetActiveChannels(pGpuDevice);
    }
    if(channels.size() > 0)
    {
        return channels[0];
    }
    else
    {
        ErrPrintf("Action.RestartEngineFaultedChannel: Fails to find an inband channel\n");
        return NULL;
    }
}

//--------------------------------------------------------------------
//! \brief send methods for volta
//!
RC PmAction_RestartFaultedChannel::SendMethodsForVolta
(
    PmChannel *pFaultingChannel,
    PmChannel *pInbandChannel
)
{
    // Seeing //hw/lwgpu/manuals/glit0/dev_pbdma.ref
    //#define LW_UDMA_CLEAR_FAULTED                            0x00000084 /* -W-4R */
    //
    //#define LW_UDMA_CLEAR_FAULTED_CHID                             11:0 /* -W-VF */
    //#define LW_UDMA_CLEAR_FAULTED_TYPE                            31:31 /* -W-VF */
    //#define LW_UDMA_CLEAR_FAULTED_TYPE_PBDMA_FAULTED         0x00000000 /* -W--V */
    //#define LW_UDMA_CLEAR_FAULTED_TYPE_ENG_FAULTED           0x00000001 /* -W--V */

    RC rc;
    LwRm* pLwRm = pFaultingChannel->GetLwRmPtr();
    UINT32  data      = 0;

    // Get the ring token to use in CLEAR_FAULTED
    LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS params = {};
    CHECK_RC(pLwRm->Control(pFaultingChannel->GetHandle(),
                            LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN,
                            &params,
                            sizeof(params)));

    //
    // Bug: 1905719 needs to take care of changes done to CLEAR_FAULTED for Turing
    // Until then blindly using workSubmitToken instead of using right bit fields
    //
    data = params.workSubmitToken;

    //LWC36F_CLEAR_FAULTED_TYPE_ENG_FAULTED
    switch(m_FaultType)
    {
    case ENGINE:
        data = FLD_SET_DRF(C36F, _CLEAR_FAULTED, _TYPE, _ENG_FAULTED, data);
        break;
    case PBDMA:
        data = FLD_SET_DRF(C36F, _CLEAR_FAULTED, _TYPE, _PBDMA_FAULTED, data);
        break;
    default:
        MASSERT(!"unknown fault type!");
    }

    PM_BEGIN_INSERTED_METHODS(pInbandChannel);

    CHECK_RC(pInbandChannel->Write(
        0,
        LWC36F_CLEAR_FAULTED,
        data));

    PM_END_INSERTED_METHODS();

    CHECK_RC(pInbandChannel->Flush());

    DebugPrintf(GetTeeCode(), "Method which is send to PB in %s(). address: 0x%08x, data: 0x%08x\n",
                              GetName().c_str(), LWC36F_CLEAR_FAULTED, data);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_RestartEngineFaultedChannel
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_RestartEngineFaultedChannel::PmAction_RestartEngineFaultedChannel
(
    const PmChannelDesc *pChannelDesc,
    bool  isInBand,
    PmChannelDesc *pInbandChannelDesc
):
    PmAction_RestartFaultedChannel("Action.RestartEngineFaultedChannel",
                                   pChannelDesc,
                                   isInBand,
                                   pInbandChannelDesc,
                                   PmAction_RestartFaultedChannel::ENGINE)
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_RestartPbdmaFaultedChannel
//////////////////////////////////////////////////////////////////////
//! \brief constructor
//!
PmAction_RestartPbdmaFaultedChannel::PmAction_RestartPbdmaFaultedChannel
(
    const PmChannelDesc *pChannelDesc,
    bool  isInBand,
    PmChannelDesc *pInbandChannelDesc
):
    PmAction_RestartFaultedChannel("Action.RestartPbdmaFaultedChannel",
                                    pChannelDesc,
                                    isInBand,
                                    pInbandChannelDesc,
                                    PmAction_RestartFaultedChannel::PBDMA)
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearAccessCounter
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearAccessCounter::PmAction_ClearAccessCounter
(
    PmGpuDesc *pGpuDesc,
    const bool bInBand,
    const string& counterType
):
    PmAction("Action.ClearAccessCounter"),
    m_pGpuDesc(pGpuDesc),
    m_StrCounterType(counterType),
    m_CounterType(LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC),
    m_bInBand(bInBand)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ClearAccessCounter::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

RC PmAction_ClearAccessCounter::ColwertStrType()
{
    RC rc;

    m_bTargeted = false;
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC                0x00000000
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC                0x00000001
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL                 0x00000002
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_TARGETED            0x00000003
    if ("mimc" == m_StrCounterType)
    {
        m_CounterType = LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC;
    }
    else if ("momc" == m_StrCounterType)
    {
        m_CounterType = LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC;
    }
    else if ("all" == m_StrCounterType)
    {
        m_CounterType = LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL;
    }
    else if ("targeted" == m_StrCounterType)
    {
        m_CounterType = LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_TARGETED;
        m_bTargeted = true;
    }
    else
    {
        ErrPrintf("Action.ClearAccessCounter: Invalid type or targeted type '%s'; only 'mimc', 'momc', 'all', 'targeted' are supported.\n",
                  m_StrCounterType.c_str());
        rc = RC::ILWALID_ARGUMENT;
    }

    return rc;
}

RC PmAction_ClearAccessCounter::CheckType() const
{
    RC rc;

    if (m_bTargeted)
    {
        // IBTargeted
        if (m_CounterType > LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC)
        {
            ErrPrintf("Action.ClearAccessCounter: Invalid targeted type 0x%x; only a value 0 (mimc) or 1 (momc) is supported.\n",
                      m_CounterType);
            rc = RC::ILWALID_ARGUMENT;
        }
    }
    else
    {
        // OOB or IBUntargeted
        if (m_CounterType > LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL)
        {
            ErrPrintf("Action.ClearAccessCounter: Invalid type 0x%x; only a value in range 0..2 (mimc, momc, all) is supported for untargeted clears.\n",
                      m_CounterType);
            rc = RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}
//--------------------------------------------------------------------
//! \brief clear counter
//!
/* virtual */ RC PmAction_ClearAccessCounter::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    CHECK_RC(ColwertStrType());
    if (m_bTargeted)
    {
        const PmEvent_AccessCounterNotification* pNotification =
                dynamic_cast<const PmEvent_AccessCounterNotification*>(pEvent);
        MASSERT(pNotification != 0); // Failing if the event is not PmEvent_AccessCounterNotification.
        m_CounterType = LWC365_NOTIFY_BUF_ENTRY_TYPE_GPU == pNotification->GetAccessType() ?
                LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC : LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC;
        m_TargetedBank = pNotification->GetBank();
        m_TargetedNotifyTag = pNotification->GetNotifyTag();
    }
    CHECK_RC(CheckType());

    GpuDevices gpuDevices;

    gpuDevices = m_pGpuDesc->GetMatchingDevices(pTrigger, pEvent);

    for (GpuDevices::iterator ppGpuDev = gpuDevices.begin();
         ppGpuDev != gpuDevices.end(); ++ppGpuDev)
    {
        GpuSubdevices gpuSubdevs;

        gpuSubdevs = m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);
        for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevs.begin();
             ppGpuSubdevice != gpuSubdevs.end(); ++ppGpuSubdevice)
        {
            GpuSubdevice* pSubdev = *ppGpuSubdevice;
            if (m_bInBand)
            {
                CHECK_RC(InbandClear(pSubdev));
            }
            else
            {
                CHECK_RC(OutOfBandClear(pSubdev));
            }
        }
    }

    return rc;
}

void PmAction_ClearAccessCounter::MemOps(UINT32* pMemOpC,
        UINT32* pMemOpD) const
{
    UINT32& memOpD = *pMemOpD;
    // Untargeted inband clear

    // #define LWC36F_MEM_OP_C                                            (0x00000030)
    // #define LWC36F_MEM_OP_C_ACCESS_COUNTER_CLR_TARGETED_NOTIFY_TAG            19:0
    // #define LWC36F_MEM_OP_D                                            (0x00000034)
    // #define LWC36F_MEM_OP_D_OPERATION                                        31:27
    // #define LWC36F_MEM_OP_D_OPERATION_ACCESS_COUNTER_CLR                0x00000016
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE                            1:0
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC                0x00000000
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC                0x00000001
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL                 0x00000002
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_TARGETED            0x00000003
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE                   2:2
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE_MIMC       0x00000000
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE_MOMC       0x00000001
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_BANK                   6:3
    memOpD = FLD_SET_DRF(C36F,_MEM_OP_D, _OPERATION, _ACCESS_COUNTER_CLR, memOpD);
    DebugPrintf(GetTeeCode(), "AccessCounter: MEM_OP_D 0x%x (OPERATION = 0x%x)\n",
                              memOpD, DRF_VAL(C36F,_MEM_OP_D, _OPERATION, memOpD));
    switch(m_CounterType)
    {
    case LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC:
        memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, _MIMC, memOpD);
        break;
    case LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC:
        memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, _MOMC, memOpD);
        break;
    case LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL:
        memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, _ALL, memOpD);
        break;
    default:
        // Never be here because of a check in Execute(), unless there's spec change in related headers.
        break;
    }

    DebugPrintf(GetTeeCode(), "AccessCounter: (In-band) Untargeted clear for counter type 0x%x, MEM_OP_D 0x%x, OPERATION = 0x%x\n",
                              DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, memOpD), memOpD, DRF_VAL(C36F,_MEM_OP_D, _OPERATION, memOpD));
}

void PmAction_ClearAccessCounter::MemOpsTargeted(UINT32* pMemOpC,
        UINT32* pMemOpD) const
{
    UINT32& memOpC = *pMemOpC;
    UINT32& memOpD = *pMemOpD;
    // Targeted inband clear

    // #define LWC36F_MEM_OP_C                                            (0x00000030)
    // #define LWC36F_MEM_OP_C_ACCESS_COUNTER_CLR_TARGETED_NOTIFY_TAG            19:0
    // #define LWC36F_MEM_OP_D                                            (0x00000034)
    // #define LWC36F_MEM_OP_D_OPERATION                                        31:27
    // #define LWC36F_MEM_OP_D_OPERATION_ACCESS_COUNTER_CLR                0x00000016
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE                            1:0
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC                0x00000000
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC                0x00000001
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_ALL                 0x00000002
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_TARGETED            0x00000003
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE                   2:2
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE_MIMC       0x00000000
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_TYPE_MOMC       0x00000001
    // #define LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TARGETED_BANK                   6:3
    memOpC = FLD_SET_DRF_NUM(C36F, _MEM_OP_C, _ACCESS_COUNTER_CLR_TARGETED_NOTIFY_TAG,
        m_TargetedNotifyTag, memOpC);
    DebugPrintf(GetTeeCode(), "AccessCounter: MEM_OP_C 0x%x, TargetedNotifyTag 0x%x.\n",
                              memOpC, DRF_VAL(C36F, _MEM_OP_C, _ACCESS_COUNTER_CLR_TARGETED_NOTIFY_TAG, memOpC));
    memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _OPERATION, _ACCESS_COUNTER_CLR, memOpD);
    memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, _TARGETED, memOpD);
    DebugPrintf(GetTeeCode(), "AccessCounter: MEM_OP_D 0x%x (OPERATION = 0x%x and ClearType = 0x%x)\n",
                              memOpD, DRF_VAL(C36F, _MEM_OP_D, _OPERATION, memOpD), DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, memOpD));
    switch(m_CounterType)
    {
    case LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MIMC:
        memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_TYPE, _MIMC, memOpD);
        break;
    case LWC36F_MEM_OP_D_ACCESS_COUNTER_CLR_TYPE_MOMC:
        memOpD = FLD_SET_DRF(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_TYPE, _MOMC, memOpD);
        break;
    default:
        // Never be here because of a check in Execute(), unless there's spec change in related headers.
        break;
    }
    DebugPrintf(GetTeeCode(), "AccessCounter: MEM_OP_D 0x%x (OPERATION = 0x%x, ClearType = 0x%x, TargetedType = 0x%x)\n",
                              memOpD, DRF_VAL(C36F, _MEM_OP_D, _OPERATION, memOpD), DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, memOpD),
                              DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_TYPE, memOpD));
    memOpD = FLD_SET_DRF_NUM(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_BANK, m_TargetedBank, memOpD);

    DebugPrintf(GetTeeCode(), "AccessCounter: (In-band) Clear for ClearType = 0x%x. TargetedNotifyTag 0x%x, MEM_OP_C 0x%x; "
                              "Targeted Type 0x%x Bank 0x%x, MEM_OP_D 0x%x, OPERATION = 0x%x.\n",
                              DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TYPE, memOpD),
                              DRF_VAL(C36F, _MEM_OP_C, _ACCESS_COUNTER_CLR_TARGETED_NOTIFY_TAG, memOpC),
                              memOpC,
                              DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_TYPE, memOpD),
                              DRF_VAL(C36F, _MEM_OP_D, _ACCESS_COUNTER_CLR_TARGETED_BANK, memOpD),
                              memOpD,
                              DRF_VAL(C36F, _MEM_OP_D, _OPERATION, memOpD));
}

RC PmAction_ClearAccessCounter::WriteMethod(PmChannel *pChannel)
{
    UINT32 memOpC = 0;
    UINT32 memOpD = 0;

    if (m_bTargeted)
    {
        MemOpsTargeted(&memOpC, &memOpD);
    }
    else
    {
        MemOps(&memOpC, &memOpD);
    }

    RC rc;
    SemaphoreChannelWrapper *pChWrap =
        pChannel->GetModsChannel()->GetSemaphoreChannelWrapper();
    UINT32 subch = pChWrap ? pChWrap->GetLwrrentSubchannel() : 0;

    PM_BEGIN_INSERTED_METHODS(pChannel);

    CHECK_RC(pChannel->Write(
        subch,
        LWC36F_MEM_OP_C,
        2,
        memOpC,
        memOpD
        ));

    PM_END_INSERTED_METHODS();

    CHECK_RC(pChannel->Flush());

    return rc;
}

RC PmAction_ClearAccessCounter::InbandClear(GpuSubdevice* pSubdev)
{
    RC rc;
    PmChannels channels = GetPolicyManager()->GetActiveChannels(pSubdev->GetParentDevice());
    if (channels.size() < 1)
    {
        ErrPrintf("Action.ClearAccessCounter: No active channel found.\n");
        return RC::SOFTWARE_ERROR;
    }

    PmChannel *pChannel = channels[0];
    switch (pChannel->GetModsChannel()->GetClass())
    {
    case GF100_CHANNEL_GPFIFO:
    case KEPLER_CHANNEL_GPFIFO_A:
    case KEPLER_CHANNEL_GPFIFO_B:
    case KEPLER_CHANNEL_GPFIFO_C:
    case MAXWELL_CHANNEL_GPFIFO_A:
        ErrPrintf("Action.ClearAccessCounter: Incorrect channel class.\n");
        return RC::SOFTWARE_ERROR;
    default:
        // Call the Pascal function for Pascal and
        // newer chips.
        break;
    }
    return WriteMethod(pChannel);
}

RC PmAction_ClearAccessCounter::OutOfBandClear(GpuSubdevice* pSubdev)
{
    RegHal &regs = pSubdev->Regs();
    UINT32 regVal = regs.Read32(MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR);

    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR                                0x00100A1C /* -W-4R */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MIMC                                  0:0 /* -WIVF */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MIMC_CLR                              0x1 /* -W--V */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MOMC                                  1:1 /* -WIVF */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MOMC_CLR                              0x1 /* -W--V */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_ALL_COUNTERS                          2:2 /* -WIVF */
    // #define LW_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_ALL_COUNTERS_CLR                      0x1 /* -W--V */
    switch(m_CounterType)
    {
    case LWC365_CTRL_ACCESS_COUNTER_TYPE_MIMC:
        regs.SetField(&regVal, MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MIMC_CLR);
        break;
    case LWC365_CTRL_ACCESS_COUNTER_TYPE_MOMC:
        regs.SetField(&regVal, MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_MOMC_CLR);
        break;
    case LWC365_CTRL_ACCESS_COUNTER_TYPE_ALL:
        regs.SetField(&regVal, MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR_ALL_COUNTERS_CLR);
        break;
    default:
        // Never be here because of a check in Execute(), unless there's spec change in related headers.
        break;
    }

    regs.Write32(MODS_PFB_NISO_ACCESS_COUNTER_NOTIFY_BUFFER_CLR, regVal);
    DebugPrintf(GetTeeCode(), "AccessCounter: (Out-of-band) Clear for counter type 0x%x\n", m_CounterType);

    return OK;
}


//////////////////////////////////////////////////////////////////////
// PmAction_ResetAccessCounterCachedGetPointer
//////////////////////////////////////////////////////////////////////

PmAction_ResetAccessCounterCachedGetPointer::PmAction_ResetAccessCounterCachedGetPointer
(
    PmGpuDesc *pGpuDesc
):
    PmAction("Action.ResetAccessCounterCachedGetPointer"),
    m_pGpuDesc(pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ResetAccessCounterCachedGetPointer::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief reset cached GET pointer
//!
/* virtual */ RC PmAction_ResetAccessCounterCachedGetPointer::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    auto accessCounterInts = GetPolicyManager()->GetAccessCounterInts();

    for (auto pSubDev : m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent))
    {
        accessCounterInts[pSubDev]->ResetCachedGetPointer();
    }

    return rc;
}
//////////////////////////////////////////////////////////////////////
// PmAction_ForceNonReplayableFaultBufferOverflow
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ForceNonReplayableFaultBufferOverflow::PmAction_ForceNonReplayableFaultBufferOverflow
(
):
    PmAction("Action.PmAction_ForceNonReplayableFaultBufferOverflow")
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ForceNonReplayableFaultBufferOverflow::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief update GET = PUT + 1 % SIZE
//!
/* virtual */ RC PmAction_ForceNonReplayableFaultBufferOverflow::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    map<GpuSubdevice*,PmNonReplayableInt*> nonReplayableInts =
        GetPolicyManager()->GetNonReplayableInts();

    map<GpuSubdevice*,PmNonReplayableInt*>::iterator iter;

    for ( iter = nonReplayableInts.begin();
          iter != nonReplayableInts.end(); ++iter)
    {
        PmNonReplayableInt *pmNonReplayableInt =
            iter->second;
        CHECK_RC(pmNonReplayableInt->ForceNonReplayableFaultBufferOverflow());
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_StartVFTest
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_StartVFTest::PmAction_StartVFTest
(
    const PmVfTestDesc * pVfDesc
) :
    PmAction("Action.StartVFTest"),
   m_pVfDesc(pVfDesc)
{
    MASSERT(m_pVfDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_StartVFTest::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Start the indicated timer
//!
/* virtual */ RC PmAction_StartVFTest::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    vector<VmiopElwManager::VfConfig> vfConfigs;
    vector<VmiopMdiagElwManager::VFSetting*> vfSettings;
    for (auto* pVfTestDesc: m_pVfDesc->GetMatchingVfs(pTrigger, pEvent))
    {
        vfConfigs.push_back(*pVfTestDesc->GetVfSetting()->pConfig);
        // We are making a copy of the const pointer here so changing it to
        // non-const should be fine
        // Also, need to pass pointers to VFSetting here since
        // ConfigFrameBufferLength modifies the VFSetting data
        vfSettings.push_back(const_cast<VmiopMdiagElwManager::VFSetting*>(pVfTestDesc->GetVfSetting()));
    }

    if (vfConfigs.size() == 0)
    {
        ErrPrintf("Action.StartVFTest: Cannot find a matching VF.\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(VmiopMdiagElwManager::Instance()->RulwfTests(vfConfigs, vfSettings));
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WaitProcEvent
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WaitProcEvent::PmAction_WaitProcEvent
(
    const PmVfTestDesc * pVfDesc,
    const string & message
) :
    PmAction("Action.WaitProcEvent"),
    m_pVfDesc(pVfDesc),
    m_Message(message)
{
    MASSERT(m_pVfDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_WaitProcEvent::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Start the indicated timer
//!
/* virtual */ RC PmAction_WaitProcEvent::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    set<UINT32> gfidsToPoll;

    //Wait from PF which sequenceId or GFID equals 0
    if (m_pVfDesc->GetGFID().compare("0") == 0 ||
            m_pVfDesc->GetSeqId().compare("0") == 0)
    {
        UINT32 gfid = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
        gfidsToPoll.insert(gfid);
    }
    else
    {
        PmVfTests PmVfTests = m_pVfDesc->GetMatchingVfs(pTrigger, pEvent);
        for (auto it = PmVfTests.begin(); it != PmVfTests.end(); ++it)
        {
            UINT32 gfid = (*it)->GetGFID();
            gfidsToPoll.insert(gfid);
        }
    }

    if (gfidsToPoll.size() > 0)
    {
        CHECK_RC(MDiagUtils::PollSharedMemoryMessage(gfidsToPoll, m_Message));
    }
    else
    {
        WarnPrintf("No GFIDs found for Action.WaitProcEvent. Ignoring %s\n",
                   GetName().c_str());
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SendProcEvent
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SendProcEvent::PmAction_SendProcEvent
(
    const PmVfTestDesc * pVfDesc,
    const string & message
) :
    PmAction("Action.SendProcEvent"),
    m_pVfDesc(pVfDesc),
    m_Message(message)
{
    MASSERT(m_pVfDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SendProcEvent::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Start the indicated timer
//!
/* virtual */ RC PmAction_SendProcEvent::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();

    //Sent to PF which sequenceId or GFID equals 0
    if (m_pVfDesc->GetGFID().compare("0") == 0 ||
            m_pVfDesc->GetSeqId().compare("0") == 0)
    {
        UINT32 gfid = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
        VmiopMdiagElw * pVmiopMdiagElw =
            pVmiopMdiagElwMgr->GetVmiopMdiagElw(gfid);
        CHECK_RC(pVmiopMdiagElw->SendProcMessage(m_Message.size(),
                    m_Message.c_str()));
    }
    else
    {
        PmVfTests PmVfTests = m_pVfDesc->GetMatchingVfs(pTrigger, pEvent);
        for (auto it = PmVfTests.begin(); it != PmVfTests.end(); ++it)
        {
            UINT32 gfid = (*it)->GetGFID();
            VmiopMdiagElw * pVmiopMdiagElw =
                pVmiopMdiagElwMgr->GetVmiopMdiagElw(gfid);
            CHECK_RC(pVmiopMdiagElw->SendProcMessage(m_Message.size(),
                        m_Message.c_str()));
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WaitErrorLoggerInterrupt
//////////////////////////////////////////////////////////////////////
//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WaitErrorLoggerInterrupt::PmAction_WaitErrorLoggerInterrupt
(
    const PmGpuDesc *pGpuDesc,
    const std::string &Name,
    FLOAT64 TimeoutMs
):
    PmAction("Action.WaitErrorLoggerInterrupt"),
    m_pGpuDesc(pGpuDesc),
    m_Name(Name),
    m_TimeoutMs(TimeoutMs)
{
    MASSERT(pGpuDesc != 0);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_WaitErrorLoggerInterrupt::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pGpuDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! \brief Wait for a matching interrupt to be logged
//!
/* virtual */ RC PmAction_WaitErrorLoggerInterrupt::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    auto pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr != nullptr);
    if (pGpuDevMgr->NumDevices() > 1 || pGpuDevMgr->NumGpus() > 1)
    {
        ErrPrintf("Action.WaitErrorLoggerInterrupt: Multi-GPU not supported.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    GpuSubdevices gpuSubdevices =
        m_pGpuDesc->GetMatchingSubdevices(pTrigger, pEvent);

    if (0 == gpuSubdevices.size())
    {
        ErrPrintf("Action.WaitErrorLoggerInterrupt: Can't find gpuSubdevice specified.\n");
        return RC::BAD_PARAMETER;
    }

    for (GpuSubdevices::iterator ppGpuSubdevice = gpuSubdevices.begin();
         ppGpuSubdevice != gpuSubdevices.end(); ++ppGpuSubdevice)
    {
        const PmErrorLoggerInt::Events *pEvents =
            PolicyManager::Instance()->GetErrorLoggerIntMap(
                *ppGpuSubdevice)->GetEvents();
        for (const auto &event : *pEvents)
        {
            if (std::get<0>(event) == m_Name)
            {
                m_WaitEvents.push_back(std::get<1>(event));
            }
        }
    }

    // Only count events that are triggered after we start waiting.
    for (const auto &waitEvent : m_WaitEvents)
    {
        waitEvent->Clear();
    }

    return POLLWRAP_HW(PmAction_WaitErrorLoggerInterrupt::PollInterrupts, this,
        m_TimeoutMs);
}

/* static */ bool PmAction_WaitErrorLoggerInterrupt::PollInterrupts(void *pvArgs)
{
    PmErrorLoggerInt::Event *pLwrEvent;
    PmAction_WaitErrorLoggerInterrupt *pThis;
    vector<PmErrorLoggerInt::Event*>::iterator pWaitEntry;

    pThis = static_cast<PmAction_WaitErrorLoggerInterrupt*>(pvArgs);

    pWaitEntry = pThis->m_WaitEvents.begin();

    while (pWaitEntry != pThis->m_WaitEvents.end())
    {
        pLwrEvent = *pWaitEntry;

        if (pLwrEvent->IsSet())
        {
            pWaitEntry = pThis->m_WaitEvents.erase(pWaitEntry);
        }
        else
        {
            pWaitEntry++;
        }
    }

    return pThis->m_WaitEvents.empty();
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAction_WaitErrorLoggerInterrupt::~PmAction_WaitErrorLoggerInterrupt()
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_VirtFunctionLevelReset
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_VirtFunctionLevelReset::PmAction_VirtFunctionLevelReset
(
    const PmVfTestDesc *pVfTestDesc
) :
    PmAction("Action.VirtFunctionLevelReset"),
    m_pVfTestDesc(pVfTestDesc)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_VirtFunctionLevelReset::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief virtual function associated with this VF level reset .
//!
/* virtual */ RC PmAction_VirtFunctionLevelReset::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmVfTests pmVfTests = m_pVfTestDesc->GetMatchingVfs(pTrigger, pEvent);
    RC rc;

    for (auto it = pmVfTests.begin();
         it != pmVfTests.end(); ++it)
    {
        UINT32 bus = (*it)->GetBus();
        UINT32 domain = (*it)->GetDomain();
        UINT32 function = (*it)->GetFunction();
        UINT32 device = (*it)->GetDevice();

        VmiopMdiagElw* pElw = (*it)->GetVmiopMdiagElw();
        const UINT32 gfid = pElw->GetGfid();
        pElw->StartFlr();

        VmiopMdiagElwManager* pVmiopElwMgr = VmiopMdiagElwManager::Instance();
        if (pVmiopElwMgr->IsEarlyPluginUnload())
        {
            DebugPrintf("PolicyManager: PmAction_VirtFunctionLevelReset to wait GFID %d for vGpu plugin unload ...\n", gfid);
            // Wait until VmiopElw ExitStarted or PhysFun thread shutdown and VmiopElwMgr released the VmiopElw.
            Tasker::PollHw(Tasker::NO_TIMEOUT,[gfid, pVmiopElwMgr]()->bool
                    {
                        VmiopMdiagElw* pSafeElw = pVmiopElwMgr->GetVmiopMdiagElw(gfid);
                        if (!pSafeElw || pSafeElw->IsExitStarted())
                        {
                            return true;
                        }
                        return false;
                    },
                    __FUNCTION__);
            DebugPrintf("PolicyManager: PmAction_VirtFunctionLevelReset, GFID %d vGpu plugin unload done and VmiopElw exit complete.\n");
        }
        else
        {
            DebugPrintf("PolicyManager: PmAction_VirtFunctionLevelReset early vplugin unload not required.\n");
        }
        rc = MDiagUtils::VirtFunctionReset(bus, domain, function, device);
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_WaitFlrDone
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_WaitFlrDone::PmAction_WaitFlrDone
(
    const PmVfTestDesc *pVfTestDesc
) :
    PmAction("Action.WaitFlrDone"),
    m_pVfTestDesc(pVfTestDesc)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_WaitFlrDone::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Wait the virtual function associated with the VF FLR done.
//   And release the plugin.
//!
/* virtual */ RC PmAction_WaitFlrDone::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmVfTests pmVfTests = m_pVfTestDesc->GetMatchingVfs(pTrigger, pEvent);
    RC rc;

    for (auto it = pmVfTests.begin();
         it != pmVfTests.end(); ++it)
    {
        UINT32 GFID = (*it)->GetGFID();

        DebugPrintf("PolicyManager: PmAction_WaitFlrDone, Wait GFID %d.\n", GFID);
        CHECK_RC(MDiagUtils::WaitFlrDone(GFID));
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ClearFlrPendingBit
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ClearFlrPendingBit::PmAction_ClearFlrPendingBit
(
    const PmVfTestDesc *pVfTestDesc
) :
    PmAction("Action.ClearFlrPendingBit"),
    m_pVfTestDesc(pVfTestDesc)
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ClearFlrPendingBit::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Clear the VF's pending bit.
//!
/* virtual */ RC PmAction_ClearFlrPendingBit::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    PmVfTests pmVfTests = m_pVfTestDesc->GetMatchingVfs(pTrigger, pEvent);
    RC rc;
    vector<UINT32> data;

    MDiagUtils::ReadFLRPendingBits(&data);

    for (auto it = pmVfTests.begin();
         it != pmVfTests.end(); ++it)
    {
        UINT32 GFID = (*it)->GetGFID();
        MDiagUtils::UpdateFLRPendingBits(GFID, &data);
    }

    MDiagUtils::ClearFLRPendingBits(data);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ExitLwrrentVfProcess
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAction_ExitLwrrentVfProcess::PmAction_ExitLwrrentVfProcess
(
) :
    PmAction("Action.ExitLwrrentVfProcess")
{
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ExitLwrrentVfProcess::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief shutdown the vf process and protect from the hw transaction.
//!
/* virtual */ RC PmAction_ExitLwrrentVfProcess::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    Platform::SetForcedTermination();

    CHECK_RC(MDiagUtils::TagTrepFilePass());

    // Flush mods.log.
    Tee::FlushToDisk();

   // Flush standard out.
   fflush(stdout);

   // exit current VF process directly instead of calling Utility::ExitMods
   // to avoid crashing issues introduced by GPU shutdown/Cleanup.
   // bug 200492379 && bug 2489071
   _exit(0);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnEngineIsExisting
//////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_OnEngineIsExisting::PmAction_OnEngineIsExisting
(
    const string &engineName,
    const PmSmcEngineDesc *pSmcEngineDesc
) :
    PmAction_Goto("Action.OnEngineIsExisting"),
    m_EngineName(engineName),
    m_pSmcEngineDesc(pSmcEngineDesc)
{
    MASSERT(!engineName.empty());
}

//--------------------------------------------------------------------
//! brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_OnEngineIsExisting::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! brief Abort the test associated with the channel.
//!
/* virtual */ RC PmAction_OnEngineIsExisting::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    bool *pGoto
)
{
    RC rc;
    PmSmcEngine * pPmSmcEngine = nullptr;
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;

    if (m_pSmcEngineDesc)
    {
        PmSmcEngines smcEngines = m_pSmcEngineDesc->GetMatchingSmcEngines(pTrigger, pEvent);
        MASSERT(smcEngines.size() <= 1);

        if (smcEngines.size() == 0)
        {
            DebugPrintf("Spcified SmcEngine syspipe = %s can't be accessable at the policy file.\n",
                        m_pSmcEngineDesc->GetSysPipe().c_str());
            *pGoto = true;
            return rc;
        }
        pPmSmcEngine = smcEngines[0];
    }

    if (m_EngineName == "graphics")
    {
        engineId = LW2080_ENGINE_TYPE_GRAPHICS;
        if (pPmSmcEngine)
        {
            engineId = MDiagUtils::GetGrEngineId(0);
        }
    }
    else if (m_EngineName.compare(0, 4, "copy") == 0)
    {
        const char * format = "copy%d";
        UINT32 ceIndex;

        if (1 != sscanf(m_EngineName.c_str(), format, &ceIndex))
        {
            ErrPrintf("Action.OnEngineIsExisting: Need the Engine Name match the syntax copy%d.\n");
            *pGoto = true;
            return RC::SOFTWARE_ERROR;
        }

        engineId = MDiagUtils::GetCopyEngineId(ceIndex);
    }
    else
    {
        MASSERT(!"Just support engine name: graphics and copy%d");
        *pGoto = true;
        return RC::SOFTWARE_ERROR;
    }

    PolicyManager * pPolicyManager = PolicyManager::Instance();
    GpuDevices gpuDevices = pPolicyManager->GetGpuDevices();
    for (auto const & gpuDevice : gpuDevices)
    {
        LwRm * pLwRm = LwRmPtr().Get();
        if (pPmSmcEngine)
            pLwRm = pPmSmcEngine->GetLwRmPtr();

        vector<UINT32> engines;
        CHECK_RC(gpuDevice->GetEngines(&engines, pLwRm));
        if (find(engines.begin(), engines.end(), engineId)
                == engines.end())
        {
            *pGoto = true;
            return rc;
        }
    }

    *pGoto = false;
    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SaveCHRAM
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_SaveCHRAM::PmAction_SaveCHRAM
(
    const PmChannelDesc *pChannelDesc,
    const string& filename
) :
    PmAction("Action.SaveCHRAM"),
    m_pChannelDesc(pChannelDesc),
    m_Filename(filename)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SaveCHRAM::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! brief that source channel dump channel RAM to a file.
//!
/* virtual */ RC PmAction_SaveCHRAM::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    FileHolder fh;
    CHECK_RC(fh.Open(m_Filename.c_str(), "w"));
    fprintf(fh.GetFile(), CHRAM_FILE_HDR "\n");

    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (auto& ppChannel : channels)
    {
        LwRm* pLwRm = ppChannel->GetLwRmPtr();
        GpuDevice *pDevice = ppChannel->GetGpuDevice();
        UINT32 channelId = ppChannel->GetModsChannel()->GetChannelId();
        UINT32 engineId = ppChannel->GetEngineId();

        string testName = ppChannel->GetTest()->GetName();
        string channelName = ppChannel->GetName();
        SmcEngine* pSmcEngine = ppChannel->GetPmSmcEngine() ? ppChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr;

        for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
        {
            GpuSubdevice *pSubdev = pDevice->GetSubdevice(subdev);
            LWGpuResource *pGpuRes = GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);
            RegHal& regs = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

            LwU32 data;
            UINT32 chConfig;
            UINT32 chramBase = 0;
            UINT32 runlistBase = 0;

            // Can't access MODS_CHRAM_CHANNEL directly on VF since the register does not exist. So access it by RM.
            if (Platform::IsVirtFunMode())
            {
                runlistBase = pSubdev->GetDomainBase(engineId, RegHalDomain::RUNLIST, pLwRm);
                chConfig = pSubdev->RegOpRd32(runlistBase + LW_RUNLIST_CHANNEL_CONFIG, pLwRm);
                chramBase = DRF_VAL(_RUNLIST, _CHANNEL_CONFIG, _CHRAM_BAR0_OFFSET, chConfig) << DRF_BASE(LW_RUNLIST_CHANNEL_CONFIG_CHRAM_BAR0_OFFSET);

                UINT32 phyChannelId = MDiagUtils::GetPhysChannelId(channelId, pLwRm, engineId);
                DebugPrintf("PmAction_SaveCHRAM: SRIOV vf mode: physical channel id %d, virtual channel id %d \n", phyChannelId, channelId);
                data =  pSubdev->RegOpRd32(chramBase + LW_CHRAM_CHANNEL(phyChannelId), pLwRm);
            }
            else
            {
                data = regs.Read32(engineId, MODS_CHRAM_CHANNEL, channelId);
            }

            fprintf(fh.GetFile(), CHRAM_FILE_FORMAT "\n", testName.c_str(), channelName.c_str(), subdev, data);
            DebugPrintf("PmAction_SaveCHRAM: Source Channel on subdevice %d, which channelId is %d, engineId is %d, channelName is %s, testName is %s,"
                        "save Channel RAM data %d to file %s \n",
                        subdev, channelId, engineId, channelName.c_str(), testName.c_str(), data, m_Filename.c_str());
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// Action.RestoreCHRAM
//////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_RestoreCHRAM::PmAction_RestoreCHRAM
(
    const PmChannelDesc *pChannelDesc,
    const string& filename
) :
    PmAction("Action.RestoreCHRAM"),
    m_pChannelDesc(pChannelDesc),
    m_Filename(filename)
{
    MASSERT(pChannelDesc != NULL);
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_RestoreCHRAM::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return (m_pChannelDesc->IsSupportedInAction(pTrigger));
}

//--------------------------------------------------------------------
//! brief that dst channel load channel RAM from a file.
//!
/* virtual */ RC PmAction_RestoreCHRAM::Execute
(
    PmTrigger     *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    // Parse file data
    FileHolder fh;
    CHECK_RC(fh.Open(m_Filename.c_str(), "r"));

    vector<CHRAMParseData> CHRAMParseResults;
    char line[MAX_LINE];
    char testName[MAX_LINE];
    char channelName[MAX_LINE];

    char *ret = fgets(line, MAX_LINE, fh.GetFile());
    while (ret && (!feof(fh.GetFile())))
    {
        if (fgets(line, MAX_LINE, fh.GetFile()))
        {
            CHRAMParseData CHRAMParseResult("", "", 0, 0);
            sscanf(line, CHRAM_FILE_FORMAT, testName, channelName,
                   &CHRAMParseResult.subdevice, &CHRAMParseResult.data);
            CHRAMParseResult.testName = testName;
            CHRAMParseResult.channelName = channelName;
            CHRAMParseResults.push_back(move(CHRAMParseResult));
        }
    }

    // Restore channel RAM to dst channel
    PmChannels channels = m_pChannelDesc->GetMatchingChannels(pTrigger, pEvent);
    for (auto& ppChannel : channels)
    {
        LwRm* pLwRm = ppChannel->GetLwRmPtr();
        GpuDevice *pDevice = ppChannel->GetGpuDevice();
        UINT32 channelId = ppChannel->GetModsChannel()->GetChannelId();
        UINT32 engineId = ppChannel->GetEngineId();

        string testName = ppChannel->GetTest()->GetName();
        string channelName = ppChannel->GetName();
        SmcEngine* pSmcEngine = ppChannel->GetPmSmcEngine() ? ppChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr;

        for (auto& CHRAMParseResult : CHRAMParseResults)
        {
            if (CHRAMParseResult.testName == testName &&
                CHRAMParseResult.channelName == channelName)
            {
                GpuSubdevice *pSubdev = pDevice->GetSubdevice(CHRAMParseResult.subdevice);
                LWGpuResource *pGpuRes = GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);
                RegHal& regs = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

                UINT32 chConfig;
                UINT32 chramBase = 0;
                UINT32 runlistBase = 0;

                // Can't access MODS_CHRAM_CHANNEL directly on VF
                if (Platform::IsVirtFunMode())
                {
                    runlistBase = pSubdev->GetDomainBase(engineId, RegHalDomain::RUNLIST, pLwRm);
                    chConfig = pSubdev->RegOpRd32(runlistBase + LW_RUNLIST_CHANNEL_CONFIG, pLwRm);
                    chramBase = DRF_VAL(_RUNLIST, _CHANNEL_CONFIG, _CHRAM_BAR0_OFFSET, chConfig) << DRF_BASE(LW_RUNLIST_CHANNEL_CONFIG_CHRAM_BAR0_OFFSET);

                    UINT32 phyChannelId = MDiagUtils::GetPhysChannelId(channelId, pLwRm, engineId);
                    DebugPrintf("PmAction_RestoreCHRAM: SRIOV vf mode: physical channel id %d, virtual channel id %d \n", phyChannelId, channelId);
                    pSubdev->RegOpWr32(chramBase + LW_CHRAM_CHANNEL(phyChannelId), LW_CHRAM_CHANNEL_UPDATE_CLEAR_CHANNEL, pLwRm);
                    pSubdev->RegOpWr32(chramBase + LW_CHRAM_CHANNEL(phyChannelId), CHRAMParseResult.data, pLwRm);
                }
                else
                {
                    regs.Write32(engineId, MODS_CHRAM_CHANNEL, channelId, LW_CHRAM_CHANNEL_UPDATE_CLEAR_CHANNEL);
                    regs.Write32(engineId, MODS_CHRAM_CHANNEL, channelId, CHRAMParseResult.data);
                }

                DebugPrintf("PmAction_RestoreCHRAM: Dst channel on subdev %d, which channelId is %d, engineId is %d, channelName is %s, testName is %s,"
                            "restore Channel RAM data %d to file %s \n",
                            CHRAMParseResult.subdevice, channelId, engineId, channelName.c_str(), testName.c_str(), CHRAMParseResult.data, m_Filename.c_str());
            }
        }
    }

    return rc;
}

PmAction_RestoreCHRAM::CHRAMParseData::CHRAMParseData
(
    const string& testName,
    const string& chanName,
    UINT32 subdev,
    LwU32 data
) :
    testName(testName),
    channelName(chanName),
    subdevice(subdev),
    data(data)
{
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbCopyBase
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbCopyBase::PmAction_FbCopyBase
(
    const string& actionName,
    bool bRead,
    const string& fileName
) :
    PmAction(actionName),
    m_File(fileName),
    m_bRead(bRead)
{
    GetMDiagvGpuMigration()->Enable();
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_FbCopyBase::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Read/write FB mem.
//!
/* virtual */ RC PmAction_FbCopyBase::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    MDiagVmScope::StreamTask task;
    MDiagVmObjs vm;
    vm.m_pTask = &task;
    CHECK_RC(Setup(pTrigger, pEvent, &vm));
    // AllocBa1 will map the fb segment directly to bar1, while
    // AllocSurf will use a mdiagSurf object to represent the FB segment.
    task.SetAllocType(m_bUseMdiagSurf ? MDiagVmScope::AllocSurf : MDiagVmScope::AllocBar1);
    if (m_bRead)
    {
        CHECK_RC(Read(&vm));
    }
    else
    {
        CHECK_RC(Write(&vm));
    }
    Cleanup(&vm);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Read FB mem.
//!
RC PmAction_FbCopyBase::Read(MDiagVmObjs* pVm)
{
    MASSERT(pVm != nullptr);

    RC rc;

    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MASSERT(pTask != nullptr);
    if (m_File.size() > 0)
    {
        CHECK_RC(pTask->SaveMemToFile(m_File));
    }
    else
    {
        MDiagVmScope::MemData dataBuf;
        CHECK_RC(pTask->ReadMem(&dataBuf));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write FB mem.
//!
RC PmAction_FbCopyBase::Write(MDiagVmObjs* pVm)
{
    MASSERT(pVm != nullptr);

    RC rc;

    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MASSERT(pTask != nullptr);
    MASSERT(pTask->GetDstTaskDesc() != nullptr);
    if (m_File.size() > 0)
    {
        CHECK_RC(pTask->RestoreMemFromFile(m_File));
    }
    else
    {
        CHECK_RC(pTask->WriteSavedMem());
    }

    return rc;
}

/* virtual */ void PmAction_FbCopyBase::Cleanup
(
    MDiagVmObjs* pVm
)
{
    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::Task::Desc* pDesc = pTask->GetDstTaskDesc();
    pDesc->GetCreator()->FreeDesc(pDesc);
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbCopy
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbCopy::PmAction_FbCopy
(
    const string& actionName,
    bool bRead,
    const PmVfTestDesc* pVfDesc,
    const PmSmcEngineDesc* pSmcEngDesc,
    const string& fileName
)
    : PmAction_FbCopyBase(actionName, bRead, fileName),
    m_pVfDesc(pVfDesc),
    m_pSmcEngDesc(pSmcEngDesc)
{
    // Must be: one of the two is NULL but the other is not NULL
    MASSERT(!(m_pVfDesc && m_pSmcEngDesc) &&
        (m_pVfDesc || m_pSmcEngDesc));
}

//--------------------------------------------------------------------
//! \brief Set up FbCopy
//!
/* virtual */ RC PmAction_FbCopy::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    if (m_pVfDesc != nullptr)
    {
        //PF sequenceId is 0, or SRIOV not enabled.
        if (m_pVfDesc->GetGFID().compare("0") == 0 ||
                m_pVfDesc->GetSeqId().compare("0") == 0)
        {
            MASSERT(!Platform::IsVirtFunMode());
            return SetupWholeFb(pVm);
        }

        MASSERT(Platform::IsPhysFunMode());
        return SetupSriov(pTrigger, pEvent, pVm);
    }

    return SetupSmc(pTrigger, pEvent, pVm);
}

RC PmAction_FbCopy::SetupWholeFb(MDiagVmObjs* pVm)
{
    RC rc;

    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::Task::Desc* pDesc = GetMDiagVmResources()->CreateTaskDescWholeFb();
    pTask->SetDstTaskDesc(pDesc);

    return rc;
}

RC PmAction_FbCopy::SetupSmc
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    MASSERT(Platform::IsDefaultMode());
    PmSmcEngines smcEngines = m_pSmcEngDesc->GetMatchingSmcEngines(pTrigger, pEvent);
    MASSERT(smcEngines.size() > 0);
    const UINT32 syspipe = smcEngines[0]->GetSysPipe();
    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::Task::Desc* pTaskDesc = MDiagVmScope::GetSmcResources()->CreateTaskDesc(syspipe);
    if (pTaskDesc != nullptr)
    {
        pTask->SetDstTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC PmAction_FbCopy::SetupSriov
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    PmVfTests vfTests = m_pVfDesc->GetMatchingVfs(pTrigger, pEvent);
    if (vfTests.size() == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }
    PmVfTest* pVfTest = vfTests.front();
    DebugPrintf("PmAction_FbCopy::%s, " vGpuMigrationTag " Using GFID 0x%x.\n",
                __FUNCTION__, pVfTest->GetGFID());
    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::Task::Desc* pTaskDesc =
            GetMDiagVmResources()->CreateTaskDescByGfid(pVfTest->GetGFID());
    if (pTaskDesc != nullptr)
    {
        pTask->SetDstTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbCopySeg
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbCopySeg::PmAction_FbCopySeg
(
    const string& actionName,
    bool bRead,
    const string& fileName,
    UINT64 paOff,
    UINT64 size
)
    : PmAction_FbCopyBase(actionName, bRead, fileName),
    m_PaOff(paOff),
    m_Size(size)
{
}

//--------------------------------------------------------------------
//! \brief Setup FB copy.
//!
/* virtual */ RC PmAction_FbCopySeg::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::MemSegDesc seg(m_PaOff, m_Size);
    MDiagVmScope::Task::Desc* pTaskDesc = GetMDiagVmResources()->CreateTaskDescSingleSeg(seg);
    pTask->SetDstTaskDesc(pTaskDesc);

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbCopySegSurf
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbCopySegSurf::PmAction_FbCopySegSurf
(
    const string& actionName,
    bool bRead,
    const string& fileName,
    UINT64 paOff,
    UINT64 size
)
    : PmAction_FbCopySeg(actionName, bRead, fileName, paOff, size)
{
    SetUseMdiagSurf();
}

//////////////////////////////////////////////////////////////////////
// PmAction_FbCopyRunlist
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_FbCopyRunlist::PmAction_FbCopyRunlist
(
    const string& actionName,
    bool bRead,
    const PmChannelDesc* pDesc,
    const string& fileName
)
    : PmAction_FbCopyBase(actionName, bRead, fileName),
    m_pDesc(pDesc)
{
    MASSERT(m_pDesc != nullptr);
}

//--------------------------------------------------------------------
//! \brief Setup FbCopy
//!
/* virtual */ RC PmAction_FbCopyRunlist::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    UINT32 engId = 0;
    PmChannels channels = m_pDesc->GetMatchingChannels(pTrigger, pEvent);
    MASSERT(channels.size() > 0);
    auto pChannel = channels[0];
    GpuDevice* pGpuDev = pChannel->GetGpuDevice();
    engId = pChannel->GetEngineId();
    DebugPrintf("PmAction_FbCopyRunlist::%s, " vGpuMigrationTag " EngID 0x%x Channel # 0x%x(%d) ID 0x%x(%d) name %s handle 0x%x.\n",
                __FUNCTION__, engId, pChannel->GetChannelNumber(), pChannel->GetChannelNumber(), pChannel->GetModsChannel()->GetChannelId(),
                pChannel->GetModsChannel()->GetChannelId(), pChannel->GetName().c_str(), pChannel->GetHandle());

    GpuSubdevice* pSubdev = pGpuDev->GetSubdevice(0);
    LWGpuResource *pLwGpu = GetPolicyManager()->GetLWGpuResourceBySubdev(pSubdev);
    LwRm* pLwRm = pChannel->GetLwRmPtr();

    MDiagVmScope::Runlists::Resources res;
    res.Init(pLwGpu, pLwRm, engId, 
        pChannel->GetPmSmcEngine() ? pChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr);

    MDiagVmScope::StreamTask* pTask = pVm->GetStreamTask();
    MDiagVmScope::Task::Desc* pTaskDesc = nullptr;
    if (IsRead())
    {
        pTaskDesc = MDiagVmScope::GetRunlists()->CreateTaskDesc(res);
    }
    else
    {
        pTaskDesc = MDiagVmScope::GetRunlists()->GetDesc(res.GetEngineId());
    }
    MASSERT(pTaskDesc != nullptr);
    pTask->SetDstTaskDesc(pTaskDesc);
    pTask->SetHexFileNumData(1);

    return rc;
}

/* virtual */ void PmAction_FbCopyRunlist::Cleanup
(
    MDiagVmObjs* pVm
)
{
    if (!IsRead())
    {
        PmAction_FbCopyBase::Cleanup(pVm);
    }
}

//////////////////////////////////////////////////////////////////////
// PmAction_DmaFbCopyBase
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_DmaFbCopyBase::PmAction_DmaFbCopyBase
(
    const string& actionName
)
    : PmAction(actionName)
{
    GetMDiagvGpuMigration()->Enable();
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_DmaFbCopyBase::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Read/write FB mem.
//!
/* virtual */ RC PmAction_DmaFbCopyBase::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    MDiagVmScope::CopyTask task;
    MDiagVmObjs vm;
    vm.m_pTask = &task;
    CHECK_RC(Setup(pTrigger, pEvent, &vm));
    if (m_Cfg.m_b2StepsCopy)
    {
        task.SetSurfList(GetMDiagVmResources()->GetSurfList());
        if (m_Cfg.m_bRead)
        {
            CHECK_RC(task.Save(m_Cfg.m_bSysmem));
        }
        else
        {
            CHECK_RC(task.Restore());
        }
    }
    else
    {
        CHECK_RC(task.Copy());
    }
    Cleanup(&vm);

    return rc;
}

/* virtual */ void PmAction_DmaFbCopyBase::Cleanup
(
    MDiagVmObjs* pVm
)
{
    MDiagVmScope::CopyTask* pTask = pVm->GetCopyTask();
    MDiagVmScope::Task::Desc* pDesc = pTask->GetSrcTaskDesc();
    if (pDesc)
    {
        pDesc->GetCreator()->FreeDesc(pDesc);
    }
    pDesc = pTask->GetDstTaskDesc();
    if (pDesc)
    {
        pDesc->GetCreator()->FreeDesc(pDesc);
    }
}

//////////////////////////////////////////////////////////////////////
// PmAction_DmaCopyFb
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_DmaCopyFb::PmAction_DmaCopyFb
(
    const PmVfTestDesc* pVfDescFrom,
    const PmVfTestDesc* pVfDescTo,
    const PmSmcEngineDesc* pSmcEngDescFrom,
    const PmSmcEngineDesc* pSmcEngDescTo
) :
    PmAction_DmaFbCopyBase("Action.DmaCopyFb"),
    m_pVfDescFrom(pVfDescFrom),
    m_pVfDescTo(pVfDescTo),
    m_pSmcEngDescFrom(pSmcEngDescFrom),
    m_pSmcEngDescTo(pSmcEngDescTo)
{
    // Must be: one of the two is NULL but the other is not NULL
    MASSERT(!(m_pVfDescFrom && m_pSmcEngDescFrom) &&
        (m_pVfDescFrom || m_pSmcEngDescFrom));
    // The same as above
    MASSERT(!(m_pVfDescTo && m_pSmcEngDescTo) &&
        (m_pVfDescTo || m_pSmcEngDescTo));

    // Must be: both of the two are NULL, or both not NULL
    MASSERT((nullptr == m_pVfDescFrom && nullptr == m_pVfDescTo) ||
        (nullptr != m_pVfDescFrom && nullptr != m_pVfDescTo));
}

//--------------------------------------------------------------------
//! \brief Set up CE FB Copy.
//!
/* virtual */ RC PmAction_DmaCopyFb::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;
    if (m_pVfDescFrom != nullptr)
    {
        MASSERT(Platform::IsPhysFunMode());
        //Not support sequenceId 0 case.
        if (m_pVfDescFrom->GetGFID().compare("0") == 0 ||
                m_pVfDescFrom->GetSeqId().compare("0") == 0)
        {
            return RC::ILWALID_ARGUMENT;
        }
        if (m_pVfDescTo->GetGFID().compare("0") == 0 ||
                m_pVfDescTo->GetSeqId().compare("0") == 0)
        {
            return RC::ILWALID_ARGUMENT;
        }
        return SetupSriov(pTrigger, pEvent, pVm);
    }

    return SetupSmc(pTrigger, pEvent, pVm);
}

RC PmAction_DmaCopyFb::SetupSmc
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    MASSERT(Platform::IsDefaultMode());
    MDiagVmScope::CopyTask* pTask = pVm->GetCopyTask();

    PmSmcEngines smcEngines = m_pSmcEngDescFrom->GetMatchingSmcEngines(pTrigger, pEvent);
    MASSERT(smcEngines.size() > 0);
    UINT32 syspipe = smcEngines[0]->GetSysPipe();
    MDiagVmScope::Task::Desc* pTaskDesc = MDiagVmScope::GetSmcResources()->CreateTaskDesc(syspipe);
    if (pTaskDesc != nullptr)
    {
        pTask->SetSrcTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    smcEngines = m_pSmcEngDescTo->GetMatchingSmcEngines(pTrigger, pEvent);
    MASSERT(smcEngines.size() > 0);
    syspipe = smcEngines[0]->GetSysPipe();
    pTaskDesc = MDiagVmScope::GetSmcResources()->CreateTaskDesc(syspipe);
    if (pTaskDesc != nullptr)
    {
        pTask->SetDstTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC PmAction_DmaCopyFb::SetupSriov
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    MDiagVmScope::CopyTask* pTask = pVm->GetCopyTask();

    PmVfTests vfTests = m_pVfDescFrom->GetMatchingVfs(pTrigger, pEvent);
    if (vfTests.size() == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }
    PmVfTest* pVfTest = vfTests.front();
    DebugPrintf("PmAction_DmaCopyFb::%s, " vGpuMigrationTag " Using source GFID 0x%x.\n",
                __FUNCTION__, pVfTest->GetGFID());
    MDiagVmScope::Task::Desc* pTaskDesc =
                MDiagVmScope::GetSriovResources()->CreateTaskDesc(pVfTest->GetGFID());
    if (pTaskDesc != nullptr)
    {
        pTask->SetSrcTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    vfTests = m_pVfDescTo->GetMatchingVfs(pTrigger, pEvent);
    if (vfTests.size() == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }
    pVfTest = vfTests.front();
    DebugPrintf("PmAction_DmaCopyFb::%s, " vGpuMigrationTag " Using dest GFID 0x%x.\n",
                __FUNCTION__, pVfTest->GetGFID());
    pTaskDesc = MDiagVmScope::GetSriovResources()->CreateTaskDesc(pVfTest->GetGFID());
    if (pTaskDesc != nullptr)
    {
        pTask->SetDstTaskDesc(pTaskDesc);
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_2StepsDmaCopyFb
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_2StepsDmaCopyFb::PmAction_2StepsDmaCopyFb
(
    const string& actionName,
    const PmVfTestDesc* pVfDesc,
    bool bRead,
    bool bSysmem
) :
    PmAction_DmaFbCopyBase(actionName),
    m_pVfDesc(pVfDesc)
{
    MASSERT(m_pVfDesc);
    m_Cfg.m_b2StepsCopy = true;
    m_Cfg.m_bRead = bRead;
    m_Cfg.m_bSysmem = bSysmem;
    SetConfig(m_Cfg);
}

//--------------------------------------------------------------------
//! \brief Set up CE FB Copy.
//!
/* virtual */ RC PmAction_2StepsDmaCopyFb::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;
    MASSERT(Platform::IsPhysFunMode());
    //Not support sequenceId 0 case.
    if (m_pVfDesc->GetGFID().compare("0") == 0 ||
            m_pVfDesc->GetSeqId().compare("0") == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }

    MDiagVmScope::CopyTask* pTask = pVm->GetCopyTask();

    PmVfTests vfTests = m_pVfDesc->GetMatchingVfs(pTrigger, pEvent);
    if (vfTests.size() == 0)
    {
        return RC::ILWALID_ARGUMENT;
    }
    PmVfTest* pVfTest = vfTests.front();
    DebugPrintf("PmAction_IndirectDmaCopyFb::%s, " vGpuMigrationTag " Using GFID 0x%x.\n",
                __FUNCTION__, pVfTest->GetGFID());
    MDiagVmScope::Task::Desc* pTaskDesc =
                MDiagVmScope::GetSriovResources()->CreateTaskDesc(pVfTest->GetGFID());
    if (pTaskDesc != nullptr)
    {
        if (m_Cfg.m_bRead)
        {
            pTask->SetSrcTaskDesc(pTaskDesc);
            MASSERT(nullptr == pTask->GetDstTaskDesc());
        }
        else
        {
            pTask->SetDstTaskDesc(pTaskDesc);
            MASSERT(nullptr == pTask->GetSrcTaskDesc());
        }
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_DmaCopySeg
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_DmaCopySeg::PmAction_DmaCopySeg
(
    const string& actionName,
    bool bRead,
    PHYSADDR paOff,
    UINT64 size
) :
    PmAction_DmaFbCopyBase(actionName),
    m_PaOff(paOff),
    m_Size(size)
{
    MASSERT(m_Size);
    m_Cfg.m_b2StepsCopy = true;
    m_Cfg.m_bRead = bRead;
    m_Cfg.m_bSysmem = false;
    SetConfig(m_Cfg);
}

//--------------------------------------------------------------------
//! \brief Set up CE FB Copy.
//!
/* virtual */ RC PmAction_DmaCopySeg::Setup
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    MDiagVmObjs* pVm
)
{
    RC rc;

    MDiagVmScope::CopyTask* pTask = pVm->GetCopyTask();
    MDiagVmScope::MemSegDesc seg(m_PaOff, m_Size);
    MDiagVmScope::Task::Desc* pTaskDesc = GetMDiagVmResources()->CreateTaskDescSingleSeg(seg);
    if (pTaskDesc != nullptr)
    {
        if (m_Cfg.m_bRead)
        {
            pTask->SetSrcTaskDesc(pTaskDesc);
            MASSERT(nullptr == pTask->GetDstTaskDesc());
        }
        else
        {
            pTask->SetDstTaskDesc(pTaskDesc);
            MASSERT(nullptr == pTask->GetSrcTaskDesc());
        }
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_SaveSmcPartFbInfo
//////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
//! \brief constructor
//!
PmAction_SaveSmcPartFbInfo::PmAction_SaveSmcPartFbInfo
(
    const string& fileName
) :
    PmAction("Action.SaveSmcPartFbInfo"),
    m_File(fileName)
{
    GetMDiagvGpuMigration()->Enable();
}

//--------------------------------------------------------------------
//! \brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_SaveSmcPartFbInfo::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! \brief Dump FB mem info of SMC partitions to a file.
//!
/* virtual */ RC PmAction_SaveSmcPartFbInfo::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;

    MDiagVmScope::MemDescs descs;
    CHECK_RC(MDiagVmScope::GetSmcResources()->GetAllPartitionInfo(&descs));
    MASSERT(descs.GetCount() > 0);

    FileHolder fh;
    CHECK_RC(fh.Open(m_File.c_str(), "w"));
    fprintf(fh.GetFile(), "SMC memory partition regions\n");
    UINT32 i = 0;
    MDiagVmScope::MemDescs::DescList memList;
    descs.GetDescList(&memList);
    for (const auto& fbSeg : memList)
    {
        fprintf(fh.GetFile(), "Partition %d %016llx %016llx\n",
            ++i,
            fbSeg.GetAddr(),
            fbSeg.GetSize());
    }

    fprintf(fh.GetFile(), "SMC memory partition config parameters\n");
    const RegHal& regs = MDiagVmScope::GetGpuUtils()->GetRegHal();

    i = regs.Read32(MODS_PFB_PRI_MMU_MEM_PARTITION_BOTTOM);
    fprintf(fh.GetFile(), "MEM_PARTITION_BOTTOM %08x\n",
        i
        );

    i = regs.Read32(MODS_PFB_PRI_MMU_MEM_PARTITION_MIDDLE);
    fprintf(fh.GetFile(), "MEM_PARTITION_MIDDLE %08x\n",
        i
        );

    i = regs.Read32(MODS_PFB_PRI_MMU_MEM_PARTITION_SELWRE_TOP);
    fprintf(fh.GetFile(), "MEM_PARTITION_SELWRE_TOP %08x\n",
        i
        );

    i = regs.Read32(MODS_PFB_PRI_MMU_MEM_PARTITION_BOUNDARY_CFG0);
    fprintf(fh.GetFile(), "MEM_PARTITION_BOUNDARY_CFG0 %08x\n",
        i
        );

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_ReleaseMutex
//////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------
//! brief constructor
//! Internal PmAction and can't be accessed by the user
//!
PmAction_ReleaseMutex::PmAction_ReleaseMutex
(
    RegExp mutex
) :
    PmAction("Action.ReleaseMutex"),
    m_MutexName(mutex)
{
}

//--------------------------------------------------------------------
//! brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_ReleaseMutex::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! brief Free all related this actionblock resource.
//!
/* virtual */ RC PmAction_ReleaseMutex::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    Mutexes mutexList = pPolicyManager->GetUtlMutex(m_MutexName);

    for (auto it : mutexList)
    {
        Tasker::ReleaseMutex(it.second);
        DebugPrintf("%s: Mutex %s release.\n", __FUNCTION__, (it.first).c_str());
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_OnMutex
//////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_OnMutex::PmAction_OnMutex
(
    const RegExp &mutexName
) :
    PmAction("Action.OnMutex"),
    m_MutexName(mutexName)
{
}

//--------------------------------------------------------------------
//! brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_OnMutex::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! brief Acquire the mutex associated with mutex name.
//!
/* virtual */ RC PmAction_OnMutex::Execute
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent
)
{
    RC rc;
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    Mutexes mutexList = pPolicyManager->GetUtlMutex(m_MutexName);

    if (!mutexList.empty() && Utl::HasInstance())
    {
        for (auto it : mutexList)
        {
            Tasker::AcquireMutex(it.second);
            DebugPrintf("%s: AddMutex %s require.\n", __FUNCTION__, (it.first).c_str());
        }
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////
// PmAction_TryMutex
//////////////////////////////////////////////////////////////////////


//--------------------------------------------------------------------
//! brief constructor
//!
PmAction_TryMutex::PmAction_TryMutex
(
    const RegExp &mutexName
) :
    PmAction_Goto("Action.TryMutex"),
    m_MutexName(mutexName)
{
}

//--------------------------------------------------------------------
//! brief Return OK if the action can be exelwted
//!
/* virtual */ RC PmAction_TryMutex::IsSupported
(
    const PmTrigger *pTrigger
) const
{
    return OK;
}

//--------------------------------------------------------------------
//! brief Acquire the mutex associated with mutex name.
//!
/* virtual */ RC PmAction_TryMutex::DoExelwte
(
    PmTrigger *pTrigger,
    const PmEvent *pEvent,
    bool *pGoto
)
{
    RC rc;
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    Mutexes mutexList = pPolicyManager->GetUtlMutex(m_MutexName);

    typedef tuple<string, void *> Mutex;
    vector<Mutex> acquiredMutexes;

    for (auto it : mutexList)
    {
        if (Tasker::TryAcquireMutex(it.second))
        {
            DebugPrintf("%s: AddMutex %s require.\n", __FUNCTION__, (it.first).c_str());
            acquiredMutexes.push_back(make_tuple(it.first,it.second));
        }
        else
        {
            DebugPrintf("%s: TryAcquireMutex %s failed.\n", __FUNCTION__, (it.first).c_str());
            for (auto it : acquiredMutexes)
            {
                Tasker::ReleaseMutex(std::get<1>(it));
                DebugPrintf("%s: Mutex %s release.\n", __FUNCTION__, std::get<0>(it).c_str());
            }
            acquiredMutexes.clear();
            *pGoto = true;
            return rc;
        }
    }

    *pGoto = false;
    return rc;
}
