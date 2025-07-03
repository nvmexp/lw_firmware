/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_topology_mgr.h"

#include "class/cl000f.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl000f.h"
#include "device/interface/lwlink.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/lwlink/lwlerrorcollector.h"
#include "lwl_devif_fact.h"
#include "lwl_devif_mgr.h"
#include "lwl_topology_mgr_impl.h"

namespace LwLinkDevIf
{
    namespace TopologyManager
    {
        //! Pointer to topology manager implementations
        vector<TopologyManagerImplPtr> m_TopologyManagerImpls;
        TopologyManagerImplPtr         m_TopoMgrImpl;
        vector<TestDevicePtr>          m_Devices;
#ifdef INCLUDE_FM
        FMLibIfPtr                     m_pFMLibIf;
#endif
        TopologyMatch                  m_TopologyMatch = TM_DEFAULT;
        HwDetectMethod                 m_HwDetectMethod = HWD_NONE;
        LinkConfig                     m_ForcedLinkConfig = LC_NONE;
        UINT32                         m_TopologyFlags = TF_NONE;
        string                         m_TopologyFile = "";
        string                         m_SocketPath = "";
        bool                           m_bInitialized = false;
        bool                           m_bSkipTopologyDetect = false;
        bool                           m_bSkipLinkShutdownOnInit = false;
        bool                           m_bSkipLinkShutdownOnExit = false;
        bool                           m_bIgnoreMissingCC = false;
        UINT32                         m_LinkTrainAttempts = 1;
        bool                           m_bSysParamPresent = false;
        string m_PostDiscoveryDumpFunction = "";
        string m_PreTrainingDumpFunction = "";
        string m_PostTrainingDumpFunction  = "";
        string m_PostShutdownDumpFunction = "";

        RC CheckTopologyManagerState(const char * pFund, bool *pbSkipTopology)
        {
            *pbSkipTopology = m_bSkipTopologyDetect;

            if (m_bSkipTopologyDetect)
                return OK;

            if (!m_bInitialized)
            {
                Printf(Tee::PriError, GetTeeModule()->GetCode(),
                       "%s : Topology manager not initialzied!\n",
                       __FUNCTION__);
                return RC::SOFTWARE_ERROR;
            }

            if (m_TopologyManagerImpls.empty())
            {
                Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                       "%s : No topology manager implementation present, skipping!\n",
                       __FUNCTION__);
                *pbSkipTopology = true;
                return OK;
            }

            if (!m_TopoMgrImpl)
            {
                Printf(Tee::PriError, GetTeeModule()->GetCode(),
                       "%s : Topology manager implementation not found!\n",
                       __FUNCTION__);
                return RC::SOFTWARE_ERROR;
            }

            return OK;
        }

        //------------------------------------------------------------------------------
        bool GetIsInitialized()
        {
            return m_bInitialized;
        }

        RC CallDumpFunction(string dumpFunction, LwLink::DumpReason reason)
        {
            if (dumpFunction == "")
                return RC::OK;

            RC rc;
            JsArray params;
            JavaScriptPtr pJs;
            jsval retValJs  = JSVAL_NULL;
            jsval jsDumpReason;

            CHECK_RC(pJs->ToJsval(static_cast<UINT32>(reason), &jsDumpReason));
            params.push_back(jsDumpReason);
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), dumpFunction.c_str(), params, &retValJs));

            UINT32 retVal = RC::SOFTWARE_ERROR;
            CHECK_RC(pJs->FromJsval(retValJs, &retVal));
            if (RC::UNDEFINED_JS_METHOD == retVal)
                retVal = RC::OK;
            return retVal;
        }
    };
};

//------------------------------------------------------------------------------
//! \brief Get the topology manager implementation
//!
//! \return Topology manager implementation
//!
LwLinkDevIf::TopologyManagerImplPtr LwLinkDevIf::TopologyManager::GetImpl()
{
    return m_TopoMgrImpl;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::GetExpectedLinkMask
(
    TestDevicePtr pDevice,
    UINT64 *pLinkMask
)
{
    MASSERT(m_TopoMgrImpl);
    return m_TopoMgrImpl->GetExpectedLinkMask(pDevice, pLinkMask);
}

//------------------------------------------------------------------------------
//! \brief Get the minimum number of forced devices for a particular type
//!
//! \param devType : Type of device to get number of forced devices on
//!
//! \return Number of devices to force
//!
UINT32 LwLinkDevIf::TopologyManager::GetMinForcedDevices(TestDevice::Type devType)
{
    MASSERT(m_TopoMgrImpl);
    return m_TopoMgrImpl->GetMinForcedDevices(devType);
}

//------------------------------------------------------------------------------
string LwLinkDevIf::TopologyManager::GetPostDiscoveryDumpFunction()
{
    return m_PostDiscoveryDumpFunction;
}

//------------------------------------------------------------------------------
string LwLinkDevIf::TopologyManager::GetPostTrainingDumpFunction()
{
    return m_PostTrainingDumpFunction;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::GetDevices
(
    vector<TestDevicePtr>* pDevices
)
{
    MASSERT(pDevices);
    pDevices->clear();
    pDevices->insert(pDevices->begin(), m_Devices.begin(), m_Devices.end());
    return RC::OK;
}

//------------------------------------------------------------------------------
LwLinkDevIf::FMLibIfPtr LwLinkDevIf::TopologyManager::GetFMLibIf()
{
#ifdef INCLUDE_FM
    return m_pFMLibIf;
#else
    return nullptr;
#endif
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManager::GetIgnoreMissingCC()
{
    return m_bIgnoreMissingCC;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::TopologyManager::GetLinkTrainAttempts()
{
    return m_LinkTrainAttempts;
}

//------------------------------------------------------------------------------
//! \brief Get all routes
//!
//! \param pRoutes : Vector of route pointers to return
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManager::GetRoutes
(
    vector<LwLinkRoutePtr> *pRoutes
)
{
    RC rc;
    bool bSkipTopology = false;
    CHECK_RC(CheckTopologyManagerState(__FUNCTION__, &bSkipTopology));
    if (bSkipTopology)
        return OK;
    CHECK_RC(m_TopoMgrImpl->GetRoutes(pRoutes));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get all routes for a particular lwlink device
//!
//! \param pDevice : Pointer to device to return the connections on
//! \param pRoutes : Vector of route pointers to return
//!
//! \return OK if successful, not OK otherwise
RC LwLinkDevIf::TopologyManager::GetRoutesOnDevice
(
    TestDevicePtr           pDevice
   ,vector<LwLinkRoutePtr> *pRoutes
)
{
    RC rc;
    bool bSkipTopology = false;
    CHECK_RC(CheckTopologyManagerState(__FUNCTION__, &bSkipTopology));
    if (bSkipTopology)
        return OK;
    CHECK_RC(m_TopoMgrImpl->GetRoutesOnDevice(pDevice, pRoutes));
    return rc;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManager::GetSkipLinkShutdownOnExit()
{
    return m_bSkipLinkShutdownOnExit;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManager::GetSkipLinkShutdownOnInit()
{
    return m_bSkipLinkShutdownOnInit;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManager::GetSkipTopologyDetect()
{
    return m_bSkipTopologyDetect;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::TopologyManager::GetSystemParameterPresent()
{
    return m_bSysParamPresent;
}

//------------------------------------------------------------------------------
//! \brief Get the topology file
//!
//! \return Topology file
//!
string LwLinkDevIf::TopologyManager::GetTopologyFile()
{
    return m_TopologyFile;
}

//------------------------------------------------------------------------------
//! \brief Get the Socket Path
//!
//! \return Socket Path string
//!
string LwLinkDevIf::TopologyManager::GetSocketPath()
{
    return m_SocketPath;
}

//-----------------------------------------------------------------------------
UINT32 LwLinkDevIf::TopologyManager::GetTopologyFlags()
{
    return m_TopologyFlags;
}

//------------------------------------------------------------------------------
//! \brief Get the topology match config
//!
//! \return Topology match
//!
LwLinkDevIf::TopologyMatch LwLinkDevIf::TopologyManager::GetTopologyMatch()
{
    return m_TopologyMatch;
}

//------------------------------------------------------------------------------
//! \brief Initialize the topology manager
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManager::Initialize()
{
    RC rc;

    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Topology manager already initialized!\n",
               __FUNCTION__);
        return OK;
    }

#ifdef INCLUDE_FM
    m_pFMLibIf = Factory::CreateFMLibInterface();
    if (!m_pFMLibIf)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unable to create fabricmanager core library interface!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(m_pFMLibIf->InitializeLocalFM());
        // Skip Global FM init if a custom socket has been provided
        // In this situation we intend to run Multi-Node MODS
        if (GetSocketPath().empty())
        {
            CHECK_RC(m_pFMLibIf->InitializeGlobalFM());

        }
    }
#endif

    // Create the implementations
    TopologyManagerImplFactory::CreateAll(m_TopologyMatch,
                                          m_TopologyFile,
                                          &m_TopologyManagerImpls);
    if (m_TopologyManagerImpls.empty())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : No topology manager implementations found!\n",
               __FUNCTION__);
        m_bInitialized = true;
        return OK;
    }

    for (size_t lwrMgr = 0; lwrMgr < m_TopologyManagerImpls.size(); lwrMgr++)
    {
        CHECK_RC(m_TopologyManagerImpls[lwrMgr]->Initialize());
        if ((m_TopoMgrImpl == nullptr) && m_TopologyManagerImpls[lwrMgr]->ShouldUse())
        {
            m_TopoMgrImpl = m_TopologyManagerImpls[lwrMgr];
        }
    }

    if (!m_TopoMgrImpl)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Topology manager implementation not found!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // TODO : HW detect method should be triggered by board ID rather than command
    // line parameter, but that functionality is not yet available
    m_TopoMgrImpl->SetHwDetectMethod(m_HwDetectMethod);

    m_TopoMgrImpl->SetTopologyFlags(m_TopologyFlags);

    m_bInitialized = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Print the topology at the specified priority
//!
//! \param devices : List of detected devices
//! \param pri     : Priority to print the topology at
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManager::Print(Tee::Priority pri)
{
    RC rc;
    bool bSkipTopology = false;
    CHECK_RC(CheckTopologyManagerState(__FUNCTION__, &bSkipTopology));
    if (bSkipTopology)
        return OK;
    m_TopoMgrImpl->Print(m_Devices, pri);
    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetForcedLinkConfig(LinkConfig flc)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_ForcedLinkConfig = flc;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetHwDetectMethod(HwDetectMethod hwd)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_HwDetectMethod = hwd;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetIgnoreMissingCC(bool bIgnoreMissingCC)
{
    m_bIgnoreMissingCC = bIgnoreMissingCC;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetLinkTrainAttempts(UINT32 linkTrainAttempts)
{
    if (linkTrainAttempts == 0)
    {
        Printf(Tee::PriError, "Link training attempts may not be zero\n");
        return RC::BAD_PARAMETER;
    }
    m_LinkTrainAttempts = linkTrainAttempts;
    return RC::OK;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::TopologyManager::SetPostDiscoveryDumpFunction(string func)
{
    m_PostDiscoveryDumpFunction = func;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::TopologyManager::SetPreTrainingDumpFunction(string func)
{
    m_PreTrainingDumpFunction = func;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::TopologyManager::SetPostTrainingDumpFunction(string func)
{
    m_PostTrainingDumpFunction = func;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::TopologyManager::SetPostShutdownDumpFunction(string func)
{
    m_PostShutdownDumpFunction = func;
}


//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetSkipLinkShutdownOnExit(bool bSkipLinkShutdownOnExit)
{
    m_bSkipLinkShutdownOnExit = bSkipLinkShutdownOnExit;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetSkipLinkShutdownOnInit(bool bSkipLinkShutdownOnInit)
{
    m_bSkipLinkShutdownOnInit = bSkipLinkShutdownOnInit;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetSkipTopologyDetect(bool bSkipTopologyDetect)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_bSkipTopologyDetect = bSkipTopologyDetect;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetSystemParameterPresent(bool bPresent)
{
    m_bSysParamPresent = bPresent;
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Set the topology file to use
//!
//! \param topologyFile : Topology file to use
//!
RC LwLinkDevIf::TopologyManager::SetTopologyFile(string topologyFile)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_TopologyFile = topologyFile;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Set the socket path to use while intializing FM
//!
//! \param pathString : socket Path to use
//!
RC LwLinkDevIf::TopologyManager::SetSocketPath(string pathString)
{
    m_SocketPath = pathString;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::SetTopologyFlag(TopologyFlag flag)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_TopologyFlags |= flag;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::TopologyManager::ClearTopologyFlag(TopologyFlag flag)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_TopologyFlags &= ~flag;
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Set the topology match configuration
//!
//! \param topo : Topolgy match configuration
//!
RC LwLinkDevIf::TopologyManager::SetTopologyMatch(TopologyMatch topo)
{
    MASSERT(m_TopologyManagerImpls.empty());
    m_TopologyMatch = topo;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup the topology
//!
//! \param devices : List of detected devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManager::Setup()
{
    RC rc;

    if (m_bSkipTopologyDetect)
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "%s : Skipping topology detection, LwLink devices are present but connections,"
               " paths and routes are not populated!\n",
               __FUNCTION__);
        return OK;
    }

    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Topology manager not initialized!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // All devices must be initialized before initializing the topology manager
    // since the devices that are present determine the type of topology
    // implementer that needs to be created
    if (!LwLinkDevIf::Manager::AllDevicesInitialized())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Not all devices have not been initialized!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (m_TopologyManagerImpls.empty())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : No topology manager implementation found, skipping!\n",
               __FUNCTION__);
        return OK;
    }

    if (!m_TopoMgrImpl)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Topology manager implementation not found!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (!pTestDeviceMgr)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : TestDeviceMgr not initialized!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_TopoMgrImpl->GetDevices(&m_Devices));

    CHECK_RC(m_TopoMgrImpl->Setup(m_Devices));

    LinkConfig linkConfig = m_TopoMgrImpl->GetDefaultLinkConfig();
    if (m_ForcedLinkConfig != LC_NONE)
        linkConfig = m_ForcedLinkConfig;

    UINT32 linkTrainAttempts = (linkConfig == LC_MANUAL) ? m_LinkTrainAttempts : 1;

    auto LogUphyOnTrain = [&] () -> RC
    {
        const UphyRegLogger::UphyInterface loggedIf =
            static_cast<UphyRegLogger::UphyInterface>(UphyRegLogger::GetLoggedInterface());
        if (UphyRegLogger::GetLoggingEnabled() &&
            ((loggedIf == UphyRegLogger::UphyInterface::LwLink) ||
             (loggedIf == UphyRegLogger::UphyInterface::All)) &&
            (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining))
        {
            // Per-device logging settings have not been initialized at this point, so it is
            // necessary to force the log (do not log device perf data as at this point MODS
            // has not initialized far enough to be able to query it)
            return UphyRegLogger::ForceLogRegs(UphyRegLogger::UphyInterface::LwLink,
                                               UphyRegLogger::PostLinkTraining,
                                               rc,
                                               false);
        }
        return RC::OK;
    };

    bool bFirstPassTraining = true;
    bool bAllLinksTrained = false;
    do
    {
        if ((linkConfig == LC_MANUAL) || m_bSysParamPresent)
        {
            CHECK_RC(m_TopoMgrImpl->DiscoverConnections(m_Devices));
        }

        for (auto pLwrDev : m_Devices)
        {
            CHECK_RC(pLwrDev->GetInterface<LwLink>()->InitializePostDiscovery());
        }

        CHECK_RC(CallDumpFunction(m_PostDiscoveryDumpFunction, LwLink::DR_POST_DISCOVERY));

        CHECK_RC(m_TopoMgrImpl->SetupPostDiscovery(m_Devices));

        if (linkConfig == LC_MANUAL)
        {
            // In order to apply LwLink settings that may be changed through MODS command line
            // options it is necessary to ensure that MODS actually trained the links and that
            // they were not simply left in SAFE or  HS mode from either a previous run of MODS
            // or FM.
            //
            // This must be done after post discovery setup so that both the switch driver and
            // MODS sw state are fully updated and the links trained
            if (!m_bSkipLinkShutdownOnInit && bFirstPassTraining)
            {
                Printf(Tee::PriLow, "Shutting down links during initialization\n");
                CHECK_RC(m_TopoMgrImpl->ShutdownLinks(m_Devices));
                CHECK_RC(CallDumpFunction(m_PostShutdownDumpFunction, LwLink::DR_POST_SHUTDOWN));
            }

            CHECK_RC(CallDumpFunction(m_PreTrainingDumpFunction, LwLink::DR_PRE_TRAINING));

            DEFERNAME(logUphy) { LogUphyOnTrain(); };

            CHECK_RC(m_TopoMgrImpl->TrainLinks(m_Devices, &bAllLinksTrained));
            if (!bAllLinksTrained && (linkTrainAttempts != 1))
            {
                CHECK_RC(m_TopoMgrImpl->ReinitFailedLinks());
            }

            logUphy.Cancel();
            CHECK_RC(LogUphyOnTrain());

            CHECK_RC(CallDumpFunction(m_PostTrainingDumpFunction, LwLink::DR_POST_TRAINING));
        }
        else
        {
            bool bAnyLinksTrainedDuringInit = false;
            for (size_t lwrDevIdx = 0;
                  (lwrDevIdx < m_Devices.size()) && !bAnyLinksTrainedDuringInit; ++lwrDevIdx)
            {
                auto pLwLink = m_Devices[lwrDevIdx]->GetInterface<LwLink>();
                for (UINT32 lwrLink = 0;
                      (lwrLink < pLwLink->GetMaxLinks()) && !bAnyLinksTrainedDuringInit;
                      ++lwrLink)
                {
                    bAnyLinksTrainedDuringInit = pLwLink->IsLinkActive(lwrLink);
                }
            }
            if (bAnyLinksTrainedDuringInit)
            {
                CHECK_RC(LogUphyOnTrain());
            }
        }
        linkTrainAttempts--;
        bFirstPassTraining = false;
    } while (linkTrainAttempts && !bAllLinksTrained);

    CHECK_RC(m_TopoMgrImpl->SetupPostTraining(m_Devices));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the topology manager
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TopologyManager::Shutdown()
{
    StickyRC rc;

    if (m_TopoMgrImpl)
    {
        LinkConfig linkConfig = m_TopoMgrImpl->GetDefaultLinkConfig();
        if (m_ForcedLinkConfig != LC_NONE)
            linkConfig = m_ForcedLinkConfig;

        rc = LwLinkErrorCollector::Stop();

        if (linkConfig == LC_MANUAL)
        {
            if (!m_bSkipLinkShutdownOnExit)
            {
                rc = m_TopoMgrImpl->ShutdownLinks(m_Devices);
                rc = CallDumpFunction(m_PostShutdownDumpFunction, LwLink::DR_POST_SHUTDOWN);

                rc = m_TopoMgrImpl->ResetTopology(m_Devices);
            }
            else
            {
                Printf(Tee::PriNormal, "Skipping LwLink shutdown on exit\n");
            }
        }

        m_TopoMgrImpl.reset();
    }

    for (auto & pLwrMgr : m_TopologyManagerImpls)
    {
        rc = pLwrMgr->Shutdown();
        if (!pLwrMgr.unique())
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "TopologyManager : Multiple references to %s"
                   " during shutdown!\n", pLwrMgr->GetName().c_str());
            rc = RC::SOFTWARE_ERROR;
        }
        pLwrMgr.reset();
    }

#ifdef INCLUDE_FM
    if (m_pFMLibIf)
    {
        rc = m_pFMLibIf->Shutdown();
        m_pFMLibIf.reset();
    }
#endif

    m_Devices.clear();
    m_TopologyManagerImpls.clear();
    m_bInitialized = false;

    return rc;
}

//-----------------------------------------------------------------------------
static JSClass TopologyManager_class =
{
    "LwLinkTopologyManager",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
static SObject TopologyManager_Object
(
    "LwLinkTopologyManager",
    TopologyManager_class,
    0,
    0,
    "TopologyManager JS Object"
);

using namespace LwLinkDevIf;
//-----------------------------------------------------------------------------
PROP_READWRITE_NAMESPACE(TopologyManager, SkipLinkShutdownOnInit, bool, "Skip link shutdown on init");
PROP_READWRITE_NAMESPACE(TopologyManager, SkipLinkShutdownOnExit, bool, "Skip link shutdown on exit");
PROP_READWRITE_NAMESPACE(TopologyManager, SkipTopologyDetect, bool, "Skip topology detection in topology manager");
PROP_READWRITE_NAMESPACE(TopologyManager, SystemParameterPresent, bool, "Set if a system parameter was adjusted on the command line");
PROP_READONLY_NAMESPACE(TopologyManager,  IsInitialized, bool, "Check if the topology manager is initialized");
PROP_READWRITE_NAMESPACE(TopologyManager, IgnoreMissingCC, bool, "Ignore missing cable controllers");
PROP_READWRITE_NAMESPACE(TopologyManager, LinkTrainAttempts, UINT32, "Set the number of link training retries");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(TopologyManager,
                Initialize,
                0,
                "Initialize the topology manager")
{
    STEST_HEADER(0, 0, "Usage: LwLinkTopologyManager.LoadTopology()");

    RC rc;

    C_CHECK_RC(LwLinkDevIf::TopologyManager::Initialize());
    C_CHECK_RC(LwLinkDevIf::TopologyManager::Setup());

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(TopologyManager,
                Shutdown,
                0,
                "Shutdown the topology manager")
{
    STEST_HEADER(0, 0, "Usage: LwLinkTopologyManager.Shutdown()");

    RC rc;

    C_CHECK_RC(LwLinkDevIf::TopologyManager::Shutdown());

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  Print,
                  1,
                  "Print the discovered lwlink topology")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.Print(printPri)");
    STEST_ARG(0, UINT32, printPriInt);
    Tee::Priority pri = static_cast<Tee::Priority>(printPriInt);

    LwLinkDevIf::TopologyManager::Print(pri);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetTopologyMatch,
                  1,
                  "Set the LwLink topology match configuration")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetTopologyMatch(matchConfig)\n");
    STEST_ARG(0, UINT32, topologyMatch);

    LwLinkDevIf::TopologyManager::SetTopologyMatch(static_cast<LwLinkDevIf::TopologyMatch>(topologyMatch)); //$

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetHwDetectMethod,
                  1,
                  "Set the LwLink hw detect method")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetHwDetectMethod(hwDetectMethod)\n");
    STEST_ARG(0, UINT32, hwd);

    LwLinkDevIf::TopologyManager::SetHwDetectMethod(static_cast<LwLinkDevIf::HwDetectMethod>(hwd));

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetForcedLinkConfig,
                  1,
                  "Set the LwLink forced link configuration method")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetForcedLinkConfig(linkConfigMethod)\n");
    STEST_ARG(0, UINT32, flc);

    LwLinkDevIf::TopologyManager::SetForcedLinkConfig(static_cast<LwLinkDevIf::LinkConfig>(flc));

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  GetTopologyFile,
                  0,
                  "Get the LwLink topology file")
{
    STEST_HEADER(0, 0, "Usage: LwLinkTopologyManager.GetTopolgyFile()\n");

    auto s = LwLinkDevIf::TopologyManager::GetTopologyFile();

    if (OK != pJavaScript->ToJsval(s, pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in LwLinkDevMgr.GetTopolgyFile()");
        return JS_FALSE;
    }

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetTopologyFile,
                  1,
                  "Set the LwLink topology file")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetTopolgyFile(topologyFile)\n");
    STEST_ARG(0, string, topologyFile);

    LwLinkDevIf::TopologyManager::SetTopologyFile(topologyFile);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetSocketPath,
                  1,
                  "Set the Socket Path for FM")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetSocketPath(pathString)\n");
    STEST_ARG(0, string, pathString);

    LwLinkDevIf::TopologyManager::SetSocketPath(pathString);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetTopologyFlag,
                  1,
                  "Set the specified Topology flag")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetTopologyFlag(topologyFlag)\n");
    STEST_ARG(0, UINT32, topologyFlag);

    TopologyFlag flag = static_cast<TopologyFlag>(topologyFlag);

    LwLinkDevIf::TopologyManager::SetTopologyFlag(flag);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  ClearTopologyFlag,
                  1,
                  "Clear the specified Topology flag")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.ClearTopologyFlag(topologyFlag)\n");
    STEST_ARG(0, UINT32, topologyFlag);

    TopologyFlag flag = static_cast<TopologyFlag>(topologyFlag);

    LwLinkDevIf::TopologyManager::ClearTopologyFlag(flag);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  GetTopologyFlags,
                  0,
                  "Get the set Topology flags mask")
{
    STEST_HEADER(0, 0, "Usage: LwLinkTopologyManager.GetTopologyFlags()\n");

    UINT32 flags = LwLinkDevIf::TopologyManager::GetTopologyFlags();

    if (RC::OK != pJavaScript->ToJsval(flags, pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in LwLinkDevMgr.GetTopologyFlags()");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetPostDiscoveryDumpFunction,
                  1,
                  "Dump function to call after discovery is complete (default = \"\")")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetPostDiscoveryDumpFunction(function)\n");
    STEST_ARG(0, string, func);

    LwLinkDevIf::TopologyManager::SetPostDiscoveryDumpFunction(func);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetPreTrainingDumpFunction,
                  1,
                  "Dump function to call before link training is started, switch only (default = \"\")")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetPreTrainingDumpFunction(function)\n");
    STEST_ARG(0, string, func);

    LwLinkDevIf::TopologyManager::SetPreTrainingDumpFunction(func);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetPostTrainingDumpFunction,
                  1,
                  "Dump function to call after link training is complete, switch only (default = \"\")")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetPostTrainingDumpFunction(function)\n");
    STEST_ARG(0, string, func);

    LwLinkDevIf::TopologyManager::SetPostTrainingDumpFunction(func);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(TopologyManager,
                  SetPostShutdownDumpFunction,
                  1,
                  "Dump function to call after link shutdown is complete, switch only (default = \"\")")
{
    STEST_HEADER(1, 1, "Usage: LwLinkTopologyManager.SetPostShutdownDumpFunction(function)\n");
    STEST_ARG(0, string, func);

    LwLinkDevIf::TopologyManager::SetPostShutdownDumpFunction(func);

    *pReturlwalue = JSVAL_NULL;
    return JS_TRUE;
}
