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

#include "gpu/include/dmawrap.h"
#include "core/include/tasker.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputest.h"
#include "core/include/platform.h"
#include "class/cl2080.h"

namespace
{
    // This is our genie who fetches GpuTestConfiguration for us.
    class GpuTestGenie: public GpuTest
    {
        public:
            virtual ~GpuTestGenie();
            virtual RC Run();
            GpuTestConfiguration* GetTestConfiguration();
    };

    GpuTestConfiguration* GpuTestGenie::GetTestConfiguration()
    {
        return GpuTest::GetTestConfiguration();
    }
}

//------------------------------------------------------------------------------
//! \brief Provides a JS interface for working with DMAWrapper
class JSSurfaceCopy
{
public:
    JSSurfaceCopy();
    RC Initialize(
        GpuTestConfiguration* pTestConfig,  //!< TestConfig to allocate channels on
        Memory::Location NfyLoc);           //!< Memory in which to alloc semaphore.
    RC Initialize(
        GpuTestConfiguration* pTestConfig  //!< TestConfig to allocate channels on
    );

    RC Cleanup();

    struct CopyInst
    {
        Surface2D* pSrcSurf;
        UINT32 srcX,       //!< Starting src X offset in pixels
               srcY,       //!< Starting src Y offset in lines
               srcWidth,   //!< Width of copied rect, in pixels
               srcHeight;  //!< Height of copied rect, in lines
        Surface2D* pDestSurf;
        UINT32 destX,      //!< Starting dst X, in pixels.
               destY;      //!< Starting dst Y, in lines.
    };
    RC Copy
    (
        const vector<CopyInst>& copies,
        UINT32 numLoops,          //!< Number of times to loop through all the copies
        FLOAT64 TimeoutMs,        //!< Timeout for dma operation
        UINT32 SubdevNum         //!< Subdevice number we are working on
    );
    RC Wait(FLOAT64 timeoutMs);
    RC IsCopyComplete(bool *pComplete);
    UINT32 GetPitchAlignRequirement();

    // Read and Write to a part of a surface in a detached thread 'numTimes' times
    // The copyDir/cpyBlkSize controls the direction of read/writes (horiz/vertical)
    // and how many pixels are written before read and vice versa
    RC ReadWriteSurfaceChunkContinuously(Surface2D *surf,
            UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
            UINT32 Value, UINT32 numTimes,
            UINT32 copyDir, UINT32 cpyBlkSize,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    SETGET_PROP(FlushedCopy, bool);
    SETGET_PROP(CopyMask, UINT08);
    SETGET_PROP(CEInstance, UINT32);

private:
    bool m_FlushedCopy;
    UINT08 m_CopyMask;
    UINT32 m_CEInstance;
    DmaWrapper m_pDmaWrap;
};

//-----------------------------------------------------------------------------
JSSurfaceCopy::JSSurfaceCopy() :
m_FlushedCopy(true)
,m_CopyMask(0xF)
,m_CEInstance(DmaWrapper::DEFAULT_ENGINE_INSTANCE)
{}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::Initialize(
        GpuTestConfiguration* pTestConfig,
        Memory::Location NfyLoc
)
{
    RC rc;

    if (m_CEInstance != DmaWrapper::DEFAULT_ENGINE_INSTANCE)
    {
        DmaCopyAlloc alloc;
        UINT32       engineId;
        if (OK != alloc.GetEngineId(pTestConfig->GetGpuDevice(),
                                    pTestConfig->GetRmClient(),
                                    m_CEInstance,
                                    &engineId))
        {
            Printf(Tee::PriLow, "User-selected CE%u instance does not exist, using default CE instead\n",
                   m_CEInstance);
            m_CEInstance = DmaWrapper::DEFAULT_ENGINE_INSTANCE;
        }
    }

    CHECK_RC(m_pDmaWrap.Initialize(pTestConfig,
        NfyLoc,
        static_cast<DmaWrapper::PREF_TRANS_METH>(DmaWrapper::COPY_ENGINE),
        m_CEInstance));
    return rc;
}

RC JSSurfaceCopy::Initialize(
        GpuTestConfiguration* pTestConfig
)
{
    RC rc;
    CHECK_RC(m_pDmaWrap.Initialize(pTestConfig, Memory::Optimal));
    return rc;
}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::Copy(const vector<CopyInst>& copies,
                       UINT32 numLoops,
                       FLOAT64 TimeoutMs,
                       UINT32 SubdevNum)
{
    RC rc;

    // Run copies in a detached thread
    Tasker::DetachThread threadDetacher;
    if (!m_FlushedCopy)
    {
        m_pDmaWrap.SetFlush(false);
    }

    for (UINT32 iLoop = 0; iLoop < numLoops; iLoop++)
    {
        for (vector<CopyInst>::const_iterator copyIt = copies.begin();
             copyIt != copies.end(); copyIt++)
        {
            bool lastCopy = (((copyIt + 1) == copies.end()) &&
                            ((iLoop + 1) == numLoops));  // needed to flush on last copy
            //do not wait for copy to complete if flushing is not requested
            bool waitForFinish = m_FlushedCopy && lastCopy;

            bool oddLoop = ((iLoop+1) % 2 != 0);
            // if we want to perform copy even number of times, the surfaces must be split in half
            // and copy between them alternately, so that data is different in every copy
            // so the surfaces must have even height, to split it equally into 2 halves
            if (!oddLoop && copyIt->srcHeight % 2)
            {
                Printf(Tee::PriHigh, "Error: For multiple copies, surface height must be even!\n");
                return RC::SOFTWARE_ERROR;
            }

            // Start copy
            CHECK_RC(m_pDmaWrap.SetSurfaces(copyIt->pSrcSurf, copyIt->pDestSurf));
            m_pDmaWrap.SetCopyMask(m_CopyMask);
            if (oddLoop)
            {
                CHECK_RC(m_pDmaWrap.Copy(copyIt->srcX, copyIt->srcY,
                                         copyIt->srcWidth, copyIt->srcHeight,
                                         copyIt->destX, copyIt->destY,
                                         Tasker::GetDefaultTimeoutMs(),
                                         0, /* subdev */
                                         waitForFinish,
                                         lastCopy, lastCopy));
            }
            else
            {
                CHECK_RC(m_pDmaWrap.Copy(copyIt->srcX, copyIt->srcY,
                                         copyIt->srcWidth, copyIt->srcHeight/2,
                                         copyIt->destX, copyIt->destY + copyIt->srcHeight/2,
                                         Tasker::GetDefaultTimeoutMs(),
                                         0, /* subdev */
                                         waitForFinish,
                                         lastCopy, lastCopy));
                CHECK_RC(m_pDmaWrap.Copy(copyIt->srcX, copyIt->srcY + copyIt->srcHeight/2,
                                         copyIt->srcWidth, copyIt->srcHeight/2,
                                         copyIt->destX, copyIt->destY,
                                         Tasker::GetDefaultTimeoutMs(),
                                         0, /* subdev */
                                         waitForFinish,
                                         lastCopy, lastCopy));
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::Cleanup()
{
    RC rc;
    CHECK_RC(m_pDmaWrap.Cleanup());
    return rc;
}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::Wait(FLOAT64 timeoutMs)
{
    RC rc;
    CHECK_RC(m_pDmaWrap.Wait(timeoutMs));
    return rc;
}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::IsCopyComplete(bool *pComplete)
{
    RC rc;
    CHECK_RC(m_pDmaWrap.IsCopyComplete(pComplete));
    return rc;
}

//-----------------------------------------------------------------------------
UINT32 JSSurfaceCopy::GetPitchAlignRequirement()
{
    return m_pDmaWrap.GetPitchAlignRequirement();
}

//-----------------------------------------------------------------------------
RC JSSurfaceCopy::ReadWriteSurfaceChunkContinuously(
    Surface2D *pSurf,
    UINT32 x,
    UINT32 y,
    UINT32 Width,
    UINT32 Height,
    UINT32 Value,
    UINT32 numTimes,
    UINT32 cpyDirection,
    UINT32 cpyBlockSize,
    UINT32 subdev)
{
    RC rc;
    UINT32 PixelX, PixelY;
    Tasker::DetachThread threadDetacher;
    MASSERT(pSurf);

    if (Width % cpyBlockSize != 0 ||  Height % cpyBlockSize != 0)
    {
        Printf(Tee::PriHigh, "ReadWriteSurfaceChunkContinuously:: cpyBlockSize must be divisible by width/height!!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (cpyDirection > 1)
    {
        Printf(Tee::PriHigh, "The copy direction value should be 0 or 1!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (cpyBlockSize > Width || cpyBlockSize > Height)
    {
        Printf(Tee::PriHigh, "The size of the copy box is greater than width/height\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pSurf->GetBytesPerPixel() != 4)
    {
        Printf(Tee::PriHigh, "Continous read/write to a surface only supports 4 bpp surfaces\n");
        return RC::SOFTWARE_ERROR;
    }

    const Surface2D::MappingSaver SavedMapping(*pSurf);
    if (pSurf->GetAddress())
    {
        pSurf->Unmap();
    }

    UINT32 valRead = 0x0;
    CHECK_RC(pSurf->MapRect(x, y, Width, Height, subdev));

    switch (cpyDirection)
    {
        case 0: //column wise
            for (UINT32 LoopCt = 0; LoopCt < numTimes; LoopCt++)
            {
                UINT32 count = 0;
                UINT32 startX = x;
                UINT32 numLoops = Width/cpyBlockSize;

                //write 1st column block
                for (PixelY = y; PixelY < y+Height; PixelY++)
                {
                    for (PixelX = startX; PixelX < startX+cpyBlockSize; PixelX++)
                    {
                        count++;
                        UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                        MEM_WR32((UINT08 *)(pSurf->GetAddress()) + Offset, Value + count);
                    }
                }

                //write nth column block, read n-1th column block
                for (UINT32 loop = 1; loop < numLoops; loop++)
                {
                    startX = x + loop * cpyBlockSize;

                    for (PixelY = y; PixelY < y+Height; PixelY++)
                    {
                        for (PixelX = startX; PixelX < startX+cpyBlockSize; PixelX++)
                        {
                            count++;
                            UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                            MEM_WR32((UINT08 *)(pSurf->GetAddress()) + Offset, Value + count);

                            Offset = pSurf->GetPixelOffset(PixelX - cpyBlockSize, PixelY);
                            valRead = MEM_RD32((UINT08 *)(pSurf->GetAddress()) + Offset);
                            if (valRead != (Value + count - Height * cpyBlockSize))
                            {
                                rc = RC::GOLDEN_VALUE_MISCOMPARE;
                            }
                        }
                    }

                }

                //read last column block
                startX = x + cpyBlockSize * (numLoops - 1);
                for (PixelY = y; PixelY < y+Height; PixelY++)
                {
                    for (PixelX = startX; PixelX < startX+cpyBlockSize; PixelX++)
                    {
                        count++;
                        UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                        valRead = MEM_RD32((UINT08 *)(pSurf->GetAddress()) + Offset);
                        if (valRead != (Value + count - Height * cpyBlockSize))
                        {
                            rc = RC::GOLDEN_VALUE_MISCOMPARE;
                        }
                    }
                }
            }
            break;

        case 1: //row wise
            for (UINT32 LoopCt = 0; LoopCt < numTimes; LoopCt++)
            {
                UINT32 count = 0;
                UINT32 startY = y;
                UINT32 numLoops = Height/cpyBlockSize;

                //write 1st row block
                for (PixelX = x; PixelX < x+Width; PixelX++)
                {
                    for (PixelY = startY; PixelY < startY+cpyBlockSize; PixelY++)
                    {
                        count++;
                        UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                        MEM_WR32((UINT08 *)(pSurf->GetAddress()) + Offset, Value + count);
                    }
                }

                //write nth column block, read n-1th column block
                for (UINT32 loop = 1; loop < numLoops; loop++)
                {
                    startY = y + loop * cpyBlockSize;

                    for (PixelX = x; PixelX < x+Width; PixelX++)
                    {
                        for (PixelY = startY; PixelY < startY+cpyBlockSize; PixelY++)
                        {
                            count++;
                            UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                            MEM_WR32((UINT08 *)(pSurf->GetAddress()) + Offset, Value + count);

                            Offset = pSurf->GetPixelOffset(PixelX, PixelY - cpyBlockSize);
                            valRead = MEM_RD32((UINT08 *)(pSurf->GetAddress()) + Offset);
                            if (valRead != (Value + count - Width * cpyBlockSize))
                            {
                                rc = RC::GOLDEN_VALUE_MISCOMPARE;
                            }
                        }

                    }
                }

                //read last column block
                startY = y + cpyBlockSize * (numLoops - 1);
                for (PixelX = x; PixelX < x+Width; PixelX++)
                {
                    for (PixelY = startY; PixelY < startY+cpyBlockSize; PixelY++)
                    {
                        count++;
                        UINT64 Offset = pSurf->GetPixelOffset(PixelX, PixelY);
                        valRead = MEM_RD32((UINT08 *)(pSurf->GetAddress()) + Offset);
                        if (valRead != (Value + count - Width * cpyBlockSize))
                        {
                            rc = RC::GOLDEN_VALUE_MISCOMPARE;
                        }
                    }
                }
            }
            break;

        default:
            MASSERT(!"Invalid copy direction");
            break;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Constructor for a SurfaceCopy object.
static JSBool C_JSSurfaceCopy_constructor
(
    JSContext *cx,
    JSObject *thisObj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    JSSurfaceCopy * pJSSurfaceCopy = new JSSurfaceCopy();
    MASSERT(pJSSurfaceCopy);
    JS_SetPrivate(cx, thisObj, pJSSurfaceCopy);
    return JS_TRUE;
}

static void C_JSSurfaceCopy_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JSSurfaceCopy * pJSSurfaceCopy;
    pJSSurfaceCopy = (JSSurfaceCopy *)JS_GetPrivate(cx, obj);
    if (pJSSurfaceCopy)
    {
        delete pJSSurfaceCopy;
        pJSSurfaceCopy = nullptr;
    }
};

static JSClass JSSurfaceCopy_class =
{
    "SurfaceCopy",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JSSurfaceCopy_finalize
};

static SObject JSSurfaceCopy_Object
(
    "SurfaceCopy",
    JSSurfaceCopy_class,
    0,
    0,
    "SurfaceCopy JS object",
    C_JSSurfaceCopy_constructor
);

//-----------------------------------------------------------------------------
// SurfaceCopy JS Properties
// Property has valid values after initialization.{SurfaceCopy.Initialize(GpuTest)}
CLASS_PROP_READONLY(JSSurfaceCopy, PitchAlignRequirement, UINT32,
                    "Return PitchAlignRequirement for DMA operations based \
                     on current configuration");

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                SetCopyMask,
                1,
                "SetCopyMask for copying with holes.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.SetCopyMask(mask)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    UINT08 copyMask;
    if (OK != pJs->FromJsval(pArguments[0], &copyMask))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        pJSSurfaceCopyObj->SetCopyMask(copyMask);
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                SetFlushedCopy,
                1,
                "Enable/Disable flushing copy methods to channel.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.SetFlushedCopy(true/false)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    bool flush;
    if (OK != pJs->FromJsval(pArguments[0], &flush))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        pJSSurfaceCopyObj->SetFlushedCopy(flush);
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                SetCEInstance,
                1,
                "Set CE instance to use.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.SetCEInstance(Instance)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    UINT32 CEInstance;
    if (OK != pJs->FromJsval(pArguments[0], &CEInstance))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        pJSSurfaceCopyObj->SetCEInstance(CEInstance);
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                Initialize,
                1,
                "Initializes DMA wrapper based on current test configuration")
{
    RC rc;
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.Initialize(GpuTest)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSObject* pGpuTestObj = nullptr;
    if (OK != pJs->FromJsval(pArguments[0], &pGpuTestObj))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuTestGenie* const pGpuTest = static_cast<GpuTestGenie*>(JS_GetPrivate(pContext, pGpuTestObj));
    if (pGpuTest == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        C_CHECK_RC(pJSSurfaceCopyObj->Initialize(pGpuTest->GetTestConfiguration()));
    }

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                Copy,
                3,
                "Isses a series of copies between surfaces")
{
    RC rc;
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.Copy(GpuTest, SemaphoreLocation, numLoops ,"
        "[ [Surface2D, [x,y,w,h], Surface2D, [x,y]], ... ])";

    if (NumArguments != 4)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSObject* pGpuTestObj = nullptr;
    if (OK != pJs->FromJsval(pArguments[0], &pGpuTestObj))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    GpuTestGenie* const pGpuTest = static_cast<GpuTestGenie*>(JS_GetPrivate(pContext, pGpuTestObj));
    if (pGpuTest == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    UINT32 semaLocation = 0;
    if (OK != pJs->FromJsval(pArguments[1], &semaLocation))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    UINT32 numLoops = 0;
    if (OK != pJs->FromJsval(pArguments[2], &numLoops))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsArray copies;
    if (OK != pJs->FromJsval(pArguments[3], &copies))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        C_CHECK_RC(pJSSurfaceCopyObj->Initialize(pGpuTest->GetTestConfiguration(),
                                    static_cast<Memory::Location>(semaLocation)));
    }

    vector<JSSurfaceCopy::CopyInst> copyInsts;
    for (UINT32 iCopy=0; iCopy < copies.size(); iCopy++)
    {
        JSSurfaceCopy::CopyInst copyInst = {};

        JsArray copyArray;
        if (OK != pJs->FromJsval(copies[iCopy], &copyArray))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }

        if (copyArray.size() != 4)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }

        JSObject* pSrcSurfObj = nullptr;
        if (OK != pJs->FromJsval(copyArray[0], &pSrcSurfObj))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        Surface2D* const pSrcSurf = static_cast<Surface2D*>(JS_GetPrivate(pContext, pSrcSurfObj));
        if (pSrcSurfObj == nullptr)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        copyInst.pSrcSurf = pSrcSurf;

        JsArray srcRect;
        if (OK != pJs->FromJsval(copyArray[1], &srcRect))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        if (srcRect.size() != 4)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        UINT32 srcDims[4] = {0};
        for (UINT32 i=0; i < 4; i++)
        {
            if (OK != pJs->FromJsval(srcRect[i], &srcDims[i]))
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
        }
        copyInst.srcX      = (pSrcSurf->GetBitsPerPixel() * srcDims[0]) >> 3;
        copyInst.srcY      = srcDims[1];
        copyInst.srcWidth  = (pSrcSurf->GetBitsPerPixel() * srcDims[2]) >> 3;
        copyInst.srcHeight = srcDims[3];

        JSObject* pDestSurfObj = nullptr;
        if (OK != pJs->FromJsval(copyArray[2], &pDestSurfObj))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        Surface2D* const pDestSurf = static_cast<Surface2D*>(JS_GetPrivate(pContext, pDestSurfObj));
        if (pDestSurfObj == nullptr)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        copyInst.pDestSurf = pDestSurf;

        JsArray destRect;
        if (OK != pJs->FromJsval(copyArray[3], &destRect))
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        if (destRect.size() != 2)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
        UINT32 destDims[2] = {0};
        for (UINT32 i=0; i < 2; i++)
        {
            if (OK != pJs->FromJsval(destRect[i], &destDims[i]))
            {
                JS_ReportError(pContext, usage);
                return JS_FALSE;
            }
        }
        copyInst.destX = (pDestSurf->GetBitsPerPixel() * destDims[0]) >> 3;
        copyInst.destY = destDims[1];

        copyInsts.push_back(copyInst);
    }

    // Start copy
    C_CHECK_RC(pJSSurfaceCopyObj->Copy(copyInsts,
                                       numLoops,
                                       Tasker::GetDefaultTimeoutMs(),
                                       0 /* subdev */));

    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                IsCopyComplete,
                1,
                "Check if copy is complete.")
{
    RC rc;
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JSObject * pReturnStatus = 0;

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.IsCopyComplete([complete])";

    if (1 != pJs->UnpackArgs(pArguments, NumArguments, "o", &pReturnStatus))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    bool pComplete = false;
    if (pJSSurfaceCopyObj != nullptr)
    {
        C_CHECK_RC(pJSSurfaceCopyObj->IsCopyComplete(&pComplete));
        RETURN_RC(pJs->SetElement(pReturnStatus, 0, pComplete));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                Wait,
                1,
                "Wait for Copy to Complete.")
{
    RC rc;
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.Wait(TimeoutMs)";

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    FLOAT64 timeoutMs;
    if (OK != pJs->FromJsval(pArguments[0], &timeoutMs))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        C_CHECK_RC(pJSSurfaceCopyObj->Wait(timeoutMs));
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                Cleanup,
                0,
                "Cleanup resources acquired for copy.")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.Cleanup()";

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        pJSSurfaceCopyObj->Cleanup();
    }
    RETURN_RC(OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JSSurfaceCopy,
                ReadWriteSurfaceChunkContinuously,
                9,
                "Wait for Copy to Complete.")
{
    RC rc;
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJs;
    const char usage[] = "Usage: SurfaceCopy.ReadWriteSurfaceChunkContinuously("
            "surface, startX, startY, Width, Height, copyValue, loopCount"
            "copyDirection, copyBlockSize)";

    if (NumArguments != 9)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSObject* pSurfaceObj = nullptr;
    if (OK != pJs->FromJsval(pArguments[0], &pSurfaceObj))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    if (pSurfaceObj == nullptr)
    {
        return JS_FALSE;
    }
    Surface2D* const pSurf = static_cast<Surface2D*>(JS_GetPrivate(pContext, pSurfaceObj));

    if (pSurf == nullptr)
    {
        Printf(Tee::PriHigh, "Invalid Surface object!\n");
        return JS_FALSE;
    }

    UINT32 x, y, Width, Height, Value, numTimes, cpyDirection, cpyBlockSize;
    if ((OK != pJs->FromJsval(pArguments[1], &x)) ||
       (OK != pJs->FromJsval(pArguments[2], &y)) ||
       (OK != pJs->FromJsval(pArguments[3], &Width)) ||
       (OK != pJs->FromJsval(pArguments[4], &Height)) ||
       (OK != pJs->FromJsval(pArguments[5], &Value)) ||
       (OK != pJs->FromJsval(pArguments[6], &numTimes)) ||
       (OK != pJs->FromJsval(pArguments[7], &cpyDirection)) ||
       (OK != pJs->FromJsval(pArguments[8], &cpyBlockSize)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JSSurfaceCopy *pJSSurfaceCopyObj =
                JS_GET_PRIVATE(JSSurfaceCopy, pContext, pObject, "SurfaceCopy");
    if (pJSSurfaceCopyObj != nullptr)
    {
        C_CHECK_RC(pJSSurfaceCopyObj->ReadWriteSurfaceChunkContinuously(pSurf,
            x, y, Width, Height, Value, numTimes, cpyDirection, cpyBlockSize));
    }
    RETURN_RC(OK);
}
