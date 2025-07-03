/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2015-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpumgr.h -- declares GpuDevMgr.

#pragma once

#ifndef INCLUDED_GPUMGR_H
#define INCLUDED_GPUMGR_H

#include "core/include/devmgr.h"
#include "gpudev.h"
#include "core/include/tee.h"
#include "gpusbdev.h"
#include "core/include/tasker.h"
#include "ctrl/ctrl0000.h"
#include <vector>

// Use this define when mapping peer gpus and you want RM to determine the peerId
#define USE_DEFAULT_RM_MAPPING  (UINT32)~0

typedef struct ModsGpuDevice_t ModsGpuDevice;
class EventThread;

class GpuDevMgr;

namespace DevMgrMgr
{
    extern GpuDevMgr* d_GraphDevMgr;
}

//! \brief Manages all GpuDevice and GpuSubdevices in the system.

class GpuDevMgr : public DevMgr
{
public:
    // GPA peer access: using peer aperture in GMMU page tables to reach a peer's memory.
    // SPA peer access: using ATS mappings to reach memory in peer GPU.
    enum PeerAccessType
    {
        GPA,
        SPA
    };

private:

    // PeerId must be part of the index because there are arch use cases where they allocate
    // multiple mappings between the same 2 subdevices using different explicit peer IDs
    struct PeerMappingIndex
    {
        PeerMappingIndex
        (
            LwRm *pLwRm,
            GpuSubdevice *pSubdev1,
            GpuSubdevice *pSubdev2,
            UINT32 peerId,
            PeerAccessType peerAccessType
        ) :
            m_pLwRm(pLwRm),
            m_PeerId(peerId),
            m_PeerAccessType(peerAccessType)
        {
            // always reference the lowest pSubdev as the 1st parameter to support bi-directional
            // mappings without using two entries.
            m_pSubdev1 = (pSubdev1 < pSubdev2) ? pSubdev1 : pSubdev2;
            m_pSubdev2 = (pSubdev1 < pSubdev2) ? pSubdev2 : pSubdev1;
        }

        bool operator<(const PeerMappingIndex& rhs) const;
        bool Contains(const GpuSubdevice* const pSubdevice) const;

        LwRm *m_pLwRm;
        GpuSubdevice *m_pSubdev1;
        GpuSubdevice *m_pSubdev2;
        UINT32 m_PeerId;
        PeerAccessType m_PeerAccessType;
    };
    struct PeerMapping
    {
        PeerMapping() :
            m_hP2P(0), m_PeerId1(USE_DEFAULT_RM_MAPPING), m_PeerId2(USE_DEFAULT_RM_MAPPING) {}
        PeerMapping(LwRm::Handle h, UINT32 p1, UINT32 p2) :
            m_hP2P(h), m_PeerId1(p1), m_PeerId2(p2) {}
        LwRm::Handle m_hP2P;
        UINT32       m_PeerId1;
        UINT32       m_PeerId2;
    };
    using PeerMappings = std::map<PeerMappingIndex, PeerMapping>;

  public:
    GpuDevMgr();
    ~GpuDevMgr();

    RC InitializeAllWithoutRm();
    RC InitializeAll();
    RC ShutdownAll();

    void OnExit();

    UINT32 NumDevices();
    UINT32 NumGpus();

    RC FindDevices();
    RC PreCreateGpuDeviceObjects();
    void AssignSubdeviceInstances();
    RC CreateGpuDeviceObjects();
    RC CreateGpuDeviceObjectsWithoutResman();
    RC AddGpuSubdevice(const PciDevInfo& pciDevInfo);
    void DestroySubdevicesInRm();
    void FreeDeviceObjects();
    void FreeDevices();

    RC HookInterrupts(bool pollInterrupts, UINT32 irqType);
    void UnhookInterrupts();
    RC ServiceInterrupt
    (
        UINT32 pciDomain,
        UINT32 pciBus,
        UINT32 pciDevice,
        UINT32 pciFunction,
        UINT32 irqNum
    );
    RC ServiceInterrupt(UINT32 irqNum);
    void Run1HzCallbacks();
    void PostRmShutdown();

    RC GetDevice(UINT32 DevInst, Device **Device);
    GpuDevice* GetPrimaryDevice();

    GpuDevice * GetFirstGpuDevice() { return GetNextGpuDevice(NULL); }
    GpuDevice * GetNextGpuDevice(GpuDevice * prev);

    GpuSubdevice * GetFirstGpu()    { return GetNextGpu(NULL); }
    GpuSubdevice * GetNextGpu(GpuSubdevice * prev);

    // This allows us to use d_GraphDevMgr in for-range loops to iterate
    // over GpuSubdevices
    class Iterator
    {
        public:
            Iterator()                           = default;

            // Copyable
            Iterator(const Iterator&)            = default;
            Iterator& operator=(const Iterator&) = default;

            explicit Iterator(GpuSubdevice* pSubdev)
                : m_pSubdev(pSubdev)
            {
            }

            GpuSubdevice* operator*() const
            {
                return m_pSubdev;
            }

            // Get next subdevice (pre-increment)
            Iterator& operator++()
            {
                MASSERT(DevMgrMgr::d_GraphDevMgr);
                m_pSubdev = DevMgrMgr::d_GraphDevMgr->GetNextGpu(m_pSubdev);
                return *this;
            }

            // Get next subdevice (post-increment)
            Iterator operator++(int)
            {
                MASSERT(DevMgrMgr::d_GraphDevMgr);
                Iterator prev(m_pSubdev);
                m_pSubdev = DevMgrMgr::d_GraphDevMgr->GetNextGpu(m_pSubdev);
                return prev;
            }

            bool operator==(const Iterator& other) const
            {
                return m_pSubdev == other.m_pSubdev;
            }

            bool operator!=(const Iterator& other) const
            {
                return m_pSubdev != other.m_pSubdev;
            }

        private:
            GpuSubdevice* m_pSubdev = nullptr;
    };

    Iterator begin()
    {
        return Iterator(GetFirstGpu());
    }

    static Iterator end()
    {
        return Iterator(nullptr);
    }

    GpuSubdevice *GetSubdeviceByGpuId(UINT32 GpuId);

    RC LinkGpus
    (
        UINT32 NumGpus, //!< Number of UINT32s in input and output arrays.
        UINT32 *pInsts, //!< Array of Gpu instances that we're requesting
                        //!< to link, which may be in the wrong order.
        UINT32 DispInst //!< Instance of the requested display Gpu.
    );
    RC UnlinkDevice(UINT32 DevInst);

    void UnlinkAll();

    void ListDevices(Tee::Priority pri);

    bool HasFoundDevices() const { return m_Found; }
    bool IsPreInitialized() const { return m_PreInitialized; }
    bool IsInitialized() const { return m_Initialized; }

    GpuSubdevice *GetSubdeviceByGpuInst(UINT32 GpuInstance) const;
    UINT32 GetDevInstByGpuInst(UINT32 gpuInst) const;

    RC AddPeerMapping(LwRm* pLwRm, GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2);
    // Use this API specifically for loopback P2P mapping.
    RC AddPeerMapping
    (
        LwRm* pLwRm,
        GpuSubdevice *pSubdev1,
        GpuSubdevice *pSubdev2,
        UINT32 peerIdBit
    );
    RC AddPeerMapping
    (
        LwRm* pLwRm,
        GpuSubdevice *pSubdev1,
        GpuSubdevice *pSubdev2,
        UINT32 peerIdBit,
        PeerAccessType peerAccessType
    );

    //! \brief Remove a peer mapping between two subdevices.
    //!
    //! \return OK if removing a mapping succeeds, not OK otherwise (inc. if not initialized)
    RC RemovePeerMapping(const PeerMappingIndex& peerMapping);
    RC RemovePeerMapping(PeerMappings::const_iterator& peerMappingIt);

    //! \brief Remove all peer mappings containing a given subdevice.
    //!
    //! \param pSubdev : Subdevice to remove mappings from
    //!
    //! \return OK if removing the mappings succeeds, not OK otherwise (inc. if not initialized)
    RC RemoveAllPeerMappings(GpuSubdevice *pSubdev);

    //! \brief Get the PeerId on pSubdev1 used to access pSubdev2
    RC GetPeerId
    (
        LwRm* pLwRm,
        GpuSubdevice *pSubdev1,
        GpuSubdevice *pSubdev2,
        PeerAccessType peerAccessType,
        UINT32 *pPeerId
    );

    SETGET_PROP(VasReverse, bool);
    SETGET_PROP(NumUserdAllocs, UINT32);
    SETGET_PROP_LWSTOM(UserdLocation, UINT32);
    enum class SubdevDiscoveryMode
    {
        Allocate,
        Reassign
    };
    RC FindDevicesClientSideResman(SubdevDiscoveryMode mode);

    void ShutdownDrivers();
  protected:
    RC GetGpuInfo(UINT32 GpuId, UINT32 *GpuInstance,
                  UINT32 *DeviceInstance, UINT32 *SubdeviceInstance);

  private:
    RC FindDevicesWithoutPci();
    RC TryInitializePciDevice
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        UINT32 classCode,
        SubdevDiscoveryMode mode
    );
    RC TryInitializeVfs(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function, UINT32 classCode);
    RC AddSubdevice(const PciDevInfo& pciDevInfo);

    bool m_PreInitialized;
    bool m_Initialized;
    bool m_Found;
    bool m_Exiting;
    bool m_AddingNewDevices;
    UINT32 m_IgnoredAndObsoleteDevices;
    bool m_VasReverse;
    UINT32 m_NumUserdAllocs;
    Memory::Location m_UserdLocation;

    GpuDevice *m_GpuDevices[LW0080_MAX_DEVICES];
    vector<GpuSubdevice*> m_GpuSubdevices;
    UINT32 m_GpuDevicesByGpuInst[LW0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    PeerMappings m_SubdevicePeerMappings;
    Tasker::Mutex m_PeerMapMutex = Tasker::NoMutex();
    Tasker::Mutex m_GpuInstMapMutex = Tasker::NoMutex();

    Tasker::ThreadID m_PollingInterruptThreadID;
    bool m_ContinuePollingInterruptThread;

    RC BuildGpuIdList
    (
        UINT32         NumGpus,       //!< Number of UINT32s in pInsts and pRetIds arrays.
        const UINT32 * pInsts,        //!< Array of Gpu Instance numbers to translate.
        UINT32         DispInst,      //!< Gpu Instance that is the desired display Gpu.
        UINT32 *       pRetIds,       //!< Output Gpu ID array.
        UINT32 *       pRetDispId     //!< Output Gpu ID that matches DispInst.
    );

    RC GetValidConfiguration
    (
        UINT32         NumGpus,      //!< Number of UINT32s in input and output arrays.
        const UINT32 * pIds,         //!< Array of Gpu IDs that we're requesting to
                                     //!<   link, which might be in the wrong order.
        UINT32         DispId,       //!< ID of the requested display Gpu.
        UINT32 *       pRetIds,      //!< Output array of Gpu IDs.  This contains the
                                     //!<   same IDs as pIDs but possibly reordered.
        UINT32 *       pRetDispIndex //!< Output index of DispId in pRetIds.
    );

    RC ValidateConfiguration
    (
        UINT32   NumGpus,   //!< Number of UINT32s in input and output arrays.
        UINT32 * pIds       //!< Array of Gpu IDs that we're requesting to
                            //!<   link, which might be in the wrong order.
    );
    void ListSLIConfigurations
    (
        Tee::Priority Pri,
        const vector<LW0000_CTRL_GPU_SLI_CONFIG> &SliConfigList,
        const vector<UINT32> &SliIlwalidReasonList
    );

    static void PollingInterruptThread(void *pGpuDevMgr);
};

#endif // INCLUDED_GPUMGR_H
