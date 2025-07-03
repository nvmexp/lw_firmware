/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "slisurf.h"

#include "mdiag/utils/utils.h"
#include "core/include/lwrm.h"
#include "core/utility/errloggr.h"

#include "core/include/tee.h"
#include "core/include/fpicker.h"

#include "mdiag/utils/surfutil.h"

#include "gpu/include/gpudev.h"
#include "tracemod.h"
#include "teegroups.h"
// //////////////////////////////////////////////////////////////////////////

#define MSGID() T3D_MSGID(SLI)

static char* Bool2String(bool b)
{
    static char True[] = "True";
    static char False[] = "False";
    return (b ? True : False);
}

SliSurfaceMapper::PerGpuDeviceInfo::
PerGpuDeviceInfo(GpuDevice* pgd) : m_pGpuDev(pgd)
{
    MASSERT(pgd);
}

void
SliSurfaceMapper::PerGpuDeviceInfo::
PrintMap(const Accessing2Hosting& m) const
{
    InfoPrintf("Accessing subdevice -> Hosting subdevice\n");
    for (Accessing2Hosting::const_iterator i = m.begin();
         i != m.end(); ++i)
    {
        InfoPrintf("%d -> %d (%s)\n",
                   i->first, i->second,
                   ( (i->first == i->second) ? "local" : "remote") );
    }
}

void SliSurfaceMapper::PerGpuDeviceInfo::Print() const
{
    InfoPrintf("PerGpuDeviceInfo::%s()\n", __FUNCTION__);
    InfoPrintf("m_pGpuDev= %p", m_pGpuDev);
    InfoPrintf("Num dmaBuffer= %u, Num render Surface= %u\n",
               m_DmaBufferSet.size(), m_RenderSurfaceSet.size());
    InfoPrintf("Num dmaBufferMap= %u, Num RenderSurfaceMap= %u\n",
               m_DmaBufferMap.size(), m_RenderSurfaceMap.size());
    for (map<MdiagSurf*, Accessing2Hosting>::const_iterator
         i = m_DmaBufferMap.begin();
         i != m_DmaBufferMap.end(); ++i)
    {
        InfoPrintf("m_DmaBufferMap= %p (%s)\n", i->first, i->first->GetName().c_str());
        PrintMap(i->second);
        PrintDmaBufferParams(*i->first);
    }
    for (map<MdiagSurf*, Accessing2Hosting>::const_iterator
         i = m_RenderSurfaceMap.begin();
         i != m_RenderSurfaceMap.end(); ++i)
    {
        InfoPrintf("m_RenderSurfaceMap= %p (%s)\n", i->first, i->first->GetName().c_str());
        PrintMap(i->second);
        PrintParams(i->first);
    }
}

RC SliSurfaceMapper::PerGpuDeviceInfo::MapPeer()
{
    RC rc;
    DebugPrintf(MSGID(), "PerGpuDeviceInfo::%s()\n", __FUNCTION__);
    // Misc surfaces
    for (map<MdiagSurf*, Accessing2Hosting>::const_iterator
         i = m_DmaBufferMap.begin();
         i != m_DmaBufferMap.end(); ++i)
    {
        bool need_peer_mapping = false;
        if (i->first->GetLocation() == Memory::Fb &&
            i->first->GetMemHandle() != 0)
        {
            Accessing2Hosting::const_iterator mapping_end = i->second.end();
            for (Accessing2Hosting::const_iterator mapping_i = i->second.begin();
                    mapping_i != mapping_end; ++mapping_i)
            {
                if (mapping_i->first != mapping_i->second)
                    need_peer_mapping = true; // if at least one surface is not local...
            }
        }
        if (need_peer_mapping)
        {
            rc = i->first->MapPeer();
            if (rc != OK)
            {
                ErrPrintf("MapPeer call on MdiagSurf %p returned an error\n",
                        i->first);
                return rc;
            }
        }
    }

    return rc;
}

SliSurfaceMapper::SliSurfaceMapper() :
    m_Params(0),
    m_SliLocalSurfaces(0), m_SliRemoteSurfaces(0), m_SliSeed(0) {}

SliSurfaceMapper::~SliSurfaceMapper()
{
}

RC SliSurfaceMapper::Setup()
{
    RC rc = OK;
    if (GetParams() == 0)
    {
        ErrPrintf("No params are specified for SliSurfaceMapper.\n");
        return RC::SOFTWARE_ERROR;
    }
    // Assign the cmd line values or use the default values.
    m_SliLocalSurfaces =
        m_Params->ParamUnsigned("-sli_local_surfaces", 100);
    InfoPrintf("%s: Setting m_SliLocalSurfaces= %d\n",
        __FUNCTION__, m_SliLocalSurfaces);

    m_SliRemoteSurfaces =
        m_Params->ParamUnsigned("-sli_remote_surfaces", 0);
    InfoPrintf("%s: Setting m_SliRemoteSurfaces= %d\n",
        __FUNCTION__, m_SliRemoteSurfaces);

    // no need to check args   // HACK: Waiting for the levels to be updated to run Milestone 1.

#if 0
    if (_DMA_TARGET_P2P == (_DMA_TARGET)m_Params->ParamUnsigned("-loc_tex", _DMA_TARGET_VIDEO))
    {
        m_SliLocalSurfaces = 0;
        m_SliRemoteSurfaces = 100;
    }
#endif

    if (m_SliLocalSurfaces + m_SliRemoteSurfaces == 0)
    {
        ErrPrintf("%s: ERROR -sli_local_surfaces and "
            "-sli_remote_surfaces cannot be both 0.\n",
            __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    m_SliSeed = m_Params->ParamUnsigned("-sli_seed", 0xdeadd1a8);
    InfoPrintf("%s: Setting SliSeed= %u\n", __FUNCTION__, m_SliSeed);

    InfoPrintf("%s: IsMilestone1Mode= %s\n", __FUNCTION__,
            Bool2String(IsMilestone1Mode()));
    InfoPrintf("%s: IsMilestone2Mode= %s\n", __FUNCTION__,
            Bool2String(IsMilestone2Mode()));
    InfoPrintf("%s: IsMilestone3Mode= %s\n", __FUNCTION__,
            Bool2String(IsMilestone3Mode()));
    return rc;
}

int SliSurfaceMapper::GetSurfaceNumber(GpuDevice* pGpuDev) const
{
    MASSERT(pGpuDev);
    if (m_GpuDeviceInfo.find(pGpuDev) == m_GpuDeviceInfo.end())
    {
        InfoPrintf("%s Warning: GpuDevice %p not registered with "
                "SliSurfaceMapper\n", __FUNCTION__, pGpuDev);
        return 0;
    }
    const PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(pGpuDev)->second;
    return pgdi.m_DmaBufferSet.size() + pgdi.m_RenderSurfaceSet.size();
}

bool SliSurfaceMapper::IsSliSystem(GpuDevice* pGpuDev) const
{
    return pGpuDev->GetNumSubdevices() > 1;
}

bool SliSurfaceMapper::IsMilestone1Mode() const
{
    // If the user specifies that SliRemoteSurfaces == 0 -> Milestone1 mode.
    return (m_SliRemoteSurfaces == 0);
}

bool SliSurfaceMapper::IsMilestone2Mode() const
{
    // If the user specifies that SliRemoteSurfaces != 0 -> Milestone2 mode.
    return (m_SliRemoteSurfaces != 0);
}

bool SliSurfaceMapper::IsMilestone3Mode() const
{
    return IsMilestone2Mode();
}

// //////////////////////////////////////////////////////////////////////////
void SliSurfaceMapper::AddDmaBuffer(GpuDevice* pGpuDevMods,
                                    MdiagSurf* pSurf,
                                    const Accessing2Hosting& a2h,
                                    bool canBePartialRemote)
{
    AddDmaBuffer(pGpuDevMods, pSurf, canBePartialRemote);
    GpuDeviceInfo_type::iterator pGpuDev = m_GpuDeviceInfo.find(pGpuDevMods);
    PerGpuDeviceInfo& pgdi = pGpuDev->second;
    pgdi.m_RenderSurfaceMap.insert(make_pair(pSurf, a2h));
}

// //////////////////////////////////////////////////////////////////////////
void SliSurfaceMapper::AddDmaBuffer(GpuDevice* pGpuDev,
                                    MdiagSurf* pSurf,
                                    bool canBePartialRemote)
{
    MASSERT(pGpuDev);
    MASSERT(pSurf);

    if (m_GpuDeviceInfo.find(pGpuDev) == m_GpuDeviceInfo.end())
    {
        // First surface registered by this Gpu device.
        PerGpuDeviceInfo pgdi(pGpuDev);
        pgdi.m_DmaBufferSet.insert(pSurf);
        m_GpuDeviceInfo.insert(make_pair(pGpuDev, pgdi));
    }
    else
    {
        PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(pGpuDev)->second;
        if (pgdi.m_DmaBufferSet.find(pSurf) !=
            pgdi.m_DmaBufferSet.end())
        {
            InfoPrintf("%s: Warning you are pushing multiple times the "
                       "same DmaBuffer\n", __FUNCTION__);
        }
        pgdi.m_DmaBufferSet.insert(pSurf);
    }

    if (m_CanBePartialRemote.find(pSurf) != m_CanBePartialRemote.end())
    {
        // Some files with same type share one MdiagSurf instance, if one
        // file has been pushed into map and it cannot do partial remote
        // mapping, all files associated to that MdiagSurf instance are
        // not allowed to be partial remote
        if (m_CanBePartialRemote[pSurf])
        {
            m_CanBePartialRemote[pSurf] = canBePartialRemote;
        }
    }
    else
    {
        m_CanBePartialRemote[pSurf] = canBePartialRemote;
    }
}

void SliSurfaceMapper::AddLWSurface(GpuDevice* pGpuDev,
                                    MdiagSurf* pSurf)
{
    MASSERT(pGpuDev);
    MASSERT(pSurf);
    if (m_GpuDeviceInfo.find(pGpuDev) == m_GpuDeviceInfo.end())
    {
        // First surface registered by this Gpu device.
        PerGpuDeviceInfo pgdi(pGpuDev);
        pgdi.m_RenderSurfaceSet.insert(pSurf);
        m_GpuDeviceInfo.insert(make_pair(pGpuDev, pgdi));
    }
    else
    {
        PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(pGpuDev)->second;
        if (pgdi.m_RenderSurfaceSet.find(pSurf) !=
            pgdi.m_RenderSurfaceSet.end())
        {
            InfoPrintf("%s: Warning you are pushing multiple times the "
                       "same render Surface\n", __FUNCTION__);
        }
        pgdi.m_RenderSurfaceSet.insert(pSurf);
    }
    m_CanBePartialRemote[pSurf] = true;
}

UINT32 SliSurfaceMapper::GetHostingSubdevice
(
    MdiagSurf *surf,
    GpuDevice *gpuDev,
    UINT32     accessingSubdevice
) const
{
    MASSERT(surf);
    MASSERT(gpuDev);
    MASSERT(accessingSubdevice != ~0x0U);
    MASSERT(accessingSubdevice < gpuDev->GetNumSubdevices());

    UINT32 hostingSubdevice = ~0x0U;

    // If not peer-able it simply returns a local subdev
    if (!surf->HasMap() ||
        (surf->GetAllocSize() == 0) ||
        !surf->IsPeerMapped())
    {
        hostingSubdevice = accessingSubdevice;
    }
    else
    {

        // The GPU device must have been registered
        MASSERT(m_GpuDeviceInfo.find(gpuDev) != m_GpuDeviceInfo.end());
        const PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(gpuDev)->second;

        // The surface must have been pushed
        PerGpuDeviceInfo::SurfaceMap::const_iterator surfIt =
            pgdi.m_DmaBufferMap.find(surf);
        if (surfIt == pgdi.m_DmaBufferMap.end())
        {
            surfIt = pgdi.m_RenderSurfaceMap.find(surf);
            MASSERT(surfIt != pgdi.m_RenderSurfaceMap.end());
        }
        const PerGpuDeviceInfo::Accessing2Hosting& a2h = surfIt->second;
        // The accessing subdevice must be valid and registered.
        MASSERT(a2h.find(accessingSubdevice) != a2h.end());
        hostingSubdevice = a2h.find(accessingSubdevice)->second;
        MASSERT(hostingSubdevice < gpuDev->GetNumSubdevices());
    }
    DebugPrintf(MSGID(), "SLI access mapping: %s %d->%d\n", surf->GetName().c_str(),
        accessingSubdevice, hostingSubdevice);

    return hostingSubdevice;
}

UINT32 SliSurfaceMapper::GetAccessingSubdevice
(
    MdiagSurf *surf,
    GpuDevice *gpuDev,
    UINT32     hostingSubdevice
) const
{
    MASSERT(surf);
    MASSERT(gpuDev);
    MASSERT(hostingSubdevice != ~0x0U);
    MASSERT(hostingSubdevice < gpuDev->GetNumSubdevices());

    UINT32 accessingSubdevice = ~0x0U;

    // If not peer-able it simply returns a local subdev
    if (!surf->HasMap() ||
        (surf->GetAllocSize() == 0) ||
        !surf->IsPeerMapped())
    {
        accessingSubdevice = hostingSubdevice;
    }
    else
    {
        // The GPU device must have been registered
        MASSERT(m_GpuDeviceInfo.find(gpuDev) != m_GpuDeviceInfo.end());
        const PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(gpuDev)->second;

        // The surface must have been pushed
        PerGpuDeviceInfo::SurfaceMap::const_iterator surfIt =
            pgdi.m_DmaBufferMap.find(surf);
        if (surfIt == pgdi.m_DmaBufferMap.end())
        {
            surfIt = pgdi.m_RenderSurfaceMap.find(surf);
            MASSERT(surfIt != pgdi.m_RenderSurfaceMap.end());
        }
        const PerGpuDeviceInfo::Accessing2Hosting& a2h = surfIt->second;

        for (auto a2hIter = a2h.begin(); a2hIter != a2h.end(); ++a2hIter)
        {
            if (a2hIter->second == hostingSubdevice)
            {
                // MODS can't support more than one subdevice pointing
                // to the same surface on a single subdevice.
                MASSERT(accessingSubdevice == ~0x0U);

                accessingSubdevice = a2hIter->first;
                MASSERT(accessingSubdevice < gpuDev->GetNumSubdevices());
            }
        }
    }
    DebugPrintf(MSGID(), "SLI access mapping: %s %d->%d\n", surf->GetName().c_str(),
        accessingSubdevice, hostingSubdevice);

    return accessingSubdevice;
}

// //////////////////////////////////////////////////////////////////////////
UINT64 SliSurfaceMapper::GetCtxDmaOffset(MdiagSurf* surf,
    GpuDevice* gpuDev,
    UINT32 accessingSubdevice) const
{
    const UINT32 hostingSubdevice = GetHostingSubdevice(surf, gpuDev,
        accessingSubdevice);

    UINT64 addr = 0;
    if (hostingSubdevice == accessingSubdevice)
    {
        addr = surf->GetCtxDmaOffsetGpu();
    }
    else
    {
        addr = surf->GetCtxDmaOffsetGpuPeer(hostingSubdevice);
    }

    return addr;
}

// //////////////////////////////////////////////////////////////////////////
static string LeftJustified(const char* str, size_t length)
{
    return str;
#if 0
    // Build the format string.
    const int fmtSize = 8;
    char* fmt = new char[fmtSize];
    snprintf(fmt, fmtSize, "%%%ds", length);
    // Print left justified.
    char* tmp = new char[length+1];
    snprintf(tmp, length+1, fmt, str);
    delete []tmp;
    delete []fmt;
    return tmp;
#endif
}

static string GpuName(int d)
{
    return Utility::StrPrintf("GPU%d", d);
}

RC SliSurfaceMapper::AllocateSurfaces(GpuDevice* pGpuDev_)
{
    InfoPrintf("%s: Entering", __FUNCTION__);
    MASSERT(pGpuDev_);
    GpuDeviceInfo_type::iterator pGpuDev = m_GpuDeviceInfo.find(pGpuDev_);
    MASSERT(pGpuDev != m_GpuDeviceInfo.end());

    RC rc = OK;
    // The algorithm implemented here is (as in bug 289645):
    //
    // // normalize the distribution, if not already normalized
    // peer_distribution.normalize()
    // foreach gpu in gpu_set
    //   foreach surface in surface_list
    //     surface_location = sample(peer_distribution)
    //     if (surface_location == "remote")
    //       // pick a random gpu to use as remote excluding current gpu
    //       surface_location[gpu] = rand(gpu_set - {gpu} )
    //     else
    //       surface.location[gpu] = gpu

    // Define a fancy picker context.
    FancyPicker::FpContext fpCtx_local;
    fpCtx_local.Rand.SeedRandom(m_SliSeed);
    fpCtx_local.RestartNum = 0;
    fpCtx_local.LoopNum = 0;
    fpCtx_local.pJSObj = NULL;
    // Define the local/remote picker.
    FancyPicker fp_local;
    fp_local.SetContext(&fpCtx_local);
    rc = fp_local.ConfigRandom();
    if (rc != OK)
    {
        InfoPrintf("%s: ConfigRandom failed", __FUNCTION__);
        return rc;
    }
    enum { LOCAL = 0, REMOTE };
    fp_local.AddRandItem(m_SliLocalSurfaces, LOCAL);
    fp_local.AddRandItem(m_SliRemoteSurfaces, REMOTE);
    fp_local.CompileRandom();

    int numSubdevices = pGpuDev->first->GetNumSubdevices();
    if (numSubdevices <= 1)
    {
        ErrPrintf("trace_3d: The GPU device %p is not in SLI mode\n",
            pGpuDev->first);
        return RC::SOFTWARE_ERROR;
    }
    DebugPrintf(MSGID(), "%s: Allocating surfaces of GpuDevice= %p with %d "
            "subdevices\n",
            __FUNCTION__, pGpuDev->first, numSubdevices);
    PerGpuDeviceInfo& pgdi = pGpuDev->second;

    // ILW: first time is allocated.
    MASSERT(pgdi.m_DmaBufferMap.size() == 0);
    MASSERT(pgdi.m_RenderSurfaceMap.size() == 0);

    // Allocation table info.
    const size_t surfCols = 15;
    const size_t gpuCols = 10;
    vector<string> lines;

    // Build the first line of the table.
    string line = LeftJustified("Surface", surfCols);
    for (int accessingGpuSubdevice = 0;
        accessingGpuSubdevice < numSubdevices; ++accessingGpuSubdevice)
    {
        line += LeftJustified(GpuName(accessingGpuSubdevice).c_str(),
                              gpuCols);
    }
    line += "\n";
    lines.push_back(line);

    bool isPeeredSurface = false;
    Random  random_gen;
    random_gen.SeedRandom(m_SliSeed);

    // Create a map for each DmaBuffer.
    for (set<MdiagSurf*>::const_iterator pSurf =
         pgdi.m_DmaBufferSet.begin();
         pSurf != pgdi.m_DmaBufferSet.end(); ++pSurf)
    {
        // DmaBuffers have no name so we use the virtual address.
        char tmp[surfCols];
        snprintf(tmp, surfCols, "0x%08x:", (*pSurf)->GetMemHandle());
        // Write the name right-justified.
        string line = LeftJustified(tmp, surfCols);

        PerGpuDeviceInfo::Accessing2Hosting a2h;
        vector<UINT32> fp_list(numSubdevices);
        bool canBePartialRemote = m_CanBePartialRemote[*pSurf];
        // Scan the accessing subdevices.
        for (UINT32 accessingGpuSubdevice = 0;
             accessingGpuSubdevice < (UINT32)numSubdevices; ++accessingGpuSubdevice)
        {
            UINT32 hostingGpuSubdevice = accessingGpuSubdevice;

            if (((*pSurf)->GetLocation() == Memory::Fb) &&
                ((*pSurf)->GetMemHandle() != 0))
            {

                if ((((*pSurf)->GetProtect() == Memory::Readable) &&
                    canBePartialRemote == true) ||
                    (accessingGpuSubdevice == 0))
                {
                    // For readonly buffers or the buffers on the first subdevice,
                    // it could be either remote or local
                    // For those surfaces that will be relocated, we don't allow
                    // them to be partial remote accessed since the contents patched
                    // to those surfaces are not deterministic
                    if (fp_local.Pick() == REMOTE)
                    {
                        CreateRandomList( fp_list, random_gen, numSubdevices );
                        hostingGpuSubdevice = fp_list[accessingGpuSubdevice];
                        isPeeredSurface = true;
                    }
                }
                else if (a2h[0] != 0) // writable buffers on subdev other than 0
                {
                    // For writable buffers we have to generate symmetric traffic,
                    // ie if subdev0 access remote, all the rest subdevices should
                    // access remote. vice versa.
                    hostingGpuSubdevice = fp_list[accessingGpuSubdevice];
                    isPeeredSurface = true;
                }
            }

            line += LeftJustified(GpuName(hostingGpuSubdevice).c_str(),
                gpuCols);
            a2h.insert(make_pair(accessingGpuSubdevice,
                hostingGpuSubdevice));
            // Set surface location info local/remote
            MASSERT(accessingGpuSubdevice < 8);
            char gpuTag = accessingGpuSubdevice + '0';
            string strName = (*pSurf)->GetName() + '(' + gpuTag + ')';
            BuffAccessInfo info = {accessingGpuSubdevice, hostingGpuSubdevice, *pSurf};
            m_Name2Buff[strName] = info;
        }
        pgdi.m_DmaBufferMap.insert(make_pair(*pSurf, a2h));
        // Update the text table.
        line += "\n";
        lines.push_back(line);
    } // pSurf

    // Create a map for each Surface.
    for (set<MdiagSurf*>::const_iterator pSurf =
         pgdi.m_RenderSurfaceSet.begin();
         pSurf != pgdi.m_RenderSurfaceSet.end(); ++pSurf)
    {
        // Use the type name for Surfaces.
        char tmp[surfCols];
        snprintf(tmp, surfCols, "%s:", (*pSurf)->GetName().c_str());
        // Write the name right-justified.
        string line = LeftJustified(tmp, surfCols);

        PerGpuDeviceInfo::Accessing2Hosting a2h;
        vector<UINT32> fp_list(numSubdevices);
        bool canBePartialRemote = m_CanBePartialRemote[*pSurf];
        // Scan the accessing subdevices.
        for (UINT32 accessingGpuSubdevice = 0;
             accessingGpuSubdevice < (UINT32)numSubdevices; ++accessingGpuSubdevice)
        {
            UINT32 hostingGpuSubdevice = accessingGpuSubdevice;

            if (IsSuitableForPeerMemory(*pSurf))
            {

                if ((((*pSurf)->GetProtect() == Memory::Readable) &&
                    canBePartialRemote == true) ||
                    (accessingGpuSubdevice == 0))
                {
                    // For readonly buffers, it could be either remote or local
                    // For those surfaces that will be relocated, we don't allow
                    // them to be partial remote accessed since the contents patched
                    // to those surfaces are not deterministic
                    if (fp_local.Pick() == REMOTE)
                    {
                        CreateRandomList( fp_list, random_gen, numSubdevices );
                        hostingGpuSubdevice = fp_list[accessingGpuSubdevice];
                        isPeeredSurface = true;
                    }
                }
                else if (a2h[0] != 0)
                {
                    // For writable buffers we have to generate symmetric traffic,
                    // ie if subdev0 access remote, all the rest subdevices should
                    // access remote. vice versa.
                    hostingGpuSubdevice = fp_list[accessingGpuSubdevice];
                    isPeeredSurface = true;
                }
            }
            line += LeftJustified(GpuName(hostingGpuSubdevice).c_str(),
                gpuCols);
            a2h.insert(make_pair(accessingGpuSubdevice,
                                 hostingGpuSubdevice));
            // Set surface location info local/remote
            char gpuTag = accessingGpuSubdevice + '0';
            string strName = (*pSurf)->GetName() + '(' + gpuTag + ')';
            BuffAccessInfo info = {accessingGpuSubdevice, hostingGpuSubdevice, *pSurf};
            m_Name2Buff[strName] = info;
        }
        pgdi.m_RenderSurfaceMap.insert(make_pair(*pSurf, a2h));
        // Update the text table.
        line += "\n";
        lines.push_back(line);
    } // pSurf

    rc = pgdi.MapPeer();
    if (rc != OK)
    {
        ErrPrintf("MapPeer on %p pgdi failed\n", &pgdi);
        return rc;
    }
    InfoPrintf("%s: After MapPeer\n", __FUNCTION__);
    Print();

    // Print the allocation table.
    InfoPrintf("%s: Alloc P2P buffers:\n", __FUNCTION__);
    //for (size_t i = 0; i < lines.size(); ++i) {
    //    InfoPrintf("%s", lines[i].c_str());
    //}
    InfoPrintf("IsPeeredSurface= %s:\n",
        (isPeeredSurface ? "True" : "False"));

    InfoPrintf("%s: MapPeer done\n", __FUNCTION__);
    return rc;
}

void SliSurfaceMapper::Print(GpuDevice* pGpuDev) const
{
    MASSERT(pGpuDev);
    InfoPrintf("sli_local_surfaces= %u, sli_remote_surfaces= %u, "
        "sli_seed= %u\n", m_SliLocalSurfaces, m_SliRemoteSurfaces,
        m_SliSeed);
    // The GPU device must have been registered.
    MASSERT(m_GpuDeviceInfo.find(pGpuDev) != m_GpuDeviceInfo.end());
    const PerGpuDeviceInfo& pgdi = m_GpuDeviceInfo.find(pGpuDev)->second;
    pgdi.Print();
}

void SliSurfaceMapper::Print() const
{
    for (GpuDeviceInfo_type::const_iterator pGpuDev = m_GpuDeviceInfo.begin();
         pGpuDev != m_GpuDeviceInfo.end(); ++pGpuDev)
    {
        const PerGpuDeviceInfo& pgdi = pGpuDev->second;
        pgdi.Print();
    }
}

// //////////////////////////////////////////////////////////////////////////

void SliSurfaceMapper::DumpMemory(void* ptr, UINT32 byteSize) const {
    DebugPrintf(MSGID(), "%s(ptr= %p, byteSize= 0x%x)\n", __FUNCTION__,
            ptr, byteSize);
    char* ptr_ = static_cast<char*>(ptr);
    string line = "";
    for (unsigned int i = 0; i < byteSize; ++i) {
        UINT08 data = Platform::VirtualRd08(ptr_ + i);
        char buf[1024];
        sprintf(buf, "0x%x ", data);
        line += buf;
        if (i != 0 && i % 8 == 0) {
            DebugPrintf(MSGID(), "%s\n", line.c_str());
            line = "";
        }
    }
}

//
// CreateRandomList will generate a random list of number from 0..size-1, and
// guarantee that for any location i, the number is not i.
//
void SliSurfaceMapper::CreateRandomList( vector<UINT32>& rlist,
    Random& rad_obj, UINT32 size) const
{
    MASSERT(size > 1);
    rlist.resize( size );

    if (size == 2)
    {
        // Only one choice
        rlist[0] = 1;
        rlist[1] = 0;
        return;
    }

    UINT32 i;

    // first fill the array with numbers
    for (i=0; i<size; ++i)
    {
        rlist[i] = i;
    }

    // Now for each location, shuffle the data until this location
    // get a value differnet from the location number
    i = 0;
    while (i < (size - 1))
    {
        if (rlist[i] == i)
        {
            rad_obj.Shuffle(size-i, &(rlist[i]));
        }
        else
        {
            ++i;
        }
    }

    if (rlist[size-1] == (size-1))
    {
        // Special handling for last location
        rlist[size-1] = rlist[size-2];
        rlist[size-2] = size-1;
    }
}
