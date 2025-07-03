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

#include "lwl_fabricmanager_libif.h"

#include <memory>

class GlobalFabricManager;
class LocalFabricManagerControl;
class GlobalFMLWLinkConnRepo;

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief FM core library interface definition for interacting with the
    //! fabricmanager core library when it is compiled directly into MODS
    class FMLibIfUser : public FMLibInterface
    {
    public:
        FMLibIfUser() {}
        virtual ~FMLibIfUser() { }
    protected:
        RC InitializeGlobalFM() override;
        RC InitializeLocalFM() override;
        bool IsInitialized() override { return m_bInitialized; }
        RC Shutdown() override;

        RC InitializeAllLwLinks() override;
        RC DiscoverAllLwLinks() override;
        RC TrainLwLinkConns(LwLinkTrainType type) override;
        RC ValidateLwLinksTrained(bool *pbAllLinksTrained) override;
        RC ReinitFailedLinks() override;
        RC FinishInitialize() override;

        UINT32 GetNumGpus() const override;
        UINT32 GetGpuMaxLwLinks(UINT32 physicalId) const override;
        RC GetGpuPhysicalId(UINT32 index, UINT32* pId) const override;
        RC GetGpuId(UINT32 physicalId, TestDevice::Id* pPhysicalId) const override;
        UINT32 GetNumLwSwitches() const override;
        RC GetLwSwitchPhysicalId(UINT32 index, UINT32* pId) const override;
        RC GetLwSwitchId(UINT32 physicalId, TestDevice::Id* pPhysicalId) const override;

        RC GetFabricAddressBase(UINT32 physicalId, UINT64* pBase) const override;
        RC GetFabricAddressRange(UINT32 physicalId, UINT64* pRange) const override;
        RC GetGpaAddressBase(UINT32 physicalId, UINT64* pBase) const override;
        RC GetGpaAddressRange(UINT32 physicalId, UINT64* pRange) const override;
        RC GetFlaAddressBase(UINT32 physicalId, UINT64* pBase) const override;
        RC GetFlaAddressRange(UINT32 physicalId, UINT64* pRange) const override;

        RC GetRequestorLinkId(const PortInfo& portInfo, UINT32* pLinkId) override;
        RC GetRemotePortInfo(const PortInfo& portInfo, PortInfo* pRemInfo) override;

        RC AllocMulticastGroup(UINT32* pGroupId) override;
        RC FreeMulticastGroup(UINT32 groupId) override;
        RC SetMulticastGroup
        (
            UINT32 groupId,
            bool reflectiveMode,
            bool excludeSelf,
            const set<TestDevicePtr>& gpus
        ) override;
        RC GetMulticastGroupBaseAddress(UINT32 groupId, UINT64* pBase) override;

        RC GetIngressRequestPorts(const PortInfo& portInfo, UINT32 fabricIndex, set<UINT32>* pPorts) override;
        RC GetIngressResponsePorts(const PortInfo& portInfo, UINT64 fabricBase, set<UINT32>* pPorts) override;
        RC GetRidPorts(const PortInfo& portInfo, UINT32 targetId, set<UINT32>* pPorts) override;

    private:
        unique_ptr<GlobalFabricManager>        m_pGFM;
        unique_ptr<LocalFabricManagerControl>  m_pLFM;
        unique_ptr<GlobalFMLWLinkConnRepo>     m_pFailedLinks;
        bool m_bInitialized = false;
        bool m_bInitializedGFM = false;
    };
};
