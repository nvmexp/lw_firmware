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

#include "../multinode/transport/inc/connection.h"
#include "inc/bytestream.h"
#include "core/include/log.h"
#include "core/include/mods_gdm_client.h"

#include "protobuf/pbwriter.h"
#include "gdm_message_structs.h"
#include "gdm_message_handler.h"
#include "gdm_message_writer.h"
#include "gdm_message_reader.h"

#include <memory>

namespace
{
    //------------------------------------------------------------------------------
    LwDiagUtils::EC HandleMessage(Connection * pConnection, const ByteStream & message)
    {
        return GdmMessageHandler::HandleMessages(message, pConnection);
    }

    //------------------------------------------------------------------------------
    LwDiagUtils::EC HandleEvent(Connection * pConnection, UINT32 eventTypes)
    {
        if ((eventTypes & TransportCommon::EventType::EXITING) ||
            (eventTypes & TransportCommon::EventType::ERROR))
        {
            Printf(Tee::PriNormal,
                   "Client %s exited%s\n",
                   pConnection->GetConnectionString().c_str(),
                   (eventTypes & TransportCommon::EventType::ERROR) ? " unexpectedly" : "");
        }
        return LwDiagUtils::EC::OK;
    }
}

ModsGdmClient::ModsGdmClient()
{
    m_ModsGdmClientMutex = Tasker::CreateMutex("mods client for gdm mutex", Tasker::mtxUnchecked);
}

ModsGdmClient& ModsGdmClient::Instance()
{
    static ModsGdmClient modsGdmClientInstance;
    return modsGdmClientInstance;
}

//Tasker::ThreadID s_ClientGdmThread = Tasker::NULL_THREAD;
//------------------------------------------------------------------------------
RC ModsGdmClient::Start(string& ipaddr, UINT32 port)
{
    // Create the server and start it running in a thread
    m_pClient.reset(GdmClient::CreateLibEventClient(ipaddr,
                                                 port,
                                                 10,
                                                 HandleMessage,
                                                 HandleEvent));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC ModsGdmClient::RunOnce()
{
    LWDASSERT(m_pClient);
    return Utility::InterpretModsUtilErrorCode(m_pClient->RunOnce());
}

RC ModsGdmClient::Run()
{
    LWDASSERT(m_pClient);
    return Utility::InterpretModsUtilErrorCode(m_pClient->Run());
}

//------------------------------------------------------------------------------
RC ModsGdmClient::Disconnect()
{
    ByteStream bs;
    auto sd = GdmMessageWriter::Messages::shutdown(&bs);
    {
        sd
        .header()
        .node_id(Xp::GetPid());
    }
    sd.status(0U);
    sd.Finish();

    LwDiagUtils::EC ec = LwDiagUtils::OK;
    FIRST_EC(m_pClient->SendMessage(bs));
    if (Tasker::WaitOnEvent(s_GdmShutDownAck, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriWarn, "Failed to recieve shutdown ack from GDM, disconnecting\n");
    }
    FIRST_EC(m_pClient->Disconnect());

    return Utility::InterpretModsUtilErrorCode(ec);
}

//------------------------------------------------------------------------------
RC ModsGdmClient::SendMessage(ByteStream &bs)
{
    return Utility::InterpretModsUtilErrorCode(m_pClient->SendMessage(bs));
}

//------------------------------------------------------------------------------
RC ModsGdmClient::Connect()
{
    return Utility::InterpretModsUtilErrorCode(m_pClient->Connect());
}

static void ClientGdmThread(void*)
{
    Tasker::DetachThread detach;
    while (Tasker::WaitOnEvent(ModsGdmClient::Instance().s_ClientExitEvent, 5) == RC::TIMEOUT_ERROR)
    {
        ModsGdmClient::Instance().RunOnce();
    }
}

void ModsGdmClient::UpdateGdmHeartBeat()
{
    Tasker::MutexHolder lock(m_ModsGdmClientMutex);
    const UINT64 lwrrentTime = Xp::GetWallTimeMS();
    const UINT64 expirationTime = m_GdmHbContext.HeartBeatPeriodMs + lwrrentTime;
    m_GdmHbContext.HeartBeatPeriodExpirationTime = expirationTime;
}

void ModsGdmClient::RegisterGdmHeartBeat(UINT64 gdmHeartBeatPeriodMs)
{
    Tasker::MutexHolder lock(m_ModsGdmClientMutex);
    const UINT64 lwrrentTime = Xp::GetWallTimeMS();
    const UINT64 expirationTime = gdmHeartBeatPeriodMs + lwrrentTime;
    m_GdmHbContext.HeartBeatPeriodMs = gdmHeartBeatPeriodMs;
    m_GdmHbContext.HeartBeatPeriodExpirationTime = expirationTime;
}

void ModsGdmClient::UpdateModsRegId(HeartBeatMonitor::MonitorHandle regId)
{
    Tasker::MutexHolder lock(m_ModsGdmClientMutex);
    m_ModsGdmRegId = regId;
}

RC ModsGdmClient::RegisterModsGdm()
{
    ByteStream bs;
    auto hb = GdmMessageWriter::Messages::register_app(&bs);
    {
        hb
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    /* Adding a constant value here, it should come from config json file */
    hb.heart_beat_period(10);
    hb.Finish();
    ModsGdmClient::SendMessage(bs);
    return RC::OK;
}

RC ModsGdmClient::SendModsHeartBeat()
{
    ByteStream bs;
    auto hb = GdmMessageWriter::Messages::heartbeat(&bs);
    {
        hb
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    hb.hb_reg_id(m_ModsGdmRegId);
    hb.Finish();
    ModsGdmClient::SendMessage(bs);
    return RC::OK;
}

RC ModsGdmClient::InitModsGdmClient(string& gdmAddress)
{
    RC rc;
    string delimiter = ":";
    string gdmIp   = gdmAddress.substr(0, gdmAddress.find(delimiter));
    string gdmPortString = gdmAddress.substr
                           (
                            gdmAddress.find(delimiter) + 1, gdmAddress.size() - 1 - gdmAddress.size()
                           ).c_str();
    UINT32 gdmPort = 0;
    CHECK_RC(Utility::StringToUINT32(gdmPortString, &gdmPort, 10));
    if (gdmIp.empty() || gdmPort == 0 || gdmPort > 65535)
    {
        Printf(Tee::PriError, "Unable to create gdm client thread, invalid port/ip input\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    ModsGdmClient::Start(gdmIp, gdmPort);
    ModsGdmClient::Connect();
    DEFER
    {
        if (!ModsGdmClient::Instance().m_IsGdmEnabled)
        {
            if (s_GdmShutDownAck)
            {
                Tasker::FreeEvent(s_GdmShutDownAck);
                s_GdmShutDownAck = nullptr;
            }
            if (s_ClientExitEvent)
            {
                Tasker::FreeEvent(s_ClientExitEvent);
                s_ClientExitEvent = nullptr;
            }
            ModsGdmClient::Disconnect();
        }
    };
    s_GdmShutDownAck = Tasker::AllocEvent("GdmShutDownAck");
    if (s_GdmShutDownAck == nullptr)
    {
        Printf(Tee::PriError, "Unable to allocate gdm shutdown ack event\n");
        return RC::LWRM_OPERATING_SYSTEM;
    }
    s_ClientExitEvent = Tasker::AllocEvent("ClientExitEvent");
    if (s_ClientExitEvent == nullptr)
    {
        Printf(Tee::PriError, "Unable to allocate client exit event\n");
        return RC::LWRM_OPERATING_SYSTEM;
    }
    m_ClientGdmThread = Tasker::CreateThread(ClientGdmThread, 0, 0, "ClientGdmThread");
    if (m_ClientGdmThread == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriError, "Unable to create gdm client thread\n");
        return RC::LWRM_OPERATING_SYSTEM;
    }
    ModsGdmClient::RegisterModsGdm();
    ModsGdmClient::Instance().m_IsGdmEnabled = true;
    return RC::OK;
}

void ModsGdmClient::UnRegisterModsGdmClient()
{
    ByteStream bs;
    auto ur = GdmMessageWriter::Messages::unregister_app(&bs);
    {
        ur
        .header()
        .node_id(Xp::GetPid());
    }
    ur.hb_reg_id(m_ModsGdmRegId);
    ur.Finish();
    ModsGdmClient::SendMessage(bs);
}

void ModsGdmClient::StopModsGdmClient()
{
    if (IsGdmEnabled())
    {
        ModsGdmClient::UnRegisterModsGdmClient();
        ModsGdmClient::Disconnect();
        Tasker::SetEvent(ModsGdmClient::Instance().s_ClientExitEvent);
        Tasker::Join(m_ClientGdmThread);
        m_ClientGdmThread = Tasker::NULL_THREAD;
        Tasker::FreeEvent(s_GdmShutDownAck);
        s_GdmShutDownAck = nullptr;
        Tasker::FreeEvent(s_ClientExitEvent);
        s_ClientExitEvent = nullptr;
    }
}

void ModsGdmClient::SendMonitorHeartBeat()
{
    if (IsGdmEnabled())
    {
        SendModsHeartBeat();
        if (!CheckGdmHeartBeat())
        {
            Log::SetFirstError(ErrorMap(RC::TIMEOUT_ERROR));
            Log::SetNext(false);
            JavaScriptPtr()->CallMethod(JavaScriptPtr()->GetGlobalObject(),
                    "PrintErrorWrapperPostTeardown");
            Tee::FlushToDisk();
            Utility::ExitMods(RC::TIMEOUT_ERROR, Utility::ExitQuickAndDirty);
        }
    }
}

bool ModsGdmClient::CheckGdmHeartBeat()
{
    Tasker::MutexHolder lock(m_ModsGdmClientMutex);
    const UINT64 lwrrentTime = Xp::GetWallTimeMS();
    const UINT64 expirationTime = m_GdmHbContext.HeartBeatPeriodExpirationTime;
    if (expirationTime < lwrrentTime)
    {
        Printf(Tee::PriError,
                "Missing heartbeat from GDM, last update time %llu"
                ",current time %llu, expected update interval %llu, exiting\n",
                (expirationTime - m_GdmHbContext.HeartBeatPeriodMs),
                lwrrentTime, m_GdmHbContext.HeartBeatPeriodMs
              );
        return false;
    }
    return true;
}

bool ModsGdmClient::IsGdmEnabled()
{
    return ModsGdmClient::Instance().m_IsGdmEnabled;
}

RC ModsGdmClient::GetGfmNumGpus(UINT32 *numGpu)
{
    // Verified
    RC rc;
    // Check if GPU Size is already parsed
    if (m_GfmNumGpus != 0)
    {
        *numGpu = m_GfmNumGpus;
        return RC::OK;
    }

    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_num_gpu(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Failed to recieve GPU Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }
    *numGpu = m_GfmNumGpus;

    return RC::OK;
}

RC ModsGdmClient::GetGfmNumLwSwitch(UINT32 *numLwSwitch)
{
    RC rc;
    // Check if LwSwitch Size is already parsed
    if (m_GfmNumLwSwitch != 0)
    {
        *numLwSwitch = m_GfmNumLwSwitch;
        return RC::OK;
    }

    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_num_lw_switch(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Failed to recieve LWSwitch Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }
    *numLwSwitch = m_GfmNumLwSwitch;
    return RC::OK;
}

void ModsGdmClient::SetModsGpuMaxLwLinks(UINT32 physicalId, UINT32 maxLink)
{
    m_GfmGpuNumMaxLwLinks.insert(pair<int, int>(physicalId, maxLink));
}

RC ModsGdmClient::GetGfmGpuMaxLwLinks(UINT32 physicalId, UINT32 *maxLinks)
{
    RC rc;
    auto it = m_GfmGpuNumMaxLwLinks.find(physicalId);
    if (it != m_GfmGpuNumMaxLwLinks.end())
    {
        *maxLinks = it->second;
        return RC::OK;
    }

    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_max_lwlinks(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.physical_id(physicalId);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Failed to recieve GPU LwLink info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }
    it = m_GfmGpuNumMaxLwLinks.find(physicalId);
    if (it == m_GfmGpuNumMaxLwLinks.end())
    {
        return RC::SOFTWARE_ERROR;
    }

    *maxLinks = it->second;
    return RC::OK;

}

void ModsGdmClient::SetModsPhysicalID(UINT32 index, UINT32 physicalId, bool gpu)
{
    if (gpu)
    {
        m_GfmGpuPhyicalIDs.insert(pair<int, int>(index, physicalId));
    }
    else
    {
        m_GfmLwSwitchPhyicalIDs.insert(pair<int, int>(index, physicalId));
    }
}

RC ModsGdmClient::GetGfmGpuPhysicalId(UINT32 index, UINT32 *physicalId)
{
    RC rc;

    auto it = m_GfmGpuPhyicalIDs.find(index);
    if (it != m_GfmGpuPhyicalIDs.end())
    {
        *physicalId = it->second;
        return RC::OK;
    }

    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_physical_id(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.index(index);
    msg.gpu(true);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Failed to recieve GPU Physical ID info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }
    it = m_GfmGpuPhyicalIDs.find(index);
    if (it == m_GfmGpuPhyicalIDs.end())
    {
        return RC::SOFTWARE_ERROR;
    }

    *physicalId = it->second;
    return RC::OK;
}

RC ModsGdmClient::GetGfmLwSwitchPhysicalId(UINT32 index, UINT32 *physicalId)
{
    RC rc;

    auto it = m_GfmLwSwitchPhyicalIDs.find(index);
    if (it != m_GfmLwSwitchPhyicalIDs.end())
    {
        *physicalId = it->second;
        return RC::OK;
    }

    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_physical_id(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.index(index);
    msg.gpu(false);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Failed to recieve GPU Physical ID info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }
    it = m_GfmLwSwitchPhyicalIDs.find(index);
    if (it == m_GfmLwSwitchPhyicalIDs.end())
    {
        return RC::SOFTWARE_ERROR;
    }

    *physicalId = it->second;
    return RC::OK;
}

void ModsGdmClient::SetModsGpuEnumIndex
(

    UINT32 nodeID,
    UINT32 physicalId,
    UINT32 enumIndex,
    bool found,
    bool gpu
)
{
    if (found)
    {
        if (gpu)
        {
            m_GfmGpuEnumIdxsMap.insert({{nodeID, physicalId}, enumIndex});
        }
        else
        {
            m_GfmLwSwitchEnumIdxsMap.insert({{nodeID, physicalId}, enumIndex});
        }
    }
}

RC ModsGdmClient::GetGfmGpuEnumIndex(UINT32 nodeID, UINT32 physicalId, UINT32 *enumIndex)
{

    auto it = m_GfmGpuEnumIdxsMap.find(make_pair(nodeID, physicalId));
    if (it != m_GfmGpuEnumIdxsMap.end())
    {
        *enumIndex = it->second;
    }

    RC rc;
    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_gpu_enum_idx(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.node_id(nodeID);
    msg.physical_id(physicalId);
    msg.gpu(true);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriWarn, "Failed to recieve GPU Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }

    it = m_GfmGpuEnumIdxsMap.find(make_pair(nodeID, physicalId));
    if (it == m_GfmGpuEnumIdxsMap.end())
    {
        return RC::DEVICE_NOT_FOUND;
    }
    *enumIndex = it->second;
    return RC::OK;
}

RC ModsGdmClient::GetGfmLwSwitchEnumIndex(UINT32 nodeID, UINT32 physicalId, UINT32 *enumIndex)
{

    auto it = m_GfmLwSwitchEnumIdxsMap.find(make_pair(nodeID, physicalId));
    if (it != m_GfmLwSwitchEnumIdxsMap.end())
    {
        *enumIndex = it->second;
    }

    RC rc;
    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_gpu_enum_idx(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.node_id(nodeID);
    msg.physical_id(physicalId);
    msg.gpu(false);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriWarn, "Failed to recieve GPU Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }

    it = m_GfmLwSwitchEnumIdxsMap.find(make_pair(nodeID, physicalId));
    if (it == m_GfmLwSwitchEnumIdxsMap.end())
    {
        return RC::DEVICE_NOT_FOUND;
    }
    *enumIndex = it->second;
    return RC::OK;
}

void ModsGdmClient::SetModsGpuPciBdf
(
    UINT32 nodeID,
    UINT32 enumIndex,
    FMPciInfo_t pciInfo,
    bool found,
    bool gpu
)
{
    if (found)
    {
        if (gpu)
        {
            m_GfmGpuPciBdfMap.insert({{nodeID, enumIndex}, pciInfo});
        }
        else
        {
            m_GfmLwSwitchPciBdfMap.insert({{nodeID, enumIndex}, pciInfo});
        }
    }
}

RC ModsGdmClient::GetGfmGpuPciBdf(UINT32 nodeID, UINT32 enumIndex, FMPciInfo_t *pciInfo)
{

    auto it = m_GfmGpuPciBdfMap.find(make_pair(nodeID, enumIndex));
    if (it != m_GfmGpuPciBdfMap.end())
    {
        *pciInfo = it->second;
    }

    RC rc;
    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_pci_bdf(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.node_id(nodeID);
    msg.enum_idx(enumIndex);
    msg.gpu(true);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriWarn, "Failed to recieve GPU Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }

    it = m_GfmGpuPciBdfMap.find(make_pair(nodeID, enumIndex));
    if (it == m_GfmGpuPciBdfMap.end())
    {
        return RC::DEVICE_NOT_FOUND;
    }
    *pciInfo = it->second;
    return RC::OK;
}

RC ModsGdmClient::GetGfmLwSwitchPciBdf(UINT32 nodeID, UINT32 enumIndex, FMPciInfo_t *pciInfo)
{

    auto it = m_GfmLwSwitchPciBdfMap.find(make_pair(nodeID, enumIndex));
    if (it != m_GfmLwSwitchPciBdfMap.end())
    {
        *pciInfo = it->second;
    }

    RC rc;
    DEFER
    {
        FreeGdmParsingEvent();

    };
    CHECK_RC(AllocGdmParsingEvent());

    ByteStream bs;
    auto msg = GdmMessageWriter::Messages::get_gfm_pci_bdf(&bs);
    {
        msg
        .header()
        .node_id(Xp::GetPid())
        .app_type(GdmMessageWriter::MessageHeader::AppType::mods);
    }
    msg.node_id(nodeID);
    msg.enum_idx(enumIndex);
    msg.gpu(false);
    msg.Finish();
    ModsGdmClient::SendMessage(bs);
    if (Tasker::WaitOnEvent(s_GdmParsingEvent, 2000) == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriWarn, "Failed to recieve GPU Parsing info from  GDM\n");
        return RC::TIMEOUT_ERROR;
    }

    it = m_GfmLwSwitchPciBdfMap.find(make_pair(nodeID, enumIndex));
    if (it == m_GfmLwSwitchPciBdfMap.end())
    {
        return RC::DEVICE_NOT_FOUND;
    }
    *pciInfo = it->second;
    return RC::OK;
}

RC ModsGdmClient::AllocGdmParsingEvent()
{
    if (s_GdmParsingEvent == nullptr)
    {
        s_GdmParsingEvent = Tasker::AllocEvent("GdmParsingEvent");
        if (s_GdmParsingEvent == nullptr)
        {
            Printf(Tee::PriError, "Unable to allocate gdm parsing event\n");
            return RC::LWRM_OPERATING_SYSTEM;
        }
    }
    return RC::OK;

}

void ModsGdmClient::FreeGdmParsingEvent()
{
    Tasker::FreeEvent(s_GdmParsingEvent);
    s_GdmParsingEvent = nullptr;

}
