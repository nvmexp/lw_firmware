/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_GLGOLDENSURFACES_H
#define INCLUDED_GLGOLDENSURFACES_H

//! \file glgoldensurfaces.h
//! \brief Declares the GLGoldenSurfaces class.
#ifndef INCLUDED_GOLDEN_H
#include "core/include/golden.h"
#endif
#ifndef INCLUDED_MODSGL_H
#include "modsgl.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

//------------------------------------------------------------------------------
//! \brief Mods OpenGL-specific derived class of GoldenSurfaces.
//!
//! This class is implemented using the OpenGL driver.
//! It is the right surfaces adaptor for most OpenGL manufacturing tests.
//
class GLGoldenSurfaces : public GoldenSurfaces
{
public:
    GLGoldenSurfaces();
    virtual ~GLGoldenSurfaces();

    //! \brief Describe surfaces to be read back.
    void DescribeSurfaces
    (
        UINT32 width,                   //!< The width in pixels of the window.
        UINT32 height,                  //!< The hight in pixels of the window.
        ColorUtils::Format colorFmt,    //!< Mods color format of the color surface.
        ColorUtils::Format zFmt,        //!< GL color format of the Z/stencil surface.
        UINT32 displayMask              //!< Display mask for hw CRCs.
    );

    RC CreateSurfCheckShader();
    void CleanupSurfCheckShader();
    void SetCalcAlgorithmHint(Goldelwalues::CalcAlgorithm calcAlgoHint);

    //! \brief Add a multiple-render-targets "extra" color surface.
    //!
    //! Should be called after DescribeSurfaces.
    void AddMrtColorSurface
    (
        ColorUtils::Format colorFmt,    //!< Mods color format of the surface.
        GLenum             attachment,  //!< GL_COLOR_ATTACHMENT1_EXT or greater.
        UINT32             width,       //!< The width in pixels of the window.
        UINT32             height       //!< The hight in pixels of the window.
    );

    virtual void AddSurf
    (
        string  name
        ,ColorUtils::Format hwformat
        ,UINT32 displayMask
        ,GLenum attachment
        ,GLint bufferId
        ,UINT32 width
        ,UINT32 height
    );

    void SetReadPixelsBuffer
    (
        GLenum frontOrBackOrAttachment
    );

    // Implement GoldenSurfaces interfaces:
    virtual int NumSurfaces() const;
    virtual void InjectErrorAtSurface(int surfNum);
    virtual void SetGoldensBinSize(UINT32 binSize);
    virtual RC SetSurfaceCheckMethod(int surfNum, Goldelwalues::Code calcMethod);
    virtual void SetExpectedCheckMethod(Goldelwalues::Code calcMethod);
    virtual const string & Name (int surfNum) const;
    virtual RC CheckAndReportDmaErrors(UINT32 subdevNum);
    virtual void * GetCachedAddress(int surfNum,Goldelwalues::BufferFetchHint bufFetchHint,UINT32 subdevNum,vector<UINT08> *surfDumpBuffer);
    virtual void Ilwalidate();
    virtual INT32 Pitch(int surfNum) const;
    virtual UINT32 BufferId(int surfNum) const;
    virtual UINT32 Width(int surfNum) const;
    virtual UINT32 Height(int surfNum) const;
    virtual ColorUtils::Format Format(int surfNum) const;
    virtual UINT32 Display(int surfNum) const;
    virtual void SetNumLayeredSurfaces(int numLayers);
    virtual void SetSurfaceTextureIDs(UINT32 depthTextureID, UINT32 colorSurfaceID);
    virtual RC GetModifiedSurfProp(int surfNum, UINT32 *Width, UINT32 *Height,
                                     UINT32 *Pitch);

    // Fetch and Callwlate Crc/Checksum buffer using a GL vertex/fragment shader.
    virtual RC FetchAndCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    );

    //! Flag for whether we should reduce test optimization (prefer CPU vs GPU)
    virtual bool ReduceOptimization(bool reduce);

private:
    struct Surf
    {
        string              Name;           // I.e. "Z", "C0", "C1", etc.
        UINT32              Displaymask;    // Will be 0 except for C0.
        GLenum              ReadPixelsBuffer;

        // The BufferId is used for non-display surfaces that require a
        // different method for caching the data.
        GLenum              BufferId;

        // The HW color format that will be used inside the GL driver.
        // This is based on the GL texture Internal Format.
        ColorUtils::Format  HwFormat;

        // The HW color format that most closely matches the format returned
        // by glReadPixels, which might not exactly match HwFormat.
        // This is the format we must communicate to Goldelwalues.
        ColorUtils::Format  ReadbackFormat;

        UINT32              Width;
        UINT32              Height;

        // Check surface using compute shader?
        INT32               NumWorkGroups;
        INT32               WorkGroupSize;
        Goldelwalues::CalcAlgorithm CalcAlgorithm;
    };

    RC PgmCallwlate
    (
        int surfNum,
        UINT32 subdevNum,
        UINT32 numCodeBins,
        Goldelwalues::Code calcMethod,
        UINT32 * pRtnCodeValues,
        vector<UINT08> *surfDumpBuffer
    );

    vector<Surf> m_Surfs;
    int         m_LwrCachedSurface;
    vector<UINT08> m_CacheMem;
    const mglTexFmtInfo *m_pCachedFmtInfo;
    INT32       m_CachedAbsPitch;
    UINT64      m_CachedSurfaceSize;

    INT32       m_InjectErrorAtSurface;

    //Layered surfaces
    INT32       m_NumLayers;
    UINT32      m_DepthTextureID;
    UINT32      m_ColorSurfaceID;
    vector<UINT08> m_LayeredBuffer; // buffer to cache layered images

    struct Buffer
    {
        string name;
        ColorUtils::Format fmt;
        UINT32 displayMask;

        Buffer(string Name, ColorUtils::Format Fmt, UINT32 Mask)
        {
            name = Name;
            fmt = Fmt;
            displayMask = Mask;
        }
    };

    struct CpuFragmentedThreadArgs
    {
        UINT08 *pSrcSurf;
        UINT08 *newSurf;
        UINT32 step;
        UINT32 binSize;
        UINT32 workLoadPerWorkGroup;
        UINT32 pixelStride;
        INT32 numWorkGroups;
        INT32 workGroupSize;
        Goldelwalues::Code calcMethod;
    };

    // surface check using compute shader
    UINT32             m_BinSize;
    Goldelwalues::Code m_CalcMethod;
    GLuint             m_PixelPackBuffer;
    GLuint             m_TextureBuffer;
    GLuint             m_StorageBuffer;
    ColorUtils::Format m_StorageBufferFmt;
    UINT32             m_ComputeCheckSumProgram;
    UINT32             m_ComputeCRCProgram;
    Goldelwalues::CalcAlgorithm m_CalcAlgoHint;
    bool               m_ReduceOptimization;

    Goldelwalues::CalcAlgorithm GetSurfCalcAlgorithm(int surfNum) const;
    bool IsSurfChkUsingFragAlgo(int surfNum) const;

    RC SetupCache();
    //! subdevNum is 0-indexed (subdevice instance)
    RC GetSurfaceForGoldelwals(int surfNum, vector<UINT08> *surfDumpBuffer);
    void CopyLayeredBuffer(vector<UINT08> &dst, int surfNum);
    RC CacheLayeredSurface(int surfNum, bool forceCacheToCpu);
    void CacheSurface(int surfNum);
    void CacheFragGPUSurface(int surfNum);
    RC CheckSurfaceOnGPU(int surfNum);
    bool StoreCachedSurfaceToBuffer(int surfNum, vector<UINT08> *buf);

    RC CheckSurfaceOnCPU(int surfNum);
    static void CheckSurfaceOnCPUThreadChecksum(void *args);
    static void CheckSurfaceOnCPUThreadCRC(void *args);

    UINT32 CheckAndResetInjectError(int surfNum);

    virtual RC GetPitchAlignRequirement(UINT32 *pitch)
    {
        MASSERT(pitch);

        *pitch = 1;

        return OK;
    }
};
#endif //INCLUDED_GLGOLDENSURFACES_H

