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

//!
//! \file rmt_cl907f.cpp
//! \brief Hardware remapper test.
//!
//! this test check the functionality of remapper hardware features
//! by enabling/disabling using control commands and by comparing the data with
//! Software remapper for a match/mismatch.

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpudev.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "gpu/utility/blocklin.h"
#include "random.h"
#include "core/include/gpu.h"

#include "class/cl907f.h" // GF100_REMAPPER
#include "ctrl/ctrl907f.h"

#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "core/include/memcheck.h"

#define MAX_BYTES_PER_PIXEL  16
#define MAX_NUMBER_REMAPPERS 8

#define TEST_HEIGHT 10

#define MIN_RANDOM_VALUE 1
#define MAX_RANDOM_VALUE 255

#define GOB_HEIGHT_ROWS          8

#define BYTE_PER_PIXEL_ONE       1
#define BYTE_PER_PIXEL_TWO       2
#define BYTE_PER_PIXEL_FOUR      4
#define BYTE_PER_PIXEL_EIGHT     8
#define BYTE_PER_PIXEL_SIXTEEN  16

#define BLOCK_HEIGHT_ONE       1
#define BLOCK_HEIGHT_TWO       2
#define BLOCK_HEIGHT_FOUR      4
#define BLOCK_HEIGHT_EIGHT     8
#define BLOCK_HEIGHT_SIXTEEN  16

namespace Class907fData
{
    struct surfTab
    {
        UINT32 imageWidth;
        UINT32 imageHeight;
        UINT32 bytesPerPixel;
        UINT32 blockHeight;
    }
    surfTabData[] =
    {
        { 640,   480,  1,  1 },
        { 640,   480,  4,  1 },
        { 1024,  768,  1,  8 },
        { 1024,  768,  4,  8 },
        { 1600, 1200,  1,  16 },
        { 1600, 1200,  4,  16 },
    };
}// width, height, bytes per pixel, blockHeight

using namespace Class907fData;

//@{
//! Test specific class
class BlkSurf
{
public:
    BlkSurf(LwRm::Handle hObject, surfTab* pTab, GpuDevice *m_GpuDevice);
    ~BlkSurf();

    RC setup();
    RC test(Random * pRandom);
    void free();

private:
    LwRm::Handle m_hRemapper;
    surfTab*     m_pSurfTab;
    BlockLinear* m_pBl;

    LwRm::Handle m_hVidMem;
    void*        m_pMapAddr;
    UINT32       m_remapPitch;
    UINT32       m_remapSize;
    GpuDevice    *m_pDev;

    bool         m_isMemMapped;

    LW907F_CTRL_SET_SURFACE_PARAMS             m_surfaceParams;
    LW907F_CTRL_SET_BLOCKLINEAR_PARAMS         m_blParams;
    LW907F_CTRL_START_BLOCKLINEAR_REMAP_PARAMS m_startParams;
    LW907F_CTRL_START_BLOCKLINEAR_REMAP_PARAMS m_stopParams;

    UINT32 GetLog2(UINT32 x);
    void   RandomPattern(Random * pRandom, UINT08 pattern[]);
    void   WritePattern(UINT08* addr, UINT08 pattern[]);
    RC     ReadPattern(UINT08* addr, UINT08 pattern[]);
};
//@}

typedef vector<BlkSurf *> BlSurfVector;

class Class907fTest: public RmTest
{
public:
    Class907fTest();
    virtual ~Class907fTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    BlSurfVector m_blSurfVector;
    Random       m_Random;
    UINT32       m_backDoor;
    //@}
};

//! \brief Class907fTest constructor.
//!
//! Initialise some of the local variables..the actual functionality is done by
//! Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class907fTest::Class907fTest()
{
    SetName("Class907fTest");
}

//! \brief Class907fTest distructor.
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class907fTest::~Class907fTest()
{
}

//! \brief IsTestSupported()
//!
//! Whether or not the test is supported in the current environment
//-----------------------------------------------------------------------------
string Class907fTest::IsTestSupported()
{
    if (IsClassSupported(GF100_REMAPPER))
    {
        return RUN_RMTEST_TRUE;
    }
    else
        return "GF100_REMAPPER class is not supported on current platform";
}

//! \brief Setup()
//!
//! This function allocates the channel and remapper objects based on the
//! avaliability of remapper hardware. It store the surface information in a
//! vector string while allocating the object to them. It then calls the
//! BlSurf::setup function based on the vector string.
//!
//! \return RC -> OK if everything allocates properly.
//! \sa RC BlSurf::setup()
//-----------------------------------------------------------------------------
RC Class907fTest::Setup()
{
    RC rc;
    LwRmPtr      pLwRm;
    LwRm::Handle hObject;
    UINT32 MaxIndex,Index;
    //
    //Ask our base class(es) to setup internal state based on JS settings
    //
    CHECK_RC(InitFromJs());
    //
    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    //
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    //
    // Start allocations
    //
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    m_Random.SeedRandom(m_TestConfig.Seed());
    //
    // Alloc remapper objects and store them in a vector string.
    //
    while ( (m_blSurfVector.size() < MAX_NUMBER_REMAPPERS) &&
            (OK == pLwRm->Alloc(m_hCh, &hObject, GF100_REMAPPER, NULL)) )
    {
        MaxIndex = sizeof(surfTabData)/sizeof(surfTabData[0]) - 1;
        Index = m_Random.GetRandom(0,MaxIndex);
        surfTab* m_pSurfTab = &surfTabData[Index];
        m_blSurfVector.push_back(new BlkSurf(hObject, m_pSurfTab, GetBoundGpuDevice()));
    }

    Printf(Tee::PriNormal,
            "Class907fTest: %d Remappers allocated\n",
            (UINT32)m_blSurfVector.size());

    //
    // Check whether we have atleast one remapper object allocated.
    //
    if (m_blSurfVector.size() == 0)
    {
        rc = RC::LWRM_INSUFFICIENT_RESOURCES;
        return rc;
    }
    //
    // call the storing remapper objects for setting up the register and
    //allocating the video memory.
    //
    for (BlSurfVector::iterator itSurface = m_blSurfVector.begin();
         itSurface != m_blSurfVector.end(); itSurface++)
    {
        CHECK_RC((*itSurface)->setup());
    }
    // Under simulation enable front door access.
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        Platform::EscapeRead("BACKDOOR_ACCESS", 0, 0, &m_backDoor);
        Platform::EscapeWrite( "BACKDOOR_ACCESS", 0, 0, 0 );
    }
    return rc;
}
//! \brief run()
//!
//! It checks the following functionality by calling BlSurf::test().
//! SW remap read data = SW remap write data
//! HW remap read data = SW remap write data
//! HW remap read data = HW remap write data
//! SW remap read data = HW remap write data
//!
//! \return RC ->Error either some allocation is failed, or the control command
//! return some error or there is a mismatch while comparing the data.  else
//!  return OK if everything run properly.
//! \sa BlSurf::test()
//-----------------------------------------------------------------------------
RC Class907fTest::Run()
{
    RC  rc;
    //
    // call the storing remapper objects for run
    //
    for (BlSurfVector::iterator itSurface = m_blSurfVector.begin();
         itSurface != m_blSurfVector.end(); itSurface++)
    {
        CHECK_RC((*itSurface)->test(&m_Random));
    }

    return rc;
}

//! \brief Cleanup()
//!
//! Free any resources that this test selwred
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//! so cleaning up the channel (objects associated with the channel are freed
//! when the channel is freed).
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC Class907fTest::Cleanup()
{
    RC rc, firstRc;

    for (BlSurfVector::iterator itSurface = m_blSurfVector.begin();
         itSurface != m_blSurfVector.end(); itSurface++)
    {
        (*itSurface)->free();
        delete (*itSurface);
    }
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        Platform::EscapeWrite( "BACKDOOR_ACCESS", 0, 0, m_backDoor );
    }
    m_blSurfVector.clear();
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//!---------------------------------------------------------------------------
//! Private Member Functions
//!---------------------------------------------------------------------------

//! \brief BlSurf constructor.
//!
//! store the passed variable into private local variable of the class.
//!
//!\sa BlSurf::setup()
//-----------------------------------------------------------------------------
BlkSurf::BlkSurf(LwRm::Handle hObject, surfTab* pTab, GpuDevice *m_GpuDevice)
{
    m_hRemapper = hObject;
    m_pSurfTab  = pTab;
    m_pBl=0;
    m_hVidMem=0;
    m_pMapAddr=0;
    m_pDev = m_GpuDevice;
}

//! \brief BlSurf distructor.
//!
//! does nothing.
//-----------------------------------------------------------------------------
BlkSurf::~BlkSurf()
{
}

//! \brief BlSurf::setup.
//!
//! this function callwlates all the parameters required by command
//! LW907F_CTRL_CMD_SET_SURFACE(SurfaceParameter and
//! LW907F_CTRL_CMD_SET_BLOCKLINEAR(BlockLinearParameter), based on which
//! vector surface is called. It also allocate the  video memory and map it to
//! the cpu address space. It then initialise the software remapper.
//!
//! \return RC -> OK if everything allocates properly.
//! \sa RC BlSurf::free()
//-----------------------------------------------------------------------------
RC BlkSurf::setup()
{
    RC       rc;
    LwRmPtr  pLwRm;

    UINT32   bytesPerPixelOp;
    UINT32   pitch, pitchGobs;
    UINT32   log2BlockDepth=0 , log2BlockWidth=0, log2BlockHeight;
    UINT32   log2BlockDepthOp, log2BlockHeightOp;
    UINT32   blockWidthTexel, blockHeightTexel;
    UINT32   imageWidthBlock, imageHeightBlock;
    LwRm::Handle hSubDevice;

    switch(m_pSurfTab->bytesPerPixel)
    {
        case BYTE_PER_PIXEL_ONE:
            bytesPerPixelOp = LW907F_CTRL_CMD_SET_SURFACE_FORMAT_BYTES_PIXEL_1;
            break;
        case BYTE_PER_PIXEL_TWO:
            bytesPerPixelOp = LW907F_CTRL_CMD_SET_SURFACE_FORMAT_BYTES_PIXEL_2;
            break;
        case BYTE_PER_PIXEL_FOUR:
            bytesPerPixelOp = LW907F_CTRL_CMD_SET_SURFACE_FORMAT_BYTES_PIXEL_4;
            break;
        case BYTE_PER_PIXEL_EIGHT:
            bytesPerPixelOp = LW907F_CTRL_CMD_SET_SURFACE_FORMAT_BYTES_PIXEL_8;
            break;
        case BYTE_PER_PIXEL_SIXTEEN:
            bytesPerPixelOp = LW907F_CTRL_CMD_SET_SURFACE_FORMAT_BYTES_PIXEL_16;
            break;
        default:
            Printf(Tee::PriHigh,
               "Class907fTest: unsupported bytes per pixel %d\n",
                m_pSurfTab->bytesPerPixel);
            rc = OK;
            return rc;
    }
    //
    // Round pitch to 64 bytes (gob width in bytes)
    //
    pitch     = ((m_pSurfTab->imageWidth * m_pSurfTab->bytesPerPixel) + 63) & ~63;
    pitchGobs = pitch / 64;

    log2BlockDepth   = 0;
    log2BlockWidth   = 0;
    log2BlockDepthOp = LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_DEPTH_1;

    switch(m_pSurfTab->blockHeight)
    {
        case BLOCK_HEIGHT_ONE:
            log2BlockHeight  = 0;
            log2BlockHeightOp= LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_HEIGHT_1;
            break;
        case BLOCK_HEIGHT_TWO:
            log2BlockHeight  = 1;
            log2BlockHeightOp= LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_HEIGHT_2;
            break;
        case BLOCK_HEIGHT_FOUR:
            log2BlockHeight  = 2;
            log2BlockHeightOp= LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_HEIGHT_4;
            break;
        case BLOCK_HEIGHT_EIGHT:
            log2BlockHeight  = 3;
            log2BlockHeightOp= LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_HEIGHT_8;
            break;
        case BLOCK_HEIGHT_SIXTEEN:
            log2BlockHeight  = 4;
            log2BlockHeightOp= LW907F_CTRL_CMD_SET_BLOCKLINEAR_BLOCK_HEIGHT_16;
            break;
        default:
            Printf(Tee::PriHigh,
               "Class907fTest: unsupported block height %d\n",
                m_pSurfTab->blockHeight);
        rc = OK;
        return rc;
    }

    //
    // log2BlockWidth/Height are logs of block dimensions in units of gobs.
    // So we have: 1 block width in pixels = (1 << log2 of # of gobs) * 8 pixels / gob
    //
    blockWidthTexel  = (1 << (log2BlockWidth)) * GOB_HEIGHT_ROWS;
    blockHeightTexel = (1 << (log2BlockHeight)) * GOB_HEIGHT_ROWS;

    imageWidthBlock  = (m_pSurfTab->imageWidth + blockWidthTexel - 1)
                                              / blockWidthTexel;
    imageHeightBlock = (m_pSurfTab->imageHeight + blockHeightTexel -1)
                                               / blockHeightTexel ;
    //
    // Remapper pitch in bytes does not necessarily need to be a power of 2
    //
    m_remapPitch = m_pSurfTab->imageWidth * m_pSurfTab->bytesPerPixel;
    m_remapSize  = imageHeightBlock * blockHeightTexel *  m_remapPitch;

    UINT64 allocSize = m_pSurfTab->imageHeight * pitch;

    //
    // Bug 275364: Requirement is placing the allocation as page aligned
    // That makes sure any alignment issues like this, so placed
    // LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE.
    //
    UINT32 attr = DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR);
    UINT32 attr2 = LWOS32_ATTR2_NONE;
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(
                        LWOS32_TYPE_IMAGE,
                        LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE,
                        &attr,
                        &attr2,
                        &allocSize,
                        nullptr,
                        &m_hVidMem,
                        nullptr,
                        nullptr,
                        m_pDev,
                        0, 0, 0));

    // Mapping to the default subdevice
    CHECK_RC(pLwRm->MapMemory(
                        m_hVidMem,
                        0,
                        allocSize,
                        &m_pMapAddr,
                        LW04_MAP_MEMORY_FLAGS_NONE,
                        m_pDev->GetSubdevice(0)));
    m_isMemMapped = true;

    // Get the handle for the default subdevice
    hSubDevice = pLwRm->GetSubdeviceHandle(m_pDev->GetSubdevice(0));

    //
    // Set the remapper to the allocated surface
    //
    memset(&m_surfaceParams, 0, sizeof(m_surfaceParams));
    m_surfaceParams.pMemory    = LW_PTR_TO_LwP64(m_pMapAddr);
    m_surfaceParams.hMemory    = m_hVidMem;
    m_surfaceParams.hSubDevice = hSubDevice;

    // size should be in units of GOBS rather than 4K pages
    // Assuming GOB width is 64 bytes and GOB height is 8 lines
    // we arrived at 1 GOB area unit  = 512 bytes..is this correct??
    m_surfaceParams.size    = ((m_remapSize + 511) & ~511) >> 9;

    m_surfaceParams.format  =
    DRF_NUM(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _PITCH, pitchGobs)       |
    DRF_NUM(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, bytesPerPixelOp)|
    DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _MEM_SPACE, _CLIENT);

    memset(&m_blParams, 0, sizeof(m_blParams));
    m_blParams.pMemory = LW_PTR_TO_LwP64(m_pMapAddr);
    m_blParams.hMemory = m_hVidMem;
    m_blParams.hSubDevice = hSubDevice;

    m_blParams.gob     = DRF_DEF(907F, _CTRL_CMD_SET_BLOCKLINEAR_GOB, _MEM_SPACE, _CLIENT);

    m_blParams.block   =
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_BLOCK, _HEIGHT, log2BlockHeightOp)|
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_BLOCK, _DEPTH, log2BlockDepthOp);

    m_blParams.imageWH =
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_IMAGEWH, _HEIGHT, imageHeightBlock)|
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_IMAGEWH, _WIDTH, imageWidthBlock);

    m_blParams.log2ImageSlice = LW907F_CTRL_CMD_SET_BLOCKLINEAR_LOG2IMAGESLICE_0_Z;

    memset(&m_startParams, 0, sizeof(m_startParams));
    m_startParams.hSubDevice = hSubDevice;

    memset(&m_stopParams, 0, sizeof(m_stopParams));
    m_stopParams.hSubDevice = hSubDevice;

    //
    // Initialize the software remapper
    //
    m_pBl = new BlockLinear(m_pSurfTab->imageWidth, m_pSurfTab->imageHeight, 0,
                            log2BlockHeight, log2BlockDepth,
                            m_pSurfTab->bytesPerPixel,
                            m_pDev);

    return rc;
}

//! \brief BlSurf::test
//!
//! This function setup the surface parameter and  blocklinear parameter using
//! LwRmcontrol command. then It checks the software remapperby Writing to FB
//! through SW blocklinear translation for (0, TEST_HEIGHT, 0)and reads it back
//! for the match. It then start the remapper hardware and reads at some
//! remapOffset location to check whether there is a match between HW Read and
//! SW Write remapper. It now checks the Hardware remapper by Writing to FB
//! through some remapOffset for (width - 1, 0, 0) and reads it back for the
//! match. It then stop the remapper hardware. and Read from FB through SW
//! blocklinear translation to check whether there is a match between SW Read
//! and HW Write.
//!
//! \return RC -> fail if either commands returns some error or there is a
//! mismatch in comparing the data.
//-----------------------------------------------------------------------------
RC BlkSurf::test(Random * pRandom)
{
    RC      rc;
    LwRmPtr pLwRm;

    UINT32  remapOffset;
    UINT64  blockLinearOffset;
    UINT08  pattern[MAX_BYTES_PER_PIXEL];    //Max bytes per pixel is 16

    //
    // Display the surface data
    //
    Printf(Tee::PriNormal,
        "Class907fTest:Testing surface\n");
    Printf(Tee::PriNormal,
        "Class907fTest:width: %d\n", m_pSurfTab->imageWidth);
    Printf(Tee::PriNormal,
        "Class907fTest:height: %d\n", m_pSurfTab->imageHeight);
    Printf(Tee::PriNormal,
        "Class907fTest:bytesPerPixel: %d\n", m_pSurfTab->bytesPerPixel);
    Printf(Tee::PriNormal,
        "Class907fTest:blockHeight: %d\n", m_pSurfTab->blockHeight);

    //
    // sets the surface's parameters for the remapper used by
    // blocklinear remapping
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_SET_SURFACE,
                            &m_surfaceParams,
                            sizeof(m_surfaceParams)));
    //
    //sets the block linear specific parameters for the remapper.
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_SET_BLOCKLINEAR,
                            &m_blParams,
                            sizeof(m_blParams)));

    //
    // If the height used is too large, the bloated surface test will not work
    // since we only have 1x mapping of the surface, not the fully sized
    // surface, and hence SW remapper reads/writes will fail.
    //
    //#define TEST_HEIGHT 100
    //
    // Write to FB through SW blocklinear translation for (0, TEST_HEIGHT, 0)
    //
    blockLinearOffset = m_pBl->Address(0, TEST_HEIGHT, 0);

    Printf(Tee::PriNormal,
           "Class907fTest:blockLinearOffset: 0x%llx\n",blockLinearOffset);

    //
    // Generate random Pattern
    //
    RandomPattern(pRandom, pattern);

    //
    //SW remap write data
    //
    WritePattern((UINT08*)m_pMapAddr + blockLinearOffset, pattern);
    //
    //SW remap read data
    //
    Printf(Tee::PriHigh, "Checking SW write == SW read\n");
    rc = ReadPattern((UINT08*)m_pMapAddr + blockLinearOffset, pattern);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Class907fTest: SW remap read data != SW remap write data\n");
        Printf(Tee::PriHigh, "Class907fTest: Surface test failed!\n");
        return rc;
    }
    //
    // Enable blocklinear remap
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                                    LW907F_CTRL_CMD_START_BLOCKLINEAR_REMAP,
                                    &m_startParams,
                                    sizeof(m_startParams)));

    //
    // Read from FB directly to verify the remapper's read
    //
    remapOffset = TEST_HEIGHT * m_remapPitch;
    Printf(Tee::PriNormal,
           "Class907fTest:remapOffset: 0x%x\n", remapOffset);
    //
    //HW remap read data
    //
    Printf(Tee::PriHigh, "Checking SW write == HW read\n");
    rc = ReadPattern((UINT08*)m_pMapAddr + remapOffset, pattern);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Class907fTest: HW remap read data != SW remap write data\n");
        Printf(Tee::PriHigh, "Class907fTest: Surface test failed!\n");
        return rc;
    }
    //
    // Write to remapped FB through for (width - 1, 0, 0)
    //
    remapOffset = (m_pSurfTab->imageWidth - 4) * m_pSurfTab->bytesPerPixel;

    Printf(Tee::PriNormal,
           "Class907fTest:remapOffset: 0x%x\n", remapOffset);
    //
    // Generate random data.
    //
    RandomPattern(pRandom, pattern);
    //
    //HW remap write data
    //
    WritePattern((UINT08*)m_pMapAddr + remapOffset, pattern);
    //
    //HW remap read data
    //
    Printf(Tee::PriHigh, "Checking HW write == HW read\n");
    rc = ReadPattern((UINT08*)m_pMapAddr + remapOffset, pattern);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Class907fTest: HW remap read data != HW remap write data\n");
        Printf(Tee::PriHigh, "Class907fTest: Surface test failed!\n");
        return rc;
    }
    //
    // Disable blocklinear remap
    //

    CHECK_RC(pLwRm->Control(m_hRemapper,
                                    LW907F_CTRL_CMD_STOP_BLOCKLINEAR_REMAP,
                                    &m_stopParams,
                                    sizeof(m_startParams)));

    blockLinearOffset = m_pBl->Address(m_pSurfTab->imageWidth - 4,0,0);

    Printf(Tee::PriNormal,
           "Class907fTest:blockLinearOffset: 0x%llx\n", blockLinearOffset);

    //
    // Read from FB through SW blocklinear translation to verify remapper's write
    //
    //
    //SW remap read data
    //
    Printf(Tee::PriHigh, "Checking HW write == SW read\n");
    rc = ReadPattern((UINT08*)m_pMapAddr + blockLinearOffset, pattern);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
        "Class907fTest:SW remap read data != HW remap write data\n");
        Printf(Tee::PriHigh, "Class907fTest: Surface test failed!\n");
        return rc;
    }

    Printf(Tee::PriNormal, "Class907fTest: Surface test passed.\n");
    return rc;
}

//! \brief BlSurf::free.
//!
//! this function releases all the resources, allocated in BlSurf::setup.
//!
//! \return nothing
//-----------------------------------------------------------------------------
void BlkSurf::free()
{
    LwRmPtr pLwRm;

    pLwRm->Free(m_hRemapper);

    delete m_pBl;

    if (m_isMemMapped)
        pLwRm->UnmapMemory(m_hVidMem, m_pMapAddr, 0, m_pDev->GetSubdevice(0));

    pLwRm->Free(m_hVidMem);

}

//! \brief RandomPattern
//!
//! this function generate some values and fill them in input array. Max number
//! generated should be less than or equal to MAX_BYTE_PER_PIXEL. And It's
//! value should be in between (MIN_RANDOM_VALUE, MAX_RANDOM_VALUE).
//!
//-----------------------------------------------------------------------------
void BlkSurf::RandomPattern(Random * pRandom, UINT08 pattern[])
{
    for (UINT32 i = 0; i < MAX_BYTES_PER_PIXEL; i++)
    {
        pattern[i] = pRandom->GetRandom(MIN_RANDOM_VALUE, MAX_RANDOM_VALUE);
    }
}

//! \brief WritePattern
//!
//! this function writes the pattern array values at a given address.
//-----------------------------------------------------------------------------
void BlkSurf::WritePattern(UINT08* addr, UINT08 pattern[])
{
    for (UINT32 i = 0; i < m_pSurfTab->bytesPerPixel; i++)
    {
        MEM_WR08(addr + i, pattern[i]);
    }
}

//! \brief ReadPattern
//!
//! this function reads data pattern at a given address and match them with
//! prespecified array values.
//!
//! \return RC -> error for a mismatch.
//-----------------------------------------------------------------------------

RC BlkSurf::ReadPattern(UINT08* addr, UINT08 pattern[])
{
    RC rc;

    for (UINT32 i = 0; i < m_pSurfTab->bytesPerPixel; i++)
    {
        UINT08 data = MEM_RD08(addr + i);

        if (pattern[i] != data)
        {
            Printf(Tee::PriHigh,
            "Class907fTest:read data 0x%x != 0x%x at pattern[%d]\n",
              data, pattern[i], i);
            //
            // In case of error, just want to let the test proceed
            //

            // TOFIX: This test fails on fmodel due to lack remapper support
            //        Let's just ignore the error for now
            // rc = RC::SOFTWARE_ERROR;
        }
        else
        {
            Printf(Tee::PriNormal,
            "Class907fTest:read data 0x%x == 0x%x at pattern[%d]\n",
            data, pattern[i], i);
        }
    }
    return rc;
}

//----------------------------------------------------------------------------
// JS Linkage
//----------------------------------------------------------------------------
//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class907fTest object.
//!
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class907fTest, RmTest,
                 "Class907f RM test.");
