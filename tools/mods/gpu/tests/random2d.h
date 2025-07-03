/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2003-2013,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   random2d.h
 * @brief  Declare classes for implementing the Random2d test.
 */

#ifndef RANDOM2D_INCLUDED
#define RANDOM2D_INCLUDED

#include "core/include/rc.h"
#include "core/include/golden.h"
#include "gputestc.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gralloc.h"
#include "gpu/utility/surf2d.h"
#include "class/cla06fsubch.h"
#include "gputest.h"

class GpuGoldenSurfaces;
class Random2d;

//------------------------------------------------------------------------------
//! Abstract base class for MiscPickHelper, SurfPickHelper, etc.
//------------------------------------------------------------------------------
class PickHelper
{
public:
    PickHelper(Random2d * pRnd2d, int numPickers, GrClassAllocator * pGrAlloc);
    virtual ~PickHelper();

    //@{
    //! FancyPicker utilities, non-virtual, implemented in PickHelper.
    RC    PickerFromJsval(int index, jsval value);
    RC    PickerToJsval(int index, jsval * pvalue);
    RC    PickToJsval(int index, jsval * pvalue);
    void  CheckInitialized();
    void  SetContext(FancyPicker::FpContext * pFpCtx);
    //@}

    //@{
    //! LW HW class alloc utilies, implemented in GrClassAllocator.
    LwRm::Handle       GetHandle() const;
    UINT32             GetClass() const;
    void               SetClassOverride(UINT32 requestedClass);
    //@}

    //@{
    //! These are implemented in PickHelper(), but are overridden in some classes.
    virtual bool IsSupported(GpuDevice *pGpuDev) const;
    virtual void Restart(void);
    virtual RC CopySrcToDst(Channel * pCh);
    virtual RC CopyHostToSrc(Channel * pCh);
    virtual RC ClearScreen(Channel * pCh, UINT32 DacColorDepth);
    //@}

    //@{
    //! Pure virtual functions implemented only in the derived XxxPickHelper classes.
    virtual void SetDefaultPicker(UINT32 first, UINT32 last) = 0;
    virtual RC   Alloc
    (
        GpuTestConfiguration * pTestConfig,
        Channel             ** ppCh,
        LwRm::Handle         * phCh
    ) = 0;
    virtual void Free(void) = 0;
    virtual RC   Instantiate(Channel * pCh) = 0;
    virtual void Pick() = 0;
    virtual void Print(Tee::Priority pri) const = 0;
    virtual RC   Send(Channel * pCh) = 0;
    //@}

    //! HW subchannel assignments:
    //! These are pretty arbitrary, I try to make sure no subchannel is reused
    //! within the same draw loop, for easier debugging.
    //! All Send() methods should do a set-obj method every time.
    enum
    {
        scTwod   = LWA06F_SUBCHANNEL_2D
    };

protected:
    FancyPickerArray     m_Pickers;
    Random2d *           m_pRandom2d;
    GrClassAllocator *   m_pGrAlloc;
};

//------------------------------------------------------------------------------
class MiscPickHelper : public PickHelper
{
public:
    MiscPickHelper(Random2d * pRnd2d);
    virtual ~MiscPickHelper();
    virtual void SetDefaultPicker(UINT32 first, UINT32 last);
    virtual RC   Alloc
    (
        GpuTestConfiguration * pTestConfig,
        Channel             ** ppCh,
        LwRm::Handle         * phCh
    );
    virtual RC   Instantiate(Channel * pCh);
    virtual void Free(void);
    virtual void Pick();
    virtual void Print(Tee::Priority pri) const;
    virtual RC   Send(Channel * pCh);

    void PickFirst();
    void PickLast();
    void ForceNotify(bool);

    bool Stop() const;
    bool SuperVerbose() const;
    bool Skip() const;
    UINT32 WhoDraws() const;
    bool Notify() const;
    bool SwapSrcDst() const;
    bool ForceBigSimple() const;
    UINT32 ClearColor() const;
    UINT32 SleepMs() const;
    UINT32 Noops() const;
    bool   Flush() const;

    struct BoundingBox
    {
        UINT32 Offset; //!< Bytes from beginning of surface
        UINT32 Width;  //!< width in bytes
        UINT32 Height; //!< height in lines (assuming default surface pitch)
    };
    void GetBigSimpleBoundingBox(BoundingBox * pbb);

private:
    UINT32 m_DrawClass;
    bool   m_ForceNotify;
};

//------------------------------------------------------------------------------
class TwodPickHelper : public PickHelper
{
public:
    TwodPickHelper(Random2d * pRnd2d);
    virtual ~TwodPickHelper();
    virtual void SetDefaultPicker(UINT32 first, UINT32 last);
    virtual RC   Alloc
    (
        GpuTestConfiguration * pTestConfig,
        Channel             ** ppCh,
        LwRm::Handle         * phCh
    );
    virtual RC   Instantiate(Channel * pCh);
    virtual void Free(void);
    virtual void Restart(void);
    virtual void Pick();
    virtual void Print(Tee::Priority pri) const;
    virtual RC   Send(Channel * pCh);

    void Print(Tee::PriDebugStub) const
    {
        Print(Tee::PriSecret);
    }

    //! There's no SurfPickHelper when using the mega-2D class, it is its
    //! own surface object.  Replicate these methods here also.
    virtual RC CopySrcToDst(Channel * pCh);
    virtual RC CopyHostToSrc(Channel * pCh);
    virtual RC ClearScreen(Channel * pCh, UINT32 DacColorDepth);

    bool IsPfcColor(void);
    bool IsPfcMono(void);
    bool IsPfcIndex(void);

private:
    enum
    {
        m_MinPitch  = 256,      //!< @@@ just a guess
        m_DataSize  = 4096      //!< arbitrary
    };

    UINT32 m_Prim;                  //!< LW502D_RENDER_SOLID_PRIM_MODE or _ImgMem or _ImgCpu
    bool   m_SendXY16;              //!< if true, use _PRIM_POINT_X_Y, else _PRIM_POINT_Y.
    UINT32 m_Source;                //!< use dst as source also or not
    UINT32 m_NumTpcs;               //!< LW502D_SET_NUM_TPCS
    UINT32 m_RenderEnable;          //!< LW_502D_SET_RENDER_ENABLE_C_MODE
    UINT32 m_BigEndianCtrl;         //!< LW_502D_SET_BIG_ENDIAN_CONTROL

    const Surface2D * m_DstSurf;
    UINT32 m_DstFmt;                //!< LW502D_SET_DST_FORMAT
    UINT32 m_DstOffset;             //!< LW502D_SET_DST_OFFSET_*
    UINT32 m_DstPitch;              //!< LW502D_SET_DST_PITCH
    UINT32 m_DstH;
    UINT32 m_DstW;

    const Surface2D * m_SrcSurf;
    UINT32 m_SrcFmt;                //!< LW502D_SET_SRC_FORMAT
    UINT32 m_SrcOffset;             //!< LW502D_SET_SRC_OFFSET_*
    UINT32 m_SrcPitch;              //!< LW502D_SET_SRC_PITCH
    UINT32 m_SrcH;
    UINT32 m_SrcW;

    bool   m_ClipEnable;            //!< LW502D_SET_CLIP_ENABLE
    UINT32 m_ClipX;                 //!< LW502D_SET_CLIP_X0
    UINT32 m_ClipY;                 //!< LW502D_SET_CLIP_Y0
    UINT32 m_ClipW;                 //!< LW502D_SET_CLIP_WIDTH
    UINT32 m_ClipH;                 //!< LW502D_SET_CLIP_HEIGHT

    bool   m_CKeyEnable;            //!< LW502D_SET_COLOR_KEY_ENABLE
    UINT32 m_CKeyFormat;            //!< LW502D_SET_COLOR_KEY_FORMAT
    UINT32 m_CKeyColor;             //!< LW502D_SET_COLOR_KEY

    UINT32 m_Rop;                   //!< LW502D_SET_ROP
    UINT32 m_Beta1;                 //!< LW502D_SET_BETA1
    UINT32 m_Beta4;                 //!< LW502D_SET_BETA4
    UINT32 m_Op;                    //!< LW502D_SET_OPERATION
    UINT32 m_PrimColorFormat;       //!< LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT
    UINT32 m_LineTieBreakBits;      //!< LW502D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS

    UINT32 m_PattOffsetX;           //!< LW502D_SET_PATTERN_OFFSET
    UINT32 m_PattOffsetY;           //!< LW502D_SET_PATTERN_OFFSET
    UINT32 m_PattSelect;            //!< LW502D_SET_PATTERN_SELECT
    UINT32 m_PattMonoColorFormat;   //!< LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT
    UINT32 m_PattMonoFormat;        //!< LW502D_SET_MONOCHROME_PATTERN_FORMAT
    UINT32 m_PattMonoColor[2];      //!< LW502D_SET_MONOCHROME_PATTERN_COLOR0,1
    UINT32 m_PattMonoPatt[2];       //!< LW502D_SET_MONOCHROME_PATTERN0,1
    UINT32 m_PattColorLoadMethod;
    UINT32 m_PattColorImage[64];

    UINT32 m_PrimColor[4];          //!< LW902D_SET_RENDER_SOLID_PRIM_COLOR[0-3]
    UINT32 m_NumWordsToSend;        //!< How much of m_Data to use.
    UINT32 m_VxPerPrim;
    bool   m_DoNotify;

    UINT32 m_PcpuDataType;          //!< LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE
    UINT32 m_PcpuColorFormat;       //!< LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT
    UINT32 m_PcpuIndexFormat;       //!< LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT
    UINT32 m_PcpuMonoFormat;        //!< LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT
    bool   m_PcpuIdxWrap;           //!< LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP
    UINT32 m_PcpuMonoColor[2];      //!< LW502D_SET_PIXELS_FROM_CPU_COLOR0,1
    bool   m_PcpuMonoOpacity;       //!< LW502D_SET_PIXELS_FROM_CPU_MONO_OPACITY
    UINT32 m_PcpuSrcW;              //!< LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH
    UINT32 m_PcpuSrcH;              //!< LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT
    INT32  m_PcpuDxDuInt;           //!< LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT
    UINT32 m_PcpuDxDuFrac;          //!< LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC
    INT32  m_PcpuDyDvInt;           //!< LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT
    UINT32 m_PcpuDyDvFrac;          //!< LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC
    INT32  m_PcpuDstXInt;           //!< LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT
    UINT32 m_PcpuDstXFrac;          //!< LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC
    INT32  m_PcpuDstYInt;           //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT
    UINT32 m_PcpuDstYFrac;          //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC

    UINT32 m_PmemBlockShape;        //!< LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE
    bool   m_PmemSafeOverlap;       //!< LW502D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP
    UINT32 m_PmemOrigin;            //!< LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN
    UINT32 m_PmemFilter;            //!< LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER
    UINT32 m_PmemDstX0;             //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_X0
    UINT32 m_PmemDstY0;             //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0
    UINT32 m_PmemDstW;              //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH
    UINT32 m_PmemDstH;              //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT
    UINT32 m_PmemDuDxFrac;          //!< LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC
    INT32  m_PmemDuDxInt;           //!< LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_INT
    UINT32 m_PmemDvDyFrac;          //!< LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC
    INT32  m_PmemDvDyInt;           //!< LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_INT
    UINT32 m_PmemSrcX0Frac;         //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC
    INT32  m_PmemSrcX0Int;          //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT
    UINT32 m_PmemSrcY0Frac;         //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC
    INT32  m_PmemSrcY0Int;          //!< LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT
    UINT32 m_PmemDirectionH;        //!< LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL
    UINT32 m_PmemDirectiolw;        //!< LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL

    //! Vertex or pixels-from-cpu data.
    UINT32 m_Data[m_DataSize/sizeof(UINT32)];

    // Private functions:

    //! Look up properties of a color-format enum value.
    void GetFmtInfo
    (
        UINT32 fmt,
        UINT32 * pBitsPerPixel,
        bool * pIsFpOrSrgb,
        bool * pIsY,
        bool * pIsFermi,
        bool * pIsValidForSolid
    );
    void   SetDefaultPicks(UINT32 wSrc, UINT32 wDst, UINT32 colorDepth);
    void   PickVertices();
    void   PickPixels();
    void   PickImage();
    const char * FmtToName(UINT32 fmt) const;
};

extern void Random2d_Print(void * pvRandom2d);

//! Mixed-class 2d graphics test.
/**
 * Random2d is a random 2D graphics test that knows how to exercise many
 * different HW rendering classes: blit, scaled-image, rect, etc. and also the
 * applicable "context" classes: surfaces, rop, ckey, etc.
 *
 * Also, it can exercise the new LW50_TWOD replacement class in newer chips.
 *
 * The test creates three surfaces and randomly selects one of these as
 * a destination, and one as a source surface at the beginning of each frame.
 *
 * Typically, on pre-LW50/G80 GPUS, surface "A" is in FB and is tiled, surface
 * "B" is FB untiled, and surface "C" is host memory and non-coherent.
 *
 * The test is heavily configurable from JavaScript; the user may change
 * surface features such as size and location, and the FancyPicker class is
 * used to generate most method data.
 * See the file fpk_comm.h for documentation on how to configure the FancyPicker
 * objects used by Random2d.
 */
class Random2d : public GpuTest
{
public:
    Random2d();
    ~Random2d();

    virtual bool IsSupported();
    virtual bool IsSupportedVf();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    //! FancyPicker setup: JS->C++.
    RC PickerFromJsval(int index, jsval value);

    //! FancyPicker setup: C++->JS.
    RC PickerToJsval(int index, jsval * value);

    //! FancyPicker most recent pick: C++->JS.
    RC PickToJsval(int index, jsval * value);

    //! Overrides of FP access from ModsTest
    virtual RC SetPicker(UINT32 idx, jsval v);
    virtual UINT32 GetPick(UINT32 idx);
    virtual RC GetPicker(UINT32 idx, jsval *v);
    virtual UINT32 GetNumPickers()
            {return RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET;}
    virtual RC SetDefaultPickers(UINT32 first, UINT32 last);

    //! Print current picks to screen and logfile.  Goldelwalues callback.
    void Print(Tee::Priority pri) const;

    //! This test operates on all GPUs of an SLI device in parallel.
    virtual bool GetTestsAllSubdevices() { return true; }

    //! Description of one of the 3 memory contexts (tiled, untiled, AGP).
    enum WhichMem
    {
        SrcMem,
        DstMem,
        SysMem
    };

    //@{
    //! Functions used by PickHelpers to view test-global data owned by Random2d.
    Surface2D *GetMemoryInfo(WhichMem whichMem);
    bool   IsAllocated(UINT32 whichHelper) const;
    FLOAT64 TimeoutMs();
    UINT32 GetLoopsPerFrame();
    UINT32 GetChannelSize();
    //@}

    //@{
    //! Access by the PickHelpers to the test-global random-number generator.
    void   SeedRandom(UINT32 s);
    UINT32 GetRandom();
    UINT32 GetRandom(UINT32 min, UINT32 max);
    //@}

public:
    // Each helper has a pointer to this Random2d object.
    // Our helpers are public so that they can find each other.
    MiscPickHelper    m_Misc;
    TwodPickHelper    m_Twod;

    // The helpers need access to the notifier.
    Notifier          m_Notifier;

private:
    //@{
    //! JavaScript configuration data (user preferences).
    bool              m_RgbClear;
    bool              m_CpuCopy;
    vector<UINT32>    m_TopFillPattern;
    vector<UINT32>    m_BottomFillPattern;
    //@}

    FancyPicker::FpContext * m_pFpCtx;
    PickHelper *      m_AllPickHelpers[RND2D_NUM_RANDOMIZERS];
    LwRm::Handle      m_hCh;
    Channel *         m_pCh;
    Surface2D         m_SurfA;      //!< primary surface, usually tiled
    Surface2D         m_SurfB;      //!< other surface, not tiled
    Surface2D         m_SurfC;      //!< system memory, usually AGP
    bool              m_DstIsSurfA;
    bool              m_SrcIsFilled;
    GpuGoldenSurfaces * m_pGGSurfs;

    // Private functions:
    RC Restart(void);
    RC DoOneLoop(void);
    RC AllocMem (UINT32 DacPitch);
    void FreeMem ();

public:
    SETGET_PROP(RgbClear, bool);
    SETGET_PROP(CpuCopy, bool);
};

#endif
