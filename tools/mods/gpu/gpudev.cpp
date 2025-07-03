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

#include "gpu/include/gpudev.h"

#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl0070.h" // LW01_MEMORY_SYSTEM_DYNAMIC
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "class/cle3f1.h" // TEGRA_VASPACE_A
#include "core/include/channel.h"
#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"  // for LW2080_CTRL_FB_INFO (FB_IS_BROKEN)
#include "device/interface/lwlink/lwlfla.h"
#include "gpu/display/nul_disp.h"
#include "gpu/include/dispmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/protmanager.h"
#include "gpu/include/userdalloc.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/runlist.h"
#include "js_gpu.h"
#include "lwos.h"
#include "lwRmReg.h"
#include "rm.h"

#include <algorithm>

extern "C"
{
#include "modeset.h"
}

#include "ctrl/ctrl0080.h" // LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES_PARAMS

using ResetControls = GpuSubdevice::ResetControls;

//-----------------------------------------------------------------------------
//! Create an empty GpuDevice.  This doesn't do any initialization.
// NOTE: Manually setting Gob Size information.  This will likely need to
//          be removed for Fermi and Later
GpuDevice::GpuDevice() :
    m_Initialized(false),
    m_InitializedWithoutRm(false),
    m_InitDone(0),
    m_Device(LW0080_MAX_DEVICES),
    m_GrCapsLoaded(false),
    m_HostCapsLoaded(false),
    m_FbCapsLoaded(false),
    m_NumSubdevices(0),
    m_pDisplay(0),
    m_pDisplayMgr(0),
    m_pUserdManager(nullptr),
    m_GobHeight(0),
    m_BigPageSize(0),
    m_CompressionPageSize(0),
    m_GpuVaBitsCount(0),
    m_UseRobustChCallback(false),
    m_ResetInProgress(false),
    m_ResetType(0),
    m_ForcePerf20(false),
    m_NumUserdAllocs(128),
    m_UserdLocation(Memory::Optimal),
    m_Family(None),
    m_AllocFbMutex(Tasker::AllocMutex("GpuDevice::m_AllocFbMutex", Tasker::mtxUnchecked)),
    m_DevicesAllocated(false),
    m_HeapSizeKb(0),
    m_FrameBufferLimit(0),
    m_GpuGartLimit(0)
{
    for (UINT32 subdev = 0; subdev < LW2080_MAX_SUBDEVICES; subdev++)
        m_Subdevices[subdev] = 0;

    memset(m_GrCapsTable, 0, LW0080_CTRL_GR_CAPS_TBL_SIZE);
    memset(m_HostCapsTable, 0, LW0080_CTRL_HOST_CAPS_TBL_SIZE);
    memset(m_FbCapsTable, 0, LW0080_CTRL_FB_CAPS_TBL_SIZE);

    GpuDeviceHandles handles = { 0 };
    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        m_Handles[LwRmPtr(client).Get()] = handles;
    }
}

//! The destructor should call shutdown in the case it hasn't been done
//! explicitly.
GpuDevice::~GpuDevice()
{
    if (m_Initialized)
    {
        Printf(Tee::PriLow, "Warning: freeing GpuDevice which hasn't been "
               "shutdown yet.  Shutting down...\n");
        Shutdown();
    }
    Tasker::FreeMutex(m_AllocFbMutex);
}

//! Set the Gpu DeviceInstance
RC GpuDevice::SetDeviceInst(UINT32 devInst)
{
    if (devInst >= LW0080_MAX_DEVICES)
    {
        Printf(Tee::PriHigh, "Device ID is greater than max device ID.");
        return RC::ILWALID_DEVICE_ID;
    }

    m_Device = devInst;

    return OK;
}

//! \brief Initialize the Gpu Device
//!
//! The Gpu Device should be initialized by passing the device ID.  The device
//! well then initialize itself through the RM, and initialize all subdevices
//! which the RM reports are linked to it.  This function should only be
//! called after calling AddSubdevice() on all the GpuIds, to ensure that
//! this step is not forgotten (the device can unfortunately not easily infer
//! from the RM what it's subdevice GpuIds are--only how many subdevices it
//! has).
RC GpuDevice::Initialize(UINT32 numUserdAllocs, Memory::Location userdLoc, UINT32 devInst)
{
    RC rc = OK;

    m_NumUserdAllocs = numUserdAllocs;
    m_UserdLocation = userdLoc;
    CHECK_RC(SetDeviceInst(devInst));
    CHECK_RC(Initialize());

    return rc;
}

RC GpuDevice::Initialize(UINT32 numUserdAllocs, Memory::Location userdLoc)
{
    RC rc = OK;

    m_NumUserdAllocs = numUserdAllocs;
    m_UserdLocation = userdLoc;
    CHECK_RC(Initialize());

    return rc;
}

RC GpuDevice::SetChipFamily()
{
    // Look up and remember the chip family.
    m_Family = static_cast<GpuFamily>(
        Gpu::DeviceIdToFamily(DeviceId()));

    for (size_t subdev = 0; subdev < static_cast<size_t>(m_NumSubdevices); ++subdev)
    {
        if (!m_Subdevices[subdev])
        {
            Printf(Tee::PriError,
                   "Failed to initialize device %d with %d subdevices "
                   "because subdevice %zu was not added.\n",
                   m_Device, m_NumSubdevices, subdev);
            return RC::ILWALID_DEVICE_ID;
        }
        if ((Xp::GetOperatingSystem() == Xp::OS_WINDA) ||
            (Xp::GetOperatingSystem() == Xp::OS_LINDA))
        {
            // Direct Amodel has no device id
            switch (m_Subdevices[subdev]->Architecture())
            {
            case LW2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_TU100:
                m_Family = Turing;
                break;
            case LW2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_GA100:
                m_Family = Ampere;
                break;
            case LW2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_GH100:
                m_Family = Hopper;
                break;
            case LW2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_AD100:
                m_Family = Ada;
                break;
            default:
                Printf(Tee::PriError, "Unrecognized architecture 0x%08x.\n",
                       m_Subdevices[subdev]->Architecture());
                return RC::ILWALID_DEVICE_ID;
            }
        }
    }
    return RC::OK;
}

//! \brief Initialize the Gpu Device
RC GpuDevice::Initialize()
{
    // @TODO
    // What if we don't know about this device?  What if it's a GT900 and our
    // branch is too old?  In that case, the software device (this) will exist,
    // but it really doesn't make sense to try anything.
    // I believe that there's no good way to not report it to the RM, but
    // that's one possibility and would eliminate "this" even existing,
    // but the alternative is a IsActive flag in GpuDevice which, if false,
    // causes all functions to be bypassed.
    RC rc = OK;

    if (m_Device == LW0080_MAX_DEVICES)
    {
        Printf(Tee::PriHigh,
               "Unable to initialize due to improper device ID.\n");
        return RC::ILWALID_DEVICE_ID;
    }

    if (m_Initialized)
    {
        Printf(Tee::PriNormal,
               "Device %d already initialized.  Nothing to do!\n", m_Device);
        return OK;
    }

    CHECK_RC(AllocDevices());

    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};

    /* get number of subdevices for device */
    CHECK_RC(LwRmPtr()->ControlByDevice(this,
                                        LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                                        &numSubDevicesParams,
                                        sizeof (numSubDevicesParams)));

    // @TODO I shouldn't have to allocate the device first, because that
    // just leaves us in a bad state here...
    if (numSubDevicesParams.numSubDevices != m_NumSubdevices)
    {
        Printf(Tee::PriHigh,
               "Failed to initialize device %d with %d subdevices "
               "because only %d subdevices have been added\n", m_Device,
               (int)numSubDevicesParams.numSubDevices, m_NumSubdevices);
        return RC::ILWALID_DEVICE_ID;
    }

    MASSERT(m_NumSubdevices > 0);
    // This results in incorrect logging for SLI, but we're OK with this,
    // because SLI is going away
    SCOPED_DEV_INST(m_Subdevices[0]);

    CHECK_RC(SetChipFamily());
    CHECK_RC(GetGpuVaCaps());

    /* allocate subdevice(s) */
    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        CHECK_RC(m_Subdevices[subdev]->Initialize(this, subdev));
    }

    // rcInitWatchdog
    // after reorg, this code is only exelwted if SkipRmStateInit is false
    if (RmInitDevice(m_Device) != LW_OK)
    {
        if (!m_Subdevices[0]->IsSOC())
        {
            return RC::WAS_NOT_INITIALIZED;
        }
    }

    m_InitDone |= MODSRM_INIT_RC;

    Printf(Tee::PriDebug, "Initialized devices.\n");

    CHECK_RC(AllocClientStuff());

    m_InitDone |= MODSRM_INIT_CLIENT;

    m_pDisplayMgr = new DisplayMgr(this);
    if (NULL == m_pDisplayMgr)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    m_pDisplay = m_pDisplayMgr->GetRealDisplay();
    if (!m_pDisplay)
    {
        delete m_pDisplayMgr;
        m_pDisplayMgr = nullptr;
        return RC::DID_NOT_INITIALIZE_RM;
    }

    LW0080_CTRL_GPU_GET_DISPLAY_OWNER_PARAMS DispOwnerParams = {0};
    if (!m_Subdevices[0]->IsSOC())
    {
        CHECK_RC(LwRmPtr()->ControlByDevice(
            this,
            LW0080_CTRL_CMD_GPU_GET_DISPLAY_OWNER,
            &DispOwnerParams,
            sizeof (DispOwnerParams)));
    }

    m_pDisplay->SetMasterSubdevice(DispOwnerParams.subDeviceInstance);

    CHECK_RC(m_pDisplay->Initialize());

    Printf(Tee::PriLow, "Created LwRm client and devices for device %d\n",
           m_Device);

    // Sanity check for the Azalia controllers
    //
    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        GpuSubdevice *pGpuSubdevice = m_Subdevices[subdev];
        if (!pGpuSubdevice->HasFeature(GPUSUB_HAS_AZALIA_CONTROLLER) ||
            !pGpuSubdevice->IsFeatureEnabled(GPUSUB_HAS_AZALIA_CONTROLLER))
        {
            if (pGpuSubdevice->IsAzaliaMandatory())
            {
                Printf(Tee::PriHigh,
                       "Error: Azalia is mandatory, but disabled\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    // initialize any client stuff on all subdevice(s)
    // This must be done after to allocating client stuff on the device since
    // subdevice client stuff may depend upon the device client stuff
    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        CHECK_RC(m_Subdevices[subdev]->AllocClientStuff());
    }

    m_Initialized = true;

    return rc;
}

RC GpuDevice::InitializeWithoutRm()
{
    RC rc;
    m_pDisplay = new NullDisplay(this);
    MASSERT(m_pDisplay);

    CHECK_RC(SetChipFamily());

    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        CHECK_RC(m_Subdevices[subdev]->InitializeWithoutRm(this, subdev));
    }

    // this is to ilwoke m_Subdevice->ShutdownSubdevice();
    m_InitializedWithoutRm = true;
    return rc;
}

void GpuDevice::ShutdownWithoutRm()
{
    delete m_pDisplay;
    m_pDisplay = NULL;
    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        m_Subdevices[subdev]->ShutdownSubdevice();
    }
}

RC GpuDevice::GetGpuVaCaps()
{
    RC rc = OK;

    LwRmPtr pLwRm;
    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS param = { 0 };
    rc = pLwRm->ControlByDevice(this,
        LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
        &param,
        sizeof(param));

    if (rc != OK)
    {
        Printf(Tee::PriNormal, "%s: Failed to get va caps. "
            "The error code was 0x%x\n", __FUNCTION__, INT32(rc));
        return rc;
    }

    set<UINT64> pageSizes;
    pageSizes.insert(PAGESIZE_4KB);
    pageSizes.insert(param.bigPageSize);
    pageSizes.insert(param.hugePageSize);
    pageSizes.insert(param.pageSize512MB);
    pageSizes.erase(0);
    m_PageSizes.assign(pageSizes.begin(), pageSizes.end());

    m_BigPageSize         = param.bigPageSize;
    m_CompressionPageSize = param.compressionPageSize;
    m_GpuVaBitsCount      = param.vaBitCount;

    if (m_BigPageSize == 0 && GetSubdevice(0)->IsSOC())
    {
        m_BigPageSize = PAGESIZE_4KB;
    }

    return rc;
}

//! Shutdown the GpuDevice and free subdevices/device in the RM.
RC GpuDevice::Shutdown()
{
    StickyRC firstRc;

    // Shutdown the display
    if (m_pDisplay)
    {
        m_pDisplay->Shutdown();

        delete m_pDisplayMgr;
        m_pDisplayMgr = NULL;
        if (!m_InitializedWithoutRm) // The objected is deleted in ShutdownWithoutRm()
        {
            m_pDisplay = NULL;
        }
    }

    if (m_InitDone & MODSRM_INIT_CLIENT)
    {
        // free any client stuff on all subdevice(s)
        // This must be done before to freeing client stuff on the device since
        // subdevice client stuff may depend upon the device client stuff
        for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
        {
            firstRc = m_Subdevices[subdev]->FreeClientStuff();
        }

        // Destroy the mods clients and channels.
        firstRc = FreeClientStuff();

        m_InitDone &= ~MODSRM_INIT_CLIENT;
    }

    // shutdown subdevice(s)
    // free gpu resources first to prevent any use of unmapped memory, fixes bug 282862
    for (UINT32 subdev = 0; subdev < m_NumSubdevices; subdev++)
    {
        m_Subdevices[subdev]->ShutdownSubdevice();
    }

    // free the device
    if (m_DevicesAllocated)
    {
        for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
        {
            LwRmPtr(client)->FreeDevice(m_Device);
        }
        m_DevicesAllocated = false;
    }

    if (!m_Initialized)
    {
        if (m_InitializedWithoutRm)
        {
            ShutdownWithoutRm();
        }
        else
        {
            Printf(Tee::PriLow,
                   "Device not initialized or already shutdown.  Nothing to do.\n");
        }
        return firstRc;
    }

    if (m_InitDone & MODSRM_INIT_RC)
    {
        // Destroy the RC watchdog channel.
        RmDestroyDevice(m_Device);

        m_InitDone &= ~MODSRM_INIT_RC;
    }

    m_Initialized = false;

    return firstRc;
}

UINT32 GpuDevice::GetNumSubdevices() const
{
    return m_NumSubdevices;
}

//! Get the GpuId for a given subdevice
GpuSubdevice *GpuDevice::GetSubdevice(UINT32 subdev) const
{
    MASSERT(subdev < LW2080_MAX_SUBDEVICES);
    if (subdev >= LW2080_MAX_SUBDEVICES)
    {
        return nullptr;
    }
    return m_Subdevices[subdev];
}

//! Add the GpuSubdevice for the next subdevice (or a particular subdevice)
RC GpuDevice::AddSubdevice(GpuSubdevice *gpuSubdev, UINT32 loc)
{
    MASSERT(gpuSubdev);
    MASSERT(m_NumSubdevices < LW2080_MAX_SUBDEVICES);
    MASSERT(!m_Subdevices[loc]);

    if (m_Initialized)
    {
        Printf(Tee::PriHigh, "Can't add subdevices after initialization!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_Subdevices[loc] = gpuSubdev;

    m_NumSubdevices++;

    return OK;
}


//! Load the Graphics caps once per device
RC GpuDevice::LoadGrCaps()
{
    RC rc;

    if (m_GrCapsLoaded) return OK;

    if (m_Subdevices[0]->IsDFPGA()) // Bug 200023889
    {
        memset(m_GrCapsTable, 0, LW0080_CTRL_GR_CAPS_TBL_SIZE);
    }
    else
    {
        // CheetAh does not support LW2080_CTRL_CM_GR_GET_CAPS_V2 as of now
        // Use old control call for it
        if (GetSubdevice(0)->IsSOC() && Platform::UsesLwgpuDriver())
        {
            LW0080_CTRL_GR_GET_CAPS_PARAMS GraphicsCaps = {0};
            GraphicsCaps.capsTblSize=LW0080_CTRL_GR_CAPS_TBL_SIZE;
            GraphicsCaps.capsTbl=LW_PTR_TO_LwP64(m_GrCapsTable);
            rc = LwRmPtr()->ControlByDevice(this,
                                            LW0080_CTRL_CMD_GR_GET_CAPS,
                                            &GraphicsCaps, sizeof(GraphicsCaps));
        }
        else
        {
            GpuSubdevice *pSubdev = GetSubdevice(0); // use first subdevice
            LW2080_CTRL_GR_GET_CAPS_V2_PARAMS params = { };

            // Set up GrRouteInfo for VF in SMC mode
            if (pSubdev->IsSMCEnabled())
            {
                vector<UINT32> smcEngineIds = pSubdev->GetSmcEngineIds();
                pSubdev->SetGrRouteInfo(&(params.grRouteInfo), smcEngineIds[0]);
            }

            rc = LwRmPtr()->ControlBySubdevice(pSubdev,
                                               LW2080_CTRL_CMD_GR_GET_CAPS_V2,
                                               &params, sizeof(params));

            memcpy(m_GrCapsTable, params.capsTbl, sizeof(params.capsTbl[0]) * LW0080_CTRL_GR_CAPS_TBL_SIZE);
        }
        if (rc != OK)
        {
            memset(m_GrCapsTable, 0, LW0080_CTRL_GR_CAPS_TBL_SIZE);
            Printf(Tee::PriHigh, "Failed to load graphics caps!!\n");
            MASSERT(!"Failed to load graphics caps!");
            return rc;
        }
    }
    m_GrCapsLoaded = true;
    return rc;
}

//! Load the Host caps once per device
RC GpuDevice::LoadHostCaps()
{
    RC rc;

    if (m_HostCapsLoaded) return OK;

    LW0080_CTRL_HOST_GET_CAPS_PARAMS HostCaps = {0};
    HostCaps.capsTblSize=LW0080_CTRL_HOST_CAPS_TBL_SIZE;
    HostCaps.capsTbl=LW_PTR_TO_LwP64(m_HostCapsTable);
    rc = LwRmPtr()->ControlByDevice(this,
                                    LW0080_CTRL_CMD_HOST_GET_CAPS,
                                    &HostCaps, sizeof(HostCaps));
    if (rc != OK)
    {
        memset(m_HostCapsTable, 0, LW0080_CTRL_HOST_CAPS_TBL_SIZE);
        Printf(Tee::PriHigh, "Failed to load host caps!!\n");
        MASSERT(!"Failed to load host caps!");
        return rc;
    }

    m_HostCapsLoaded = true;
    return rc;

}

//! Load the fb caps once per device
RC GpuDevice::LoadFbCaps()
{
    RC rc;

    if (m_FbCapsLoaded) return OK;

    LW0080_CTRL_FB_GET_CAPS_PARAMS FbCaps = {0};
    FbCaps.capsTblSize=LW0080_CTRL_FB_CAPS_TBL_SIZE;
    FbCaps.capsTbl=LW_PTR_TO_LwP64(m_FbCapsTable);
    rc = LwRmPtr()->ControlByDevice(this,
                                    LW0080_CTRL_CMD_FB_GET_CAPS,
                                    &FbCaps, sizeof(FbCaps));
    if (rc != OK)
    {
        memset(m_FbCapsTable, 0, LW0080_CTRL_FB_CAPS_TBL_SIZE);
        Printf(Tee::PriHigh, "Failed to load FB caps!!\n");
        MASSERT(!"Failed to load FB caps!");
        return rc;
    }

    m_FbCapsLoaded = true;
    return rc;
}

//! Load the [graphics/host/fb] caps once per device
RC GpuDevice::LoadCaps()
{
    RC rc;
    CHECK_RC(LoadGrCaps());
    CHECK_RC(LoadHostCaps());
    CHECK_RC(LoadFbCaps());
    return rc;
}

bool GpuDevice::CheckGrCapsBit(CapsBits bit)
{
    UINT32 ret = 0;
    if (LoadGrCaps() != OK)
    {
        MASSERT(!"Failed to check Gr Caps Bit!");
        return false;
    }

    switch (bit)
    {
        case AA_LINES:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_AA_LINES);
            break;
        case AA_POLYS:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_AA_POLYS);
            break;
        case LOGIC_OPS:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_LOGIC_OPS);
            break;
        case TWOSIDED_LIGHTING:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_2SIDED_LIGHTING);
            break;
        case QUADRO_GENERIC:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_QUADRO_GENERIC);
            break;
        case TURBOCIPHER_SUPPORTED:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_TURBOCIPHER_SUPPORTED);
            break;
        case TITAN:
            ret = LW0080_CTRL_GR_GET_CAP(m_GrCapsTable,
                    LW0080_CTRL_GR_CAPS_TITAN);
            break;

        default:
            MASSERT(!"Invalid caps bit sent to GpuDevice::CheckGrCapsBit");
    }

    return ret > 0;
}

bool GpuDevice::CheckHostCapsBit(CapsBits bit)
{
    UINT32 ret = 0;
    if (LoadHostCaps() != OK)
    {
        MASSERT(!"Failed to check Host Caps Bit!");
        return false;
    }

    switch(bit)
    {
        case BUG_114871:
            ret = LW0080_CTRL_HOST_GET_CAP(m_HostCapsTable,
                    LW0080_CTRL_HOST_CAPS_LARGE_NONCOH_UPSTR_WRITE_BUG_114871);
            break;
        case BUG_115115:
            ret = LW0080_CTRL_HOST_GET_CAP(m_HostCapsTable,
                    LW0080_CTRL_HOST_CAPS_LARGE_UPSTREAM_WRITE_BUG_115115);
            break;
        default:
            MASSERT(!"Invalid caps bit sent to GpuDevice::CheckHostCapsBit");
    }

    return ret > 0;
}

bool GpuDevice::CheckFbCapsBit(CapsBits bit)
{
    UINT32 ret = 0;
    if (LoadFbCaps() != OK)
    {
        MASSERT(!"Failed to check Fb Caps Bit!");
        return false;
    }

    switch(bit)
    {
        case RENDER_TO_SYSMEM:
            ret = LW0080_CTRL_FB_GET_CAP(m_FbCapsTable,
                    LW0080_CTRL_FB_CAPS_SUPPORT_RENDER_TO_SYSMEM);
            break;
        case SYSMEM_COMPRESSION:
            ret = LW0080_CTRL_FB_GET_CAP(m_FbCapsTable,
                    LW0080_CTRL_FB_CAPS_SUPPORT_SYSMEM_COMPRESSION);
            break;
        default:
            MASSERT(!"Invalid caps bit sent to GpuDevice::CheckFbCapsBit");
    }

    return ret > 0;
}

bool GpuDevice::CheckCapsBit(CapsBits bit)
{
    bool ret = false;
    switch (bit)
    {
        case AA_LINES:
        case AA_POLYS:
        case LOGIC_OPS:
        case TWOSIDED_LIGHTING:
        case QUADRO_GENERIC:
        case TURBOCIPHER_SUPPORTED:
        case TITAN:
            ret = CheckGrCapsBit(bit);
            break;
        case BUG_114871:
        case BUG_115115:
            ret = CheckHostCapsBit(bit);
            break;
        case RENDER_TO_SYSMEM:
        case SYSMEM_COMPRESSION:
            ret = CheckFbCapsBit(bit);
            break;
        default:
            MASSERT(!"Invalid caps bit sent to GpuDevice::CheckCapsBit");
    }

    return ret;
}

void GpuDevice::PrintCaps()
{
    LoadCaps();

    Printf(Tee::PriNormal, "GRAPHICS_CAPS: 0x");
    for(INT32 i = LW0080_CTRL_GR_CAPS_TBL_SIZE-1; i >= 0; i--)
    {
        Printf(Tee::PriNormal, "%02x", m_GrCapsTable[i]);
    }
    Printf(Tee::PriNormal, "\n");
    Printf(Tee::PriNormal, "HOST_CAPS: 0x");
    for(INT32 i = LW0080_CTRL_HOST_CAPS_TBL_SIZE-1; i >= 0; i--)
    {
        Printf(Tee::PriNormal, "%02x", m_HostCapsTable[i]);
    }
    Printf(Tee::PriNormal, "\n");
    Printf(Tee::PriNormal, "FB_CAPS: 0x");
    for(INT32 i = LW0080_CTRL_FB_CAPS_TBL_SIZE-1; i >= 0; i--)
    {
        Printf(Tee::PriNormal, "%02x", m_FbCapsTable[i]);
    }
    Printf(Tee::PriNormal, "\n");
}

UINT32 GpuDevice::GetFbMem(LwRm *pLwRm)
{
    if (pLwRm == 0)
        pLwRm = LwRmPtr(0).Get();

    MASSERT(m_Handles.count(pLwRm));
    if (GetSubdevice(0)->FbHeapSize())
      return m_Handles[pLwRm].hFrameBufferMem;
    else
      return m_Handles[pLwRm].hNcohGartDmaCtx;
}

RC GpuDevice::SetFbCtxDma(LwRm::Handle CtxDmaHandle, LwRm *pLwRm)
{
    if (pLwRm == 0)
        pLwRm = LwRmPtr(0).Get();

    MASSERT(m_Handles.count(pLwRm));
    m_Handles[pLwRm].hFrameBufferDmaCtx = CtxDmaHandle;
    return OK;
}

UINT32 GpuDevice::GetFbCtxDma(LwRm *pLwRm)
{
    if (pLwRm == 0)
        pLwRm = LwRmPtr(0).Get();

    MASSERT(m_Handles.count(pLwRm));
    return m_Handles[pLwRm].hFrameBufferDmaCtx;
}

UINT32 GpuDevice::GetGartCtxDma(LwRm *pLwRm)
{
    if (pLwRm == 0)
        pLwRm = LwRmPtr(0).Get();

    MASSERT(m_Handles.count(pLwRm));
    return m_Handles[pLwRm].hCohGartDmaCtx;
}

UINT32 GpuDevice::GetMemSpaceCtxDma
(
    Memory::Location Loc,
    bool ForcePhysical,
    LwRm *pLwRm
)
{
    if (pLwRm == 0)
        pLwRm = LwRmPtr(0).Get();
    MASSERT(m_Handles.count(pLwRm));

    if (ForcePhysical)
    {
        // Force the use of a physical context DMA (generally a context DMA for
        // the entire framebuffer)
        switch (Loc)
        {
            case Memory::Optimal:
            case Memory::Fb:
                return m_Handles[pLwRm].hFrameBufferDmaCtx;
            case Memory::Coherent:
            case Memory::NonCoherent:
                // No such thing as a phys ctx DMA for these addr spaces
                return 0;
            default:
                MASSERT(!"Invalid Location");
                return 0;
        }
    }
    else if (GetSubdevice(0)->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
    {
        // MMU model: one context DMA for everything.
        return (m_Handles[pLwRm].hCohGartDmaCtx ? m_Handles[pLwRm].hCohGartDmaCtx :
            m_Handles[pLwRm].hNcohGartDmaCtx);
    }
    else
    {
        // FB/GART model: one context DMA for each memory space.
        switch (Loc)
        {
            case Memory::Optimal:
            case Memory::Fb:
                return m_Handles[pLwRm].hFrameBufferDmaCtx;
            case Memory::Coherent:
                return m_Handles[pLwRm].hCohGartDmaCtx;
            case Memory::NonCoherent:
                return m_Handles[pLwRm].hNcohGartDmaCtx;
            default:
                MASSERT(!"Invalid Location");
                return 0;
        }
    }
}

RC GpuDevice::AllocFlaVaSpace()
{
    RC rc;

    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        LwRmPtr pLwRm(client);
        CHECK_RC(AllocFlaVaSpace(pLwRm.Get()));
    }
    return rc;
}

RC GpuDevice::AllocFlaVaSpace(LwRm *pLwRm)
{
    RC rc;

    m_Handles[pLwRm].hFermiFLAVASpace = 0;
    auto pLwLinkFla = GetSubdevice(0)->GetInterface<LwLinkFla>();
    if (pLwRm->IsClassSupported(FERMI_VASPACE_A, this) &&
        (pLwLinkFla != nullptr) && pLwLinkFla->GetFlaEnabled())
    {
        LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
        vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_FLA;
        CHECK_RC(pLwRm->Alloc(
                    pLwRm->GetDeviceHandle(this),
                    &m_Handles[pLwRm].hFermiFLAVASpace,
                    FERMI_VASPACE_A,
                    &vaParams));
    }
    return rc;
}

LwRm::Handle GpuDevice::GetVASpace(UINT32 vaSpaceClass, UINT32 index, LwRm* pLwRm) const
{
    if (pLwRm == 0)
    {
        pLwRm = LwRmPtr(0).Get();
    }

    Handles::const_iterator rmIt = m_Handles.find(pLwRm);
    MASSERT(rmIt != m_Handles.end());

    if (vaSpaceClass == FERMI_VASPACE_A)
    {
        if (index == LW_VASPACE_ALLOCATION_INDEX_GPU_NEW)
        {
            return rmIt->second.hFermiVASpace;
        }
        if (index == LW_VASPACE_ALLOCATION_INDEX_GPU_FLA)
        {
            return rmIt->second.hFermiFLAVASpace;
        }
    }
    return 0;
}

UINT32 GpuDevice::GetVASpaceClassByHandle(LwRm::Handle vaSpaceHandle) const
{
    UINT32 vaSpaceClass = 0;
    if (0 == vaSpaceHandle)
    {
        if (GetVASpace(FERMI_VASPACE_A)) //Gmmu vaspace is supported
            vaSpaceClass = FERMI_VASPACE_A;
        else
            vaSpaceClass = 0;
    }
    else if (vaSpaceHandle == GetVASpace(FERMI_VASPACE_A,
                                         LW_VASPACE_ALLOCATION_INDEX_GPU_NEW,
                                         0))
    {
        vaSpaceClass = FERMI_VASPACE_A; // Gmmu handle
    }
    else if (vaSpaceHandle == GetVASpace(FERMI_VASPACE_A,
                                         LW_VASPACE_ALLOCATION_INDEX_GPU_FLA,
                                         0))
    {
        vaSpaceClass = FERMI_VASPACE_A; // FLA mmu handle
    }
    else
    {
        MASSERT(!"Unknown VA space handle!");
        return 0;
    }

    return vaSpaceClass;
}

//! \brief Allocate various client-side stuff
//!
//! This was formerly known as RmInitClient.
RC GpuDevice::AllocClientStuff()
{
    RC rc;

    GpuSubdevice *pSubdev = GetSubdevice(0); // use first subdevice
    LwRm *pLwRm0 = LwRmPtr(0).Get();

    // Obtain GPU's heap size
    LW2080_CTRL_FB_INFO fbInfo = {0};
    fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;
    LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&fbInfo) };
    CHECK_RC(pLwRm0->ControlBySubdevice(
                 pSubdev,
                 LW2080_CTRL_CMD_FB_GET_INFO,
                 &params, sizeof(params)));
    m_HeapSizeKb = fbInfo.data;

    CHECK_RC(pSubdev->LoadBusInfo());

    // Create memory objects referring to the entire FB, the entire GPU GART, and
    // the entire AGP aperture, if any.
    if (m_HeapSizeKb)
    {
        if (Platform::UsesLwgpuDriver())
        {
            m_FrameBufferLimit = m_HeapSizeKb * 1024 - 1;
            m_GpuGartLimit = m_FrameBufferLimit;
        }
        CHECK_RC(pLwRm0->AllocMemory(
            &m_Handles[pLwRm0].hFrameBufferMem,
            LW01_MEMORY_LOCAL_USER, DRF_DEF(OS02, _FLAGS, _ALLOC, _NONE),
            NULL, &m_FrameBufferLimit, this));
    }

    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        LwRm *pLwRm = LwRmPtr(client).Get();
        CHECK_RC(AllocSingleClientStuff(pLwRm, pSubdev));
    }

    // Check for broken FB (i.e. in GF100 a01).
    LW2080_CTRL_FB_INFO Info = { LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN };
    LW2080_CTRL_FB_GET_INFO_PARAMS Params = { 1, LW_PTR_TO_LwP64(&Info) };
    CHECK_RC(pLwRm0->ControlBySubdevice(
            GetSubdevice(0),
            LW2080_CTRL_CMD_FB_GET_INFO,
            &Params, sizeof(Params)));

    const bool FbIsBroken = (Info.data != 0);

    // Check for Cache Only Mode.
    LW2080_CTRL_FB_INFO Info1 = { LW2080_CTRL_FB_INFO_INDEX_L2CACHE_ONLY_MODE };
    LW2080_CTRL_FB_GET_INFO_PARAMS Params1 = { 1, LW_PTR_TO_LwP64(&Info1) };
    CHECK_RC(pLwRm0->ControlBySubdevice(
            GetSubdevice(0),
            LW2080_CTRL_CMD_FB_GET_INFO,
            &Params1, sizeof(Params1)));

    const bool bIsL2CacheOnlyMode = (Info1.data != 0);

    // Store the frame buffer size.
    // and L2 Cache only Bit
    for (UINT32 subdev = 0; subdev < GetNumSubdevices(); subdev++)
    {
        pSubdev = GetSubdevice(subdev);

        if (FbIsBroken)
        {
            // Don't put any surfaces in FB, or else the gpu will hang.
            pSubdev->SetFbHeapSize(0);
        }
        else
        {
            pSubdev->SetFbHeapSize(1024*UINT64(m_HeapSizeKb));
        }

        if (bIsL2CacheOnlyMode)
        {
            pSubdev->SetL2CacheOnlyMode(true);
        }
        else
        {
            pSubdev->SetL2CacheOnlyMode(false);
        }
    }

    m_pUserdManager = new UserdManager(this, m_NumUserdAllocs, m_UserdLocation);
    CHECK_RC(m_pUserdManager->Alloc());

    // Get the protected region manager so that if it hasn't been created
    // yet, it will be now.  No need to use the pointer, though.
    ProtectedRegionManager *manager;
    CHECK_RC(GetProtectedRegionManager(&manager));

    return OK;
}

//! \brief Destroy the various client-side stuff
//!
//! This was formerly known as RmDestroyClient.
RC GpuDevice::FreeClientStuff()
{
    StickyRC firstRc;

    if (m_pUserdManager)
    {
        m_pUserdManager->Free();
        delete m_pUserdManager;
        m_pUserdManager = nullptr;
    }

    if (m_ProtectedRegionManager.get())
    {
        m_ProtectedRegionManager.reset(nullptr);
    }

    // Free the engine runlists, if any
    for (multimap<UINT32, Runlist*>::iterator iter = m_Runlists.begin();
         iter != m_Runlists.end(); ++iter)
    {
        iter->second->Free();
        delete iter->second;
    }
    m_Runlists.clear();

    // Since the memory handles were actually duplicated from client 0, they
    // need to be freed in reverse order
    for (INT32 client = (LwRmPtr::GetValidClientCount() - 1);
         client >= 0; client--)
    {
        LwRm * pLwRm = LwRmPtr((UINT32)client).Get();
        FreeFlaVaSpace(pLwRm);
        FreeSingleClientStuff(pLwRm);
    }

    return firstRc;
}

//! Wrapper to get the first subdevice's DeviceId.
//!
//! I finally had to put this in to get golden.cpp to work for both
//! GPU and WMP builds.  This way, GpuPtr()->DeviceId() works, as well as
//! gpudev->DeviceId().
Gpu::LwDeviceId GpuDevice::DeviceId() const
{
    return GetSubdevice(0)->DeviceId();
}

UINT32 GpuDevice::GetDisplaySubdeviceInst() const
{
    MASSERT(m_pDisplay != 0);
    return m_pDisplay->GetMasterSubdevice();
}

//! Return the GpuSubdevice pointer which is the display subdevice
GpuSubdevice *GpuDevice::GetDisplaySubdevice() const
{
    return GetSubdevice(GetDisplaySubdeviceInst());
}

//! We must override Device::HasBug here because GpuDevice is a collection of
//! N GpuSubdevice objects, which might have varying bugs.
//! Also, the GpuDevice itself might require a WAR, for example there could be
//! a BR04 bug that logically belongs on the GpuDevice.
/* virtual */
bool GpuDevice::HasBug(UINT32 bugNum) const
{
    for (UINT32 sub = 0; sub < GetNumSubdevices();  ++sub)
    {
        MASSERT(sub < LW2080_MAX_SUBDEVICES);
        MASSERT(m_Subdevices[sub]);
        if (m_Subdevices[sub]->HasBug(bugNum))
            return true;
    }
    return Device::HasBug(bugNum);
}

bool GpuDevice::IsStrictSku()
{
    return CheckCapsBit(QUADRO_GENERIC);
}

//! Determine if rendering to system memory is supported on this GpuDevice
bool GpuDevice::SupportsRenderToSysMem()
{
    return CheckCapsBit(GpuDevice::RENDER_TO_SYSMEM);
}

void GpuDevice::FreeNonEssentialAllocations()
{
    m_pDisplay->FreeNonEssentialAllocations();
}

UINT32 GpuDevice::FixSubdeviceInst(UINT32 subdevInst)
{
    if (subdevInst == Gpu::UNSPECIFIED_SUBDEV)
    {
        return 0;
    }
    MASSERT(subdevInst < m_NumSubdevices);
    return subdevInst;
}

//--------------------------------------------------------------------
//! \brief Return true if the indicated page size is supported on this GPU
//!
bool GpuDevice::SupportsPageSize(UINT64 pageSize) const
{
    MASSERT(!m_PageSizes.empty()); // Set in Initialize or GetSupportedPageSizes
    return binary_search(m_PageSizes.begin(), m_PageSizes.end(), pageSize);
}

//--------------------------------------------------------------------
//! \brief Retrieve all page sizes supported on this GPU
//!
//! This method can be called before GpuDevice::Initialize, since it
//! loads page-size info from the RM if needed.
//!
//! \param *pSizes On exit, contains the page sizes in order from
//! smallest to largest.
//!
RC GpuDevice::GetSupportedPageSizes(vector<UINT64>* pSizes)
{
    RC rc;
    if (m_PageSizes.empty())
    {
        CHECK_RC(GetGpuVaCaps());
    }
    if (pSizes != nullptr)
    {
        *pSizes = m_PageSizes;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the biggest page size supported on this GPU
//!
UINT64 GpuDevice::GetMaxPageSize() const
{
    MASSERT(!m_PageSizes.empty()); // Set in Initialize or GetSupportedPageSizes
    return m_PageSizes.back();
}

//--------------------------------------------------------------------
//! \brief Return the biggest page size between lowerBound & upperBound
//!
//! Return the biggest page size supported on this GPU such that
//! lowerBound <= pageSize <= upperBound.  Return 0 if there is no
//! such page size.
//!
UINT64 GpuDevice::GetMaxPageSize(UINT64 lowerBound, UINT64 upperBound) const
{
    MASSERT(!m_PageSizes.empty()); // Set in Initialize or GetSupportedPageSizes
    for (auto pPageSize = m_PageSizes.rbegin();
         pPageSize != m_PageSizes.rend(); ++pPageSize)
    {
        if (*pPageSize <= upperBound)
        {
            return (*pPageSize >= lowerBound) ? *pPageSize : 0;
        }
    }
    return 0;
}

UINT64 GpuDevice::GetBigPageSize() const
{
    MASSERT(!m_PageSizes.empty()); // Set in Initialize or GetSupportedPageSizes
    return m_BigPageSize;
}

RC GpuDevice::GetCompressionPageSize(UINT64 *size)
{
    RC rc = OK;
    *size = 0;

    if (m_CompressionPageSize == 0)
    {
        CHECK_RC(GetGpuVaCaps());
    }

    *size = m_CompressionPageSize;
    return rc;
}

RC GpuDevice::GetGpuVaBitCount(UINT32 *bitCount)
{
    RC rc = OK;
    *bitCount = 0;

    if (m_GpuVaBitsCount == 0)
    {
        CHECK_RC(GetGpuVaCaps());
    }

    *bitCount = m_GpuVaBitsCount;
    return rc;
}

//! \brief If there is a fake EDID specified in JS, apply it to all possible
//! DFPs.
void GpuDevice::SetupSimulatedEdids()
{
    bool simulateAllDfps;
    JavaScriptPtr pJs;

    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_SimulateAllDfps", &simulateAllDfps) != OK)
        return;

    Display * pDisplay = GetDisplay();

    // By default we only want to fake the selected display
    UINT32 fakeMask = pDisplay->GetPrimaryDisplay();

    if (simulateAllDfps)
    {
        Printf(Tee::PriLow, "Faking EDIDs on all possible DFPs. (Device Inst = %d)\n", GetDeviceInst());

        // Since we're simulating all DFPs now, our mask should cover everything
        fakeMask = 0xffffffff;
    }

    SetupSimulatedEdids(fakeMask);

    return;
}

//! \brief If there is a fake EDID specified in JS, apply it to the possible
//! specified DFPs.
void GpuDevice::SetupSimulatedEdids(UINT32 fakeMask)
{
    UINT32 simulatedEdidType;
    JsArray JsEdid;
    JavaScriptPtr pJs;

    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_SimulatedDdEdid", &JsEdid) != OK)
        return;

    if (pJs->GetProperty(pJs->GetGlobalObject(), "g_SimulatedEdidType", &simulatedEdidType) != OK)
        return;

    Display * pDisplay = GetDisplay();
    // Apply forced DP lane count here since it affects golden generation for simulating DP
    if (simulatedEdidType == Display::Encoder::DP)
    {
        JsArray JsForceDpLanesByDev;
        if (pJs->GetProperty(pJs->GetGlobalObject(),
                             "g_ForceDPLanesByDev", &JsForceDpLanesByDev) == OK)
        {
            UINT32 devInst = GetDeviceInst();
            UINT32 forcedDpLaneCount = 0;
            if (devInst < JsForceDpLanesByDev.size() &&
                (pJs->FromJsval(JsForceDpLanesByDev[devInst], &forcedDpLaneCount) == OK))
            {
                if (OK != pDisplay->SetForceDPLanes(forcedDpLaneCount))
                {
                    Printf(Tee::PriWarn,
                           "%s: Unable to force DP lane count = %u on device instance %u\n",
                           __FUNCTION__, forcedDpLaneCount, devInst);
                }
            }
        }
    }

    // Retrieve the EDID if possible
    UINT32 EdidSize = (UINT32) JsEdid.size();
    if (EdidSize > EDID_SIZE_MAX)
    {
        Printf(Tee::PriNormal,
            "SetupSimulatedEdids: Edid is too large (%d bytes).\n", EdidSize);
        return;
    }

    UINT08 Edid[EDID_SIZE_MAX];
    for (UINT32 i = 0; i < EdidSize; ++i)
    {
        UINT32 Element;
        if (pJs->FromJsval(JsEdid[i], &Element))
            return;
        if (Element > 0xFFU)
            return;
        Edid[i] = Element;
    }

    RC rc;
    UINT32 connectors;

    if (pDisplay->GetConnectors(&connectors) != OK)
    {
        Printf(Tee::PriNormal, "Unable to get connectors to simualte DFPs\n");
        return;
    }

    UINT32 connectorsToCheck = connectors;
    while (connectorsToCheck)
    {
        UINT32 disp = connectorsToCheck & ~(connectorsToCheck - 1);
        connectorsToCheck ^= disp;

        // First make sure we should attempt this display
        if (!(disp & fakeMask))
            continue;

        if (pDisplay->GetDisplayType(disp) == Display::DFP)
        {
            Display::Encoder encoder;
            if (pDisplay->GetEncoder(disp, &encoder) != OK)
                continue;

            if (encoder.Signal != (Display::Encoder::SignalType)simulatedEdidType)
                continue;
        }
        else if ((pDisplay->GetDisplayType(disp) != Display::CRT) ||
                 (Display::Encoder::CRT != simulatedEdidType))
        {
            continue;
        }

        if (simulatedEdidType == Display::Encoder::DP)
        {
            UINT32 simulatedDisplayIDsMask = 0;
            rc = pDisplay->EnableSimulatedDPDevices(disp,
                Display::NULL_DISPLAY_ID, Display::DPMultistreamDisabled,
                1, Edid, EdidSize, &simulatedDisplayIDsMask);
            MASSERT(simulatedDisplayIDsMask == disp); // Expect SST
        }
        else
        {
            rc = pDisplay->SetEdid(disp, (UINT08 *)Edid, EdidSize);

            if (rc == OK)
            {
                Printf(Tee::PriLow, "\tFaked EDID on display 0x%x.\n", disp);

                // Make sure to mark this display as forced
                pDisplay->SetForceDetectMask(pDisplay->GetForceDetectMask() | disp);
            }
        }
    }

    return;
}

/* virtual */
int GpuDevice::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    int result = 0;

    for (UINT32 i = 0; i < GetNumSubdevices(); i++)
    {
        result = m_Subdevices[i]->EscapeWrite(Path, Index, Size, Value);

        if (result != 0)
            return result;
    }

    return 0;
}

/* virtual */
int GpuDevice::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
    if (GetNumSubdevices() > 1)
    {
        Printf(Tee::PriHigh,
               "GpuDevice::EscapeRead not supported for SLI, use GpuSubdevice::EscapeRead instead\n");
        return -1;
    }

    return m_Subdevices[0]->EscapeRead(Path, Index, Size, Value);
}

/* virtual */
int GpuDevice::EscapeWriteBuffer(const char *Path, UINT32 Index, size_t Size, const void *Buf)
{
    int result = 0;

    for (UINT32 i = 0; i < GetNumSubdevices(); i++)
    {
        result = m_Subdevices[i]->EscapeWriteBuffer(Path, Index, Size, Buf);

        if (result != 0)
            return result;
    }

    return 0;
}

/* virtual */
int GpuDevice::EscapeReadBuffer(const char *Path, UINT32 Index, size_t Size, void *Buf) const
{
    if (GetNumSubdevices() > 1)
    {
        Printf(Tee::PriHigh,
               "GpuDevice::EscapeRead not supported for SLI, use GpuSubdevice::EscapeRead instead\n");
        return -1;
    }

    return m_Subdevices[0]->EscapeReadBuffer(Path, Index, Size, Buf);
}

RC GpuDevice::GetRunlist
(
    UINT32 Engine,
    LwRm *pLwRm,
    bool MustBeAllocated,
    Runlist **ppRunlist
)
{
    pair<multimap<UINT32, Runlist*>::iterator,
         multimap<UINT32, Runlist*>::iterator> engineRunlists;
    multimap<UINT32, Runlist*>::iterator pRunListEntry;
    Runlist *pRunlist = NULL;
    RC rc;

    // If a runlist with the indicated Engine & RmClient already
    // exists, then use that.
    //
    engineRunlists = m_Runlists.equal_range(Engine);
    for (pRunListEntry = engineRunlists.first;
         pRunListEntry != engineRunlists.second; pRunListEntry++)
    {
        if (pRunListEntry->second->GetRmClient() == pLwRm)
        {
            pRunlist = pRunListEntry->second;
            break;
        }
    }

    // If we didn't find a matching runlist above, create a new one.
    //
    if (pRunlist == NULL)
    {
        pRunlist = new Runlist(this, pLwRm, Engine);
        m_Runlists.insert(pair<UINT32, Runlist*>(Engine, pRunlist));
    }

    // If MustBeAllocated is set, then make sure we've called Alloc().
    //
    if (MustBeAllocated && !pRunlist->IsAllocated())
    {
        CHECK_RC(pRunlist->Alloc(Memory::NonCoherent,
                                 GpuPtr()->GetRunlistSize()));
    }

    // Return the runlist
    //
    *ppRunlist = pRunlist;
    return rc;
}

vector<Runlist*> GpuDevice::GetAllocatedRunlists() const
{
    vector<Runlist*> allocatedRunlists;
    multimap<UINT32, Runlist*>::const_iterator pRunlistEntry;

    for (pRunlistEntry = m_Runlists.begin();
         pRunlistEntry != m_Runlists.end(); ++pRunlistEntry)
    {
        if (pRunlistEntry->second->IsAllocated())
        {
            allocatedRunlists.push_back(pRunlistEntry->second);
        }
    }
    return allocatedRunlists;
}

RC GpuDevice::GetEngines(vector<UINT32> *pEngines, LwRm* pLwRm)
{
    MASSERT(pEngines != NULL);
    RC rc;
    if (!pLwRm)
        pLwRm = LwRmPtr().Get();

    GpuSubdevice *pGpuSubdevice = GetSubdevice(0);
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS Params;

    // Get the list of engines
    //
    memset(&Params, 0, sizeof(Params));
    CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                                       LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                                       &Params,
                                       sizeof(Params)));
    UINT32 EngineCount = Params.engineCount;

    m_Engines[pLwRm].resize(EngineCount);
    memcpy(&(m_Engines[pLwRm][0]), Params.engineList,
           EngineCount * sizeof(Params.engineList[0]));

    *pEngines = m_Engines[pLwRm];
    return rc;
}

RC GpuDevice::GetEnginesFilteredByClass(const vector<UINT32>& classIds,
                                        vector<UINT32>* pEngines,
                                        LwRm* pLwRm)
{
    MASSERT(pEngines != NULL);
    RC rc;
    if (!pLwRm)
        pLwRm = LwRmPtr().Get();

    vector<UINT32> AllEngines;

    // Get current engines
    CHECK_RC(GetEngines(&AllEngines, pLwRm));

    // check each engine's classlist for the desired classIds
    for ( UINT32 i = 0; i < AllEngines.size(); ++i)
    {
        UINT32 lwrrentEngine = AllEngines[i];
        vector<UINT32> classList;

        GpuSubdevice *pGpuSubdevice = GetSubdevice(0);
        LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS params = {0};
        params.engineType = lwrrentEngine;
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                     &params, sizeof(params)));
        classList.resize(params.numClasses);

        memset(&params, 0, sizeof(params));
        params.engineType = lwrrentEngine;
        params.numClasses = (LwU32)classList.size();
        params.classList = LW_PTR_TO_LwP64(&classList[0]);
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                     &params, sizeof(params)));

        // Sort the classlist for the binary search
        sort(classList.begin(), classList.end());

        for (UINT32 j = 0; j < classIds.size(); ++j)
            if (binary_search(classList.begin(), classList.end(), classIds[j]))
            {
                pEngines->push_back(lwrrentEngine);
                break;
            }
    }

    return rc;
}

RC GpuDevice::GetPhysicalEnginesFilteredByClass(const vector<UINT32>& classIds,
                                                vector<UINT32>* pEngines,
                                                LwRm *pLwRm)
{
    MASSERT(pEngines);
    MASSERT(classIds.size() >= 1);
    RC rc;
    LwRmPtr pLwRmPtr;
    GpuSubdevice *pGpuSubdevice = GetSubdevice(0);

    vector<UINT32> filteredEngines;
    if (pLwRm == NULL)
    {
        pLwRm = pLwRmPtr.Get();
    }
    CHECK_RC(GetEnginesFilteredByClass(classIds, &filteredEngines, pLwRm));

    typedef map<UINT32,UINT32> PhysicalToLogicalMap;
    map<UINT32, PhysicalToLogicalMap> engineMaps;

    for (UINT32 engine : filteredEngines)
    {
        switch (engine)
        {
            case LW2080_ENGINE_TYPE_COPY0:
            case LW2080_ENGINE_TYPE_COPY1:
            case LW2080_ENGINE_TYPE_COPY2:
            case LW2080_ENGINE_TYPE_COPY3:
            case LW2080_ENGINE_TYPE_COPY4:
            case LW2080_ENGINE_TYPE_COPY5:
            case LW2080_ENGINE_TYPE_COPY6:
            case LW2080_ENGINE_TYPE_COPY7:
            case LW2080_ENGINE_TYPE_COPY8:
            case LW2080_ENGINE_TYPE_COPY9:
            {
                PhysicalToLogicalMap& pceToLce = engineMaps[LW2080_ENGINE_TYPE_COPY0];

                UINT32 pceMask = 0x0;
                if (Platform::UsesLwgpuDriver())
                {
                    // rmapi_tegra doesn't support LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK.
                    // So far on all CheetAh chips there is only one physical CE, so
                    // return it as bit 1.
                    pceMask = BIT(0);
                }
                else
                {
                    LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS params = {};
                    params.ceEngineType = engine;
                    params.pceMask = 0x0;
                    CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                                                       LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK,
                                                       &params, sizeof(params)));
                    pceMask = params.pceMask;
                }

                for (INT32 lwrPceIdx = Utility::BitScanForward(pceMask, 0);
                     lwrPceIdx != -1;
                     lwrPceIdx = Utility::BitScanForward(pceMask, lwrPceIdx+1))
                {
                    if (pceToLce.find(lwrPceIdx) != pceToLce.end())
                    {
                        // Favor non-GRCEs
                        // Find out whether the engine lwrrently mapped to this
                        // PCE is a GRCE. If it is, replace it.

                        DmaCopyAlloc dmaAlloc;
                        bool bIsGrce;

                        CHECK_RC(dmaAlloc.IsGrceEngine(engine,
                                                       pGpuSubdevice,
                                                       pLwRm,
                                                       &bIsGrce));
                        if (!bIsGrce)
                        {
                            pceToLce[lwrPceIdx] = engine;
                        }
                    }
                    else
                    {
                        pceToLce[lwrPceIdx] = engine;
                    }
                }
                break;
            }
            default:
            {
                engineMaps[engine][0] = engine;
                break;
            }
        }
    }

    for (auto engineItr = engineMaps.begin(); engineItr != engineMaps.end(); engineItr++)
    {
        for (auto physItr = engineItr->second.begin(); physItr != engineItr->second.end(); physItr++)
        {
            pEngines->push_back(physItr->second);
        }
    }

    sort(pEngines->begin(), pEngines->end());
    pEngines->erase(unique(pEngines->begin(), pEngines->end()), pEngines->end());

    return OK;
}

/* static */ const char *GpuDevice::GetEngineName(UINT32 engine)
{
    // This should match the engines in sdk/lwpu/inc/class/cl2080.h.
    //
    switch (engine)
    {
        case LW2080_ENGINE_TYPE_NULL:       return "NULL";
        // Keep the first engine name as GRAPHICS only since policy files use this
        // to match the engine name
        case LW2080_ENGINE_TYPE_GR0:        return "GRAPHICS";
        case LW2080_ENGINE_TYPE_GR1:        return "GRAPHICS1";
        case LW2080_ENGINE_TYPE_GR2:        return "GRAPHICS2";
        case LW2080_ENGINE_TYPE_GR3:        return "GRAPHICS3";
        case LW2080_ENGINE_TYPE_GR4:        return "GRAPHICS4";
        case LW2080_ENGINE_TYPE_GR5:        return "GRAPHICS5";
        case LW2080_ENGINE_TYPE_GR6:        return "GRAPHICS6";
        case LW2080_ENGINE_TYPE_GR7:        return "GRAPHICS7";
        case LW2080_ENGINE_TYPE_COPY0:      return "COPY0";
        case LW2080_ENGINE_TYPE_COPY1:      return "COPY1";
        case LW2080_ENGINE_TYPE_COPY2:      return "COPY2";
        case LW2080_ENGINE_TYPE_COPY3:      return "COPY3";
        case LW2080_ENGINE_TYPE_COPY4:      return "COPY4";
        case LW2080_ENGINE_TYPE_COPY5:      return "COPY5";
        case LW2080_ENGINE_TYPE_COPY6:      return "COPY6";
        case LW2080_ENGINE_TYPE_COPY7:      return "COPY7";
        case LW2080_ENGINE_TYPE_COPY8:      return "COPY8";
        case LW2080_ENGINE_TYPE_COPY9:      return "COPY9";
        case LW2080_ENGINE_TYPE_VP:         return "VP";
        case LW2080_ENGINE_TYPE_PPP:        return "PPP";
        case LW2080_ENGINE_TYPE_BSP:        return "BSP";
        case LW2080_ENGINE_TYPE_LWDEC1:     return "LWDEC1";
        case LW2080_ENGINE_TYPE_LWDEC2:     return "LWDEC2";
        case LW2080_ENGINE_TYPE_LWDEC3:     return "LWDEC3";
        case LW2080_ENGINE_TYPE_LWDEC4:     return "LWDEC4";
        case LW2080_ENGINE_TYPE_LWDEC5:     return "LWDEC5";
        case LW2080_ENGINE_TYPE_LWDEC6:     return "LWDEC6";
        case LW2080_ENGINE_TYPE_LWDEC7:     return "LWDEC7";
        case LW2080_ENGINE_TYPE_MPEG:       return "MPEG";
        case LW2080_ENGINE_TYPE_SW:         return "SW";
        case LW2080_ENGINE_TYPE_CIPHER:     return "CIPHER";
        case LW2080_ENGINE_TYPE_VIC:        return "VIC";
        case LW2080_ENGINE_TYPE_MSENC:      return "MSENC";
        case LW2080_ENGINE_TYPE_LWENC1:     return "LWENC1";
        case LW2080_ENGINE_TYPE_LWENC2:     return "LWENC2";
        case LW2080_ENGINE_TYPE_SEC2:       return "SEC2";
        case LW2080_ENGINE_TYPE_LWJPG:      return "LWJPG";
        case LW2080_ENGINE_TYPE_LWJPEG1:    return "LWJPEG1";
        case LW2080_ENGINE_TYPE_LWJPEG2:    return "LWJPEG2";
        case LW2080_ENGINE_TYPE_LWJPEG3:    return "LWJPEG3";
        case LW2080_ENGINE_TYPE_LWJPEG4:    return "LWJPEG4";
        case LW2080_ENGINE_TYPE_LWJPEG5:    return "LWJPEG5";
        case LW2080_ENGINE_TYPE_LWJPEG6:    return "LWJPEG6";
        case LW2080_ENGINE_TYPE_LWJPEG7:    return "LWJPEG7";
        case LW2080_ENGINE_TYPE_OFA:        return "OFA";
        case LW2080_ENGINE_TYPE_ALLENGINES: return "ALLENGINES";
    }

    // Should never get here
    //
    MASSERT(!"Error: Unknown engine in GpuDevice::GetEngineName()."
            "  Have new engines been added to cl2080.h?");
    return NULL;
}

RC GpuDevice::StopEngineRunlist(UINT32 engine, LwRm* pLwRm)
{
    if (!pLwRm)
        pLwRm = LwRmPtr().Get();
    LW0080_CTRL_FIFO_STOP_RUNLIST_PARAMS stopRunlistParams = {0};
    stopRunlistParams.engineID = engine;
    return pLwRm->ControlByDevice(
                         this,
                         LW0080_CTRL_CMD_FIFO_STOP_RUNLIST,
                         &stopRunlistParams, sizeof(stopRunlistParams));
}

RC GpuDevice::StartEngineRunlist(UINT32 engine, LwRm* pLwRm)
{
    if (!pLwRm)
        pLwRm = LwRmPtr().Get();
    LW0080_CTRL_FIFO_START_RUNLIST_PARAMS startRunlistParams = {0};
    startRunlistParams.engineID = engine;
    return pLwRm->ControlByDevice(
                         this,
                         LW0080_CTRL_CMD_FIFO_START_RUNLIST,
                         &startRunlistParams, sizeof(startRunlistParams));
}

//! Configure the device to use vista-style two-stage robust channel
//! error recovery, which ilwolves direct callbacks between mods and
//! RM.  Client side resman only.
RC GpuDevice::SetUseRobustChannelCallback(bool useRobustChannelCallback)
{
    if (useRobustChannelCallback && !Platform::HasClientSideResman())
    {
        Printf(Tee::PriHigh,
               "GpuDevice::SetUseRobustChannelCallback is only supported in"
               " client-side RM\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_UseRobustChCallback = useRobustChannelCallback;
    return OK;
}

Runlist *GpuDevice::GetRunlist(Channel *pChannel) const
{
    for (multimap<UINT32, Runlist*>::const_iterator iter = m_Runlists.begin();
         iter != m_Runlists.end(); ++iter)
    {
        if (iter->second->ContainsChannel(pChannel))
        {
            return iter->second;
        }
    }
    return NULL;
}

RC GpuDevice::SetContextOverride
(
    UINT32 regAddr,
    UINT32 andMask,
    UINT32 orMask,
    LwRm   *pLwRm
)
{
    if (!pLwRm)
        pLwRm = LwRmPtr().Get();

    LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE_PARAMS GrCtxArgs = {};
    GrCtxArgs.regAddr = regAddr;
    GrCtxArgs.andMask = andMask;
    GrCtxArgs.orMask  = orMask;

    return pLwRm->ControlByDevice(
            this,
            LW0080_CTRL_CMD_GR_SET_CONTEXT_OVERRIDE,
            &GrCtxArgs,
            sizeof (GrCtxArgs));
}

//--------------------------------------------------------------------
//! \brief Reset all GPUs in the device.
//!
//! Reset the device back to the power-on state.  All pending work on
//! the GPU is aborted.  After calling this method, the device will be
//! in an unusable state until the caller calls RecoverFromReset().
//!
//! All channels/surfaces created by the pending test are lost, and
//! should preferably be freed between Reset() and RecoverFromReset().
//!
RC GpuDevice::Reset(UINT32 lw0080CtrlBifResetType)
{
    RC rc;

    LW0080_CTRL_BIF_RESET_PARAMS Params = {0};
    Params.flags = DRF_VAL(0080_CTRL_BIF_RESET, _FLAGS, _TYPE,
                           lw0080CtrlBifResetType);
    CHECK_RC(LwRmPtr()->ControlByDevice(this, LW0080_CTRL_CMD_BIF_RESET,
                                        &Params, sizeof(Params)));
    m_ResetInProgress = true;
    m_ResetType = lw0080CtrlBifResetType;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Re-init the device after calling Reset()
//!
RC GpuDevice::RecoverFromReset()
{
    JavaScriptPtr pJs;
    RC rc;

    if (!m_ResetInProgress)
    {
        return OK;
    }

    CHECK_RC(RecoverFromResetShutdown(ResetControls::DefaultReset));
    CHECK_RC(RecoverFromResetInit(ResetControls::DefaultReset));

    // Clear m_ResetInProgress, now that the device is usable again.
    //
    m_ResetInProgress = false;
    m_ResetType = 0;
    return rc;
}

RC GpuDevice::RecoverFromResetShutdown(UINT32 resetControlsMask)
{
    RC rc;
    //
    // *** Temporary function until RM bug 848554 is fixed. ***
    //
    // RM explicitly blocks all access to registers after a reset. However, RM
    // shutdown code is not well-equipped to deal with blocked register
    // accesses and thus will assert several times. Disabling breakpoints
    // during shutdown after a reset is a temporary solution until bug 848554
    // is fixed.
    //
    unique_ptr<Platform::DisableBreakPoints> disBp;
    if (Platform::GetSimulationMode() == Platform::RTL ||
        Platform::GetSimulationMode() == Platform::Hardware)
    {
        disBp.reset(new Platform::DisableBreakPoints);
    }

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;
    for (UINT32 i = 0; i < GetNumSubdevices(); ++i )
    {
        pGpuDevMgr->RemoveAllPeerMappings(GetSubdevice(i));
    }

    // Shut down the device
    if (m_ResetType == LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SBR)
    {
        resetControlsMask |= ResetControls::GpuIsLost;
    }

    for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
    {
        CHECK_RC(GetSubdevice(ii)->FreeClientStuff(resetControlsMask));
    }
    CHECK_RC(FreeClientStuff());

    for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
    {
        GetSubdevice(ii)->ShutdownSubdevice(GpuSubdevice::Trigger::RECOVER_FROM_RESET);
    }

    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        LwRmPtr(client)->FreeDevice(m_Device);
    }
    m_DevicesAllocated = false;

    if (!RmDestroyDevice(GetDeviceInst()))
    {
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
    {
        GetSubdevice(ii)->DestroyInRm();
        GetSubdevice(ii)->DisableSMC();
    }

    // *** Temporary function until RM bug 848554 is fixed. ***
    disBp.reset(0);

    return rc;
}

RC GpuDevice::RecoverFromResetInit(UINT32 resetControlsMask)
{
    RC rc;
    JavaScriptPtr pJs;

    bool sbr = (m_ResetType == LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SBR);
    bool SkipRmStateInit = false;
    pJs->GetProperty(Gpu_Object, Gpu_SkipRmStateInit, &SkipRmStateInit);
    for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
    {
        CHECK_RC(GetSubdevice(ii)->InitGpuInRm(SkipRmStateInit, true, false, sbr));
    }

    CHECK_RC(AllocDevices());
    for (UINT32 ii = 0; ii < LwRmPtr::GetValidClientCount(); ++ii)
    {
        LwRmPtr pLwRm(ii);

        for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
        {
            CHECK_RC(GetSubdevice(ii)->Initialize(GpuSubdevice::Trigger::RECOVER_FROM_RESET));
        }
    }

    if (RmInitDevice(GetDeviceInst()) != LW_OK)
    {
        return RC::WAS_NOT_INITIALIZED;
    }

    CHECK_RC(AllocClientStuff());
    CHECK_RC(AllocFlaVaSpace());
    CHECK_RC(m_pDisplay->Initialize());
    for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
    {
        CHECK_RC(GetSubdevice(ii)->AllocClientStuff(resetControlsMask));
    }
    return rc;
}

RC GpuDevice::GetProtectedRegionManager(ProtectedRegionManager **manager)
{
    RC rc;
    MASSERT(manager);

    if (nullptr == m_ProtectedRegionManager.get())
    {
        m_ProtectedRegionManager.reset(new ProtectedRegionManager(this));

        UINT64 vprSize;
        UINT64 vprAlignment;
        UINT64 acr1Size;
        UINT64 acr1Alignment;
        UINT64 acr2Size;
        UINT64 acr2Alignment;

        GpuSubdevice *gpuSubdevice = GetSubdevice(0);

        CHECK_RC(gpuSubdevice->GetVprSize(&vprSize, &vprAlignment));
        CHECK_RC(gpuSubdevice->GetAcr1Size(&acr1Size, &acr1Alignment));
        CHECK_RC(gpuSubdevice->GetAcr2Size(&acr2Size, &acr2Alignment));

        CHECK_RC(m_ProtectedRegionManager->AllocRegions(
            vprSize,
            vprAlignment,
            acr1Size,
            acr1Alignment,
            acr2Size,
            acr2Alignment));

        for (UINT32 ii = 0; ii < GetNumSubdevices(); ++ii)
        {
            CHECK_RC(m_ProtectedRegionManager->InitSubdevice(GetSubdevice(ii)));
        }

    }

    *manager = m_ProtectedRegionManager.get();

    return rc;
}

GpuDevice::GpuFamily GpuDevice::GetFamilyFromDeviceId() const
{
    if ((Xp::GetOperatingSystem() == Xp::OS_WINDA) ||
        (Xp::GetOperatingSystem() == Xp::OS_LINDA))
    {
        // Direct Amodel has no device id
        return GpuDevice::GpuFamily::None;
    }

    return static_cast<GpuDevice::GpuFamily>(Gpu::DeviceIdToFamily(DeviceId()));
}

RC GpuDevice::AllocDevices()
{
    RC rc;

    if (m_DevicesAllocated)
    {
        return rc;
    }

    if (m_Device == LW0080_MAX_DEVICES)
    {
        Printf(Tee::PriError,
            "Unable to allocate devices due to uninitialized device ID.\n");
        return RC::ILWALID_DEVICE_ID;
    }

    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        CHECK_RC(LwRmPtr(client)->AllocDevice(m_Device));
    }

    m_DevicesAllocated = true;

    return rc;
}

GpuDevice::LockAllocFb::LockAllocFb(GpuDevice* pSubdev)
: m_Mutex(pSubdev->m_AllocFbMutex)
{
    Tasker::AcquireMutex(m_Mutex);
}

GpuDevice::LockAllocFb::~LockAllocFb()
{
    Tasker::ReleaseMutex(m_Mutex);
}

// This function completes the allocation of an RMclient/GpuDevice/
// GpuSubdevice. It allocates VaSpaces and some GpuDevice memory.
// For non-default RM clients in SMC mode, this function needs to be called
// after subscribing to a partition- else memory allocations will fail in RM.
RC GpuDevice::AllocSingleClientStuff(LwRm *pLwRm, GpuSubdevice *pSubdev)
{
    RC rc;
    LwRm *pLwRm0 = LwRmPtr(0).Get();

    // Allocate VA space for use on all classic GPUs
    m_Handles[pLwRm].hFermiVASpace = 0;
    if (pLwRm->IsClassSupported(FERMI_VASPACE_A, this))
    {
        LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
        CHECK_RC(pLwRm->Alloc(
                    pLwRm->GetDeviceHandle(this),
                    &m_Handles[pLwRm].hFermiVASpace,
                    FERMI_VASPACE_A,
                    &vaParams));
    }

    if (pSubdev->GpuGartSize())
    {
        CHECK_RC(pLwRm->AllocMemory(
            &m_Handles[pLwRm].hGpuGartMem,
            LW01_MEMORY_SYSTEM_DYNAMIC, 0,
            NULL, &m_GpuGartLimit, this));
    }

    if (pLwRm0 != pLwRm)
    {
        if (m_HeapSizeKb)
        {
            CHECK_RC(pLwRm->AllocMemory(
                    &m_Handles[pLwRm].hFrameBufferMem,
                    LW01_MEMORY_LOCAL_USER, DRF_DEF(OS02, _FLAGS, _ALLOC, _NONE),
                    NULL, &m_FrameBufferLimit, this));
        }
    }

    const bool cacheSnoop = pSubdev->HasFeature(GPUSUB_HAS_COHERENT_SYSMEM);

    if (pSubdev->GpuGartSize())
    {
        // One context DMA For the entire GPU virtual address space
        if (cacheSnoop)
        {
            // Try to create the coherent GART context DMA
            // Note that it is possible for us to be unable to create the
            // coherent GART context DMA.  The only type of system where this
            // should occur is an AGP GPU behind a BR02, such as LW40+BR02 OR on CheetAh (pre-T186).
            CHECK_RC(pLwRm->AllocContextDma(
                &m_Handles[pLwRm].hCohGartDmaCtx,
                DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE),
                m_Handles[pLwRm].hGpuGartMem,
                0, m_GpuGartLimit));
        }

        // Try to create the noncoherent GART context DMA
        // We don't bother checking the GPU GART flag for noncoherent.
        CHECK_RC(pLwRm->AllocContextDma(
            &m_Handles[pLwRm].hNcohGartDmaCtx,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE),
            m_Handles[pLwRm].hGpuGartMem,
            0, m_GpuGartLimit));
    }

    // Another context DMA for the entire framebuffer
    // We'd prefer to not have to create this one, but it is required
    // for display.  Display only supports physical context DMAs.  This
    // context DMA must never be used for anything other than display,
    // since the page kinds associated with this context DMA will be
    // wrong.
    if (m_HeapSizeKb)
    {
        CHECK_RC(pLwRm->AllocContextDma(
            &m_Handles[pLwRm].hFrameBufferDmaCtx,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            (cacheSnoop ? 0 : DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE)),
            m_Handles[pLwRm].hFrameBufferMem,
            0, m_FrameBufferLimit));
    }
    else
    {
        m_Handles[pLwRm].hFrameBufferDmaCtx =
            m_Handles[pLwRm].hNcohGartDmaCtx;
    }

    return rc;
}

// This function is used by Arch-MODS for allocating a new RM client/GpuDevice/
// GpuSubdevice for an SMC partition. AllocSingleClientStuff() needs to be
// called to finish the allocation for the new RM client and device. But that
// needs to happen after the RM client has subscribed to a partitions- since it
// has some memory allocations which would fail in RM without the subscription.
// Hence, it is not called in this function- rather the caller has the
// responsibility to make that call to AllocSingleClientStuff() after partition
// subscription.
RC GpuDevice::Alloc(LwRm *pLwRm)
{
    RC rc;
    GpuSubdevice *pSubdev = GetSubdevice(0);

    CHECK_RC(pLwRm->AllocRoot());

    // Allocate Device on the new RM client
    CHECK_RC(pLwRm->AllocDevice(m_Device));

    // Alloc Subdevice on the new RM client
    UINT32 subdevInst = pSubdev->GetSubdeviceInst();
    CHECK_RC(pLwRm->AllocSubDevice(m_Device, subdevInst));

    GpuDeviceHandles handles = { 0 };
    m_Handles[pLwRm] = handles;
    CHECK_RC(AllocFlaVaSpace(pLwRm));

    return rc;
}

void GpuDevice::FreeSingleClientStuff(LwRm *pLwRm)
{
    GpuDeviceHandles handles = { 0 };

    // Free the FB and GART memory
    pLwRm->Free(m_Handles[pLwRm].hFrameBufferMem);
    pLwRm->Free(m_Handles[pLwRm].hGpuGartMem);

    // Free VA spaces
    if (m_Handles[pLwRm].hFermiVASpace)
    {
        pLwRm->Free(m_Handles[pLwRm].hFermiVASpace);
    }
    if (m_Handles[pLwRm].hTegraVASpace)
    {
        pLwRm->Free(m_Handles[pLwRm].hTegraVASpace);
    }
    if (m_Handles[pLwRm].hGPUSMMUVASpace)
    {
        pLwRm->Free(m_Handles[pLwRm].hGPUSMMUVASpace);
    }

    m_Handles[pLwRm] = handles;
}

void GpuDevice::FreeFlaVaSpace(LwRm *pLwRm)
{
    if (m_Handles[pLwRm].hFermiFLAVASpace)
    {
        pLwRm->Free(m_Handles[pLwRm].hFermiFLAVASpace);
        m_Handles[pLwRm].hFermiFLAVASpace = 0;
    }
}

RC GpuDevice::Free(LwRm *pLwRm)
{
    RC rc;
    UINT32 subdevInst = GetSubdevice(0)->GetSubdeviceInst();

    FreeFlaVaSpace(pLwRm);
    FreeSingleClientStuff(pLwRm);
    pLwRm->FreeSubDevice(m_Device, subdevInst);
    pLwRm->FreeDevice(m_Device);
    CHECK_RC(pLwRm->FreeClient());
    m_Engines.erase(pLwRm);

    return rc;
}
