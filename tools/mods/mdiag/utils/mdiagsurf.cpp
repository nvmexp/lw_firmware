/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiagsurf.h"

#include "class/cl00f3.h" // LW01_MEMORY_FLA
#include "core/include/modsdrv.h"
#include "core/include/pci.h"
#include "core/include/registry.h"
#include "core/include/utility.h"
#include "cpumodel.h"
#include "ctrl/ctrl90f1.h" // LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS
#include "device/interface/pcie.h"
#include "gpu/utility/surfrdwr.h"
#include "hopper/gh100/dev_ram.h"
#include "mdiag/mdiag.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/tests/gpu/trace_3d/trace_3d.h"
#include "mdiag/utl/utl.h"
#include "mmu/gmmu_fmt.h"
#include "mmuutils.h"
#include "lwRmApi.h"
#include "lwRmReg.h"

#include <queue>
#include <map>

namespace
{
    const ArgReader* GetParams()
    {
        // Test::s_ThreadId2Test is always updated during test setup/running
        // in order to make sure the test fetched through the current thread is right.
        Test *pTest = Test::FindTestForThread(Tasker::LwrrentThread());
        if (pTest != nullptr)
        {
            return pTest->GetParam();
        }
        else
        {
            return Mdiag::GetParam();
        }
    }

    UINT32 GetTestId()
    {
        // Test::s_ThreadId2Test is always updated during test setup/running
        // in order to make sure the test fetched through the current thread is right.
        Test *pTest = Test::FindTestForThread(Tasker::LwrrentThread());
        if (pTest != nullptr)
        {
            return pTest->GetTestId();
        }
        else
        {
            return LWGpuResource::TEST_ID_GLOBAL;
        }
    }

    LWGpuResource * GetLwgpu(const GpuDevice* pGpuDev)
    {
        // Test::s_ThreadId2Test is always updated during test setup/running
        // in order to make sure the test fetched through the current thread is right.
        Test *pTest = Test::FindTestForThread(Tasker::LwrrentThread());
        if (pTest != nullptr && pTest->GetGpuResource() != nullptr)
        {
            return pTest->GetGpuResource();
        }
        else
        {
            return LWGpuResource::GetGpuByDeviceInstance(pGpuDev->GetDeviceInst());           
        }
    }
}

namespace PixelFormatTransform
{
    void DoSwizzle
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size,
        BlockLinearLayout srcLayout,
        BlockLinearLayout dstLayout
    );

    const char* GetBlockLinearName(BlockLinearLayout layout);

    //Address translation table for left half gob.
    //Byte i in naive gob(pitch internal) is stored at
    //byte Naive2FbRawGob_Z24S8[i] in fb_raw gob
    //Spec: //hw/doc/gpu/pascal/pascal/design/Functional_Descriptions/Pascal_Color_Z_Packing.doc
    //Spec: //hw/doc/gpu/maxwell/maxwell/design/Functional_Descriptions/physical-to-raw.vsd
    const UINT08 Naive2FbRawGob_Z24S8[256] = {
        0x00, 0x40, 0x41, 0x42, 0x01, 0x43, 0x44, 0x45,
        0x02, 0x46, 0x47, 0x48, 0x03, 0x49, 0x4A, 0x4B,
        0x20, 0xE0, 0xE1, 0xE2, 0x21, 0xE3, 0xE4, 0xE5,
        0x22, 0xE6, 0xE7, 0xE8, 0x23, 0xE9, 0xEA, 0xEB,
        0x04, 0x4C, 0x4D, 0x4E, 0x05, 0x4F, 0x50, 0x51,
        0x06, 0x52, 0x53, 0x54, 0x07, 0x55, 0x56, 0x57,
        0x24, 0xEC, 0xED, 0xEE, 0x25, 0xEF, 0xF0, 0xF1,
        0x26, 0xF2, 0xF3, 0xF4, 0x27, 0xF5, 0xF6, 0xF7,
        0x08, 0x58, 0x59, 0x5A, 0x09, 0x5B, 0x5C, 0x5D,
        0x0A, 0x7E, 0x5E, 0x5F, 0x0B, 0x60, 0x61, 0x62,
        0x28, 0xC0, 0xC1, 0xC2, 0x29, 0xDE, 0xFE, 0xFF,
        0x2A, 0xF8, 0xF9, 0xFA, 0x2B, 0xFB, 0xFC, 0xFD,
        0x0C, 0x63, 0x64, 0x65, 0x0D, 0x66, 0x67, 0x68,
        0x0E, 0x69, 0x6A, 0x6B, 0x0F, 0x6C, 0x6D, 0x6E,
        0x2C, 0xC3, 0xC4, 0xC5, 0x2D, 0xC6, 0xC7, 0xC8,
        0x2E, 0xC9, 0xCA, 0xCB, 0x2F, 0xCC, 0xCD, 0xCE,
        0x10, 0x6F, 0x70, 0x71, 0x11, 0x72, 0x73, 0x74,
        0x12, 0x75, 0x76, 0x77, 0x13, 0x78, 0x79, 0x7A,
        0x30, 0xCF, 0xD0, 0xD1, 0x31, 0xD2, 0xD3, 0xD4,
        0x32, 0xD5, 0xD6, 0xD7, 0x33, 0xD8, 0xD9, 0xDA,
        0x14, 0x80, 0x81, 0x82, 0x15, 0x83, 0x84, 0x85,
        0x16, 0x7F, 0x9E, 0x9F, 0x17, 0x7B, 0x7C, 0x7D,
        0x34, 0xdb, 0xdc, 0xdd, 0x35, 0xdf, 0xbe, 0xbf,
        0x36, 0xa0, 0xa1, 0xa2, 0x37, 0xa3, 0xa4, 0xa5,
        0x18, 0x86, 0x87, 0x88, 0x19, 0x89, 0x8a, 0x8b,
        0x1a, 0x8c, 0x8d, 0x8e, 0x1b, 0x8f, 0x90, 0x91,
        0x38, 0xa6, 0xa7, 0xa8, 0x39, 0xa9, 0xaa, 0xab,
        0x3a, 0xac, 0xad, 0xae, 0x3b, 0xaf, 0xb0, 0xb1,
        0x1c, 0x92, 0x93, 0x94, 0x1d, 0x95, 0x96, 0x97,
        0x1e, 0x98, 0x99, 0x9a, 0x1f, 0x9b, 0x9c, 0x9d,
        0x3c, 0xb2, 0xb3, 0xb4, 0x3d, 0xb5, 0xb6, 0xb7,
        0x3e, 0xb8, 0xb9, 0xba, 0x3f, 0xbb, 0xbc, 0xbd
    };

    //Return the address translation table
    const UINT08* GetAddrTable
    (
    )
    {
        return &Naive2FbRawGob_Z24S8[0];
    }

    //Retun the corresponding offset in raw(fb/xbar) layout for a given offset of naive gob
    //offsetInNaiveGob : offset in naive gob
    //format: raw surface layout. Should be FB_RAW or XBAR_RAW
    UINT32 GetOffsetInRawGob
    (
        UINT32 offsetInNaiveGob,
        BlockLinearLayout layout,
        GpuDevice *pGpuDevice
    )
    {
        UINT32 offset = 0;
        UINT32 width = pGpuDevice->GobWidth();
        UINT32 height = pGpuDevice->GobHeight();
        UINT32 size = pGpuDevice->GobSize();

        //For a linear addr, callwlate the (x, y) index
        UINT32 x = offsetInNaiveGob & (width -1);
        UINT32 y = offsetInNaiveGob >> log2(width);

        const UINT32 sectorWidth = 16;
        const UINT32 sectorHeight = 2;

        if(layout == XBAR_RAW)
        {
            //offset = (x/32)*256 + (y/4)*128 + ((x%32)/16)*64 + ((y%4)/2) * 32  + (y%2)*16 + (x%16);
            offset =  (x >> log2(width >> 1)) << log2(size >> 1);
            offset += (y >> log2(height >> 1)) << log2(size >> 2);
            offset += ((x & ((width >> 1) -1)) >> log2(sectorWidth)) << log2(size >> 3);
            offset += ((y & ((sectorHeight << 1) - 1)) >> log2(sectorHeight)) << log2(size >> 4);
            offset += (y & (sectorHeight -1)) << log2(sectorWidth);
            offset +=  x & (sectorWidth - 1);
        }
        else if(layout == FB_RAW)
        {
            UINT32 index = y << log2(width >> 1);
            index += x & ((width >> 1) - 1);
            offset = (x >> log2(width >> 1)) << log2(size >> 1);
            const UINT08 *table = GetAddrTable();
            offset += table[index];
        }
        else
        {
            MASSERT(!"Invalid Blocklinear Format");
        }

        return offset;
    }

    //Do swizzle between xbar_raw/fb_raw and naive in gob level
    void ColwertBetweenRawAndNaiveGob
    (
        UINT08 *buf,
        BlockLinearLayout srcLayout,
        BlockLinearLayout dstLayout,
        GpuDevice *pGpuDevice
    )
    {
        const UINT32 std_size = 512;
        UINT32 size = pGpuDevice->GobSize();
        MASSERT(size == std_size);

        vector<UINT08> gob(std_size);
        if(dstLayout == NAIVE_BL)
        {
            //Colwert byte by byte inside gob
            for (UINT32 i = 0; i < size; ++i)
            {
                //Get the corresponding index in raw layout for byte i.
                UINT32 raw_byte_index = GetOffsetInRawGob(i, srcLayout, pGpuDevice);
                gob[i] = buf[raw_byte_index];
            }
        }
        else
        {
            for (UINT32 i = 0; i < size; ++i)
            {
                UINT32 raw_byte_index = GetOffsetInRawGob(i, dstLayout, pGpuDevice);
                gob[raw_byte_index] = buf[i];
            }
        }
        memcpy(buf, &gob[0], size);
    }

    //Do swizzle between xbar_raw/fb_raw bl and naive bl in surface level
    //buff  : surface data ptr
    //size  : surface data size
    //srcFmt: current surface layout
    //dstFmt: target surface layout
    void ColwertBetweenRawAndNaiveBlSurf
    (
        UINT08 *buff,
        UINT32 size,
        BlockLinearLayout srcLayout,
        BlockLinearLayout dstLayout,
        GpuDevice *pGpuDevice
    )
    {
        MASSERT(!(size % pGpuDevice->GobSize()));

        //For different kinds of bl surf, the gob sequence is same.
        //So just need to colwiert inside each gob
        for (UINT32 i = 0; i < size; i += pGpuDevice->GobSize())
        {
            ColwertBetweenRawAndNaiveGob(&buff[i], srcLayout, dstLayout, pGpuDevice);
        }
    }

    //Swizzle surface data from naive bl layout to xbar_raw/fb_raw bl layout.
    //This IF is called at surface clear/initialization phase for direct mapping.
    //surf  : surface ptr
    //size  : surface data size
    void ColwertNaiveToRawSurf
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size
    )
    {
        //Colwert the naive bl surf to xbar_raw or fb_raw bl surf for direct memory mapping
        BlockLinearLayout srcLayout = NAIVE_BL;
        BlockLinearLayout dstLayout = XBAR_RAW;

        GpuSubdevice *pSubdev = surf->GetGpuDev()->GetSubdevice(0);
        INT32 pteKind = pSubdev->GetKindTraits(surf->GetPteKind());

        //use FB RAW for Z24S8 in sysmem as we will use direct mapping to access it
        if ((pteKind & GpuSubdevice::Z24S8) && (surf->GetLocation() != Memory::Fb))
        {
            dstLayout = FB_RAW;
        }

        if (surf->UseCEForMemAccess(0, MdiagSurf::WrSurf))
        {
            dstLayout = XBAR_RAW;
        }

        DoSwizzle(surf, data, size, srcLayout, dstLayout);
    }

    //Swizzle surface data from xbar_raw/fb_raw bl layout to naive bl layout.
    //This IF is called at crc check/image dump phase for direct mapping.
    //surf  : surface ptr
    //size  : surface data size
    void ColwertRawToNaiveSurf
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size
    )
    {
        //Colwert the xbar_raw or fb_raw bl surf to naive bl surf for direct memory mapping
        BlockLinearLayout srcLayout = XBAR_RAW;
        BlockLinearLayout dstLayout = NAIVE_BL;

        GpuSubdevice *pSubdev = surf->GetGpuDev()->GetSubdevice(0);
        INT32 pteKind = pSubdev->GetKindTraits(surf->GetPteKind());

        //use FB RAW for Z24S8 if we use direct mapping to access it
        if ((pteKind & GpuSubdevice::Z24S8) && (surf->GetLocation() != Memory::Fb))
        {
            srcLayout = FB_RAW;
        }

        if (surf->UseCEForMemAccess(0, MdiagSurf::RdSurf))
        {
            srcLayout = XBAR_RAW;
        }

        DoSwizzle(surf, data, size, srcLayout, dstLayout);
    }

    //Do swizzle between xbar_raw/fb_raw bl and naive bl
    void DoSwizzle
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size,
        BlockLinearLayout srcLayout,
        BlockLinearLayout dstLayout
    )
    {
        GpuSubdevice *pSubdev = surf->GetGpuDev()->GetSubdevice(0);
        INT32 pteKind = pSubdev->GetKindTraits(surf->GetPteKind());
        if ((pteKind & GpuSubdevice::Pitch)
            || ((pteKind & GpuSubdevice::Generic_Memory) && (surf->GetLayout() == Surface2D::Pitch)))
        {
            DebugPrintf("%s: Need not do swizzle for pitch surface %s.\n",
                         __FUNCTION__, surf->GetName().c_str());
            return;
        }

        ColwertBetweenRawAndNaiveBlSurf(data, size, srcLayout, dstLayout,
                                        surf->GetGpuDev());
        DebugPrintf("%s: Colwert surface %s from %s to %s.\n",
                    __FUNCTION__, surf->GetName().c_str(),
                    GetBlockLinearName(srcLayout), GetBlockLinearName(dstLayout));
    }

    const char*  GetBlockLinearName(BlockLinearLayout layout)
    {
        switch (layout)
        {
            case FB_RAW:
                return "fb_raw bl";
            case XBAR_RAW:
                return "xbar_raw bl";
            case NAIVE_BL:
                return "naive bl";
            default:
                MASSERT(0);
                return "illegal value";
        }
    }
}

MdiagSurf::MdiagSurf()
{
    m_ParentSurface = nullptr;
    m_AtsPageSize = 0;
    m_AtsReadPermission = true;
    m_AtsWritePermission = true;
    m_NoGmmuMap = false;
    m_PhysAlloc = nullptr;
    m_PhysAddrConstraint = None;
    m_FixedPhysAddr = _UINT64_MAX;
    m_FixedPhysAddrMin = _UINT64_MAX;
    m_FixedPhysAddrMax = _UINT64_MAX;
    m_VirtAddrConstraint = None;
    m_FixedVirtAddr = _UINT64_MAX;
    m_FixedVirtAddrMin = _UINT64_MAX;
    m_FixedVirtAddrMax = _UINT64_MAX;
    m_ExternalTraceMem = 0;
    m_AccessingGpuDevice = nullptr;
    m_HostingGpuDevice = nullptr;
    m_useAlignedHeight = false;
    m_GmmuMappingSent = false;
    m_SurfIommuVAType = Mdiag::IsSmmuEnabled() ? GPA : No;

    m_Surface2D.SetAddressModel(Memory::Paging);
    m_Surface2D.SetAmodelRestrictToFb(false);

}

MdiagSurf::~MdiagSurf()
{
    if (Utl::HasInstance())
    {
        Utl::Instance()->RemoveSurface(this);
    }

    if (!m_Surface2D.GetDmaBufferAlloc())
    {
        Free();
    }
}

RC MdiagSurf::SetSurfaceViewParent(const MdiagSurf* surface, UINT64 offset)
{
    RC rc = m_Surface2D.SetSurfaceViewParent(surface->GetSurf2D(), offset );

    if (rc == OK)
    {
        m_ParentSurface = surface;
    }

    return rc;
}

bool MdiagSurf::GetCompressed() const
{
    return m_ParentSurface != nullptr? m_ParentSurface->GetCompressed() : m_Surface2D.GetCompressed();
}

RC MdiagSurf::DownloadWithCE
(
    const ArgReader* params,
    GpuVerif* gpuVerif,
    UINT08* Data,
    UINT32 DataSize,
    UINT64 bufOffset,
    bool repeat,
    UINT32 subdev
)
{
    RC rc;

    UINT32 allocSize = max(DataSize,
            (UINT32)(GetAllocSize()&0xffffffff));
    // Get the surface copier
    gpuVerif->GetDmaUtils()->SetupDmaBuffer(allocSize, subdev);
    gpuVerif->GetDmaUtils()->SetupSurfaceCopier(subdev);
    DmaReader* pCopier = gpuVerif->GetDmaUtils()->GetSurfaceReader()->GetSurfaceCopier();

    pCopier->AllocateBuffer(GetPageLayoutFromParams(params, NULL),  allocSize);
    MdiagSurf* pSurface = pCopier->GetCopySurface();

    // Copy buffer to the surface copier's temp surface using CPU
    auto pWriter = SurfaceUtils::CreateMappingSurfaceWriter(pSurface);
    pWriter->SetTargetSubdevice(subdev);
    if (repeat)
    {
        UINT64 totalSize = GetSize() + GetExtraAllocSize();
        for (UINT64 Offset=0; Offset < totalSize; Offset += DataSize)
        {
            const size_t CopySize = min(static_cast<size_t>(DataSize),
                    static_cast<size_t>(totalSize - Offset));
            CHECK_RC(pWriter->WriteBytes(Offset, Data, CopySize, Surface2D::MapDirect));
        }
    }
    else
    {
        UINT32 writeSize = min(DataSize, (UINT32)(GetAllocSize()&0xffffffff));
        CHECK_RC(pWriter->WriteBytes(bufOffset, Data, writeSize, Surface2D::MapDirect));
    }

    LwRm* pLwRm = GetLwRmPtr();
    UINT32 offset = 0;
    queue<UINT32> savedKinds;
    queue<UINT32> savedFlags;

    if (!m_NoGmmuMap)
    {
        // save pte kind if surface is gmmu mapped
        MASSERT(GetActualPageSizeKB() > 0);
        while (offset < GetAllocSize())
        {
            LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = { 0 };
            getParams.gpuAddr = GetCtxDmaOffsetGpu() + offset;
            getParams.hVASpace = GetGpuVASpace();

            CHECK_RC(pLwRm->ControlByDevice(
                        GetGpuDev(),
                        LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                        &getParams, sizeof(getParams)));

            LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
            setParams.gpuAddr = getParams.gpuAddr;
            setParams.hVASpace = GetGpuVASpace();
            for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
            {
                setParams.pteBlocks[ii] = getParams.pteBlocks[ii];
                savedKinds.push(getParams.pteBlocks[ii].kind);
                savedFlags.push(getParams.pteBlocks[ii].pteFlags);

                if (setParams.pteBlocks[ii].pageSize == 0 ||
                    FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                        _FALSE, setParams.pteBlocks[ii].pteFlags))
                {
                    continue;
                }

                UINT32 pteKind;
                GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
                pSubdev->GetPteKindFromName("SMSKED_MESSAGE", &pteKind);
                if (getParams.pteBlocks[ii].kind == pteKind && m_FlaImportMem == nullptr)
                {
                    UINT32 pteKindBL;
                    if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY))
                    {
                        pSubdev->GetPteKindFromName("GENERIC_MEMORY", &pteKindBL);
                    }
                    else
                    {
                        pSubdev->GetPteKindFromName("GENERIC_16BX2", &pteKindBL);
                    }
                    setParams.pteBlocks[ii].kind = pteKindBL;
                }
                setParams.pteBlocks[ii].pteFlags =
                    FLD_SET_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS,
                        _SHADER_ACCESS, _READ_WRITE,
                        setParams.pteBlocks[ii].pteFlags);
                setParams.pteBlocks[ii].pteFlags =
                    FLD_SET_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS,
                        _READ_ONLY, _FALSE, setParams.pteBlocks[ii].pteFlags);
            }

            CHECK_RC(pLwRm->ControlByDevice(GetGpuDev(),
                        LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                        &setParams, sizeof(setParams)));

            offset += GetActualPageSizeKB() * 1024;
        }
    }

    UINT64 SrcOffset = pSurface->GetCtxDmaOffsetGpu();
    UINT64 DstOffset = GetCtxDmaOffsetGpu();

    const UINT32 chunkSize = params->ParamUnsigned("-surfaceinit_ce_chunk_size", 1024*1024);
    MASSERT(chunkSize);

    UINT32 writeSize = allocSize;
    if (!repeat)
    {
        writeSize = min(DataSize, (UINT32)(GetAllocSize()&0xffffffff));
        SrcOffset += bufOffset;
        DstOffset += bufOffset;
    }

    while (writeSize > 0)
    {
        if (writeSize > chunkSize)
        {
            CHECK_RC(pCopier->ReadLine(pSurface->GetCtxDmaHandle(),
                        SrcOffset,
                        0, 1, chunkSize, 0, pSurface->GetSurf2D(), this, DstOffset));
            writeSize -= chunkSize;
            SrcOffset += chunkSize;
            DstOffset += chunkSize;
        }
        else
        {
            CHECK_RC(pCopier->ReadLine(pSurface->GetCtxDmaHandle(),
                        SrcOffset,
                        0, 1, writeSize, 0, pSurface->GetSurf2D(), this, DstOffset));
            writeSize = 0;
        }
    }

    if (!m_NoGmmuMap)
    {
        // restore pte kind if surface is gmmu mapped
        offset = 0;

        while (offset < GetAllocSize())
        {
            LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = { 0 };
            getParams.gpuAddr = GetCtxDmaOffsetGpu() + offset;
            getParams.hVASpace = GetGpuVASpace();

            CHECK_RC(pLwRm->ControlByDevice(
                        GetGpuDev(),
                        LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                        &getParams, sizeof(getParams)));

            LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
            setParams.gpuAddr = getParams.gpuAddr;
            setParams.hVASpace = GetGpuVASpace();

            for (UINT32 ii = 0; ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
            {
                setParams.pteBlocks[ii] = getParams.pteBlocks[ii];
                MASSERT(savedKinds.size() > 0);
                setParams.pteBlocks[ii].kind = savedKinds.front();
                setParams.pteBlocks[ii].pteFlags = savedFlags.front();
                savedKinds.pop();
                savedFlags.pop();
            }
            CHECK_RC(pLwRm->ControlByDevice(GetGpuDev(),
                        LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                        &setParams, sizeof(setParams)));

            offset += GetActualPageSizeKB() * 1024;
        }
    }
    return OK;
}

RC MdiagSurf::SendSurfaceTypeEscapeWriteBuf
(
    const UINT64 virtAddrBase,
    const UINT64 virtAddrOffset,
    const UINT64 virtAllocSize,
    const UINT64 physAddrBase,
    const UINT64 physAddrOffset,
    const bool isSparse,
    const string virtSurfName
)
{
    RC rc;
    string aperture;
    GpuDevice* pGpuDev = GetGpuDev();

    if (GetLoopBack())
    {
        aperture = "Peer";
    }
    else if (GetLocation() == Memory::Fb)
    {
        aperture = "VidMem";
    }
    else if (GetLocation() == Memory::Coherent)
    {
        aperture = "CohSysMem";
    }
    // Send "SysMem" for Non-Coh sysmem due to legacy reasons
    // See https://lwbugs/3507581/6
    else
    {
        aperture = "SysMem";
    }
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        CHECK_RC(SendSurfaceTypeEscapeWriteBuf(pGpuDev->GetSubdevice(subdev), virtAddrBase, virtAddrOffset, virtAllocSize, physAddrBase, physAddrOffset, aperture, isSparse, virtSurfName));
    }

    return rc;
}

RC MdiagSurf::SendSurfaceTypeExtendedEscapeWriteBuf
(
    GpuSubdevice* gpuSubDev,
    const UINT64 virtAddrBase,
    const UINT64 virtAddrOffset,
    const UINT64 virtAllocSize,
    const string aperture,
    const UINT32 vaSpaceId,
    const UINT64 physAddrBase,
    const UINT64 physAddrOffset,
    const UINT64 mappedSize,
    const UINT32 pageSizeBit,
    const UINT32 pteKind,
    const bool isSparse,
    const string mappedSurfName,
    const string virtSurfName
)
{
    RC rc;
    const size_t bufferSize =
                    sizeof(UINT64) +          // virtAddrBase
                    sizeof(UINT64) +          // virtAddrOffset
                    sizeof(UINT64) +          // virtAllocSize
                    aperture.size() + 1 +     // aperture
                    sizeof(UINT32) +          // vaSpaceId
                    sizeof(UINT64) +          // physAddrBase
                    sizeof(UINT64) +          // physAddrOffset
                    sizeof(UINT64) +          // Mapped size
                    sizeof(UINT32) +          // pageSizeBit
                    sizeof(UINT32) +          // pteKind
                    sizeof(bool)   +          // sparse
                    mappedSurfName.size() + 1+// mappedSurfName
                    virtSurfName.size() + 1;  // VirtSurfName


    vector<UINT08> escwriteBuffer(bufferSize);
    UINT08* buf = &escwriteBuffer[0];

    // Virtual Address Base
    *(UINT64*)buf = virtAddrBase;
    buf += sizeof(UINT64);

    // Virtual Address Offset
    *(UINT64*)buf = virtAddrOffset;
    buf += sizeof(UINT64);

    // virtAllocSize
    *(UINT64*)buf = virtAllocSize;
    buf += sizeof(UINT64);

    // Aperture Type- VidMem/CohSysMem/SysMem/Peer
    memcpy(buf, aperture.c_str(), aperture.size());
    buf += aperture.size();
    *buf = '\0';
    ++buf;

    // vaSpaceId
    *(UINT32*)buf = vaSpaceId;
    buf += sizeof(UINT32);

    // Physical Address Base
    *(UINT64*)buf = physAddrBase;
    buf += sizeof(UINT64);

    // Physical Address Offset
    *(UINT64*)buf = physAddrOffset;
    buf += sizeof(UINT64);

    // Mapped size
    *(UINT64*)buf = isSparse ? 0 : mappedSize;
    buf += sizeof(UINT64);

    // pageSizeBit
    *(UINT32*)buf = pageSizeBit;
    buf += sizeof(UINT32);

    // pteKind
    *(UINT32*)buf = pteKind;
    buf += sizeof(UINT32);

    // sparse from the ALLOC_VIRTUAL only
    *(bool*)buf = isSparse;
    buf += sizeof(bool);

    // mappedSurfaceName
    memcpy(buf, mappedSurfName.c_str(), mappedSurfName.size());
    buf += mappedSurfName.size();
    *buf = '\0';
    ++buf;

    // virtualSurfaceName
    memcpy(buf, virtSurfName.c_str(), virtSurfName.size());
    buf += virtSurfName.size();
    *buf = '\0';
    ++buf;

    gpuSubDev->EscapeWriteBuffer("SurfaceTypeExtended", 0, bufferSize, &escwriteBuffer[0]);

    return rc;
}

RC MdiagSurf::SendSurfaceTypeEscapeWriteBuf
(
    GpuSubdevice* gpuSubDev,
    const UINT64 virtAddrBase,
    const UINT64 virtAddrOffset,
    const UINT64 virtAllocSize,
    const UINT64 physAddrBase,
    const UINT64 physAddrOffset,
    const string aperture,
    const bool isSparse,
    const string virtSurfName
)
{
    RC rc;
    const UINT64 mappedSize = GetSize();
    const string surfName = GetName();

    // If PageSize is not set then send default pageSize = 64K for Amodel
    const UINT32 pageSizeBit = log2((GetPageSize() ? GetPageSize() : 64) * 1024);
    const UINT32 pteKind = GetPteKind();

    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
    params.hVASpace = GetVASpaceHandle(GetVASpace());
    rc = GetLwRmPtr()->Control(GetLwRmPtr()->GetDeviceHandle(gpuSubDev->GetParentDevice()),
         LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS, &params, sizeof (params));
    if (rc != OK)
    {
        ErrPrintf("%s: LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS returned an error\n", __FUNCTION__);
        return rc;
    }

    const UINT32 vaSpaceId = static_cast<UINT32>(params.vaSpaceId);

    UINT32 supportsSurfaceTypeExtended = 0;
    if ((0 == gpuSubDev->EscapeReadBuffer("supported/SurfaceTypeExtended", 0, sizeof(LwU32), &supportsSurfaceTypeExtended)) &&
        (supportsSurfaceTypeExtended != 0))
    {
        return SendSurfaceTypeExtendedEscapeWriteBuf(
            gpuSubDev,
            virtAddrBase,
            virtAddrOffset,
            virtAllocSize,
            aperture,
            vaSpaceId,
            physAddrBase,
            physAddrOffset,
            mappedSize,
            pageSizeBit,
            pteKind,
            isSparse,
            surfName,
            virtSurfName);
    }

    const size_t bufferSize =
                    sizeof(UINT64) +        // virtAddr
                    sizeof(UINT64) +        // allocSize
                    aperture.size() + 1 +   // aperture
                    sizeof(UINT32) +        // vaSpaceId
                    sizeof(UINT64) +        // physAddr
                    sizeof(UINT32) +        // pageSizeBit
                    sizeof(UINT32) +        // pteKind
                    surfName.size() + 1;    // surfName

    vector<UINT08> escwriteBuffer(bufferSize);
    UINT08* buf = &escwriteBuffer[0];

    // Virtual Address
    *(UINT64*)buf = virtAddrBase + virtAddrOffset;
    buf += sizeof(UINT64);

    // Mapped size
    *(UINT64*)buf = mappedSize;
    buf += sizeof(UINT64);

    // Aperture Type- VidMem/CohSysMem/SysMem/Peer
    memcpy(buf, aperture.c_str(), aperture.size());
    buf += aperture.size();
    *buf = '\0';
    ++buf;

    // vaSpaceId
    *(UINT32*)buf = vaSpaceId;
    buf += sizeof(UINT32);

    // Physical Address
    *(UINT64*)buf = physAddrBase + physAddrOffset;
    buf += sizeof(UINT64);

    // pageSizeBit
    *(UINT32*)buf = pageSizeBit;
    buf += sizeof(UINT32);

    // pteKind
    *(UINT32*)buf = pteKind;
    buf += sizeof(UINT32);

    // surfName
    memcpy(buf, surfName.c_str(), surfName.size());
    buf += surfName.size();
    *buf = '\0';
    ++buf;

    gpuSubDev->EscapeWriteBuffer("SurfaceType", 0, bufferSize, &escwriteBuffer[0]);

    return rc;
}

RC MdiagSurf::Alloc
(
    GpuDevice *gpudev,
    LwRm *pLwRm
)
{
    MASSERT(gpudev);
    RC rc;

    // Override the gpu device for physical allocation if a remote hosting
    // device is specified. The input gpu device is still used for accessing
    // through virtual addressing.
    if (m_HostingGpuDevice != nullptr)
    {
        MASSERT(gpudev != nullptr);
        MASSERT(m_HostingGpuDevice->GetDeviceInst() != gpudev->GetDeviceInst());
        m_AccessingGpuDevice = gpudev;
        gpudev = m_HostingGpuDevice;
    }

    // If the requested ATS page size is greater than the requested GMMU page
    // size, the surface may need some extra alignment and padding to make sure
    // that the ATS mappings are on the proper ATS page boundaries.
    // (RM doesn't know about the ATS page size.)
    if (GetAtsPageSize() > GetPageSize())
    {
        UINT64 atsPageSizeBytes = GetAtsPageSize() << 10;

        // Make sure that the physical and virtual addresses of the allocation
        // are aligned to the ATS page size, assuming a greater alignment
        // wasn't requested by the user.

        if (atsPageSizeBytes > GetAlignment())
        {
            SetAlignment(atsPageSizeBytes);
        }
        if (atsPageSizeBytes > GetVirtAlignment())
        {
            SetVirtAlignment(atsPageSizeBytes);
        }

        // Pad the size of the allocation up to the nearest
        // ATS page size boundary.

        UINT64 expectedSize = m_Surface2D.EstimateSize(gpudev);
        UINT64 alignedSize = ALIGN_UP(expectedSize, atsPageSizeBytes);
        UINT64 extraSize = alignedSize - expectedSize;
        m_Surface2D.SetArrayPitch(extraSize +
            m_Surface2D.GetArrayPitch());
    }

    if (m_NoGmmuMap && HasMap())
    {
        MASSERT(HasVirtual() && HasPhysical());
        SetSpecialization(Surface2D::VirtualPhysicalNoMap);
    }

    const ArgReader* params = GetParams();

    if (HasVirtAddrRange() &&
        params != nullptr &&
        params->ParamPresent("-va_range_limit") > 0)
    {
        const UINT64 vaRangeLimit = params->ParamUnsigned64("-va_range_limit");
        if (vaRangeLimit < GetVirtAddrRangeMax())
        {
            SetVirtAddrRange(GetVirtAddrRangeMin(), vaRangeLimit);
        }
    }

    // PLC compression can have non-deterministic differences between fmodel
    // and RTL.  If -RawImages is specified, this can cause CRC failures,
    // so disable PLC compression here to prevent this.  (See bug 2046546.)
    if ((Platform::GetSimulationMode() != Platform::Amodel) &&
        (gpudev != nullptr) &&
        gpudev->GetSubdevice(0)->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY) &&
        ((LWOS32_ATTR_COMPR_REQUIRED == GetCompressedFlag()) ||
            (LWOS32_ATTR_COMPR_ANY == GetCompressedFlag())) &&
        (params != nullptr) &&
        params->ParamPresent("-RawImageMode") &&
        (RAWOFF != (_RAWMODE)params->ParamUnsigned("-RawImageMode")))
    {
        SetCompressedFlag(LWOS32_ATTR_COMPR_DISABLE_PLC_ANY);
    }

    if ((params != nullptr) &&
        (params->ParamPresent("-using_external_memory") > 0) &&
        HasFixedPhysAddr())
    {
        CHECK_RC(AllocateExternalTraceMemory(gpudev, pLwRm));
    }

    if (m_FlaImportMem != nullptr)
    {
        CHECK_RC(AllocateFlaImportMemory(gpudev, pLwRm));

        // When in loopback mode, Surface2D creates the peer mapping and sets
        // the flags. Otherwise, we have to set it here.
        if (m_FlaImportMem->pDestinationSurface != nullptr &&
            gpudev->GetDeviceInst() !=
            m_FlaImportMem->pDestinationSurface->GetGpuDev()->GetDeviceInst())
        {
            MASSERT(!GetLoopBack());
            auto pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            CHECK_RC(pGpuDevMgr->AddPeerMapping(pLwRm,
                    gpudev->GetSubdevice(0),
                    m_FlaImportMem->pDestinationSurface->GetGpuDev()->GetSubdevice(0)));

            m_Surface2D.SetPeerMappingFlags(
                DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, 0));
        }
        else
        {
            MASSERT(GetLoopBack());
        }
    }

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        // If PageSize is set and alignment is not set then
        // set the alignment to pagesize
        // PageSize is stored in KB and alignment is stored in B
        if (GetPageSize())
        {
            if (0 == GetAlignment())
            {
                SetAlignment(GetPageSize()*1024);
            }
            if (0 == GetVirtAlignment())
            {
                SetVirtAlignment(GetPageSize()*1024);
            }
        }
    }

    if (GetLoopBack() && IsAtsMapped() && m_NoGmmuMap)
    {
        SetIsSPAPeerAccess(true);    
    }

    {
        bool disabledLocationOverride = false;
        INT32 oldLocationOverride = Surface2D::NO_LOCATION_OVERRIDE;
        if (params != nullptr &&
            params->ParamPresent("-disable_location_override_for_mdiag"))
        {
            oldLocationOverride = Surface2D::GetLocationOverride();
            Surface2D::SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
            disabledLocationOverride = true;
        }
        DEFER
        {
            if (disabledLocationOverride)
            {
                Surface2D::SetLocationOverride(oldLocationOverride);
            }
        };

        CHECK_RC(m_Surface2D.Alloc(gpudev, pLwRm));
    }

    // Send SurfaceType Escape Write for Amodel
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        UINT64 physAddr = ~0ULL;

        // This is for MAP command
        if (HasMap())
        {
            if (GetMemHandle())
            {
                CHECK_RC(GetPhysAddress(0, &physAddr));
            }
            CHECK_RC(SendSurfaceTypeEscapeWriteBuf(m_Surface2D.GetCtxDmaOffsetGpu(), 0, GetSize(), physAddr, 0, false, ""));
        }
        // This is for ALLOC_VIRTUAL command. The escape write it sent only for sparse virtual surfaces only.
        else if (IsVirtualOnly() && GetIsSparse())
        {
            CHECK_RC(SendSurfaceTypeEscapeWriteBuf(m_Surface2D.GetCtxDmaOffsetGpu(), 0, GetSize(),  physAddr, 0, true, GetName()));
        }
    }

    bool usingCpuModelTest =
        ((params != nullptr) && (params->ParamPresent("-cpu_model_test") > 0));

    if (usingCpuModelTest && Platform::IsSelfHosted())
    {
        ErrPrintf("-cpu_model_test conflicts with self-hosted mode.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (HasVirtual() && HasPhysical())
    {
        UINT64 virtAddr = m_Surface2D.GetCtxDmaOffsetGpu();

        // Send the relevant mapping info to CPU model/Smmu driver,
        // if this surface should be ATS/inline mapped.
        if (m_SurfIommuVAType == GVA ||
            m_SurfIommuVAType == GPA)
        {
            CHECK_RC(CreateIommuMapping(virtAddr, 0, GetLoopBack(),
                GetLoopBackPeerId()));
        }

        // Send GMMU info if a CPU model test is specified.
        if (usingCpuModelTest && !m_NoGmmuMap)
        {
            CHECK_RC(SendGmmuMapping(virtAddr, 0, GetLoopBack(), GetLoopBackPeerId(), false));
        }
    }

    if (IsRemoteAllocated() && HasMap())
    {
        rc = MapPeer(m_AccessingGpuDevice);
        if (OK != rc)
        {
            ErrPrintf("Surface=%s couldn't be mapped on the peer GPU device %d.\n",
                m_Surface2D.GetName().c_str(), m_AccessingGpuDevice->GetDeviceInst());
            return RC::SOFTWARE_ERROR;
        }

        DebugPrintf("Surface=%s mapped as peer on GPU device %d, "
            "hosted on GPU device %d.\n", m_Surface2D.GetName().c_str(),
            m_AccessingGpuDevice->GetDeviceInst(),
            GetGpuDev()->GetDeviceInst());
    }

    GpuSubdevice *pSubdev = GetGpuSubdev(0);
    // In some cases, we need to used aligned height for surface reading
    // Such as Z64 surface
    m_useAlignedHeight = pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY) &&
                         params != nullptr &&
                         params->ParamPresent("-RawImageMode") > 0 &&
                         (_RAWMODE)params->ParamUnsigned("-RawImageMode") == RAWON &&
                         GetColorFormat() == ColorUtils::ZF32_X24S8;
    return rc;
}

//In P2P allocation, surface of one gpu(originating/access) would be remotely mapped to the other gpu(host/remote).
//The accessing gpu would have only a peer VA address. However, if the local offset is used it would access a local VA address.
//      Peer address would point to the physical address on host gpu
//      Local address would be meaningless and could either cause an mmu fault or have a weird side-effect of accessing physical address of some other surface
//The host gpu would a physical address, and a local VA. The local VA wouldn't be used as the trace on host gpu wouldn't be aware of surface of another trace.
//To make sure the accessing gpu uses the peer address, we need to return the peer handle/offset
UINT64 MdiagSurf::GetCtxDmaOffsetGpu(LwRm *pLwRm) const
{
    if (IsRemoteAllocated() && HasMap()) //return peer offset
    {
        DebugPrintf("%s: Returning Peer offset for surface=%s of GpuDevice= %d remote on GpuDevice = %d\n",
                __FUNCTION__, m_Surface2D.GetName().c_str(), m_AccessingGpuDevice->GetDeviceInst(), GetGpuDev()->GetDeviceInst());
        UINT32 gpuSubdInstHost = GetGpuDev()->GetSubdevice(0)->GetSubdeviceInst();
        UINT32 gpuSubdInstAccess = m_AccessingGpuDevice->GetSubdevice(0)->GetSubdeviceInst();
        return GetCtxDmaOffsetGpuPeer(gpuSubdInstHost, m_AccessingGpuDevice, gpuSubdInstAccess);
    }
    else // return local handle
    {
        DebugPrintf("%s: Returning local offset for surface=%s\n",
                __FUNCTION__, m_Surface2D.GetName().c_str());
        return m_Surface2D.GetCtxDmaOffsetGpu( pLwRm );
    }
}

LwRm::Handle MdiagSurf::GetCtxDmaHandleGpu(LwRm *pLwRm) const
{
    if (IsRemoteAllocated() && HasMap()) //return peer handle
    {
        DebugPrintf("%s: Returning Peer handle for surface=%s of GpuDevice= %d remote on GpuDevice = %d\n",
                __FUNCTION__, m_Surface2D.GetName().c_str(), m_AccessingGpuDevice->GetDeviceInst(), GetGpuDev()->GetDeviceInst());
        return GetCtxDmaHandleGpuPeer(m_AccessingGpuDevice, pLwRm);
    }
    else // return local handle
    {
        DebugPrintf("%s: Returning local handle for surface=%s\n",
                __FUNCTION__, m_Surface2D.GetName().c_str());
        return m_Surface2D.GetCtxDmaHandleGpu( pLwRm );
    }
}

// Sets remote hosting device. Setting nullptr disables remote hosting.
void MdiagSurf::SetSurfaceAsRemote(GpuDevice* hostingDevice)
{
    m_HostingGpuDevice = hostingDevice;
}

bool MdiagSurf::IsRemoteAllocated() const
{
    return IsAllocated() && m_AccessingGpuDevice != nullptr;
}

RC MdiagSurf::CreateIommuMapping
(
    UINT64 virtAddr,
    UINT64 physicalOffset,
    bool usePeer,
    UINT32 peerId
)
{
    RC rc;
    UINT64 offset = 0;

    // If there is no user-specified ATS page size,
    // set it equal to the GMMU page size or 64K, whatever is bigger.
    if (GetAtsPageSize() == 0)
    {
        const UINT64 bigPageSize = GetGpuDev()->GetBigPageSize();
        SetAtsPageSize(max(GetActualPageSizeKB(), UNSIGNED_CAST(UINT32, bigPageSize >> 10)));
    }

    MASSERT(GetAtsPageSize() != 0);
    UINT64 atsPageSize = GetAtsPageSize() << 10;

    LwRm::Handle vaSpaceHandle = GetVASpaceHandle(GetVASpace());
    if (vaSpaceHandle == 0)
    {
        ErrPrintf("IOMMU mapping for surface %s requires non-default address space.\n", GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    LWGpuResource * lwgpu = GetLwgpu(GetGpuDev());
    MASSERT(lwgpu != nullptr);

    string addressSpaceName = lwgpu->GetAddressSpaceName(vaSpaceHandle, GetLwRmPtr());
    if (addressSpaceName.empty())
    {
        ErrPrintf("Missing address space name for surface %s.\n", GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    UINT32 pasid = lwgpu->GetAddressSpacePasid(vaSpaceHandle, GetLwRmPtr());

    string aperture;

    if (usePeer)
    {
        if (peerId == USE_DEFAULT_RM_MAPPING)
        {
            peerId = 0;
        }
        aperture = Utility::StrPrintf("Peer%d", peerId);
    }
    else if (GetLocation() == Memory::Fb)
    {
        aperture = "Local";
    }
    else if ((GetLocation() == Memory::Coherent) ||
        (GetLocation() == Memory::NonCoherent))
    {
        aperture = "Sysmem";
    }
    else
    {
        ErrPrintf("Unrecognized aperture for ATS mapping of surface %s.\n",
            GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
    MASSERT(pSubdev != nullptr);
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

    // Send information for every page of the surface.
    while (offset < GetAllocSize())
    {
        UINT64 physAddr;
        CHECK_RC(GetPhysAddress(physicalOffset + offset, &physAddr));

        IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
        MASSERT(iommu);

        // https://confluence.lwpu.com/display/ArchMods/GPA+ATS
        // If this surface should be inline/GPA ats mapped,
        // Smmu driver creates inline smmu translated/GPA ats mapping for (GPA, GPA),
        // based on that GPA equals to SPA.
        CHECK_RC(iommu->CreateAtsMapping(
            m_Surface2D.GetName(),
            addressSpaceName,
            aperture,
            pGpuPcie,
            GetTestId(),
            m_SurfIommuVAType == GVA ? pasid : 0,
            atsPageSize,
            m_SurfIommuVAType == GVA ? virtAddr + offset : physAddr,
            physAddr,
            m_AtsReadPermission,
            m_AtsWritePermission));

        offset += atsPageSize;
    }

    return rc;
}

RC MdiagSurf::MapVirtToPhys
(
    GpuDevice *gpudev,
    MdiagSurf *virtAlloc,
    MdiagSurf *physAlloc,
    UINT64 virtualOffset,
    UINT64 physicalOffset,
    LwRm* pLwRm
)
{
    RC rc;

    m_PhysAlloc = physAlloc;

    // There isn't a way to set P2P loopback mode on a MAP trace header
    // command, so grab the value from the physical allocation.
    if (physAlloc->GetLoopBack())
    {
        SetLoopBack(true);
    }

    if (HasMap())
    {
        // When in loopback mode, Surface2D creates the peer mapping and sets the
        // flags. Otherwise, we have to set it here.

        // Remote peer
        if (virtAlloc->GetGpuDev()->GetDeviceInst() != physAlloc->GetGpuDev()->GetDeviceInst())
        {
            MASSERT(gpudev->GetDeviceInst() == virtAlloc->GetGpuDev()->GetDeviceInst());
            MASSERT(GetLocation() == Memory::Fb);
            MASSERT(!GetLoopBack());

            // This lwrrently only adds a single mapping.
            MASSERT(virtAlloc->GetGpuDev()->GetNumSubdevices() == 1);
            MASSERT(physAlloc->GetGpuDev()->GetNumSubdevices() == 1);

            auto* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            CHECK_RC(pGpuDevMgr->AddPeerMapping(pLwRm,
                    virtAlloc->GetGpuDev()->GetSubdevice(0),
                    physAlloc->GetGpuDev()->GetSubdevice(0)));

            m_Surface2D.SetPeerMappingFlags(
                DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, 0));
        }
        // Remote FLA
        else if (physAlloc->HasFlaImportMem() &&
            physAlloc->GetDestinationFlaSurface() != nullptr &&
            virtAlloc->GetGpuDev()->GetDeviceInst() !=
            physAlloc->GetDestinationFlaSurface()->GetGpuDev()->GetDeviceInst())
        {
            MASSERT(gpudev->GetDeviceInst() == virtAlloc->GetGpuDev()->GetDeviceInst());
            MASSERT(gpudev->GetDeviceInst() == physAlloc->GetGpuDev()->GetDeviceInst());
            MASSERT(GetLocation() == Memory::Fb);
            MASSERT(!GetLoopBack());

            auto pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            CHECK_RC(pGpuDevMgr->AddPeerMapping(pLwRm,
                    virtAlloc->GetGpuDev()->GetSubdevice(0),
                    physAlloc->GetDestinationFlaSurface()->GetGpuDev()->GetSubdevice(0)));

            m_Surface2D.SetPeerMappingFlags(
                DRF_DEF(OS46, _FLAGS, _P2P_ENABLE, _NOSLI)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_SRC, 0)
                | DRF_NUM(OS46, _FLAGS, _P2P_SUBDEV_ID_TGT, 0));
        }
        // Local FLA
        else if (physAlloc->HasFlaImportMem())
        {
            MASSERT(gpudev->GetDeviceInst() == virtAlloc->GetGpuDev()->GetDeviceInst());
            MASSERT(gpudev->GetDeviceInst() == physAlloc->GetGpuDev()->GetDeviceInst());
            MASSERT(GetLocation() == Memory::Fb);
            MASSERT(GetLoopBack());
        }

        if (GetLoopBack() && IsAtsMapped() && m_NoGmmuMap)
        {
            SetIsSPAPeerAccess(true);
        }

        CHECK_RC(m_Surface2D.MapVirtToPhys(gpudev, virtAlloc->GetSurf2D(),
            physAlloc->GetSurf2D(), virtualOffset, physicalOffset, pLwRm,
            !m_NoGmmuMap));

        // Send SurfaceType Escape Write for Amodel
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            UINT64 physAddrBase = ~0ULL;
            if (physAlloc->GetMemHandle())
            {
                CHECK_RC(physAlloc->GetPhysAddress(0, &physAddrBase));
            }

            // m_Surface2D.GetCtxDmaOffsetGpu() has offset already added. So, subtracting offset
            // to get the base address.
            // Note:  Sparse is always set to "False" in MAP command even though the virtual
            // surface can be sparse.
            SendSurfaceTypeEscapeWriteBuf(
                m_Surface2D.GetCtxDmaOffsetGpu() - virtualOffset,
                virtualOffset,
                virtAlloc->GetSurf2D()->GetSize(),
                physAddrBase,
                physicalOffset,
                false,
                virtAlloc->GetName());
        }
    }

    const ArgReader* params = GetParams();
    bool usingCpuModelTest =
        ((params != nullptr) && (params->ParamPresent("-cpu_model_test") > 0));

    if (usingCpuModelTest && Platform::IsSelfHosted())
    {
        ErrPrintf("-cpu_model_test conflicts with self-hosted mode.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT64 virtAddr = virtAlloc->GetCtxDmaOffsetGpu() + virtualOffset;

    UINT32 loopbackPeerId = (GetLoopBackPeerId() == USE_DEFAULT_RM_MAPPING) ?
        physAlloc->GetLoopBackPeerId() : GetLoopBackPeerId();

    // Send the relevant mapping info to CPU model/Smmu driver,
    // if this surface should be ATS/inline mapped.
    if (m_SurfIommuVAType == GVA ||
        m_SurfIommuVAType == GPA)
    {
        CHECK_RC(CreateIommuMapping(virtAddr, physicalOffset,
            GetLoopBack(), loopbackPeerId));
    }

    // Send GMMU info if a CPU model test is specified.
    if (usingCpuModelTest && !m_NoGmmuMap)
    {
        CHECK_RC(SendGmmuMapping(virtAddr, physicalOffset,
            GetLoopBack(), loopbackPeerId, false));
    }

    return rc;
}

RC MdiagSurf::UpdateAtsPermission(UINT64 virtAddr, const string &type, bool value)
{
    RC rc;
    LwRm::Handle vaSpaceHandle = GetVASpaceHandle(GetVASpace());
    MASSERT(vaSpaceHandle != 0);

    LWGpuResource * lwgpu = GetLwgpu(GetGpuDev());
    MASSERT(lwgpu != nullptr);

    string addressSpaceName = lwgpu->GetAddressSpaceName(vaSpaceHandle, GetLwRmPtr());
    MASSERT(!addressSpaceName.empty());

    GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
    MASSERT(pSubdev != nullptr);
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

    UINT32 pasid = lwgpu->GetAddressSpacePasid(vaSpaceHandle, GetLwRmPtr());

    UINT64 physAddr;
    CHECK_RC(GetPhysAddress(0, &physAddr));

    IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
    MASSERT(iommu);

    CHECK_RC(iommu->UpdateAtsPermission(
        addressSpaceName,
        type,
        pGpuPcie,
        pasid,
        virtAddr,
        physAddr,
        value,
        m_AtsReadPermission,
        m_AtsWritePermission));

    return rc;
}

RC MdiagSurf::SendGmmuMapping(UINT64 virtAddr, UINT64 physicalOffset, bool usePeer, UINT32 peerId, bool doUnmap)
{
    RC rc;

    string addressSpaceName;
    LwRm::Handle vaSpaceHandle = GetVASpaceHandle(GetVASpace());
    LWGpuResource * lwgpu = GetLwgpu(GetGpuDev());
    MASSERT(lwgpu != nullptr);
    if (vaSpaceHandle == 0)
    {
        addressSpaceName = "default";
    }
    else
    {
        addressSpaceName = lwgpu->GetAddressSpaceName(vaSpaceHandle, GetLwRmPtr());

        if (addressSpaceName.empty())
        {
            ErrPrintf("Missing address space name for surface %s.\n", GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    string aperture;
    if (usePeer)
    {
        if (peerId == USE_DEFAULT_RM_MAPPING)
        {
            peerId = 0;
        }
        aperture = Utility::StrPrintf("Peer%d", peerId);
    }
    else if (GetLocation() == Memory::Fb)
    {
        aperture = "Local";
    }
    else if ((GetLocation() == Memory::Coherent) ||
        (GetLocation() == Memory::NonCoherent))
    {
        aperture = "Sysmem";
    }
    else
    {
        ErrPrintf("Unrecognized aperture for GMMU mapping of surface %s.\n",
            GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    UINT32 pasid = lwgpu->GetAddressSpacePasid(vaSpaceHandle, GetLwRmPtr());
    UINT32 pageSize = GetActualPageSizeKB() << 10;
    UINT64 offset = 0;

    while (offset < GetAllocSize())
    {
        UINT64 physAddr;

        CHECK_RC(GetPhysAddress(offset + physicalOffset, &physAddr));

        IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
        MASSERT(iommu);

        CHECK_RC(iommu->SendGmmuMapping(
            m_Surface2D.GetName(),
            addressSpaceName,
            aperture,
            GetTestId(),
            pasid,
            pageSize,
            virtAddr + offset,
            physAddr,
            doUnmap));

        offset += pageSize;
    }

    m_GmmuMappingSent = true;

    return rc;
}

void MdiagSurf::SetAtsPageSize(UINT32 atsPageSize)
{
    m_AtsPageSize = atsPageSize;
}

RC MdiagSurf::UpdateAtsMapping
(
    IommuDrv::MappingType updateType,
    UINT64 virtAddr,
    UINT64 physAddr,
    const string &aperture
)
{
    RC rc;

    LwRm::Handle vaSpaceHandle = GetVASpaceHandle(GetVASpace());
    MASSERT(vaSpaceHandle != 0);

    LWGpuResource * lwgpu = GetLwgpu(GetGpuDev());
    MASSERT(lwgpu != nullptr);

    string addressSpaceName = lwgpu->GetAddressSpaceName(vaSpaceHandle, GetLwRmPtr());
    MASSERT(!addressSpaceName.empty());

    GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
    MASSERT(pSubdev != nullptr);
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

    UINT32 pasid = lwgpu->GetAddressSpacePasid(vaSpaceHandle, GetLwRmPtr());

    IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
    MASSERT(iommu);

    CHECK_RC(iommu->UpdateAtsMapping(
        addressSpaceName,
        aperture,
        pGpuPcie,
        pasid,
        virtAddr,
        physAddr,
        updateType,
        m_AtsReadPermission,
        m_AtsWritePermission));

    return rc;
}

// Whether external memory is CPU mappable. Returns true if no external memory.
bool MdiagSurf::IsExternalMemoryCpuMappable() const
{
    // Due to a limitation with the RM API, external memory (via
    // -using_external_memory arg) in sysmem can't be CPU mapped. FLA memory
    // can't be CPU mapped if the destination GPU surface with the physical
    // address is not available. Other memory types using the external memory
    // mechanism can be potentially be CPU mapped.
    return m_ForceCpuMappable ||
        GetExternalPhysMem() == 0 ||
        (!HasFlaImportMem() && GetLocation() == Memory::Fb) ||
        (HasFlaImportMem() && GetDestinationFlaSurface() != nullptr);
}

RC MdiagSurf::MapPeer()
{
    RC rc;

    CHECK_RC(m_Surface2D.MapPeer());

    // if AMODEL
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        GpuDevice * pDevice = GetGpuDev();
        for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); subdev++)
        {
            const UINT64 peerVA = GetCtxDmaOffsetGpuPeer(subdev);
            const string aperture("Peer");
            UINT64 physAddr = ~0ULL;
            if (GetMemHandle())
            {
                CHECK_RC(GetPhysAddress(0, &physAddr));
            }
            SendSurfaceTypeEscapeWriteBuf(pDevice->GetSubdevice(subdev), peerVA, 0, GetSize(), physAddr, 0, aperture, false, "");
        }
    }

    return rc;
}

RC MdiagSurf::MapPeer(GpuDevice *pPeerDev)
{
    RC rc;

    CHECK_RC(m_Surface2D.MapPeer(pPeerDev));

    // Send SurfaceType Escape Write for Amodel
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        GpuDevice * pLocalDev = GetGpuDev();
        for (UINT32 localSD = 0; localSD < pLocalDev->GetNumSubdevices(); localSD++)
        {
            for (UINT32 peerSD = 0; peerSD < pPeerDev->GetNumSubdevices(); peerSD++)
            {
                const UINT64 peerVA = GetCtxDmaOffsetGpuPeer(localSD, pPeerDev, peerSD);
                const string aperture("Peer");
                UINT64 physAddr = ~0ULL;
                if (GetMemHandle())
                {
                    CHECK_RC(GetPhysAddress(0, &physAddr));
                }
                SendSurfaceTypeEscapeWriteBuf(pPeerDev->GetSubdevice(peerSD), peerVA, 0, GetSize(), physAddr, 0, aperture, false, "");
            }
        }
    }

    return rc;
}

RC MdiagSurf::MapLoopback()
{
    return MapLoopback(USE_DEFAULT_RM_MAPPING);
}

RC MdiagSurf::MapLoopback(UINT32 loopbackPeerId)
{
    RC rc;

    CHECK_RC(m_Surface2D.MapLoopback(loopbackPeerId));

    UINT64 virtAddr = (loopbackPeerId == USE_DEFAULT_RM_MAPPING) ?
        GetCtxDmaOffsetGpuPeer(0) :
        GetCtxDmaOffsetGpuPeer(0, loopbackPeerId);

    // Send SurfaceType Escape Write for Amodel
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        GpuDevice * pLocalDev = GetGpuDev();
        const string aperture("Peer");
        UINT64 physAddr = ~0ULL;
        if (GetMemHandle())
        {
            CHECK_RC(GetPhysAddress(0, &physAddr));
        }
        SendSurfaceTypeEscapeWriteBuf(pLocalDev->GetSubdevice(0), virtAddr, 0, GetSize(), physAddr, 0, aperture, false, "");
    }

    // Create an ATS peer mapping if required.
    if (m_SurfIommuVAType == GVA ||
        m_SurfIommuVAType == GPA)
    {
        CHECK_RC(CreateIommuMapping(virtAddr, 0, true, loopbackPeerId));
    }

    return rc;
}

LwRm::Handle MdiagSurf::GetExternalPhysMem() const
{
    return m_ParentSurface != nullptr ? 
        m_ParentSurface->GetExternalPhysMem() : m_Surface2D.GetExternalPhysMem();
}

RC MdiagSurf::GetPhysAddress
(
    UINT64 virtOffset,
    PHYSADDR* pPhysAddr,
    LwRm *pLwRm
) const
{
    if (m_ParentSurface != nullptr)
    {
        if (virtOffset > GetSize())
        {
            ErrPrintf("Surface offset exceeds surface size\n");
            return RC::SOFTWARE_ERROR;
        }

        return m_ParentSurface->GetPhysAddress(GetCpuMapOffset() + virtOffset, pPhysAddr, pLwRm);
    }

    // If this is a map-only surface, there will be a separate surface
    // to represent the physical allocation for the mapping.  The mapping
    // size might be different from the physical allocation size, so use
    // the physical allocation surface when calling Surface2D::GetPhysAddress
    // so that correct size is used.
    if (m_PhysAlloc != nullptr)
    {
        return m_PhysAlloc->m_Surface2D.GetPhysAddress(virtOffset, pPhysAddr,
            pLwRm);
    }
    else
    {
        return m_Surface2D.GetPhysAddress(virtOffset, pPhysAddr, pLwRm);
    }
}

RC MdiagSurf::GetDevicePhysAddress
(
    UINT64 virtOffset,
    Surface2D::VASpace vaSpace,
    PHYSADDR *pPhysAddr,
    LwRm *pLwRm
) const
{
    // If this is a map-only surface, there will be a separate surface
    // to represent the physical allocation for the mapping.  The mapping
    // size might be different from the physical allocation size, so use
    // the physical allocation surface when calling
    // Surface2D::GetDevicePhysAddress so that correct size is used.
    if (m_PhysAlloc != nullptr)
    {
        return m_PhysAlloc->m_Surface2D.GetDevicePhysAddress(virtOffset,
            vaSpace, pPhysAddr, pLwRm);
    }
    else
    {
        return m_Surface2D.GetDevicePhysAddress(virtOffset, vaSpace,
            pPhysAddr, pLwRm);
    }
}

RC MdiagSurf::SetPageSizeAttr(UINT32 *pAttr, UINT32 *pAttr2)
{
    RC rc;

    switch (GetPageSize())
    {
        case 4:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, *pAttr);
            break;
        // Choose 64K page size if one wasn't specified.
        case 0:
            SetPageSize(64);
        case 64:
        case 128:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _BIG, *pAttr);
            break;
        case 2048:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttr);
            *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _2MB,
                                  *pAttr2);
            break;
        case 524288:
            *pAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _HUGE, *pAttr);
            *pAttr2 = FLD_SET_DRF(OS32, _ATTR2, _PAGE_SIZE_HUGE, _512MB,
                                  *pAttr2);
            break;
        default:
            ErrPrintf("Invalid page size in %s: %u\n",
                __FUNCTION__, GetPageSize());
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC MdiagSurf::AllocateExternalTraceMemory(GpuDevice *gpuDevice, LwRm* pLwRm)
{
    RC rc;

    UINT32 attrib = DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
    UINT32 attrib2 = LWOS32_ATTR2_NONE;

    m_Surface2D.SetLocationAttr(GetLocation(), &attrib);
    CHECK_RC(SetPageSizeAttr(&attrib, &attrib2));

    if (pLwRm == nullptr)
    {
        pLwRm = GetLwRmPtr();
    }
    UINT64 physAddr = GetFixedPhysAddr();
    rc = pLwRm->VidHeapOsDescriptor(0, 0, attrib, attrib2, &physAddr,
        m_Surface2D.EstimateSize(gpuDevice), gpuDevice, &m_ExternalTraceMem);

    if (rc == OK)
    {
        m_Surface2D.SetExternalPhysMem(m_ExternalTraceMem, physAddr);
    }
    else
    {
        ErrPrintf("Couldn't allocate external memory.\n");
    }

    return rc;
}

RC MdiagSurf::AllocateFlaImportMemory(GpuDevice* gpudev, LwRm* pLwRm)
{
    RC rc;
    UINT32 attr = LWOS32_ATTR_NONE;
    UINT32 attr2 = LWOS32_ATTR2_NONE;

    DebugPrintf("Allocating FLA import memory for surface %s\n",
        GetName().c_str());

    CHECK_RC(SetPageSizeAttr(&attr, &attr2));

    LW_FLA_MEMORY_ALLOCATION_PARAMS params = {0};
    params.type = LWOS32_TYPE_IMAGE;
    params.attr = attr;
    params.attr2 = attr2;

    if (pLwRm == nullptr)
    {
        pLwRm = GetLwRmPtr();
    }
    params.hExportSubdevice =
        pLwRm->GetSubdeviceHandle(m_FlaImportMem->pDestinationSurface->GetGpuDev()->GetSubdevice(0));

    params.hExportHandle =
        m_FlaImportMem->pDestinationSurface->GetVirtMemHandle();
    params.hExportClient =
        m_FlaImportMem->pDestinationSurface->GetLwRmPtr()->GetClientHandle();
    auto *pParams = &params;
    auto memLimit = m_FlaImportMem->size - 1;

    CHECK_RC(pLwRm->AllocMemory(
            &m_FlaImportMem->hMemory,
            LW01_MEMORY_FLA,
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _CONTIGUOUS),
            reinterpret_cast<void**>(&pParams),
            &memLimit,
            gpudev));

    m_Surface2D.SetExternalPhysMem(m_FlaImportMem->hMemory,
        m_FlaImportMem->address);

    return rc;
}

// Import FLA memory in place of local physical memory. Destination surface can
// be nullptr if destination surface is unavailable.
RC MdiagSurf::SetFlaImportMem(UINT64 address, UINT64 size,
    MdiagSurf* pDestinationSurface)
{
    RC rc;

    if (m_FlaImportMem == nullptr)
    {
        m_FlaImportMem = make_shared<FlaMemory>();
    }
    *m_FlaImportMem = {0};
    m_FlaImportMem->address = address;
    m_FlaImportMem->size = size;
    m_FlaImportMem->pDestinationSurface = pDestinationSurface;

    DebugPrintf("%s: surface=%s address=0x%llx size=0x%llx\n", __FUNCTION__,
        GetName().c_str(), address, size);

    return rc;
}

// Whether this surface has imported FLA memory in place of local physical
// memory.
bool MdiagSurf::HasFlaImportMem() const
{
    // If this is a map-only surface, there will be a separate surface
    // to represent the physical allocation for the mapping.
    if (m_PhysAlloc != nullptr)
    {
        return m_PhysAlloc->HasFlaImportMem();
    }

    return m_FlaImportMem != nullptr;
}

// Return FLA surface in destination GPU. Can be nullptr if we are only tracking
// the FLA address and size.
MdiagSurf* MdiagSurf::GetDestinationFlaSurface() const
{
    // If this is a map-only surface, there will be a separate surface
    // to represent the physical allocation for the mapping.
    if (m_PhysAlloc != nullptr)
    {
        return m_PhysAlloc->GetDestinationFlaSurface();
    }

    if (m_FlaImportMem != nullptr)
    {
        return m_FlaImportMem->pDestinationSurface;
    }

    return nullptr;
}

// Return a surface for CPU mapping. For FLA mapped surfaces, returns the
// destination surface with the actual physical memory. Otherwise returns this
// surface. Can be nullptr if the destination FLA surface is not available.
const MdiagSurf* MdiagSurf::GetCpuMappableSurface() const
{
    if (HasFlaImportMem())
    {
        return GetDestinationFlaSurface();
    }

    return this;
}

MdiagSurf* MdiagSurf::GetCpuMappableSurface()
{
    return const_cast<MdiagSurf*>(
        static_cast<const MdiagSurf*>(this)->GetCpuMappableSurface());
}

void MdiagSurf::SendFreeAllocRangeEscapeWrite()
{
    UINT32 supportsFreeAllocRange = 0;
    if ((0 == GetGpuDev()->GetSubdevice(0)->EscapeReadBuffer("supported/FreeAllocRange", 0, sizeof(LwU32), &supportsFreeAllocRange)) &&
        (supportsFreeAllocRange != 0))
    {
        // FreeAllocRange escape write only needs to be sent for surfaces created using
        // ALLOC_VIRTUAL and ALLOC_PHYSICAL.
        if (m_Surface2D.IsVirtualOnly() || m_Surface2D.IsPhysicalOnly())
        {
            const string freeType = m_Surface2D.IsVirtualOnly() ? "virtual" : "physical";
            const UINT32 vaSpaceId = m_Surface2D.GetVASpaceId();
            string surfName = m_Surface2D.GetName();
            const size_t bufferSize =
                sizeof(UINT64) +        // VA base
                sizeof(UINT32) +        // VA space Id
                freeType.size() + 1 +   // Free type (virtual/physical)
                surfName.size() + 1;    // Surface name

            vector<UINT08> escwriteBuffer(bufferSize);
            UINT08* buf = &escwriteBuffer[0];

            // virtual offset only used at the MAP and MAP corresponding free is handled by RM
            // So we don't care about it in this branch
            *(UINT64*)buf = m_Surface2D.GetCtxDmaOffsetGpu();
            buf += sizeof(UINT64);

            *(UINT32*)buf = vaSpaceId;
            buf += sizeof(UINT32);

            memcpy(buf, freeType.c_str(), freeType.size());
            buf += freeType.size();
            *buf = '\0';
            ++buf;

            memcpy(buf, surfName.c_str(), surfName.size());
            buf += surfName.size();
            *buf = '\0';
            ++buf;

            GetGpuDev()->GetSubdevice(0)->EscapeWriteBuffer("FreeAllocRange", 0, bufferSize, &escwriteBuffer[0]);
        }
    }
} 

void MdiagSurf::Free()
{
    if (GetDmaBufferAlloc())
    {
        DebugPrintf("%s: This surface %s  will be free at other stage.\n",
                __FUNCTION__, GetName().c_str());
        return;
    }

    if (m_GmmuMappingSent)
    {
        UINT64 virtAddr = m_Surface2D.GetCtxDmaOffsetGpu();

        RC rc = SendGmmuMapping(virtAddr, 0, GetLoopBack(), GetLoopBackPeerId(), true);
        if (rc != OK)
            ErrPrintf("%s: SendGmmuMapping failed rc=%d\n", __FUNCTION__, rc);

        m_GmmuMappingSent = false;
    }

    LwRm* pLwRm = GetLwRmPtr();

    // Need to free the MmuLevel information before LwRmPtr has been cleaned at the surf2d
    if (MmuLevelTreeManager::HasInstance())
    {
        MmuLevelTreeManager::Instance()->FreeResource(this);
    }

    if (m_ExternalTraceMem != 0)
    {
        GpuDevice* gpuDev = GetGpuDev();
        m_Surface2D.Free();
        pLwRm->FreeOsDescriptor(gpuDev, m_ExternalTraceMem);
        m_ExternalTraceMem = 0;
    }
    else
    {
        m_Surface2D.Free();
    }

    if (m_FlaImportMem != nullptr && m_FlaImportMem->hMemory != 0)
    {
        pLwRm->Free(m_FlaImportMem->hMemory);
        m_FlaImportMem->hMemory = 0;
    }

    m_ParentSurface = nullptr;
    m_AccessingGpuDevice = nullptr;
    m_HostingGpuDevice = nullptr;
}

void MdiagSurf::SetFixedPhysAddr(UINT64 address)
{
    MASSERT(address != _UINT64_MAX);
    m_PhysAddrConstraint = FixedAddress;
    m_FixedPhysAddr = address;
    m_FixedPhysAddrMin = _UINT64_MAX;
    m_FixedPhysAddrMax = _UINT64_MAX;
    m_Surface2D.SetGpuPhysAddrHint(address);
    m_Surface2D.SetGpuPhysAddrHintMax(0);
}

UINT64 MdiagSurf::GetFixedPhysAddr() const
{
    MASSERT(m_PhysAddrConstraint == FixedAddress);
    return m_FixedPhysAddr;
}

bool MdiagSurf::HasFixedPhysAddr() const
{
    return (m_PhysAddrConstraint == FixedAddress);
}

void MdiagSurf::SetPhysAddrRange(UINT64 minAddress, UINT64 maxAddress)
{
    MASSERT(minAddress != _UINT64_MAX);
    MASSERT(maxAddress != _UINT64_MAX);

    if (m_PhysAddrConstraint != FixedAddress)
    {
        m_PhysAddrConstraint = AddressRange;
        m_FixedPhysAddrMin = minAddress;
        m_FixedPhysAddrMax = maxAddress;
        m_Surface2D.SetGpuPhysAddrHint(minAddress);
        m_Surface2D.SetGpuPhysAddrHintMax(maxAddress);
    }
}

UINT64 MdiagSurf::GetPhysAddrRangeMin() const
{
    MASSERT(m_PhysAddrConstraint == AddressRange);
    return m_FixedPhysAddrMin;
}

UINT64 MdiagSurf::GetPhysAddrRangeMax() const
{
    MASSERT(m_PhysAddrConstraint == AddressRange);
    return m_FixedPhysAddrMax;
}

bool MdiagSurf::HasPhysAddrRange() const
{
    return (m_PhysAddrConstraint == AddressRange);
}

void MdiagSurf::SetFixedVirtAddr(UINT64 address)
{
    MASSERT(address != _UINT64_MAX);
    m_VirtAddrConstraint = FixedAddress;
    m_FixedVirtAddr = address;
    m_FixedVirtAddrMin = _UINT64_MAX;
    m_FixedVirtAddrMax = _UINT64_MAX;
    m_Surface2D.SetGpuVirtAddrHint(address);
    m_Surface2D.SetGpuVirtAddrHintMax(0);
}

UINT64 MdiagSurf::GetFixedVirtAddr() const
{
    MASSERT(m_VirtAddrConstraint == FixedAddress);
    return m_FixedVirtAddr;
}

bool MdiagSurf::HasFixedVirtAddr() const
{
    return (m_VirtAddrConstraint == FixedAddress);
}

void MdiagSurf::SetVirtAddrRange(UINT64 minAddress, UINT64 maxAddress)
{
    MASSERT(minAddress != _UINT64_MAX);
    MASSERT(maxAddress != _UINT64_MAX);

    if (m_VirtAddrConstraint != FixedAddress)
    {
        m_VirtAddrConstraint = AddressRange;
        m_FixedVirtAddrMin = minAddress;
        m_FixedVirtAddrMax = maxAddress;
        m_Surface2D.SetGpuVirtAddrHint(minAddress);
        m_Surface2D.SetGpuVirtAddrHintMax(maxAddress);
    }
}

UINT64 MdiagSurf::GetVirtAddrRangeMin() const
{
    MASSERT(m_VirtAddrConstraint == AddressRange);
    return m_FixedVirtAddrMin;
}

UINT64 MdiagSurf::GetVirtAddrRangeMax() const
{
    MASSERT(m_VirtAddrConstraint == AddressRange);
    return m_FixedVirtAddrMax;
}

bool MdiagSurf::HasVirtAddrRange() const
{
    return (m_VirtAddrConstraint == AddressRange);
}

// use to check whether mods can read/write this surface with direct mapping.
bool MdiagSurf::IsDirectMappable(OpType type) const
{
    if (!IsExternalMemoryCpuMappable())
    {
        DebugPrintf("%s: Direct mapping can't be used due to external memory can't be CPU mapped.\n", __FUNCTION__);
        return false;
    }

    bool bDirectMapping = false;
    GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
    INT32 pteKind = pSubdev->GetKindTraits(GetPteKind());
    const bool bCompressible = pteKind & GpuSubdevice::Compressible;

    if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY) &&
        (pteKind & GpuSubdevice::Generic_Memory))
    {
        if (GetLayout() == Surface2D::BlockLinear || GetLayout() == Surface2D::Pitch)
        {
            if (type == WrSurf)
            {
                bDirectMapping = true;
            }
            else // RdSurf
            {
                if (!bCompressible)
                    bDirectMapping = true;
            }
        }
    }
    else
    {
        //Use direct memory mapping for below kinds of surface.
        //1. all pitch surfaces
        //2. xbar_raw bl color buffer
        //3. fb_raw bl z buffer (Z24S8 only)
        //4. Init compressed surface (generic 16BX2 and Z24S8)
        if (bCompressible)
        {
            //According to spec, memory layout of compressed surface is blocklinear.
            //PTEKIND_COMPRESSIBLE && KIND_C: xbar raw block linear (Generic 16BX2)
            //PTEKIND_COMPRESSIBLE && KIND_Z24S8: fb raw block linear with subpkt swizzle
            if (type == WrSurf &&
                ((pteKind & GpuSubdevice::Color) ||
                 (pteKind & GpuSubdevice::Z24S8)))
            {
                bDirectMapping = true;
            }
        }
        else
        {
            if ((pteKind & GpuSubdevice::Pitch) ||
                (pteKind & GpuSubdevice::Generic_16BX2) ||
                (pteKind & GpuSubdevice::Z24S8))
            {
                bDirectMapping = true;
            }
        }
    }

    DebugPrintf("%s: Direct mapping can %s be used to %s%s surface %s with pteKind %s!\n",
                __FUNCTION__, bDirectMapping? "" : " not",
                (type == WrSurf) ? "write" : "read",
                bCompressible ? " compressed" : "",
                 GetName().c_str(), pSubdev->GetPteKindName(GetPteKind()));

    return bDirectMapping;
}

// This function determines if the copy engine is needed in order to
// read from or write to this surface.
//
bool MdiagSurf::UseCEForMemAccess(const ArgReader *params, OpType memOp) const
{
    if (params == nullptr)
    {
        params = GetParams();
        MASSERT(params != nullptr);
    }

    // If data is about to be written to the surface, check for a command-line
    // argument that forces this surface to be initialized via copy engine.
    if ((memOp == WrSurf) && (GetLocation() != Memory::Fb))
    {
        if (params->ParamPresent("-surface_init") > 0)
        {
            UINT32 argValue = params->ParamUnsigned("-surface_init");
            if ((argValue == 0) ||
                ((argValue == 1) && (GetLayout() == Surface::BlockLinear)))
            {
                DebugPrintf("Using copy engine to write surface %s because of command-line argument.\n", 
                    GetName().c_str());
                return true;
            }
        }
    }

    // If data is about to be read from the surface, check for a command-line
    // argument that forces this surface to be read via copy engine.
    if ((memOp == RdSurf) && params->ParamPresent("-dma_check"))
    {
        DebugPrintf("Using copy engine to read surface %s because of command-line argument.\n",
            GetName().c_str());
        return true;
    }

    if (Platform::GetSimulationMode() == Platform::RTL
        && !params->ParamPresent("-disable_compressed_vidmem_surfaces_dma")
        && GetGpuDev()->GetFamily() >= GpuDevice::Blackwell)
    {
        // If the operation is "Read", surface is compressed and in video memory, we can DMA the data
        // to system memory and read it from there for better performance in RTL targets.
        if ((memOp == RdSurf) && (GetLocation() == Memory::Fb) && GetCompressed())
        {
            DebugPrintf("Using copy engine to read compressed vidmem surface %s on RTL.\n", GetName().c_str());
            return true;
        }
    }

    if (GetSplit())
    {
        DebugPrintf("Using copy engine to access split surface %s.\n",GetName().c_str());
        return true;
    }

    // Due to a limitation with the RM API, external memory in sysmem
    // can't be CPU mapped, so the copy engine will always be required
    // in order to read from or write to this surface.
    if (!IsExternalMemoryCpuMappable())
    {
        DebugPrintf("Using copy engine to access surface %s since external memory can't be CPU mapped.\n", 
            GetName().c_str());
        return true;
    }

    GpuSubdevice *gpuSubdevice = GetGpuDev()->GetSubdevice(0);

    // If the device doesn't support reflected mapping and a direct mapping can't be used
    // (e.g. the surface format can't be swizzled by MODS), use the
    // copy engine so that hardware will do the swizzling.
    const bool isDirectMappable = IsDirectMappable(memOp);
    if (!gpuSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING) &&
        (GetLocation() != Memory::Fb) && !isDirectMappable)
    {
        DebugPrintf("Using copy engine to access sysmem surface %s "
            "since reflected mapping is not supported and direct mapping can't be used.\n", 
            GetName().c_str());
        return true;
    }

    //bug200384391. For those surface views whose parent surface are virtual
    //surface, test will assert because physical memory handler = 0 during crc
    //check. This issue can be solved if surface view do crc check through ce.
    if (IsSurfaceView())
    {
        if (GetParentSurface() &&
            GetParentSurface()->IsVirtualOnly())
        {
            DebugPrintf("Using copy engine to access surface view %s "
                "whose parent surface is virtual surface.\n", 
                GetName().c_str());
            return true;
        }
    }

    return false;
}

// This function determines if memory access to a surface requires a colwersion
// of data from one blocklinear type to another (e.g. naive to XBAR raw).
// The function caller is responsible for doing the appropriate colwersion.
//
// TODO: The colwersion could be pulled into this function.
//
bool MdiagSurf::MemAccessNeedsColwersion(const ArgReader *params, OpType memOp) const
{
    if (params == nullptr)
    {
        params = GetParams();
        MASSERT(params != nullptr);
    }

    if (params->ParamPresent("-disable_swizzle_between_naive_raw"))
    {
        DebugPrintf("%s: return false due to argument -disable_swizzle_between_naive_raw.\n", __FUNCTION__);
        return false;
    }

    GpuSubdevice *gpuSubdevice = GetGpuDev()->GetSubdevice(0);
    UINT32 pteKindTraits = gpuSubdevice->GetKindTraits(GetPteKind());

    DebugPrintf("%s: pteKindTraits: %x.\n", __FUNCTION__, pteKindTraits);

    if (pteKindTraits & GpuSubdevice::Pitch)
    {
        return false;
    }
    else if (pteKindTraits & GpuSubdevice::Generic_Memory)
    {
        // Blocklinear generic memory surfaces need to be colwerted between
        // naive and raw formats, even if the copy engine is used.
        if (GetLayout() == Surface2D::BlockLinear)
        {
            DebugPrintf("%s: surface access needs blocklinear colwersion due to generic memory.\n", __FUNCTION__);
            return true;
        }
        else if (GetLayout() == Surface2D::Pitch)
        {
            return false;
        }
    }

    if ((pteKindTraits & GpuSubdevice::Z) && gpuSubdevice->HasFeature(Device::GPUSUB_DEPRECATED_RAW_SWIZZLE))
    {
        DebugPrintf("%s: surface access needs blocklinear colwersion due to FBHUB swizzle removing.\n", __FUNCTION__);
        return true;
    }

    // If the device doesn't support reflected sysmem mapping and doesn't require the copy engine to do
    // the colwersion, MODS will do the colwersion.
    if (!gpuSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING) &&
       (GetLocation() != Memory::Fb) && !MdiagSurf::UseCEForMemAccess(params, memOp))
    {
        DebugPrintf("%s: surface access needs blocklinear colwersion due to lack of reflected mapping supporting.\n", __FUNCTION__);
        return true;
    }

    return false;
}

bool MdiagSurf::FormatHasXBits() const
{
    const ColorUtils::Format surfaceFormat = GetColorFormat();

    return (surfaceFormat == ColorUtils::X8Z24) ||
            (surfaceFormat == ColorUtils::ZF32_X16V8X8) ||
            (surfaceFormat == ColorUtils::ZF32_X24S8) ||
            (surfaceFormat == ColorUtils::X8Z24_X16V8S8) ||
            (surfaceFormat == ColorUtils::ZF32_X16V8S8);
}

UINT64 MdiagSurf::GetPixelOffset
(
    UINT32 x,
    UINT32 y
)
{
    return GetPixelOffset( x, y, 0, 0);
}

UINT64 MdiagSurf::GetPixelOffset
(
    UINT32 x,
    UINT32 y,
    UINT32 z,
    UINT32 a
)
{
    UINT64 offsetInSurf = m_Surface2D.GetPixelOffset( x, y, z, a );
    GpuSubdevice *pSubdev = GetGpuDev()->GetSubdevice(0);
    INT32 pteKind = pSubdev->GetKindTraits(GetPteKind());

    if (((pteKind & GpuSubdevice::Z24S8) ||
         (pteKind & GpuSubdevice::Generic_16BX2)) &&
         ((GetLocation() != Memory::Fb) && !(pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING))))
    {
       //Callwlate offset in Gob for NBL format
        const UINT32 offsetInGobNBL = ( x * GetBytesPerPixel() ) % (GetGpuDev()->GobWidth()) +
                                      ( y % (GetGpuDev()->GobHeight()) ) * (GetGpuDev()->GobWidth());
        const PixelFormatTransform::BlockLinearLayout blLayout = (pteKind & GpuSubdevice::Z24S8) ?
                                                                 PixelFormatTransform::FB_RAW :
                                                                 PixelFormatTransform::XBAR_RAW;
        //Fix offset in Gob from NBL format to XBAR_RAW format or FB_RAW format
        return offsetInSurf - offsetInGobNBL +
               PixelFormatTransform::GetOffsetInRawGob( offsetInGobNBL, blLayout, GetGpuDev());
    }
    else
    {
        return offsetInSurf;
    }
}

MdiagSegList::MdiagSegList(string Type) :
    m_Type(Type)
{
    m_pLwRm = nullptr;
    m_pGpuDev = nullptr;
}

MdiagSegList::~MdiagSegList()
{
    for (auto& s : m_RawMemoryList)
        s->Unmap();
}

RC MdiagSurf::CreateSegmentList(string mode, UINT64 offset, MdiagSegList** ppSegList)
{
    RC rc;
    LwRm* pLwRm = GetLwRmPtr();
    MdiagSegList* pSeg = new MdiagSegList(mode);
    LwRm::Handle hSubDevice = pLwRm->GetSubdeviceHandle(GetGpuDev()->GetSubdevice(0));

    *ppSegList = nullptr;

    MASSERT(GetGpuDev()->GetNumSubdevices() == 1);

    pSeg->SetLwRmPtr(pLwRm);
    pSeg->SetGpuDev(GetGpuDev());
    unique_ptr<MDiagUtils::RawMemory> rawMemory = make_unique<MDiagUtils::RawMemory>(pLwRm, GetGpuDev());

    if (mode == "CBC")
    {
        LW2080_CTRL_CMD_FB_GET_CBC_BASE_ADDR_PARAMS getCbcParams = {0};

        CHECK_RC(pLwRm->ControlBySubdevice(
                    GetGpuDev()->GetSubdevice(0), LW2080_CTRL_CMD_FB_GET_CBC_BASE_ADDR,
                    &getCbcParams, sizeof(getCbcParams)));

        rawMemory->SetPhysAddress(getCbcParams.cbcBaseAddress);
    }
    else if (mode == "IB" || mode == "PDB")
    {
        if (GetChannel() == nullptr)
        {
            ErrPrintf("%s: Cannot get instance block handle: No channel associated with surface=%s\n",
                __FUNCTION__,
                m_Surface2D.GetName().c_str());
            MASSERT(0);
        }
        LW2080_CTRL_FIFO_MEM_INFO channelInfo = MDiagUtils::GetInstProperties(
            GetGpuDev()->GetSubdevice(0),
            GetChannel()->ChannelHandle(),
            pLwRm
        );
        if (mode == "PDB")
        {
            // MODS will need per-chip behavior here if this value changes in the future.
            rawMemory->SetPhysAddress(channelInfo.base + (DRF_BASE(LW_RAMIN_PAGE_DIR_BASE_TARGET) >> 3));
        }
        else
        {
            rawMemory->SetPhysAddress(channelInfo.base + offset);
        }
        InfoPrintf("Instance Block: Creating segment list at addr=0x%llx\n", rawMemory->GetPhysAddress());
    }
    else if (mode == "METHOD_BUFFER")
    {
        LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO_PARAMS params = {0};

        // fail if method buffer is not in vidmem, or if surface has no channel
        UINT32 vidMethodBuffer = 0;
        rc = Registry::Read("ResourceManager", LW_REG_STR_RM_INST_LOC_3, &vidMethodBuffer);
        if (OK != rc ||
            !FLD_TEST_DRF(_REG_STR_RM, _INST_LOC_3, _FAULT_METHOD_BUFFER, _VID, vidMethodBuffer))
        {
            ErrPrintf("%s: Please use -vid_method_buffer for METHOD_BUFFER mode.\n", __FUNCTION__);
            MASSERT(0);
        }
        if (GetChannel() == nullptr)
        {
            ErrPrintf("%s: Cannot get method buffer handle: No channel associated with surface=%s\n",
                __FUNCTION__,
                m_Surface2D.GetName().c_str());
            MASSERT(0);
        }
        rc.Clear();

        params.hChannel = GetChannel()->ChannelHandle();
        CHECK_RC(pLwRm->ControlBySubdevice(
                     GetGpuDev()->GetSubdevice(0), LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO,
                     &params, sizeof(params)));
        // default to runqueue 0
        rawMemory->SetPhysAddress(params.chMemInfo.methodBuf[0].base + offset);

        InfoPrintf("Method buffer: Creating segment list at addr=0x%llx\n", rawMemory->GetPhysAddress());
    }
    else if (mode == "GR_MAIN_CTX_BUFFER")
    {
        LW2080_CTRL_GR_GET_CTX_BUFFER_INFO_PARAMS params = {0};
        LW2080_CTRL_GR_CTX_BUFFER_INFO *pCtxInfo = NULL;
        UINT32 i;

        // fail if the main context buffer is not in vidmem, or if surface has no channel
        UINT32 vidMainCtxBuffer = 0;
        rc = Registry::Read("ResourceManager", LW_REG_STR_RM_INST_LOC, &vidMainCtxBuffer);
        if (OK != rc ||
            !FLD_TEST_DRF(_REG_STR_RM, _INST_LOC, _GRCTX, _VID, vidMainCtxBuffer))
        {
            ErrPrintf("%s: Please use -vid_grctx for GRCTX mode.\n", __FUNCTION__);
            MASSERT(0);
        }
        if (GetChannel() == nullptr)
        {
            ErrPrintf("%s: Cannot get context buffer handle: No channel associated with surface=%s\n",
                __FUNCTION__,
                m_Surface2D.GetName().c_str());
            MASSERT(0);
        }
        rc.Clear();

        params.hChannel = GetChannel()->ChannelHandle();
        params.hUserClient = pLwRm->GetClientHandle();
        params.bufferCount = LW2080_CTRL_GR_MAX_CTX_BUFFER_COUNT;
        CHECK_RC(pLwRm->ControlBySubdevice(
                     GetGpuDev()->GetSubdevice(0), LW2080_CTRL_CMD_GR_GET_CTX_BUFFER_INFO,
                     &params, sizeof(params)));

        // Find the main buffer related information
        for (i = 0; i < LW2080_CTRL_GR_MAX_CTX_BUFFER_COUNT; i++)
        {
            pCtxInfo = &params.ctxBufferInfo[i];

            // Ignore the entry if size is 0
            if (pCtxInfo->size == 0)
                continue;

            // If it's the main context buffer's info, we're done
            if (pCtxInfo->bufferType == LW2080_CTRL_GPU_PROMOTE_CTX_BUFFER_ID_MAIN)
                break;
        }
        MASSERT(i < LW2080_CTRL_GR_MAX_CTX_BUFFER_COUNT && pCtxInfo != NULL);

        // Offset has to be within limit and allocation of the context buffer and all
        // subcontext buffers should be contiguous. Also, make sure that the aperture is
        // really vidmem.
        if (!pCtxInfo->bIsContigous && offset > 0x1000) // Offset beyond the first 4K page
        {
            ErrPrintf("%s: offset is beyond the contiguous context buffer.\n", __FUNCTION__);
            MASSERT(0);
        }
        if (pCtxInfo->bIsContigous && offset > pCtxInfo->size)
        {
            ErrPrintf("%s: offset is beyond the context buffer size.\n", __FUNCTION__);
            MASSERT(0);
        }
        // 2 is vidmem 
        if (pCtxInfo->aperture != 2)
        {
            ErrPrintf("%s: The context buffer's aperture is not vidmem.\n", __FUNCTION__);
            MASSERT(0);
        }
        rawMemory->SetPhysAddress(pCtxInfo->physAddr + offset);
        InfoPrintf("Context buffer: Creating segment list at addr=0x%llx\n", rawMemory->GetPhysAddress());
    }
    else if (mode == "FB")
    {
        rawMemory->SetPhysAddress(offset);
        InfoPrintf("FB: Creating segment list at addr=0x%llx\n", rawMemory->GetPhysAddress());
    }
    else if ((mode == "PTE")  || (mode == "PDE0") || (mode == "PDE1") ||
             (mode == "PDE2") || (mode == "PDE3") || (mode == "PDE4"))
    {
        LwRm::Handle hVASpace = GetGpuVASpace();

        // Use default VA space if it is not specified
        if (!hVASpace)
        {
            LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
            vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE;

            CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetGpuDev()),
                &hVASpace,
                FERMI_VASPACE_A,
                &vaParams));
        }

        // Get GmmuFmt for given hVASpace and hSubDevice
        LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS gmmuFmtParams = {0};
        gmmuFmtParams.hSubDevice = hSubDevice;
        CHECK_RC(pLwRm->Control(hVASpace,
                LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
                &gmmuFmtParams,
                sizeof(gmmuFmtParams)));

        UINT32 pageSize = GetActualPageSizeKB() << 10;

        LwU64 GpuPageVA = GetCtxDmaOffsetGpu() + offset;
        LW90F1_CTRL_VASPACE_GET_PAGE_LEVEL_INFO_PARAMS mmuLevelInfo = {0};
        mmuLevelInfo.hSubDevice = 0; // Use subdevice id (not handle)
        mmuLevelInfo.subDeviceId = 0;
        mmuLevelInfo.virtAddress = GpuPageVA;
        mmuLevelInfo.pageSize = pageSize;

        CHECK_RC(pLwRm->Control(
                hVASpace,
                LW90F1_CTRL_CMD_VASPACE_GET_PAGE_LEVEL_INFO,
                &mmuLevelInfo,
                sizeof(mmuLevelInfo)));

        const GMMU_FMT *pFmt = (const GMMU_FMT *) LwP64_VALUE(gmmuFmtParams.pFmt);
        int version = pFmt->version;
        int level = 0;
        int entry = 0;

        // Version1: 2-level (40b VA) format supported Fermi through Maxwell
        // Version2: 5-level (49b VA) format supported on Pascal+.
        // Version3: 6-level (57b VA) format supported on Hopper+, but only 5-level in total
        // if 57-bit virtual address space is used.
        DebugPrintf("Gmmu format in verison %d, has %d levels.\n", version, mmuLevelInfo.numLevels);

        if (mode == "PTE")
            level = mmuLevelInfo.numLevels - 1;
        else if (mode.substr(0,3) ==  "PDE")
        {
            UINT32 index = static_cast<INT32>(Utility::Strtoull(mode.substr(3).c_str(), 0, 0));
            UINT32 maxPdeLevelNum = mmuLevelInfo.numLevels - 2;
            if (maxPdeLevelNum >= index)
            {
                level = maxPdeLevelNum - index;
            }
            else
            {
                ErrPrintf("Create segment list: Surface=%s page table only supports PDE[0-%d] or PTE requested %s\n",
                    m_Surface2D.GetName().c_str(),
                    maxPdeLevelNum,
                    mode.c_str());
                MASSERT(0);
            }
        }

        rawMemory->SetSize(mmuLevelInfo.levels[level].levelFmt.entrySize);

        UINT64 mask = (1 << (mmuLevelInfo.levels[level].levelFmt.virtAddrBitHi -
                                 mmuLevelInfo.levels[level].levelFmt.virtAddrBitLo + 1)) - 1;
        entry = (GpuPageVA >> mmuLevelInfo.levels[level].levelFmt.virtAddrBitLo) & mask;

        rawMemory->SetPhysAddress(mmuLevelInfo.levels[level].physAddress+
                                entry*mmuLevelInfo.levels[level].levelFmt.entrySize);

        // colwert GMMU_APERTURE to Memory::Location

        switch (mmuLevelInfo.levels[level].aperture)
        {
            case GMMU_APERTURE_VIDEO:
                rawMemory->SetLocation(Memory::Fb);
                break;

            case GMMU_APERTURE_SYS_NONCOH:
                rawMemory->SetLocation(Memory::NonCoherent);
                break;

            case GMMU_APERTURE_SYS_COH:
                rawMemory->SetLocation(Memory::Coherent);
                break;

            default:
                ErrPrintf("%s: Surface=%s unknown aperture %d\n",
                    __FUNCTION__,
                    m_Surface2D.GetName().c_str(),
                    mmuLevelInfo.levels[level].aperture);
                MASSERT(0);
        }
        rawMemory->Map();
    }
    else
    {
        ErrPrintf("%s: Surface=%s unknown segment: %s\n",
                    __FUNCTION__,
                    m_Surface2D.GetName().c_str(),
                    mode.c_str());
        MASSERT(0);
    }
    pSeg->m_RawMemoryList.insert(pSeg->m_RawMemoryList.end(), move(rawMemory));
    *ppSegList = pSeg;

    return OK;
}

bool MdiagSurf::IsOnSysmem() const
{
    if ((GetLocation() == Memory::Coherent) ||
             (GetLocation() == Memory::NonCoherent) ||
             (GetLocation() == Memory::CachedNonCoherent) ||
             (GetLocation() == Memory::Optimal && Platform::IsTegra()))
    {
        return true;
    }

    return false;
}
