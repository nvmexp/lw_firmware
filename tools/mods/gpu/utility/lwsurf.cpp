/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwsurf.h"
#include "core/include/crc.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/chanwmgr.h"

//-----------------------------------------------------------------------------
RC LwSurfRoutines::Initialize(GpuDevice *pGpuDev)
{
    RC rc;

    if (m_Initialized)
        return OK;

    if (pGpuDev->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriError, "LwSurfRoutines not supported on SLI devices!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    DEFERNAME(initFail)
    {
        Free();
    };

    CHECK_RC(m_Lwda.Init());
    m_Lwda.FindDevice(*pGpuDev, &m_LwdaDevice);
    CHECK_RC(m_Lwda.InitContext(m_LwdaDevice));

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper() ||
        !m_LwdaDevice.IsValid())
    {
        return RC::LWDA_INIT_FAILED;
    }

    m_MaxThreadsPerBlock = m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    m_MaxBlocks          = m_LwdaDevice.GetShaderCount();
    m_WarpSize           = m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

    CHECK_RC(m_Lwda.AllocDeviceMem(m_LwdaDevice, Crc::TableSizeBytes(), &m_DevCrcTableBuf));
    vector<UINT32> crcTable;
    Crc::GetCRCTable(&crcTable);
    CHECK_RC(m_DevCrcTableBuf.Set(&crcTable[0], Crc::TableSizeBytes()));
    CHECK_RC(m_Lwda.Synchronize());

    CHECK_RC(m_Lwda.LoadNewestSupportedModule("lwsurf", &m_LwSurfModule));

    Lwca::Function m_ComputeComponentCrcFunc;
    Lwca::Function m_InjectErrorFunc;

    m_CpuMatchingCrcFunc = m_LwSurfModule["GetCPUMatchingCRC"];
    CHECK_RC(m_CpuMatchingCrcFunc.InitCheck());

    m_CpuMatchingCompCrcFunc = m_LwSurfModule["GetCPUMatchingComponentCRC"];
    CHECK_RC(m_CpuMatchingCompCrcFunc.InitCheck());

    m_InjectErrorFunc = m_LwSurfModule["InjectError"];
    CHECK_RC(m_InjectErrorFunc.InitCheck());

    initFail.Cancel();
    m_pGpuDevice  = pGpuDev;
    m_Initialized = true;
    return rc;
}

//------------------------------------------------------------------------------
bool LwSurfRoutines::IsSupported() const
{
    return true;
}

//------------------------------------------------------------------------------
void LwSurfRoutines::Free()
{
    if (m_LwSurfModule.IsValid())
        m_LwSurfModule.Unload();
    if (m_DevCrcTableBuf.IsValid())
        m_DevCrcTableBuf.Free();
    m_Lwda.Free();
    m_Initialized = false;
    m_pGpuDevice = nullptr;
}

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
RC LwSurfRoutines::GetCRC
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle,
    const UINT64           offset,
    const UINT64           size,
    const Memory::Location loc,
    const bool             perComponent,
    vector<UINT32>        *pCrc
)
{
    RC rc;
    MASSERT(pCrc);

    CHECK_RC(Initialize(pGpuDev));

    if (Crc::s_NumCRCBlocks % m_WarpSize != 0)
    {
        // The constant 's_NumCRCBlocks' defines number of threads
        // that will be launched during CRC gathering. If this is not a multiple
        // of the warp size then the CRCs cannot be callwlated
        Printf(Tee::PriError, "Illegal kernel grid dimension.\n");
        return RC::SOFTWARE_ERROR;
    }

    const UINT32 numBlocks = Crc::s_NumCRCBlocks/m_WarpSize;

    if (!IsMemorySupported(pGpuDev, size, numBlocks, m_WarpSize))
    {
        Printf(Tee::PriError,
               "%s : provided memory is not supported (see previous messages)\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // The input surface to the kernel is assumed in R8G8B8A8 format
    // Each thread with index 'idx' computes CRC on a chunk of surface and
    // writes it at position 'idx' in the 'return surface'

    // the resulting CRC can be a single UINT32 (RU32 format, written at position
    // 'idx' or a UINT32 per component (RU32_GU32_BU32_AU32), written at position
    // 4*idx + component

    ColorUtils::Format returnSurfFmt = ColorUtils::RU32;
    if (perComponent)
    {
        returnSurfFmt = ColorUtils::RU32_GU32_BU32_AU32;
    }
    UINT32 returnSurfSize = numBlocks * m_WarpSize * ColorUtils::PixelBytes(returnSurfFmt);

    // if the return surface is in RU32 format we will get a single CRC out of it,
    // if its in RU32_GU32_BU32_AU32 format however, we will get 4 CRCs (per component)
    const UINT32 numCRCs = perComponent ? 4 : 1;
    pCrc->resize(numCRCs);

    Lwca::DeviceMemory computedCrcBuf;
    CHECK_RC(m_Lwda.AllocDeviceMem(returnSurfSize, &computedCrcBuf));
    DEFER(){ computedCrcBuf.Free(); };

    Lwca::ClientMemory cliMem;
    cliMem = m_Lwda.MapClientMem(handle, offset, size, pGpuDev, loc);
    CHECK_RC(cliMem.InitCheck());
    DEFER() { cliMem.Free(); };

    Lwca::Function &computeKernel = perComponent ? m_CpuMatchingCompCrcFunc :
                                                   m_CpuMatchingCrcFunc;
    computeKernel.SetGridDim(numBlocks);
    computeKernel.SetBlockDim(m_WarpSize);

    CHECK_RC(computeKernel.Launch(cliMem.GetDevicePtr(),
                                  m_DevCrcTableBuf.GetDevicePtr(),
                                  computedCrcBuf.GetDevicePtr(),
                                  size / sizeof(UINT64)));
    CHECK_RC(m_Lwda.Synchronize());

    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        hostCrcBuffer(returnSurfSize/sizeof(UINT32),
                      UINT32(),
                      Lwca::HostMemoryAllocator<UINT32>(&m_Lwda, m_LwdaDevice));
    CHECK_RC(computedCrcBuf.Get(&hostCrcBuffer[0], hostCrcBuffer.size() * sizeof(UINT32)));
    CHECK_RC(m_Lwda.Synchronize());

    Crc::Callwlate(&hostCrcBuffer[0],
                    numBlocks * m_WarpSize,
                    1,
                    returnSurfFmt,
                    returnSurfSize,
                    1,
                    &(*pCrc)[0]);

    return rc;
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::InjectError
(
    GpuDevice          *pGpuDev,
    const LwRm::Handle  handle,
    const UINT64        size,
    const UINT64        location,
    const UINT64        errorValue
)
{
    RC rc;
    CHECK_RC(Initialize(pGpuDev));
    CHECK_RC(InitCheck(pGpuDev, size, sizeof(UINT64)));

    Lwca::ClientMemory cliMem;
    cliMem = m_Lwda.MapClientMem(handle, 0, size, pGpuDev, Memory::Fb);
    CHECK_RC(cliMem.InitCheck());
    DEFER() { cliMem.Free(); };

    m_InjectErrorFunc.SetGridDim(1);
    m_InjectErrorFunc.SetBlockDim(m_WarpSize);

    //The kernel writes a 64 bit value.
    CHECK_RC(m_InjectErrorFunc.Launch(cliMem.GetDevicePtr(),
                                      size / sizeof(UINT64),
                                      location,
                                      errorValue));
    CHECK_RC(m_Lwda.Synchronize());

    return rc;
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::CompareBuffers
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle1,
    const UINT64           size1,
    const Memory::Location loc1,
    const LwRm::Handle     handle2,
    const UINT64           size2,
    const Memory::Location loc2,
    const UINT32           mask,
    bool                  *pResult
)
{
    // Debug: Inject errors on this surface?
    //CHECK_RC(InjectError(pGpuDev, handle1, size1, 0, 0x12345678));
    return CompareBuffers<UINT64>(pGpuDev,
                                  handle1,
                                  size1,
                                  loc1,
                                  handle2,
                                  size2,
                                  loc2,
                                  mask,
                                  m_MaxBlocks,
                                  2 * m_WarpSize,
                                  pResult);
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::InitCheck(GpuDevice *pGpuDev, UINT64 size, UINT32 alignment)
{
    if (!m_Initialized)
    {
        Printf(Tee::PriError, "LwSurfRoutines object not initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((size != 0) && (size % alignment))
    {
        Printf(Tee::PriError, "size (%llu) must be a multiple of %u!\n", size, alignment);
        return RC::SOFTWARE_ERROR;
    }

    if ((pGpuDev != nullptr) && (pGpuDev != m_pGpuDevice))
    {
        Printf(Tee::PriError,
               "Memory not allocated on the same device as the LwSurfRoutines object\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
bool LwSurfRoutines::IsMemorySupported
(
    GpuDevice    *pGpuDev,
    const UINT64  size,
    const UINT32  blocks,
    const UINT32  threadsPerBlock
)
{
    MASSERT(m_Initialized);

    if (pGpuDev != m_pGpuDevice)
    {
        Printf(Tee::PriError,
               "Memory must be allocated on the same device as the LwSurfRoutines object\n");
        return false;
    }

    const UINT64 reqAlignment = sizeof(UINT64);
    if (size % reqAlignment)
    {
        Printf(Tee::PriError,
               "Surface size must be a multiple of %llu bytes\n",
               reqAlignment);
        return false;
    }

    // Surface accesses are split across all launched threads and operations are performed in
    // parallel. Memory region size per shard must be a multiple of 64 bit
    const UINT64 totalThreads  = static_cast<UINT64>(blocks) * threadsPerBlock;
    const UINT64 dataPerThread = size / totalThreads;
    if (dataPerThread % reqAlignment)
    {
        Printf(Tee::PriError,
               "Data processed per thread (%llu bytes with %llu threads) must be a"
               " multiple of %llu bytes\n",
               dataPerThread,
               totalThreads,
               reqAlignment);
        return false;
    }

    // If there are any residual elements, those elements are considered while
    // computing the CRC by the threads (each thread picks one residual element
    // and uses it while computing the CRC). Need to check that this data is a
    // multiple of 64 bit
    UINT64 residualDataSize = size % totalThreads;
    if (residualDataSize  % reqAlignment)
    {
        Printf(Tee::PriError,
               "Residual data (%llu bytes) must be a multiple of %llu bytes\n",
               residualDataSize,
               reqAlignment);
        return false;
    }

    return true;
}

