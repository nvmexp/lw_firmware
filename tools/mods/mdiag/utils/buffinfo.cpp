/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "buffinfo.h"
#include "core/include/lwrm.h"
#include "core/include/massert.h"
#include "mdiag/sysspec.h"
#include "surfutil.h"
#include "gpu/utility/blocklin.h"
#include "tex.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0080/ctrl0080dma.h" // LW0080_CTRL_CMD_DMA_GET_PTE_INFO
#include "mdiag/utils/utils.h"
#include "class/cl844c.h"       // G84_PERFBUFFER

#define MSGID() __MSGID_DIR__(Mdiag)

BuffInfo::BuffInfo() :
    m_MaxEntryLength(COL_NUM),
    m_PrintBufferOnce(false),
    m_LwrrentBufferMemory(0)
{
    Entry line(COL_NUM);

    // The column labels should always be printed.
    line.m_Header = true;

    // the header of the table
    m_Entries.push_back(line);
    m_Entries.back().m_Columns[COL_BUFF] = "Name";
    m_MaxEntryLength[COL_BUFF] = m_Entries.back().m_Columns[COL_BUFF].length();
    m_Entries.back().m_Columns[COL_HMEM] = " hMem";
    m_MaxEntryLength[COL_HMEM] = m_Entries.back().m_Columns[COL_HMEM].length();
    m_Entries.back().m_Columns[COL_HVA] = " hVASpace";
    m_MaxEntryLength[COL_HVA] = m_Entries.back().m_Columns[COL_HVA].length();
    m_Entries.back().m_Columns[COL_VA] = "VA/Offset";
    m_MaxEntryLength[COL_VA] = m_Entries.back().m_Columns[COL_VA].length();
    m_Entries.back().m_Columns[COL_TEGRA_VA] = "CheetAh VA";
    m_MaxEntryLength[COL_TEGRA_VA] = m_Entries.back().m_Columns[COL_TEGRA_VA].length();
    m_Entries.back().m_Columns[COL_PA] = "PhysAddr";
    m_MaxEntryLength[COL_PA] = m_Entries.back().m_Columns[COL_PA].length();
    m_Entries.back().m_Columns[COL_APER] = "Aper";
    m_MaxEntryLength[COL_APER] = m_Entries.back().m_Columns[COL_APER].length();
    m_Entries.back().m_Columns[COL_SIZE_B] = "Size(B)";
    m_MaxEntryLength[COL_SIZE_B] = m_Entries.back().m_Columns[COL_SIZE_B].length();
    m_Entries.back().m_Columns[COL_SIZE_P] = "Size(pix)";
    m_MaxEntryLength[COL_SIZE_P] = m_Entries.back().m_Columns[COL_SIZE_P].length();
    m_Entries.back().m_Columns[COL_BL] = "BL";
    m_MaxEntryLength[COL_BL] = 3;
    m_Entries.back().m_Columns[COL_COMP] = "Comp";
    m_MaxEntryLength[COL_COMP] = m_Entries.back().m_Columns[COL_COMP].length();
    m_Entries.back().m_Columns[COL_GCACH] = "gCach";
    m_MaxEntryLength[COL_GCACH] = m_Entries.back().m_Columns[COL_GCACH].length();
    m_Entries.back().m_Columns[COL_KIND] = "Kind";
    m_MaxEntryLength[COL_KIND] = m_Entries.back().m_Columns[COL_KIND].length();
    m_Entries.back().m_Columns[COL_PRIV] = "Priv";
    m_MaxEntryLength[COL_PRIV] = m_Entries.back().m_Columns[COL_PRIV].length();
    m_Entries.back().m_Columns[COL_DESC] = "Desc";
    m_MaxEntryLength[COL_DESC] = m_Entries.back().m_Columns[COL_DESC].length();
    m_Entries.back().m_Columns[COL_PROT] = "Prot";
    m_MaxEntryLength[COL_PROT] = m_Entries.back().m_Columns[COL_PROT].length();
    m_Entries.back().m_Columns[COL_SPRT] = "sPrt";
    m_MaxEntryLength[COL_SPRT] = m_Entries.back().m_Columns[COL_SPRT].length();
    m_Entries.back().m_Columns[COL_PAGE_SIZE] = "PageSize";
    m_MaxEntryLength[COL_PAGE_SIZE] = m_Entries.back().m_Columns[COL_PAGE_SIZE].length();
    m_Entries.back().m_Columns[COL_CONTIG] = "Contig";
    m_MaxEntryLength[COL_CONTIG] = m_Entries.back().m_Columns[COL_CONTIG].length();
}

void BuffInfo::AddEntry(const string& name)
{
    Entry line(COL_NUM);
    m_Entries.push_back(line);
    fill(m_Entries.back().m_Columns.begin(), m_Entries.back().m_Columns.end(), "n/a");
    m_Entries.back().m_Columns[COL_BUFF]  = name;
    m_Entries.back().m_Columns[COL_BUFF] += ":";
    m_MaxEntryLength[COL_BUFF] = max(m_MaxEntryLength[COL_BUFF], UINT32(m_Entries.back().m_Columns[COL_BUFF].length()));
}

void BuffInfo::SetHMem(UINT32 hMem)
{
    string str = Utility::StrPrintf("0x%08x", hMem);
    m_Entries.back().m_Columns[COL_HMEM] = str;
    m_MaxEntryLength[COL_HMEM] = max(m_MaxEntryLength[COL_HMEM], UINT32(str.length()));
}

void BuffInfo::SetCompr(UINT32 hMem, LwRm* pLwRm)
{
#ifdef LW_VERIF_FEATURES
    if (!hMem)
        return;

    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset  = 0;

    RC rc = pLwRm->Control( hMem, LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                           &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf("%s\n", rc.Message());
        MASSERT(0);
    }
    m_Entries.back().m_Columns[COL_COMP] = params.comprFormat != 0 ? "yes" : "no";
    m_MaxEntryLength[COL_COMP] = max((size_t)m_MaxEntryLength[COL_COMP], m_Entries.back().m_Columns[COL_COMP].length());
#endif
}

void BuffInfo::SetHVA(UINT32 hVa)
{
    string str = Utility::StrPrintf("0x%08x", hVa);
    m_Entries.back().m_Columns[COL_HVA] = str;
    m_MaxEntryLength[COL_HVA] = max(m_MaxEntryLength[COL_HVA], UINT32(str.length()));
}

void BuffInfo::SetHVA(const string &name, UINT32 hVa)
{
    Entries::iterator end = m_Entries.end();
    for (Entries::iterator iter = m_Entries.begin(); iter != end; ++iter)
    {
        string::size_type pos = (*iter).m_Columns[COL_BUFF].rfind(':');
        if (name == (*iter).m_Columns[COL_BUFF].substr(0, pos))
        {
            string str = Utility::StrPrintf("0x%08x", hVa);
            (*iter).m_Columns[COL_HVA] = str;
            m_MaxEntryLength[COL_HVA] = max(m_MaxEntryLength[COL_HVA], UINT32(str.length()));
            return;
        }
    }
}

void BuffInfo::SetVA(UINT64 va)
{
    string str = Utility::StrPrintf("0x%llx", va);
    m_Entries.back().m_Columns[COL_VA] = str;
    m_MaxEntryLength[COL_VA] = max(m_MaxEntryLength[COL_VA], UINT32(str.length()));
}

void BuffInfo::SetVA(const string &name, UINT64 va)
{
    Entries::iterator end = m_Entries.end();
    for (Entries::iterator iter = m_Entries.begin(); iter != end; ++iter)
    {
        string::size_type pos = (*iter).m_Columns[COL_BUFF].rfind(':');
        if (name == (*iter).m_Columns[COL_BUFF].substr(0, pos))
        {
            string str = Utility::StrPrintf("0x%llx", va);
            (*iter).m_Columns[COL_VA] = str;
            m_MaxEntryLength[COL_VA] = max(m_MaxEntryLength[COL_VA], UINT32(str.length()));
            return;
        }
    }
}

void BuffInfo::SetTegraVA(UINT64 va)
{
    string str = Utility::StrPrintf("0x%llx", va);
    m_Entries.back().m_Columns[COL_TEGRA_VA] = str;
    m_MaxEntryLength[COL_TEGRA_VA] = max(m_MaxEntryLength[COL_TEGRA_VA], UINT32(str.length()));
}

void BuffInfo::SetTegraVA(const string &name, UINT64 va)
{
    Entries::iterator end = m_Entries.end();
    for (Entries::iterator iter = m_Entries.begin(); iter != end; ++iter)
    {
        string::size_type pos = (*iter).m_Columns[COL_BUFF].rfind(':');
        if (name == (*iter).m_Columns[COL_BUFF].substr(0, pos))
        {
            string str = Utility::StrPrintf("0x%llx", va);
            (*iter).m_Columns[COL_TEGRA_VA] = str;
            m_MaxEntryLength[COL_TEGRA_VA] = max(m_MaxEntryLength[COL_TEGRA_VA], UINT32(str.length()));
            return;
        }
    }
}

void BuffInfo::SetSizeB(UINT64 va)
{
    string str = Utility::StrPrintf("0x%llx", va);
    m_Entries.back().m_Columns[COL_SIZE_B] = str;
    m_Entries.back().m_BufferSize = va;
    m_MaxEntryLength[COL_SIZE_B] = max(m_MaxEntryLength[COL_SIZE_B], UINT32(str.length()));
}

void BuffInfo::SetTarget(const MdiagSurf& buff, bool needPeerMapping, bool isLocal)
{
    if (LocationToTarget(buff.GetLocation()) == _DMA_TARGET_VIDEO &&
        buff.GetLoopBack())
    {
        m_Entries.back().m_Columns[COL_APER] = "p2ploopback";
        m_MaxEntryLength[COL_APER] = max(m_MaxEntryLength[COL_APER],
                                         UINT32(m_Entries.back().m_Columns[COL_APER].length()));
    }
    else if (LocationToTarget(buff.GetLocation()) == _DMA_TARGET_VIDEO &&
             needPeerMapping)
    {
        m_Entries.back().m_Columns[COL_APER] = isLocal ? "local" : "remote";
        m_MaxEntryLength[COL_APER] = max(m_MaxEntryLength[COL_APER],
                                       UINT32(m_Entries.back().m_Columns[COL_APER].length()));
    }
    else
    {
        SetTarget(LocationToTarget(buff.GetLocation()));
    }
}

void BuffInfo::SetTarget(const string &name, const bool isLocal)
{
    Entries::iterator end = m_Entries.end();
    for (Entries::iterator iter = m_Entries.begin(); iter != end; ++iter)
    {
        string::size_type pos = (*iter).m_Columns[COL_BUFF].rfind(':');
        if (name == (*iter).m_Columns[COL_BUFF].substr(0, pos))
        {
            (*iter).m_Columns[COL_APER] = isLocal ? "local" : "remote";
            m_MaxEntryLength[COL_APER] = max(m_MaxEntryLength[COL_APER], (UINT32)6);
            return;
        }
    }
}

void BuffInfo::SetTarget(_DMA_TARGET target)
{
    switch(target) {
        case _DMA_TARGET_VIDEO:
            m_Entries.back().m_Columns[COL_APER] = "video";
            break;
        case _DMA_TARGET_COHERENT:
            m_Entries.back().m_Columns[COL_APER] = "coh";
            break;
        case _DMA_TARGET_NONCOHERENT:
            m_Entries.back().m_Columns[COL_APER] = "ncoh";
            break;
        case _DMA_TARGET_P2P:
            m_Entries.back().m_Columns[COL_APER] = "p2ploopback";
            break;
        default:
            m_Entries.back().m_Columns[COL_APER] = "unknown";
            break;
    }
    m_MaxEntryLength[COL_APER] = max(m_MaxEntryLength[COL_APER], UINT32(m_Entries.back().m_Columns[COL_APER].length()));
}

void BuffInfo::outputChannelInformation(UINT32 chan, LwRm* pLwRm, GpuDevice* pGpuDev) const
{
    if (!pGpuDev || Platform::GetSimulationMode() == Platform::Amodel)
        return;

    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        LW2080_CTRL_FIFO_MEM_INFO instProperties = MDiagUtils::GetInstProperties(pGpuDev->GetSubdevice(subdev), chan, pLwRm);

        string str;
        str = Utility::StrPrintf("GPU %d Channel 0x%x instance memory aperture 0x%x"
                                 " base 0x%llx"
                                 " size 0x%llx\n",
                                 subdev,
                                 chan,
                                 instProperties.aperture,
                                 instProperties.base,
                                 instProperties.size);
        InfoPrintf(MSGID(), str.c_str());
    }
}

void BuffInfo::SetDmaBuff
(
    const  string& name,
    const  MdiagSurf& buff,
    UINT64 offset,          // Optional, default 0
    UINT32 subdev,          // Optional, default Gpu::UNSPECIFIED_SUBDEV
    bool   needPeerMapping, // Optional, default false
    UINT64 size_override,   // Optional, default 0
    bool   isLocal          // Optional, default true
)
{
    if (buff.GetSize())
    {
        if (buff.IsSurfaceView())
        {
            const MdiagSurf* parentSurf = buff.GetParentSurface();
            string surfName = name + "(" + parentSurf->GetName() + ")";
            AddEntry(surfName);
        }
        else
        {
            AddEntry(name);
        }

        if (buff.IsVirtualOnly())
        {
            SetHMem(buff.GetVirtMemHandle());
        }
        else
        {
            SetHMem(buff.GetMemHandle());
        }

        if (!buff.IsPhysicalOnly())
        {
            SetHVA(buff.GetGpuVASpace());
            SetVA(buff.GetCtxDmaOffsetGpu() + buff.GetExtraAllocSize());
        }
        SetPageSize(buff);

        SetSizeB(size_override?size_override:buff.GetSize());

        if (!buff.IsVirtualOnly())
        {
            SetTarget(buff, needPeerMapping, isLocal);
        }

        SetPTE(buff);
        SetPriv(buff.GetPriv());
        SetProtect(buff.GetProtect());
        SetShaderAccess(buff);
        SetContig(buff.GetPhysContig());

        string str;
        if (buff.GetMemHandle())
        {
            if (buff.GetExternalPhysMem() != 0)
            {
                PHYSADDR physAddr;
                buff.GetPhysAddress(offset, &physAddr);
                str = Utility::StrPrintf("0x%llx", physAddr);
            }
            else if (Memory::Fb == buff.GetLocation())
            {
                SetCompr(buff.GetMemHandle(), buff.GetLwRmPtr());
                getMemOffsetAndCache0041(buff, offset, str);
            }
            else
            {
                getMemOffsetAndCache003E(buff, offset, str);
            }
            m_Entries.back().m_Columns[COL_PA] = str;
            m_MaxEntryLength[COL_PA] = max(m_MaxEntryLength[COL_PA], UINT32(str.length()));
        }
    }
}

void BuffInfo::SetRenderSurface(const MdiagSurf& surf)
{
    string surfName = surf.GetName();

    if (surf.IsSurfaceView())
    {
        const MdiagSurf* parentSurf = surf.GetParentSurface();
        surfName += "(" + parentSurf->GetName() + ")";
    }
    AddEntry(surfName);
    SetHMem(surf.GetMemHandle());
    SetHVA(surf.GetGpuVASpace());
    SetVA(surf.GetCtxDmaOffsetGpu()+surf.GetExtraAllocSize());
    SetSizeB(surf.GetSize());
    SetTarget(surf.GetLoopBack() ? _DMA_TARGET_P2P : LocationToTarget(surf.GetLocation()));
    SetSizePix(surf);
    SetSizeBL(surf);
    SetPTE(surf);
    SetPriv(surf.GetPriv());
    SetProtect(surf.GetProtect());
    SetShaderAccess(surf);
    SetPageSize(surf);
    SetContig(surf.GetPhysContig());

    // SetCompr() should be used instead
    m_Entries.back().m_Columns[COL_COMP] = surf.GetCompressed() ? "yes" : "no";
    m_MaxEntryLength[COL_COMP] = max(m_MaxEntryLength[COL_COMP], UINT32(m_Entries.back().m_Columns[COL_COMP].length()));

    string str;

    if (surf.GetMemHandle() != 0)
    {
        if (surf.GetExternalPhysMem() != 0)
        {
            PHYSADDR physAddr;
            surf.GetPhysAddress(0, &physAddr);
            str = Utility::StrPrintf("0x%llx", physAddr);
        }
        else if (Memory::Fb == surf.GetLocation())
        {
            getMemOffsetAndCache0041(surf, 0, str);
        }
        else
        {
            getMemOffsetAndCache003E(surf, 0, str);
        }

        m_Entries.back().m_Columns[COL_PA] = str;
        m_MaxEntryLength[COL_PA] = max(m_MaxEntryLength[COL_PA], UINT32(str.length()));
    }
}

void BuffInfo::Print(const char *subtitle, GpuDevice* pGpuDev)
{
    Entries::iterator eiter;
    bool willPrint = false;

    // Check and see if any of the entries will be printed.

    if (m_PrintBufferOnce)
    {
        for (eiter = m_Entries.begin(); eiter != m_Entries.end(); ++eiter)
        {
            if (!(*eiter).m_Printed && !(*eiter).m_Header)
            {
                willPrint = true;
            }
        }
    }
    else
    {
        willPrint = true;
    }

    // Don't bother printing anything if there are no buffers to print.
    if (!willPrint)
    {
        return;
    }

    RawPrintf(MSGID(), "\n");
    for (auto channelHandle : m_channelHandles)
        outputChannelInformation(channelHandle.m_Handle, channelHandle.m_HandleLwRm, pGpuDev);

    if (subtitle != 0)
    {
        RawPrintf(MSGID(), " Alloc buffers (%s):\n", subtitle);
    }
    else
    {
        RawPrintf(MSGID(), " Alloc buffers:\n");
    }

    char entry[m_EntryLength];
    char line_format[8];
    UINT64 totalBufferSize = 0;
    bool printTegraVA = Platform::IsTegra();

    for (eiter = m_Entries.begin(); eiter != m_Entries.end(); ++eiter)
    {
        if (!m_PrintBufferOnce || !(*eiter).m_Printed || (*eiter).m_Header)
        {
            string line;
            string escapeWrite;
            for (int i = COL_BUFF; i < COL_NUM; ++i)
            {
                if (i == COL_TEGRA_VA && !printTegraVA)
                {
                    continue;
                }
                snprintf(line_format, 8, "%%%ds", m_MaxEntryLength[i] + 1);
                snprintf(entry, m_EntryLength, line_format, (*eiter).m_Columns[i].c_str());
                line += entry;
                // Name, Physaddr, Size, Prot, sProt
                // FIXME: VA or PA?
                if (i == COL_BUFF || i == COL_VA || i == COL_SIZE_B || i == COL_PROT || i == COL_SPRT)
                {
                    escapeWrite += entry;
                }
            }
            RawPrintf(MSGID(), "%s\n", line.c_str());
            if (eiter != m_Entries.begin())
            {
                MDiagUtils::SetMemoryTag tag(pGpuDev, escapeWrite.c_str());
            }
            (*eiter).m_Printed = true;
            totalBufferSize += (*eiter).m_BufferSize;
        }
    }

    m_LwrrentBufferMemory += totalBufferSize;
    RawPrintf(MSGID(), "\n");
    RawPrintf(MSGID(), " Total buffer size in table = 0x%llx\n", totalBufferSize);
    RawPrintf(MSGID(), " Current buffer memory allocated = 0x%llx\n", m_LwrrentBufferMemory);
    RawPrintf(MSGID(), "\n");
}

void BuffInfo::SetSizePix(const MdiagSurf& surf)
{
    if (surf.GetWidth() > 0 && surf.GetHeight() > 0)
    {
        string str;

        if (surf.GetDepth() > 1)
        {
            str = Utility::StrPrintf("%dx%dx%d",
                surf.GetWidth(), surf.GetHeight(), surf.GetDepth());
        }
        else
        {
            str = Utility::StrPrintf("%dx%d",
                surf.GetWidth(), surf.GetHeight());
        }

        SetSizePix(str.c_str());
    }
}

void BuffInfo::SetSizePix(const char* str)
{
    m_Entries.back().m_Columns[COL_SIZE_P] = str;
    m_MaxEntryLength[COL_SIZE_P] = max(m_MaxEntryLength[COL_SIZE_P], UINT32(m_Entries.back().m_Columns[COL_SIZE_P].length()));
}

void BuffInfo::SetSizeBL(const char* bl)
{
    m_Entries.back().m_Columns[COL_BL] = bl;
    m_MaxEntryLength[COL_BL] = max(m_MaxEntryLength[COL_BL], UINT32(m_Entries.back().m_Columns[COL_BL].length()));
}

void BuffInfo::SetSizeBL(const MdiagSurf& surf)
{
    if (surf.GetLayout() == Surface2D::BlockLinear)
    {
        string str = Utility::StrPrintf("%dx%dx%d", 1 << surf.GetLogBlockWidth(),
                                  1 << surf.GetLogBlockHeight(),
                                  1 << surf.GetLogBlockDepth());
        SetSizeBL(str.c_str());
    }
    else
    {
        MASSERT(surf.GetLayout() == Surface2D::Pitch);
        SetSizeBL("pitch");
    }
}

void BuffInfo::SetSizeBL(const BlockLinear* bl)
{
    if (bl)
    {
        string str = Utility::StrPrintf("%dx%dx%d", 1 << bl->Log2BlockWidthGob(),
                                 1 << bl->Log2BlockHeightGob(),
                                 1 << bl->Log2BlockDepthGob());
        SetSizeBL(str.c_str());
    }
    else
    {
        SetSizeBL("pitch");
    }
}

void BuffInfo::SetSizeBL(const TextureHeader* th)
{
    MASSERT(th);

    string str;
    if (th->IsBlocklinear())
    {

        str = Utility::StrPrintf("%dx%dx%d", th->BlockWidth(), th->BlockHeight(), th->BlockDepth());
        SetSizeBL(str.c_str());
    }
    else
    {
        SetSizeBL("pitch");
    }

    str = Utility::StrPrintf("%dx%dx%d", th->TexWidth(), th->TexHeight(), th->TexDepth());
    SetSizePix(str.c_str());
}

void  BuffInfo::specifyChannelHandle(UINT32 ch, LwRm* pLwRm)
{
    for (auto channelHandle : m_channelHandles)
        if (channelHandle.m_Handle == ch)
            return
    m_channelHandles.push_back(ChannelHandle(ch, pLwRm));
}

void BuffInfo::getMemOffsetAndCache0041(const MdiagSurf& surf, UINT64 offset, string &str)
{
#ifdef LW_VERIF_FEATURES
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset  = offset;

    LwRm* pLwRm = surf.GetLwRmPtr();
    RC rc = pLwRm->Control(surf.GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf("%s\n", rc.Message());
        MASSERT(0);
    }

    str = Utility::StrPrintf("0x%llx", params.memOffset);

    const char *cacheAttr="";
    const UINT32 gpuCacheAttr = surf.GetLoopBack() ? params.gpuP2PCacheAttr : params.gpuCacheAttr;
    if (gpuCacheAttr == LW0041_CTRL_GET_SURFACE_PHYS_ATTR_GPU_CACHED)
        cacheAttr = "yes";
    else if (gpuCacheAttr == LW0041_CTRL_GET_SURFACE_PHYS_ATTR_GPU_UNCACHED)
        cacheAttr = "no";
    else
        cacheAttr = "n/a";
    m_Entries.back().m_Columns[COL_GCACH] = cacheAttr;
    m_MaxEntryLength[COL_GCACH] = max((size_t)m_MaxEntryLength[COL_GCACH], m_Entries.back().m_Columns[COL_GCACH].length());
#endif
}

void BuffInfo::getMemOffsetAndCache003E(const MdiagSurf& surf, UINT64 offset, string &str)
{
#ifdef LW_VERIF_FEATURES
    LW003E_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
    params.memOffset = offset;

    LwRm* pLwRm = surf.GetLwRmPtr();
    RC rc = pLwRm->Control( surf.GetMemHandle(),
                            LW003E_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                            &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf("%s\n", rc.Message());
        MASSERT(0);
    }

    str = Utility::StrPrintf("0x%llx", params.memOffset);

    const char *cacheAttr="";
    const UINT32 gpuCacheAttr = surf.GetLoopBack() ? params.gpuP2PCacheAttr : params.gpuCacheAttr;
    if (gpuCacheAttr == LW003E_CTRL_GET_SURFACE_PHYS_ATTR_GPU_CACHED)
        cacheAttr = "yes";
    else if (gpuCacheAttr == LW003E_CTRL_GET_SURFACE_PHYS_ATTR_GPU_UNCACHED)
        cacheAttr = "no";
    else
        cacheAttr = "n/a";
    m_Entries.back().m_Columns[COL_GCACH] = cacheAttr;
    m_MaxEntryLength[COL_GCACH] = max((size_t)m_MaxEntryLength[COL_GCACH], m_Entries.back().m_Columns[COL_GCACH].length());
#endif
}

void BuffInfo::SetPTE(const MdiagSurf& surf)
{
    INT32 pte = surf.GetPteKind();
    if (pte != -1)
    {
        string str = Utility::StrPrintf("0x%2.2x", pte);
        SetPTE(str.c_str());
    }
}

void BuffInfo::SetPTE(const char* str)
{
    m_Entries.back().m_Columns[COL_KIND] = str;
    m_MaxEntryLength[COL_KIND] = max(m_MaxEntryLength[COL_KIND], UINT32(m_Entries.back().m_Columns[COL_KIND].length()));
}

void BuffInfo::SetPriv(bool priv)
{
    string str = Utility::StrPrintf("%s", priv ? "yes" : "no");
    m_Entries.back().m_Columns[COL_PRIV] = str;
    m_MaxEntryLength[COL_PRIV] = max(m_MaxEntryLength[COL_PRIV], UINT32(m_Entries.back().m_Columns[COL_PRIV].length()));
}

void BuffInfo::SetContig(bool contig)
{
    string str = Utility::StrPrintf("%s", contig ? "yes" : "no");
    m_Entries.back().m_Columns[COL_CONTIG] = str;
    m_MaxEntryLength[COL_CONTIG] = max(m_MaxEntryLength[COL_CONTIG], UINT32(m_Entries.back().m_Columns[COL_CONTIG].length()));
}

//! \brief Helper method for translating and storing Memory::Protect flag.
//!
//! \param prot protection flag to be translated/stored
//! \param col  column index for storing the protection string
void BuffInfo::translateAccessFlag(Memory::Protect prot, int col)
{
    string& str = m_Entries.back().m_Columns[col];
    switch (prot)
    {
        case Memory::Readable:
            str = "r";
            break;
        case Memory::Writeable:
            str = "w";
            break;
        case Memory::ReadWrite: // Readable | Writable
            str = "rw";
            break;
        case Memory::Exelwtable:
            str = "x";
            break;
        case Memory::ProtectDefault:
        default:
            break; // leave 'n/a' as-is
    }
    m_MaxEntryLength[col] = max(m_MaxEntryLength[col], UINT32(str.length()));
}

//! \brief Set the protection column of the current LOG row.
//!
//! We obtain this info directly from Surface because lwrrently.
//! there is no Control command for such flag.
//! \param prot protection flag of the Surface
void BuffInfo::SetProtect(Memory::Protect prot)
{
    translateAccessFlag(prot, COL_PROT);
}

void BuffInfo::SetPageSize(const MdiagSurf& surf)
{
    string& str = m_Entries.back().m_Columns[COL_PAGE_SIZE];
    UINT32 pageSize = surf.GetPageSize();
    UINT32 hMem = surf.GetMemHandle();
    LwRm* pLwRm = surf.GetLwRmPtr();

    // Skip querying RM for Perfmon Buffer since they are alloacted in a 
    // different VaSpace and RM asserts when querying PTE info
    if (surf.GetParentClass() != G84_PERFBUFFER)
    {
        // RM ctrl call used in GetActualPageSizeKB() is not supported on 
        // AMODEL and for Physical-only/Virtual-only surfaces
        if (Platform::GetSimulationMode() != Platform::Amodel &&
            !surf.IsPhysicalOnly() &&
            !surf.IsVirtualOnly())
        {
            pageSize = surf.GetActualPageSizeKB();
        }
        // AMODEL or Physical-only surfaces with the exception of surfaces like
        // VPR, FLA, VAB (RM ctrl call is not supported for such surfaces)
        else if (hMem != 0 && surf.GetExternalPhysMem() == 0)
        {
            LW0041_CTRL_GET_MEM_PAGE_SIZE_PARAMS params = { };

            RC rc = pLwRm->Control(hMem, LW0041_CTRL_CMD_GET_MEM_PAGE_SIZE,
                                    &params, sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s: LW0041_CTRL_CMD_GET_MEM_PAGE_SIZE failed with the error message %s for surface %s\n", 
                    __FUNCTION__, rc.Message(), surf.GetName().c_str());
                MASSERT(0);
            }
            pageSize = params.pageSize/1024;   
        }
    }
    
    switch (pageSize)
    {
        case 4:
            str = "4KB";
            break;
        case 64:
            str = "64KB";
            break;
        case 128:
            str = "128KB";
            break;
        case 2048:
            str = "2MB";
            break;
        case 524288:
            str = "512MB";
            break;
        default:
            break;
    }
    m_MaxEntryLength[COL_PAGE_SIZE] = max(m_MaxEntryLength[COL_PAGE_SIZE], UINT32(str.length()));
}

//! \brief Set the shader access column of the current LOG row.
//!
//! We obtain this info by issuing the RM's Control call with
//! LW0080_CTRL_CMD_DMA_GET_PTE_INFO command.
//! \param surf the surface object
void BuffInfo::SetShaderAccess(const MdiagSurf& surf)
{
    if ((Platform::GetSimulationMode() == Platform::Amodel) ||
        (Platform::GetSimulationMode() == Platform::RTL))
    {
        // Because Amodel does not support the GET_PTE_INFO command,
        // we fallback to query the Surface for such information.
        translateAccessFlag(surf.GetShaderProtect(), COL_SPRT);
    }
    else
    {
        // Use LW0080_CTRL_CMD_DMA_GET_PTE_INFO command
        // (adapted from gpu/tests/rm/dma/rmt_ctrl0080dma.cpp)
        LwRm* pLwRm = surf.GetLwRmPtr();
        LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS params;
        LW0080_CTRL_DMA_PTE_INFO_PTE_BLOCK* pPteBlock = NULL;

        // Specify the address of the shader we are querying
        memset(&params, 0, sizeof(params));
        params.gpuAddr = surf.GetCtxDmaOffsetGpu();
        params.skipVASpaceInit = true;

        // Some buffers (e.g., Err(FakeGL)) might have NULL address,
        // which is invalid for the Control command. Thus, we skip.
        if (!params.gpuAddr)
            return;

        RC rc = pLwRm->Control
        (
            pLwRm->GetDeviceHandle(surf.GetGpuDev()), // bug: 791104
            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
            &params,
            sizeof (params)
        );

        if (OK != rc)
        {
            ErrPrintf("%s\n", rc.Message());
            return;
        }

        // There should be only one valid block, we search for it.
        for (UINT32 j = 0; j < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; j++)
        {
            if (DRF_VAL(0080_CTRL, _DMA_PTE_INFO,
                    _PARAMS_FLAGS_VALID, params.pteBlocks[j].pteFlags)
                == LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
            {
                pPteBlock = params.pteBlocks + j;
                break;
            }
        }

        if (pPteBlock)
        {
            // Retrieve the access flag
            UINT32 access = DRF_VAL
            (
                    0080_CTRL,
                    _DMA_PTE_INFO,
                    _PARAMS_FLAGS_SHADER_ACCESS,
                    pPteBlock->pteFlags
            );

            string& str = m_Entries.back().m_Columns[COL_SPRT];
            switch(access)
            {
                case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_WRITE:
                    str = "rw";
                    break;
                case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_ONLY:
                    str = "r";
                    break;
                case LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_WRITE_ONLY:
                    str = "w";
                    break;
                default:
                    // Leave 'n/a' as-is
                    break;
            }
            m_MaxEntryLength[COL_SPRT] = max(m_MaxEntryLength[COL_SPRT], UINT32(str.length()));
        }
    }
}

void BuffInfo::SetDescription(const string &desc)
{
    m_Entries.back().m_Columns[COL_DESC] = desc;
    m_MaxEntryLength[COL_DESC] = max(m_MaxEntryLength[COL_DESC], UINT32(m_Entries.back().m_Columns[COL_DESC].length()));
}

void BuffInfo::FreeBufferMemory(UINT64 bufferSize)
{
    m_LwrrentBufferMemory -= bufferSize;

    DebugPrintf(MSGID(), " Current buffer memory allocated = 0x%llx\n",
        m_LwrrentBufferMemory);
}
