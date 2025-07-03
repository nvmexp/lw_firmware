/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2019-2022 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// Please keep the documentation at the following page in sync with the code:
// http://engwiki/index.php/MODS/GPU_Verification/trace_3d/File_Format

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "gputrace.h"
#include "tracemod.h"
#include "tracerel.h"
#include "trace_3d.h"
#include "slisurf.h"
#include "tracesubchan.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "mdiag/utils/tex.h"
#include "gpu/utility/surffmt.h"
#include "class/cl9097tex.h"
#include "class/cl9097.h"
#include "class/cla097tex.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(TraceReloc)

//---------------------------------------------------------------------------
// Helper functions

// NOTE: Trace3d patches with the relocations the push buffer in 2 places:
// 1) When the relocations are performed (DoRelocations)
// 2) Building a map "SubdevMaskDataMap" during DoRelocations,
//    which is later used in TraceModule::MassageAndInsertSubdeviceMasks
//
// This function is used for debugging.  It allows you to selectively
// choose which relocation should use the map SubdevMaskDataMap
// (in terms of subdev masks and peer offsets) for step 2.
// The rest of the relocations will be patched immediately in step 1.
//
// IMPORTANT: ShouldUseMap MUST be called in each DoOffset function in order
//            to keep incrementing LwrrentReloc for each relocation.
static bool ShouldUseMap(bool oldValue, UINT32 subdev)
{
    return true;

    // Set this constant to the relocation you would like to isolate.
    //  0 => All relocations will use the map where appropriate.
    //       USE A ZERO FOR NORMAL, NON-DEBUG MODE.
    // ~0 => Do no relocations with the map. (Patch immediately in step 1)
    //  1 => Only use the map for the first relocation (if appropriate).
    //  2 => Only use the map for the second relocation (if appropriate).
    // ...
    // Above: "Appropriate" means that the system is in SLI and the relocation
    //        is on a push buffer.
    static const UINT32 ISOLATED_RELOC = 1;

    // Relocation Counter: Always initialize to zero. (Debug Variable)
    static UINT32 LwrrentReloc = 0;

    // If ISOLATE_RELOC is not 0, we are in debug mode.
    // We should only check this once for each reloc instantiation, so we will
    //    check only for subdevice 0.
    // See above (top of file) for more information.
    if (ISOLATED_RELOC && subdev == 0)
    {
        if (ISOLATED_RELOC == ++LwrrentReloc)
        {
            // This is the relocation we are trying to isolate
            return true;
        }
        else
        {
            // This is not the relocation we are trying to isolate
            return false;
        }
    }

    // We aren't in debug mode (ISOLATED_RELOC is 0) OR
    // we already determined whether or not to use the map (subdev is not 0)
    // Just use the oldValue.
    return oldValue;
}

static inline UINT32 InsertBits39to32(UINT64 orig_val, UINT64 new_val)
{
    return (UINT32)((orig_val & 0xffffff00) | ((new_val >> 32) & 0xff));
}

static inline RC SanityCheckVAS(LwRm::Handle hsrc, LwRm::Handle hdst)
{
    RC rc;
    if (hsrc != hdst)
    {
        ErrPrintf("can not reloc across vaspace, hsrc %x, hdst %x,please check test.hdr!\n", hsrc, hdst);
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC AddMapPartToMap(TraceModule::SubdevMaskDataMap *pMapPart,
                   TraceModule::SubdevMaskDataMap *pMap)
{
    TraceModule::SubdevMaskDataMap::iterator iterOut;
    for (iterOut = pMapPart->begin(); iterOut != pMapPart->end(); ++iterOut)
    {
        TraceModule::Subdev2DataMap::iterator iterIn;
        for (iterIn = iterOut->second.begin(); iterIn != iterOut->second.end();
                ++iterIn)
        {
            // Are we overwriting a value in the map?
            if ((*pMap)[iterOut->first].count(iterIn->first) != 0)
            {
                // If you see this error, you tried to patch
                // this particular offset/subdevice combination more than once
                ErrPrintf("tracerel.cpp: Overwriting a value in the map!\n");
                MASSERT(0);
                return RC::SOFTWARE_ERROR;
            }

            // Write the data to the map
            (*pMap)[iterOut->first][iterIn->first] = iterIn->second;
        }
    }

    return OK;
}

// //////////////////////////////////////////////////////////////////////////
// TraceRelocation
// //////////////////////////////////////////////////////////////////////////
TraceRelocation::TraceRelocation(UINT32 offset,
    TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
    UINT32 peerNum, UINT32 peerID) :
        m_UseMap(true), m_Offset(offset),
        m_pSubdevMaskDataMap(pSubdevMaskDataMap),
        m_PeerNum(peerNum), m_PeerID(peerID)
{
}

RC TraceRelocation::SliSupported(TraceModule *mod) const
{
    SliSurfaceMapper *ssm = mod->GetTest()->GetSSM();
    if (ssm->IsMilestone1Mode() &&
        !IsSliMilestone1Compliant())
    {
        ErrPrintf("You can't run this test in SLI Milestone1 mode, since "
            "one of the TraceRelocations does not support Milestone2 mode\n");
        MASSERT(0);
        return RC::BAD_TRACE_DATA;
    }
    else if (ssm->IsMilestone2Mode() &&
        !IsSliMilestone2Compliant())
    {
        ErrPrintf("You can't run this test in SLI Milestone2 mode, since "
            "one of the TraceRelocations does not support Milestone2 mode\n");
        MASSERT(0);
        return RC::BAD_TRACE_DATA;
    }
    else if (ssm->IsMilestone3Mode() &&
        !IsSliMilestone3Compliant())
    {
        ErrPrintf("You can't run this test in SLI Milestone3 mode, since "
            "one of the TraceRelocations does not support Milestone3 mode\n");
        MASSERT(0);
        return RC::BAD_TRACE_DATA;
    }
    else
    {
        return OK;
    }
}

void TraceRelocation::GetSurfacesFromName
(
    const string &surfName,
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    MASSERT(pModule != NULL);
    MASSERT(pSurfaces != NULL);

    GpuDevice *pGpuDevice = pModule->GetGpuDev();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        const MdiagSurf *pSurface = NULL;
        TraceModule::SomeKindOfSurface skos;
        pModule->StringToSomeKindOfSurface(subdev, m_PeerNum, surfName, &skos);
        switch (skos.SurfaceType)
        {
            case TraceModule::SURF_TYPE_DMABUFFER:
                pSurface = pModule->SKOS2DmaBufPtr(&skos);
                break;
            case TraceModule::SURF_TYPE_LWSURF:
                pSurface = pModule->SKOS2LWSurfPtr(&skos);
                break;
            case TraceModule::SURF_TYPE_PEER:
                pSurface = pModule->SKOS2ModPtr(&skos)->GetDmaBuffer();
                break;
            default:
                MASSERT(!"Illegal case in TraceRelocation::StringToSurfaces");
        }
        MASSERT(pSurface != NULL);
        pSurfaces->insert(pSurface);
    }
}

void TraceRelocation::GetSurfacesFromType
(
    SurfaceType surfType,
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    MASSERT(pModule != NULL);
    MASSERT(pSurfaces != NULL);

    GpuDevice *pGpuDevice = pModule->GetGpuDev();
    for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
    {
        const MdiagSurf *pSurface = pModule->GetSurface(surfType, subdev);
        MASSERT(pSurface != NULL);
        pSurfaces->insert(pSurface);
    }
}

void TraceRelocation::GetSurfacesFromTarget
(
    TraceModule *pTarget,
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    MASSERT(pTarget != NULL);
    MASSERT(pModule != NULL);
    MASSERT(pSurfaces != NULL);

    const MdiagSurf *pSurface = pTarget->GetDmaBuffer();
    MASSERT(pSurface != NULL);
    pSurfaces->insert(pSurface);
}

void TraceRelocation::PrintHeader(TraceModule *module, const string &header,
    UINT32 subdev)
{
    if (!Tee::WillPrint(Tee::PriDebug))
    {
        return;
    }

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER)
    {
        // There is only one copy of push buffer, so get data from subdev 0
        subdev = 0;
    }

    DebugPrintf(MSGID(), "\t%s(%s) %s\n",
        GpuTrace::GetFileTypeData(module->GetFileType()).Description,
        module->GetName().c_str(),
        header.c_str());

    UINT64 max_size =
        min(module->GetSize(), (UINT64) module->GetDebugPrintSize());

    if (module->GetDebugPrintSize() == 0)
        max_size = module->GetSize();
    for (unsigned i = 0; i < max_size; i=i+4)
    {
        UINT32 alignedOffset = i + ALIGN_DOWN(m_Offset, 4U);
        if (alignedOffset < module->GetSize())
        {
            DebugPrintf(MSGID(), "\t\t\toffset: %3x, data: %08x\n",
                alignedOffset, module->Get032(alignedOffset, subdev));
        }
    }
    if (max_size != module->GetSize())
            DebugPrintf(MSGID(), "\t\t\t...\n");
}

TraceModule *TraceRelocation::GetTarget() const
{
    return NULL;
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationCtxDMAHandle::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    // TODO(anyone) Get Master Subdevice Properly
    UINT32 master_subdev = 0;
    UINT32 orig_val = module->Get032(m_Offset, module->GetFileType() == GpuTrace::FT_PUSHBUFFER ?
                                               master_subdev : subdev);

    TraceModule::SomeKindOfSurface skos;
    module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName, &skos);

    UINT32 new_val = skos.CtxDmaHandle;

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    // Patch the offset now
    DebugPrintf(MSGID(), "reloc CtxDmaHandle (%s) @ %08x : ", m_SurfName.c_str(),
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n", orig_val, new_val);

    module->DoReloc(m_Offset, new_val, subdev);
    return OK;
}

void TraceRelocationCtxDMAHandle::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromName(m_SurfName, pModule, pSurfaces);
}

// //////////////////////////////////////////////////////////////////////////
RC TraceRelocationOffset::DoOffset(TraceModule *module, UINT32 subdev)
{
    DebugPrintf(MSGID(), "TraceRelocationOffset::%s Subdev: %d Offset: 0x%x\n",
                    __FUNCTION__, subdev, m_Offset);

    RC rc;
    CHECK_RC(SliSupported(module));

    if (module->GetName() == m_SurfName)
    {
        ErrPrintf("Relwrsive relocations are not supported\n");
        return RC::BAD_TRACE_DATA;
    }

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 orig_val = 0;
    UINT32 orig_val_2 = 0;

    TraceModule::SomeKindOfSurface skos;
    module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName, &skos, m_PeerID);

    UINT64 new_val;
    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            // TODO(anyone) Get Master Subdevice Properly
            UINT32 master_subdev = 0;
            orig_val = module->Get032(m_Offset, master_subdev);
            orig_val_2 = module->Get032(m_Offset+4, master_subdev);

            // HACK(saggese): It will be cleaned up in the next CL.
            DebugPrintf(MSGID(), "Singleton: Offset without P2P: 0x%llx\n",
                        skos.CtxDmaOffset);

            UINT64 peerOffset = 0;

            if (skos.SurfaceType == TraceModule::SURF_TYPE_LWSURF)
            {
                MASSERT(m_PeerNum == Gpu::MaxNumSubdevices);
                MdiagSurf* lwrsurf = module->SKOS2LWSurfPtr(&skos);
                if (lwrsurf->GetCtxDmaHandleGpu() ==0)
                {
                    // There are cases where color/Z is disabled from
                    // command line, but the trace still has RELOC command
                    // using color/Z. Just skip the RELOC here
                    return OK;
                }

                // Determine which subdevice will be accessing the copy of the
                // surface to be modified.  If a peer address is reloced to
                // this surface, it needs to be callwlated based on how the
                // modified surface is accessed rather than on which subdevice
                // the modified surface is stored.
                UINT32 accessingSubdev = subdev;
                CHECK_RC(module->GetAccessingSubdevice(subdev, &accessingSubdev));
                peerOffset = module->GetTest()->GetSSM()->
                    GetCtxDmaOffset(lwrsurf, module->GetGpuDev(), accessingSubdev);
            }
            else if (skos.SurfaceType == TraceModule::SURF_TYPE_DMABUFFER)
            {
                MASSERT(m_PeerNum == Gpu::MaxNumSubdevices);
                TraceModule* mod = m_pTrace->ModFind(m_SurfName.c_str());

                // Determine which subdevice will be accessing the copy of the
                // surface to be modified.  If a peer address is reloced to
                // this surface, it needs to be callwlated based on how the
                // modified surface is accessed rather than on which subdevice
                // the modified surface is stored.
                UINT32 accessingSubdev = subdev;
                CHECK_RC(module->GetAccessingSubdevice(subdev, &accessingSubdev));
                peerOffset = module->GetTest()->GetSSM()->
                      GetCtxDmaOffset(module->SKOS2DmaBufPtr(&skos),
                              module->GetGpuDev(), accessingSubdev) +
                              (mod ? mod->GetOffsetWithinDmaBuf() : 0);
            }
            else if (skos.SurfaceType == TraceModule::SURF_TYPE_PEER)
            {
                // For peer modules, offset returned by
                // StringToSomeKindOfSurface is correct
                peerOffset = skos.CtxDmaOffset;
            }

            DebugPrintf(MSGID(), "Singleton: Offset with P2P: 0x%llx\n", peerOffset);

            // Callwlate new value
            new_val = (((orig_val & 0xff) << 32) | orig_val_2) + peerOffset;

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = InsertBits39to32(orig_val, new_val);
            newMapPart[m_Offset+4][subdev] = UINT32(new_val);

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(skos.GetVASpaceHandle(), module->GetVASpaceHandle()));
    // Patch the offset now

    // Callwlate new value
    UINT64 target_offset = skos.CtxDmaOffset;
    orig_val = module->Get032(m_Offset, subdev);
    orig_val_2 = module->Get032(m_Offset+4, subdev);
    new_val = (((orig_val & 0xff) << 32) | orig_val_2) + target_offset;

    // Patch Push Buffer
    DebugPrintf(MSGID(), "reloc CtxDmaOffset (%s) @ %08x : ", m_SurfName.c_str(),
            m_Offset);
    DebugPrintf(MSGID(), "%08x:%08x -> %08x:%08x\n", UINT32(orig_val), orig_val_2,
            InsertBits39to32(orig_val, new_val), UINT32(new_val));

    module->DoReloc(m_Offset, InsertBits39to32(orig_val, new_val), subdev);
    module->DoReloc(m_Offset + 4, UINT32(new_val), subdev);

    return OK;
}

void TraceRelocationOffset::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromName(m_SurfName, pModule, pSurfaces);
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationFile::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    if (m_bUseTex)
    {
        TextureHeader *pHeader = m_pTarget->GetTxParams();

        if (NULL == pHeader)
        {
            ErrPrintf("No texture header for target file\n");
            return RC::BAD_TRACE_DATA;
        }
        if (!pHeader->PixelOffset(m_ArrayIndex, m_LwbmapFace, m_Miplevel, m_X, m_Y, m_Z, &m_TargetOffset))
        {
            ErrPrintf("Can not callwlate offset for array_index=0x%x, lwbemap_face=0x%x, miplevel=0x%x "
                    "x=0x%x y=0x%x Z=0x%x\n", m_ArrayIndex, m_LwbmapFace, m_Miplevel, m_X, m_Y, m_Z);
            return RC::BAD_TRACE_DATA;
        }
    }

    UINT64 orig_val, orig_val_2;
    if (m_Swap)
    {
        orig_val = module->Get032(m_Offset+4, subdev);
        orig_val_2 = module->Get032(m_Offset, subdev);
    }
    else
    {
        orig_val = module->Get032(m_Offset, subdev);
        orig_val_2 = module->Get032(m_Offset+4, subdev);
    }

    UINT64 target_offset;

    // For peer modules, the subdevice provided here is the remote
    // subdevice (i.e. the subdevice located on the device that does not
    // contain the actual FB memory allocation), and the peer num is
    // the local subdevice (i.e. he subdevice located on the device that
    // contains the actual FB memory allocation)
    if (m_pTarget->IsPeer())
        target_offset = m_pTarget->GetOffset(m_PeerNum, subdev);
    else if (m_PeerNum < Gpu::MaxNumSubdevices)
        target_offset = m_pTarget->GetOffset(m_PeerNum, 0);
    else
        target_offset = m_pTarget->GetOffset();

    UINT64 new_val = CallwlateValue((((orig_val & 0xff) << 32) | orig_val_2),
                                    target_offset + m_TargetOffset, m_MaskIn);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            if (m_Swap)
            {
                newMapPart[m_Offset+4][subdev] = InsertBits39to32(orig_val, new_val);
                newMapPart[m_Offset][subdev] = UINT32(new_val);
            }
            else
            {
                newMapPart[m_Offset][subdev] = InsertBits39to32(orig_val, new_val);
                newMapPart[m_Offset+4][subdev] = UINT32(new_val);
            }

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(m_pTarget->GetVASpaceHandle(), module->GetVASpaceHandle()));
    // Patch the offset now
    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ", m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x:%08x -> %08x:%08x\n", UINT32(orig_val), orig_val_2,
            InsertBits39to32(orig_val, new_val), UINT32(new_val));

    if (m_Swap)
    {
        module->DoReloc(m_Offset + 4, InsertBits39to32(orig_val, new_val), subdev);
        module->DoReloc(m_Offset, UINT32(new_val), subdev);
    }
    else
    {
        module->DoReloc(m_Offset, InsertBits39to32(orig_val, new_val), subdev);
        module->DoReloc(m_Offset + 4, UINT32(new_val), subdev);
    }

    return OK;
}

void TraceRelocationFile::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationSurface::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 orig_val, orig_val_2;

    // see comment about this in class definition
    if (m_Swapped)
    {
        orig_val = module->Get032(m_Offset+4, subdev);
        orig_val_2 = module->Get032(m_Offset, subdev);
    }
    else
    {
        orig_val = module->Get032(m_Offset, subdev);
        orig_val_2 = module->Get032(m_Offset+4, subdev);
    }

    UINT64 target_offset;
    const MdiagSurf *surf = 0;
    if (m_Surf == SURFACE_TYPE_CLIPID)
    {
        surf = module->GetClipIDSurface();
        target_offset = surf->GetCtxDmaOffsetGpu();
    }
    else
    {
        if (m_Surf != SURFACE_TYPE_UNKNOWN)
        {
            surf = module->GetSurface(m_Surf, subdev);
        }
        else
        {
            // Find the surface via name
            TraceModule::SomeKindOfSurface skos = {0};
            module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName, &skos);
            switch (skos.SurfaceType)
            {
                case TraceModule::SURF_TYPE_DMABUFFER:
                    surf = module->SKOS2DmaBufPtr(&skos);
                    break;
                case TraceModule::SURF_TYPE_LWSURF:
                    surf = module->SKOS2LWSurfPtr(&skos);
                    break;
                case TraceModule::SURF_TYPE_PEER:
                    surf = module->SKOS2ModPtr(&skos)->GetDmaBufferNonConst();
                    break;
                default:
                    MASSERT(!"Illegal case in TraceRelocation::StringToSurfaces");
            }
        }

        if (m_PeerNum < Gpu::MaxNumSubdevices)
        {
            target_offset = surf->GetCtxDmaOffsetGpuPeer(m_PeerNum, m_PeerID);
        }
        else
        {
            target_offset = surf->GetCtxDmaOffsetGpu();
        }
        target_offset += surf->GetExtraAllocSize();
    }

    CHECK_RC(SanityCheckVAS(surf->GetGpuVASpace(), module->GetVASpaceHandle()));

    SurfaceFormat *surffmt = 0;
    if (surf && (surffmt = SurfaceFormat::CreateFormat(const_cast<Surface2D*>(surf->GetSurf2D()), m_GpuDev)))
    {
        if (OK != surffmt->Offset(&m_SurfOffset, m_X, m_Y, m_Z, m_Miplevel, m_LwbmapFace, m_ArrayIndex))
        {
            ErrPrintf("Can not callwlate offset for array_index=0x%x, lwbemap_face=0x%x, miplevel=0x%x "
                      "x=0x%x y=0x%x Z=0x%x\n", m_ArrayIndex, m_LwbmapFace, m_Miplevel, m_X, m_Y, m_Z);
            delete surffmt;
            return RC::BAD_TRACE_DATA;
        }
        delete surffmt;
        surffmt = 0;
    }

    UINT64 new_val = CallwlateValue((((orig_val & 0xff) << 32) | orig_val_2),
                        target_offset + m_SurfOffset, m_MaskIn);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        //Don't massage now.  Dump into map for single pass massage later

        TraceModule::SubdevMaskDataMap newMapPart;
        if (m_Swapped)
        {
            newMapPart[m_Offset+4][subdev] = InsertBits39to32(orig_val, new_val);
            newMapPart[m_Offset][subdev] = UINT32(new_val);
        }
        else
        {
            newMapPart[m_Offset][subdev] = InsertBits39to32(orig_val, new_val);
            newMapPart[m_Offset+4][subdev] = UINT32(new_val);
        }

        CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

        return OK;
    }

    // Patch the offset now
    if (m_Swapped)
    {
        module->DoReloc(m_Offset + 4, InsertBits39to32(orig_val, new_val), subdev);
        module->DoReloc(m_Offset, UINT32(new_val), subdev);
    }
    else
    {
        module->DoReloc(m_Offset, InsertBits39to32(orig_val, new_val), subdev);
        module->DoReloc(m_Offset + 4, UINT32(new_val), subdev);
    }

    return OK;
}

void TraceRelocationSurface::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromType(m_Surf, pModule, pSurfaces);
}

TraceRelocationSurface::~TraceRelocationSurface()
{
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocation40BitsSwapped::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    // This should never been run on push buffers
    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER)
    {
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(SliSupported(module));

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 orig_val = module->Get032(m_Offset+4, subdev);
    UINT32 orig_val_2 = module->Get032(m_Offset, subdev);

    UINT64 new_val;

    // Get Peer-To-Peer offset for subdevices
    UINT64 target_offset;
    if (m_Peer && module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        MASSERT(subdev < 2); // only two way for now
        if (m_pTarget->IsPeer())
        {
            // For peer modules, the subdevice provided here is the remote
            // subdevice (i.e. the subdevice located on the device that does not
            // contain the actual FB memory allocation), and the peer num is
            // the local subdevice (i.e. he subdevice located on the device that
            // contains the actual FB memory allocation)
            target_offset = m_pTarget->GetOffset(m_PeerNum, subdev);
        }
        else
        {
            MASSERT(m_PeerNum == Gpu::MaxNumSubdevices);
            target_offset = m_pTarget->GetDmaBuffer()->GetCtxDmaOffsetGpuPeer(subdev ? 0 : 1, m_PeerID);
        }
    }
    else
    {
        if (m_pTarget->IsPeer())
        {
            // For peer modules, the subdevice provided here is the remote
            // subdevice (i.e. the subdevice located on the device that does not
            // contain the actual FB memory allocation), and the peer num is
            // the local subdevice (i.e. he subdevice located on the device that
            // contains the actual FB memory allocation)
            target_offset = m_pTarget->GetOffset(m_PeerNum, subdev);
        }
        else if (m_PeerNum < Gpu::MaxNumSubdevices)
        {
            target_offset = m_pTarget->GetOffset(m_PeerNum, 0);
        }
        else
        {
            target_offset = m_pTarget->GetOffset();
        }
    }

    CHECK_RC(SanityCheckVAS(m_pTarget->GetVASpaceHandle(), module->GetVASpaceHandle()));

    // Callwlate value to patch
    new_val = (((orig_val & 0xff) << 32) | orig_val_2) + target_offset;

    // Patch the offset now
    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ", m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x:%08x -> %08x:%08x\n", UINT32(orig_val), orig_val_2,
            InsertBits39to32(orig_val, new_val), UINT32(new_val));

    module->DoReloc(m_Offset+4, InsertBits39to32(orig_val, new_val), subdev);
    module->DoReloc(m_Offset, UINT32(new_val), subdev);

    return OK;
}

void TraceRelocation40BitsSwapped::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocation40BitsSwapped::GetTarget() const
{
    return m_pTarget;
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationDynTex::DoOffset(TraceModule* module, UINT32 subdev)
{
    // Unlike usual offset, here we "create" texture header based on the
    // surface picked by a test.
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    // NOTE: The map isn't implemented for this relocation, but the counter
    // still needs to be updated.  See the top of this file for information.
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    IGpuSurfaceMgr *surfMgr = module->GetSurfaceMgr();
    TraceModule::SomeKindOfSurface skos;
    MdiagSurf *surf = 0;
    if ("VCAA" == m_SurfName ||
        "STENCIL" == m_SurfName)
    {
        surf = surfMgr->GetSurface(SURFACE_TYPE_Z, subdev);
    }
    else
    {
        module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName, &skos);
        if (skos.CtxDmaOffset != 0 && skos.Size != 0)
        {
            switch (skos.SurfaceType)
            {
            case TraceModule::SURF_TYPE_LWSURF:
                surf = module->SKOS2LWSurfPtr(&skos);
                break;
            case TraceModule::SURF_TYPE_DMABUFFER:
                surf = module->SKOS2DmaBufPtr(&skos);
                break;
            case TraceModule::SURF_TYPE_PEER:
            default:
                MASSERT(!"RELOC_D: Surface not found\n");
                return RC::BAD_TRACE_DATA;
            }
        }
        else
        {
            ErrPrintf("%s: Surface %s not found\n", __FUNCTION__, m_SurfName.c_str());
            return RC::BAD_TRACE_DATA;
        }
    }
    LW50Raster* pRaster = surfMgr->GetRaster(surf->GetColorFormat());

    if (surfMgr->GetForceNull(surf))
    {
        ErrPrintf("Can not create texture header out of disabled surface\n");
        return RC::BAD_TRACE_DATA;
    }

    CHECK_RC(SanityCheckVAS(surf->GetGpuVASpace(), module->GetVASpaceHandle()));

    UINT32 header_offset = m_Offset*TEXTURE_HEADER_STATE_SIZE*sizeof(UINT32);
    TextureHeaderState texHeaderState;

    texHeaderState.DWORD0 = module->Get032(header_offset + 0 * sizeof(UINT32), subdev);
    texHeaderState.DWORD1 = module->Get032(header_offset + 1 * sizeof(UINT32), subdev);
    texHeaderState.DWORD2 = module->Get032(header_offset + 2 * sizeof(UINT32), subdev);
    texHeaderState.DWORD3 = module->Get032(header_offset + 3 * sizeof(UINT32), subdev);
    texHeaderState.DWORD4 = module->Get032(header_offset + 4 * sizeof(UINT32), subdev);
    texHeaderState.DWORD5 = module->Get032(header_offset + 5 * sizeof(UINT32), subdev);
    texHeaderState.DWORD6 = module->Get032(header_offset + 6 * sizeof(UINT32), subdev);
    texHeaderState.DWORD7 = module->Get032(header_offset + 7 * sizeof(UINT32), subdev);

    GpuSubdevice *subDevice = module->GetGpuDev()->GetSubdevice(subdev);
    MASSERT(subDevice);

    TextureHeader* header = TextureHeader::CreateTextureHeader(&texHeaderState, subDevice, m_TextureHeaderFormat);

    UINT64 offset;
    if (m_PeerNum < Gpu::MaxNumSubdevices)
    {
        offset = surf->GetCtxDmaOffsetGpuPeer(m_PeerNum, m_PeerID);
    }
    else
    {
        offset = surf->GetCtxDmaOffsetGpu();
    }

    offset += surf->GetExtraAllocSize();

    CHECK_RC(SanityCheckVAS(surf->GetGpuVASpace(), module->GetVASpaceHandle()));

    CHECK_RC(header->RelocTexture(
        surf,
        offset,
        pRaster,
        m_Mode,
        module->GetGpuDev(),
        !m_NoOffset,
        !m_NoSurfaceAttr,
        m_OptimalPromotion,
        m_CenterSpoof,
        &texHeaderState));

    module->DoReloc(header_offset, texHeaderState.DWORD0, subdev);

    if (!m_NoOffset)
    {
        module->DoReloc(header_offset + sizeof(UINT32), texHeaderState.DWORD1, subdev);
    }

    module->DoReloc(header_offset + 2*sizeof(UINT32), texHeaderState.DWORD2, subdev);

    if (m_NoSurfaceAttr)
    {
        return OK;
    }

    module->DoReloc(header_offset + 3*sizeof(UINT32), texHeaderState.DWORD3, subdev);
    module->DoReloc(header_offset + 4*sizeof(UINT32), texHeaderState.DWORD4, subdev);
    module->DoReloc(header_offset + 5*sizeof(UINT32), texHeaderState.DWORD5, subdev);
    module->DoReloc(header_offset + 6*sizeof(UINT32), texHeaderState.DWORD6, subdev);
    module->DoReloc(header_offset + 7*sizeof(UINT32), texHeaderState.DWORD7, subdev);

    return OK;
}

void TraceRelocationDynTex ::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    // Empty
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationSize::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    TraceModule::SomeKindOfSurface skos;
    module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName, &skos);

    //size of the module is always 32-bits
    UINT32 orig_val = module->Get032(m_Offset, subdev);
    UINT32 new_val = (UINT32)CallwlateValue((UINT64)orig_val, (UINT64)skos.Size, (UINT64)m_MaskIn);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    DebugPrintf(MSGID(), "reloc (%s) @ %08x : ", m_SurfName.c_str(),
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n",
            module->Get032(m_Offset, subdev),
            new_val);

    module->DoReloc(m_Offset, new_val, subdev);

    return OK;
}

void TraceRelocationSize::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromName(m_SurfName, pModule, pSurfaces);
}

bool TraceRelocationSize::operator== (const TraceRelocationSize& relocSize) const
{
    return this->m_Offset == relocSize.m_Offset
        && *(this->m_pSubdevMaskDataMap) == *(relocSize.m_pSubdevMaskDataMap)
        && this->m_PeerNum == relocSize.m_PeerNum
        && this->m_pTarget == relocSize.m_pTarget
        && this->m_mask == relocSize.m_mask
        && this->m_Add == relocSize.m_Add
        && this->m_MaskIn == relocSize.m_MaskIn
        && this->m_SurfName == relocSize.m_SurfName;
}

// //////////////////////////////////////////////////////////////////////////

RC TraceRelocationSurfaceProperty::DoOffset(TraceModule* module, UINT32 subdev)
{
    if (module->IsFrozenAt(m_Offset))
        return OK;

    // number of DWORD to patch
    UINT32 num = 1;

    UINT32 value32 = 0;
    UINT64 value64 = 0;
    MdiagSurf *surf = 0;
    UINT32 index;

    TraceModule::SomeKindOfSurface skos;
    module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfaceName, &skos);

    if (skos.SurfaceType == TraceModule::SURF_TYPE_LWSURF)
    {
        surf = module->SKOS2LWSurfPtr(&skos);
    }
    else if (skos.SurfaceType == TraceModule::SURF_TYPE_DMABUFFER)
    {
        surf = module->SKOS2DmaBufPtr(&skos);
    }

    if (surf == 0)
    {
        ErrPrintf("TraceRelocationSurfaceProperty::DoOffset: Surface not found\n");
        return RC::BAD_TRACE_DATA;
    }

    IGpuSurfaceMgr *surfmgr = module->GetSurfaceMgr();

    if (m_Property == "PIXEL_WIDTH")
        value32 = surf->GetWidth() / surf->GetAAWidthScale();
    else if (m_Property == "PIXEL_HEIGHT")
        value32 = surf->GetHeight() / surf->GetAAHeightScale();
    else if (m_Property == "SAMPLE_WIDTH")
    {
        if (surf->GetLayout() == Surface2D::BlockLinear)
        {
            value32 = surf->GetAllocWidth();
        }
        else
        {
            // Pitch mode need surface pitch other than actual width
            // Also, fermi requires 128 byte alignment
            MASSERT(surf->GetLayout() == Surface2D::Pitch);
            value32 = (surf->GetPitch() + 0x7f) & ~0x7f;
        }
    }
    else if (m_Property == "SAMPLE_HEIGHT")
        value32 = surf->GetAllocHeight();
    else if (m_Property == "DEPTH")
        value32 = surf->GetDepth();
    else if (m_Property == "ARRAY_PITCH")
    {
        // return size in DW
        value32 = surf->GetArrayPitch() >> 2;
    }
    else if (m_Property == "ARRAY_SIZE")
        value32 = surf->GetArraySize();
    else if (m_Property == "BLOCK_WIDTH")
        value32 = surf->GetLogBlockWidth();
    else if (m_Property == "BLOCK_HEIGHT")
        value32 = surf->GetLogBlockHeight();
    else if (m_Property == "BLOCK_DEPTH")
        value32 = surf->GetLogBlockDepth();
    else if (m_Property == "LAYOUT")
    {
        value32 = (surf->GetLayout() == Surface2D::BlockLinear) ?
            LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_BLOCKLINEAR :
            LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_PITCH;
    }
    else if (m_Property == "FORMAT")
    {
        // Color/Z surfaces need to be handled differently due to
        // command-line arguments like -formatC and -zt_count_0.
        if (IsSurfaceTypeColorZ3D(m_SurfType))
        {
            // If the color/z surface is going to be allocated,
            // just return the corresponding device format for
            // the color format of the surface.
            if (surfmgr->GetValid(surf) ||
                (m_ProgZtAsCt0 && (m_SurfType == SURFACE_TYPE_COLORA)))
            {
                value32 = surfmgr->DeviceFormatFromFormat(
                    surf->GetColorFormat());
            }

            // There is no "disabled" format for Z buffers,
            // so just return a default format.
            else if (m_SurfType == SURFACE_TYPE_Z)
            {
                value32 = LW9097_SET_ZT_FORMAT_V_Z24S8;
            }

            // If a color surface will not be allocated, return the
            // DISABLED format.
            else
            {
                value32 = LW9097_SET_COLOR_TARGET_FORMAT_V_DISABLED;
            }
        }

        // For non-color/z surfaces, just return the corresponding
        // device format for the color format of the surface.
        else
        {
            value32 = surfmgr->DeviceFormatFromFormat(surf->GetColorFormat());
        }
    }
    else if (m_Property == "AA_ENABLED")
    {
        // The -mscontrol command-line argument overrides the
        // AA mode of the surface for determining if anti-aliasing
        // should be enabled.
        if (surfmgr->GetMultiSampleOverride())
        {
            value32 = surfmgr->GetMultiSampleOverrideValue();
        }
        else if (surf->GetAAMode() != Surface2D::AA_1)
        {
            value32 = 1;
        }
        else
        {
            value32 = 0;
        }
    }
    else if (m_Property == "MEMORY")
    {
        UINT32 log_block_width = surf->GetLogBlockWidth();
        UINT32 log_block_height = surf->GetLogBlockHeight();
        UINT32 log_block_depth = surf->GetLogBlockDepth();
        UINT32 layout = surf->GetLayout() == Surface2D::Pitch ?
            LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_PITCH :
            LW9097_SET_COLOR_TARGET_MEMORY_LAYOUT_BLOCKLINEAR;

        if (m_SurfType == SURFACE_TYPE_Z)
            value32 = DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _WIDTH, log_block_width)   |
                    DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _HEIGHT, log_block_height) |
                    DRF_NUM(9097, _SET_ZT_BLOCK_SIZE, _DEPTH, log_block_depth);
        else
            value32 = DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_WIDTH, log_block_width)   |
                    DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_HEIGHT, log_block_height) |
                    DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _BLOCK_DEPTH, log_block_depth)   |
                    DRF_NUM(9097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, layout);
    }
    else if (m_Property == "AA_MODE")
    {
        switch (GetAAModeFromSurface(surf))
        {
        case AAMODE_1X1:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_1X1);
            break;
        case AAMODE_2X1:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1);
            break;
        case AAMODE_2X2:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2);
            break;
        case AAMODE_4X2:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2);
            break;
        case AAMODE_4X4:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X4);
            break;
        case AAMODE_2X1_D3D:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X1_D3D);
            break;
        case AAMODE_4X2_D3D:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_D3D);
            break;
        case AAMODE_2X2_VC_4:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_4);
            break;
        case AAMODE_2X2_VC_12:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_2X2_VC_12);
            break;
        case AAMODE_4X2_VC_8:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_8);
            break;
        case AAMODE_4X2_VC_24:
            value32 = DRF_DEF(9097, _SET_ANTI_ALIAS, _SAMPLES, _MODE_4X2_VC_24);
            break;
        default:
            ErrPrintf("RELOC_SURFACE_PROPERTY: Ilwalide AAMode\n");
            return RC::BAD_TRACE_DATA;
        }
    }
    else if (m_Property == "PHYS_ADDRESS")
    {
        RC rc = OK;

        if (!surf->GetPhysContig())
        {
            ErrPrintf("Physical relocations require that the physical memory be contiguous. Try adding the CONTIG option to ALLOC_SURFACE or use -as_contig <surface name>\n");
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(surf->GetPhysAddress(0, &value64));

        num = 2;
    }
    else if (1 == sscanf(m_Property.c_str(), "SAMPLE_POSITION%d", &index))
    {
        value32 = GetProgrammableSampleData(surf, index,
            module->GetTest()->GetParam());
    }
    else if (1 == sscanf(m_Property.c_str(), "SAMPLE_POSITION_FLOAT%d", &index))
    {
        value32 = GetProgrammableSampleDataFloat(surf, index,
            module->GetTest()->GetParam());
    }
    else
    {
        ErrPrintf("RELOC_SURFACE_PROPERTY: Unknown option - %s\n", m_Property.c_str());
        return RC::BAD_TRACE_DATA;
    }

    if (m_Property == "FORMAT")
    {
        DebugPrintf(MSGID(), "RELOC_SURFACE_PROPERTY: %s = %s\n", m_Property.c_str(),
            ColorUtils::FormatToString(static_cast<ColorUtils::Format>(surf->GetColorFormat())).c_str());
    }
    else if (m_Property == "LAYOUT")
    {
        string layout = (surf->GetLayout() == Surface2D::Pitch) ? "Pitch" : "BlockLinear";
        DebugPrintf(MSGID(), "RELOC_SURFACE_PROPERTY: %s = %s\n", m_Property.c_str(), layout.c_str());
    }
    else if (m_Property == "AA_MODE")
    {
        string AAMode_str[] = {"AAMODE_1X1", "AAMODE_2X1_D3D", "AAMODE_4X2_D3D", "AAMODE_2X1",
            "AAMODE_2X2_REGGRID", "AAMODE_2X2", "AAMODE_4X2", "AAMODE_2X2_VC_4", "AAMODE_2X2_VC_12",
            "AAMODE_4X2_VC_8", "AAMODE_4X2_VC_24", "AAMODE_4X4"};
        DebugPrintf(MSGID(), "RELOC_SURFACE_PROPERTY: %s = %s\n", m_Property.c_str(),
            AAMode_str[GetAAModeFromSurface(surf)].c_str());
    }
    else
        DebugPrintf(MSGID(), "RELOC_SURFACE_PROPERTY: %s = %ull\n", m_Property.c_str(),
        1 == num ? value32 : value64);

    // Patch the offset now
    switch (num)
    {
    case 2:
        {
            UINT64 ov1 = module->Get032(m_Offset, subdev);
            UINT64 ov2 = module->Get032(m_Offset + 4, subdev);

            UINT64 orig_val = m_Swap ? ((ov2 << 32) | ov1) : ((ov1 << 32) | ov2);
            UINT64 new_val = CallwlateValue(orig_val, value64, m_MaskIn);

            ov1 = m_Swap ? new_val & 0xffffffff : new_val >> 32;
            ov2 = m_Swap ? new_val >> 32 : new_val & 0xffffffff;

            module->DoReloc(m_Offset, (UINT32)ov1, subdev);
            module->DoReloc(m_Offset + 4, (UINT32)ov2, subdev);
            break;
        }
    case 1:
    default:
        {
            UINT32 orig_val = module->Get032(m_Offset, subdev);
            UINT32 new_val = (UINT32)CallwlateValue((UINT64)orig_val, (UINT64)value32, m_MaskIn);

            module->DoReloc(m_Offset, new_val, subdev);
            break;
        }
    }

    return OK;
}

void TraceRelocationSurfaceProperty::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromName(m_SurfaceName, pModule, pSurfaces);
}

RC TraceRelocationLong::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 val1 = module->Get032(m_Offset, subdev);
    UINT64 val2 = module->Get032(m_OffsetHi, subdev);

    UINT64 target_offset = 0;
    const MdiagSurf *surf = 0;
    LwRm::Handle hVASpace = 0;
    if (m_pTarget)
    {
        // For peer modules, the subdevice provided here is the remote
        // subdevice (i.e. the subdevice located on the device that does not
        // contain the actual FB memory allocation), and the peer num is
        // the local subdevice (i.e. he subdevice located on the device that
        // contains the actual FB memory allocation)
        if (m_pTarget->IsPeer())
        {
            target_offset = m_pTarget->GetOffset(m_PeerNum, subdev);
        }
        else if (m_PeerNum < Gpu::MaxNumSubdevices)
        {
            target_offset = m_pTarget->GetOffset(m_PeerNum, 0);
        }
        else
        {
            target_offset = m_pTarget->GetOffset();
        }
        hVASpace = m_pTarget->GetVASpaceHandle();
    }
    else
    {
        if (m_Surf == SURFACE_TYPE_CLIPID)
        {
            surf = module->GetClipIDSurface();
            target_offset = surf->GetCtxDmaOffsetGpu();
        }
        else
        {
            surf = module->GetSurface(m_Surf, subdev);
            if (m_PeerNum < Gpu::MaxNumSubdevices)
            {
                target_offset = surf->GetCtxDmaOffsetGpuPeer(m_PeerNum, m_PeerID);
            }
            else
            {
                target_offset = surf->GetCtxDmaOffsetGpu();
            }
            target_offset += surf->GetExtraAllocSize();
        }
        hVASpace = surf->GetGpuVASpace();
    }

    // Callwlate value to patch
    UINT64 new_val = (((val2 & 0xff) << 32) | val1) + target_offset;

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = UINT32(new_val);
            newMapPart[m_OffsetHi][subdev] = InsertBits39to32(val2, new_val);

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(hVASpace, module->GetVASpaceHandle()));

    // Patch the offset now
    module->DoReloc(m_Offset, UINT32(new_val), subdev);
    module->DoReloc(m_OffsetHi, InsertBits39to32(val2, new_val), subdev);

    return OK;
}

void TraceRelocationLong::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    if (m_pTarget)
        GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
    else
        GetSurfacesFromType(m_Surf, pModule, pSurfaces);
}

TraceModule *TraceRelocationLong::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocationSizeLong::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 val1 = module->Get032(m_Offset, subdev);
    UINT64 val2 = module->Get032(m_OffsetHi, subdev);

    // Callwlate value to patch
    UINT64 new_val = (((val2 & 0xff) << 32) | val1) + m_pTarget->GetSize();

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = UINT32(new_val);
            newMapPart[m_OffsetHi][subdev] = InsertBits39to32(val2, new_val);

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    // Patch the offset now
    module->DoReloc(m_Offset, UINT32(new_val), subdev);
    module->DoReloc(m_OffsetHi, InsertBits39to32(val2, new_val), subdev);

    return OK;
}

void TraceRelocationSizeLong::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocationSizeLong::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocationActiveRegion::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    // NOTE: The map isn't implemented for this relocation, but the counter
    // still needs to be updated.  See the top of this file for information.
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    if (m_pTarget == "Z")
    {
        MdiagSurf *surf =
            module->GetSurface(m_pTrace->FindSurface(m_pTarget), subdev);
        UINT32 regionID = surf->GetZlwllRegion();;
        if( m_forcedRegionId >= 0)
        {
            regionID = (UINT32) m_forcedRegionId;
            InfoPrintf("Forcing RELOC_ACTIVE_REGION to 0x%x\n", regionID);
        }
        InfoPrintf("RELOC_ACTIVE_REGION @0x%08x: 0x%08x -> 0x%08x\n",
                m_Offset, module->Get032(m_Offset, subdev),
                DRF_NUM(9097, _SET_ACTIVE_ZLWLL_REGION, _ID,
                    regionID));
        module->DoReloc(m_Offset, DRF_NUM(9097, _SET_ACTIVE_ZLWLL_REGION, _ID,
                    regionID), subdev);

        module->AddZLwllId(surf->GetZlwllRegion());
    }
    else
    {
        LwRm* pLwRm = module->GetTest()->GetLwRmPtr();
        LW0041_CTRL_GET_SURFACE_ZLWLL_ID_PARAMS ZlwllIdParams = {0};
        TraceModule *tmodule = m_pTrace->ModFind(m_pTarget.c_str());

        if( !tmodule ) {
            ErrPrintf("Unknown buffer ('%s') for RELOC_ACTIVE_REGION, need either Z or a filename\n",
                m_pTarget.c_str());
            MASSERT( 0 );
        }
        RC rc = pLwRm->Control(
                tmodule->GetDmaBuffer()->GetMemHandle(),
                LW0041_CTRL_CMD_GET_SURFACE_ZLWLL_ID,
                &ZlwllIdParams, sizeof(ZlwllIdParams));

        if (OK != rc)
            ErrPrintf("Can not get ZlwllIdParams for %s: %s\n",
                    m_pTarget.c_str(), rc.Message());

        MASSERT(OK == rc);

        UINT32 regionID = ZlwllIdParams.zlwllId;
        if( m_forcedRegionId >= 0)
        {
            regionID = (UINT32) m_forcedRegionId;
            InfoPrintf("Forcing RELOC_ACTIVE_REGION to 0x%x\n", regionID);
        }

        InfoPrintf("RELOC_ACTIVE_REGION @0x%08x: 0x%08x -> 0x%08x\n", m_Offset,
                module->Get032(m_Offset, subdev),
                DRF_NUM(9097, _SET_ACTIVE_ZLWLL_REGION, _ID,
                    regionID));
        module->DoReloc(m_Offset,
                DRF_NUM(9097, _SET_ACTIVE_ZLWLL_REGION, _ID,
                    regionID), subdev);
        // only override the active region method, not the assigned regionID
        // otherwise you can end up with issues with the Zlwll sanity check getting run
        module->AddZLwllId(ZlwllIdParams.zlwllId);
    }

    return OK;
}

void TraceRelocationActiveRegion::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    if (m_pTarget == "Z")
    {
        GetSurfacesFromType(m_pTrace->FindSurface(m_pTarget),
                            pModule, pSurfaces);
    }
    else
    {
        GetSurfacesFromTarget(m_pTrace->ModFind(m_pTarget.c_str()),
                              pModule, pSurfaces);
    }
}

RC TraceRelocationType::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    // Note that we are patching the *next* word -- confusing.
    UINT32 orig_val = 0;
    UINT64 target_offset = m_pTarget->GetOffsetWithinDmaBuf();
    UINT32 new_val = 0;
    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            // Note that we are patching the *next* word -- confusing.
            UINT32 master_subdev = 0;
            orig_val = module->Get032(m_Offset+4, master_subdev);

            TraceModule::SubdevMaskDataMap newMapPart;
            new_val = orig_val + target_offset;
            newMapPart[m_Offset+4][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(m_pTarget->GetVASpaceHandle(), module->GetVASpaceHandle()));

    // Patch the offset now

    // Note that we are patching the *next* word -- confusing.
    orig_val = module->Get032(m_Offset+4, subdev);
    new_val = orig_val + target_offset;
    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ",
            m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n", orig_val, new_val);

    module->DoReloc(m_Offset + 4, new_val, subdev);

    return OK;
}

void TraceRelocationType::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocationType::GetTarget() const
{
    return m_pTarget;
}

UINT64 TraceRelocationMask::MaskedValue(UINT64 val, UINT64 mask) const
{
    UINT64 ret = 0;
    for (UINT64 i = 1, zeroes = 0; i; i <<= 1)
    {
        if (i & mask)
            ret |= (i & val) >> zeroes;
        else
            ++zeroes;
    }
    return ret;
}

UINT64 TraceRelocationMask::UnMaskedValue(UINT64 val, UINT64 mask) const
{
    UINT64 ret = 0;
    for (UINT64 i = 1, zeroes = 0; i; i <<= 1)
    {
        if (i & mask)
            ret |= ((i >> zeroes) & val) << zeroes;
        else
            ++zeroes;
    }
    return ret;
}

UINT64 TraceRelocationMask::CallwlateValue(UINT64 orig_val, UINT64 new_val, UINT64 mask_in) const
{
    UINT64 value = 0;

    if (m_Add)
    {
        // FakeGL writes original value in the offset in order to
        // make address meaningful for trace_disassembler use
        UINT64 tmp = MaskedValue(orig_val, m_mask) +
                     MaskedValue(new_val, mask_in);
        value = (orig_val & ~m_mask) | UnMaskedValue(tmp, m_mask);
    }
    else
    {
        // Override the original value
        value = UnMaskedValue(MaskedValue(new_val, mask_in), m_mask);
    }

    return value;
}

UINT64 TraceRelocationMask::CallwlateScaledValue
(
    UINT64 origVal,
    UINT64 scale,
    UINT64 maskIn
) const
{
    UINT64 tmp = MaskedValue(origVal, m_mask) * MaskedValue(scale, maskIn);
    UINT64 newValue = (origVal & ~m_mask) | UnMaskedValue(tmp, m_mask);

    return newValue;
}

TraceModule *TraceRelocationMask::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocationBranch::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    //actual relocation
    UINT64 dw1 = module->Get032(m_Offset, subdev);
    UINT64 dw2 = module->Get032(m_Offset+4, subdev);
    UINT64 orig_val = m_Swap ? (dw2 << 32 | dw1) : (dw1 << 32 | dw2);

    UINT64 target_offset = m_pTarget->GetOffsetWithinDmaBuf();

    // Callwlate value to patch
    UINT64 new_val = CallwlateValue(orig_val, target_offset, m_MaskIn);

    dw1 = m_Swap ? new_val & 0xffffffff : new_val >> 32;
    dw2 = m_Swap ? new_val >> 32 : new_val & 0xffffffff;

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = static_cast<UINT32>(dw1);
            newMapPart[m_Offset+4][subdev] = static_cast<UINT32>(dw2);

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(m_pTarget->GetVASpaceHandle(), module->GetVASpaceHandle()));

    // Patch the offset now
    // Patch the offset now

    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : %llx -> %llx\n",
                m_pTarget->GetFileType(),
                GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
                m_Offset, orig_val, new_val);

    module->DoReloc(m_Offset, static_cast<UINT32>(dw1), subdev);
    module->DoReloc(m_Offset+4, static_cast<UINT32>(dw2), subdev);

    return OK;
}

void TraceRelocationBranch::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceRelocation64::TraceRelocation64(UINT64 mask_out,
                                     UINT64 mask_in,
                                     UINT32 offset,
                                     bool swap,
                                     string name,
                                     bool peer,
                                     bool badd,
                                     TraceModule* target,
                                     TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap, /*= 0*/
                                     UINT32 peerNum, /* = Gpu::MaxNumSubdevices */
                                     UINT32 peerID, /* = USE_DEFAULT_RM_MAPPING */
                                     UINT64 signExtendMask, /* = 0 */
                                     string signExtendMode /* = "" */) :
            TraceRelocationMask(mask_out, offset, target, badd, pSubdevMaskDataMap, peerNum, peerID),
            m_Swap(swap), 
            m_MaskIn(mask_in), 
            m_SurfName(name), 
            m_Peer(peer), 
            m_SignExtendMask(signExtendMask), m_SignExtendMode(signExtendMode)
{
}

RC TraceRelocation64::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    // bug 200587320
    // no need to read high/low 32bits if this reloc action does not want to update the high/low 32bits
    // ignore32Bits is required to support the cornor RELOC64 for QMD4.0.
    bool ignore32Bits = false;
    if ((m_Swap && ((m_mask >> 32) == 0)) ||
        (!m_Swap && ((m_mask && 0xffffffff) == 0)))
    {
        ignore32Bits = true;
    }

    UINT64 ov1 = module->Get032(m_Offset, subdev);
    UINT64 ov2 = ignore32Bits ? 0 : module->Get032(m_Offset+4, subdev);

    UINT64 orig_val = m_Swap ? ((ov2 << 32) | ov1) : ((ov1 << 32) | ov2);

    UINT64 target_offset;
    TraceModule::SomeKindOfSurface skos;
    module->StringToSomeKindOfSurface(subdev, m_PeerNum, m_SurfName,
        &skos, m_PeerID);

    target_offset = skos.CtxDmaOffset;
    if (module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {

        if (m_Peer)
        {
            MASSERT((skos.SurfaceType == TraceModule::SURF_TYPE_DMABUFFER) ||
                (skos.SurfaceType == TraceModule::SURF_TYPE_PEER));
            MASSERT(subdev < 2); // only two way for now

            // For peer modules, offset returned by
            // StringToSomeKindOfSurface is correct
            if (skos.SurfaceType != TraceModule::SURF_TYPE_PEER)
            {
                MASSERT(m_PeerNum == Gpu::MaxNumSubdevices);
                target_offset = module->SKOS2DmaBufPtr(&skos)->GetCtxDmaOffsetGpuPeer(subdev ? 0 : 1, m_PeerID);
            }
        }
        else
        {
            // Color/Z surfaces could be used here
            MdiagSurf* psurf = (skos.SurfaceType == TraceModule::SURF_TYPE_LWSURF)?
                module->SKOS2LWSurfPtr(&skos):module->SKOS2DmaBufPtr(&skos);

            // Determine which subdevice will be accessing the copy of the
            // surface to be modified.  If a peer address is reloced to
            // this surface, it needs to be callwlated based on how the
            // modified surface is accessed rather than on which subdevice
            // the modified surface is stored.
            UINT32 accessingSubdev = subdev;
            CHECK_RC(module->GetAccessingSubdevice(subdev, &accessingSubdev));
            target_offset = module->GetTest()->GetSSM()->GetCtxDmaOffset(psurf,
                module->GetGpuDev(), accessingSubdev);

            if (m_pTarget)
            {
                // The tracemodule could be suballocated in DmaBuffer.
                // We need to add the offset as well
                target_offset += m_pTarget->GetOffsetWithinDmaBuf();
            }
        }
    }


    if (m_SignExtendMask)
    {
        Trace3DTest *pTest = module->GetTest();
        RegHal& regHal = pTest->GetGpuResource()->GetRegHal(subdev, pTest->GetLwRmPtr(), pTest->GetSmcEngine());

        if ((m_SignExtendMode == "X86_63_47_MATCH" &&
                regHal.Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_X86_63_47_MATCH)) ||
            (m_SignExtendMode == "ARM_63_48_MATCH" &&
                regHal.Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_ARM_63_48_MATCH)) ||
            (m_SignExtendMode == "POWER_63_46_ZERO" &&
                regHal.Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_POWER_63_46_ZERO)) ||
            (m_SignExtendMode == "POWER_63_49_ZERO" &&
                regHal.Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_POWER_63_49_ZERO)) ||
            (m_SignExtendMode == "ARM_55_48_MATCH" &&
                regHal.Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_TEXIO_CONTROL_OOR_ADDR_CHECK_MODE_ARM_55_48_MATCH)))
        {
            // signBitMask is the mask with only sign bit is set.
            UINT64 signBitMask = m_SignExtendMask ^ (m_SignExtendMask & (m_SignExtendMask - 1));
            bool isSignBitSet = target_offset & signBitMask;

            // bitsToExtend is the mask of which bits need to be sign-extended.
            UINT64 bitsToExtend = m_SignExtendMask & ~signBitMask;

            if (target_offset & ~(signBitMask | (signBitMask - 1)))
            {
                ErrPrintf("virtual address of surface %s (0x%llx) overlaps RELOC64 SIGN_EXTEND mask (0x%llx).\n",
                        m_SurfName.c_str(), target_offset, signBitMask);
                return RC::BAD_TRACE_DATA;
            }

            if (isSignBitSet)
            {
                target_offset = target_offset | bitsToExtend;
            }
            else
            {
                target_offset = target_offset & ~bitsToExtend;
            }
        }
    }

    UINT64 new_val = CallwlateValue(orig_val, target_offset, m_MaskIn);

    DebugPrintf(MSGID(), "reloc (%s) @ %08x : %llx -> %llx\n",
            m_SurfName.c_str(),
            m_Offset, orig_val, new_val);

    ov1 = m_Swap ? new_val & 0xffffffff : new_val >> 32;
    ov2 = m_Swap ? new_val >> 32 : new_val & 0xffffffff;

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            if (m_Swap)
            {
                if (0xffffffff & m_mask)
                    newMapPart[m_Offset][subdev] = (UINT32)ov1;
                if (0xffffffff00000000ULL & m_mask)
                    newMapPart[m_Offset+4][subdev] = (UINT32)ov2;
            }
            else
            {
                if (0xffffffff00000000ULL & m_mask)
                    newMapPart[m_Offset][subdev] = (UINT32)ov1;
                if (0xffffffff & m_mask)
                    newMapPart[m_Offset+4][subdev] = (UINT32)ov2;
            }

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(skos.GetVASpaceHandle(), module->GetVASpaceHandle()));

    // Patch the offset now
    module->DoReloc(m_Offset, (UINT32)ov1, subdev);
    if (!ignore32Bits)
    {
        module->DoReloc(m_Offset + 4, (UINT32)ov2, subdev);
    }

    return OK;
}

void TraceRelocation64::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromName(m_SurfName, pModule, pSurfaces);
}

UINT32 OverwriteWithMaskedAndShiftedValue(UINT32 original_value,
                                          UINT32 overwrite_value,
                                          UINT32 number_of_bits,
                                          UINT32 start_bit_position)
{
    UINT32 mask = 0xFFFFFFFF & ( (1<<number_of_bits) - 1);
    mask <<= start_bit_position;

    UINT32 result = original_value & (~mask);
    result |= (overwrite_value << start_bit_position) & mask;

    return result;
}

RC TraceRelocatiolwP2Pitch::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    // Implementation not prepared for other case
    MASSERT( (m_NumberOfBits + m_StartBitPosition) <= 32 );

    UINT32 pitch_value = 0xbad;
    if (m_pTarget->GetVP2PitchValue(&pitch_value) != OK)
    {
        ErrPrintf("RELOC_VP2_PITCH was requested but parameters were not provided");
        return RC::BAD_TRACE_DATA;
    }

    UINT32 orig_val = module->Get032(m_Offset, subdev);
    UINT32 new_val = OverwriteWithMaskedAndShiftedValue(orig_val, pitch_value,
        m_NumberOfBits, m_StartBitPosition);

    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ",
            m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n", orig_val, new_val);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    // Patch the offset now
    module->DoReloc(m_Offset, new_val, subdev);

    return OK;
}

void TraceRelocatiolwP2Pitch::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocatiolwP2Pitch::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocatiolwP2TileMode::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT32 tile_mode_value = 0xbad;

    if (m_pTarget->GetVP2TilingModeID(&tile_mode_value) != OK)
    {
        ErrPrintf("RELOC_VP2_TILEMODE was requested but parameters were not provided");
        return RC::BAD_TRACE_DATA;
    }

    UINT32 orig_val = module->Get032(m_Offset, subdev);
    UINT32 new_val = OverwriteWithMaskedAndShiftedValue(orig_val,
        tile_mode_value, m_NumberOfBits, m_StartBitPosition);

    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ",
            m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n", orig_val, new_val);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    // Patch the offset now
    module->DoReloc(m_Offset, new_val, subdev);

    return OK;
}

void TraceRelocatiolwP2TileMode::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocatiolwP2TileMode::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocationFileMs32::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT32 orig_val = module->Get032(m_Offset, subdev);

    UINT64 target_offset;

    // For peer modules, the subdevice provided here is the remote
    // subdevice (i.e. the subdevice located on the device that does not
    // contain the actual FB memory allocation), and the peer num is
    // the local subdevice (i.e. he subdevice located on the device that
    // contains the actual FB memory allocation)
    if (m_pTarget->IsPeer())
        target_offset = m_pTarget->GetOffset(m_PeerNum, subdev);
    else if (m_PeerNum < Gpu::MaxNumSubdevices)
        target_offset = m_pTarget->GetOffset(m_PeerNum, 0);
    else
        target_offset = m_pTarget->GetOffset();

    if (target_offset & 0xff)
    {
        ErrPrintf("Error in RELOC_MS32 (%d:%s) @ %08x : target offset 0x%02x%08x is not 256-byte aligned!\n",
            m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset,
            UINT32(target_offset >> 32),
            (UINT32)target_offset);
    }

    UINT32 new_val = orig_val + (UINT32)(target_offset >> 8);

    DebugPrintf(MSGID(), "reloc (%d:%s) @ %08x : ",
            m_pTarget->GetFileType(),
            GpuTrace::GetFileTypeData(m_pTarget->GetFileType()).Description,
            m_Offset);
    DebugPrintf(MSGID(), "%08x -> %08x\n", orig_val, new_val);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = new_val;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(m_pTarget->GetVASpaceHandle(), module->GetVASpaceHandle()));

    // Patch the offset now
    module->DoReloc(m_Offset, new_val, subdev);

    return OK;
}

void TraceRelocationFileMs32::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    GetSurfacesFromTarget(m_pTarget, pModule, pSurfaces);
}

TraceModule *TraceRelocationFileMs32::GetTarget() const
{
    return m_pTarget;
}

RC TraceRelocationConst32::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT32 orig_val = module->Get032(m_Offset, subdev);

    DebugPrintf(MSGID(), "reloc (const) @ %08x : %08x -> %08x\n",
            m_Offset,
            orig_val,
            m_Value);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_Offset][subdev] = m_Value;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    // Patch the offset now
    module->DoReloc(m_Offset, m_Value, subdev);

    return OK;
}

RC TraceRelocationZLwll::DoOffset(TraceModule *module, UINT32 subdev)
{
    RC rc;
    CHECK_RC(SliSupported(module));

    // Debugging Purposes: Should we use the map here (if we can)?
    m_UseMap = ShouldUseMap(m_UseMap, subdev);

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 addr;

    if (m_PeerNum < Gpu::MaxNumSubdevices)
        addr = m_Storage->GetCtxDmaOffsetGpuPeer(m_PeerNum, m_PeerID);
    else
        addr = m_Storage->GetCtxDmaOffsetGpu();
    UINT64 limit = addr + m_Storage->GetAllocSize();

    // Get New Data
    UINT64 orig_val = (UINT64(module->Get032(m_OffsetA, subdev) & 0xff) << 32)
                        + module->Get032(m_OffsetB, subdev);
    UINT64 orig_val2 = (UINT64(module->Get032(m_OffsetC, subdev) & 0xff) << 32)
                        + module->Get032(m_OffsetD, subdev);
    UINT64 new_val = orig_val + addr;
    UINT64 new_val2 = orig_val + limit;

    if (new_val & 0xffffff0000000000ll || new_val2 & 0xffffff0000000000ll)
    {
        ErrPrintf("Can not relocate SetZlwllStorage methods "
                  "because address is too big\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 newDataA = InsertBits39to32(orig_val, new_val);
    UINT32 newDataB = UINT32(new_val);
    UINT32 newDataC = InsertBits39to32(orig_val2, new_val2);
    UINT32 newDataD = UINT32(new_val2);

    if (module->GetFileType() == GpuTrace::FT_PUSHBUFFER &&
            module->GetTest()->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // This if is for debugging purposes.  See the comments at the
        // top of this file.
        if (m_UseMap)
        {
            // Don't massage now.  Dump into map for single pass massage later

            TraceModule::SubdevMaskDataMap newMapPart;
            newMapPart[m_OffsetA][subdev] = newDataA;
            newMapPart[m_OffsetB][subdev] = newDataB;
            newMapPart[m_OffsetC][subdev] = newDataC;
            newMapPart[m_OffsetD][subdev] = newDataD;

            CHECK_RC(AddMapPartToMap(&newMapPart, m_pSubdevMaskDataMap));

            return OK;
        }
        else if (subdev != 0)
        {
            // We are in debug mode and we already patched the one and only
            // pushbuffer, so nothing to do.
            return OK;
        }
        else
        {
            // We are in debug mode because we were told not to use the map,
            // and this is subdevice 0, so we need to patch the pushbuffer.
            // Continuing to patch pushbuffer code...
        }
    }

    CHECK_RC(SanityCheckVAS(m_Storage->GetGpuVASpace(), module->GetVASpaceHandle()));

    module->DoReloc(m_OffsetA, newDataA, subdev);
    module->DoReloc(m_OffsetB, newDataB, subdev);
    module->DoReloc(m_OffsetC, newDataC, subdev);
    module->DoReloc(m_OffsetD, newDataD, subdev);

    return rc;
}

void TraceRelocationZLwll::GetSurfaces
(
    TraceModule *pModule,
    SurfaceSet *pSurfaces
) const
{
    MASSERT(pModule != NULL);
    MASSERT(pSurfaces != NULL);
    MASSERT(m_Storage != NULL);

    pSurfaces->insert(m_Storage);
}

RC TraceRelocationScale::DoOffset(TraceModule* module, UINT32 subdev)
{
    RC rc;

    if (module->IsFrozenAt(m_Offset))
        return OK;

    UINT64 valueLower = module->Get032(m_Offset, subdev);
    UINT64 valueUpper = module->Get032(m_Offset2, subdev);
    UINT64 valueFull = m_SwapWords ? ((valueUpper << 32) | valueLower) :
        ((valueLower << 32) | valueUpper);

    UINT64 scale = 1;

    if (m_ScaleByTPCCount)
    {
        scale *= module->GetTest()->GetTPCCount(subdev);
    }
    if (m_ScaleByComputeWarpsPerTPC)
    {
        scale *= module->GetTest()->GetComputeWarpsPerTPC(subdev);
    }
    if (m_ScaleByGraphicsWarpsPerTPC)
    {
        scale *= module->GetTest()->GetGraphicsWarpsPerTPC(subdev);
    }

    const UINT64 maskIn = 0xffffffffffffffffull;
    UINT64 newValueFull = CallwlateScaledValue(valueFull, scale, maskIn);

    DebugPrintf(MSGID(), "reloc_scale 0x%08x + 0x%08x : 0x%llx -> 0x%llx\n",
        m_Offset, m_Offset2, valueFull, newValueFull);

    valueLower = m_SwapWords ? newValueFull & 0xffffffff : newValueFull >> 32;
    valueUpper = m_SwapWords ? newValueFull >> 32 : newValueFull & 0xffffffff;

    module->DoReloc(m_Offset, (UINT32)valueLower, subdev);
    module->DoReloc(m_Offset2, (UINT32)valueUpper, subdev);

    return OK;
}
