/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "channelallocinfo.h"

#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "mdiag/tests/gpu/trace_3d/tracetsg.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "mdiag/advschd/pmchan.h"
#include "mdiag/utils/utils.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/advschd/pmsmcengine.h"

#include <algorithm>

// translate aperture enum into human readable string
string ChannelAllocInfo::InstApertureToString(UINT32 instAperture)
{
    string apertureStr;
    switch(instAperture)
    {
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_VIDMEM:
            apertureStr = "video";
            break;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_COH:
            apertureStr = "coh";
            break;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_NCOH:
            apertureStr = "ncoh";
            break;
        default:
            MASSERT(!"instance memory aperture is invalid!");
            break;
    }
    return apertureStr;
}

const std::map<ChannelAllocInfo::COL_ID, std::string> ChannelAllocInfo::m_ColumnName =
{
    {COL_NAME, "Name"}, {COL_HCHANNEL, "hChannel"}, {COL_CHID, "ChId"},
    {COL_INSTAPER, "InstAper"}, {COL_INSTPA, "InstPhysAddr"}, {COL_INSTSIZE, "InstSize"},
    {COL_TSG, "Tsg(hTsg)"}, {COL_VASPACE, "VASpace(hVASpace)"},
    {COL_SUBCTX, "Subctx(hSubctx)"}, {COL_VEID, "VEID"}, {COL_ENGINE, "ENGINE"}
};

ChannelAllocInfo::ChannelAllocInfo()
: m_MaxEntryLength(COL_NUM)
{
    InitHeader();
}

void ChannelAllocInfo::AddChannel(PmChannel* pPmChannel, GpuSubdevice *pSubGpuDevice, UINT32 engineId)
{
    m_Entries.emplace_back();

    SmcEngine* pSmcEngine = pPmChannel->GetPmSmcEngine() ? pPmChannel->GetPmSmcEngine()->GetSmcEngine() : nullptr;

    SetName(pPmChannel->GetName());
    SetChannel(pPmChannel->GetModsChannel(), pSubGpuDevice, pPmChannel->GetLwRmPtr());
    SetTsg(pPmChannel->GetTsg());
    SetVaSpace(pPmChannel->GetVaSpace());
    SetSubCtx(pPmChannel->GetSubCtx().get());
    SetEngine(engineId, pPmChannel->GetModsChannel()->GetGpuDevice(), pSmcEngine);
}

void ChannelAllocInfo::AddChannel(LWGpuChannel* pLWGpuChannel)
{
    m_Entries.emplace_back();

    Channel* pChannel = pLWGpuChannel->GetModsChannel();
    GpuDevice* pGpuDevice = pLWGpuChannel->GetGpuResource()->GetGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);
    LwRm* pLwRm = pLWGpuChannel->GetLwRmPtr();

    SetName(pLWGpuChannel->GetName());
    SetChannel(pChannel, pGpuSubdevice, pLwRm);
    SetTsg(pLWGpuChannel->GetLWGpuTsg());
    SetVaSpace(pLWGpuChannel->GetVASpace().get());
    SetSubCtx(pLWGpuChannel->GetSubCtx().get());
    SetEngine(pLWGpuChannel->GetEngineId(), pGpuDevice, pLWGpuChannel->GetSmcEngine());
}

void ChannelAllocInfo::Print(const std::string& subtitle)
{
    if (!IsPrintNeeded())
    {
        return;
    }

    InfoPrintf("\n");
    if (!subtitle.empty())
    {
        InfoPrintf(" Alloc channels (%s):\n", subtitle.c_str());
    }
    else
    {
        InfoPrintf(" Alloc channels:\n");
    }

    char entryStr[m_EntryLength];
    char lineFormat[8];
    int channlesInTable = 0;
    for (auto& entry : m_Entries)
    {
        if (!m_PrintOnce || !entry.m_Printed || entry.m_Header)
        {
            std::string line;
            std::string escapeWrite;
            for (int i = COL_NAME; i < COL_NUM; ++i)
            {
                snprintf(lineFormat, 8, "%%%ds", m_MaxEntryLength[i] + 1);
                snprintf(entryStr, m_EntryLength, lineFormat, entry.m_Columns[i].c_str());
                line += entryStr;
            }
            InfoPrintf("%s\n", line.c_str());
            entry.m_Printed = true;
            if (!entry.m_Header)
            {
                ++channlesInTable;
            }
        }
    }

    InfoPrintf("\n");
    InfoPrintf(" %d channel(s) in table\n", channlesInTable);
    InfoPrintf("\n");
}

void ChannelAllocInfo::InitHeader()
{
    Entry header;

    // The column labels should always be printed.
    header.m_Header = true;

    for (const auto& col : m_ColumnName)
    {
        header.m_Columns[col.first] = col.second;
        m_MaxEntryLength[col.first] = static_cast<UINT32>(col.second.size());
    }

    m_Entries.push_back(std::move(header));
}

void ChannelAllocInfo::SetName(const std::string& name)
{
    auto& line = m_Entries.back();
    line.m_Columns[COL_NAME] = name + ":";
    UpdateColumnMaxLength(COL_NAME, line.m_Columns[COL_NAME].size());
}

void ChannelAllocInfo::SetChannel(Channel* pChannel, GpuSubdevice *pSubGpuDevice, LwRm* pLwRm)
{
    auto& line = m_Entries.back();

    line.m_Columns[COL_HCHANNEL] = Utility::StrPrintf("0x%08x", pChannel->GetHandle());
    UpdateColumnMaxLength(COL_HCHANNEL, line.m_Columns[COL_HCHANNEL].size());

    line.m_Columns[COL_CHID] = Utility::StrPrintf("0x%x", pChannel->GetChannelId());
    UpdateColumnMaxLength(COL_CHID, line.m_Columns[COL_CHID].size());

    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        LW2080_CTRL_FIFO_MEM_INFO instProperties = MDiagUtils::GetInstProperties(pSubGpuDevice, pChannel->GetHandle(), pLwRm);
        line.m_Columns[COL_INSTAPER] = InstApertureToString(instProperties.aperture);
        UpdateColumnMaxLength(COL_CHID, line.m_Columns[COL_INSTAPER].size());
        line.m_Columns[COL_INSTPA] = Utility::StrPrintf("0x%llx", instProperties.base);
        UpdateColumnMaxLength(COL_INSTPA, line.m_Columns[COL_INSTPA].size());
        line.m_Columns[COL_INSTSIZE] = Utility::StrPrintf("0x%llx", instProperties.size);
        UpdateColumnMaxLength(COL_INSTSIZE, line.m_Columns[COL_INSTSIZE].size());
    }
}

void ChannelAllocInfo::SetTsg(const LWGpuTsg* pTsg)
{
    if (pTsg == nullptr)
    {
        return;
    }

    auto& line = m_Entries.back();
    line.m_Columns[COL_TSG] = Utility::StrPrintf("%s(0x%08x)",
                                                 pTsg->GetName().c_str(),
                                                 pTsg->GetHandle());
    UpdateColumnMaxLength(COL_TSG, line.m_Columns[COL_TSG].size());
}

void ChannelAllocInfo::SetVaSpace(VaSpace* pVaSpace)
{
    if (pVaSpace == nullptr)
    {
        return;
    }

    auto& line = m_Entries.back();
    line.m_Columns[COL_VASPACE] = Utility::StrPrintf("%s(0x%08x)",
                                                     pVaSpace->GetName().c_str(),
                                                     pVaSpace->GetHandle());
    UpdateColumnMaxLength(COL_VASPACE, line.m_Columns[COL_VASPACE].size());
}

void ChannelAllocInfo::SetSubCtx(SubCtx* pSubCtx)
{
    if (pSubCtx == nullptr)
    {
        return;
    }
    auto& line = m_Entries.back();

    line.m_Columns[COL_SUBCTX] = Utility::StrPrintf("%s(0x%08x)",
                                                    pSubCtx->GetName().c_str(),
                                                    pSubCtx->GetHandle());
    UpdateColumnMaxLength(COL_SUBCTX, line.m_Columns[COL_SUBCTX].size());

    line.m_Columns[COL_VEID] = Utility::StrPrintf("%d", pSubCtx->GetVeid());
    UpdateColumnMaxLength(COL_SUBCTX, line.m_Columns[COL_SUBCTX].size());
}

void ChannelAllocInfo::SetEngine(UINT32 engineId, GpuDevice* pGpuDevice, SmcEngine* pSmcEngine)
{
    auto& line = m_Entries.back();

    string engineStr;
    
    if (pSmcEngine && MDiagUtils::IsGraphicsEngine(engineId))
    {
        engineStr = pSmcEngine->GetName();
    }
    else
    {
        engineStr = pGpuDevice->GetEngineName(engineId);
    }

    line.m_Columns[COL_ENGINE] = engineStr;
    UpdateColumnMaxLength(COL_ENGINE, line.m_Columns[COL_ENGINE].size());
}

void ChannelAllocInfo::UpdateColumnMaxLength(COL_ID colID, size_t length)
{
    m_MaxEntryLength[colID] = std::max(m_MaxEntryLength[colID], static_cast<UINT32>(length));
}

bool ChannelAllocInfo::IsPrintNeeded() const
{
    if (!m_PrintOnce)
    {
        return true;
    }

    for (const auto& entry : m_Entries)
    {
        if (!entry.m_Printed && !entry.m_Header)
        {
            return true;
        }
    }

    return false;
}

