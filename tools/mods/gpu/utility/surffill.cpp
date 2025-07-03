/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "surffill.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "surf2d.h"
#include "surfrdwr.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "zbccolwertutil.h"
#include "class/cl9096.h"
#include "class/cla097.h"
#include "class/cl90b5.h"
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "class/cla06f.h"
#include "class/cla06fsubch.h"
#include "ctrl/ctrl9096.h"

namespace
{
    bool s_Allow3DFill = true;
}

P_(Global_Get_SurfFillAllow3D);
P_(Global_Set_SurfFillAllow3D);
static SProperty Global_SurfFillAllow3D
(
    "SurfFillAllow3D",
    0,
    false,
    Global_Get_SurfFillAllow3D,
    Global_Set_SurfFillAllow3D,
    0,
    "Allow use of 3D class in surface fills."
);

P_(Global_Get_SurfFillAllow3D)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    if (OK != JavaScriptPtr()->ToJsval(s_Allow3DFill, pValue))
    {
        JavaScriptPtr()->Throw(pContext, RC::CANNOT_COLWERT_BOOLEAN_TO_JSVAL,
                               "Failed to get SurfFillAllow3D.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_SurfFillAllow3D)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    // Colwert the argument.
    bool state = false;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &state))
    {
        JavaScriptPtr()->Throw(pContext, RC::CANNOT_COLWERT_JSVAL_TO_BOOLEAN,
                               "Failed to set SurfFillAllow3D.");
        return JS_FALSE;
    }

    s_Allow3DFill = state;

    return JS_TRUE;
}

//--------------------------------------------------------------------
//! \brief Fill a surface with an 8-bit value
//!
RC SurfaceUtils::FillSurface08
(
    Surface2D *pSurface,
    UINT08 fillValue,
    const GpuSubdevice *pGpuSubdevice
)
{
    return FillSurface(pSurface, fillValue, 8, pGpuSubdevice);
}

//--------------------------------------------------------------------
//! \brief Fill a surface with a 32-bit little-endian value
//!
RC SurfaceUtils::FillSurface32
(
    Surface2D *pSurface,
    UINT32 fillValue,
    const GpuSubdevice *pGpuSubdevice
)
{
    return FillSurface(pSurface, fillValue, 32, pGpuSubdevice);
}

//--------------------------------------------------------------------
//! Fill a surface with an N-bit little-endian value, where N is
//! determinedby bpp.  In SLI, this method does a broadcast fill
//! unless you specify pGpuSubdevice.
//!
RC SurfaceUtils::FillSurface
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    const GpuSubdevice *pGpuSubdevice,
    FillMethod fillMethod
)
{
    const UINT64 ALLOC_OVERHEAD = 0x400000;
    RC rc;

    // An OptimalSurfaceFiller usually uses the GPU to fill the
    // surface.  But this colwenience-wrapper subroutine doesn't
    // re-use the object, so for smaller surfaces, it's faster to use
    // memory-mapped CPU writes to save the overhead of allocating a
    // channel and such.  Experimentally, the crossover point seems to
    // be around 4M, at least on GK110 and T210.

    const bool useOptimalSurfaceFiller = pSurface->GetArrayPitch() * pSurface->GetArraySize() >= ALLOC_OVERHEAD ||
        (pSurface->GetLayout() == Surface2D::BlockLinear &&
        (pGpuSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK)));

    // LWENC/LWDEC tests in CheetAh may want to run the tests without
    // using the GPU. The useMappingSurfaceFiller is used in them.
    // When the platform is FModel always use the CPU to fill the surface.
    if ((fillMethod != FillMethod::Cpufill &&
            Platform::GetSimulationMode() != Platform::Fmodel) && useOptimalSurfaceFiller)
    {
        OptimalSurfaceFiller filler(pSurface->GetGpuDev());
        CHECK_RC(filler.SetTargetSubdevice(pGpuSubdevice));
        CHECK_RC(filler.Fill(pSurface, fillValue, bpp));
    }
    else
    {
        MappingSurfaceFiller filler(pSurface->GetGpuDev());
        CHECK_RC(filler.SetTargetSubdevice(pGpuSubdevice));
        CHECK_RC(filler.Fill(pSurface, fillValue, bpp));
    }
    return rc;
}

//--------------------------------------------------------------------
SurfaceUtils::ISurfaceFiller::ISurfaceFiller(GpuDevice *pGpuDevice) :
    m_pGpuDevice(pGpuDevice),
    m_pTargetSubdevice(nullptr),
    m_AutoWait(true)
{
    MASSERT(pGpuDevice);
}

//--------------------------------------------------------------------
//! \brief Select the subdevice to fill on SLI
//!
//! If pTargetSubdevice is nullptr (the default), then we do a
//! broadcast fill.  The target subdevice is mostly ignored on non-SLI
//! systems.
//!
RC SurfaceUtils::ISurfaceFiller::SetTargetSubdevice
(
    const GpuSubdevice *pTargetSubdevice
)
{
    MASSERT(pTargetSubdevice == nullptr ||
            pTargetSubdevice->GetParentDevice() == m_pGpuDevice);
    RC rc;

    // In SLI, the current implementation of WaitSemaphore() won't
    // work if we change the target subdevice since calling
    // WriteSemaphore().  Calling Wait() here avoids that problem.
    //
    if (m_pGpuDevice->GetNumSubdevices() > 1 &&
        m_pTargetSubdevice != pTargetSubdevice)
    {
        CHECK_RC(Wait());
    }

    m_pTargetSubdevice = pTargetSubdevice;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Fill the entire surface, except the Hidden/Extra/CDE part.
//!
//! \param pSurface The surface to fill
//! \param fillValue The value to write to the surface.
//! \param bpp The number of bits of fillValue to use.
//!
RC SurfaceUtils::ISurfaceFiller::Fill
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp
)
{
    const UINT64 fillSize = (pSurface->GetArrayPitch() *
                             pSurface->GetArraySize());
    return FillRange(pSurface, fillValue, bpp, 0, fillSize);
}

//--------------------------------------------------------------------
//! This method is used to help implement the static IsSupported()
//! methods in the subclasses.
//!
/* static */ bool SurfaceUtils::ISurfaceFiller::IsSupported
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    if (bpp == 0 || bpp > 64 || bpp % 8 != 0)
    {
        return false;
    }
    return true;
}

//--------------------------------------------------------------------
//! This method is adjusts the fillValue & bpp arguments passed to
//! Fill() to match the PTE (page-table entry).
//!
//! For example, suppose the "kind" field in the PTE says that a page
//! has 32-bit color values, and the caller asks to fill the page with
//! an 8-bit value such as 0x42.  Unfortunately, the 3D engine fails
//! with an interrupt if we use an 8-bit color in a 32-bit page.  So
//! this method colwerts the values passed by the caller into the
//! 32-bit value 0x42424242.
//!
//! This method isn't used by all subclasses.  Most of them aren't as
//! fussy as the 3D engine.
//!
//! Lwrrently, this method only works if the required bpp is an even
//! multiple of the caller-requested bpp.
//!
//! \return OK on success, or BAD_PARAMETER if a the operation is impossible
//!
/* static */ RC SurfaceUtils::ISurfaceFiller::ResizeFillValue
(
    const Surface2D &surface,
    UINT64 *pFillValue,
    UINT32 *pBpp
)
{
    MASSERT(pFillValue);
    MASSERT(pBpp);
    UINT32 surfaceBpp;

    // Figure out how many bpp (bits per pixel) are required by the
    // surface.  Derive this from the attr bits if possible, or the
    // color format if not.
    //
    // The complication is mostly due to tests such as GpuTrace, which
    // uses SetDefaultVidHeapAttr() to set the attr bits directly, and
    // sets the color format to Y8.  In that case, the color format
    // says the surface has 8 bpp, but the GPU cares about the bpp in
    // the attr bits.
    //
    switch (DRF_VAL(OS32, _ATTR, _DEPTH, surface.GetVidHeapAttr()))
    {
        case LWOS32_ATTR_DEPTH_8:
            surfaceBpp = 8;
            break;
        case LWOS32_ATTR_DEPTH_16:
            surfaceBpp = 16;
            break;
        case LWOS32_ATTR_DEPTH_24:
            surfaceBpp = 24;
            break;
        case LWOS32_ATTR_DEPTH_32:
            surfaceBpp = 32;
            break;
        case LWOS32_ATTR_DEPTH_64:
            surfaceBpp = 64;
            break;
        case LWOS32_ATTR_DEPTH_128:
            surfaceBpp = 128;
            break;
        default:
            surfaceBpp = surface.GetBitsPerPixel();
            break;
    }

    // Adjust *pFillValue and *pBpp to match the surface's bpp, if
    // possible.
    //
    if (*pBpp == 0 || surfaceBpp < *pBpp || surfaceBpp % *pBpp != 0)
    {
        return RC::BAD_PARAMETER;
    }
    while (*pBpp < surfaceBpp)
    {
        const UINT64 bppMask = _UINT64_MAX >> (64 - *pBpp);
        *pFillValue = (*pFillValue << *pBpp) | (*pFillValue & bppMask);
        *pBpp = min(2 * *pBpp, surfaceBpp);
    }
    return OK;
}

//--------------------------------------------------------------------
//! Colwenience method to set the subdevice mask on a channel to the
//! target subdevice.  Does nothing on non-SLI systems and/or
//! broadcast mode.
//!
//! \param pCh The channel to write the mask on
//! \param[out] pDidSetMask If true, then this method set the
//!     subdevice mask, and the caller must restore broadcast mode
//!     later.
//!
RC SurfaceUtils::ISurfaceFiller::SetSubdeviceMask
(
    Channel *pCh,
    bool *pDidSetMask
)
{
    MASSERT(pCh);
    MASSERT(pDidSetMask);
    RC rc;

    *pDidSetMask = false;
    if (m_pGpuDevice->GetNumSubdevices() > 1 && m_pTargetSubdevice != nullptr)
    {
        const UINT32 mask = 1 << m_pTargetSubdevice->GetSubdeviceInst();
        CHECK_RC(pCh->WriteSetSubdevice(mask));
        *pDidSetMask = true;
    }
    return rc;
}

//--------------------------------------------------------------------
//! Colwenience method to restore a channel to broadcast mode.
//!
//! \param pCh The channel.  May be nullptr, in which case this method
//!     does nothing.
//!
/* static */ RC SurfaceUtils::ISurfaceFiller::ClearSubdeviceMask(Channel *pCh)
{
    RC rc;
    if (pCh)
    {
        CHECK_RC(pCh->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }
    return rc;
}

//--------------------------------------------------------------------
//! Colwenience method to write a semaphore address & payload to a channel.
//!
//! This method takes a NotifySemaphore and channel, increments the
//! semaphore's TriggerPayload by 1, and writes methods to the channel
//! so that it'll trigger the semaphore.
//!
//! In SLI, this method assumes that pSemaphore has one target
//! semaphore per subchannel, and that the caller has already set the
//! subdevice mask.
//!
RC SurfaceUtils::ISurfaceFiller::WriteSemaphore
(
    NotifySemaphore *pSemaphore,
    Channel         *pCh,
    UINT32           subch,
    UINT32           methodA
)
{
    MASSERT(pSemaphore);
    MASSERT(pCh);
    RC rc;

    // Prepare the payload and update the semaphore so
    // that it'll trigger in response to the new payload.
    //
    const UINT32 oldTriggerPayload = pSemaphore->GetTriggerPayload(0);
    const UINT32 newTriggerPayload = oldTriggerPayload + 1;
    pSemaphore->SetPayload(oldTriggerPayload);
    pSemaphore->SetTriggerPayload(newTriggerPayload);

    // Write the semaphore address to the channel.  In broadcast SLI,
    // we write a different address to each subdevice.
    //
    if (m_pGpuDevice->GetNumSubdevices() > 1 && m_pTargetSubdevice == nullptr)
    {
        // Broadcast SLI
        Utility::CleanupOnReturnArg<void, RC, Channel*> cleanupSubdeviceMask(
                &ClearSubdeviceMask, pCh);
        for (UINT32 subdev = 0;
             subdev < m_pGpuDevice->GetNumSubdevices(); ++subdev)
        {
            const UINT64 virtualAddr = pSemaphore->GetCtxDmaOffsetGpu(subdev);
            CHECK_RC(pCh->WriteSetSubdevice(1 << subdev));
            CHECK_RC(pCh->Write(subch, methodA, 3,
                        LwU64_HI32(virtualAddr),
                        LwU64_LO32(virtualAddr),
                        newTriggerPayload));
        }
    }
    else
    {
        // Just one subdevice
        const UINT32 subdevInst =
            (m_pTargetSubdevice ? m_pTargetSubdevice->GetSubdeviceInst() : 0);
        const UINT64 virtualAddr = pSemaphore->GetCtxDmaOffsetGpu(subdevInst);
        CHECK_RC(pCh->Write(subch, methodA, 3,
                    LwU64_HI32(virtualAddr),
                    LwU64_LO32(virtualAddr),
                    newTriggerPayload));
    }
    return rc;
}

//--------------------------------------------------------------------
//! Colwenience method to wait for the semaphore used by
//! WriteSemaphore().  On CheetAh, this method also flushes the GPU.
//!
RC SurfaceUtils::ISurfaceFiller::WaitSemaphore
(
    NotifySemaphore *pSemaphore,
    Channel *pCh,
    FLOAT64 timeoutMs
)
{
    MASSERT(pSemaphore);
    MASSERT(pCh);
    RC rc;

    if (m_pGpuDevice->GetNumSubdevices() > 1 && m_pTargetSubdevice != nullptr)
    {
        const UINT32 targetInst = m_pTargetSubdevice->GetSubdeviceInst();
        CHECK_RC(pSemaphore->WaitOnOne(targetInst, timeoutMs));
    }
    else
    {
        CHECK_RC(pSemaphore->Wait(timeoutMs));
    }

    if (m_pGpuDevice->GetSubdevice(0)->IsSOC())
    {
        CHECK_RC(pCh->WriteL2FlushDirty());
        CHECK_RC(pCh->WaitIdle(timeoutMs));
    }

    return rc;
}

//--------------------------------------------------------------------
//! Tells if the indicated FillRange() parameters are supported in this subclass
//!
/* static */ bool SurfaceUtils::MappingSurfaceFiller::IsSupported
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    if (!ISurfaceFiller::IsSupported(surface, fillValue, bpp, offset, size))
    {
        return false;
    }

    // We don't have correct support for 24bpp pixels with BlockLinear
    // in MappingSurfaceFiller.
    if (surface.GetLayout() == Surface2D::BlockLinear && bpp == 24)
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief Fill a range of the surface with a value
//!
//! This is the "important" method of this class.
//!
//! \param pSurface The surface to write
//! \param fillValue The little-endian value to write into the surface
//! \param bpp The number of bits of fillValue to use
//! \param offset The offset into the surface, relative to the first
//!     byte to the first byte after the Hidden & Extra data.
//! \param size The number of bytes to fill, starting at offset
//!
/* virtual */ RC SurfaceUtils::MappingSurfaceFiller::FillRange
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    MASSERT(IsSupported(*pSurface, fillValue, bpp, offset, size));
    MASSERT(pSurface->GetGpuDev() == GetGpuDevice());
    const UINT32 MAX_BUFFER_SIZE = 4096;
    const UINT64 startOffset = (pSurface->GetHiddenAllocSize() +
                                pSurface->GetExtraAllocSize() +
                                offset);
    const UINT64 endOffset = startOffset + size;
    RC rc;

    const UINT32 alignment = pSurface->GetLayout() == Surface2D::BlockLinear ?
                             GetGpuDevice()->GobSize() : pSurface->GetPitch();

    // Load the fillValue into a buffer so that we can write it in
    // reasonably large chunks
    //
    vector<UINT08> fillBuffer;
    fillBuffer.reserve(max<size_t>(MAX_BUFFER_SIZE, alignment));
    for (size_t bits = 0; bits < bpp; bits += 8)
    {
        fillBuffer.push_back(static_cast<UINT08>(fillValue >> bits));
    }
    const auto ExpandVector = [](auto& buffer, size_t maxSize)
    {
        const auto unitSize = buffer.size();
        while (buffer.size() * 2 <= maxSize)
        {
            const auto oldSize = buffer.size();
            buffer.resize(oldSize * 2);
            const auto midPoint = buffer.begin() + oldSize;
            copy(buffer.begin(), midPoint, midPoint);
        }
        if (buffer.size() < maxSize)
        {
            const auto remaining = ((maxSize - buffer.size()) / unitSize) * unitSize;
            if (remaining > 0)
            {
                const auto oldSize = buffer.size();
                MASSERT(remaining < oldSize);
                buffer.resize(oldSize + remaining);
                copy(buffer.begin(), buffer.begin() + remaining, buffer.begin() + oldSize);
            }
        }
    };
    // Ensure we adhere to pitch alignment or GOB alignment
    ExpandVector(fillBuffer, alignment);
    // Add FFs at the end to meet alignment
    if (fillBuffer.size() < alignment)
    {
        fillBuffer.resize(alignment, 0xFF);
    }
    // Fill the entire buffer up to max size with repeated pitch-aligned blocks
    ExpandVector(fillBuffer, min<UINT64>(size, MAX_BUFFER_SIZE));

    // Create a SurfaceMapper to write the surface
    //
    SurfaceMapper mapper(*pSurface);
    if (GetGpuDevice()->GetNumSubdevices() > 1)
    {
        mapper.SetTargetSubdevice(GetTargetSubdevice()->GetSubdeviceInst(),
                                    false);
    }

    size_t copySize = 0;
    for (UINT64 dstOffset = startOffset;
         dstOffset < endOffset; dstOffset += copySize)
    {
        // Make sure the interesting area is mapped
        //
        CHECK_RC(mapper.Remap(dstOffset, SurfaceMapper::WriteOnly));

        // Decide how many bytes we can copy in this iteration
        //
        const size_t srcOffset =
            static_cast<size_t>((dstOffset - startOffset) % alignment);
        copySize = fillBuffer.size() - srcOffset;
        if (copySize > mapper.GetMappedEndOffset() - dstOffset)
        {
            copySize = static_cast<size_t>(mapper.GetMappedEndOffset() -
                                           dstOffset);
        }
        if (copySize > endOffset - dstOffset)
        {
            copySize = static_cast<size_t>(endOffset - dstOffset);
        }

        // Copy bytes into the surface
        //
        UINT08 *pDest = (mapper.GetMappedAddress() +
                         dstOffset - mapper.GetMappedOffset());
        Platform::VirtualWr(pDest, &fillBuffer[srcOffset],
                            static_cast<UINT32>(copySize));
    }
    return rc;
}

//--------------------------------------------------------------------
SurfaceUtils::CopyEngineSurfaceFiller::CopyEngineSurfaceFiller
(
    GpuDevice *pGpuDevice,
    UINT32 engineNum
) :
    ISurfaceFiller(pGpuDevice),
    m_EngineNum(engineNum),
    m_pCh(nullptr),
    m_hCh(0),
    m_hVASpace(0U)
{
}

//--------------------------------------------------------------------
//! Tells if the indicated FillRange() parameters are supported in this subclass
//!
/* static */ bool SurfaceUtils::CopyEngineSurfaceFiller::IsSupported
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    GpuDevice *pGpuDevice = surface.GetGpuDev();
    DmaCopyAlloc tmpDmaCopyAlloc;

    return
        ISurfaceFiller::IsSupported(surface, fillValue, bpp, offset, size) &&
        (bpp == 8 || bpp == 16 || bpp == 32 || bpp == 64) &&
        tmpDmaCopyAlloc.IsSupported(pGpuDevice);
}

//--------------------------------------------------------------------
//! \brief Allocate the channel & objects used by this class, if needed
//!
RC SurfaceUtils::CopyEngineSurfaceFiller::Alloc()
{
    const UINT32 SUBCH = LWA06F_SUBCHANNEL_COPY_ENGINE;
    GpuDevice *pGpuDevice = GetGpuDevice();
    UINT32 numSubdevices = pGpuDevice->GetNumSubdevices();
    LwRmPtr pLwRm;
    RC rc;

    if (!m_hVASpace)
    {
        LW_VASPACE_ALLOCATION_PARAMETERS params = { 0 };
        CHECK_RC(pLwRm->Alloc(
                    pLwRm->GetDeviceHandle(pGpuDevice),
                    &m_hVASpace,
                    FERMI_VASPACE_A,
                    &params));
        m_Mapping.SetVASpace(m_hVASpace);
    }

    if (!m_pCh)
    {
        m_TestConfig.BindGpuDevice(pGpuDevice);
        CHECK_RC(m_TestConfig.InitFromJs());

        UINT32 engineId;
        if (m_EngineNum == ANY_ENGINE)
        {
            CHECK_RC(m_DmaCopyAlloc.GetDefaultEngineId(pGpuDevice, pLwRm.Get(), &engineId));
        }
        else
        {
            CHECK_RC(m_DmaCopyAlloc.GetEngineId(pGpuDevice, pLwRm.Get(), m_EngineNum, &engineId));
        }

        CHECK_RC(m_TestConfig.AllocateChannelGr(&m_pCh,
                                                &m_hCh,
                                                nullptr,
                                                m_hVASpace,
                                                &m_DmaCopyAlloc,
                                                0,
                                                engineId));

        CHECK_RC(m_pCh->SetObject(SUBCH, m_DmaCopyAlloc.GetHandle()));
        if (m_DmaCopyAlloc.GetClass() == GF100_DMA_COPY)
        {
            CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_APPLICATION_ID,
                                  LW90B5_SET_APPLICATION_ID_ID_NORMAL));
        }

        m_Semaphore.Setup(NotifySemaphore::ONE_WORD,
                          Memory::Coherent, numSubdevices);
        for (UINT32 subdev = 0; subdev < numSubdevices; ++subdev)
        {
            CHECK_RC(m_Semaphore.Allocate(m_pCh, subdev));
        }
        m_Semaphore.SetTriggerPayload(0);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free any resources allocated by Alloc(), if needed
//!
/* virtual */ RC SurfaceUtils::CopyEngineSurfaceFiller::Cleanup()
{
    StickyRC firstRc;

    m_Semaphore.Free();
    if (m_DmaCopyAlloc.GetHandle())
    {
        m_DmaCopyAlloc.Free();
    }
    if (m_pCh)
    {
        firstRc = m_TestConfig.FreeChannel(m_pCh);
        m_pCh = nullptr;
        m_hCh = 0;
    }
    if (m_hVASpace)
    {
        LwRmPtr()->Free(m_hVASpace);
        m_hVASpace = 0U;
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Fill a range of the surface with a value
//!
//! This is the "important" method of this class.
//!
//! \param pSurface The surface to write
//! \param fillValue The little-endian value to write into the surface
//! \param bpp The number of bits of fillValue to use
//! \param offset The offset into the surface, relative to the first
//!     byte to the first byte after the Hidden & Extra data.
//! \param offset The number of bytes to fill, starting at offset
//!
RC SurfaceUtils::CopyEngineSurfaceFiller::FillRange
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    MASSERT(IsSupported(*pSurface, fillValue, bpp, offset, size));
    MASSERT(pSurface->GetGpuDev() == GetGpuDevice());
    const UINT32 SUBCH = LWA06F_SUBCHANNEL_COPY_ENGINE;
    const UINT32 MAX_DMA_SIZE = 0x10000000;
    const UINT32 bytesPerPixel = bpp / 8;
    RC rc;

    CHECK_RC(Alloc());
    UINT64 mappedVirtAddr = 0;
    CHECK_RC(m_Mapping.MapSurface(pSurface, &mappedVirtAddr));
    CHECK_RC(pSurface->BindGpuChannel(m_hCh));

    // Write the subdevice mask to the channel
    //
    bool didSetMask;
    CHECK_RC(SetSubdeviceMask(m_pCh, &didSetMask));
    Utility::CleanupOnReturnArg<void, RC, Channel*> cleanupSubdeviceMask(
            &ClearSubdeviceMask, didSetMask ? m_pCh : NULL);

    // Write the surface-clear methods to the channel.
    //
    // Note: This code attempts to work even if size isn't a multiple
    // of bpp, by callwlating "effBytesPerPixel" and "effValue" so
    // that the final handful of DMA operations can be as small as 1
    // byte.
    //
    UINT64 startAddr = (mappedVirtAddr +
                        pSurface->GetExtraAllocSize() +
                        offset);
    UINT64 endAddr = startAddr + size;
    UINT32 dmaSize = 0;
    for (UINT64 dmaAddr = startAddr; dmaAddr < endAddr; dmaAddr += dmaSize)
    {
        dmaSize = MAX_DMA_SIZE;
        if (dmaSize > endAddr - dmaAddr)
            dmaSize = static_cast<UINT32>(endAddr - dmaAddr);
        if (dmaSize >= bytesPerPixel)
            dmaSize = ALIGN_DOWN(dmaSize, bytesPerPixel);
        else if (dmaSize >= 4)
            dmaSize = 4;
        else if (dmaSize >= 2)
            dmaSize = 2;
        else
            dmaSize = 1;
        const UINT64 effFillValue =
            fillValue >> (((dmaAddr - startAddr) % bytesPerPixel) * 8);
        const UINT32 effBytesPerPixel = min(dmaSize, bytesPerPixel);

        CHECK_RC(m_pCh->Write(SUBCH, LW90B5_OFFSET_OUT_UPPER,
                              LwU64_HI32(dmaAddr)));
        CHECK_RC(m_pCh->Write(SUBCH, LW90B5_OFFSET_OUT_LOWER,
                              LwU64_LO32(dmaAddr)));
        CHECK_RC(m_pCh->Write(SUBCH, LW90B5_LINE_LENGTH_IN,
                              dmaSize / effBytesPerPixel));
        CHECK_RC(m_pCh->Write(SUBCH, LW90B5_LINE_COUNT, 1));
        switch (effBytesPerPixel)
        {
            case 1:
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_A,
                                      LwU64_LO32(effFillValue) & 0xff));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_COMPONENTS,
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_X,_CONST_A) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_COMPONENT_SIZE,_ONE) |
                    DRF_DEF(90B5, _SET_REMAP_COMPONENTS,
                            _NUM_DST_COMPONENTS, _ONE)));
                break;
            case 2:
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_A,
                                      LwU64_LO32(effFillValue) & 0xff));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_B,
                                      LwU64_LO32(effFillValue >> 8) & 0xff));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_COMPONENTS,
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_X,_CONST_A) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_Y,_CONST_B) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_COMPONENT_SIZE,_ONE) |
                    DRF_DEF(90B5, _SET_REMAP_COMPONENTS,
                            _NUM_DST_COMPONENTS, _TWO)));
                break;
            case 4:
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_A,
                                      LwU64_LO32(effFillValue)));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_COMPONENTS,
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_X,_CONST_A) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_COMPONENT_SIZE,_FOUR) |
                    DRF_DEF(90B5, _SET_REMAP_COMPONENTS,
                            _NUM_DST_COMPONENTS, _ONE)));
                break;
            case 8:
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_A,
                                      LwU64_LO32(effFillValue)));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_CONST_B,
                                      LwU64_HI32(effFillValue)));
                CHECK_RC(m_pCh->Write(SUBCH, LW90B5_SET_REMAP_COMPONENTS,
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_X,_CONST_A) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_DST_Y,_CONST_B) |
                    DRF_DEF(90B5,_SET_REMAP_COMPONENTS,_COMPONENT_SIZE,_FOUR) |
                    DRF_DEF(90B5, _SET_REMAP_COMPONENTS,
                            _NUM_DST_COMPONENTS, _TWO)));
                break;
            default:
                MASSERT(!"Illegal bytesPerPixel");
                break;
        }

        CHECK_RC(WriteSemaphore(&m_Semaphore, m_pCh, SUBCH, LW90B5_SET_SEMAPHORE_A));
        CHECK_RC(m_pCh->Write(SUBCH, LW90B5_LAUNCH_DMA,
            DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
            DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
            DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE) |
            DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                    _RELEASE_ONE_WORD_SEMAPHORE)));
    }

    // Clear the subdevice mask, and wait for the GPU to finish
    //
    CHECK_RC(ClearSubdeviceMask(didSetMask ? m_pCh : NULL));
    cleanupSubdeviceMask.Cancel();
    CHECK_RC(m_pCh->Flush());
    if (GetAutoWait())
    {
        CHECK_RC(Wait());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Wait for FillRange() to finish
//!
RC SurfaceUtils::CopyEngineSurfaceFiller::Wait()
{
    RC rc;
    if (m_pCh)
    {
        CHECK_RC(WaitSemaphore(&m_Semaphore, m_pCh, m_TestConfig.TimeoutMs()));
        m_Mapping.ReleaseMappings();
    }
    return rc;
}

//--------------------------------------------------------------------
SurfaceUtils::ThreeDSurfaceFiller::ThreeDSurfaceFiller(GpuDevice *pGpuDevice) :
    ISurfaceFiller(pGpuDevice),
    m_pCh(nullptr),
    m_hCh(0),
    m_hVASpace(0U)
{
}

//--------------------------------------------------------------------
//! Tells if the indicated FillRange() parameters are supported in
//! this subclass.
//!
//! 3D is a bit picky.  The CLEAR method used by this class only
//! supports color surfaces (IMAGE & TEXTURE), we have to obey the PTE
//! kind as described in ResizeFillValue(), and data has to be
//! GOB-aligned.
//!
/* static */ bool SurfaceUtils::ThreeDSurfaceFiller::IsSupported
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    GpuDevice *pGpuDevice = surface.GetGpuDev();
    const Surface2D::Layout layout = surface.GetLayout();
    const UINT32 type = surface.GetType();
    const UINT32 gobSize = pGpuDevice->GobWidth() * pGpuDevice->GobHeight();
    const UINT64 unfilledPrefixSize = (surface.GetHiddenAllocSize() +
                                       surface.GetExtraAllocSize());
    ThreeDAlloc tmpThreeDAlloc;
    tmpThreeDAlloc.SetOldestSupportedClass(KEPLER_A);

    if (ResizeFillValue(surface, &fillValue, &bpp) != OK)
    {
        return false;
    }

    return
        ISurfaceFiller::IsSupported(surface, fillValue, bpp, offset, size) &&
        (bpp == 8 || bpp == 16 || bpp == 32 || bpp == 64) &&
        (size % (bpp/8) == 0) &&
        (layout == Surface2D::Pitch || layout == Surface2D::BlockLinear) &&
        (type == LWOS32_TYPE_IMAGE || type == LWOS32_TYPE_TEXTURE) &&
        unfilledPrefixSize % gobSize == 0 &&
        offset % gobSize == 0 &&
        size % gobSize == 0 &&
        tmpThreeDAlloc.IsSupported(pGpuDevice) &&
        s_Allow3DFill;
}

//--------------------------------------------------------------------
//! Tells if this surface supports ZBC
//!
//! Lwrrently, we support ZBC only for 0x00 and 0xff fills, even
//! though the ZBC table is capable of holding other values.  This is
//! partially to save space in the ZBC table (which holds 15 values,
//! tops), partially because other values aren't portable across color
//! formats and PTE kinds, and partially because those are the only
//! two fill values we tend to use anyways.
//!
/* static */ bool SurfaceUtils::ThreeDSurfaceFiller::SupportsZbc
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    GpuDevice *pGpuDevice = surface.GetGpuDev();
    LwRmPtr pLwRm;
    if (ResizeFillValue(surface, &fillValue, &bpp) != OK)
    {
        return false;
    }
    const UINT64 bppMask = _UINT64_MAX >> (64-bpp);

    return
        IsSupported(surface, fillValue, bpp, offset, size) &&
        surface.GetLayout() == Surface2D::BlockLinear &&
        ((fillValue & bppMask) == 0 || (fillValue & bppMask) == bppMask) &&
        surface.GetCompressed() &&
        surface.GetZbcMode() != Surface2D::ZbcOff &&
        pLwRm->IsClassSupported(GF100_ZBC_CLEAR, pGpuDevice);
}

//--------------------------------------------------------------------
//! \brief Allocate the channel & objects used by this class, if needed
//!
RC SurfaceUtils::ThreeDSurfaceFiller::Alloc()
{
    const UINT32 SUBCH = LWA06F_SUBCHANNEL_3D;
    GpuDevice *pGpuDevice = GetGpuDevice();
    UINT32 numSubdevices = pGpuDevice->GetNumSubdevices();
    RC rc;

    if (!m_hVASpace)
    {
        LW_VASPACE_ALLOCATION_PARAMETERS params = { 0 };
        LwRmPtr pLwRm;
        CHECK_RC(pLwRm->Alloc(
                    pLwRm->GetDeviceHandle(pGpuDevice),
                    &m_hVASpace,
                    FERMI_VASPACE_A,
                    &params));
        m_Mapping.SetVASpace(m_hVASpace);
    }

    if (!m_pCh)
    {
        m_TestConfig.BindGpuDevice(pGpuDevice);
        CHECK_RC(m_TestConfig.InitFromJs());

        m_ThreeDAlloc.SetOldestSupportedClass(KEPLER_A);
        CHECK_RC(m_TestConfig.AllocateChannelGr(&m_pCh,
                                                &m_hCh,
                                                nullptr,
                                                m_hVASpace,
                                                &m_ThreeDAlloc,
                                                0,
                                                LW2080_ENGINE_TYPE_ALLENGINES));
        CHECK_RC(m_pCh->SetObject(SUBCH, m_ThreeDAlloc.GetHandle()));

        m_Semaphore.Setup(NotifySemaphore::ONE_WORD,
                          Memory::Coherent, numSubdevices);
        for (UINT32 subdev = 0; subdev < numSubdevices; ++subdev)
        {
            CHECK_RC(m_Semaphore.Allocate(m_pCh, subdev));
        }
        m_Semaphore.SetTriggerPayload(0);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free any resources allocated by Alloc(), if needed
//!
/* virtual */ RC SurfaceUtils::ThreeDSurfaceFiller::Cleanup()
{
    StickyRC firstRc;

    m_Semaphore.Free();
    if (m_ThreeDAlloc.GetHandle())
    {
        m_ThreeDAlloc.Free();
    }
    if (m_pCh)
    {
        firstRc = m_TestConfig.FreeChannel(m_pCh);
        m_pCh = nullptr;
        m_hCh = 0;
    }
    if (m_hVASpace)
    {
        LwRmPtr()->Free(m_hVASpace);
        m_hVASpace = 0U;
    }
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Fill a range of the surface with a value
//!
//! This is the "important" method of this class.
//!
//! \param pSurface The surface to write
//! \param fillValue The little-endian value to write into the surface
//! \param bpp The number of bits of fillValue to use
//! \param offset The offset into the surface, relative to the first
//!     byte to the first byte after the Hidden & Extra data.
//! \param offset The number of bytes to fill, starting at offset
//!
/* virtual */ RC SurfaceUtils::ThreeDSurfaceFiller::FillRange
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    GpuDevice *pGpuDevice = GetGpuDevice();
    LwRmPtr pLwRm;
    RC rc;
    CHECK_RC(ResizeFillValue(*pSurface, &fillValue, &bpp));
    MASSERT(IsSupported(*pSurface, fillValue, bpp, offset, size));
    MASSERT(pSurface->GetGpuDev() == pGpuDevice);

    const UINT32 MAX_WIDTH = 0x8000;
    const UINT32 MAX_HEIGHT = 0x8000;
    const UINT32 SUBCH = LWA06F_SUBCHANNEL_3D;
    const bool isPitch = (pSurface->GetLayout() == Surface2D::Pitch);
    const UINT32 bytesPerPixel = bpp / 8;

    // Callwlate the color
    //
    cARGB color = {{0}};
    UINT32 zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ILWALID;
    UINT32 lw9097Format = LWA097_SET_COLOR_TARGET_FORMAT_V_DISABLED;
    UINT32 lw9097Colors = 0;
    switch (bpp)
    {
        case 8:
            color.red.f   = ((fillValue >>  0) & 0xff) / 255.0f;
            color.green.f = color.red.f;
            color.blue.f  = color.red.f;
            color.alpha.f = color.red.f;
            color.cout[0].ui =
                static_cast<UINT32>((fillValue & 0xff) * 0x01010101);
            zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8;
            lw9097Format = LWA097_SET_COLOR_TARGET_FORMAT_V_R8;
            lw9097Colors = DRF_DEF(A097, _CLEAR_SURFACE, _R_ENABLE, _TRUE);
            break;
        case 16:
            color.red.f   = ((fillValue >>  0) & 0xff) / 255.0f;
            color.green.f = ((fillValue >>  8) & 0xff) / 255.0f;
            color.blue.f  = color.red.f;
            color.alpha.f = color.green.f;
            color.cout[0].ui =
                static_cast<UINT32>((fillValue & 0xffff) * 0x00010001);
            zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8;
            lw9097Format = LWA097_SET_COLOR_TARGET_FORMAT_V_G8R8;
            lw9097Colors = (DRF_DEF(A097, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _G_ENABLE, _TRUE));
            break;
        case 32:
            color.red.f   = ((fillValue >>  0) & 0xff) / 255.0f;
            color.green.f = ((fillValue >>  8) & 0xff) / 255.0f;
            color.blue.f  = ((fillValue >> 16) & 0xff) / 255.0f;
            color.alpha.f = ((fillValue >> 24) & 0xff) / 255.0f;
            color.cout[0].ui = LwU64_LO32(fillValue);
            zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8;
            lw9097Format = LWA097_SET_COLOR_TARGET_FORMAT_V_A8B8G8R8;
            lw9097Colors = (DRF_DEF(A097, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _G_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _B_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _A_ENABLE, _TRUE));
            break;
        case 64:
            color.red.f   = ((fillValue >>  0) & 0xffff) / 65535.0f;
            color.green.f = ((fillValue >> 16) & 0xffff) / 65535.0f;
            color.blue.f  = ((fillValue >> 32) & 0xffff) / 65535.0f;
            color.alpha.f = ((fillValue >> 48) & 0xffff) / 65535.0f;
            color.cout[0].ui = LwU64_LO32(fillValue);
            color.cout[1].ui = LwU64_HI32(fillValue);
            zbcFormat =
                LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_R16_G16_B16_A16;
            lw9097Format = LWA097_SET_COLOR_TARGET_FORMAT_V_R16_G16_B16_A16;
            lw9097Colors = (DRF_DEF(A097, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _G_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _B_ENABLE, _TRUE) |
                            DRF_DEF(A097, _CLEAR_SURFACE, _A_ENABLE, _TRUE));
            break;
        default:
            MASSERT(!"Illegal bpp; should have been prevented by IsSupported");
    }

    if (!SupportsZbc(*pSurface, fillValue, bpp, offset, size))
    {
        zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ILWALID;
    }

    if (zbcFormat != LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ILWALID)
    {
        const UINT64 bppMask = _UINT64_MAX >> (64-bpp);
        if ((fillValue & bppMask) == 0)
        {
            memset(&color, 0, sizeof(color));
            zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ZERO;
        }
        else if ((fillValue & bppMask) == bppMask)
        {
            color.red.f   = 1.0f;
            color.green.f = 1.0f;
            color.blue.f  = 1.0f;
            color.alpha.f = 1.0f;
            color.cout[0].ui = 0xffffffff;
            color.cout[1].ui = 0xffffffff;
            color.cout[2].ui = 0xffffffff;
            color.cout[3].ui = 0xffffffff;
            zbcFormat = LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_UNORM_ONE;
        }
    }

    // Allocate the channel and set up the ZBC tables
    //
    CHECK_RC(Alloc());
    UINT64 mappedVirtAddr = 0;
    CHECK_RC(m_Mapping.MapSurface(pSurface, &mappedVirtAddr));
    CHECK_RC(pSurface->BindGpuChannel(m_hCh));
    if (zbcFormat != LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ILWALID)
    {
        const UINT32 subdevStart = GetTargetSubdevice() ?
            GetTargetSubdevice()->GetSubdeviceInst() : 0;
        const UINT32 subdevEnd = GetTargetSubdevice() ?
            subdevStart + 1 : pGpuDevice->GetNumSubdevices();
        for (UINT32 subdev = subdevStart; subdev < subdevEnd; ++subdev)
        {
            LwRm::Handle hZbc = pGpuDevice->GetSubdevice(subdev)->GetZbc();
            if (hZbc)
            {
                LW9096_CTRL_SET_ZBC_COLOR_CLEAR_PARAMS params = {{0}};
                params.colorFB[0] = color.cout[0].ui;
                params.colorFB[1] = color.cout[1].ui;
                params.colorFB[2] = color.cout[2].ui;
                params.colorFB[3] = color.cout[3].ui;
                params.colorDS[0] = color.red.ui;
                params.colorDS[1] = color.green.ui;
                params.colorDS[2] = color.blue.ui;
                params.colorDS[3] = color.alpha.ui;
                params.format = zbcFormat;
                CHECK_RC(pLwRm->Control(hZbc,
                                        LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR,
                                        &params, sizeof(params)));
            }
        }
    }

    // Write the subdevice mask to the channel
    //
    bool didSetMask;
    CHECK_RC(SetSubdeviceMask(m_pCh, &didSetMask));
    Utility::CleanupOnReturnArg<void, RC, Channel*> cleanupSubdeviceMask(
            &ClearSubdeviceMask, didSetMask ? m_pCh : NULL);

    // Write the surface-clear methods to the channel
    //
    UINT64 startAddr = (mappedVirtAddr +
                        pSurface->GetExtraAllocSize() +
                        offset);
    UINT64 endAddr = startAddr + size;
    UINT32 dmaWidth = 0;
    UINT32 dmaHeight = 0;
    for (UINT64 dmaAddr = startAddr; dmaAddr < endAddr;
         dmaAddr += static_cast<UINT64>(dmaWidth) * dmaHeight * bytesPerPixel)
    {
        UINT32 maxDmaPixels = MAX_WIDTH * MAX_HEIGHT;
        if (maxDmaPixels > (endAddr - dmaAddr) / bytesPerPixel)
        {
            maxDmaPixels = static_cast<UINT32>((endAddr - dmaAddr) /
                                               bytesPerPixel);
        }
        if (isPitch)
        {
            dmaWidth = min(MAX_WIDTH, maxDmaPixels);
            dmaHeight = min(MAX_HEIGHT, maxDmaPixels / dmaWidth);
        }
        else
        {
            UINT32 gobHeight = pGpuDevice->GobHeight();
            dmaWidth = min(MAX_WIDTH, maxDmaPixels / gobHeight);
            dmaHeight = ALIGN_DOWN(maxDmaPixels / dmaWidth, gobHeight);
            MASSERT((dmaWidth * bytesPerPixel) % pGpuDevice->GobWidth() == 0);
        }
        MASSERT(dmaWidth * dmaHeight > 0);

        // Write the methods
        //
        CHECK_RC(m_pCh->Write(SUBCH, LWA097_SET_SURFACE_CLIP_HORIZONTAL, 2,
            // SET_SURFACE_CLIP_HORIZONTAL
            DRF_NUM(A097, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, dmaWidth),
            // SET_SURFACE_CLIP_VERTICAL
            DRF_NUM(A097, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, dmaHeight)));
        CHECK_RC(m_pCh->Write(SUBCH, LWA097_SET_COLOR_TARGET_A(0), 6,
            // SET_COLOR_TARGET_A
            LwU64_HI32(dmaAddr),
            // SET_COLOR_TARGET_B
            LwU64_LO32(dmaAddr),
            // SET_COLOR_TARGET_WIDTH
            static_cast<UINT32>(isPitch ? dmaWidth*bytesPerPixel : dmaWidth),
            // SET_COLOR_TARGET_HEIGHT
            static_cast<UINT32>(dmaHeight),
            // SET_COLOR_TARGET_FORMAT
            lw9097Format,
            // SET_COLOR_TARGET_MEMORY
            isPitch ?
                DRF_DEF(A097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _PITCH) :
                DRF_DEF(A097, _SET_COLOR_TARGET_MEMORY, _LAYOUT, _BLOCKLINEAR)));
        CHECK_RC(m_pCh->Write(SUBCH, LWA097_SET_COLOR_CLEAR_VALUE(0), 4,
            color.red.ui,
            color.green.ui,
            color.blue.ui,
            color.alpha.ui));
        CHECK_RC(m_pCh->WriteImmediate(SUBCH, LWA097_SET_CLEAR_SURFACE_CONTROL, 0));
        CHECK_RC(m_pCh->WriteImmediate(SUBCH, LWA097_CLEAR_SURFACE, lw9097Colors));
    }

    CHECK_RC(WriteSemaphore(&m_Semaphore, m_pCh, SUBCH, LWA097_SET_REPORT_SEMAPHORE_A));
    CHECK_RC(m_pCh->Write(SUBCH, LWA097_SET_REPORT_SEMAPHORE_D,
        DRF_DEF(A097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
        DRF_DEF(A097, _SET_REPORT_SEMAPHORE_D, _RELEASE,
                _AFTER_ALL_PRECEEDING_WRITES_COMPLETE) |
        DRF_DEF(A097, _SET_REPORT_SEMAPHORE_D, _PIPELINE_LOCATION, _ALL) |
        DRF_DEF(A097, _SET_REPORT_SEMAPHORE_D, _STRUCTURE_SIZE, _ONE_WORD)));

    // Clear the subdevice mask, and wait for the GPU to finish
    //
    CHECK_RC(ClearSubdeviceMask(didSetMask ? m_pCh : NULL));
    cleanupSubdeviceMask.Cancel();
    CHECK_RC(m_pCh->Flush());
    if (GetAutoWait())
    {
        CHECK_RC(Wait());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Wait for FillRange() to finish
//!
RC SurfaceUtils::ThreeDSurfaceFiller::Wait()
{
    RC rc;
    if (m_pCh)
    {
        CHECK_RC(WaitSemaphore(&m_Semaphore, m_pCh, m_TestConfig.TimeoutMs()));
        m_Mapping.ReleaseMappings();
    }
    return rc;
}

//--------------------------------------------------------------------
//! Tells if the indicated FillRange() parameters are supported in this subclass
//!
/* static */ bool SurfaceUtils::OptimalSurfaceFiller::IsSupported
(
    const Surface2D &surface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    return (MappingSurfaceFiller::IsSupported(surface, fillValue,
                                              bpp, offset, size) ||
            CopyEngineSurfaceFiller::IsSupported(surface, fillValue,
                                                 bpp, offset, size) ||
            ThreeDSurfaceFiller::IsSupported(surface, fillValue,
                                             bpp, offset, size));
}

//--------------------------------------------------------------------
//! \brief Fill a range of the surface with a value
//!
//! This is the "important" method of this class.
//!
//! \param pSurface The surface to write
//! \param fillValue The little-endian value to write into the surface
//! \param bpp The number of bits of fillValue to use
//! \param offset The offset into the surface, relative to the first
//!     byte to the first byte after the Hidden & Extra data.
//! \param offset The number of bytes to fill, starting at offset
//!
/* virtual */ RC SurfaceUtils::OptimalSurfaceFiller::FillRange
(
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    UINT64 offset,
    UINT64 size
)
{
    ISurfaceFiller *pFiller = nullptr;
    RC rc;

    if (ThreeDSurfaceFiller::IsSupported(*pSurface, fillValue, bpp,
                                         offset, size))
    {
        if (m_pThreeDFiller.get() == nullptr)
        {
            m_pThreeDFiller.reset(new ThreeDSurfaceFiller(GetGpuDevice()));
        }
        pFiller = m_pThreeDFiller.get();
    }
    else if (CopyEngineSurfaceFiller::IsSupported(*pSurface, fillValue, bpp,
                                                  offset, size))
    {
        if (m_pCopyEngineFiller.get() == nullptr)
        {
            m_pCopyEngineFiller.reset(
                    new CopyEngineSurfaceFiller(GetGpuDevice()));
        }
        pFiller = m_pCopyEngineFiller.get();
    }
    else
    {
        if (GetTargetSubdevice()->HasFeature(
                    Device::GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK) &&
                pSurface->GetLayout() == Surface2D::BlockLinear)
        {
            Printf(Tee::PriError,
               "CPU fill of BlockLinear surfaces on P9 is not possible \n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }

        if (m_pMappingFiller.get() == nullptr)
        {
            m_pMappingFiller.reset(new MappingSurfaceFiller(GetGpuDevice()));
        }
        pFiller = m_pMappingFiller.get();
    }

    CHECK_RC(pFiller->SetTargetSubdevice(GetTargetSubdevice()));
    pFiller->SetAutoWait(GetAutoWait());
    CHECK_RC(pFiller->FillRange(pSurface, fillValue, bpp, offset, size));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Wait for FillRange() to finish
//!
/* virtual */ RC SurfaceUtils::OptimalSurfaceFiller::Wait()
{
    RC rc;
    if (m_pMappingFiller.get())
        CHECK_RC(m_pMappingFiller->Wait());
    if (m_pCopyEngineFiller.get())
        CHECK_RC(m_pCopyEngineFiller->Wait());
    if (m_pThreeDFiller.get())
        CHECK_RC(m_pThreeDFiller->Wait());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free any resources allocated by this object
//!
/* virtual */ RC SurfaceUtils::OptimalSurfaceFiller::Cleanup()
{
    StickyRC firstRc;
    if (m_pMappingFiller.get())
    {
        firstRc = m_pMappingFiller->Cleanup();
        m_pMappingFiller.reset();
    }
    if (m_pCopyEngineFiller.get())
    {
        firstRc = m_pCopyEngineFiller->Cleanup();
        m_pCopyEngineFiller.reset();
    }
    if (m_pThreeDFiller.get())
    {
        firstRc = m_pThreeDFiller->Cleanup();
        m_pThreeDFiller.reset();
    }
    return firstRc;
}

//------------------------------------------------------------------------------
// SurfaceHandler: The abstract base class to abstract the DMA/Non-DMA differences
//                 during surface handling
//------------------------------------------------------------------------------
SurfaceUtils::SurfaceFill::SurfaceFill(bool useMemoryFillFns)
: m_UseMemoryFillFns(useMemoryFillFns)
{

}

RC SurfaceUtils::SurfaceFill::Setup(GpuTest *gpuTest, GpuTestConfiguration *testConfig)
{
    RC rc;

    m_pGpuDev = gpuTest->GetBoundGpuDevice();
    m_GpuSubDevInstance = gpuTest->GetBoundGpuSubdevice()->GetSubdeviceInst();
    m_pTstCfg = testConfig;

    CHECK_RC(InitPatternInfoMap());

    return rc;
}

RC SurfaceUtils::SurfaceFill::Setup(GpuSubdevice *pGpuSubDev, GpuTestConfiguration *testConfig)
{
    RC rc;

    m_pGpuDev = pGpuSubDev->GetParentDevice();
    m_GpuSubDevInstance = pGpuSubDev->GetSubdeviceInst();
    if (m_UseMemoryFillFns && !testConfig)
    {
        Printf(Tee::PriError, "Incorrect setup of SurfaceFill\n");
        return RC::SOFTWARE_ERROR;
    }
    m_pTstCfg = testConfig;

    CHECK_RC(InitPatternInfoMap());

    return rc;
}

RC SurfaceUtils::SurfaceFill::InitPatternInfoMap()
{
    constexpr char ImageHorizontalBars[] = "horiz_bars";
    constexpr char ImageVerticalBars[] = "vert_bars";
    constexpr char ImageHorizontalStripes[] = "horiz_stripes";
    constexpr char ImageDiagStripes[] = "diag_stripes";
    constexpr char ImageFpGray[] = "fp_gray";
    constexpr char ImageDefault[] = "default";
    constexpr char ImageRandom[] = "random";

    PatternInfo patternInfoHorizontalBars("gradient", "multiple", "horizontal");
    PatternInfo patternInfoVerticalMultiCscBars("bars", "multicsc2", "vertical");
    PatternInfo patternInfoHorizontalStripes("special", "dfpbars", "na");
    PatternInfo patternInfoDiagonalStripes("gradient", "mfgdiag", "horizontal");
    PatternInfo patternInfoFpGray("fp_gray", "na", "na");
    PatternInfo patternInfoDefault = patternInfoHorizontalBars;

    m_PatternInfoMap[ImageHorizontalBars] = patternInfoHorizontalBars;
    m_PatternInfoMap[ImageVerticalBars] = patternInfoVerticalMultiCscBars;
    m_PatternInfoMap[ImageHorizontalStripes] = patternInfoHorizontalStripes;
    m_PatternInfoMap[ImageDiagStripes] = patternInfoDiagonalStripes;
    m_PatternInfoMap[ImageFpGray] = patternInfoFpGray;
    m_PatternInfoMap[ImageDefault] = patternInfoDefault;

    if (m_pTstCfg)
    {
        string seedStr = Utility::StrPrintf("seed=%u", m_pTstCfg->Seed());
        PatternInfo patternInfoRandom("random", seedStr, "na");
        m_PatternInfoMap[ImageRandom] = patternInfoRandom;
    }
    return OK;
}

RC SurfaceUtils::SurfaceFill::AllocSurface
(
    const Display::Mode& resolutionMode,
    Memory::Location location,
    ColorUtils::Format colorFormat,
    bool isDisplayable,
    unique_ptr<Surface2D> *ppSurface
)
{
    RC rc;
    unique_ptr<Surface2D> surface (new Surface2D());
    UINT32 width = resolutionMode.width;
    UINT32 height = resolutionMode.height;

    if (colorFormat == ColorUtils::LWFMT_NONE)
    {
        surface->SetColorFormat(ColorUtils::ColorDepthToFormat(resolutionMode.depth));
    }
    else
    {
        surface->SetColorFormat(colorFormat);
    }
    surface->SetPlanesColorFormat(m_PlanesFormat);
    if (surface->IsSurface422Compressed())
    {
        width /= 2;
    }
    else if (surface->IsSurface420Compressed())
    {
        width /= 2;
        height /= 2;
    }
    surface->SetWidth(width);
    surface->SetHeight(height);
    surface->SetType(m_Type);
    surface->SetLocation(location);
    surface->SetLayout(m_Layout);

    if (m_pGpuDev->GetSubdevice(0)->IsSOC() &&
        m_pGpuDev->GetDisplay()->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        surface->SetLocation(Memory::NonCoherent);
        surface->SetRouteDispRM(true);
        surface->SetDisplayable(isDisplayable);
    }
    else if (location == Memory::Fb || location == Memory::Optimal)
    {
        surface->SetDisplayable(isDisplayable);
        surface->SetLogBlockWidth(m_LogBlockWidth);
        surface->SetLogBlockHeight(m_LogBlockHeight);
    }
    else
    {
        surface->SetLayout(Surface2D::Pitch);
    }
    surface->SetYCrCbType(m_YCrCbType);

    CHECK_RC(surface->Alloc(m_pGpuDev));

    *ppSurface = std::move(surface);

    return rc;
}

bool SurfaceUtils::SurfaceFill::IsSupportedImageFile(const string& fileName)
{
    if (!Xp::DoesFileExist(fileName))
        return false;
    if (fileName.find(".tga") != std::string::npos)
        return true;
    if (fileName.find(".raw") != std::string::npos)
        return true;
    if (fileName.find(".hdr") != std::string::npos)
        return true;
    if (fileName.find(".png") != std::string::npos)
        return true;
    return false;
}
bool SurfaceUtils::SurfaceFill::IsSupportedImage(const string& imageName)
{
    auto it = m_PatternInfoMap.find(imageName);

    return (it != m_PatternInfoMap.end() ? true : false);
}

namespace SurfaceUtils
{
namespace FramePatterns
{
    /**
     * \class IsValidBpp
     * \brief Compile time check to see if an integer can be casted into SupportedBpp type
     */
    template <size_t N>
    struct IsValidBpp
    {
        enum
        {
            v = (N) && !(N & (N-1)) && (N <= SupportedBpp::BPP_16)
        };
        static_assert(v, "BPP are power of 2 and maximum supported value is 16");
    };
    /**
     * \class Pixel
     * \brief Represents Pixel of array width BPP bytes
     *
     * All pixels support bitwise unary operations binary operations. The class
     * should support any kind of colour format in single colour plane
     * i.e.RGB plane or Y plane, U plane pixel. This class can avoid typecasting
     * 64 bit pixels into 32 bit pixel kinds.
     */
    template <size_t BPP, int dummy = IsValidBpp<BPP>::v>
    struct Pixel
    {
    public:
        template<typename... T>
            Pixel(T... vals) : m_Val{ vals... }
        {
        }
        explicit Pixel(const Pixel<BPP> &inPixel)
        {
            memcpy(&m_Val, &(inPixel.m_Val), BPP);
        }

        constexpr size_t GetBytesPerPixel() const
        {
            return BPP;
        }

        Byte& operator [] (UINT32 index)
        {
            return m_Val[index];
        }
        bool operator == (Pixel<BPP> rhs)
        {
            for (int i = 0; i < BPP; i++)
            {
                if (m_Val[i] != rhs[i])
                    return false;
            }
            return true;
        }
        Pixel<BPP> operator >> (UINT32 rshift)
        {
            Pixel<BPP> retVal;
            Byte carry = 0;
            for (int i = BPP - 1; i <= 0; i--)
            {
                retVal[i] = carry | (m_Val[i] >> rshift);
                if (m_Val[i] & 1)
                {
                    carry = 1 << 7;
                }
            }
            return retVal;
        }
        Pixel<BPP> operator << (UINT32 lshift)
        {
            Pixel<BPP> retVal;
            Byte carry = 0;
            for (int i = 0; i < BPP; i++)
            {
                retVal[i] = carry | (m_Val[i] << lshift);
                if (m_Val[i] & (1 << 7))
                {
                    carry = 1;
                }
            }
            return retVal;
        }
        Pixel<BPP> operator ~ ()
        {
            Pixel<BPP> retVal;
            for (int i = 0; i < BPP; i++)
            {
                retVal[i] = ~m_Val[i];
            }
            return retVal;
        }
#define BINARYOP(op) \
        Pixel<BPP> operator op(const Pixel<BPP> &rhs) const\
        {\
            Pixel<BPP> retVal;\
            for (int i = 0; i < BPP; i++)\
            {\
                retVal[i] = m_Val[i] op rhs.m_Val[i];\
            }\
            return retVal;\
        }
        BINARYOP(|)
        BINARYOP(&)
        BINARYOP(^)
private:
        Byte m_Val[BPP] = { 0 };
    };

    template <size_t BPP> using PixelRow = vector<Pixel<BPP>>;
    template <size_t BPP> using PixelBlock = vector<PixelRow<BPP>>;

    /**
     * \class Property
     *
     * \brief Generic Properties associated with Pixel
     *
     *  This class is specialized into RowType(index and size) and column (index and height)
     */
    template <typename T, typename Tag>
    struct Property
    {
        public:
            explicit Property(T const &val): m_Val(val){}
            explicit Property(const Property<T, Tag> &rhs)
                : m_Val(rhs.Get())
            {
            }
            T Get() { return m_Val; }
            T const& Get() const { return m_Val; }

        private:
            T m_Val;
    };

    struct WidthType{};
    struct HeightType{};


    template <typename DimType, typename DimUnit = void>
    using Dim = Property<UINT32, DimType>;

    template <size_t Bpp>
    using WidthPixel = Dim<WidthType, Pixel<Bpp>>;

    using Height = Dim<HeightType, Byte>;

    /**
     * \function CopyRow
     *
     * \brief Copy a row of pixels into a given 'SurfaceMap' at a particular Row offset
     *
     */
    RC CopyRow
    (
        const void *pData,
        const UINT32 &bpp,
        const UINT32 destRowId,
        const UINT32 numPixels,
        SurfaceUtils::SurfaceMapper &mapper
    )
    {
        RC rc;
        const UINT32 copySize = numPixels * bpp;
        const UINT64 rowStartOffset = copySize * destRowId;
        CHECK_RC(mapper.Remap(rowStartOffset, SurfaceUtils::SurfaceMapper::WriteOnly));
        UINT08 *pDest = (mapper.GetMappedAddress() + rowStartOffset - mapper.GetMappedOffset());
        Platform::VirtualWr(pDest, pData, static_cast<UINT32>(copySize));
        return rc;
    }

    /**
     * \function GenPixel
     *
     * \brief Implementation for Random kind Pixel generation.
     */
    template<size_t BPP>
    RC GenPixel
    (
        const UINT32 seed,
        Pixel<BPP> &outPixel,
        PatRand *pPattern
    )
    {
        RC rc;
        const UINT32 maxLimit = std::numeric_limits<SurfaceUtils::FramePatterns::Byte>::max();
        const UINT32 minLimit = 0;
        Random rand;
        rand.SeedRandom(seed);
        INT32 i = 0;
        while (i < BPP)
        {
            outPixel[i++] = static_cast<Byte>(rand.GetRandom(minLimit, maxLimit));
        }
        return rc;
    }

    /**
     * \function GenPixel
     *
     * \brief Implementation for Random-DSC kind Pixel generation.
     */
    template<size_t BPP>
    RC GenPixel
    (
        const UINT32 seed,
        Pixel<BPP> &outPixel,
        PatRandDSC *pPattern
    )
    {
        const UINT32 maxLimit = std::numeric_limits<SurfaceUtils::FramePatterns::Byte>::max();
        const UINT32 minLimit = 0;
        Random rand;
        rand.SeedRandom(seed);
        UINT32 i = 0;
        while (i < BPP)
        {
            outPixel[i++] = static_cast<Byte>(rand.GetRandom(minLimit, maxLimit));
        }
        return RC::OK;
    }

    /**
     * \function GenPixelRow
     *
     * \brief Generate a single Row of Pixel based on the Pattern type.
     *
     * \param wdPixels    [in] Number of pixels contained in one row.
     * \param outPixelRow [out] Reference to the PixelRow object that needs generation.
     * \return RC
     */
    template<size_t BPP, typename T>
    RC GenPixelRow
    (
        const UINT32 seed,
        const WidthPixel<BPP> &wdPixels,
        PixelRow<BPP> &outPixelRow,
        T* pPatType
    )
    {
        RC rc;
        outPixelRow.resize(wdPixels.Get());
        // DSC refmode pattern algorithm:
        // DSC algorithm can search for same pixels to reduce the bit rate and
        // DSC pipe line is 3 pixels/cycle. We should use following stream for max stress:
        // Cycle 1:  randomize  pixel 0-2
        // Cycle 2:  toggle all the bits of pixel 0-2 to generate pixel 3-5
        // Repeat cycle 1 & 2
        //
        //TODO: JIRA MT-888: Remove template code as this logic can be implemented without it
        for (UINT32 i = 0; i < wdPixels.Get(); i++)
        {
            if ((i / 3) % 2 == 0)
            {
                CHECK_RC(GenPixel(seed + i, outPixelRow[i], pPatType));
            }
            else
            {
                for (UINT32 j = 0; j < BPP; j++)
                {
                    outPixelRow[i][j] = ~outPixelRow[i - 3][j];
                }
            }
        }
        return rc;
    }

    /**
     * \function GenPixelBlock
     *
     * \brief Generate a Block (num Column * numRow) of Pixels based on the Pattern type.
     *
     * \param wdPixels      [ in] Number of Columns in the block.
     * \param htBlock       [ in] Number of rows in the block
     * \param outPixelBlock [ out] Reference to the PixelBlock object that needs generation.
     * \return RC
     */
    template<size_t BPP, typename T>
    RC GenPixelBlock
    (
        const UINT32 seed,
        const WidthPixel<BPP> &wdPixels,
        const SurfaceUtils::FramePatterns::Height htBlock,
        PixelBlock<BPP> &outPixelBlock,
        T* pPatType
    )
    {
        RC rc;
        const size_t ht = static_cast<size_t>(htBlock.Get());
        outPixelBlock.resize(ht);
        for (UINT32 i = 0; i < static_cast<UINT32>(ht); i++)
        {
            PixelRow<BPP> &row = outPixelBlock[i];
            CHECK_RC(GenPixelRow(row, wdPixels, pPatType));
        }
        return rc;
    }
} // namespace FramePatterns
} // namespace SurfaceUtils

/**
 *\brief Fill pixels of random values in the input surface
 */
RC SurfaceUtils::SurfaceFill::FillSurfaceRandom
(
    SurfaceId surfaceId,
    FramePatterns::PatRand* pPatKind
)
{
    RC rc;
    Surface2D *pSurf = GetSurface(surfaceId);
    // TODO: Implement a meaningful Fill() for FP16 RGB surfaces that won't
    // trigger LW_PDISP_FE_EVT_STAT_ERROR_FP16 from NaN and +/- INF values
    if (pSurf->GetColorFormat() == ColorUtils::RF16_GF16_BF16_AF16)
    {
        pSurf->Fill(0x73FF73FF, m_GpuSubDevInstance);
    }
    else
    {
        string seedVal = Utility::StrPrintf("seed=%u", m_pTstCfg->Seed());
        CHECK_RC(pSurf->FillPattern(
            1, 1, "random", seedVal.c_str(), nullptr, m_GpuSubDevInstance));
    }
    return rc;
}

/**
*\brief Fill pixels of DSC stress values in the input surface
*
* This function is not using surface2d legacy implementation. This pattern
* is used by tests like Lwdisplayrandom
*
*/
RC SurfaceUtils::SurfaceFill::FillSurfaceRandom
(
    SurfaceId surfaceId,
    FramePatterns::PatRandDSC* pPatKind
)
{
    RC rc;
    using namespace FramePatterns;
    Surface2D *pSurf = GetSurface(surfaceId);
    const UINT32 ht = pSurf->GetAllocHeight();
    const UINT32 bpp = ColorUtils::PixelBytes(pSurf->GetColorFormat());
    const UINT32 columns = pSurf->GetAllocWidth();
    SurfaceUtils::SurfaceMapper mapper(*pSurf);

    // Create a SurfaceMapper to write the surface
    if (m_pGpuDev->GetNumSubdevices() > 1)
    {
        mapper.SetTargetSubdevice(m_GpuSubDevInstance, false);
    }

    auto FillPixels = [&](auto &widthPixel, auto &pixelRow) -> RC
    {
        for (UINT32 rowId = 0; rowId < ht; rowId++)
        {
            CHECK_RC(GenPixelRow(m_pTstCfg->Seed() + rowId * ht,
                        widthPixel,
                        pixelRow,
                        pPatKind));
            CHECK_RC(CopyRow(&pixelRow[0], bpp, rowId, columns, mapper));
        }
        return RC::OK;
    };

    CHECK_RC(pSurf->Map());
    DEFER
    {
        pSurf->Unmap();
    };
    switch (bpp)
    {
        case SupportedBpp::BPP_1:
        {
            const WidthPixel<SupportedBpp::BPP_1> wdPixel(columns);
            PixelRow<SupportedBpp::BPP_1> row(columns);
            CHECK_RC(FillPixels(wdPixel, row));
        }
        break;
        case SupportedBpp::BPP_2:
        {
            const WidthPixel<SupportedBpp::BPP_2> wdPixel(columns);
            PixelRow<SupportedBpp::BPP_2> row(columns);
            CHECK_RC(FillPixels(wdPixel, row));
        }
        break;
        case SupportedBpp::BPP_4:
        {
            const WidthPixel<SupportedBpp::BPP_4> wdPixel(columns);
            PixelRow<SupportedBpp::BPP_4> row(columns);
            CHECK_RC(FillPixels(wdPixel, row));
        }
        break;
        case SupportedBpp::BPP_8:
        {
            const WidthPixel<SupportedBpp::BPP_8> wdPixel(columns);
            PixelRow<SupportedBpp::BPP_8> row(columns);
            CHECK_RC(FillPixels(wdPixel, row));
        }
        break;
        case SupportedBpp::BPP_16:
        {
            const WidthPixel<SupportedBpp::BPP_16> wdPixel(columns);
            PixelRow<SupportedBpp::BPP_16> row(columns);
            CHECK_RC(FillPixels(wdPixel, row));
        }
        break;
        default:
            Printf(Tee::PriError, "Unsupported BPP_%u\n", bpp);
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC SurfaceUtils::SurfaceFill::FillPattern
(
    SurfaceId surfaceId,
    const FramePatterns::Pattern pattern
)
{
    RC rc;
    using namespace FramePatterns;
    switch (pattern)
    {
        case Pattern::RAND_FILL:
            {
                PatternType<Pattern::RAND_FILL>::type* pRandKind = nullptr;
                CHECK_RC(FillSurfaceRandom(surfaceId, pRandKind));
                break;
            }
        case Pattern::RAND_DSC_FILL:
            {
                PatternType<Pattern::RAND_DSC_FILL>::type* pDscKind = nullptr;
                CHECK_RC(FillSurfaceRandom(surfaceId, pDscKind));
                break;
            }
        default:
            Printf(Tee::PriError, ":%s: Unsupported Pattern type\n", __FUNCTION__);
            return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

RC SurfaceUtils::SurfaceFill::FillPattern
(
    SurfaceId surfaceId,
    const string& image
)
{
    RC rc;
    Surface2D *surface = GetSurface(surfaceId);
    if (IsSupportedImage(image))
    {
        if (m_UseMemoryFillFns)
        {
            CHECK_RC(surface->Map());
            DEFER
            {
                surface->Unmap();
            };
            if (image == "default")
            {
                CHECK_RC(Memory::FillRgbBars(surface->GetAddress(),
                                            surface->GetWidth(),
                                            surface->GetHeight(),
                                            surface->GetColorFormat(),
                                            surface->GetPitch()));
            }
            else if (image == "random")
            {
                Random random;
                if (m_pTstCfg->UseOldRNG())
                {
                    random.UseOld();
                }
                random.SeedRandom(m_pTstCfg->Seed());

                CHECK_RC(Memory::FillRandom(surface->GetAddress(),
                                            surface->GetSize(),
                                            random));
            }
            else
            {
                // Unsupported test pattern when using Memory::FillAPIs
                return RC::UNSUPPORTED_FUNCTION;
            }
        }
        else
        {
            CHECK_RC(surface->FillPatternRect(
                0,
                0,
                surface->GetWidth(),
                surface->GetHeight(),
                1,
                1,
                m_PatternInfoMap[image].GetType(),
                m_PatternInfoMap[image].GetStyle(),
                m_PatternInfoMap[image].GetDirection(),
                m_GpuSubDevInstance));
        }
    }
    else
    {
        CHECK_RC(surface->ReadPng(image.c_str(), m_GpuSubDevInstance));
    }

    CHECK_RC(surface->FlushCpuCache());

    return rc;
}

RC SurfaceUtils::SurfaceFill::FillPattern
(
    SurfaceId surfaceId,
    const vector<UINT32> & patternData
)
{
    RC rc;
    Surface2D *pSurf = GetSurface(surfaceId);
    CHECK_RC(pSurf->Map());

    DEFER
    {
        pSurf->Unmap();
    };

    CHECK_RC(Memory::FillSurfaceWithPattern
            (
                pSurf->GetAddress(),
                &patternData,
                pSurf->GetWidth() * pSurf->GetBytesPerPixel(),
                pSurf->GetHeight(),
                pSurf->GetPitch()
            ));
    CHECK_RC(pSurf->FlushCpuCache());
    return rc;
}

RC SurfaceUtils::SurfaceFill::FillPatternRect
(
    SurfaceId surfaceId,
    const char *patternType,
    const char *patternStyle,
    const char *patternDir
)
{
    RC rc;

    Surface2D *surface = GetSurface(surfaceId);
    if (m_UseMemoryFillFns)
    {
        CHECK_RC(surface->Map());
        DEFER
        {
            surface->Unmap();
        };
        if (!strcmp(patternType, "default"))
        {
            CHECK_RC(Memory::FillRgbBars(surface->GetAddress(),
                                        surface->GetWidth(),
                                        surface->GetHeight(),
                                        surface->GetColorFormat(),
                                        surface->GetPitch()));
        }
        else if (!strcmp(patternType, "random"))
        {
            Random random;
            if (m_pTstCfg->UseOldRNG())
            {
                random.UseOld();
            }
            random.SeedRandom(m_pTstCfg->Seed());

            CHECK_RC(Memory::FillRandom(surface->GetAddress(),
                                        surface->GetSize(),
                                        random));
        }
        else
        {
            // Unsupported test pattern when using Memory::FillAPIs
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    else
    {
        CHECK_RC(surface->FillPatternRect(
                0,
                0,
                surface->GetWidth(),
                surface->GetHeight(),
                1,
                1,
                patternType,
                patternStyle,
                patternDir,
                m_GpuSubDevInstance));
    }

    CHECK_RC(surface->FlushCpuCache());

    return rc;
}


RC SurfaceUtils::SurfaceFill::FillPatternLwstom
(
    SurfaceId srcSurfaceId,
    SurfaceId dstSurfaceId,
    const char *pFillOperation
)
{
    RC rc;

    Surface2D *pSrcSurface = GetSurface(srcSurfaceId);
    Surface2D *pDstSurface = GetSurface(dstSurfaceId);

    if (pSrcSurface->GetWidth() != pDstSurface->GetWidth() ||
            pSrcSurface->GetHeight() != pDstSurface->GetHeight())
    {
        Printf(Tee::PriError,
                "Incorrect specification for source and destination surface pair!"
                " They should have equal width and height\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(pSrcSurface->Map());
    CHECK_RC(pDstSurface->Map());
    DEFER
    {
        pSrcSurface->Unmap();
        pDstSurface->Unmap();
    };

    if (!strcmp(pFillOperation, "flipRGB"))
    {
        CHECK_RC(Memory::FlipRGB(pSrcSurface->GetAddress(),
                                 pDstSurface->GetAddress(),
                                 pSrcSurface->GetWidth(),
                                 pSrcSurface->GetHeight(),
                                 pSrcSurface->GetColorFormat(),
                                 pSrcSurface->GetPitch()));
    }
    else
    {
        Printf(Tee::PriError, "Unsupported fill operation!\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(pSrcSurface->FlushCpuCache());
    CHECK_RC(pDstSurface->FlushCpuCache());
    return rc;
}

//------------------------------------------------------------------------------
// SurfaceHandlerDMA: Concrete implementation of SurfaceHandlerBase when using DMA
//------------------------------------------------------------------------------
SurfaceUtils::SurfaceFillDMA::SurfaceFillDMA(bool useMemoryFillFns)
: SurfaceUtils::SurfaceFill(useMemoryFillFns)
{

}

RC SurfaceUtils::SurfaceFillDMA::Setup(GpuTest *gpuTest, GpuTestConfiguration *testConfig)
{
    RC rc;

    CHECK_RC(SurfaceUtils::SurfaceFill::Setup(gpuTest, testConfig));

    CHECK_RC(m_DmaWrap.Initialize(m_pTstCfg, m_pTstCfg->NotifierLocation()));

    return rc;
}

RC SurfaceUtils::SurfaceFillDMA::Cleanup()
{
    StickyRC rc;
    rc = FreeAllSurfaces();
    rc = m_DmaWrap.Cleanup();

    return rc;
}

RC SurfaceUtils::SurfaceFillDMA::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    ColorUtils::Format forcedColorFormat
)
{
    return PrepareSurface(surfaceId, resolutionMode, forcedColorFormat,
        GetYCrCbType(), GetLayout(), GetLogBlockWidth(),
        GetLogBlockHeight(), LWOS32_TYPE_IMAGE, Surface2D::BYPASS,  nullptr);
}

RC SurfaceUtils::SurfaceFillDMA::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    UINT32 surfFieldType,
    ColorUtils::Format forcedColorFormat
)
{
    return PrepareSurface(surfaceId, resolutionMode, forcedColorFormat,
        GetYCrCbType(), GetLayout(), GetLogBlockWidth(),
        GetLogBlockHeight(), surfFieldType, Surface2D::BYPASS,  nullptr);
}

RC SurfaceUtils::SurfaceFillDMA::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    ColorUtils::Format forcedColorFormat,
    ColorUtils::YCrCbType yCrCbType,
    Surface2D::Layout layout,
    UINT32 logBlockWidth,
    UINT32 logBlockHeight,
    UINT32 surfFieldType,
    Surface2D::InputRange inputRange,
    unique_ptr<Surface2D> pSystemSurface
)
{
    RC rc;
    if (m_SurfaceMap.find(surfaceId) != m_SurfaceMap.end())
    {
        Printf(Tee::PriLow, "Surface already present for surfaceId %u\n", surfaceId);
        return rc;
    }

    SetLogBlockWidth(logBlockWidth);
    SetLogBlockHeight(logBlockHeight);
    SetYCrCbType(yCrCbType);
    SetLayout(layout);
    SetInputRange(inputRange);
    SetType(surfFieldType);

    if (!pSystemSurface || !pSystemSurface->IsAllocated())
    {
        CHECK_RC(AllocSurface(resolutionMode, Memory::Coherent, forcedColorFormat,
                    false, &pSystemSurface));
    }

    unique_ptr<Surface2D> pFbSurface;
    CHECK_RC(AllocSurface(resolutionMode, Memory::Fb, forcedColorFormat, true, &pFbSurface));

    SurfaceDmaPair surfaceDmaPair(std::move(pSystemSurface), std::move(pFbSurface));
    m_SurfaceMap.insert(make_pair(surfaceId, std::move(surfaceDmaPair)));

    return OK;
}

RC SurfaceUtils::SurfaceFillDMA::Reserve(const vector<SurfaceId> &sIds)
{
    RC rc;
    //The surfaces can be attached or allocated later based on the SurfaceId key
    for (const auto &sId : sIds)
    {
        unique_ptr<Surface2D> pFbSurf(new Surface2D());
        unique_ptr<Surface2D> pSysSurf(new Surface2D());
        SurfaceDmaPair tmpPair(std::move(pSysSurf), std::move(pFbSurf));
        m_SurfaceMap.insert(make_pair(sId, std::move(tmpPair)));
    }
    return rc;
}

RC SurfaceUtils::SurfaceFillDMA::Insert(const SurfaceId sId, unique_ptr<Surface2D> pSurf)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC SurfaceUtils::SurfaceFillDMA::Insert
(
    const SurfaceId sId,
    unique_ptr<Surface2D> pFbSurf,
    shared_ptr<Surface2D> pSysSurf
)
{
    SurfaceDmaPair tmpPair(std::move(pSysSurf), std::move(pFbSurf));
    m_SurfaceMap[sId] = std::move(tmpPair);
    return OK;
}

void SurfaceUtils::SurfaceFillDMA::Erase(const SurfaceId sId)
{
    m_SurfaceMap.erase(sId);
}

Surface2D* SurfaceUtils::SurfaceFillDMA::GetSurface(SurfaceId surfaceId)
{
    return m_SurfaceMap[surfaceId].first.get();
}

Surface2D* SurfaceUtils::SurfaceFillDMA::GetImage(SurfaceId surfaceId)
{
    Surface2D* pRes = nullptr;
    auto surfItr = m_SurfaceMap.find(surfaceId);
    if (surfItr != m_SurfaceMap.end())
        pRes = surfItr->second.second.get();
    return pRes;
}
Surface2D* SurfaceUtils::SurfaceFillDMA::operator[](SurfaceId surfaceId)
{
    Surface2D* pRes = nullptr;
    auto surfItr = m_SurfaceMap.find(surfaceId);
    if (surfItr != m_SurfaceMap.end())
            pRes = surfItr->second.second.get();
    return pRes;
}

RC SurfaceUtils::SurfaceFillDMA::CopySurface(SurfaceId surfaceId)
{
    RC rc;
    Surface2D *systemSurface = GetSurface(surfaceId);
    Surface2D *fbSurface = GetImage(surfaceId);

    CHECK_RC(m_DmaWrap.SetSurfaces(systemSurface, fbSurface));
    CHECK_RC(m_DmaWrap.Copy(0, 0, systemSurface->GetPitch(), systemSurface->GetHeight(),
                    0, 0, m_pTstCfg->TimeoutMs(), 0, false));

    return rc;
}

RC SurfaceUtils::SurfaceFillDMA::FreeAllSurfaces()
{
    for (const auto& surfaceDmaPair : m_SurfaceMap)
    {
        surfaceDmaPair.second.first->Free();
        surfaceDmaPair.second.second->Free();
    }
    m_SurfaceMap.clear();
    return OK;
}
//------------------------------------------------------------------------------
// SurfaceUtils::SurfaceFillFB: Concrete implementation of SurfaceHandler when
// directly writing to FB
//------------------------------------------------------------------------------
SurfaceUtils::SurfaceFillFB::SurfaceFillFB(bool useMemoryFillFns)
: SurfaceUtils::SurfaceFill(useMemoryFillFns)
{

}

RC SurfaceUtils::SurfaceFillFB::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    ColorUtils::Format forcedColorFormat
)
{
    return PrepareSurface(surfaceId, resolutionMode, forcedColorFormat,
        GetYCrCbType(), GetLayout(), GetLogBlockWidth(),
        GetLogBlockHeight(), LWOS32_TYPE_IMAGE, Surface2D::BYPASS, nullptr);
}

RC SurfaceUtils::SurfaceFillFB::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    UINT32 surfFieldType,
    ColorUtils::Format forcedColorFormat
)
{
    return PrepareSurface(surfaceId, resolutionMode, forcedColorFormat,
        GetYCrCbType(), GetLayout(), GetLogBlockWidth(),
        GetLogBlockHeight(), surfFieldType, Surface2D::BYPASS, nullptr);
}

RC SurfaceUtils::SurfaceFillFB::PrepareSurface
(
    SurfaceId surfaceId,
    const Display::Mode& resolutionMode,
    ColorUtils::Format forcedColorFormat,
    ColorUtils::YCrCbType colorTypeYCrCb,
    Surface2D::Layout layout,
    UINT32 logBlockWidth,
    UINT32 logBlockHeight,
    UINT32 surfFieldType,
    Surface2D::InputRange inputRange,
    unique_ptr<Surface2D> pRefSysmemSurf
)
{
    RC rc;
    if (m_SurfaceMap.find(surfaceId) != m_SurfaceMap.end())
    {
        Printf(Tee::PriLow, "Surface already present for surfaceId %u\n", surfaceId);
        return rc;
    }

    SetLogBlockWidth(logBlockWidth);
    SetLogBlockHeight(logBlockHeight);
    SetYCrCbType(colorTypeYCrCb);
    SetLayout(layout);
    SetInputRange(inputRange);
    SetType(surfFieldType);

    unique_ptr<Surface2D> pFbSurface;
    CHECK_RC(AllocSurface(resolutionMode, Memory::Fb, forcedColorFormat, true, &pFbSurface));
    m_SurfaceMap.insert(make_pair(surfaceId, std::move(pFbSurface)));

    return rc;
}

RC SurfaceUtils::SurfaceFillFB::Reserve(const vector<SurfaceId> &sIds)
{
    RC rc;
    //The surfaces can be attached or allocated later based on the SurfaceId key
    for (const auto &sId : sIds)
    {
        unique_ptr<Surface2D> pFbSurf(new Surface2D());
        m_SurfaceMap.insert(make_pair(sId, std::move(pFbSurf)));
    }
    return rc;
}

RC SurfaceUtils::SurfaceFillFB::Insert(const SurfaceId sId, unique_ptr<Surface2D> pFbSurf)
{
    m_SurfaceMap[sId] = std::move(pFbSurf);
    return OK;
}

RC SurfaceUtils::SurfaceFillFB::Insert
(
    const SurfaceId sId,
    unique_ptr<Surface2D> pFbSurf,
    shared_ptr<Surface2D> pSysSurf
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void SurfaceUtils::SurfaceFillFB::Erase(const SurfaceId sId)
{
    m_SurfaceMap.erase(sId);
}

RC SurfaceUtils::SurfaceFillFB::Cleanup()
{
    return FreeAllSurfaces();
}

Surface2D* SurfaceUtils::SurfaceFillFB::GetSurface(SurfaceId surfaceId)
{
    return GetImage(surfaceId);
}

Surface2D* SurfaceUtils::SurfaceFillFB::GetImage(SurfaceId surfaceId)
{
    Surface2D* pRes = nullptr;
    auto surfItr = m_SurfaceMap.find(surfaceId);
    if (surfItr != m_SurfaceMap.end())
            pRes = surfItr->second.get();
    return pRes;
}
Surface2D* SurfaceUtils::SurfaceFillFB::operator[](SurfaceId surfaceId)
{
    Surface2D* pRes = nullptr;
    auto surfItr = m_SurfaceMap.find(surfaceId);
    if (surfItr != m_SurfaceMap.end())
            pRes = surfItr->second.get();
    return pRes;
}

RC SurfaceUtils::SurfaceFillFB::CopySurface(SurfaceId surfaceId)
{
    // Dummy implementation
    return OK;
}

RC SurfaceUtils::SurfaceFillFB::FreeAllSurfaces()
{
    for (const auto& surface : m_SurfaceMap)
    {
        surface.second->Free();
    }
    m_SurfaceMap.clear();
    return OK;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
RC SurfaceUtils::SurfaceHelper::SetupSurface
(
    const GpuSubdevice *pGpuSubdevice,
    const SurfaceParams *pSurfParams,
    Surface2D *pChnImage,
    Display::ChannelID chId
)
{
    RC rc;
    unique_ptr<SurfaceParams> defaultSurfaceParams;
    UINT32 ourGpuId = pGpuSubdevice->GetSubdeviceInst();

    MASSERT(pChnImage);

    // Check if surface is already allocated!
    if (pChnImage->GetMemHandle() != 0)
    {
        Printf(Tee::PriError,
               "Surface2D already Allocated Please Free It .. \n");
        return RC::SOFTWARE_ERROR;
    }

    // If no parameters are specified then we use the default ones.
    if (!pSurfParams)
    {
        defaultSurfaceParams.reset(new SurfaceParams());
        pSurfParams = defaultSurfaceParams.get();
    }

    // We do not support Video Heap Allocation on CheetAh
    if (!pGpuSubdevice->FbHeapSize() && !pGpuSubdevice->IsSOC())
    {
        Printf(Tee::PriError,
               "FB broken so don't allocate Surface2D buffer on FB \n");
        return RC::SOFTWARE_ERROR;
    }
    else if (pChnImage->GetMemHandle() == 0)
    {
        //
        // fill in the default settings if nothing is provided
        // Note that ColorSpace will be set as a part of procamp settings
        // during modeset call.
        //
        pChnImage->SetWidth(pSurfParams->imageWidth);
        pChnImage->SetHeight(pSurfParams->imageHeight);
        pChnImage->SetDisplayable(true);
        pChnImage->SetLocation (Memory::Fb);
        pChnImage->SetType(LWOS32_TYPE_IMAGE);
        pChnImage->SetInputRange(pSurfParams->inputRange);
        pChnImage->SetLayout(pSurfParams->layout);

        if (pGpuSubdevice->IsSOC() && pGpuSubdevice->GetParentDevice()->
                GetDisplay()->GetDisplayClassFamily() == Display::LWDISPLAY)
        {
            pChnImage->SetLocation(Memory::NonCoherent);
            pChnImage->SetRouteDispRM(true);
        }

        if (pSurfParams->colorFormat != ColorUtils::LWFMT_NONE)
        {
            pChnImage->SetColorFormat(pSurfParams->colorFormat);
        }
        else
        {
            if (chId == Display::WINDOW_CHANNEL_ID)
            {
                pChnImage->SetColorFormat(
                    ColorUtils::ColorDepthToFormat(pSurfParams->colorDepthBpp));
            }
            else if (chId == Display::LWRSOR_IMM_CHANNEL_ID)
            {
                pChnImage->SetColorFormat(LwrsColorDepthToFormat(pSurfParams->colorDepthBpp));
            }
            else
            {
                Printf(Tee::PriError,
                       "Incorrect channel specified for surface allocation \n");
                return RC::BAD_PARAMETER;
            }
        }

        CHECK_RC(pChnImage->Alloc(pGpuSubdevice->GetParentDevice()));
    }

    if (pChnImage->GetMemHandle() != 0)
    {
        if (!pSurfParams->imageFileName.empty() && (pSurfParams->imageFileName.find(".png",0) != string::npos))
            CHECK_RC(pChnImage->ReadPng(pSurfParams->imageFileName.c_str(), ourGpuId));
        else if (!pSurfParams->imageFileName.empty() && (pSurfParams->imageFileName.find(".hdr",0) != string::npos))
            CHECK_RC(pChnImage->ReadHdr(pSurfParams->imageFileName.c_str(), ourGpuId));
        else if (!pSurfParams->imageFileName.empty() && (pSurfParams->imageFileName.find(".raw",0) != string::npos))
            CHECK_RC(pChnImage->ReadRaw(pSurfParams->imageFileName.c_str(), ourGpuId));
        else if (pSurfParams->bFillFBWithColor)
            CHECK_RC(pChnImage->Fill(pSurfParams->fbColor, ourGpuId));
        else
        {
            // Display Default Image
            if ((pChnImage->ReadPng(pSurfParams->imageFileName.c_str(), ourGpuId) != OK) &&
                (pSurfParams->bFillFBWithColor))
            {
                CHECK_RC(pChnImage->Fill(0x00004000, ourGpuId));
            }
        }
    }

   return rc;
}

ColorUtils::Format SurfaceUtils::SurfaceHelper::LwrsColorDepthToFormat
(
    UINT32 colorDepth
)
{
    switch (colorDepth)
    {
        case 16:
            return ColorUtils::A1R5G5B5;
        case 32:
            return ColorUtils::A8R8G8B8;

        default:
            MASSERT(0);
            return ColorUtils::LWFMT_NONE;
    }
}

SurfaceUtils::SurfaceParams::SurfaceParams()
{
    Reset();
}

void SurfaceUtils::SurfaceParams::Reset()
{
    imageWidth = 640;
    imageHeight = 480;
    colorDepthBpp = 32;
    bFillFBWithColor = false;
    fbColor = 0;
    //
    //TODO: Default value should be changes to BlockLinear
    // once it's supported in HW.
    //
    layout = Surface2D::Pitch;
    inputRange = Surface2D::BYPASS;
    colorFormat = ColorUtils::LWFMT_NONE;
    imageFileName = "default.bin";
}

SurfaceUtils::SurfaceStorage::SurfaceStorage()
{
    Reset();
}

void SurfaceUtils::SurfaceStorage::Reset()
{
    storageFormat = PACKED;
    const size_t maxStorageSize = max({sizeof(Packed), sizeof(PackedStereo),
        sizeof(SemiPlanar), sizeof(SemiPlanarStereo), sizeof(Planar),
        sizeof(PlanarStereo), sizeof(GpuUtility::DisplayImageDescription)});
    memset(&pkdStereo, 0, maxStorageSize);
}

bool SurfaceUtils::SurfaceStorage::IsDirty() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage != nullptr;
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage != nullptr;
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.Width > 0;
        case SEMIPLANAR:
            return semiPlanar.pYImage != nullptr;
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.pYLeftImage != nullptr;
        case PLANAR:
            return planar.pYImage != nullptr;
        case PLANAR_STEREO:
            return planarStereo.pYLeftImage != nullptr;
    }
    MASSERT(!"Invalid surface format");
    return false;
}

void SurfaceUtils::SurfaceStorage::FillPackedImage(Surface2D* pSurf)
{
    storageFormat = SurfaceUtils::PACKED;
    pkd.pWindowImage = pSurf;
}

void SurfaceUtils::SurfaceStorage::FillSemiPlanarImage
(
    ColorUtils::Format fmt,
    Surface2D* pSurfY,
    Surface2D* pSurfUV
)
{
    storageFormat = SurfaceUtils::SEMIPLANAR;
    semiPlanar.colorFormat = fmt;
    semiPlanar.pYImage = pSurfY;
    semiPlanar.pUVImage = pSurfUV;
}

void SurfaceUtils::SurfaceStorage::FillStereoImageDescription
(
    Surface2D* pSurf,
    Surface2D* pSurfRight,
    UINT32 width,
    UINT32 height
)
{
    storageFormat = SurfaceUtils::PACKED_DID_STEREO;
    imageDescription.Width = width;
    imageDescription.Height = height;
    imageDescription.Layout = pSurf->GetLayout();
    imageDescription.Pitch  = pSurf->GetPitch();
    imageDescription.ColorFormat = pSurf->GetColorFormat();
    imageDescription.NumBlocksWidth = pSurf->GetBlockWidth();
    imageDescription.LogBlockHeight = pSurf->GetLogBlockHeight();
    imageDescription.CtxDMAHandle = pSurf->GetCtxDmaHandleIso();
    imageDescription.CtxDMAHandleRight = pSurfRight->GetCtxDmaHandleIso();
}

UINT32 SurfaceUtils::SurfaceStorage::GetWidth() const
{
    switch (storageFormat)
    {
        case PACKED:
        {
            if (ColorUtils::IsYuv(pkd.pWindowImage->GetColorFormat()))
            {
                // Only 422 compressed packed YUV surfaces are supported where
                // actual displayable surface is 2x the size of the input surface
                return (pkd.pWindowImage->GetWidth() * 2);
            }
            return pkd.pWindowImage->GetWidth();
        }
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetWidth();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.Width;
        case SEMIPLANAR:
            return semiPlanar.pYImage->GetWidth();
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.pYLeftImage->GetWidth();
        case PLANAR:
            return planar.pYImage->GetWidth();
        case PLANAR_STEREO:
            return planarStereo.pYLeftImage->GetWidth();
    }
    MASSERT(!"Invalid surface format");
    return 0;
}

UINT32 SurfaceUtils::SurfaceStorage::GetHeight() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetHeight();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetHeight();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.Height;
        case SEMIPLANAR:
            return semiPlanar.pYImage->GetHeight();
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.pYLeftImage->GetHeight();
        case PLANAR:
            return planar.pYImage->GetHeight();
        case PLANAR_STEREO:
            return planarStereo.pYLeftImage->GetHeight();
    }
    MASSERT(!"Invalid surface format");
    return 0;
}

Surface2D::Layout SurfaceUtils::SurfaceStorage::GetLayout() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetLayout();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetLayout();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.Layout;
        case SEMIPLANAR:
            return semiPlanar.pYImage->GetLayout();
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.pYLeftImage->GetLayout();
        case PLANAR:
            return planar.pYImage->GetLayout();
        case PLANAR_STEREO:
            return planarStereo.pYLeftImage->GetLayout();
    }
    MASSERT(!"Invalid surface format");
    return Surface2D::Pitch;
}

UINT32 SurfaceUtils::SurfaceStorage::GetBlockWidth(UINT32 planeIdx) const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetBlockWidth();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetBlockWidth();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.NumBlocksWidth;
        case SEMIPLANAR:
        {
            switch (planeIdx)
            {
                case 0:
                    return semiPlanar.pYImage->GetBlockWidth();
                case 1:
                    return semiPlanar.pUVImage->GetBlockWidth();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case SEMIPLANAR_STEREO:
        {
            switch (planeIdx)
            {
                case 0:
                    return semiPlanarStereo.pYLeftImage->GetBlockWidth();
                case 1:
                    return semiPlanarStereo.pUVLeftImage->GetBlockWidth();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case PLANAR:
        {
            switch (planeIdx)
            {
                case 0:
                    return planar.pYImage->GetBlockWidth();
                case 1:
                    return planar.pUImage->GetBlockWidth();
                case 2:
                    return planar.pVImage->GetBlockWidth();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case PLANAR_STEREO:
        {
            switch (planeIdx)
            {
                case 0:
                    return planarStereo.pYLeftImage->GetBlockWidth();
                case 1:
                    return planarStereo.pULeftImage->GetBlockWidth();
                case 2:
                    return planarStereo.pVLeftImage->GetBlockWidth();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
    }
    MASSERT(!"Invalid surface format");
    return 0;
}

UINT32 SurfaceUtils::SurfaceStorage::GetLogBlockHeight() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetLogBlockHeight();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetLogBlockHeight();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.LogBlockHeight;
        case SEMIPLANAR:
            return semiPlanar.pYImage->GetLogBlockHeight();
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.pYLeftImage->GetLogBlockHeight();
        case PLANAR:
            return planar.pYImage->GetLogBlockHeight();
        case PLANAR_STEREO:
            return planarStereo.pYLeftImage->GetLogBlockHeight();
    }
    MASSERT(!"Invalid surface format");
    return 0;
}

UINT32 SurfaceUtils::SurfaceStorage::GetPitch(UINT32 planeIdx) const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetPitch();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetPitch();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.Pitch;
        case SEMIPLANAR:
        {
            switch (planeIdx)
            {
                case 0:
                    return semiPlanar.pYImage->GetPitch();
                case 1:
                    return semiPlanar.pUVImage->GetPitch();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case SEMIPLANAR_STEREO:
        {
            switch (planeIdx)
            {
                case 0:
                    return semiPlanarStereo.pYLeftImage->GetPitch();
                case 1:
                    return semiPlanarStereo.pUVLeftImage->GetPitch();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case PLANAR:
        {
            switch (planeIdx)
            {
                case 0:
                    return planar.pYImage->GetPitch();
                case 1:
                    return planar.pUImage->GetPitch();
                case 2:
                    return planar.pVImage->GetPitch();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
        case PLANAR_STEREO:
        {
            switch (planeIdx)
            {
                case 0:
                    return planarStereo.pYLeftImage->GetPitch();
                case 1:
                    return planarStereo.pULeftImage->GetPitch();
                case 2:
                    return planarStereo.pVLeftImage->GetPitch();
                default:
                    MASSERT(!"Invalid plane index");
                    return 0;
            }
        }
    }
    MASSERT(!"Invalid surface format");
    return 0;
}

Surface2D::InputRange SurfaceUtils::SurfaceStorage::GetInputRange() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetInputRange();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetInputRange();
        case PACKED_DID:
        case PACKED_DID_STEREO:
        case SEMIPLANAR:
        case SEMIPLANAR_STEREO:
        case PLANAR:
        case PLANAR_STEREO:
            return Surface2D::BYPASS;
    }
    MASSERT(!"Invalid surface format");
    return Surface2D::BYPASS;
}

RC SurfaceUtils::SurfaceStorage::GetSurfaces(Surfaces *pSurfs) const
{
    MASSERT(pSurfs);
    switch (storageFormat)
    {
        case PACKED:
            pSurfs->push_back(pkd.pWindowImage);
            break;
        case PACKED_STEREO:
            pSurfs->push_back(pkdStereo.pLeftWindowImage);
            pSurfs->push_back(pkdStereo.pRightWindowImage);
            break;
        case SEMIPLANAR:
            pSurfs->push_back(semiPlanar.pYImage);
            pSurfs->push_back(semiPlanar.pUVImage);
            break;
        case SEMIPLANAR_STEREO:
            pSurfs->push_back(semiPlanarStereo.pYLeftImage);
            pSurfs->push_back(semiPlanarStereo.pUVLeftImage);
            pSurfs->push_back(semiPlanarStereo.pYRightImage);
            pSurfs->push_back(semiPlanarStereo.pUVRightImage);
            break;
        case PLANAR:
            pSurfs->push_back(planar.pYImage);
            pSurfs->push_back(planar.pUImage);
            pSurfs->push_back(planar.pVImage);
            break;
        case PLANAR_STEREO:
            pSurfs->push_back(planarStereo.pYLeftImage);
            pSurfs->push_back(planarStereo.pULeftImage);
            pSurfs->push_back(planarStereo.pVLeftImage);
            pSurfs->push_back(planarStereo.pYRightImage);
            pSurfs->push_back(planarStereo.pURightImage);
            pSurfs->push_back(planarStereo.pURightImage);
            break;
        case PACKED_DID:
            // Surfaces for PACKED_DID and PACKED_DID_STEREO are not generated
            // in advance and stored. Rather surface info is stored.
            // We eventually would want to drop support for them.
            // JIRA - MODSAD10X-168
            // intentional fall through
        case PACKED_DID_STEREO:
            // intentional fall through
        default:
            Printf(Tee::PriError, "Unsupported storage format\n");
            return RC::BAD_FORMAT;
    }
    return RC::OK;
}

ColorUtils::Format SurfaceUtils::SurfaceStorage::GetColorFormat() const
{
    switch (storageFormat)
    {
        case PACKED:
            return pkd.pWindowImage->GetColorFormat();
        case PACKED_STEREO:
            return pkdStereo.pLeftWindowImage->GetColorFormat();
        case PACKED_DID:
        case PACKED_DID_STEREO:
            return imageDescription.ColorFormat;
        case SEMIPLANAR:
            return semiPlanar.colorFormat;
        case SEMIPLANAR_STEREO:
            return semiPlanarStereo.colorFormat;
        case PLANAR:
            return planar.colorFormat;
        case PLANAR_STEREO:
            return planarStereo.colorFormat;
    }
    MASSERT(!"Invalid surface format");
    return ColorUtils::LWFMT_NONE;
}
