/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "bufferdumper.h"
#include "lwos.h"
#include "mdiag/gpu/zlwll.h"
#include "tracemod.h"
#include "trace_3d.h"
#include "mdiag/resource/lwgpu/dmasurfr.h"
#include "gpu/utility/surfrdwr.h"
#include "fermi/gf100/dev_ram.h"

#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/utils/mdiagsurf.h"

RenderTarget::RenderTarget(Trace3DTest *test, bool isZeta, UINT32 rtNumber)
{
    m_Test = test;
    m_Standard = 0;
    m_Rtt = 0;
    m_IsRtt = false;
    m_IsZeta = isZeta;
    m_Enabled = true;
    m_InitialSetup = true;
    m_RtNumber = rtNumber;
    m_SelectedAs = 0;

    // Surface properties
    m_OffsetUpper = 0;
    m_OffsetLower = 0;
    m_Width = 0;
    m_Height = 0;
    m_Depth = 1;
    m_ColorFormat = 0;
    m_LogBlockWidth = 0;
    m_LogBlockHeight = 0;
    m_LogBlockDepth = 0;
    m_ArrayPitch = 0;
    m_Layout = (UINT32)Surface2D::BlockLinear;
    m_ArraySize = 1;
    m_WriteEnabled = true;
    m_IsSettingDepth = true;

    // By default, we use data in SurfMgr
    m_OverwriteWidth = false;
    m_OverwriteHeight = false;
    m_OverwriteDepth = false;
    m_OverwriteColorFormat = false;
    m_OverwriteLogBlockWidth = false;
    m_OverwriteLogBlockHeight = false;
    m_OverwriteLogBlockDepth = false;
    m_OverwriteArrayPitch = false;
    m_OverwriteLayout = false;
    m_OverwriteArraySize = false;
}

RenderTarget::~RenderTarget()
{
    if (m_Standard)
    {
        m_Standard->SetDmaBufferAlloc(true); // No free call needed
        delete m_Standard;
    }
    if (m_Rtt)
    {
        m_Rtt->SetDmaBufferAlloc(true); // No free call needed
        delete m_Rtt;
    }
}

MdiagSurf* RenderTarget::Create()
{
    return Create((MdiagSurf*)0);
}

MdiagSurf* RenderTarget::Create(MdiagSurf *surf)
{
    MdiagSurf **rt = m_IsRtt ? &m_Rtt : &m_Standard;

    if (!(*rt) ||
        (*rt)->GetCtxDmaOffsetGpu() != surf->GetCtxDmaOffsetGpu())
    {
        Delete(); // Ilwalidate old surface
        if (surf) *rt = new MdiagSurf(*surf);
        else      *rt = new MdiagSurf();

        if (!(*rt))
            ErrPrintf("RenderTarget::Create(): Can not allocate surface\n");
    }
    return *rt;
}

void RenderTarget::Delete()
{
    MdiagSurf **rt = m_IsRtt ? &m_Rtt : &m_Standard;
    if (*rt)
    {
        (*rt)->SetDmaBufferAlloc(true); // No free call needed
        delete *rt;
        *rt = 0;
    }
}

UINT32 RenderTarget::GetMaxSize() const
{
    UINT32 std = m_Standard ? (UINT32)m_Standard->GetSize() : 0;
    UINT32 rtt = m_Rtt ? (UINT32)m_Rtt->GetSize() : 0;

    return max(std, rtt);
}

RC RenderTarget::LoadSurface()
{
    UINT64 offset = ((UINT64)m_OffsetUpper << 32) | m_OffsetLower;
    IGpuSurfaceMgr *surfmgr = m_Test->GetSurfaceMgr();
    MdiagSurf *std_rt = 0;
    if (GetIsZeta() &&
        surfmgr->GetValid(surfmgr->GetSurface(SURFACE_TYPE_Z, 0)))
    {
        std_rt = surfmgr->GetSurface(SURFACE_TYPE_Z, 0);
    }
    else if (!GetIsZeta() && surfmgr->GetValid(surfmgr->GetSurface(
        ColorSurfaceTypes[m_RtNumber], 0)))
    {
        std_rt = surfmgr->GetSurface(ColorSurfaceTypes[m_RtNumber], 0);
    }

    if (std_rt != 0 &&
        ((m_InitialSetup && offset == 0) ||
          offset == std_rt->GetCtxDmaOffsetGpu()))
    {
        // In 2 cases we'll clone RT from standard RT
        //
        // 1) RT methods are injected in SetupFermi/Tesla, and by default
        // they are all standard RTs, so we don't need to capture the
        // methods in that function, just initialize RT from SurfMgr
        //
        // 2) Trace explicitly Set the offset in pb at run time
        m_InitialSetup = false;
        // Get the offset injected in Setup function
        m_OffsetUpper = std_rt->GetCtxDmaOffsetGpu() >> 32;
        m_OffsetLower = std_rt->GetCtxDmaOffsetGpu() & 0xffffffff;
        SetIsRtt(false);
        if (Create(std_rt) == 0)
        {
            ErrPrintf("RenderTarget::LoadSurface: Can't create surface\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        MdiagSurf *dma = m_Test->LocateTextureBufByOffset(offset);
        if (dma == 0)
        {
            if (m_ColorFormat != 0 && m_WriteEnabled == true)
            {
                // ACE traces often sets 8 RTs while some are disabled by format==DISABLE
                // or WriteMask==0, we just skip that, no need to output error message
                ErrPrintf("RenderTarget::LoadSurface: Trying to dump an invalid surface\n");
            }
            return RC::SOFTWARE_ERROR;
        }
        SetIsRtt(true);
        if (Create((MdiagSurf*)dma) == 0)
        {
            ErrPrintf("RenderTarget::LoadSurface: Can't create surface\n");
            return RC::SOFTWARE_ERROR;
        }
        // RTT case, surface attributes is controled by trace
        MdiagSurf *surf = GetActive();
        if (m_OverwriteWidth) surf->SetWidth(m_Width);
        if (m_OverwriteHeight) surf->SetHeight(m_Height);
        if (m_OverwriteDepth) surf->SetDepth(m_Depth);
        if (m_OverwriteColorFormat) surf->SetColorFormat((ColorUtils::Format)m_ColorFormat);
        if (m_OverwriteLogBlockWidth) surf->SetLogBlockWidth(m_LogBlockWidth);
        if (m_OverwriteLogBlockHeight) surf->SetLogBlockHeight(m_LogBlockHeight);
        if (m_OverwriteLogBlockDepth) surf->SetLogBlockDepth(m_LogBlockDepth);
        if (m_OverwriteArrayPitch) surf->SetArrayPitch(m_ArrayPitch);
        if (m_OverwriteLayout) surf->SetLayout((Surface2D::Layout)m_Layout);
        if (m_OverwriteArraySize) surf->SetArraySize(m_ArraySize);
    }

    return OK;
}

BufferDumper::BufferDumper(
    Trace3DTest *test,
    const ArgReader *params
)
{
    m_Test = test;
    m_params = params;
    m_Zt = NULL;
    m_CtCount = 0;
    m_ZtCount = 1;

    m_LogInfoEveryEnd = m_params->ParamPresent("-log_info_every_end") > 0;
    m_DumpImageEveryEnd = m_params->ParamPresent("-dump_image_every_end") > 0;
    m_DumpZlwllEveryEnd = m_params->ParamPresent("-dump_zlwll_every_end") > 0;
    m_DumpImageEveryBegin = m_params->ParamPresent("-dump_image_every_begin") > 0;

    UINT32 count = m_params->ParamPresent("-dump_image_every_n_begin");
    if (count > 0)
    {
        for (UINT32 i = 0; i < count; i++)
        {
            m_DumpEveryNBegin.push_back(
                m_params->ParamNUnsigned("-dump_image_every_n_begin", i, 0));
        }
    }

    count = m_params->ParamPresent("-dump_image_every_n_end");
    if (count > 0)
    {
        for (UINT32 i = 0; i < count; i++)
        {
            m_DumpEveryNEnd.push_back(
                m_params->ParamNUnsigned("-dump_image_every_n_end", i, 0));
        }
    }

    count = m_params->ParamPresent("-dump_image_range_begin");
    bool wrong_arg = false;
    if (count > 0)
    {
        for (UINT32 i = 0; i < count; i++)
        {
            string range = m_params->ParamNStr("-dump_image_range_begin", i, 0);
            UINT32 min = (UINT32)-1;
            UINT32 max = (UINT32)-1;
            if (range[0] == '-')
            {
                min = 0;
                if (sscanf(range.c_str(), "-%d", &max) != 1)
                {
                    wrong_arg = true;
                }
            }
            else if (range[range.size()-1] == '-')
            {
                max = (UINT32)-1;
                if (sscanf(range.c_str(), "%d-", &min) != 1)
                {
                    wrong_arg = true;
                }
            }
            else
            {
                if (sscanf(range.c_str(), "%d-%d", &min, &max) != 2 || min > max)
                {
                    wrong_arg = true;
                }
            }
            if (wrong_arg)
            {
                ErrPrintf("Unexpected -dump_image_range_begin arg value "
                    "format, need: ##-##/-##/##-\n");
                MASSERT(0);
            }
            m_DumpRangeMinBegin.push_back(min);
            m_DumpRangeMaxBegin.push_back(max);
        }
    }
    count = m_params->ParamPresent("-dump_image_range_end");
    if (count > 0)
    {
        for (UINT32 i = 0; i < count; i++)
        {
            string range = m_params->ParamNStr("-dump_image_range_end", i, 0);
            UINT32 min = (UINT32)-1;
            UINT32 max = (UINT32)-1;
            if (range[0] == '-')
            {
                min = 0;
                if (sscanf(range.c_str(), "-%d", &max) != 1)
                {
                    wrong_arg = true;
                }
            }
            else if (range[range.size()-1] == '-')
            {
                max = (UINT32)-1;
                if (sscanf(range.c_str(), "%d-", &min) != 1)
                {
                    wrong_arg = true;
                }
            }
            else
            {
                if (sscanf(range.c_str(), "%d-%d", &min, &max) != 2 || min > max)
                {
                    wrong_arg = true;
                }
            }
            if (wrong_arg)
            {
                ErrPrintf("Unexpected -dump_image_range_end arg value "
                    "format, need: ##-##/-##/##-\n");
                MASSERT(0);
            }
            m_DumpRangeMinEnd.push_back(min);
            m_DumpRangeMaxEnd.push_back(max);
        }
    }

    m_BeginMethodCount = 0;
    m_EndMethodCount = 0;

    const UINT32 argOclwrences = params->ParamPresent("-dump_image_every_method");
    for (UINT32 i = 0; i < argOclwrences; ++i)
    {
        const UINT32 classId= params->ParamNUnsigned("-dump_image_every_method", i, 0);
        const UINT32 methodAddr = params->ParamNUnsigned("-dump_image_every_method", i, 1);
        m_HookedMethodMap.insert(pair<UINT32, UINT32>(classId, methodAddr));
    }
}

MdiagSurf* RenderTarget::GetActive() const
{
    if (m_IsRtt)
    {
        // Need to call ComputeParams() to get all parameter correctly initialized
        // as this buffer will not be really allocated.
        m_Rtt->ComputeParams();
        return m_Rtt;
    }

    return m_Standard;
}

BufferDumper::~BufferDumper()
{
    for (int i=0; i<MAX_RENDER_TARGETS; i++)
        delete m_Ct[i];
    delete m_Zt;
}

BufferDumperFermi::BufferDumperFermi(
    Trace3DTest *test,
    const ArgReader *params
): BufferDumper(test, params)
{
    for (int i=0; i<MAX_RENDER_TARGETS; i++)
    {
        m_Ct[i] = new RenderTarget(test, false, i);
        m_Ct[i]->SetSelect(0);
        m_Ct[i]->SetWriteEnabled(false);
    }
    m_Ct[0]->SetWriteEnabled(true);
    m_Zt = new RenderTarget(test, true, 0);
}

BufferDumperFermi::~BufferDumperFermi()
{
}

void BufferDumper::Execute(UINT32 ptr, UINT32 method_offset,
    CachedSurface *cs, const string &name, UINT32 pbSize, UINT32 class_id)
{
    GpuTrace *trace = m_Test->GetTrace();
    int surf_num = 0;

    if (this->IsEndOp(class_id, method_offset))
    {
        string file_name_postfix("batch_end");

        if (NeedToDumpLwrrEnd())
        {
            DumpSurfaceToFile(file_name_postfix);
        }

        TraceModules::iterator bufiter = trace->GetDumpBufferEndList()->begin();
        TraceModules::iterator bufiterend = trace->GetDumpBufferEndList()->end();
        for (; bufiter != bufiterend; ++bufiter)
        {
            // BufferDumper object is shared for all TraceModule objects
            string fname = (*bufiter)->GetName();
            fname.insert(fname.rfind('.'), "_end");
            DumpBufferToFile((*bufiter)->GetCachedSurface(),
                             (*bufiter)->GetDmaBufferNonConst(),
                             fname);
        }

        if (m_DumpZlwllEveryEnd)
        {
            DumpZlwllToFile(0);
        }
        else if (!trace->GetDumpZlwllList()->empty())
        {
            DumpZlwllToFile(trace->GetDumpZlwllList());
        }

        DumpBufferMaps_t::iterator bufmapend = trace->GetDumpBufferEndMaps()->end();
        DumpBufferMaps_t::iterator& bufmapiter = *(trace->GetDumpBufferEndMapsIter());
        while ((bufmapiter != bufmapend) && (bufmapiter->first == m_EndMethodCount))
        {
            string name = bufmapiter->second->GetName();
            string::size_type pos = name.rfind('.');
            name.insert(pos, "_end");
            DumpBufferToFile(bufmapiter->second->GetCachedSurface(),
                             bufmapiter->second->GetDmaBufferNonConst(),
                             name);
            ++bufmapiter;
        }

        m_EndMethodCount++;

        if (m_LogInfoEveryEnd)
        {
            InfoPrintf("Hit end methods\n");
        }
    }
    else if (IsBeginOp(class_id, method_offset))
    {
        string file_name_postfix("batch_begin");

        if (NeedToDumpLwrrBegin())
        {
            DumpSurfaceToFile(file_name_postfix);
        }

        TraceModules::iterator bufiter = trace->GetDumpBufferList()->begin();
        TraceModules::iterator bufiterend = trace->GetDumpBufferList()->end();
        for (; bufiter != bufiterend; ++bufiter)
        {
            // BufferDumper object is shared for all TraceModule objects
            DumpBufferToFile((*bufiter)->GetCachedSurface(),
                             (*bufiter)->GetDmaBufferNonConst(),
                             (*bufiter)->GetName());
        }

        DumpBufferMaps_t::iterator bufmapend = trace->GetDumpBufferBeginMaps()->end();
        DumpBufferMaps_t::iterator& bufmapiter = *(trace->GetDumpBufferBeginMapsIter());
        while ((bufmapiter != bufmapend) && (bufmapiter->first == m_BeginMethodCount))
        {
            string name = bufmapiter->second->GetName();
            string::size_type pos = name.rfind('.');
            name.insert(pos, "_begin");
            DumpBufferToFile(bufmapiter->second->GetCachedSurface(),
                             bufmapiter->second->GetDmaBufferNonConst(),
                             name);
            ++bufmapiter;
        }
        m_BeginMethodCount++;
    }
    else if (IsSetColorTargetOp(class_id, method_offset, &surf_num))
        SetActiveCSurface(cs, ptr, surf_num, pbSize);
    else if (IsSetZetaTargetOp(class_id, method_offset))
        SetActiveZSurface(cs, ptr, pbSize);
    else if (IsSetCtCtrlOp(class_id, method_offset))
        SetCtCtrl(method_offset, cs, ptr, pbSize);

    if (IsHookedOp(class_id, method_offset))
    {
        static map<string, int> methodOclwrrences;
        const string prefix = Utility::StrPrintf("0x%x_0x%x", class_id, method_offset);
        ++methodOclwrrences[prefix];
        DumpSurfaceToFile(Utility::StrPrintf("%s_%04d", prefix.c_str(), methodOclwrrences[prefix]));
    }
}

void BufferDumper::DumpSurfaceToFile(const string &file_name_postfix)
{
    // dump drawcall only supported for graphic channel.
    TraceChannel *pTraceChannel = m_Test->GetGrChannel();
    MASSERT(pTraceChannel->GetGrSubChannel());
    LWGpuChannel *pCh = pTraceChannel->GetCh();
    GpuVerif *gpuverif = pTraceChannel->GetGpuVerif();
    UINT32 subDevNum = m_Test->GetBoundGpuDevice()->GetNumSubdevices();
    MASSERT(pCh && gpuverif);

    // Setup buffers for dmacheck
    if ( gpuverif->GetDmaUtils()->UseDmaTransfer() )
    {
        UINT32 dma_size = 0;
        if (m_CtCount > 1)
        {
            UINT32 active_count = 0;
            for (int i=0; i<MAX_RENDER_TARGETS; i++)
            {
                if (m_Ct[i]->GetEnabled())
                {
                    RenderTarget *target = m_Ct[i];
                    dma_size = dma_size < target->GetMaxSize() ? target->GetMaxSize() : dma_size;

                    if (++active_count >= m_CtCount)
                        break;
                }
            }
        }
        else
            dma_size = m_Ct[0]->GetMaxSize();
        gpuverif->GetDmaUtils()->SetupDmaBuffer(dma_size, 0);
    }

    // For multi-channel case, this WFI may be hang.
    // Consider below case, mods will be hang at step 4 because semaphore release is done in step 5.
    // 1. mods writes methods for gfx channel.
    // 2. policy manager inserts semaphore acquire for gfx channel.
    // 3. mods writes other methods for gfx channel until Begin(or other concern methods).
    // 4. mods wfi to dump surface(just below WFI). hang...
    // 5. policy manager inserts semaphore release for another channel.
    //
    // Please see bug 200501072 for more details about this case.
    // Lwrrently, mods has no solution to fix it, test should know this risk and avoid it if possible.
    DebugPrintf("If subsequent WFI hang, please confirm that semaphore acquire and release happened in pair before this WFI.");
    pCh->WaitForIdle();

    if (m_CtCount >= 1)
    {
        UINT32 active_count = 0;
        for (int i=0; i<MAX_RENDER_TARGETS; i++)
        {
            UINT32 selected_as = m_Ct[i]->GetSelect();
            RenderTarget *target = m_Ct[selected_as];
            if (target->GetEnabled())
            {
                if (target->LoadSurface() != OK) // Here we initialize the surface
                {
                    DebugPrintf("No active Ct%i to be dumpped\n", i);
                    continue;
                }
                MdiagSurf *active_surf = target->GetActive();
                if (!active_surf)
                {
                    // In case one CT is re-selected, need to re-create
                    active_surf = target->Create(GetSurface(ColorSurfaceTypes[selected_as], 0));
                }

                PrintSurfData(active_surf, false, target->GetIsRtt());
                for (UINT32 subdev=0; subdev<subDevNum; ++subdev)
                {
                    string gpuTag = subdev==0 ? "" : Utility::StrPrintf("_GPU%d", subdev);
                    string cframe = Utility::StrPrintf("active_color%d_%s_%04d%s.tga",
                        selected_as, file_name_postfix.c_str(), m_EndMethodCount, gpuTag.c_str());
                    gpuverif->DumpActiveSurface(active_surf, cframe.c_str(), subdev);
                }

                if (++active_count >= m_CtCount)
                {
                    break;
                }
            }
        }
    }
    else
        InfoPrintf("Something wrong - no active color surface was identified\n");

    if (m_ZtCount > 0)
    {
        RenderTarget *targetZ = m_Zt;
        if (targetZ->LoadSurface() != OK) // Initialize the surface
        {
            DebugPrintf("No active Zeta to be dumpped\n");
            return;
        }
        MdiagSurf *active_surf = targetZ->GetActive();
        PrintSurfData(active_surf, true, targetZ->GetIsRtt());
        for (UINT32 subdev=0; subdev<subDevNum; ++subdev)
        {
            string gpuTag = subdev==0 ? "" : Utility::StrPrintf("_GPU%d", subdev);
            string zframe = Utility::StrPrintf("active_zeta_%s_%04d%s.tga",
                file_name_postfix.c_str(), m_EndMethodCount, gpuTag.c_str());
            gpuverif->DumpActiveSurface(active_surf, zframe.c_str(), subdev);
        }
    }
}

void BufferDumper::DumpZlwllToFile(ImageList_t* DumpZlwllList)
{
    TraceChannel  *pTraceChannel = m_Test->GetLwrrentChannel(GpuTrace::CPU_MANAGED);

    if( DumpZlwllList )
    {
        if( DumpZlwllList->empty() || (m_EndMethodCount != DumpZlwllList->top()) )
        {
            return;
        }

        do
        {
            DumpZlwllList->pop();
        }
        while ( !DumpZlwllList->empty() && (m_EndMethodCount == DumpZlwllList->top()) );
    }
    // construct file name
    string fname = Utility::StrPrintf("zlwll_ram_%04d.zcl", m_EndMethodCount);

    pTraceChannel->GetCh()->WaitForIdle();
    MASSERT(!"SaveZlwllRamToFile for fermi has not been implemented");
}

void BufferDumper::DumpBufferToFile(CachedSurface *cs,
                                    MdiagSurf *dma,
                                    const string &module_name)
{
    TraceChannel  *pTraceChannel = m_Test->GetDefaultChannelByVAS(dma->GetGpuVASpace());
    GpuVerif  *gpuverif = pTraceChannel->GetGpuVerif();

    pTraceChannel->GetCh()->WaitForIdle();

    // Save to file
    string fname = module_name;
    string numstr = Utility::StrPrintf("_%04d", m_EndMethodCount);
    string::size_type posloc = fname.rfind( '.' );
    fname.insert( posloc, numstr );

    InfoPrintf("Dumping %s with size %d\n", fname.c_str(), GetSize(cs));
    if (gpuverif->GetDumpUtils()->ReadAndDumpDmaBuffer(dma, GetSize(cs), fname) != OK)
    {
        ErrPrintf("Failed to dump %s\n", fname.c_str());
    }
}

void BufferDumper::PrintSurfData( MdiagSurf* psurf, bool surface_z, bool isRTT ) const
{
    MASSERT( psurf );

    const char* name;
    if( surface_z )
        name = m_Test->GetSurfaceMgr()->ZetaNameFromZetaFormat(
            m_Test->GetSurfaceMgr()->DeviceFormatFromFormat(psurf->GetColorFormat()));
    else
        name = m_Test->GetSurfaceMgr()->ColorNameFromColorFormat(
            m_Test->GetSurfaceMgr()->DeviceFormatFromFormat(psurf->GetColorFormat()));
    InfoPrintf("+ %s (%d) - %s\n",
        psurf->GetName().c_str(), m_EndMethodCount,
        isRTT?"RTT":"standard");
    InfoPrintf("  - Dim: %dx%d format: %s layout: %s ",
        psurf->GetWidth(),
        psurf->GetHeight(), name,
        psurf->GetLayout()==Surface2D::BlockLinear? "BlockLinear":"Pitch" );
    if( psurf->GetLayout() ==  Surface2D::BlockLinear )
        RawPrintf("Block: %dx%dx%d\n",
            1 << psurf->GetLogBlockWidth(),
            1 << psurf->GetLogBlockHeight(),
            1 << psurf->GetLogBlockDepth());
    else
        RawPrintf("pitch\n");

    InfoPrintf("  - [hCtxDam 0x%x, Offset 0x%llx, hMem 0x%x]\n",
        psurf->GetCtxDmaHandle(), psurf->GetCtxDmaOffsetGpu(),
        psurf->GetMemHandle());
    DebugPrintf("  - MemoryModel Paging, Compressed %s\n",
        psurf->GetCompressed()?"yes":"no");
    DebugPrintf("  - Depth %d, Layers %d, Pitch 0x%x, Allocate Height %d\n",
        psurf->GetBytesPerPixel(), psurf->GetDepth(), psurf->GetPitch(),
        psurf->GetAllocHeight());
    DebugPrintf("  - Allocate Layers %d, ArrayPitch 0x%x, ArraySize %d, Size 0x%x\n",
        psurf->GetAllocDepth(),
        psurf->GetArrayPitch(), psurf->GetArraySize(),
        psurf->GetSize());
    DebugPrintf("  - UserOffset 0x%x, Adjust 0x%x\n",
        psurf->GetExtraAllocSize(), psurf->GetHiddenAllocSize());
}

MdiagSurf* BufferDumper::GetSurface(SurfaceType s, UINT32 subdev) const
{
    return m_Test->GetSurfaceMgr()->GetSurface(s, subdev);
}

UINT32 BufferDumper::Get032(CachedSurface *cs, UINT32 offset, UINT32 gpuSubdevIdx)
{
    return cs->Get032(gpuSubdevIdx, offset);
}

UINT32* BufferDumper::Get032Addr(CachedSurface *cs, UINT32 offset, UINT32 gpuSubdevIdx)
{
    MASSERT(cs->GetPtr(gpuSubdevIdx) != 0);
    return cs->Get032Addr(gpuSubdevIdx, offset);
}

UINT32 BufferDumper::GetSize(CachedSurface *cs) const
{
    return cs->GetSize(0);
}

bool BufferDumper::NeedToDumpLwrrBegin()
{
    // Check for -dump_image_every_begin
    if (m_DumpImageEveryBegin)
    {
        return true;
    }

    // Check for -dump_image_nth_begin
    ImageList_t *list = m_Test->GetTrace()->GetDumpImageEveryBeginList();
    if (!list->empty())
    {
        if (m_BeginMethodCount == list->top())
        {
            do
            {
                list->pop();
            } while (!list->empty() && m_BeginMethodCount == list->top());
            return true;
        }
    }

    // Check for -dump_image_every_n_begin
    if (!m_DumpEveryNBegin.empty())
    {
        for (UINT32 i = 0; i < m_DumpEveryNBegin.size(); i++)
        {
            if (m_BeginMethodCount % m_DumpEveryNBegin[i] == 0)
            {
                return true;
            }
        }
    }

    // Check for -dump_image_range_begin
    if (!m_DumpRangeMinBegin.empty())
    {
        UINT32 count = m_DumpRangeMinBegin.size();
        for (UINT32 i = 0; i < count; i++)
        {
            if (m_BeginMethodCount >= m_DumpRangeMinBegin[i] &&
                m_BeginMethodCount <= m_DumpRangeMaxBegin[i])
            {
                return true;
            }
        }
    }

    return false;
}

bool BufferDumper::NeedToDumpLwrrEnd()
{
    // Check for -dump_image_every_end
    if (m_DumpImageEveryEnd)
    {
        return true;
    }

    // Check for -dump_image_nth_end
    ImageList_t *list = m_Test->GetTrace()->GetDumpImageEveryEndList();
    if (!list->empty())
    {
        if (m_EndMethodCount == list->top())
        {
            do
            {
                list->pop();
            } while (!list->empty() && m_EndMethodCount == list->top());
            return true;
        }
    }

    // Check for -dump_image_every_n_end
    if (!m_DumpEveryNEnd.empty())
    {
        for (UINT32 i = 0; i < m_DumpEveryNEnd.size(); i++)
        {
            if (m_EndMethodCount % m_DumpEveryNEnd[i] == 0)
            {
                return true;
            }
        }
    }

   // Check for -dump_image_range_end
    if (!m_DumpRangeMinEnd.empty())
    {
        UINT32 count = m_DumpRangeMinEnd.size();
        for (UINT32 i = 0; i < count; i++)
        {
            if (m_EndMethodCount >= m_DumpRangeMinEnd[i] &&
                m_EndMethodCount <= m_DumpRangeMaxEnd[i])
            {
                return true;
            }
        }
    }

    return false;
}

bool BufferDumper::IsHookedOp(UINT32 class_id, UINT32 op) const
{
    pair<HookedMethodMapConstIter, HookedMethodMapConstIter> result =
        m_HookedMethodMap.equal_range(class_id);
    for (HookedMethodMapConstIter iter = result.first; iter != result.second; ++iter)
    {
        if (op == iter->second)
        {
            return true;
        }
    }

    return false;
}

