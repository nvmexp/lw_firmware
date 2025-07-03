/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_gpu_devif.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "device/interface/lwlink.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "lwl_simgpu_dev.h"
#include "lwl_topology_mgr.h"
#include "lwl_devif_fact.h"
#include "lwl_devif_mgr.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create an gpu lwlink device interface
    //!
    //! \return Pointer to new gpu lwlink device interface
    LwLinkDevIf::Interface * CreateLwGpuDevIf()
    {
        return new LwLinkDevIf::GpuDevInterface;
    }

    //! Register the gpu lwlink device interface with the factory
    //!
    //! The first parameter controls the order in which devices are found,
    //! initialized, and destroyed.
    LwLinkDevIf::Factory::DevIfFactoryFuncRegister m_LwGpuFact(1, "LwGpuDevIf", CreateLwGpuDevIf);
};

//------------------------------------------------------------------------------
//! \brief Find all GPU LwLink devices
//!
//! \param pDevices : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::GpuDevInterface::FindDevices(vector<TestDevicePtr> *pDevices)
{
    if (FoundDevices())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Gpu lwlink devices already found, skipping\n",
               __FUNCTION__);
        return OK;
    }

    RC rc;

    // Initialize the resource manager.
    if (!RmInitialize())
    {
        return RC::DID_NOT_INITIALIZE_RM;
    }
    Printf(Tee::PriDebug, "Initialized resource manager.\n");

    GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);
    CHECK_RC(pGpuDevMgr->FindDevices());

    for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != nullptr; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        // GpuSubdevice objects are still managed and deleted by GpuDevMgr (for now).
        // Do not delete them here.
        TestDevicePtr pGpuSubDev(pSubdev, [](auto p){});
        m_Devices.push_back(pGpuSubDev);
        pDevices->push_back(pGpuSubDev);

        // SOC devices need a lwlink library interface pointer during GPU initialization
        // (specifically SetupFeatures calls forces a call to LoadLwLinkCaps) which means
        // that they need to setup their library interface during FindDevices
        // rather than during InitializeAll.  Non-SOC Gpus do not require a library
        // interface since they use RM control calls and those are available during
        // the SetupFeatures call.  Non-SOC GPU devices lwlink interface is setup
        // during the normal GPU initialization process
        if (pSubdev->IsSOC() && !Manager::GetIgnoreTegra())
        {
#if defined(INCLUDE_LWLINK) && defined(TEGRA_MODS)
            vector<LibInterface::LibDeviceHandle> libHandles;

            CHECK_RC(LwLinkDevIf::GetTegraLibIf()->FindDevices(&libHandles));

            if (!libHandles.empty())
            {
                MASSERT(libHandles.size() == 1);

                auto pLwLink = pSubdev->GetInterface<LwLink>();
                if (pLwLink && pLwLink->IsSupported(libHandles))
                {
                    CHECK_RC(pLwLink->Initialize(libHandles));
                }
            }
#endif
        }
    }

    // Pad out the number of ebridges to the minimum required to handle any
    // forced configurations
#ifdef INCLUDE_LWLINK
    UINT32 numDevices = Manager::GetNumSimGpu();
    for (UINT32 ii = 0; ii < numDevices; ii++)
    {
        TestDevice::Id gpuId;
        gpuId.SetPciDBDF(TestDevice::Id::SIMULATED_GPU_PCI_DOMAIN,
                         TestDevice::Id::SIMULATED_GPU_PCI_BUS,
                         static_cast<UINT16>(ii));
        if (TestDevice::IsPciDeviceInAllowedList(gpuId))
        {
            TestDevicePtr pSimGpuDev(new SimGpuDev(gpuId));
            m_Devices.push_back(pSimGpuDev);
            pDevices->push_back(pSimGpuDev);
        }
    }
#endif

    m_bFoundDevices = true;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ bool LwLinkDevIf::GpuDevInterface::FoundDevices()
{
    // When RM initialization is skipped GPU devices cannot be found, therefore simply
    // flag them as found in this case
    return m_bFoundDevices || (GpuPtr()->IsInitialized() && !GpuPtr()->IsRmInitCompleted());
}

//------------------------------------------------------------------------------
//! \brief Initialize all devices
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::GpuDevInterface::InitializeAllDevices()
{
    RC rc;

    if (!FoundDevices())
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : GPU LwLink devices not found, find devices before initialization\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (!GpuPtr()->IsRmInitCompleted())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : RM not initialized, skipping GPU lwlink device initialization\n",
               __FUNCTION__);
        return OK;
    }

    if (m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : GPU LwLink devices already initialized\n",
               __FUNCTION__);
        return OK;
    }

    for (size_t ii = 0; ii < m_Devices.size(); ii++)
    {
        GpuSubdevice* pSubdev = m_Devices[ii]->GetInterface<GpuSubdevice>();
        if (!pSubdev)
        {
            CHECK_RC(m_Devices[ii]->Initialize());
        }
    }

    m_bDevicesInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown all devices controlled by the interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::GpuDevInterface::Shutdown()
{
    if (!m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : GPU devices not initialized, skipping GPU lwlink shutdown\n",
               __FUNCTION__);
        return OK;
    }

    StickyRC rc;
    for (size_t ii = 0; ii < m_Devices.size(); ii++)
    {
        GpuSubdevice* pSubdev = dynamic_cast<GpuSubdevice*>(m_Devices[ii].get());
        if (!pSubdev)
        {
            rc = m_Devices[ii]->Shutdown();
        }
        if (!m_Devices[ii].unique())
        {
            Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                   "%s : Multiple references (%ld) to LwLink device %s during shutdown!\n",
                   __FUNCTION__, m_Devices[ii].use_count(), m_Devices[ii]->GetName().c_str());
        }
    }
    m_bDevicesInitialized = false;

    m_Devices.clear();

    m_bFoundDevices = false;

    return rc;
}

