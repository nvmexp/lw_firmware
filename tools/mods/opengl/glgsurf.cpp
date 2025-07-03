/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file glgsurf.cpp
//! \brief Implements the GLGoldenSurfaces class.

#include "glgoldensurfaces.h"
#include "core/include/crc.h"
#include "core/include/mods_profile.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "lwos.h"
#include <algorithm>

static const UINT32 NO_CACHED_SURFACE = 0xffffffff;
const UINT32 CRC_TABLE_SIZE = 256;
const UINT32 POLYNOMIAL     = 0x04C11DB7;

//------------------------------------------------------------------------------
GLGoldenSurfaces::GLGoldenSurfaces()
: m_LwrCachedSurface(NO_CACHED_SURFACE)
, m_pCachedFmtInfo(NULL)
, m_CachedAbsPitch(0)
, m_InjectErrorAtSurface(-1)    //do not inject error
, m_NumLayers(0)
, m_DepthTextureID(0)
, m_ColorSurfaceID(0)
, m_BinSize(31)     //Configurable from JS
, m_CalcMethod(Goldelwalues::CheckSums)
, m_PixelPackBuffer(0)
, m_TextureBuffer(0)
, m_StorageBuffer(0)
, m_StorageBufferFmt(ColorUtils::A8R8G8B8)  //shaders are written assuming this format
, m_ComputeCheckSumProgram(0)
, m_ComputeCRCProgram(0)
, m_CalcAlgoHint(Goldelwalues::CpuCalcAlgorithm)
{
    m_ReduceOptimization = false;
}

//------------------------------------------------------------------------------
GLGoldenSurfaces::~GLGoldenSurfaces()
{
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::CleanupSurfCheckShader()
{
    if (m_CalcAlgoHint == Goldelwalues::FragmentedGpuCalcAlgorithm)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glDeleteBuffers(1, &m_PixelPackBuffer);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glDeleteBuffers(1, &m_StorageBuffer);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glDeleteTextures(1, &m_TextureBuffer);

        glUseProgram(0);
        if (m_ComputeCheckSumProgram)
        {
            glDeleteProgram(m_ComputeCheckSumProgram);
        }
        if (m_ComputeCRCProgram)
        {
            glDeleteProgram(m_ComputeCRCProgram);
        }
    }
}

//------------------------------------------------------------------------------
int GLGoldenSurfaces::NumSurfaces() const
{
    return (int) m_Surfs.size();
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::InjectErrorAtSurface(int surfNum)
{
    m_InjectErrorAtSurface = surfNum;
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::SetSurfaceCheckMethod(int surfNum,
     Goldelwalues::Code calcMethod)
{
    if (IsSurfChkUsingFragAlgo(surfNum))
    {
        //the compute shaders we have only support these 2 methods
        MASSERT(calcMethod == Goldelwalues::CheckSums ||
                calcMethod == Goldelwalues::Crc);
    }
    m_CalcMethod = calcMethod;
    return OK;
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetExpectedCheckMethod(Goldelwalues::Code calcMethod)
{
    m_CalcMethod = calcMethod;
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetCalcAlgorithmHint(Goldelwalues::CalcAlgorithm calcAlgoHint)
{
    // If GPU does not support required APIs to perform parallelism, then have
    // the algorithm callwlated with CPU
    if (calcAlgoHint == Goldelwalues::FragmentedGpuCalcAlgorithm &&
        !mglExtensionSupported ("GL_ARB_compute_variable_group_size"))
    {
        m_CalcAlgoHint = Goldelwalues::FragmentedCpuCalcAlgorithm;
    }
    else
    {
        m_CalcAlgoHint = calcAlgoHint;
    }
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetGoldensBinSize(UINT32 binSize)
{
    m_BinSize = binSize;
}

//------------------------------------------------------------------------------
const string & GLGoldenSurfaces::Name (int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());

    return m_Surfs[surfNum].Name;
}

void GLGoldenSurfaces::CopyLayeredBuffer(vector<UINT08> &dst, int surfNum)
{
    UINT08 *ptrBuf = &m_LayeredBuffer[0] + (surfNum % m_NumLayers) * m_CachedSurfaceSize;
    dst.reserve(m_CachedSurfaceSize);
    dst.assign( ptrBuf, ptrBuf + m_CachedSurfaceSize );
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::CacheLayeredSurface(int surfNum, bool forceCacheToCpu)
{
    RC rc;
    GLenum texDim = GL_TEXTURE_2D_ARRAY_EXT;
    glActiveTexture(GL_TEXTURE0_ARB);

    // The first 'm_NumLayers' are Z surfaces and the next 'm_NumLayers' are
    // color surfaces
    if (surfNum < m_NumLayers)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, m_DepthTextureID);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, m_ColorSurfaceID);
    }

    // Cache all layers while fetching the first layer
    if (surfNum % m_NumLayers == 0)
    {
        if (GetSurfCalcAlgorithm(surfNum) == Goldelwalues::FragmentedGpuCalcAlgorithm && !forceCacheToCpu)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PixelPackBuffer);
            glGetTexImage(texDim, 0, m_pCachedFmtInfo->ExtFormat, m_pCachedFmtInfo->Type, 0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        else
        {
            m_LayeredBuffer.resize(m_CachedSurfaceSize * m_NumLayers);
            glGetTexImage(texDim, 0, m_pCachedFmtInfo->ExtFormat, m_pCachedFmtInfo->Type,
                          &m_LayeredBuffer[0]);
        }
    }

    if (!IsSurfChkUsingFragAlgo(surfNum))
    {
        CopyLayeredBuffer(m_CacheMem, surfNum);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, 0);
    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::CacheFragGPUSurface(int surfNum)
{
    if (m_Surfs[surfNum].ReadPixelsBuffer == GL_ARRAY_BUFFER)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_Surfs[surfNum].BufferId);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, m_CachedAbsPitch * m_Surfs[surfNum].Height, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PixelPackBuffer);
        glReadBuffer(m_Surfs[surfNum].ReadPixelsBuffer);
        glReadPixels(0, 0, m_Surfs[surfNum].Width, m_Surfs[surfNum].Height, m_pCachedFmtInfo->ExtFormat, m_pCachedFmtInfo->Type, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::CacheSurface(int surfNum)
{
    m_CacheMem.resize(m_CachedSurfaceSize);

    if (m_Surfs[surfNum].ReadPixelsBuffer == GL_ARRAY_BUFFER)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_Surfs[surfNum].BufferId);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, m_CachedSurfaceSize, &m_CacheMem[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        glReadBuffer(m_Surfs[surfNum].ReadPixelsBuffer);
        glReadPixels(0, 0, m_Surfs[surfNum].Width, m_Surfs[surfNum].Height,
            m_pCachedFmtInfo->ExtFormat, m_pCachedFmtInfo->Type,
            &m_CacheMem[0]);
    }
}

//------------------------------------------------------------------------------
UINT32 GLGoldenSurfaces::CheckAndResetInjectError(int surfNum)
{
    if (surfNum == m_InjectErrorAtSurface)
    {
        m_InjectErrorAtSurface = -1;    //reset injecting errors
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::CheckSurfaceOnGPU(int surfNum)
{
    RC rc;
    // Surface check is done for all non-first layers during the first layer
    if (m_NumLayers && (surfNum % m_NumLayers != 0))
        return OK;

    UINT64 surfaceSize = m_CachedSurfaceSize;
    if (m_NumLayers > 0)
    {
        surfaceSize *= m_NumLayers;
    }

    auto &surf = m_Surfs[surfNum];
    UINT32 computeProgram = m_CalcMethod == Goldelwalues::Crc ?
                            m_ComputeCRCProgram : m_ComputeCheckSumProgram;
    glUseProgram(computeProgram);

    if (m_CalcMethod == Goldelwalues::Crc)
    {
        // It is not practical to use massive number of threads for CRC
        // processing as this would result in massive number of CRC values.
        // Select the same number of threads as there are "bins".
        UINT64 numPixels = surfaceSize / PixelBytes(m_StorageBufferFmt);
        glUniform1ui(glGetUniformLocation(computeProgram, "numPixels"),
            UNSIGNED_CAST(GLuint, numPixels));

        GLint maxWorkGroupSize;
        GLint maxNumWorkGroups;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWorkGroupSize);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxNumWorkGroups);
        surf.WorkGroupSize = maxWorkGroupSize;
        surf.NumWorkGroups = maxNumWorkGroups;
        if (surf.WorkGroupSize > 64)
        {
            surf.WorkGroupSize = 64;
        }
        surf.NumWorkGroups = 1 + (m_BinSize - 1) / surf.WorkGroupSize;
        if (surf.NumWorkGroups > maxNumWorkGroups)
        {
            Printf(Tee::PriError, "Too many required workgroups=%d vs %d "
                "possible for GPU CRC processing.\n", surf.NumWorkGroups,
                maxNumWorkGroups);
            return RC::SOFTWARE_ERROR;
        }
        if (static_cast<UINT32>(surf.WorkGroupSize) > m_BinSize)
        {
            surf.WorkGroupSize = m_BinSize;
        }
    }
    else
    {
        UINT64 workLoadPerWorkGroup = surfaceSize / (surf.NumWorkGroups *
                                      surf.WorkGroupSize *
                                      PixelBytes(m_StorageBufferFmt));
        glUniform1ui(glGetUniformLocation(computeProgram, "maxWorkLoad"),
            UNSIGNED_CAST(GLuint, workLoadPerWorkGroup));
    }

#ifdef INJECT_ERRORS
    glUniform1ui(glGetUniformLocation(computeProgram, "injectError"), CheckAndResetInjectError(surfNum));
#endif

    glBindTexture(GL_TEXTURE_BUFFER, m_TextureBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_StorageBuffer);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
    glDispatchComputeGroupSizeARB(surf.NumWorkGroups, 1, 1,
                                  surf.WorkGroupSize, 1, 1);
    CHECK_RC(mglGetRC());

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glMemoryBarrierEXT(GL_SHADER_STORAGE_BARRIER_BIT);

    // Extract Fragmented CRC/Checksum data from GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_StorageBuffer);
    UINT64 newSurfSize = surf.NumWorkGroups *
                         surf.WorkGroupSize *
                         m_BinSize * ColorUtils::PixelBytes(m_StorageBufferFmt);
    if (m_CalcMethod == Goldelwalues::Crc)
    {
        newSurfSize = m_BinSize * ColorUtils::PixelBytes(m_StorageBufferFmt);
    }
    m_CacheMem.resize(newSurfSize);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, newSurfSize, &m_CacheMem[0]);
    CHECK_RC(mglGetRC());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glUseProgram(0);
    return mglGetRC();
}

// This algorithm functions slightly differnetly than the GPU version, however
// it produces identical Checksum surfaces. This algorithm takes advantage of
// CPU caching by only writing once for every 0->workGroupSize reads. This
// algorithm performs about 20% better than the GPU version of the algoritmn,
// when run on the CPU.
void GLGoldenSurfaces::CheckSurfaceOnCPUThreadChecksum(void *args)
{
    Tasker::DetachThread detach;

    struct CpuFragmentedThreadArgs *tArgs = static_cast<struct CpuFragmentedThreadArgs*>(args);

    UINT32 tid = Tasker::GetThreadIdx();
    UINT32 readIndex = 0;
    UINT32 writeIndex = 0;
    for(int k = 0; k < tArgs->numWorkGroups; k++)
    {
        for(int j = 0; j < tArgs->workGroupSize; j++)
        {
            writeIndex = (k * tArgs->workGroupSize + j + (tArgs->step * tid)) * tArgs->pixelStride;
            UINT32 *pValOut = reinterpret_cast<UINT32 *>(&tArgs->newSurf[writeIndex]);

            for(unsigned int i = tid; i < tArgs->workLoadPerWorkGroup; i+=tArgs->binSize)
            {
                readIndex =  (k * tArgs->workGroupSize + j + (tArgs->step * i)) * tArgs->pixelStride;
                *pValOut += *reinterpret_cast<UINT32 *>(&tArgs->pSrcSurf[readIndex]);
            }
        }
        Tasker::Yield();
    }
}

#if defined(__amd64) || defined(__i386)
__attribute__((target("sse4.2")))
#endif
void GLGoldenSurfaces::CheckSurfaceOnCPUThreadCRC(void *args)
{
    Tasker::DetachThread detach;

    struct CpuFragmentedThreadArgs *tArgs = static_cast<struct CpuFragmentedThreadArgs*>(args);

    UINT32 tid = Tasker::GetThreadIdx();
    const UINT64 surfaceSize = tArgs->workLoadPerWorkGroup;
    const UINT32 step = tArgs->binSize * tArgs->pixelStride / sizeof(UINT32);

    // The outer loop is needed when there are less threads than bins.
    // "workGroupSize" holds number of threads.
    for (UINT32 binIdx = 0; (binIdx + tid) < tArgs->binSize; binIdx += tArgs->workGroupSize)
    {
        UINT32 crc = ~0U;
        UINT32 *data = reinterpret_cast<UINT32 *>(&tArgs->pSrcSurf[(binIdx + tid) * tArgs->pixelStride]);
        const UINT32 *dataEnd = reinterpret_cast<UINT32 *>(&tArgs->pSrcSurf[surfaceSize]);

        if (Crc32c::IsHwCrcSupported())
        {
            for (; data < dataEnd; data += step)
            {
                crc = CRC32C4(crc, *data);
            }
        }
        else
        {
            for (; data < dataEnd; data += step)
            {
                crc = Crc32c::StepSw(crc, *data);
            }
        }

        UINT32 *pValOut = reinterpret_cast<UINT32 *>(&tArgs->newSurf[(binIdx + tid) * sizeof(crc)]);
        *pValOut = crc;
    }
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::CheckSurfaceOnCPU(int surfNum)
{
    RC rc;
    MASSERT(m_CalcMethod == Goldelwalues::Crc || m_CalcMethod == Goldelwalues::CheckSums);

    UINT08 *pSrcSurf = NULL;
    UINT64 surfaceSize = m_CachedSurfaceSize;
    if (m_NumLayers > 0)
    {
        // If we're dealing with layered surfaces, we perform the CRC/CheckSum
        // callwlation only once across all the surfaces, during the first
        // surface run
        if (surfNum % m_NumLayers != 0)
            return rc;

        surfaceSize *= m_NumLayers;
        pSrcSurf = &m_LayeredBuffer[0];
    }
    else
    {
        pSrcSurf = &m_CacheMem[0];
    }

    UINT32 pixelStride = ColorUtils::PixelBytes(m_StorageBufferFmt);

#ifdef INJECT_ERRORS
    if (CheckAndResetInjectError(surfNum))
    {
        pSrcSurf[0x1234 * pixelStride]++;
    }
#endif

    UINT32 step = m_Surfs[surfNum].NumWorkGroups * m_Surfs[surfNum].WorkGroupSize;
    UINT64 workLoadPerWorkGroup = surfaceSize / (step * pixelStride);

    size_t newSurfSize = step * m_BinSize * pixelStride;
    if (m_CalcMethod == Goldelwalues::Crc)
    {
        workLoadPerWorkGroup = surfaceSize;
        newSurfSize = m_BinSize * sizeof(UINT32);
        Crc32c::GenerateCrc32cTable();
    }
    vector<UINT08> newSurf(newSurfSize, 0);

    struct CpuFragmentedThreadArgs tARgs;
    tARgs.numWorkGroups = m_Surfs[surfNum].NumWorkGroups;
    tARgs.workGroupSize = m_Surfs[surfNum].WorkGroupSize;
    if (m_CalcMethod == Goldelwalues::Crc)
    {
        // This value tells how many threads are used:
        tARgs.workGroupSize = 4;
    }
    tARgs.step = step;
    tARgs.binSize = m_BinSize;
    tARgs.newSurf = &newSurf[0];
    tARgs.workLoadPerWorkGroup = UNSIGNED_CAST(UINT32, workLoadPerWorkGroup);
    tARgs.pSrcSurf = pSrcSurf;
    tARgs.pixelStride = pixelStride;
    tARgs.calcMethod = m_CalcMethod;

    vector<Tasker::ThreadID> threads =
            Tasker::CreateThreadGroup
            (
                (m_CalcMethod == Goldelwalues::Crc ? GLGoldenSurfaces::CheckSurfaceOnCPUThreadCRC
                                                   : GLGoldenSurfaces::CheckSurfaceOnCPUThreadChecksum),
                &tARgs,
                (m_CalcMethod == Goldelwalues::Crc ? tARgs.workGroupSize
                                                   : m_BinSize),
                "CPU Fragmented Thread",
                true,
                nullptr
            );
    rc = Tasker::Join(threads);

    m_CacheMem.swap(newSurf);

    return rc;
}

bool GLGoldenSurfaces::StoreCachedSurfaceToBuffer(int surfNum, vector<UINT08> *buf)
{
    bool ret = false;
    if (!buf)
    {
        return ret;
    }

    if (m_NumLayers > 0)
    {
        // In the case of GPU Fragmented algorithm, we first need to extract
        // the surface data from the GPU
        if (GetSurfCalcAlgorithm(surfNum) == Goldelwalues::FragmentedGpuCalcAlgorithm)
        {
            CacheLayeredSurface(surfNum, true);
        }

        CopyLayeredBuffer(*buf, surfNum);
    }
    else
    {
        // In the case of GPU Fragmented algorithm, we first need to extract
        // the surface data from the GPU
        if (GetSurfCalcAlgorithm(surfNum) == Goldelwalues::FragmentedGpuCalcAlgorithm)
        {
            CacheSurface(surfNum);
        }

        *buf = m_CacheMem;
    }

    ret = true;
    return ret;
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::GetSurfaceForGoldelwals
(
    int surfNum,
    vector<UINT08> *surfDumpBuffer
)
{
    RC rc = OK;
    MASSERT(surfNum < NumSurfaces());

    m_pCachedFmtInfo = mglGetTexFmtInfo(m_Surfs[surfNum].HwFormat);
    if (!m_pCachedFmtInfo)
    {
        return RC::UNSUPPORTED_COLORFORMAT;
    }

    m_CachedAbsPitch = m_Surfs[surfNum].Width *
                       ColorUtils::PixelBytes(m_pCachedFmtInfo->ExtColorFormat);
    m_LwrCachedSurface = surfNum;
    m_CachedSurfaceSize = m_CachedAbsPitch * m_Surfs[surfNum].Height;

    if (m_NumLayers > 0)
    {
        CacheLayeredSurface(surfNum, false);
    }
    else if ( GetSurfCalcAlgorithm(surfNum) == Goldelwalues::FragmentedGpuCalcAlgorithm )
    {
        CacheFragGPUSurface(surfNum);
    }
    else
    {
        CacheSurface(surfNum);
    }

    // Store Cached Surface Data into Dump Buffer if one was provided
    StoreCachedSurfaceToBuffer(surfNum, surfDumpBuffer);

    // CRC/Checksum the surface on GPU and reduce it further
    switch (GetSurfCalcAlgorithm(surfNum))
    {
        case Goldelwalues::FragmentedGpuCalcAlgorithm:
            CHECK_RC(CheckSurfaceOnGPU(surfNum));
            break;

        case Goldelwalues::FragmentedCpuCalcAlgorithm:
            CHECK_RC(CheckSurfaceOnCPU(surfNum));
            break;

        default:
            break;
    }

#ifdef INJECT_ERRORS
    if (!IsSurfChkUsingFragAlgo(surfNum) && CheckAndResetInjectError(surfNum))
    {
        // Inject error at random location
        m_CacheMem[0x123] += 1;
    }
#endif

    return rc;
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::CheckAndReportDmaErrors(UINT32 subdev)
{
    // Re-read back the current cached surface and compare with the copy
    // already in m_CacheMem.
    // We should use a different HW mechanism than the original one, i.e.
    // CPU reads of BAR1 vs. a DMA operation, if possible.
    // Return OK if surfaces match, RC::MEM_TO_MEM_RESULT_NOT_MATCH otherwise.

    // @@@ TBD
    return OK;
}

//------------------------------------------------------------------------------
void * GLGoldenSurfaces::GetCachedAddress
(
    int surfNum,            // requested surface
    Goldelwalues::BufferFetchHint /*bufFetchHint*/, //!< Not used
    UINT32 subdevNum,       //
    vector<UINT08> *surfDumpBuffer
)
{
    RC rc;
    MASSERT(surfNum < NumSurfaces());
    if (surfNum != m_LwrCachedSurface)
        rc = GetSurfaceForGoldelwals(surfNum, surfDumpBuffer);

    if (OK == rc)
        return &m_CacheMem[0];

    return NULL;
}

void GLGoldenSurfaces::Ilwalidate()
{
    m_LwrCachedSurface = NO_CACHED_SURFACE;
}
//------------------------------------------------------------------------------
INT32 GLGoldenSurfaces::Pitch(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    MASSERT(surfNum == m_LwrCachedSurface);

    return -m_CachedAbsPitch;  // Negative pitch, because GL is Y reversed.
}

//------------------------------------------------------------------------------
Goldelwalues::CalcAlgorithm GLGoldenSurfaces::GetSurfCalcAlgorithm(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());

    Goldelwalues::CalcAlgorithm ret = m_Surfs[surfNum].CalcAlgorithm;

    // Force CPU Fragmented if surface is using GPU Fragmented and Optimization
    // has been reduced
    if(m_ReduceOptimization && ret == Goldelwalues::FragmentedGpuCalcAlgorithm)
    {
        ret = Goldelwalues::FragmentedCpuCalcAlgorithm;
    }

    return ret;
}
bool GLGoldenSurfaces::IsSurfChkUsingFragAlgo(int surfNum) const
{
    Goldelwalues::CalcAlgorithm algo = GetSurfCalcAlgorithm(surfNum);
    if (algo == Goldelwalues::FragmentedGpuCalcAlgorithm ||
        algo == Goldelwalues::FragmentedCpuCalcAlgorithm )
    {
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
UINT32 GLGoldenSurfaces::BufferId(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_Surfs[surfNum].BufferId;
}
//------------------------------------------------------------------------------
UINT32 GLGoldenSurfaces::Width(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_Surfs[surfNum].Width;
}

//------------------------------------------------------------------------------
UINT32 GLGoldenSurfaces::Height(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_Surfs[surfNum].Height;
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::GetModifiedSurfProp
(
    int surfNum,
    UINT32 *Width,
    UINT32 *Height,
    UINT32 *Pitch
)
{
    MASSERT(Width);
    MASSERT(Height);
    MASSERT(Pitch);
    if (IsSurfChkUsingFragAlgo(surfNum))
    {
        //Get the size of surface that was written in the shader
        UINT32 size = m_Surfs[surfNum].NumWorkGroups *
                      m_Surfs[surfNum].WorkGroupSize *
                      ColorUtils::PixelBytes(m_StorageBufferFmt);
        MASSERT(size % (ColorUtils::PixelBytes(m_Surfs[surfNum].HwFormat)) == 0);
        *Width = size / ColorUtils::PixelBytes(m_Surfs[surfNum].HwFormat);
        *Height = m_BinSize;
        *Pitch = (*Width) * ColorUtils::PixelBytes(m_Surfs[surfNum].HwFormat);
    }
    return OK;
}

//------------------------------------------------------------------------------
ColorUtils::Format GLGoldenSurfaces::Format(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_Surfs[surfNum].ReadbackFormat;
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetNumLayeredSurfaces(int numLayers)
{
    m_NumLayers = numLayers;
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetSurfaceTextureIDs(UINT32 depthTextureID, UINT32 colorSurfaceID)
{
    m_DepthTextureID = depthTextureID;
    m_ColorSurfaceID = colorSurfaceID;
}

//------------------------------------------------------------------------------
UINT32 GLGoldenSurfaces::Display(int surfNum) const
{
    MASSERT(surfNum < NumSurfaces());
    return m_Surfs[surfNum].Displaymask;
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::DescribeSurfaces
(
    UINT32 width,
    UINT32 height,
    ColorUtils::Format colorFmt,
    ColorUtils::Format zFmt,
    UINT32 displayMask
)
{
    GLenum lwrReadPixelsBuffer = GL_FRONT;
    if (m_Surfs.size())
    {
        // Discard old surfaces info, but retain the current ReadPixelsBuffer.
        lwrReadPixelsBuffer = m_Surfs[0].ReadPixelsBuffer;
        m_Surfs.clear();
    }

    if (displayMask & (displayMask-1))
    {
        UINT32 newMask = displayMask & ~(displayMask ^ (displayMask-1));

        Printf(Tee::PriNormal, "Multiple displays selected (0x%x), forcing just one (0x%x)\n",
                displayMask, newMask);
        displayMask = newMask;
    }

    vector<Buffer> bufs;
    bufs.push_back(Buffer("Z", zFmt, 0)); // We do any DAC CRC testing only for C0.
    bufs.push_back(Buffer("C", colorFmt, displayMask));

    if (m_NumLayers > 0)    //Handle layered surfaces as special case
    {
        bufs.clear();
        for(INT32 i = 0; i < m_NumLayers; i++)
        {
            bufs.push_back(Buffer(Utility::StrPrintf("ZL%d", i), zFmt, 0));
        }
        for(INT32 i = 0; i < m_NumLayers; i++)
        {
            bufs.push_back(Buffer(Utility::StrPrintf("CL%d", i), colorFmt, displayMask));
        }
    }

    // By convention, we check Z surfaces before C
    for (UINT32 i = 0; i < bufs.size(); i++)
    {
        if (ColorUtils::LWFMT_NONE != bufs[i].fmt)
        {
            AddSurf(bufs[i].name
                    ,bufs[i].fmt
                    ,bufs[i].displayMask
                    ,lwrReadPixelsBuffer
                    ,0
                    ,width
                    ,height
                   );
        }
    }
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::AddSurf
(
    string name
    ,ColorUtils::Format hwformat
    ,UINT32 displayMask
    ,GLenum attachment
    ,GLint bufferId
    ,UINT32 width
    ,UINT32 height
)
{
    const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(hwformat);

    Surf s;
    s.Name             = name;
    s.HwFormat         = hwformat;
    s.ReadbackFormat   = pFmtInfo->ExtColorFormat;
    s.Displaymask      = displayMask;
    s.ReadPixelsBuffer = attachment;
    s.BufferId         = bufferId;
    s.Width            = width;
    s.Height           = height;
    s.CalcAlgorithm    = m_CalcAlgoHint;

    // Determine if it is even possible to do GPU fragmented surface checking
    // for this particular surface, if not default to usual CPU method
    if (s.CalcAlgorithm > Goldelwalues::CpuCalcAlgorithm)
    {
        UINT32 numLayers = m_NumLayers > 0 ? m_NumLayers : 1;
        UINT64 surfaceSize = s.Width * s.Height *
                         ColorUtils::PixelBytes(s.HwFormat)* numLayers;

        GLint workGroupSize;
        GLint numWorkGroups;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  0, &workGroupSize);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &numWorkGroups);
        s.WorkGroupSize = workGroupSize;
        s.NumWorkGroups = numWorkGroups;

        // We can launch the compute shader with any number of workgroups and
        // workgroup size, 24x64 was found experimentally to take least time
        // on GM20x and is quite consistent across other families.
        if (s.WorkGroupSize >=64 && s.NumWorkGroups >= 24)
        {
            s.NumWorkGroups = 24;
            s.WorkGroupSize = 64;

            // Three conditions that must be met for a checksum surface check on GPU:
            // 1. The new surface size returned by GPU must be smaller than the
            // original surface.
            // 2. The original surface must be divisible by the new surface, if
            // not this will mean unequal workload among shaders and will lead
            // to conditional exelwtion.
            // 3. The new surface size must be a factor of the pixel format of
            // original surface, if not, we will not be able to impersonate the
            // new surface as old type.
            if (m_CalcMethod == Goldelwalues::CheckSums)
            {
                UINT64 shaderSurfSize = s.NumWorkGroups * s.WorkGroupSize *
                    ColorUtils::PixelBytes(m_StorageBufferFmt);

                if ((surfaceSize <= (shaderSurfSize * m_BinSize)) ||
                   (surfaceSize % shaderSurfSize != 0) ||
                   (shaderSurfSize % ColorUtils::PixelBytes(s.HwFormat)) != 0)
                {
                    s.CalcAlgorithm = Goldelwalues::CpuCalcAlgorithm;
                }
            }
        }
    }

    // Set non-fragmented callwlation algorithm
    if (s.CalcAlgorithm == Goldelwalues::CpuCalcAlgorithm)
    {
        s.NumWorkGroups = 0;
        s.WorkGroupSize = 0;
        if (m_CalcAlgoHint > Goldelwalues::CpuCalcAlgorithm)
        {
            Printf(Tee::PriLow,"Warning: Surface %u: Requested fragmented "
             "surface check, but defaulting to non-fragmented algorithm due "
             "to current surface's limitations.\n", static_cast<UINT32>(m_Surfs.size()));
        }
    }
    m_Surfs.push_back(s);
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::AddMrtColorSurface
(
    ColorUtils::Format colorFmt,    //!< Mods color format of the surface.
    GLenum             attachment,  //!< GL_COLOR_ATTACHMENT1_EXT or greater.
    UINT32             width,
    UINT32             height
)
{
    // DescribeSurfaces should have been called first.
    MASSERT(m_Surfs.size());

    // We expect that we will have Z, C0, C1, etc. in that order.
    MASSERT(attachment - GL_COLOR_ATTACHMENT0_EXT == m_Surfs.size() - 1);

    MASSERT(colorFmt != ColorUtils::LWFMT_NONE);

    // If this is the first MRT surface, go back and update Z and C0.
    if (m_Surfs.size() == 2)
    {
        m_Surfs[0].ReadPixelsBuffer = GL_COLOR_ATTACHMENT0_EXT;
        m_Surfs[1].ReadPixelsBuffer = GL_COLOR_ATTACHMENT0_EXT;
        m_Surfs[1].Name.append(1, '0');  // "C" to "C0".
    }

    string s = "C";
    s.append(1, '0' + attachment - GL_COLOR_ATTACHMENT0_EXT);
    AddSurf(s
            ,colorFmt
            ,0      // We do any DAC CRC testing only for C0.
            ,attachment
            ,0      // bufferId not required
            ,width
            ,height
            );
}

//------------------------------------------------------------------------------
RC GLGoldenSurfaces::CreateSurfCheckShader()
{
    RC rc;

    // continue only if at least 1 of the surfaces is using FragmentedGpuCalcAlgorithm
    // and the hint is to use FragmentedGpuCalcAlgorithm
    bool bContinue = false;
    for (UINT32 i = 0; i < m_Surfs.size() && !bContinue; i++)
    {
        bContinue = (GetSurfCalcAlgorithm(i) == Goldelwalues::FragmentedGpuCalcAlgorithm);
    }

    if (!bContinue || m_CalcAlgoHint != Goldelwalues::FragmentedGpuCalcAlgorithm)
    {
        return OK;
    }

    // Shader Checksum/CRC algorithm:

    // Lets say we have to Checksum/CRC a surface of size 1024*1024 bytes. A
    // Compute shader is launched with size : (4,1,1, 128,1,1), (see the API
    // glDispatchComputeGroupSizeARB for details). Since we have 512 threads,
    // the total workload for each thread is 1024*1024/512 = 2048 bytes.
    // Each thread Checksums/CRCs its 2048 bytes of data and writes the result
    // as a unsigned int(4 bytes) into another surface.
    // We now have a smaller surface of size 512*4 bytes that is returned back
    // to host to run Checksum/CRC on CPU.

    // create checksum compute shader
    GLuint computeChksumShaderID = 0;
    string Chksumshader =
        "#version 430\n"
        "#extension GL_ARB_compute_variable_group_size : enable\n"
        "layout(local_size_variable) in;\n"

        "uniform usamplerBuffer ubuffer;\n"
        "uniform unsigned int maxWorkLoad;\n"
        "uniform unsigned int binSize;\n"
#ifdef INJECT_ERRORS
        "uniform unsigned int injectError;\n"
#endif
        "layout(std430, binding = 0) buffer crcData\n"
        "{\n"
        "    uint data[];\n"
        "};\n"

        "void main()\n"
        "{\n"
        "    uvec4 texel;\n"
        "    int readIndex, writeIndex;\n"
        "    for (unsigned int i = 0 ; i < maxWorkLoad; i++)\n"
        "    {\n"
        "        readIndex = int(gl_GlobalIlwocationID.x + (gl_LocalGroupSizeARB.x * gl_NumWorkGroups.x * i));\n"
        "        writeIndex = int(gl_GlobalIlwocationID.x + (gl_LocalGroupSizeARB.x * gl_NumWorkGroups.x * (i%binSize)));\n"
        "        texel = texelFetch(ubuffer, readIndex);\n"
#ifdef INJECT_ERRORS
        "        //inject Error at random location\n"
        "        if(readIndex == 0x1234 && injectError != 0)\n"
        "        {\n"
        "           texel.x += 1;\n"
        "           texel.x %= 0xFF;\n"
        "        }\n"
#endif
        "        data[writeIndex] += (texel.x + (texel.y<<8) + (texel.z<<16) + (texel.w<<24));\n"
        "    }\n"
        "}\n";

    CHECK_RC(ModsGL::LoadShader(GL_COMPUTE_SHADER, &computeChksumShaderID, Chksumshader.c_str()));
    if (m_ComputeCheckSumProgram)
    {
        glDeleteProgram(m_ComputeCheckSumProgram);
    }

    m_ComputeCheckSumProgram = glCreateProgram();
    glAttachShader(m_ComputeCheckSumProgram, computeChksumShaderID);
    CHECK_RC(ModsGL::LinkProgram(m_ComputeCheckSumProgram));
    glDetachShader(m_ComputeCheckSumProgram, computeChksumShaderID);
    glDeleteShader(computeChksumShaderID);

    // create CRC compute shader
    GLuint computeCRCShaderID = 0;
    string CRCshader =
        "#version 430\n"
        "#extension GL_ARB_compute_variable_group_size : enable\n"
        "layout(local_size_variable) in;\n"

        "uniform usamplerBuffer ubuffer;\n"
        "uniform unsigned int numPixels;\n"
        "uniform unsigned int binSize;\n"
#ifdef INJECT_ERRORS
        "uniform unsigned int injectError;\n"
#endif
        "uniform unsigned int crcTable[256];\n"

        "layout(std430, binding = 0) buffer crcData\n"
        "{\n"
        "    uint data[];\n"
        "};\n"

        "void main()\n"
        "{\n"
        "    const int step = int(binSize);\n"
        "    unsigned int crc = ~0U;\n"
        "    uvec4 texel;\n"
        "    if (gl_GlobalIlwocationID.x >= binSize)\n"
        "    {\n"
        "        return;\n"
        "    }\n"
        "    for (int readIndex = int(gl_GlobalIlwocationID.x); readIndex < numPixels; readIndex += step)\n"
        "    {\n"
        "        texel = texelFetch(ubuffer, readIndex);\n"
#ifdef INJECT_ERRORS
        "        //inject Error at random location\n"
        "        if (readIndex == 0x1234 && injectError != 0)\n"
        "        {\n"
        "           texel.x += 1;\n"
        "           texel.x %= 0xFF;\n"
        "        }\n"
#endif
        "        crc = crcTable[(crc ^ texel.x) & 0xFF] ^ (crc >> 8);\n"
        "        crc = crcTable[(crc ^ texel.y) & 0xFF] ^ (crc >> 8);\n"
        "        crc = crcTable[(crc ^ texel.z) & 0xFF] ^ (crc >> 8);\n"
        "        crc = crcTable[(crc ^ texel.w) & 0xFF] ^ (crc >> 8);\n"
        "    }\n"
        "    data[gl_GlobalIlwocationID.x] = crc;\n"
        "}\n";

    CHECK_RC(ModsGL::LoadShader(GL_COMPUTE_SHADER, &computeCRCShaderID, CRCshader.c_str()));
    if (m_ComputeCRCProgram)
    {
        glDeleteProgram(m_ComputeCRCProgram);
    }

    m_ComputeCRCProgram = glCreateProgram();
    glAttachShader(m_ComputeCRCProgram, computeCRCShaderID);
    CHECK_RC(ModsGL::LinkProgram(m_ComputeCRCProgram));
    glDetachShader(m_ComputeCRCProgram, computeCRCShaderID);
    glDeleteShader(computeCRCShaderID);

    //create buffers/textures
    UINT64 maxSurfaceSize = 0;
    UINT64 maxShaderSurfSize = 0;
    for (UINT32 i = 0; i < m_Surfs.size(); i++)
    {
        if (GetSurfCalcAlgorithm(i) != Goldelwalues::FragmentedGpuCalcAlgorithm)
        {
            continue;
        }
        UINT32 numLayers = m_NumLayers > 0 ? m_NumLayers : 1;
        UINT64 surfaceSize = m_Surfs[i].Width * m_Surfs[i].Height *
                         ColorUtils::PixelBytes(m_Surfs[i].HwFormat)* numLayers;
        if (surfaceSize > maxSurfaceSize)
            maxSurfaceSize = surfaceSize;

        UINT64 checksumShaderSurfSize = m_Surfs[i].NumWorkGroups * m_Surfs[i].WorkGroupSize *
                                ColorUtils::PixelBytes(m_StorageBufferFmt) *
                                m_BinSize;
        if (checksumShaderSurfSize > maxShaderSurfSize)
            maxShaderSurfSize = checksumShaderSurfSize;

        UINT64 crcShaderSurfSize = m_BinSize * sizeof(UINT32);
        if (crcShaderSurfSize > maxShaderSurfSize)
            maxShaderSurfSize = crcShaderSurfSize;
    }

    glGenBuffers(1, &m_PixelPackBuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PixelPackBuffer);
    glBufferData(GL_PIXEL_PACK_BUFFER, maxSurfaceSize, NULL, GL_DYNAMIC_READ);

    glGenTextures(1, &m_TextureBuffer);
    glBindTexture(GL_TEXTURE_BUFFER, m_TextureBuffer);

    GLint maxTexBufferSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE_ARB, &maxTexBufferSize);
    MASSERT(static_cast<GLint>(maxSurfaceSize) <= maxTexBufferSize);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8UI, m_PixelPackBuffer);    //Use data from the pixel pack buffer
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    glGenBuffers(1, &m_StorageBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_StorageBuffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, maxShaderSurfSize, NULL, GL_MAP_READ_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glUseProgram(m_ComputeCheckSumProgram);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(m_ComputeCheckSumProgram, "ubuffer"), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_StorageBuffer); //use binding point 0
    glUniform1ui(glGetUniformLocation(m_ComputeCheckSumProgram, "binSize"), m_BinSize);
    CHECK_RC(mglGetRC());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glUseProgram(m_ComputeCRCProgram);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(m_ComputeCRCProgram, "ubuffer"), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_StorageBuffer); //use binding point 0
    glUniform1ui(glGetUniformLocation(m_ComputeCRCProgram, "binSize"), m_BinSize);
    CHECK_RC(mglGetRC());

    GLint maxUniforms = 0;
    glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &maxUniforms);
    MASSERT(maxUniforms > 256);

    Crc32c::GenerateCrc32cTable();
    glUniform1uiv(glGetUniformLocation(m_ComputeCRCProgram, "crcTable"), 256, &Crc32c::s_Crc32cTable[0]);
    glUseProgram(0);
    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLGoldenSurfaces::SetReadPixelsBuffer
(
    GLenum frontOrBackOrAttachment
)
{
    // In case of a layered surface, all Z and C surfaces read from attachment 0
    // otherwise, we only need to set Z and C0, any extra color surfaces will
    // use the attachment point that was initially set by "AddMrtColorSurface"
    size_t maxSurfaces = m_NumLayers ? 2 * m_NumLayers :
                          min(static_cast<size_t>(2), m_Surfs.size());
    for (size_t surfNum = 0; surfNum < maxSurfaces; surfNum++)
    {
        m_Surfs[surfNum].ReadPixelsBuffer = frontOrBackOrAttachment;
    }
}

#ifdef DEBUG_SHADERS2
//-----------------------------------------------------------------------------
// Very useful for debugging the shaders below. Just copy the shader text to
// test.txt without the double quotes and replace the call to mglLoadProgram()
// with this one. Then you don't have to keep recompiling the source when you
// change this shader.
//-----------------------------------------------------------------------------
static void LoadDebugPgm(string &pgm)
{
    FILE * pFile = fopen("test.txt", "r+");
    char buf[150];
    while (!feof(pFile))
    {
        fgets(buf,sizeof(buf),pFile);
        if( buf[0] == '/' )
            continue;
        string sztemp(buf);
        pgm += sztemp;
    }
    fclose(pFile);
}
#endif

/*******************************************************************************
This shader simply colwerts the vertex position from object to eye coordinates.
*******************************************************************************/
string VxCksumPgm =
    "!!LWvp4.0\n"
    "#VxCksumPgm\n"
    "main:\n"
    " PARAM pgmElw[256] = { program.elw[0..255] };\n"
    " DP4 result.position.x, pgmElw[0], vertex.position;\n"
    " DP4 result.position.y, pgmElw[1], vertex.position;\n"
    " DP4 result.position.z, pgmElw[2], vertex.position;\n"
    " DP4 result.position.w, pgmElw[3], vertex.position;\n"
    "END\n";

/*******************************************************************************
This shader callwlates the crc of the write SBO.\n"
The addresses for each ShaderBufferObject(SBO) must be stored in \n"
the local program environment as follows:\n"
Note: All Address's are Gpu (not CPU) address's.
program.local[0].xy= SboDataAddress:  x=LSW,y=MSW\n"
program.local[0].zw= CrcTableAddress: x=LSW,y=MSW\n"
program.local[1].xy= SemaphoreAddress:x=LSW,y=MSW\n"
program.local[1].zw= CrcBinAddress:   x=LSW,y=MSW\n"
program.local[2]   = sbo parameters:
               .x=width, .y=height, .z=colorfmt, .w=num_bins
program.local[3]   = crc parameters:
               .x=polynomial, .y=crc table size (in bytes)
               .z=unused,     .w=unused

*******************************************************************************/
string FrCrcPgm =
"!!LWfp5.0\n"
"#Confirmed this shader produces the same CRCs as the cpu for format RU32.\n"
"#FrCksumPgm \n"
"OPTION LW_gpu_program_fp64; \n"
"OPTION LW_shader_buffer_load;\n"
"PARAM Addr0   = program.local[0];   #.xy=SboData .zw=CrcTable\n"
"PARAM Addr1   = program.local[1];   #.xy=Semaphore .zw=CrcBins\n"
"PARAM SboParm = program.local[2];   #.x=Width .y=Height \n"
"                                    #.z=colorfmt .w=numbins\n"
"PARAM CrcParm = program.local[3];   #.x=polynomial .y=crc table size\n"
"                                    #.z=unused, .w=unused\n"
"TEMP Bytes;\n"
"TEMP Value;\n"
"TEMP Crc;\n"
"TEMP Pos; \n"
"TEMP Garbage;\n"
"LONG TEMP SboAddr;                  #.x=SboData, .y=CrcTable, \n"
"                                    #.z=Semaphore, .w=CrcBins\n"
"LONG TEMP Addr;                     #.x = Start, .y=End\n"
"CrcTable:                           #Compute a CRC-32 remainder table\n"
" TEMP dividend;\n"
" TEMP remainder;\n"
" TEMP count;\n"
" TEMP bit;\n"
" LONG TEMP crcAddr;\n"
" MOV.U64 crcAddr.x, SboAddr.y;          #get a copy of the crc addr.\n"
" MOV.U dividend, {0,0,0,0};\n"
" MOV.S count.x, CrcParm.y;              #sizeof crcTable\n"
" REP.U count.x;\n"
"  SHL.U remainder.x, dividend.x, 24;\n"
"  MOV.U bit.x, 8;\n"
"  REP.U bit.x;\n"
"   AND.U.CC0 remainder.y, remainder.x, 0x80000000;\n"
"   IF NE0.y;\n"
"    SHL.U remainder.x, remainder.x,1;\n"
"    XOR.U remainder.x, remainder.x, CrcParm.x;\n"
"   ELSE;\n"
"    SHL.U remainder.x, remainder.x,1;\n"
"   ENDIF;\n"
"  ENDREP;\n"
"  STORE.U32 remainder.x, crcAddr.x;             #store the CrcValue \n"
"  ADD.U64 crcAddr.x, crcAddr.x, 4;              #next entry\n"
"  ADD.U32 dividend.x, dividend.x, 1;            #next remainder\n"
" ENDREP;\n"
"RET;\n"

"Crc8Bits:  \n"
" TEMP index;\n"
" TEMP data;\n"
" SHR.U index.x, Crc.x, 24;               # = crc >> 24\n"
" XOR.U index.x, index.x, Bytes.x;        # = b ^ (crc >> 24)\n"
" MAD.U64 Addr.z, index.x, 4, SboAddr.y;  # = offset into CrcTable[]\n"
" LOAD.U32 data.x, Addr.z;                # = CrcTable[b^(crc>>24)] \n"
" SHL.U index.x, Crc.x, 8;                # = (crc << 8) \n"
" XOR.U Crc.x,  data.x, index.x;          # = CrcTable[b^(crc>>24)]^(crc <<8)\n"
"RET;\n"

"Crc32Bits:\n"
" LOAD.U32 Value.x, Addr.x;\n"
" BFE.U Bytes.x, {8,0,0,0}, Value.x;\n"
" CAL Crc8Bits;\n"
" BFE.U Bytes.x, {8,8,0,0}, Value.x;\n"
" CAL Crc8Bits;\n"
" BFE.U Bytes.x, {8,16,0,0}, Value.x;\n"
" CAL Crc8Bits;\n"
" BFE.U Bytes.x, {8,24,0,0}, Value.x;\n"
" CAL Crc8Bits;\n"
"RET;\n"

"CrcBinX:\n"
" TEMP offset;\n"
" MUL.S32 offset.x, SboParm.w, 4;         # = num_bins * 32 bits\n"
" MAD.U64 Addr.x, Pos.x, 4, SboAddr.x;    # = start addr of this bin's crc data\n"
" MUL.U64 Addr.y, SboParm.x, SboParm.y;   # = start +(sboWidth*sboHeight*32bytes)\n"
" MAD.U64 Addr.y, Addr.y, 32, SboAddr.x;  # = end of data.\n"
" MOV.U Crc.x, 0xFFFFFFFF;\n"
" REP;\n"
" SUB.S64.CC0 Garbage.x, Addr.y, Addr.x;\n"
" BRK (LE0.x);\n"
"  CAL Crc32Bits;\n"
"  ADD.U64 Addr.x, Addr.x, offset.x;      # move to this bin's next data\n"
" ENDREP; \n"
" XOR.U32 Crc.x, Crc.x, 0xFFFFFFFF;\n"
" MEMBAR;\n"
" STORE.U32 Crc.x, SboAddr.w;             # write this bin's cksum\n"
"RET;\n"

"main:\n"
" PK64.U SboAddr.xy, Addr0;                        # .x=SboData, .y=CrcTable\n"
" PK64.U SboAddr.zw, Addr1;                        # .z=Semaphores,.w=CrcBins\n"
" SUB.U.CC0 Garbage, CrcParm, {0,0,1,0};\n"
" IF EQ0.z;                                            # if action=GenCrcTable\n"
"  ATOM.CSWAP.U32.CC0 Garbage.x, {0,1,0,0}, SboAddr.y; #\n"
"  CAL CrcTable (EQ0.x);\n"
" ENDIF;\n"
" SUB.U.CC0 Garbage, CrcParm, {0,0,2,0}; \n"
" IF EQ0.z;                                             # if action = RunCrc\n"
"  CVT.U32.F32.FLR Pos, fragment.position;              # colwert to int\n"
"  MOD Pos.x, Pos.x, SboParm.w;                         # = 0 - (NumBins-1)\n"
"  MAD.U64 SboAddr.z, Pos.x, 4, SboAddr.z;              # get bin's sem offset\n"
"  MAD.U64 SboAddr.w, Pos.x, 4, SboAddr.w;              # get bin's crc offset\n"
"  ATOM.CSWAP.U32.CC0 Garbage.x, {0,1,0,0}, SboAddr.z;  # if we take the \n"
"  CAL CrcBinX (EQ0.x);                                 # sem call CrcBinX\n"
" ENDIF; \n"
" KIL {-1,-1,-1,-1}; \n"
"END\n";

/*******************************************************************************
This shader callwlates the checksum of the write SBO.\n"
The addresses for each ShaderBufferObject(SBO) must be stored in \n"
the local program environment as follows:\n"
program.local[0].xy= ReadSboAddress:   x=LSW,y=MSW\n"
program.local[0].zw= unused:           x=LSW,y=MSW\n"
program.local[1].xy= SemaphoresAddress:x=LSW,y=MSW\n"
program.local[1].zw= CksumBinsAddress: x=LSW,y=MSW\n"
program.local[2]   = sbo parameters:
               .x=width, .y=height, .z=colorfmt, .w=num_bins
*******************************************************************************/
string FrCksumPgm =
"!!LWfp5.0\n"
"#FrCksumPgm\n"
"OPTION LW_gpu_program_fp64;\n"
"OPTION LW_shader_buffer_load;\n"
"PARAM Addr0   = program.local[0];   #.xy=SboData .zw=CrcTable\n"
"PARAM Addr1   = program.local[1];   #.xy=Semaphore .zw=CksumBins\n"
"PARAM SboParm = program.local[2];   #.x=Width .y=Height \n"
"                                    #.z=colorfmt .w=numbins \n"
"TEMP Value;\n"
"TEMP Pos;\n"
"TEMP Garbage;\n"
"LONG TEMP SboAddr;\n"
"LONG TEMP Addr;                     #.x = Start, .y=End\n"
"TEMP Cksum;\n"
"# Each element is 256bits(32bytes) however we need to match the \n"
"# CPU's checksum algorithm that is based on 32bit values.\n"

"CksumBinX:\n"
" TEMP offset;\n"
" MUL.S32 offset.x, SboParm.w, 4;         # offset.x = num_bins * 32 bits\n"
" MAD.U64 Addr.x, Pos.x, 4, SboAddr.x;    # Addr.x = this bin's chksum data\n"
" MUL.U64 Addr.y, SboParm.x, SboParm.y;   # Addr.y = +(sboWidth*sboHeight*32)\n"
" MAD.U64 Addr.y, Addr.y, 32, SboAddr.x;  # Addr.y = end of data.\n"
#ifdef DEBUG_SHADERS
  " MOV.U Cksum.x, 0x1234;\n"
  " STORE.U32 Cksum.x, SboAddr.w;           # write this bin's cksum\n"
#endif
" MOV.U Cksum.x, 0x0;\n"
" REP;\n"
"  SUB.S64.CC0 Garbage.x, Addr.y, Addr.x;\n"
"  BRK (LE0.x);\n"
"  CAL Cksum32Bits;\n"
"  ADD.U64 Addr.x, Addr.x, offset.x;      # move to this bin's next data\n"
" ENDREP;\n"
" MEMBAR;\n"
" STORE.U32 Cksum.x, SboAddr.w;           # write this bin's cksum\n"
#ifdef DEBUG_SHADERS
" ATOM.CSWAP.U32.CC0 Garbage.x, {1,2,0,0}, SboAddr.z;\n" // debug
#endif
"RET;\n"

"Cksum32Bits:\n"
" LOAD.U32 Value, Addr.x;\n"
" ADD.U Cksum.x, Cksum.x, Value.x;\n"
"RET;\n"

"main:\n"
" SUB.U.CC0 Garbage, SboParm, {0,0,0,0};\n"
" IF GT0.w;                                      # if numBins >0 \n"
"  PK64.U SboAddr.xy, Addr0;                     # .x = SboData, .y=n/a\n"
"  PK64.U SboAddr.zw, Addr1;                     # .z = Semaphores, .w = Bins\n"
"  CVT.U32.F32.FLR Pos, fragment.position;       # colwert to int \n"
"  MOD Pos.x, Pos.x, SboParm.w;                  # Pos.x=0-(NumBins-1)\n"
"  MAD.U64 SboAddr.z, Pos.x, 4, SboAddr.z;       # get bin's sem offset\n"
"  MAD.U64 SboAddr.w, Pos.x, 4, SboAddr.w;            # get bin's cksum offset\n"
"  ATOM.CSWAP.U32.CC0 Garbage.x, {0,1,0,0}, SboAddr.z;# if sem taken\n"
"  CAL CksumBinX (EQ0.x);                             # call CksumBinX \n"
" ENDIF;\n"
" KIL {-1,-1,-1,-1};\n"
"END\n";

//-----------------------------------------------------------------------------
// Fetch and Callwlate the Crc/Cksum buffer of a GL surface.
// If optimization hint is opGpu then use the GPU to do the Crc/Cksum
// callwlation(which doesn't require caching the data to the CPU mem).
// Otherwise fallback to the base class for callwlating the codes.
RC GLGoldenSurfaces::FetchAndCallwlate
(
    int surfNum,
    UINT32 subdevNum,
    Goldelwalues::BufferFetchHint bufFetchHint,
    UINT32 numCodeBins,
    Goldelwalues::Code calcMethod,
    UINT32 * pBins,
    vector<UINT08> *surfDumpBuffer
)
{
    RC rc;
    switch (calcMethod)
    {
        case Goldelwalues::CheckSums:
        case Goldelwalues::Crc:
        {
            // Only SBO surfaces have a non-zero BufferId
            if (BufferId(surfNum) != 0)
            {
                PROFILE(CRC);
                rc = PgmCallwlate(surfNum,
                    subdevNum,
                    numCodeBins,
                    calcMethod,
                    pBins,
                    surfDumpBuffer);
                break;
            }
        }// else fallthru

        case Goldelwalues::OldCrc:
        {
            if ( (calcMethod == Goldelwalues::Crc) && IsSurfChkUsingFragAlgo(surfNum) )
            {
                PROFILE(CRC);

                // Instead of letting Goldens perform the CRC processing just
                // pass the already callwlated CRC values to the bins array.
                // As the returned "cached address" returns an array of CRCs.
                CHECK_RC(SetSurfaceCheckMethod(surfNum, calcMethod));
                UINT32 *addr = static_cast<UINT32*>(
                    GetCachedAddress(surfNum, bufFetchHint, subdevNum, surfDumpBuffer));
                if (addr)
                {
                    // The custom CRC processing doesn't distinguish between
                    // elements in pixel format - typically entire pixel value
                    // is CRCed. To satisfy Goldens handling of CRC bins just
                    // put the same value for each element.
                    const UINT32 numElem = PixelElements(Format(surfNum));
                    for (UINT32 binIndex = 0; binIndex < numCodeBins; binIndex++)
                    {
                        for (UINT32 elemIndex = 0; elemIndex < numElem; elemIndex++)
                        {
                            pBins[binIndex*numElem + elemIndex] = addr[binIndex];
                        }
                    }
                }
            }
            else
            {
                rc = GoldenSurfaces::FetchAndCallwlate(
                    surfNum,
                    subdevNum,
                    bufFetchHint,
                    numCodeBins,
                    calcMethod,
                    pBins,
                    surfDumpBuffer);
            }
            break;
        }

        default:
            rc = RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

/*virtual*/
bool GLGoldenSurfaces::ReduceOptimization(bool reduce)
{
    m_ReduceOptimization = reduce;
    return true;
}

//-----------------------------------------------------------------------------
// Launch the shader to callwlate the Cksum/crc's of the SBO surfaces.
// Note: This shader will not change the contents of the color/depth buffers.
// Assumptions:
// 1.The bufferId of associated with the GoldenSurface has been made resident.
// 2.Upon completion all shaders will be disabled.
/* static*/
RC GLGoldenSurfaces::PgmCallwlate
(
    int surfNum,
    UINT32 subdevNum,
    UINT32 numCodeBins,
    Goldelwalues::Code calcMethod,
    UINT32 * pBins,
    vector<UINT08> *surfDumpBuffer
)
{
    StickyRC rc;
    GLuint fragId       = 0;
    GLuint vxId         = 0;
    GLint origFrId      = 0;
    GLint origVxId      = 0;
    GLuint sboId        = 0;
    GLuint64 gpuSemAddr = 0;
    GLuint64 gpuBinsAddr= 0;
    GLuint64 gpuCrcTableAddr = 0;
    UINT32 numBins      = ((numCodeBins + 31)/32)*32;
    GLint sboSize       = (numBins*2+CRC_TABLE_SIZE)*sizeof(UINT32);
    vector<UINT08> zeroBuf(sboSize,0);

    GLuint bufId = BufferId(surfNum);
    if (bufId == 0 || !glIsNamedBufferResidentLW((GLint)bufId))
    {
        return RC::SOFTWARE_ERROR;
    }

    m_LwrCachedSurface = surfNum;

    // Get the current bindings so we can restore them later.
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB,&origVxId);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&origFrId);

    // Wait for all shaders to drain before provoking the CheckSum shader.
    glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT |
                       GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_LW);

    // Create R/W SBO, to store the mutex's, crc table, and final crc/checksum
    // data for each bin. Keep numBins aligned on 128 byte boundries to get
    // proper gpuAddress alignment. Partition the SBO as follows:
    // ---------------------------------------
    // - Semaphores(1 per bin)               -
    // ---------------------------------------
    // - Cksum/Crc Bin Data                  -
    // ---------------------------------------
    // - CrcTable                            -
    // ---------------------------------------
    glGenBuffers( 1, &sboId);
    glNamedBufferDataEXT(sboId,     // named buffer
                 sboSize,           //sboSize,
                 &zeroBuf[0],       // data
                 GL_DYNAMIC_COPY);  // hint

    #ifdef DEBUG_SHADERS
    // OUCH! calling this code before getting the GPU address for this buffer
    // causes stale data to be returned in the subsequent call to
    // glGetNamedBufferSubDataEXT
    UINT32 * pData = (UINT32*)glMapNamedBufferEXT(sboId, GL_READ_WRITE);
    MASSERT(pData);
    for(int i=0; i< sboSize/4; i++,pData++)
      MEM_WR32(pData,0);
    glUnmapNamedBufferEXT(sboId);
    #endif

    // get gpu address and make it accessable to all the shaders.
    glGetNamedBufferParameterui64vLW(sboId,
            GL_BUFFER_GPU_ADDRESS_LW,
            &gpuSemAddr);
    gpuBinsAddr = gpuSemAddr + (numBins*sizeof(UINT32));
    gpuCrcTableAddr = gpuBinsAddr + (numBins*sizeof(UINT32));

    glMakeNamedBufferResidentLW(sboId, GL_READ_WRITE);
    if ((rc = mglGetRC()) != OK)
    {
        glDeleteBuffers(1, &sboId);
        return rc;
    }

    // Load the special programs to perform checksum cal. on the Sbo
    glGenProgramsLW(1, &vxId);
    glGenProgramsLW(1, &fragId);

    rc = mglLoadProgram(GL_VERTEX_PROGRAM_LW, vxId,
            static_cast<GLsizei>(VxCksumPgm.size()),
            (const GLubyte *)VxCksumPgm.c_str());

    if (calcMethod == Goldelwalues::Crc)
    {
        rc = mglLoadProgram(GL_FRAGMENT_PROGRAM_LW, fragId,
                static_cast<GLsizei>(FrCrcPgm.size()),
                (const GLubyte *)FrCrcPgm.c_str());
    }
    else // calcMethod == Goldelwalues::Checksum
    {
        rc = mglLoadProgram(GL_FRAGMENT_PROGRAM_LW, fragId,
                static_cast<GLsizei>(FrCksumPgm.size()),
                (const GLubyte *)FrCksumPgm.c_str());
    }

    if (rc != OK)
    {
        glMakeNamedBufferNonResidentLW (sboId);
        glDeleteBuffers(1, &sboId);
        glDeleteProgramsLW(1,&vxId);
        glDeleteProgramsLW(1,&fragId);
        return rc;
    }

    glBindProgramLW(GL_VERTEX_PROGRAM_LW, vxId);
    glEnable(GL_VERTEX_PROGRAM_LW);
    glDisable(GL_TESS_CONTROL_PROGRAM_LW);
    glDisable(GL_TESS_EVALUATION_PROGRAM_LW);
    glDisable(GL_GEOMETRY_PROGRAM_LW);
    glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, fragId);
    glEnable(GL_FRAGMENT_PROGRAM_LW);

    // Get the gpu address of the SBO with the data to checksum
    GLuint64 gpuWriteAddr;
    glGetNamedBufferParameterui64vLW(
                bufId,
                GL_BUFFER_GPU_ADDRESS_LW,
                &gpuWriteAddr);

    // Give the fragment shader the needed info.
    glNamedProgramLocalParameterI4uiEXT(fragId
        ,GL_FRAGMENT_PROGRAM_LW
        ,0  // index
        ,(GLuint)(gpuWriteAddr & 0xFFFFFFFFULL)    // Write SBO gpuAddrLo
        ,(GLuint)(gpuWriteAddr >> 32)              // Write SBO gpuAddrHi
        ,(GLuint)(gpuCrcTableAddr & 0xFFFFFFFFULL) // CrcTable gpuAddrLo
        ,(GLuint)(gpuCrcTableAddr >> 32)           // CrcTable gpuAddrHi
        );

    glNamedProgramLocalParameterI4uiEXT(fragId
        ,GL_FRAGMENT_PROGRAM_LW
        ,1  // index
        ,(GLuint)(gpuSemAddr & 0xFFFFFFFFULL)    // Semaphores gpuAddrLo
        ,(GLuint)(gpuSemAddr >> 32)              // Semaphores gpuAddrHi
        ,(GLuint)(gpuBinsAddr & 0xFFFFFFFFULL)   // Bins gpuAddrLo
        ,(GLuint)(gpuBinsAddr >> 32)             // Bins gpuAddrHi
        );

    // Each element is 256bits, however there is no data format to support
    // this so the width was scaled to use a known format. Now we need to
    // colwert it back.
    UINT32 w = Width(surfNum) /
                (32/ColorUtils::PixelBytes(Format(surfNum)));

    glNamedProgramLocalParameterI4uiEXT(fragId
            ,GL_FRAGMENT_PROGRAM_LW
            ,(GLuint)2
            ,(GLuint)w
            ,(GLuint)Height(surfNum)
            ,(GLuint)Format(surfNum)
            ,(GLuint) numCodeBins);

    if ((rc = mglGetRC()) == OK)
    {

        // Draw a line across the center of the viewport to ilwoke atleast
        // numCodeBins worth of threads.
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        //      left, right, bottom, top, near, far
        glOrtho(-100, 100,  -100,    100, 0,    100);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glColor4ub(0xff, 0xff, 0xff, 0xff);

        // zero out the SBO
        glNamedBufferSubDataEXT (sboId, 0, sboSize, &zeroBuf[0]);
        // Wait for buffer writes to complete.
        //glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);

        if (calcMethod == Goldelwalues::Crc)
        {
            glNamedProgramLocalParameterI4uiEXT(fragId
                ,GL_FRAGMENT_PROGRAM_LW
                ,3 // index
                ,POLYNOMIAL
                ,CRC_TABLE_SIZE
                ,1  // 1=create crcTable, 2=perform crcCalc,
                ,0  // unused
                );

            glBegin(GL_POINTS);
            glVertex3f(0,0,0);
            glEnd();

            // Wait for shader to drain before clearing the semaphore.
            glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT |
                               GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_LW);
            if((rc = mglGetRC()) == OK)
            {
                glNamedBufferSubDataEXT (sboId, 0, sboSize, &zeroBuf[0]);
                // Wait for buffer writes to complete.
                //glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT);
            }

            glNamedProgramLocalParameterI4uiEXT(fragId
                ,GL_FRAGMENT_PROGRAM_LW
                ,3 // index
                ,POLYNOMIAL
                ,CRC_TABLE_SIZE
                ,2  // 1=create crcTable, 2=perform crcCalc,
                ,0  // unused
                );
        }

        glBegin(GL_LINES);
        glVertex3f(-100,0,0);
        glVertex3f(100,0,0);
        glEnd();

        glFlush();
        glFinish();
        // Wait for shaders to drain.
        glMemoryBarrierEXT(GL_BUFFER_UPDATE_BARRIER_BIT_EXT |
                           GL_SHADER_GLOBAL_ACCESS_BARRIER_BIT_LW);

        // Get the new checksum values.
        if((rc = mglGetRC()) == OK)
        {
            UINT64 surfSize = numCodeBins*sizeof(UINT32);
            glGetNamedBufferSubDataEXT (sboId, 32*sizeof(UINT32), surfSize, pBins);
            if (surfDumpBuffer)
            {
                UINT08 *pSurf = reinterpret_cast<UINT08*>(pBins);
                surfDumpBuffer->reserve(surfSize);
                surfDumpBuffer->assign(pSurf, pSurf + surfSize);
            }

            #ifdef DEBUG_SHADERS
            // Start of debug remove when done.
            UINT32 shadersStarted = 0, shadersFinished = 0, goodCksums = 0, initCksums = 0, zeroCksums = 0;
            vector <UINT32>  SemData(numCodeBins,0);
            glGetNamedBufferSubDataEXT (sboId, 0,numCodeBins*sizeof(UINT32), &SemData[0]);

            for( UINT32 i=0; i<numCodeBins; i++)
            {
                if(SemData[i] == 1)
                    shadersStarted++;
                else if(SemData[i] == 2)
                    shadersFinished++;

                if(pBins[i] == 0x1234)
                    initCksums++; // shader exelwted enough to initialize the cksum value to 0x1234
                else if(pBins[i])
                    goodCksums++; // any other non-zero value is probably a good chsum
                else
                    zeroCksums++;
            }
            if( goodCksums != numCodeBins)
            {   // trouble
                Printf(Tee::PriHigh,"********** WARNING ****** WARNING ****WARNING ************************\n");
                Printf(Tee::PriHigh,"Shaders started:%d,finished:%d, Cksums: zero:%d,init:%d other:%d \n",
                    shadersStarted,shadersFinished,zeroCksums,initCksums,goodCksums);
                Printf(Tee::PriHigh,"********** WARNING ****** WARNING ****WARNING ************************\n");
                rc = RC::SOFTWARE_ERROR;
            }
            // End of debug remove asap.
            #endif
        }

        glMakeNamedBufferNonResidentLW (sboId);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glDisable(GL_FRAGMENT_PROGRAM_LW);
        glDisable(GL_VERTEX_PROGRAM_LW);
    }

    // Cleanup

    glDeleteBuffers(1, &sboId);
    glDeleteProgramsLW(1,&vxId);
    glDeleteProgramsLW(1,&fragId);
    if (origVxId != 0)
    {
        glBindProgramLW(GL_VERTEX_PROGRAM_LW, (GLuint)origVxId);
    }

    if (origFrId != 0)
    {
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, (GLuint)origFrId);
    }
    rc = mglGetRC();
    return rc;
}

