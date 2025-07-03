/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwdiagutils.h"
#include "inc/bytestream.h"
#include "core/include/rc.h"
#include "core/include/heartbeatmonitor.h"
#include "core/include/xp.h"
#include "core/include/cmdline.h"
#ifdef INCLUDE_LWSWITCH
    #include "../multinode/transport/inc/client.h"
    #include "../../fabricmanager/common/FMCommonTypes.h"
#endif

class ModsGdmClient
{
public:
#ifndef INCLUDE_LWSWITCH
    typedef struct PciInfo
    {
        UINT32 domain;
        UINT32 bus;
        UINT32 device;
        UINT32 function;
    }FMPciInfo_t;
#endif
    RC                    InitModsGdmClient(string& gdmAddress);
    void                  StopModsGdmClient();
    void                  SendMonitorHeartBeat();
    void                  UpdateGdmHeartBeat();
    void                  UpdateModsRegId(HeartBeatMonitor::MonitorHandle regId);
    void                  RegisterGdmHeartBeat(UINT64 GdmHeartBeatPeriodMs);
    void                  UnRegisterModsGdmClient();

    // Parse GPU Size information
    void SetModsNumGpus(UINT32 numGpus) { m_GfmNumGpus = numGpus; }
    RC GetGfmNumGpus(UINT32 *numGpu);

    // Parse LwSwitch Size information
    void SetModsNumLwSwitch(UINT32 numLwSwitch) { m_GfmNumLwSwitch = numLwSwitch; }
    RC GetGfmNumLwSwitch(UINT32 *numLwSwitch);

    // Parse Gpu Max LwLinks
    void SetModsGpuMaxLwLinks(UINT32 physicalId, UINT32 maxLink);
    RC GetGfmGpuMaxLwLinks(UINT32 physicalId,  UINT32 *maxLinks);

    //Parse GPU Physical ID based on Index
    void SetModsPhysicalID(UINT32 index, UINT32 physicalId, bool gpu);
    RC GetGfmGpuPhysicalId(UINT32 index, UINT32 *physicalId);
    RC GetGfmLwSwitchPhysicalId(UINT32 index, UINT32 *physicalId);

    // Parse Gpu Enum Index
    void SetModsGpuEnumIndex
    (

        UINT32 nodeID,
        UINT32 physicalId,
        UINT32 enumIndex,
        bool found,
        bool gpu
    );
    RC GetGfmGpuEnumIndex(UINT32 nodeID, UINT32 physicalId, UINT32 *enumIndex);
    RC GetGfmLwSwitchEnumIndex(UINT32 nodeID, UINT32 physicalId, UINT32 *enumIndex);

    // Parse Gpu Enum Index
    void SetModsGpuPciBdf
    (
        UINT32 nodeID,
        UINT32 enumIndex,
        FMPciInfo_t pciInfo,
        bool found,
        bool gpu
    );
    RC GetGfmGpuPciBdf(UINT32 nodeID, UINT32 enumIndex, FMPciInfo_t *pciInfo);
    RC GetGfmLwSwitchPciBdf(UINT32 nodeID, UINT32 physicalId, FMPciInfo_t *pciInfo);

    RC                    Run();
    RC                    RunOnce();
    static ModsGdmClient& Instance();

    Tasker::EventID       s_GdmShutDownAck;
    Tasker::EventID       s_ClientExitEvent;
    Tasker::EventID       s_GdmParsingEvent;
    Tasker::EventID       s_GdmSyncEvent;

private:
    ModsGdmClient();

    RC     Start(string& ipaddr, UINT32 port);
    RC     Connect();
    RC     Disconnect();
    RC     SendMessage(ByteStream &bs);
    RC     SendModsHeartBeat();
    bool   CheckGdmHeartBeat();
    bool   IsGdmEnabled();
    RC     RegisterModsGdm();
    RC     AllocGdmParsingEvent();
    void   FreeGdmParsingEvent();

    struct GdmHeartBeatContext
    {
        UINT64 HeartBeatPeriodMs;
        UINT64 HeartBeatPeriodExpirationTime;
    };

#ifdef INCLUDE_LWSWITCH
    unique_ptr<GdmClient>           m_pClient;
#endif
    typedef map<pair<UINT32, UINT32>, UINT32>      GfmGpuEnumIdxs;
    typedef map<pair<UINT32, UINT32>, FMPciInfo_t> GfmGpuPciBdfs;
    map<UINT32, UINT32>                   m_GfmGpuNumMaxLwLinks;
    map<UINT32, UINT32>                   m_GfmGpuPhyicalIDs;
    map<UINT32, UINT32>                   m_GfmLwSwitchPhyicalIDs;
    GfmGpuEnumIdxs                        m_GfmGpuEnumIdxsMap;
    GfmGpuEnumIdxs                        m_GfmLwSwitchEnumIdxsMap;
    GfmGpuPciBdfs                         m_GfmGpuPciBdfMap;
    GfmGpuPciBdfs                         m_GfmLwSwitchPciBdfMap;
    Tasker::Mutex                   m_ModsGdmClientMutex            = Tasker::NoMutex();
    HeartBeatMonitor::MonitorHandle m_ModsGdmRegId                  = 0;
    GdmHeartBeatContext             m_GdmHbContext;
    Tasker::ThreadID                m_ClientGdmThread               = Tasker::NULL_THREAD;
    bool                            m_IsGdmEnabled                  = false;
    UINT32                          m_GfmNumGpus                    = 0;
    UINT32                          m_GfmNumLwSwitch                = 0;
};
