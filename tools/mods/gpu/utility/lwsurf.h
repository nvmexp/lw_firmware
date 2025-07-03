/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/memtypes.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surf2d.h"
#if defined(INCLUDE_LWDA)
#include "core/include/utility.h"
#include "gpu/tests/lwca/lwdawrap.h"
#endif
#include <vector>

class GpuDevice;

// Use Lwca to compare 2 GPU surfaces, evaluate CRC..
class LwSurfRoutines
{
public:
    class CrcSize
    {
    public:
        CrcSize() = default;
        CrcSize(UINT32 b, UINT32 tpb) : m_Blocks(b), m_ThreadsPerBlock(tpb) { }
        UINT32 Blocks() { return m_Blocks; }
        UINT32 ThreadsPerBlock() { return m_ThreadsPerBlock; }
        void Min(const CrcSize &c)
        {
            m_Blocks = min(m_Blocks, c.m_Blocks);
            m_ThreadsPerBlock = min(m_ThreadsPerBlock, c.m_ThreadsPerBlock);
        }
    private:
        UINT32 m_Blocks          = ~0U;
        UINT32 m_ThreadsPerBlock = ~0U;
    };
#if defined(INCLUDE_LWDA)
    typedef Lwca::HostMemory LwSurf;
#else
    struct LwSurf
    {
        UINT32 dummy;
        void Free() { }
        size_t GetSize() { return 0; }
    };
#endif

    LwSurfRoutines() = default;
    ~LwSurfRoutines() { Free(); }

    RC Initialize(GpuDevice *pGpuDev);
    bool IsInitialized() const { return m_Initialized; }
    bool IsSupported() const;
    void Free();

    GpuDevice * GetGpuDevice() const { return m_pGpuDevice; }

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillConstant(const Surface2D &surf, const T constant);

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillConstant
    (
        GpuDevice          *pGpuDev,
        const LwRm::Handle  handle,
        const UINT64        size,
        const T             constant
    );

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillRandom(const Surface2D &surf, const UINT32 seed);

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillRandom
    (
        GpuDevice          *pGpuDev,
        const LwRm::Handle  handle,
        const UINT64        size,
        const UINT32        seed
    );

    RC InjectError
    (
        GpuDevice          *pGpuDev,
        const LwRm::Handle  handle,
        const UINT64        size,
        const UINT64        location,
        const UINT64        errorValue
    );

    //--------------------------------------------------------------------------
    // Generate CRCs that are explicitly designed to match CPU calcluated CRCs
    // from core/utility/crc.cpp
    // 
    // handle       : RM Handle to the memory
    // offset       : offset into the buffer from where CRC will be callwlated
    // size         : Size of buffer whose CRC will be callwlated
    // loc          : Location of buffer
    // perComponent : The lwca kernel assumes input surface in R8G8B8A8 format. If CRC is
    //                requested per component, then 4 CRCs will be returned in order:(R, G, B, A)
    //                (each component = 1 byte), otherwise, a single CRC will be returned for
    //                all 4 components.
    // pCrc         : Output buffer where CRCs are returned. this buffer will either contain
    //                1 or 4 CRCs(per component) depending on value of perComponent
    RC GetCRC
    (
        GpuDevice             *pGpuDev,
        const LwRm::Handle     handle,
        const UINT64           offset,
        const UINT64           size,
        const Memory::Location loc,
        const bool             perComponent,
        vector<UINT32>        *pCrc
    );

    RC CompareBuffers
    (
        GpuDevice             *pGpuDev,
        const LwRm::Handle     handle1,
        const UINT64           size1,
        const Memory::Location loc1,
        const LwRm::Handle     handle2,
        const UINT64           size2,
        const Memory::Location loc2,
        const UINT32           mask,
        bool                  *result
    );

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    ComputeCrcs(const Surface2D &surf, CrcSize crcSize, LwSurf *pCrcData);
    CrcSize GetCrcSize() { return CrcSize(m_MaxBlocks, m_MaxThreadsPerBlock); }

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    CompareBuffers(const Surface2D &surf1, const Surface2D &surf2);

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    CompareBuffers(LwSurf &surf1, LwSurf &surf2);

private:
    GpuDevice *m_pGpuDevice         = nullptr;
    bool       m_Initialized        = false;
    UINT32     m_MaxThreadsPerBlock = 0;
    UINT32     m_MaxBlocks          = 0;
    UINT32     m_WarpSize           = 0;
#if defined(INCLUDE_LWDA)
    Lwca::Instance m_Lwda;
    Lwca::Device   m_LwdaDevice;
    Lwca::Module   m_LwSurfModule;

    // Functions exposed in the module
    Lwca::Function m_CpuMatchingCrcFunc;
    Lwca::Function m_CpuMatchingCompCrcFunc;
    Lwca::Function m_InjectErrorFunc;

    // buffers
    Lwca::DeviceMemory m_DevCrcTableBuf;

    RC InitCheck(GpuDevice *pGpuDev, UINT64 size, UINT32 alignment);
    bool IsMemorySupported
    (
        GpuDevice    *pGpuDev,
        const UINT64  size,
        const UINT32  blocks,
        const UINT32  threadsPerBlock
    );
#endif

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillConstant
    (
        GpuDevice             *pGpuDev,
        const LwRm::Handle     handle,
        const UINT64           size,
        const Memory::Location loc,
        const T                constant,
        const UINT32           blocks,
        const UINT32           threadsPerBlock
    );

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    FillRandom
    (
        GpuDevice             *pGpuDev,
        const LwRm::Handle     handle,
        const UINT64           size,
        const Memory::Location loc,
        const UINT32           seed,
        const UINT32           blocks,
        const UINT32           threadsPerBlock
    );

    template <typename T>
        enable_if_t<is_integral<T>::value &&
                    (sizeof(T) == 1 ||
                     sizeof(T) == 4 ||
                     sizeof(T) == 8), RC>
    CompareBuffers
    (
        GpuDevice             *pGpuDev,
        const LwRm::Handle     handle1,
        const UINT64           size1,
        const Memory::Location loc1,
        const LwRm::Handle     handle2,
        const UINT64           size2,
        const Memory::Location loc2,
        const UINT32           componentMask,
        const UINT32           blocks,
        const UINT32           threadsPerBlock,
        bool                  *result
    );
};

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillConstant(const Surface2D &surf, const T constant)
{
    return FillConstant<T>(surf.GetGpuDev(),
                           surf.GetMemHandle(),
                           surf.GetSize(),
                           surf.GetLocation(),
                           constant,
                           m_MaxBlocks,
                           m_MaxThreadsPerBlock);
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillConstant
(
    GpuDevice          *pGpuDev,
    const LwRm::Handle  handle,
    const UINT64        size,
    const T             constant
)
{
    return FillConstant<T>(pGpuDev,
                           handle,
                           size,
                           Memory::Fb,
                           constant,
                           m_MaxBlocks,
                           2 * m_WarpSize);
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillRandom(const Surface2D &surf, const UINT32 seed)
{
    return FillRandom<T>(surf.GetGpuDev(),
                         surf.GetMemHandle(),
                         surf.GetSize(),
                         surf.GetLocation(),
                         seed,
                         m_MaxBlocks,
                         m_MaxThreadsPerBlock);
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillRandom
(
    GpuDevice          *pGpuDev,
    const LwRm::Handle  handle,
    const UINT64        size,
    const UINT32        seed
)
{
    return FillRandom<T>(pGpuDev,
                         handle,
                         size,
                         Memory::Fb,
                         seed,
                         m_MaxBlocks,
                         2 * m_WarpSize);
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::ComputeCrcs(const Surface2D &surf, CrcSize crcSize, LwSurf *pCrcs)
{
#if defined(INCLUDE_LWDA)
    RC rc;
    CHECK_RC(InitCheck(surf.GetGpuDev(), surf.GetSize(), sizeof(T)));

    Lwca::ClientMemory surfMem;
    bool bAllocCrcData = !pCrcs->GetSize();
    const UINT64 crcDataSize = crcSize.Blocks() * crcSize.ThreadsPerBlock() * sizeof(UINT32);

    // If CRC data was already allocated, ensure that it is the correct size
    if (!bAllocCrcData && (pCrcs->GetSize() != crcDataSize))
    {
        Printf(Tee::PriWarn, "%s : Resizing CRC data\n", __FUNCTION__);
        pCrcs->Free();
        bAllocCrcData = true;
    }

    if (bAllocCrcData)
    {
        CHECK_RC(m_Lwda.AllocHostMem(m_LwdaDevice, crcDataSize, pCrcs));
    }

    surfMem = m_Lwda.MapClientMem(surf.GetMemHandle(),
                                  0,
                                  surf.GetSize(),
                                  surf.GetGpuDev(),
                                  surf.GetLocation());
    CHECK_RC(surfMem.InitCheck());
    DEFER() { surfMem.Free(); };

    Lwca::Function crcFunc;
    switch (sizeof(T))
    {
        case 1:
            crcFunc = m_LwSurfModule["GetCRC08"];
            break;
        case 4:
            crcFunc = m_LwSurfModule["GetCRC32"];
            break;
        case 8:
            crcFunc = m_LwSurfModule["GetCRC64"];
            break;
        default:
            // Shouldnt be possible to get here due to enable_if_t in template
            MASSERT(!"Unknown element size!");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(crcFunc.InitCheck());

    crcFunc.SetGridDim(crcSize.Blocks());
    crcFunc.SetBlockDim(crcSize.ThreadsPerBlock());
    CHECK_RC(crcFunc.Launch(surfMem.GetDevicePtr(),
                            m_DevCrcTableBuf.GetDevicePtr(),
                            pCrcs->GetDevicePtr(&m_Lwda, m_LwdaDevice),
                            surf.GetSize() / sizeof(T)));
    CHECK_RC(m_Lwda.Synchronize());

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::CompareBuffers(const Surface2D &surf1, const Surface2D &surf2)
{
#if defined(INCLUDE_LWDA)
    if (surf1.GetGpuDev() != surf2.GetGpuDev())
    {
        Printf(Tee::PriError, "%s : Surfaces must be allocated on the same GpuDevice\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    bool bResult = true;
    CHECK_RC(CompareBuffers<T>(surf1.GetGpuDev(),
                               surf1.GetMemHandle(),
                               surf1.GetSize(),
                               surf1.GetLocation(),
                               surf2.GetMemHandle(),
                               surf2.GetSize(),
                               surf2.GetLocation(),
                               0,
                               m_MaxBlocks,
                               m_MaxThreadsPerBlock,
                               &bResult));
    return bResult ? rc : RC::BUFFER_MISMATCH;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::CompareBuffers(LwSurf &surf1, LwSurf &surf2)
{
#if defined(INCLUDE_LWDA)
    RC rc;
    CHECK_RC(InitCheck(nullptr, 0ULL, sizeof(T)));

    if (!surf1.GetSize() || !surf2.GetSize())
    {
        Printf(Tee::PriError,
               "%s : Surfaces not valid, allocate before comparing!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    if (surf1.GetSize() != surf2.GetSize())
    {
        Printf(Tee::PriError,
               "%s : Surface buffers are not the same size!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Lwca::HostMemory compareData;
    CHECK_RC(m_Lwda.AllocHostMem(m_LwdaDevice, surf1.GetSize(), &compareData));
    DEFER() { compareData.Free(); };

    Lwca::Function compareFunc;
    switch (sizeof(T))
    {
        case 1:
            compareFunc = m_LwSurfModule["CompareBuffers08"];
            break;
        case 4:
            compareFunc = m_LwSurfModule["CompareBuffers32"];
            break;
        case 8:
            compareFunc = m_LwSurfModule["CompareBuffers64"];
            break;
        default:
            // Shouldnt be possible to get here due to enable_if_t in template
            MASSERT(!"Unknown element size!");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(compareFunc.InitCheck());
    compareFunc.SetGridDim(m_MaxBlocks);
    compareFunc.SetBlockDim(m_MaxThreadsPerBlock);
    CHECK_RC(compareFunc.Launch(surf1.GetDevicePtr(&m_Lwda, m_LwdaDevice),
                                surf2.GetDevicePtr(&m_Lwda, m_LwdaDevice),
                                compareData.GetDevicePtr(&m_Lwda, m_LwdaDevice),
                                compareData.GetSize() / sizeof(T),
                                ~0ULL));
    CHECK_RC(m_Lwda.Synchronize());

    for (UINT32 i = 0; i < compareData.GetSize() / sizeof(UINT32); i++)
    {
        if (static_cast<UINT32*>(compareData.GetPtr())[i])
        {
            if (rc == OK)
                rc = RC::GOLDEN_VALUE_MISCOMPARE;
            Printf(Tee::PriError, "Mismatch detected at index %u\n", i);
        }
    }
    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillConstant
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle,
    const UINT64           size,
    const Memory::Location loc,
    const T                constant,
    const UINT32           blocks,
    const UINT32           threadsPerBlock
)
{
#if defined(INCLUDE_LWDA)
    RC rc;
    CHECK_RC(Initialize(pGpuDev));
    CHECK_RC(InitCheck(pGpuDev, size, sizeof(T)));

    Lwca::Function fillFunc;
    switch (sizeof(T))
    {
        case 1:
            fillFunc = m_LwSurfModule["FillConstant08"];
            break;
        case 4:
            fillFunc = m_LwSurfModule["FillConstant32"];
            break;
        case 8:
            fillFunc = m_LwSurfModule["FillConstant64"];
            break;
        default:
            // Shouldnt be possible to get here due to enable_if_t in template
            MASSERT(!"Unknown element size!");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(fillFunc.InitCheck());
    Lwca::ClientMemory cliMem;
    cliMem = m_Lwda.MapClientMem(handle, 0, size, pGpuDev, loc);
    CHECK_RC(cliMem.InitCheck());
    DEFER()
    {
        cliMem.Free();
    };

    fillFunc.SetGridDim(blocks);
    fillFunc.SetBlockDim(threadsPerBlock);

    CHECK_RC(fillFunc.Launch(cliMem.GetDevicePtr(), size / sizeof(T), constant));
    CHECK_RC(m_Lwda.Synchronize());

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::FillRandom
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle,
    const UINT64           size,
    const Memory::Location loc,
    const UINT32           seed,
    const UINT32           blocks,
    const UINT32           threadsPerBlock
)
{
#if defined(INCLUDE_LWDA)
    RC rc;
    CHECK_RC(Initialize(pGpuDev));
    CHECK_RC(InitCheck(pGpuDev, size, sizeof(T)));

    Lwca::Function fillFunc;
    switch (sizeof(T))
    {
        case 1:
            fillFunc = m_LwSurfModule["FillRandom08"];
            break;
        case 4:
            fillFunc = m_LwSurfModule["FillRandom32"];
            break;
        case 8:
            fillFunc = m_LwSurfModule["FillRandom64"];
            break;
        default:
            // Shouldnt be possible to get here due to enable_if_t in template
            MASSERT(!"Unknown element size!");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(fillFunc.InitCheck());

    Lwca::ClientMemory cliMem;
    cliMem = m_Lwda.MapClientMem(handle, 0, size, pGpuDev, loc);
    CHECK_RC(cliMem.InitCheck());
    DEFER()
    {
        cliMem.Free();
    };

    fillFunc.SetGridDim(blocks);
    fillFunc.SetBlockDim(threadsPerBlock);

    CHECK_RC(fillFunc.Launch(cliMem.GetDevicePtr(), size / sizeof(T), seed));
    CHECK_RC(m_Lwda.Synchronize());

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
template <typename T>
    enable_if_t<is_integral<T>::value &&
                (sizeof(T) == 1 ||
                 sizeof(T) == 4 ||
                 sizeof(T) == 8), RC>
LwSurfRoutines::CompareBuffers
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle1,
    const UINT64           size1,
    const Memory::Location loc1,
    const LwRm::Handle     handle2,
    const UINT64           size2,
    const Memory::Location loc2,
    const UINT32           componentMask,
    const UINT32           blocks,
    const UINT32           threadsPerBlock,
    bool                  *pResult
)
{
#if defined(INCLUDE_LWDA)
    RC rc;
    if (size1 != size2)
    {
        Printf(Tee::PriError, "Surfaces to compare must be of same size.\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(Initialize(pGpuDev));
    CHECK_RC(InitCheck(pGpuDev, size1, sizeof(T)));

    MASSERT(pResult);
    *pResult = true;

    Lwca::Function compareFunc;
    switch (sizeof(T))
    {
        case 1:
            compareFunc = m_LwSurfModule["CompareBuffers08"];
            break;
        case 4:
            compareFunc = m_LwSurfModule["CompareBuffers32"];
            break;
        case 8:
            compareFunc = m_LwSurfModule["CompareBuffers64"];
            break;
        default:
            // Shouldnt be possible to get here due to enable_if_t in template
            MASSERT(!"Unknown element size!");
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(compareFunc.InitCheck());

    // The lwsurf lwbin writes as UINT32
    Lwca::DeviceMemory compareResults;
    const UINT32 cmpBufferSize = blocks * threadsPerBlock;
    CHECK_RC(m_Lwda.AllocDeviceMem(cmpBufferSize * sizeof(UINT32), &compareResults));
    CHECK_RC(compareResults.InitCheck());
    DEFER() { compareResults.Free(); };

    Lwca::ClientMemory buf1, buf2;
    buf1 = m_Lwda.MapClientMem(handle1, 0, size1, pGpuDev, loc1);
    CHECK_RC(buf1.InitCheck());
    DEFER() { buf1.Free(); };

    buf2 = m_Lwda.MapClientMem(handle2, 0, size2, pGpuDev, loc2);
    CHECK_RC(buf2.InitCheck());
    DEFER() { buf2.Free(); };

    compareFunc.SetGridDim(blocks);
    compareFunc.SetBlockDim(threadsPerBlock);

    //colwert 4 bit mask to 64 bit, e.g 0x5 to : 0x00FF00FF00FF00FF
    UINT64 kernelMask = ~0ULL;
    if (componentMask)   //mask the comparison values before checking
    {
        UINT64 kernelMask = 0;
        for (UINT32 i = 0; i < 4; i++)
        {
            if ((1 << i) & componentMask)
            {
                kernelMask = kernelMask | (0xFFLL << (i * 8));
            }
        }
        kernelMask = (kernelMask << 32) | kernelMask;
    }
    CHECK_RC(compareFunc.Launch(buf1.GetDevicePtr(),
                                buf2.GetDevicePtr(),
                                compareResults.GetDevicePtr(),
                                size1 / sizeof(T),
                                kernelMask));
    CHECK_RC(m_Lwda.Synchronize());

    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        hostCmpBuffer(cmpBufferSize,
                      UINT32(),
                      Lwca::HostMemoryAllocator<UINT32>(&m_Lwda, m_LwdaDevice));
    CHECK_RC(compareResults.Get(&hostCmpBuffer[0], hostCmpBuffer.size() * sizeof(UINT32)));
    CHECK_RC(m_Lwda.Synchronize());

    for (UINT32 i = 0; i < cmpBufferSize; i ++)
    {
        if (hostCmpBuffer[i])
        {
            *pResult = false;
            break;
        }
    }

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

