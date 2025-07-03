/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "g_lwconfig.h"
#include "rc.h"
#include "utility.h"

#include <string>
#include <set>

#define FEATURE_TYPE_MASK       0x70000
#define GPUSUB_FEATURE_TYPE     0x00000
#define GPUDEV_FEATURE_TYPE     0x10000
#define MCP_FEATURE_TYPE        0x20000
#define SOC_FEATURE_TYPE        0x40000
#define IS_GPUSUB_FEATURE(feature)                          \
    ((feature & FEATURE_TYPE_MASK) == GPUSUB_FEATURE_TYPE)
#define IS_GPUDEV_FEATURE(feature)                          \
    ((feature & FEATURE_TYPE_MASK) == GPUDEV_FEATURE_TYPE)
#define IS_MCP_FEATURE(feature)                             \
    ((feature & FEATURE_TYPE_MASK) == MCP_FEATURE_TYPE)
#define IS_SOC_FEATURE(feature)                             \
    ((feature & FEATURE_TYPE_MASK) == SOC_FEATURE_TYPE)

/* This it the device base class.  All devices handled by the device
 * manager should inherit from this base.  Individual ModsTests will
 * be bound to specific devices. */

class Device
{
public:

    typedef UINT32 LibDeviceHandle;
    static constexpr UINT32 ILWALID_LIB_HANDLE = ~0U;

    //! Type of device
    enum Type
    {
        TYPE_LWIDIA_GPU
       ,TYPE_TEGRA_MFG
       ,TYPE_IBM_NPU
       ,TYPE_EBRIDGE
       ,TYPE_LWIDIA_LWSWITCH
       ,TYPE_TREX
       ,TYPE_UNKNOWN
    };

    //! Structure describing the ID of a device.
    //!
    struct Id
    {
        enum
        {
            SIMULATED_EBRIDGE_PCI_BUS  = 0xE  //!< Special bus number for sim ebridge devices
           ,SIMULATED_LWSWITCH_PCI_BUS = 0xD  //!< Special bus number for sim lwswitch devices
           ,SIMULATED_GPU_PCI_DOMAIN   = 0xC  //!< Special domain number for sim gpu devices
           ,SIMULATED_GPU_PCI_BUS      = 0xC  //!< Special bus number for sim gpu devices
           ,SIMULATED_NPU_PCI_DOMAIN   = 0x10 //!< Special domain number for sim NPU devices
           ,TREX_PCI_BUS               = 0xB  //!< Special bus number fo trex devices
        };

        UINT64 pciDBDF;      //!< PCI domain/bus/dev/func of the device

        Id() : pciDBDF(~0ULL) { }
        Id(UINT16 domain, UINT16 bus, UINT16 device, UINT16 function)
        {
            SetPciDBDF(domain, bus, device, function);
        }

        //! Sort the device by GPU ID (therefore GPU devices are first) then by
        //! vendor and then by device.  Necessary in order to use Id in STL
        //! classes
        bool operator<(const Id &rhs) const
        {
            return pciDBDF < rhs.pciDBDF;
        }
        bool operator>(const Id &rhs) const
        {
            return pciDBDF > rhs.pciDBDF;
        }

        //! Equality test to simplify code
        bool operator==(const Id &rhs) const
        {
            return (pciDBDF == rhs.pciDBDF);
        }
        bool operator!=(const Id &rhs) const
        {
            return (pciDBDF != rhs.pciDBDF);
        }

        void SetPciDBDF(UINT16 domain = 0,
                        UINT16 bus    = 0,
                        UINT16 dev    = 0,
                        UINT16 func   = 0)
        {
            pciDBDF = (static_cast<UINT64>(domain) << 48) |
                      (static_cast<UINT64>(bus)    << 32) |
                      (static_cast<UINT64>(dev)    << 16) |
                      (static_cast<UINT64>(func));
        }

        void GetPciDBDF(UINT16* domain = nullptr,
                        UINT16* bus    = nullptr,
                        UINT16* dev    = nullptr,
                        UINT16* func   = nullptr) const
        {
            if (domain)
                *domain = (pciDBDF >> 48) & 0xFFFF;
            if (bus)
                *bus = (pciDBDF >> 32) & 0xFFFF;
            if (dev)
                *dev = (pciDBDF >> 16) & 0xFFFF;
            if (func)
                *func = pciDBDF & 0xFFFF;
        }

        string GetString() const
        {
            return Utility::StrPrintf("%04x:%02x:%02x.%x",
                                      static_cast<UINT32>((pciDBDF >> 48) & 0xFFFF),
                                      static_cast<UINT32>((pciDBDF >> 32) & 0xFFFF),
                                      static_cast<UINT32>((pciDBDF >> 16) & 0xFFFF),
                                      static_cast<UINT32>( pciDBDF        & 0xFFFF));
        }
    };

    Device();
    virtual ~Device();

    virtual RC Initialize() = 0;
    virtual RC Shutdown() = 0;

    virtual string GetName() const { return m_Name; }

    virtual bool HasBug(UINT32 bugNum) const;
    virtual void SetHasBug(UINT32 bugNum);
    virtual void ClearHasBug(UINT32 bugNum);
    virtual void PrintBugs();

    enum LwDeviceId
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_X86_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_ARM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_SIM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
#undef DEFINE_X86_GPU
#undef DEFINE_ARM_GPU
#undef DEFINE_SIM_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, Constant, ...) \
        LwId = Constant,
#define DEFINE_ARM_MFG_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, Constant, ...) \
        LwId = Constant,
#define DEFINE_SIM_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, Constant, ...) \
        LwId = Constant,
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "lwlinklist.h"
#undef DEFINE_LWL_DEV
#undef DEFINE_ARM_MFG_LWL_DEV
#undef DEFINE_SIM_LWL_DEV
#undef DEFINE_OBS_LWL_DEV

        ILWALID_DEVICE = 0xFFFF
    };

    static string DeviceIdToString(LwDeviceId id);
    static LwDeviceId StringToDeviceId(const string& s);

    enum Feature
    {
#define DEFINE_GPUSUB_FEATURE(feat, featid, desc)  \
        feat = GPUSUB_FEATURE_TYPE | featid,
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc)  \
        feat = GPUDEV_FEATURE_TYPE | featid,
#define DEFINE_MCP_FEATURE(feat, featid, desc)  \
        feat = MCP_FEATURE_TYPE | featid,
#define DEFINE_SOC_FEATURE(feat, featid, desc)  \
        feat = SOC_FEATURE_TYPE | featid,

#include "featlist.h"

#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE

        FEATURE_COUNT
    };

    virtual bool HasFeature(Feature feature) const;
    virtual void SetHasFeature(Feature feature);
    virtual void ClearHasFeature(Feature feature);

    void PrintFeatures();

    //! Perform an escape read/write into the simulator
    virtual int EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    virtual int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);

protected:
    void SetName(string name){ m_Name = name; }

private:
    static string FeatureString(Feature feature);
    static void ValidateFeatures();

    string m_Name;
    set<UINT32> m_BugSet;
    set<UINT32> m_FeatureSet;

    struct FeatureStr
    {
        Feature         feature;
        string          featureStr;
    };
    static struct FeatureStr s_FeatureStrings[];
    static bool s_FeaturesValid;
};

