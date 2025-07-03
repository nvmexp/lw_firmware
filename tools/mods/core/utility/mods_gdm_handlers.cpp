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

#include "lwdiagutils.h"
#include "../multinode/transport/inc/connection.h"
#include "gdm_message_handler.h"
#include "gdm_message_reader.h"
#include "core/include/log.h"
#include "core/include/mods_gdm_client.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

//------------------------------------------------------------------------------
LwDiagUtils::EC GdmMessageHandler::HandleVersion
(
    GdmMessages::Version const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

//------------------------------------------------------------------------------
LwDiagUtils::EC GdmMessageHandler::HandleShutdown
(
    GdmMessages::Shutdown const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}


LwDiagUtils::EC GdmMessageHandler::HandleHeartBeat
(
    GdmMessages::HeartBeat const &msg,
    void *                   pvConnection
)
{
    Connection * pConn = static_cast<Connection *>(pvConnection);
    ModsGdmClient::Instance().UpdateGdmHeartBeat();

    Printf(Tee::PriLow,
                      "%s : Node ID %u sent heart beat messages\n",
                      pConn->GetConnectionString().c_str(),
                      msg.header.node_id);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRegister
(
    GdmMessages::Register const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().RegisterGdmHeartBeat(msg.heart_beat_period);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleUnRegister
(
    GdmMessages::UnRegister const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRegistrationId
(
    GdmMessages::RegistrationId const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().UpdateModsRegId(msg.registraion_id);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleMissingHeartBeat
(
    GdmMessages::MissingHeartBeat const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleShutdownAck
(
    GdmMessages::ShutdownAck const &msg,
    void *                   pvConnection
)
{
    if (msg.shutdown_success)
    {
        Tasker::SetEvent(ModsGdmClient::Instance().s_GdmShutDownAck);
    }
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetNumGpu
(
    GdmMessages::GetNumGpu const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetNumGpu
(
    GdmMessages::RetNumGpu const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().SetModsNumGpus(msg.num_gpus);
    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetNumLwSwitch
(
    GdmMessages::GetNumLwSwitch const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetNumLwSwitch
(
    GdmMessages::RetNumLwSwitch const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().SetModsNumLwSwitch(msg.num_lw_switch);
    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetGfmGpuMaxLwLinks
(
    GdmMessages::GetGfmGpuMaxLwLinks const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetGfmGpuMaxLwLinks
(
    GdmMessages::RetGfmGpuMaxLwLinks const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().SetModsGpuMaxLwLinks(msg.physical_id, msg.max_lw_links);
    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetGfmPhysicalId
(
    GdmMessages::GetGfmPhysicalId const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetGfmPhysicalId
(
    GdmMessages::RetGfmPhysicalId const &msg,
    void *                   pvConnection
)
{
    ModsGdmClient::Instance().SetModsPhysicalID(msg.index, msg.physical_id, msg.gpu);
    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetGfmGpuEnumIdx
(
    GdmMessages::GetGfmGpuEnumIdx const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetGfmGpuEnumIdx
(
    GdmMessages::RetGfmGpuEnumIdx const &msg,
    void *                   pvConnection
)
{

    ModsGdmClient::Instance().SetModsGpuEnumIndex(msg.node_id,
                                                  msg.physical_id,
                                                  msg.enum_idx,
                                                  msg.found,
                                                  msg.gpu);

    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleGetGfmGpuPciBdf
(
    GdmMessages::GetGfmGpuPciBdf const &msg,
    void *                   pvConnection
)
{
    return LwDiagUtils::OK;
}

LwDiagUtils::EC GdmMessageHandler::HandleRetGfmGpuPciBdf
(
    GdmMessages::RetGfmGpuPciBdf const &msg,
    void *                   pvConnection
)
{
    FMPciInfo_t pciInfo;
    pciInfo.domain = msg.pci_info.domain;
    pciInfo.bus = msg.pci_info.bus;
    pciInfo.device = msg.pci_info.device;
    pciInfo.function = msg.pci_info.function;
    ModsGdmClient::Instance().SetModsGpuPciBdf(msg.node_id,
                                               msg.enum_idx,
                                               pciInfo,
                                               msg.found,
                                               msg.gpu);
    Tasker::SetEvent(ModsGdmClient::Instance().s_GdmParsingEvent);
    return LwDiagUtils::OK;
}
