/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "gpu/include/testdevice.h"

#include <memory>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief FM library interface definition for interacting with the
    //! fabricmanager core library. There will be multiple implementations of this
    //! depending user vs. kernel mode
    class FMLibInterface
    {
        public:
            enum LwLinkTrainType
            {
                LWLINK_TRAIN_OFF_TO_SAFE,
                LWLINK_TRAIN_SAFE_TO_HIGH,
                LWLINK_TRAIN_TO_OFF,
                LWLINK_TRAIN_HIGH_TO_SAFE,
                LWLINK_TRAIN_SAFE_TO_OFF
            };

            struct PortInfo
            {
                TestDevice::Type type;
                UINT32 nodeId;
                UINT32 physicalId;
                UINT32 portNum;
            };

            virtual ~FMLibInterface() = default;
            virtual RC InitializeGlobalFM() = 0;
            virtual RC InitializeLocalFM() = 0;
            virtual bool IsInitialized() = 0;
            virtual RC Shutdown() = 0;

            virtual RC InitializeAllLwLinks() = 0;
            virtual RC DiscoverAllLwLinks() = 0;
            virtual RC TrainLwLinkConns(LwLinkTrainType type) = 0;
            virtual RC ValidateLwLinksTrained(bool *pbAllLinksTrained) = 0;;
            virtual RC ReinitFailedLinks() = 0;
            virtual RC FinishInitialize() = 0;

            virtual UINT32 GetNumGpus() const = 0;
            virtual UINT32 GetGpuMaxLwLinks(UINT32 physicalId) const = 0;
            virtual RC GetGpuPhysicalId(UINT32 index, UINT32* pId) const = 0;
            virtual RC GetGpuId(UINT32 physicalId, TestDevice::Id* pPhysicalId) const = 0;
            virtual UINT32 GetNumLwSwitches() const = 0;
            virtual RC GetLwSwitchPhysicalId(UINT32 index, UINT32* pPhysicalId) const = 0;
            virtual RC GetLwSwitchId(UINT32 physicalId, TestDevice::Id* pId) const = 0;

            virtual RC GetFabricAddressBase(UINT32 physicalId, UINT64* pBase) const = 0;
            virtual RC GetFabricAddressRange(UINT32 physicalId, UINT64* pRange) const = 0;
            virtual RC GetGpaAddressBase(UINT32 physicalId, UINT64* pBase) const = 0;
            virtual RC GetGpaAddressRange(UINT32 physicalId, UINT64* pRange) const = 0;
            virtual RC GetFlaAddressBase(UINT32 physicalId, UINT64* pBase) const  = 0;
            virtual RC GetFlaAddressRange(UINT32 physicalId, UINT64* pRange) const  = 0;

            virtual RC GetRequestorLinkId(const PortInfo& portInfo, UINT32* pLinkId) = 0;
            virtual RC GetRemotePortInfo(const PortInfo& portInfo, PortInfo* pRemInfo) = 0;
            virtual RC AllocMulticastGroup(UINT32* pGroupId) = 0;
            virtual RC FreeMulticastGroup(UINT32 groupId) = 0;
            virtual RC SetMulticastGroup
            (
                UINT32 groupId,
                bool reflectiveMode,
                bool excludeSelf,
                const set<TestDevicePtr>& gpus
            ) = 0;
            virtual RC GetMulticastGroupBaseAddress(UINT32 groupId, UINT64* pBase) = 0;

            virtual RC GetIngressRequestPorts(const PortInfo& portInfo, UINT32 fabricIndex, set<UINT32>* pPorts) = 0;
            virtual RC GetIngressResponsePorts(const PortInfo& portInfo, UINT64 fabricBase, set<UINT32>* pPorts) = 0;
            virtual RC GetRidPorts(const PortInfo& portInfo, UINT32 fabricIndex, set<UINT32>* pPorts) = 0;
    };

    typedef shared_ptr<FMLibInterface> FMLibIfPtr;
};
