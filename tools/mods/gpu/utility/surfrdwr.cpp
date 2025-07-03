/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surfrdwr.cpp
 * @brief Implementation of utilities for reading and writing surfaces.
 */

#include "surfrdwr.h"
#include "surf2d.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/utility.h"

#if defined(TEGRA_MODS)
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#endif

// The max mapping size for each time, according to RM team, mapping
// with small chunk can reduce mapping failures and the performance
// drop is acceptable. Setting the max size to 4M initially
#define MAX_MAPPING_CHUNK_SIZE 0x400000

SurfaceUtils::SurfaceMapper::SurfaceMapper(const Surface& Surf)
: m_Surf(Surf), m_Subdev(0), m_MaxMappedChunkSize(MAX_MAPPING_CHUNK_SIZE),
  m_MappedHandle(0), m_MappedBegin(0), m_MappedEnd(0),
  m_MappedAddress(0), m_RawImage(false), m_HWAllocHandle(0)
{
}

SurfaceUtils::SurfaceMapper::~SurfaceMapper()
{
    Unmap();
}

void SurfaceUtils::SurfaceMapper::SetMaxMappedChunkSize(UINT32 Size)
{
    const unsigned Granularity = 128*1024;
    if ((Size > 0) && (((Size % Granularity) != 0) || (Size < Granularity)))
    {
        Printf(Tee::PriHigh, "Maximum mapped chunk size must be a multiple of %u and must not be lower than this.\n",
            Granularity);
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return;
    }
    if (Size != m_MaxMappedChunkSize)
    {
        Unmap();
        m_MaxMappedChunkSize = Size;
    }
}

void SurfaceUtils::SurfaceMapper::SetTargetSubdevice(UINT32 Subdev, bool force)
{
    if (Subdev >= m_Surf.GetGpuDev()->GetNumSubdevices())
    {
        Printf(Tee::PriHigh, "Invalid subdevice number!\n");
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return;
    }
    if ((m_Subdev != Subdev) || force)
    {
        if ((m_Surf.GetLocation() == Memory::Fb) || force)
        {
            Unmap();
        }
        m_Subdev = Subdev;
    }
}

RC SurfaceUtils::SurfaceMapper::Remap(UINT64 Offset, MapAccessType mapAccess, Surface2D::MemoryMappingMode mode)
{
    // Map appropriate part of the surface
    if ((Offset >= m_MappedEnd) || (Offset < m_MappedBegin))
    {
        // Unmap if it was already mapped
        Unmap();

        // Compute new mapped region boundary
        LwRm::Handle Handle = m_Surf.GetMemHandle();
        LwRm* pLwRm = m_Surf.GetLwRmPtr();
        UINT64 MappedBegin = (Offset >= GetExtraSize()) ? GetExtraSize() : 0;
        UINT64 MappedEnd = m_Surf.GetAllocSize();
        if (m_MaxMappedChunkSize != 0)
        {
            MappedBegin = (Offset / m_MaxMappedChunkSize)
                * m_MaxMappedChunkSize;
            MappedEnd = MappedBegin + m_MaxMappedChunkSize;
            if (MappedEnd > m_Surf.GetAllocSize())
            {
                MappedEnd = m_Surf.GetAllocSize();
            }
        }

        // Take split surfaces into account
        UINT64 RelativeMappedBegin = MappedBegin;
        UINT64 RelativeMappedEnd = MappedEnd;
        if (m_Surf.GetSplit())
        {
            MASSERT(m_Surf.GetExtraAllocSize() == 0);
            MASSERT(m_Surf.GetHiddenAllocSize() == 0);
            const UINT64 SplitBoundary = m_Surf.GetPartSize(0);
            if (Offset >= SplitBoundary)
            {
                Handle = m_Surf.GetSplitMemHandle();
                if (MappedBegin < SplitBoundary)
                {
                    MappedBegin = SplitBoundary;
                }
                RelativeMappedBegin = MappedBegin - SplitBoundary;
                RelativeMappedEnd -= SplitBoundary;
            }
            else if (MappedEnd > SplitBoundary)
            {
                const UINT64 Diff = MappedEnd - SplitBoundary;
                MappedEnd -= Diff;
                RelativeMappedEnd -= Diff;
            }
        }

        RelativeMappedBegin += m_Surf.GetCpuMapOffset();
        RelativeMappedEnd += m_Surf.GetCpuMapOffset();

        UINT32 flags = 0;

        switch (mapAccess)
        {
            case ReadOnly:
                flags |= LWOS33_FLAGS_ACCESS_READ_ONLY;
                break;

            case WriteOnly:
                flags |= LWOS33_FLAGS_ACCESS_WRITE_ONLY;
                break;

            case ReadWrite:
                flags |= LWOS33_FLAGS_ACCESS_READ_WRITE;
                break;

            default:
                Printf(Tee::PriHigh, "Invalid map accesss type passed to SurfaceUtils::SurfaceMapper::Remap\n");
                return RC::SOFTWARE_ERROR;
        }

        switch (mode)
        {
            case Surface2D::MapDefault:
                //flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DEFAULT, flags);
                break;

            case Surface2D::MapDirect:
                flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, flags);
                break;

            case Surface2D::MapReflected:
                flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _REFLECTED, flags);
                break;

            default:
                break;
        }

        // Map
        RC rc;
        GpuSubdevice* pSubdev = m_Surf.GetGpuSubdev(m_Subdev);
        if (m_RawImage && (m_Surf.GetLocation() == Memory::Fb || 
                pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING)))
        {
            LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS createFbSegParams = {0};
            UINT32 attr = m_Surf.GetVidHeapAttr();
            UINT32 attr2 = m_Surf.GetVidHeapAttr2();
            UINT64 physAddress = 0;

            attr = FLD_SET_DRF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR, attr);

            // Sysmem allocations using VidHeapHwAlloc need to be contiguous.
            // (See bug 1409262).
            if (LWOS32_ATTR_LOCATION_PCI == DRF_VAL(OS32, _ATTR, _LOCATION, attr))
            {
                attr = FLD_SET_DRF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS,
                                   attr);
            }

            UINT64 allocSize64 = MappedEnd - MappedBegin;
            CHECK_RC(pLwRm->VidHeapHwAlloc(
                        LWOS32_TYPE_IMAGE,
                        attr,
                        attr2,
                        m_Surf.GetWidth(),
                        m_Surf.GetPitch(),
                        m_Surf.GetHeight(),
                        UNSIGNED_CAST(UINT32, allocSize64),
                        &m_HWAllocHandle,
                        m_Surf.GetGpuDev()));

            m_Surf.GetPhysAddress(MappedBegin,&physAddress);
            createFbSegParams.VidOffset       = physAddress;
            createFbSegParams.ValidLength     = MappedEnd - MappedBegin;
            createFbSegParams.Length          = MappedEnd - MappedBegin;
            createFbSegParams.AllocHintHandle = m_HWAllocHandle;
            createFbSegParams.hClient         = pLwRm->GetClientHandle();
            createFbSegParams.hDevice         = pLwRm->GetDeviceHandle(m_Surf.GetGpuDev());
            createFbSegParams.Flags           =
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE);
            createFbSegParams.subDeviceIDMask = BIT(m_Subdev);
            CHECK_RC(pLwRm->CreateFbSegment(m_Surf.GetGpuDev(), &createFbSegParams));

            m_MappedHandle = createFbSegParams.hMemory;
            m_MappedAddress = (void *)createFbSegParams.ppCpuAddress[m_Subdev];
            m_MappedBegin = MappedBegin;
            m_MappedEnd = MappedEnd;
        }
        else
        {
            CHECK_RC(pLwRm->MapMemory(
                        Handle,
                        RelativeMappedBegin,
                        RelativeMappedEnd-RelativeMappedBegin,
                        &m_MappedAddress,
                        flags,
                        pSubdev));

            m_MappedHandle = Handle;
            m_MappedBegin = MappedBegin;
            m_MappedEnd = MappedEnd;
        }
    }
    return OK;
}

void SurfaceUtils::SurfaceMapper::Unmap()
{
    if (m_MappedAddress)
    {
        LwRm* pLwRm = m_Surf.GetLwRmPtr();
        GpuSubdevice* pSubdev = m_Surf.GetGpuSubdev(m_Subdev);
        if (m_RawImage && (m_Surf.GetLocation() == Memory::Fb || 
                pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING)))
        {
            LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyFbSegParams = {0};
            destroyFbSegParams.hMemory = m_MappedHandle;
            pLwRm->DestroyFbSegment(m_Surf.GetGpuDev(), &destroyFbSegParams);
            pLwRm->VidHeapHwFree(m_HWAllocHandle,LWOS32_DELETE_RESOURCES_ALL,m_Surf.GetGpuDev());
        }
        else
        {
            
            pLwRm->UnmapMemory(
                    m_MappedHandle,
                    m_MappedAddress,
                    0,
                    pSubdev);
        }
        m_MappedHandle = 0;
        m_MappedBegin = 0;
        m_MappedEnd = 0;
        m_MappedAddress = 0;
    }
}

RC SurfaceUtils::SurfaceMapper::Flush()
{
    RC rc;
    if (GetMappedSize() > 0)
    {
        CHECK_RC(Platform::FlushCpuWriteCombineBuffer());
    }
    return rc;
}

UINT64 SurfaceUtils::SurfaceMapper::GetExtraSize() const
{
    return m_Surf.GetHiddenAllocSize() + m_Surf.GetExtraAllocSize();
}

SurfaceUtils::MappingSurfaceReader::MappingSurfaceReader(const Surface& Surf)
: m_SurfMapper(Surf)
{
}

SurfaceUtils::MappingSurfaceReader::MappingSurfaceReader(const Surface& Surf, UINT32 Subdev)
: m_SurfMapper(Surf)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

SurfaceUtils::MappingSurfaceReader::~MappingSurfaceReader()
{
}

void SurfaceUtils::MappingSurfaceReader::SetTargetSubdevice(UINT32 Subdev)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

void SurfaceUtils::MappingSurfaceReader::SetTargetSubdeviceForce(UINT32 Subdev)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, true);
}

RC SurfaceUtils::SurfaceMapper::ReadBytes
(
    UINT64 Offset,
    void* Buf,
    size_t BufSize
)
{
    // Check end offset
    const UINT64 AbsEndOffset = Offset + GetExtraSize() + BufSize;
    if (AbsEndOffset > GetSurface().GetAllocSize())
    {
        Printf(Tee::PriHigh, "Requested mapping size 0x%llX"
                " at offset 0x%llX"
                " exceeds surface size 0x%llX.\n",
                static_cast<UINT64>(BufSize), Offset, GetSurface().GetSize());
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    // Account for invisible part of the surface
    Offset += GetExtraSize();

    while (BufSize > 0)
    {
        // Make sure the interesting area is mapped
        RC rc;
        CHECK_RC(Remap(Offset, ReadOnly));

        // Copy contents
        const UINT08* const Source = GetMappedAddress()
            + Offset - GetMappedOffset();
        const UINT64 MappedEnd = GetMappedEndOffset();
        UINT64 EndOffset = Offset + BufSize;
        if (EndOffset > MappedEnd)
        {
            EndOffset = MappedEnd;
        }
        const UINT32 CopySize = static_cast<UINT32>(EndOffset - Offset);
        Platform::VirtualRd(Source, Buf, CopySize);

        // Move pointers
        Buf = static_cast<UINT08*>(Buf) + CopySize;
        Offset += CopySize;
        BufSize -= CopySize;
    }
    return OK;
}

RC SurfaceUtils::MappingSurfaceReader::ReadBytes
(
    UINT64 Offset,
    void* Buf,
    size_t BufSize
)
{
    return m_SurfMapper.ReadBytes(Offset, Buf, BufSize);
}

RC SurfaceUtils::MappingSurfaceReader::ReadBlocklinearLines
(
    const BlockLinear& blAddressMapper,
    UINT32 y,
    UINT32 z,
    UINT32 arrayIndex,
    void* destBuffer,
    UINT32 lineCount
)
{
    MASSERT(!"Invalid call to SurfaceUtils::MappingSurfaceReader::ReadBlocklinearLines.");
    return RC::SOFTWARE_ERROR;
}

const Surface& SurfaceUtils::MappingSurfaceReader::GetSurface() const
{
    return m_SurfMapper.GetSurface();
}

SurfaceUtils::MappingSurfaceWriter::MappingSurfaceWriter(Surface& Surf)
: m_SurfMapper(Surf)
{
}

SurfaceUtils::MappingSurfaceWriter::MappingSurfaceWriter(Surface& Surf, UINT32 Subdev)
: m_SurfMapper(Surf)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

SurfaceUtils::MappingSurfaceWriter::~MappingSurfaceWriter()
{
}

void SurfaceUtils::MappingSurfaceWriter::SetTargetSubdevice(UINT32 Subdev)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

void SurfaceUtils::MappingSurfaceWriter::SetTargetSubdeviceForce(UINT32 Subdev)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, true);
}

RC SurfaceUtils::SurfaceMapper::WriteBytes
(
    UINT64 Offset,
    const void* Buf,
    size_t BufSize,
    Surface2D::MemoryMappingMode mode
)
{
    // Check end offset
    const UINT64 AbsEndOffset = Offset + GetExtraSize() + BufSize;
    if (AbsEndOffset > GetSurface().GetAllocSize())
    {
        Printf(Tee::PriHigh, "Requested mapping size 0x%llX"
                " at offset 0x%llX"
                " exceeds surface size 0x%llX.\n",
                static_cast<UINT64>(BufSize), Offset, GetSurface().GetSize());
        MASSERT(!"Generic assertion failure<Refer to previous message>.");
        return RC::SOFTWARE_ERROR;
    }

    // Account for invisible part of the surface
    Offset += GetExtraSize();

    while (BufSize > 0)
    {
        // Make sure the interesting area is mapped
        RC rc;
        CHECK_RC(Remap(Offset, WriteOnly, mode));

        // Overwrite contents
        UINT08* const Dest = GetMappedAddress()
            + Offset - GetMappedOffset();
        const UINT64 MappedEnd = GetMappedEndOffset();
        UINT64 EndOffset = Offset + BufSize;
        if (EndOffset > MappedEnd)
        {
            EndOffset = MappedEnd;
        }
        const UINT32 CopySize = static_cast<UINT32>(EndOffset - Offset);
        Platform::VirtualWr(Dest, Buf, CopySize);

        // Move pointers
        Buf = static_cast<const UINT08*>(Buf) + CopySize;
        Offset += CopySize;
        BufSize -= CopySize;
    }
    return OK;
}

RC SurfaceUtils::MappingSurfaceWriter::WriteBytes
(
    UINT64 Offset,
    const void* Buf,
    size_t BufSize,
    Surface2D::MemoryMappingMode mode
)
{
    RC rc;
    CHECK_RC(m_SurfMapper.WriteBytes(Offset, Buf, BufSize, mode));
    CHECK_RC(m_SurfMapper.Flush());
    return rc;
}

const Surface& SurfaceUtils::MappingSurfaceWriter::GetSurface() const
{
    return m_SurfMapper.GetSurface();
}

SurfaceUtils::MappingSurfaceReadWriter::MappingSurfaceReadWriter
(
    Surface& Surf
) :
    m_SurfMapper(Surf)
{
}

SurfaceUtils::MappingSurfaceReadWriter::MappingSurfaceReadWriter
(
    Surface& Surf,
    UINT32 Subdev
) :
    m_SurfMapper(Surf)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

void SurfaceUtils::MappingSurfaceReadWriter::SetTargetSubdevice(UINT32 Subdev)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, false);
}

void SurfaceUtils::MappingSurfaceReadWriter::SetTargetSubdeviceForce
(
    UINT32 Subdev
)
{
    m_SurfMapper.SetTargetSubdevice(Subdev, true);
}

RC SurfaceUtils::MappingSurfaceReadWriter::ReadBytes
(
    UINT64 Offset,
    void* Buf,
    size_t BufSize
)
{
    return m_SurfMapper.ReadBytes(Offset, Buf, BufSize);
}

RC SurfaceUtils::MappingSurfaceReadWriter::ReadBlocklinearLines
(
    const BlockLinear& blAddressMapper,
    UINT32 y,
    UINT32 z,
    UINT32 arrayIndex,
    void* destBuffer,
    UINT32 lineCount
)
{
    MASSERT(!"Invalid call to SurfaceUtils::MappingSurfaceReadWriter::ReadBlocklinearLines.");
    return RC::SOFTWARE_ERROR;
}

RC SurfaceUtils::MappingSurfaceReadWriter::WriteBytes
(
    UINT64 Offset,
    const void* Buf,
    size_t BufSize
)
{
    RC rc;
    CHECK_RC(m_SurfMapper.WriteBytes(Offset, Buf, BufSize));
    CHECK_RC(m_SurfMapper.Flush());
    return rc;
}

const Surface& SurfaceUtils::MappingSurfaceReadWriter::GetSurface() const
{
    return m_SurfMapper.GetSurface();
}

SurfaceUtils::VASpaceMapping::VASpaceMapping()
: m_hVASpace(0U)
{
}

SurfaceUtils::VASpaceMapping::~VASpaceMapping()
{
    ReleaseMappings();
}

void SurfaceUtils::VASpaceMapping::SetVASpace(LwRm::Handle hVASpace)
{
    m_hVASpace = hVASpace;
}

RC SurfaceUtils::VASpaceMapping::MapSurface(Surface2D* pSurface, UINT64* pVirtAddr)
{
    LwRm::Handle hMapping;
    RC rc;
    CHECK_RC(pSurface->MapToVASpace(m_hVASpace, &hMapping, pVirtAddr));
    m_Mappings.insert(hMapping);
    return rc;
}

void SurfaceUtils::VASpaceMapping::ReleaseMappings()
{
    set<LwRm::Handle>::iterator it = m_Mappings.begin();
    for ( ; it != m_Mappings.end(); ++it)
    {
        LwRmPtr()->Free(*it);
    }
    m_Mappings.clear();
}

RC SurfaceUtils::ReadSurface(const Surface& Surf, UINT64 Offset, void* Buf, size_t BufSize, UINT32 Subdev)
{
    MappingSurfaceReader reader(Surf, Subdev);
    return reader.ReadBytes(Offset, Buf, BufSize);
}

RC SurfaceUtils::WriteSurface(Surface& Surf, UINT64 Offset, const void* Buf, size_t BufSize, UINT32 Subdev)
{
    MappingSurfaceWriter writer(Surf, Subdev);
    return writer.WriteBytes(Offset, Buf, BufSize);
}
