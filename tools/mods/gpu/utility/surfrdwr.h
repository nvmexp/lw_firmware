/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surfrdwr.h
 * @brief Utilities for reading and writing surfaces.
 */

#ifndef INCLUDED_SURFRDWR_H
#define INCLUDED_SURFRDWR_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#include "surf2d.h"

typedef Surface2D Surface;
class BlockLinear;

namespace SurfaceUtils {

class ISurfaceReader {
public:
    ISurfaceReader() { }
    virtual ~ISurfaceReader() { }
    //! Sets subdevice on which the surface will be read.
    //!
    //! By default subdevice 0 is accessed, unless this function is called.
    //! The surface is not remapped for the new subdevice if it is
    //! located in system memory.
    virtual void SetTargetSubdevice(UINT32 Subdev) = 0;
    //! Forcefully sets subdevice on which the surface will be read.
    //!
    //! By default subdevice 0 is accessed, unless this function is called.
    //! This function always causes surface remapping.
    virtual void SetTargetSubdeviceForce(UINT32 Subdev) = 0;
    //! Reads the specified number of bytes into the provided buffer.
    //! @param Offset Offset from the beginning of usable surface area. ExtraAllocSize and HiddenAllocSize are automatically added.
    //! @param Buf Buffer to read the surface into.
    //! @param Size Number of bytes to read.
    virtual RC ReadBytes(UINT64 Offset, void* Buf, size_t Size) = 0;

    //! Reads the specified number of lines from a blocklinear surface
    //! and stores those lines in a pitch layout in the provided buffer.
    //! @param blAddressMapper The blocklinear info object.
    //! @param y The y coordinate of the first line to read
    //! @param z The z coordinate (depth index) of the blocks that will be read.
    //! @param arrayIndex Index of the array element to read.
    //! @param destBuffer The buffer to read the surface into.
    //! @param lineCount The number of lines to read from the surface.
    virtual RC ReadBlocklinearLines
    (
        const BlockLinear& blAddressMapper,
        UINT32 y,
        UINT32 z,
        UINT32 arrayIndex,
        void* destBuffer,
        UINT32 lineCount
    ) = 0;
    //! Gives read-only access to the surface being read.
    virtual const Surface& GetSurface() const = 0;

private:
    // Non-copyable
    ISurfaceReader(const ISurfaceReader&);
    ISurfaceReader& operator=(const ISurfaceReader&);
};

class ISurfaceWriter {
public:

    ISurfaceWriter() { }
    virtual ~ISurfaceWriter() { }
    //! Sets subdevice on which the surface will be written.
    //!
    //! By default subdevice 0 is accessed, unless this function is called.
    //! The surface is not remapped for the new subdevice if it is
    //! located in system memory.
    virtual void SetTargetSubdevice(UINT32 Subdev) = 0;
    //! Forcefully sets subdevice on which the surface will be written.
    //!
    //! By default subdevice 0 is accessed, unless this function is called.
    //! This function always causes surface remapping.
    virtual void SetTargetSubdeviceForce(UINT32 Subdev) = 0;
    //! Writes to the surface.
    //! @param Offset Offset from the beginning of usable surface area. ExtraAllocSize and HiddenAllocSize are automatically added.
    //! @param Buf Data to write to the surface at the specified offset.
    //! @param Size Number of bytes to write.
    virtual RC WriteBytes(UINT64 Offset, const void* Buf, size_t Size, Surface2D::MemoryMappingMode mode = Surface2D::MapDefault) = 0;
    //! Gives read-only access to the surface being written to.
    virtual const Surface& GetSurface() const = 0;

private:
    // Non-copyable
    ISurfaceWriter(const ISurfaceWriter&);
    ISurfaceWriter& operator=(const ISurfaceWriter&);
};

class ISurfaceReadWriter : public ISurfaceReader
{
public:
    //! Writes to the surface.
    //! \param Offset Offset from the beginning of usable surface area.
    //!     ExtraAllocSize and HiddenAllocSize are automatically added.
    //! \param Buf Data to write to the surface at the specified offset.
    //! \param Size Number of bytes to write.
    virtual RC WriteBytes(UINT64 Offset, const void* Buf, size_t Size) = 0;
};

//! Helper class for MappingSurfaceReader and MappingSurfaceWriter.
class SurfaceMapper {
public:
    enum MapAccessType
    {
        ReadOnly,
        WriteOnly,
        ReadWrite
    };

    explicit SurfaceMapper(const Surface& Surf);
    ~SurfaceMapper();
    void SetMaxMappedChunkSize(UINT32 Size);
    void SetTargetSubdevice(UINT32 Subdev, bool force);
    void SetRawImage(bool Enable) { m_RawImage = Enable;}
    RC Remap(UINT64 Offset, MapAccessType mapAccess, Surface2D::MemoryMappingMode mode = Surface2D::MapDefault);
    void Unmap();
    const Surface& GetSurface() const { return m_Surf; }
    UINT64 GetExtraSize() const;
    UINT08* GetMappedAddress() const { return static_cast<UINT08*>(m_MappedAddress); }
    UINT64 GetMappedOffset() const { return m_MappedBegin; }
    UINT64 GetMappedEndOffset() const { return m_MappedEnd; }
    UINT64 GetMappedSize() const { return m_MappedEnd - m_MappedBegin; }
    RC ReadBytes(UINT64 Offset, void* Buf, size_t Size);
    RC WriteBytes(UINT64 Offset, const void* Buf, size_t Size, Surface2D::MemoryMappingMode mode = Surface2D::MapDefault);
    RC Flush();

private:
    const Surface& m_Surf;
    UINT32 m_Subdev;
    UINT32 m_MaxMappedChunkSize;
    LwRm::Handle m_MappedHandle;
    UINT64 m_MappedBegin;
    UINT64 m_MappedEnd;
    void* m_MappedAddress;
    bool m_RawImage;
    LwRm::Handle m_HWAllocHandle;
};

//! Surface reader which uses mapping to access the surface.
class MappingSurfaceReader: public ISurfaceReader {
public:
    //! @param Surf %Surface to read.
    explicit MappingSurfaceReader(const Surface& Surf);
    //! @param Surf %Surface to read.
    //! @param Subdev Initial subdevice from which to read the surface.
    MappingSurfaceReader(const Surface& Surf, UINT32 Subdev);
    virtual ~MappingSurfaceReader();
    //! Sets maximum size of a mapped region.
    void SetMaxMappedChunkSize(UINT32 Size) {
        m_SurfMapper.SetMaxMappedChunkSize(Size);
    }
    void SetRawImage(bool Enable) {
        m_SurfMapper.SetRawImage(Enable);
    }
    //! Unmaps the surface if it's mapped.
    void Unmap() {
        m_SurfMapper.Unmap();
    }
    virtual void SetTargetSubdevice(UINT32 Subdev);
    virtual void SetTargetSubdeviceForce(UINT32 Subdev);
    virtual RC ReadBytes(UINT64 Offset, void* Buf, size_t Size);
    virtual RC ReadBlocklinearLines
    (
        const BlockLinear& blAddressMapper,
        UINT32 y,
        UINT32 z,
        UINT32 arrayIndex,
        void* destBuffer,
        UINT32 lineCount
    );
    virtual const Surface& GetSurface() const;

private:
    SurfaceMapper m_SurfMapper;
};

//! Surface writer which uses mapping to access the surface.
class MappingSurfaceWriter: public ISurfaceWriter {
public:
    //! @param Surf %Surface to write to.
    explicit MappingSurfaceWriter(Surface& Surf);
    //! @param Surf %Surface to write to.
    //! @param Subdev Initial subdevice on which to write to the surface.
    MappingSurfaceWriter(Surface& Surf, UINT32 Subdev);
    virtual ~MappingSurfaceWriter();
    //! Sets maximum size of a mapped region.
    void SetMaxMappedChunkSize(UINT32 Size) {
        m_SurfMapper.SetMaxMappedChunkSize(Size);
    }
    //! Unmaps the surface if it's mapped.
    void Unmap() {
        m_SurfMapper.Unmap();
    }
    virtual void SetTargetSubdevice(UINT32 Subdev);
    virtual void SetTargetSubdeviceForce(UINT32 Subdev);
    virtual RC WriteBytes(UINT64 Offset, const void* Buf, size_t Size, Surface2D::MemoryMappingMode mode = Surface2D::MapDefault);
    virtual const Surface& GetSurface() const;

private:
    SurfaceMapper m_SurfMapper;
};

//! Surface reader/writer which uses mapping to access the surface.
class MappingSurfaceReadWriter: public ISurfaceReadWriter {
public:
    //! \param Surf %Surface to write to.
    explicit MappingSurfaceReadWriter(Surface& Surf);
    //! \param Surf %Surface to write to.
    //! \param Subdev Initial subdevice on which to write to the surface.
    MappingSurfaceReadWriter(Surface& Surf, UINT32 Subdev);

    //! Sets maximum size of a mapped region.
    void SetMaxMappedChunkSize(UINT32 Size) {
        m_SurfMapper.SetMaxMappedChunkSize(Size);
    }
    //! Unmaps the surface if it's mapped.
    void Unmap() {
        m_SurfMapper.Unmap();
    }
    virtual void SetTargetSubdevice(UINT32 Subdev);
    virtual void SetTargetSubdeviceForce(UINT32 Subdev);
    virtual RC ReadBytes(UINT64 Offset, void* Buf, size_t Size);
    virtual RC ReadBlocklinearLines
    (
        const BlockLinear& blAddressMapper,
        UINT32 y,
        UINT32 z,
        UINT32 arrayIndex,
        void* destBuffer,
        UINT32 lineCount
    );
    virtual RC WriteBytes(UINT64 Offset, const void* Buf, size_t Size);
    virtual const Surface& GetSurface() const;

private:
    SurfaceMapper m_SurfMapper;
};

class VASpaceMapping
{
public:
    VASpaceMapping();
    ~VASpaceMapping();

    void SetVASpace(LwRm::Handle hVASpace);
    RC MapSurface(Surface2D* pSurface, UINT64* pVirtAddr);
    void ReleaseMappings();

private:
    LwRm::Handle      m_hVASpace;
    set<LwRm::Handle> m_Mappings;
};

//! Reads contents of the surface to the specified buffer.
//! @param Surf %Surface to read from.
//! @param Offset Offset to start reading from. ExtraAllocSize and HiddenAllocSize are automatically added.
//! @param Buf Buffer to fill with the surface contents.
//! @param BufSize Size of the buffer to fill, same as number of bytes to read.
//! @param Subdev Subdevice on which to access the surface.
RC ReadSurface(const Surface& Surf, UINT64 Offset, void* Buf, size_t BufSize, UINT32 Subdev);
//! Writes new contents to the surface.
//! @param Surf %Surface to write to.
//! @param Offset Offset to start writing at. ExtraAllocSize and HiddenAllocSize are automatically added.
//! @param Buf Buffer to fill with the surface with.
//! @param BufSize Size of the buffer with new contents, i.e. the number of bytes to write.
//! @param Subdev Subdevice on which to access the surface.
RC WriteSurface(Surface& Surf, UINT64 Offset, const void* Buf, size_t BufSize, UINT32 Subdev);

} // namespace SurfaceUtils

#endif // INCLUDED_SURFRDWR_H
