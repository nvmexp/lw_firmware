/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//!
//! \file bl_remapper.cpp
//! \brief This file implements the bl_remapperTest class.
//!
//! This file implements the bl_remapperTest class. This tests the 8
//! Host block linear remappers. The test steps are as follows:
//!
//! (1) Setup/Enable Host Block Linear Remappers.
//! (2) Write image data to block buffers through host.
//! (4) Verify pitch buffers match source.
//! (5) Write new data to pitch buffers.
//! (6) Block Linear DMA Copy to block buffers.
//! (7) Verify block buffers match reference via Host remappers.
//!
//! if (4) and (7) match then the test passes.
//!

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
//
#include "mdiag/tests/stdtest.h"

#include "bl_remapper.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "sim/IChip.h"
#include "lwos.h" // RM defines
#include "core/include/utility.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/sysspec.h"
#include <sstream>
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/tests/gpu/host/kepler/kepler_bl_remapper.h"
#include "mdiag/tests/gpu/host/pascal/pascal_bl_remapper.h"
#include "mdiag/tests/gpu/host/turing/turing_bl_remapper.h"
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc1b5.h" // PASCAL_DMA_COPY_B
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A

#define MAX_RANDOM_NUMBER_VALUE 255

#define T_NAME "bl_remapperTest"

using namespace Sys;

/*
This test writes to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
*/

// Test Parameters
extern const ParamDecl bl_remapper_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-num_remappers", "Sets the number of BL remappers to use"),
    {"-img_width",  "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the image width in pixels. Format: <remapper>:<width>"},
    {"-img_height", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the image height in pixels. Format: <remapper>:<height>"},
    {"-img_depth",  "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the image depth in pixels. Format: <remapper>:<depth>"},
    {"-img_bpp",    "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the image bytes per pixel. Format: <remapper>:<bpp>"},
    {"-gob_log_height_block", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the log of block height in Gobs. Format: <remapper>:<log val>"},
    {"-gob_log_depth_block",  "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets the log of block depth in Gobs. Format: <remapper>:<log val>"},
    {"-copy_offset", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Sets a byte offset to use to start the transfer. Offset needs to be a multiple of img_width*img_bpp. Format: <remapper>:<offset>"},
    UNSIGNED_PARAM("-copy_gobs",            "Number of GOBS to copy, defaults to size of smallest image"),
    SIMPLE_PARAM("-dump_surfaces",          "Dumps all the surfaces and the reference surface on failure"),
    {"-dump_surface", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Dumps surface n to disk."},
    SIMPLE_PARAM("-use_surface_position", "Assign 0x0-0xFF incrementally to the surface rather than use random data."),
    { "-block_loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                          ParamDecl::GROUP_START), 0, 2, "Memory Location for blocklinear surfaces, 0=fb, 1=sys_coh, 2=sys_ncoh"},
    { "-block_fb", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Put block linear surface in framebuffer"},
    { "-block_sys_coh", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Put block linear surface in coherent sysmem"},
    { "-block_sys_ncoh", "2", ParamDecl::GROUP_MEMBER, 0, 0, "Put block linear surface in noncoherent sysmem"},
    { "-pitch_loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                          ParamDecl::GROUP_START), 0, 2, "Memory Location for pitchlinear surfaces, 0=fb, 1=sys_coh, 2=sys_ncoh"},
    { "-pitch_fb", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Put pitch linear surface in framebuffer"},
    { "-pitch_sys_coh", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Put pitch linear surface in coherent sysmem"},
    { "-pitch_sys_ncoh", "2", ParamDecl::GROUP_MEMBER, 0, 0, "Put pitch linear surface in noncoherent sysmem"},
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    SIMPLE_PARAM("-no_backdoor",            "Disable backdoor accesses, for "
        "setup and some CRC checking. The actual test is always conducted "
        "frontdoor.  This is needed in situations where a platform does not "
        "support backdoor accesses, or backdoor and frontdoor cannot be safely "
        "toggled with an EscapeWrite command. Note on emulation and silicon the "
        "test always uses frontdoor."),
    SIMPLE_PARAM("-bl_to_pitch_burst",      "Test that the remappers deal with a bursted transaction that crosses a pitch/block boundary. All options but the block location options are ignored if this is set."),
    UNSIGNED_PARAM("-pcie_burst_size",      "Tells the test what PCIE burst size is being used for the test. This is used by the burst size test to ensure the burst crosses a pitch/block boundary."),
    UNSIGNED_PARAM("-print_interval",       "Number of bytes read/written between status updates."),
    LAST_PARAM
};

// Setup test factory
STD_TEST_FACTORY(bl_remapper, bl_remapperTest)

//! Test Constructor
//!
//! Reads and setup the command line parameters. Also initializes some class variables.
//!
bl_remapperTest::bl_remapperTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader)
{
    LwRmPtr pLwRm;
    vector<UINT32> ceClasses = EngineClasses::GetClassVec("Ce");

    pLwRm->GetFirstSupportedClass(
        ceClasses.size(),
        ceClasses.data(), 
        &copyClassToUse, 
        ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice());

    // Create the test depending on available memcpy class
    if (copyClassToUse == KEPLER_DMA_COPY_A || copyClassToUse == MAXWELL_DMA_COPY_A) {
        remapperTestChip.reset(new KeplerBlockLinearRemapperTest(*this, reader));
    } else if (copyClassToUse == PASCAL_DMA_COPY_A || copyClassToUse == PASCAL_DMA_COPY_B ||
               copyClassToUse == VOLTA_DMA_COPY_A) {
        remapperTestChip.reset(new PascalBlockLinearRemapperTest(*this, reader));
    } else if (copyClassToUse == TURING_DMA_COPY_A || copyClassToUse == AMPERE_DMA_COPY_A ||
               copyClassToUse == AMPERE_DMA_COPY_B || copyClassToUse == HOPPER_DMA_COPY_A ||
               copyClassToUse == BLACKWELL_DMA_COPY_A) {
        remapperTestChip.reset(new TuringBlockLinearRemapperTest(*this, reader));
    } else {
        MASSERT(0);
    }
}

//! Destructor
bl_remapperTest::~bl_remapperTest(void)
{
    // again, nothing special
    CleanUp();
}

//! Additional test setup work.
//!
//! This method verifies parameters.
//! Initializes the imageBuffer
//! Creates the copy object
//! Allocates the DMA surfaces
//! Sets up any semaphores
int bl_remapperTest::Setup(void)
{
    int local_status = 1;

    GpuDevice* pGpuDevice = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);

    if (!SetupLWGpuResource(1, &copyClassToUse))
    {
        return 0;
    }

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        vector<UINT32> supportedCEs;
        lwgpu->GetSupportedCeNum(pGpuSubdevice, &supportedCEs, LwRmPtr().Get());
        MASSERT(supportedCEs.size() != 0);
        engineId = MDiagUtils::GetCopyEngineId(supportedCEs[0]);
    }

    // call parent's Setup first
    if ((local_status = SetupSingleChanTest(engineId)))
    {
        local_status = remapperTestChip->Setup(lwgpu, ch);
    }

    return local_status;
}

//! Cleans up anything allocated by the test
void bl_remapperTest::CleanUp(void)
{
    DebugPrintf("bl_remapperTest::CleanUp() starts\n");

    if (remapperTestChip.get() != NULL) remapperTestChip->CleanUp();

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("bl_remapperTest::CleanUp() done\n");
}

//! Run the test
//!
void bl_remapperTest::Run(void)
{
    MASSERT(remapperTestChip.get() != NULL);
    remapperTestChip->Run();
}

///////////////////////////////////////////////////////////////////////////////
// BlockLinearRemapperTest Methods
///////////////////////////////////////////////////////////////////////////////

//! Test Constructor
//!
//! Reads and sets up the command line parameters. Also initializes some class variables.
//!
//! \param parent The parent object of type class bl_remapperTest. This is needed to set test status.
//! \param params Test arguments
//! \param lwgpu Pointer to an LWGpuResource object for this test.
//! \param ch Pointer to an LWGpuChannel object to use for the test.
BlockLinearRemapperTest::BlockLinearRemapperTest(bl_remapperTest &parent,
  ArgReader *reader) :
    m_lwgpu(NULL),
    m_ch(NULL),
    imgWidth(NULL),
    imgHeight(NULL),
    imgDepth(NULL),
    imgPitch(NULL),
    imgBpp(NULL),
    imgSize(NULL),
    burstImgSize(NULL),
    imgTotalGobs(NULL),
    imgLogBlockHeight(NULL),
    imgLogBlockWidth(NULL),
    imgLogBlockDepth(NULL),
    copyOffset(NULL),
    copySize(0),
    remapNumBytesEnum(NULL),
    imgColorFormat(NULL),
    pitchLocation(Memory::Fb),
    blockLocation(Memory::Fb),
    dumpSurface(NULL),
    m_buffInfo(NULL),
    m_enableBackdoor(true),
    usePositionData(false),
    initFail(false),
    dumpSurfaces(false),
    blPitchBurst(false),
    printStatusInterval(0),
    params(reader),
    blockBuffer(NULL),
    pitchBuffer(NULL),
    semaphoreBuffer(NULL),
    copyHandle(0),
    imageBuffer(NULL),
    tmpBuffer(NULL),
    m_testObject(parent),
    m_backupRegisters(NULL),
    numRemappers(0),
    GOB_WIDTH_BYTES(0),
    GOB_HEIGHT_IN_LINES(0),
    GOB_BYTES(0),
    blockSurfacePteKind(0),
    pitchSurfacePteKind(0)
{
    pGpuDev = 0;
}

//! Destructor - Just clean up
BlockLinearRemapperTest::~BlockLinearRemapperTest(void)
{
    CleanUp();
}

//! Additional test setup work.
//!
//! This method verifies parameters.
//! Initializes the imageBuffer
//! Creates the copy object
//! Allocates the DMA surfaces
//! Sets up any semaphores
int BlockLinearRemapperTest::Setup(LWGpuResource *lwgpu, LWGpuChannel* ch)
{
    int local_status = 1;

    MASSERT(lwgpu != NULL);
    MASSERT(ch != NULL);
    if((lwgpu == NULL) || (ch==NULL)) {
        local_status = 0;
        return local_status;
    }

    m_lwgpu = lwgpu;
    m_ch = ch;

    SetupDev();
    
    // Parse some parameters
    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);
    printStatusInterval = params->ParamUnsigned("-print_interval", 0);

    numRemappers = params->ParamUnsigned("-num_remappers", 8);
    if(numRemappers == 0)
    {
        ErrPrintf("bl_remapperTest: Set num_remappers to 0");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
    }

    pcieBurstSize = params->ParamUnsigned("-pcie_burst_size", 64);
    if(params->ParamPresent("-dump_surfaces"))
    {
        dumpSurfaces = true;
    }
    else
    {
        dumpSurfaces = false;
    }

    if(params->ParamPresent("-bl_to_pitch_burst"))
    {
        blPitchBurst = true;
    }

    // allocate our member variables
    if(AllocateMembers() != 0)
    {
        local_status = 0;
    }

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        dumpSurface[i] = false;
    }

    // Backup the registers
    if(m_backupRegisters)
    {
        BackupHostBlRegisters();
    }
    if(blPitchBurst)
    {
        local_status = local_status && SetupBoundarySplitTest();
    }
    else
    {
        local_status = local_status && SetupBasicBlockLinearRemapperTest();
    }
    // Print out buffer allocations
    m_buffInfo->Print(nullptr, lwgpu->GetGpuDevice());
    return local_status;
}

//! Run the test
//!
void BlockLinearRemapperTest::Run(void)
{
    if(blPitchBurst)
    {
        RunBoundarySplitTest();
    }
    else
    {
        RunBasicBlockLinearRemapperTest();
    }
}

//! Allocates all array members of the class
//! \return 0 on success
int BlockLinearRemapperTest::AllocateMembers(void)
{
    // Allocate all our arrays/surfaces
    if(imgWidth == NULL)
    {
        imgWidth = new UINT32[numRemappers];
    }
    if(imgWidth == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgWidth array.");
        return 1;
    }
    if(imgHeight == NULL)
    {
        imgHeight = new UINT32[numRemappers];
    }
    if(imgHeight == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgHeight array.");
        return 1;
    }
    if(imgDepth == NULL)
    {
        imgDepth = new UINT32[numRemappers];
    }
    if(imgDepth == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgDepth array.");
        return 1;
    }
    if(imgPitch == NULL)
    {
        imgPitch = new UINT32[numRemappers];
    }
    if(imgPitch == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgPitch array.");
        return 1;
    }
    if(imgBpp == NULL)
    {
        imgBpp = new UINT32[numRemappers];
    }
    if(imgBpp == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgBpp array.");
        return 1;
    }
    if(imgSize == NULL)
    {
        imgSize = new UINT32[numRemappers];
    }
    if(imgSize == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgSize array.");
        return 1;
    }
    if(burstImgSize == NULL)
    {
        burstImgSize = new UINT32[numRemappers];
    }
    if(burstImgSize == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate burstImgSize array.");
        return 1;
    }
    if(imgTotalGobs == NULL)
    {
        imgTotalGobs = new UINT32[numRemappers];
    }
    if(imgTotalGobs == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgTotalGobs array.");
        return 1;
    }
    if(imgLogBlockHeight == NULL)
    {
        imgLogBlockHeight = new UINT32[numRemappers];
    }
    if(imgLogBlockHeight == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgLogBlockHeight array.");
        return 1;
    }
    if(imgLogBlockWidth == NULL)
    {
        imgLogBlockWidth = new UINT32[numRemappers];
    }
    if(imgLogBlockWidth == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgLogBlockWidth array.");
        return 1;
    }
    if(imgLogBlockDepth == NULL)
    {
        imgLogBlockDepth = new UINT32[numRemappers];
    }
    if(imgLogBlockDepth == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgLogBlockDepth array.");
        return 1;
    }
    if(copyOffset == NULL)
    {
        copyOffset = new UINT32[numRemappers];
    }
    if(copyOffset == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate copyOffset array.");
        return 1;
    }
    if(remapNumBytesEnum == NULL)
    {
        remapNumBytesEnum = new UINT32[numRemappers];
    }
    if(remapNumBytesEnum == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate remapNumBytesEnum array.");
        return 1;
    }
    if(imgColorFormat == NULL)
    {
        imgColorFormat = new ColorUtils::Format[numRemappers];
    }
    if(imgColorFormat == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imgColorFormat array.");
        return 1;
    }
    if(dumpSurface == NULL)
    {
        dumpSurface = new bool[numRemappers];
    }
    if(dumpSurface == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate dumpSurface array.");
        return 1;
    }
    if(m_buffInfo == NULL)
    {
        m_buffInfo = new BuffInfo();
    }
    if(m_buffInfo == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate m_buffInfo array.");
        return 1;
    }
    if(m_backupRegisters == NULL)
    {
        m_backupRegisters = new bl_registers[numRemappers];
    }
    if(m_backupRegisters == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate m_backupRegisters array.");
        return 1;
    }

    if(blockBuffer == NULL)
    {
        blockBuffer = new MdiagSurf[numRemappers];
    }
    if(blockBuffer == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate blockBuffer array.");
        return 1;
    }
    if(pitchBuffer == NULL)
    {
        pitchBuffer = new MdiagSurf[numRemappers];
    }
    if(pitchBuffer == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate pitchBuffer array.");
        return 1;
    }
    if(semaphoreBuffer == NULL)
    {
        semaphoreBuffer = new MdiagSurf[numRemappers];
    }
    if(semaphoreBuffer == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate semaphoreBuffer array.");
        return 1;
    }
    if(imageBuffer == NULL)
    {
        imageBuffer = new UINT08*[numRemappers];
    }
    if(imageBuffer == NULL)
    {
        ErrPrintf(T_NAME" - Unable to allocate imageBuffer array.");
        return 1;
    }
    memset(imageBuffer, 0, sizeof(UINT08*)*numRemappers);

    return 0;
}

//!
//! Cleans up anything allocated by the test
void BlockLinearRemapperTest::CleanUp(void)
{
    DebugPrintf("bl_remapperTest::CleanUp() starts\n");
    // Restore all remapper registers to the previous value.
    if(m_backupRegisters != NULL)
    {
        RestoreHostBlRegisters();
        delete[] m_backupRegisters;
        m_backupRegisters = NULL;
    }

    // free the surface and objects we used for drawing
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        if ( blockBuffer != NULL &&
             blockBuffer[i].GetSize() )
        {
            blockBuffer[i].Unmap();
            blockBuffer[i].Free();
        }
        if ( pitchBuffer != NULL &&
             pitchBuffer[i].GetSize() )
        {
            pitchBuffer[i].Unmap();
            pitchBuffer[i].Free();
        }
        if ( semaphoreBuffer != NULL &&
             semaphoreBuffer[i].GetSize() )
        {
            semaphoreBuffer[i].Unmap();
            semaphoreBuffer[i].Free();
        }
    }

    if(imageBuffer != NULL)
    {
        for(int i = 0 ; i < numRemappers ; ++i)
        {
            if(imageBuffer[i] != NULL)
            {
                delete[] imageBuffer[i];
                imageBuffer[i] = NULL;
            }
        }
    }

    if(imgWidth)
    {
        delete[] imgWidth;
        imgWidth = NULL;
    }

    if(imgHeight)
    {
        delete[] imgHeight;
        imgHeight = NULL;
    }

    if(imgDepth)
    {
        delete[] imgDepth;
        imgDepth = NULL;
    }

    if(imgPitch)
    {
        delete[] imgPitch;
        imgPitch = NULL;
    }

    if(imgBpp)
    {
        delete[] imgBpp;
        imgBpp = NULL;
    }

    if(imgSize)
    {
        delete[] imgSize;
        imgSize = NULL;
    }

    if(burstImgSize)
    {
        delete[] burstImgSize;
        burstImgSize = NULL;
    }

    if(imgTotalGobs)
    {
        delete[] imgTotalGobs;
        imgTotalGobs = NULL;
    }

    if(imgLogBlockHeight)
    {
        delete[] imgLogBlockHeight;
        imgLogBlockHeight = NULL;
    }

    if(imgLogBlockWidth)
    {
        delete[] imgLogBlockWidth;
        imgLogBlockWidth = NULL;
    }

    if(imgLogBlockDepth)
    {
        delete[] imgLogBlockDepth;
        imgLogBlockDepth = NULL;
    }

    if(copyOffset)
    {
        delete[] copyOffset;
        copyOffset = NULL;
    }

    if(remapNumBytesEnum)
    {
        delete[] remapNumBytesEnum;
        remapNumBytesEnum = NULL;
    }

    if(imgColorFormat)
    {
        delete[] imgColorFormat;
        imgColorFormat = NULL;
    }

    if(dumpSurface)
    {
        delete[] dumpSurface;
        dumpSurface = NULL;
    }

    if(m_buffInfo)
    {
        delete m_buffInfo;
        m_buffInfo = NULL;
    }

    if(blockBuffer)
    {
        delete[] blockBuffer;
        blockBuffer = NULL;
    }

    if(pitchBuffer)
    {
        delete[] pitchBuffer;
        pitchBuffer = NULL;
    }

    if(semaphoreBuffer)
    {
        delete[] semaphoreBuffer;
        semaphoreBuffer = NULL;
    }

    if(imageBuffer)
    {
        delete[] imageBuffer;
        imageBuffer = NULL;
    }

    if ( copyHandle )
    {
        m_ch->DestroyObject(copyHandle);
        copyHandle = 0;
    }

    if ( tmpBuffer )
    {
        delete[] tmpBuffer;
        tmpBuffer = NULL;
    }

    // These are allocated in a different object
    // NULL them so we don't accidently use them after this.
    m_lwgpu = NULL;
    m_ch = NULL;
    DebugPrintf("bl_remapperTest::CleanUp() done\n");
}

//! Initialize and allocate the Surface objects
int BlockLinearRemapperTest::SetupSurfaces(void)
{
    // Need to allocate space for initial CPU write, one per host remapper
    // Create these in the graphics framebuffer.

    DebugPrintf("bl_remapperTest::SetupSurfaces() will create %d source DMA"
                " buffers\n", numRemappers);

    for(int i = 0 ; i < numRemappers ; ++i) {
        InfoPrintf(T_NAME"::SetupSurfaces() creating blocklinear buffer of size:"
                   " %dx%dx%d %d bpp %d bytes. GobLogHeight = %d GobLogDepth = %d"
                   " GobLogWidth = %d\n", imgHeight[i], imgWidth[i], imgDepth[i],
                   imgBpp[i], imgSize[i], imgLogBlockHeight[i], imgLogBlockDepth[i],
                   imgLogBlockWidth[i]);
        // Call Setup to set surface size and memory parameters
        // Set the width has actual width times bytesperpixel
        blockBuffer[i].SetWidth((imgWidth[i] * imgBpp[i]));
        blockBuffer[i].SetPitch(blockBuffer[i].GetWidth());
        blockBuffer[i].SetHeight(imgHeight[i]);
        blockBuffer[i].SetDepth(imgDepth[i]);
        blockBuffer[i].SetLocation(blockLocation);
        blockBuffer[i].SetProtect(Memory::ReadWrite);
        blockBuffer[i].SetLogBlockHeight(imgLogBlockHeight[i]);
        blockBuffer[i].SetLogBlockWidth(imgLogBlockWidth[i]);
        blockBuffer[i].SetLogBlockDepth(imgLogBlockDepth[i]);
        // Colorutils::Y8 ensures that bitsperpixel is 8. Hence we multiplied width with bpp
        blockBuffer[i].SetColorFormat(ColorUtils::Y8);
         // it was a bug now fixed: should use Surface::BlockLinear instead of Surface::Pitch
        blockBuffer[i].SetLayout(Surface::BlockLinear);
        blockBuffer[i].SetAlignment(512);
        // If we're sysmem then we want reflection to test the GPU, if it is in gpu mem then this shouldn't matter
        blockBuffer[i].SetMemoryMappingMode(Surface::MapReflected);

        // Set the page kind to that for generic 8-128 bits per sample setting
        // with block-linear
        blockBuffer[i].SetPteKind(blockSurfacePteKind);

        // Allocate the gpu memory
        if (OK != blockBuffer[i].Alloc(pGpuDev))
        {
            ErrPrintf("can't create src buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Map the memory into the address space
        if (OK != blockBuffer[i].Map())
        {
            ErrPrintf("can't map the source buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Bind the surface to a GPU Channel
        if (OK != blockBuffer[i].BindGpuChannel(m_ch->ChannelHandle()))
        {
            ErrPrintf("can't bind the src buffer %d to channel, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Add surface info to buffInfo
        std::stringstream buffName;
        buffName << "blockBuffer[" << i << "]";
        blockBuffer[i].SetName(buffName.str());
        m_buffInfo->SetRenderSurface(blockBuffer[i]);

        InfoPrintf("Created and mapped source DMA buffer %d at 0x%x\n",
                   i, blockBuffer[i].GetCtxDmaOffsetGpu());
    } // end for

    // we need a destination surface for the copy copy in FB, allocate it.
    DebugPrintf("bl_remapperTest::SetupSurfaces() will create %d destination"
                " DMA buffers\n", numRemappers);

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME"::SetupSurface() creating pitch buffer of size: %dx%dx%d %d bpp %d bytes.\n",
                   imgHeight[i], imgWidth[i], imgDepth[i], imgBpp[i], imgSize[i]);
        // Setup size and memory parameters
        pitchBuffer[i].SetWidth(imgSize[i]);
        pitchBuffer[i].SetHeight(1);
        pitchBuffer[i].SetLocation(pitchLocation);
        pitchBuffer[i].SetProtect(Memory::ReadWrite);
        pitchBuffer[i].SetColorFormat(ColorUtils::Y8);
        pitchBuffer[i].SetLayout(Surface::Pitch);
        pitchBuffer[i].SetAlignment(512);
        // If we're sysmem then we want reflection to test the GPU, if it is in gpu mem then this shouldn't matter
        pitchBuffer[i].SetMemoryMappingMode(Surface::MapReflected);

        // Set kind to pitch
        pitchBuffer[i].SetPteKind(pitchSurfacePteKind);

        // Allocate the memory
        if (OK != pitchBuffer[i].Alloc(pGpuDev))
        {
            ErrPrintf("can't create pitch buffer, line %d\n", __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Map it into the CPU/GPU's memory space.
        if (OK != pitchBuffer[i].Map())
        {
            ErrPrintf("can't map the pitch buffer, line %d\n", __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }
        // Bind the surface to a GPU Channel
        if (OK != pitchBuffer[i].BindGpuChannel(m_ch->ChannelHandle()))
        {
            ErrPrintf("can't bind the pitch buffer %d to channel, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Add surface info to buffInfo
        std::stringstream buffName;
        buffName << "pitchBuffer[" << i << "]";
        pitchBuffer[i].SetName(buffName.str());
        m_buffInfo->SetRenderSurface(pitchBuffer[i]);

        InfoPrintf("Created and mapped destination DMA buffer %d at 0x%x\n",
                   i, pitchBuffer[i].GetCtxDmaOffsetGpu());
    } // end for

    return 1;
}

//! Sets up the semaphore buffers and writes an initial value
int BlockLinearRemapperTest::SetupSemaphores(void)
{
    //Now we need a semaphore Surface
    DebugPrintf("bl_remapperTest::Setup() will create a 16 byte semaphore Surface\n");

    int semaphoreSize = 16; // Semaphore surfaces are 16 bytes

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        semaphoreBuffer[i].SetWidth(semaphoreSize);
        semaphoreBuffer[i].SetHeight(1);
        semaphoreBuffer[i].SetColorFormat(ColorUtils::Y8); // A byte color format

        semaphoreBuffer[i].SetLocation(Memory::Coherent); // Cached Sysmem
        semaphoreBuffer[i].SetProtect(Memory::ReadWrite);

        if (OK != semaphoreBuffer[i].Alloc(pGpuDev))
        {
            ErrPrintf("can't create semaphore buffer, line %d\n", __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        if (OK != semaphoreBuffer[i].Map())
        {
            ErrPrintf("can't map the semaphore buffer, line %d\n", __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        if (OK != semaphoreBuffer[i].BindGpuChannel(m_ch->ChannelHandle()))
        {
            ErrPrintf("can't bind the semaphore %d to channel, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Add surface info to buffInfo
        std::stringstream buffName;
        buffName << "semaphoreBuffer[" << i << "]";
        semaphoreBuffer[i].SetName(buffName.str());
        m_buffInfo->SetRenderSurface(semaphoreBuffer[i]);

        // Initialize the Semaphore
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()), 0x33);
    }

    return 1;
}

//! \brief Enables the backdoor access
void BlockLinearRemapperTest::EnableBackdoor()
{
    // Turn off backdoor accesses
    if(m_enableBackdoor && (Platform::GetSimulationMode() != Platform::Hardware))
    {
        DebugPrintf("Enabling backdoor");
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void BlockLinearRemapperTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
        DebugPrintf("Disabling backdoor");
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}

//! This Parses the Program Parameters
int BlockLinearRemapperTest::ParseParameters(void)
{

    // Set defaults for Width height depth
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imgWidth[i] = 64;
        imgHeight[i] = 8;
        imgDepth[i] = 1;
        imgBpp[i] = 4;
        imgLogBlockHeight[i] = 0;
        imgLogBlockWidth[i] = 0;
        imgLogBlockDepth[i] = 0;

        copyOffset[i] = 0; // start at the beginning of image by default
    }

    // Setup the images
    int result = 0;
    result |= ParseImgArg("-img_width", imgWidth, numRemappers);
    result |= ParseImgArg("-img_height", imgHeight, numRemappers);
    result |= ParseImgArg("-img_depth", imgDepth, numRemappers);
    result |= ParseImgArg("-img_bpp", imgBpp, numRemappers);
    result |= ParseImgArg("-copy_offset", copyOffset, numRemappers);

    if(result)
    {
        ErrPrintf(T_NAME"::ParseParameters() - Unable to parse parameters.\n");

        return 0;
    }

    if(params->ParamPresent("-use_surface_position"))
    {
        usePositionData = true;
    }

    // Setup dump surface array
    if(params->ParamPresent("-dump_surface"))
    {
        const vector<string>* cmdList =
            params->ParamStrList("-dump_surface", NULL);

        for(auto sIter = cmdList->begin(); sIter != cmdList->end() ; ++sIter)
        {

            int arg = 0;
            const char* argString = sIter->c_str();

            sscanf(argString, "%d", &arg);

            if(arg < numRemappers)
            {
                DebugPrintf("bl_remapperTest::ParseParameters() -dump_surface: %d.\n", arg);
                dumpSurface[arg] = true;
            }
        }
    }

    // Calc image sizes
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imgSize[i] = imgWidth[i]*imgHeight[i]*imgBpp[i]*imgDepth[i];
        imgPitch[i] = imgWidth[i]*imgBpp[i];
    }

    // Check to see if copyGOBS is to large
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        if(copySize > imgSize[i] + copyOffset[i])
        {
            ErrPrintf("bl_remapperTest::ParseParameters() - Invalid -copy_gobs setting. Copy size larger than size + offset for image %d.\n", i);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }
    }

    InfoPrintf(T_NAME"::ParseParameters() - Size of copy to perform: %d bytes.\n", copySize);

    ParseImgArg("-gob_log_height_block", imgLogBlockHeight, numRemappers);
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imgLogBlockWidth[i] = LWC8B5_SET_SRC_BLOCK_SIZE_WIDTH_ONE_GOB;
    }

    ParseImgArg("-gob_log_depth_block", imgLogBlockDepth, numRemappers);

    m_enableBackdoor = !(params->ParamPresent("-no_backdoor") > 0);

    int tempParam = params->ParamUnsigned("-block_loc", 0);

    switch(tempParam)
    {
        case 0:
            blockLocation = Memory::Fb;
            break;
        case 1:
            blockLocation = Memory::Coherent;
            break;
        case 2:
            blockLocation = Memory::NonCoherent;
            break;
        default:
            break;
    }
    tempParam = params->ParamUnsigned("-pitch_loc", 0);

    switch(tempParam)
    {
        case 0:
            pitchLocation = Memory::Fb;
            break;
        case 1:
            pitchLocation = Memory::Coherent;
            break;
        case 2:
            pitchLocation = Memory::NonCoherent;
            break;
        default:
            break;
    }

    return 1;
}

//! \brief Parses one of the image arguements
//!
//! Reads and parses an image arguement
//!
//! \param param Parameter to process
//! \param buffer Buffer to store the values in
//! \param size Number of entries in the buffer
//!
//! \return 0 on success
int BlockLinearRemapperTest::ParseImgArg(const char *param, UINT32 *buffer, UINT32 size)
{
    if(!param)
    {
        return 1;
    }

    if(params->ParamPresent(param))
    {
        const vector<string>* cmdList =
            params->ParamStrList(param, NULL);

        for(auto sIter = cmdList->begin() ;
                sIter != cmdList->end() ; ++sIter)
        {
            unsigned int remapNumber = 0;
            unsigned int arg = 0;
            const char* argString = sIter->c_str();

            sscanf(argString, "%u:%u", &remapNumber, &arg);

            if(remapNumber < size)
            {
                DebugPrintf("bl_remapperTest::ParseImgArg() arg: %s, remapper: %d, value: %d.\n", param, remapNumber, arg);
                buffer[remapNumber] = arg;
            }
            else
            {
                return 2;
            }
        }
    }
    return 0;
}

//! Writes the images to the graphics surfaces
//!
//! \param dst Array of MdiagSurf objects to write.
void BlockLinearRemapperTest::ZeroBuffers(MdiagSurf *dst)
{

    MASSERT(dst != NULL);    
    if(dst == NULL)
    {
        return;
    }

    InfoPrintf("bl_remapperTest::ZeroBuffers() - Zero out the whole Buffer\n");
    uintptr_t address = 0;

    for(int i = 0 ; i < numRemappers ; i++) {
        for(UINT32 j = 0 ; j < dst[i].GetSize() ; j+=8)
        {
            address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset[i] + j ;
            MEM_WR64(address, UINT64(0));
        }
    }
}

//! Writes the images to the graphics surfaces
//!
//! \param dst Array of MdiagSurf objects to write. 
void BlockLinearRemapperTest::WriteBuffers(MdiagSurf *dst, const Memory::Location &blockLocation)
{
    MASSERT(dst != NULL);
    if(dst == NULL)
    {
        return;
    }

    // the following commented out code is for the situation where we need to colwert from NAIVE_BL to XBAR_RAW
    // we dont need that now, but good to keep it as a reference 
    //if ((blockLocation==Memory::Coherent) || (blockLocation==Memory::NonCoherent)) {
    //    InfoPrintf("bl_remapperTest::WriteBuffers() - Colwert dst[i] buffer from NAIVE_BL to XBAR_RAW \n");
    //    for(int i = 0 ; i < numRemappers ; i++) {
    //        PixelFormatTransform::ColwertBetweenRawAndNaiveBlSurf(imageBuffer[i], imgSize[i], 
    //                                                              PixelFormatTransform::NAIVE_BL,
    //                                                              PixelFormatTransform::XBAR_RAW, pGpuDev);
    //    }
    //}
    //else {
    //    InfoPrintf("bl_remapperTest::WriteBuffers() - blockLocation is %d and is not COH/NON-COH MEMORY. using NAIVE_BL format\n", blockLocation);
    //}
    //InfoPrintf("bl_remapperTest::WriteBuffers() - Memzero the whole image buffers prior to writing them with actual values\n");

    uintptr_t address = 0;
    UINT32 byteCount = 0;
    InfoPrintf("bl_remapperTest::WriteBuffers() - Writing the image buffers.\n");
    for(UINT32 j = 0 ; j < copySize ; ++j, ++byteCount)
    {
        if(printStatusInterval && (byteCount > printStatusInterval)) {
            InfoPrintf("Read %d bytes from FB, on byte %d of %d.\n", printStatusInterval, j*numRemappers, copySize*numRemappers);
            byteCount = 0;
        }
        for(int i = 0 ; i < numRemappers ; ++i)
        {
            address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset[i] + j ;
            MEM_WR08(address, imageBuffer[i][copyOffset[i] + j]);
        }
    }
}

//! Reads the dma buffer and compares it to the reference image
//!
//! \param dst Array of MdiagSurfs to read and compare
//! \return Returns 1 on a match, 0 if no match
int BlockLinearRemapperTest::CheckBuffers(MdiagSurf *dst)
{
    UINT08 value;
    int rtnStatus = 1;
    UINT32 byteCount = 0;

    // read back the dst surface and compare it with the original
    InfoPrintf("bl_remapperTest::CheckBuffers() - reading back and comparing surfaces\n");
    //for (int i = (int)copySize - 1; i >= 0; --i) {
    for(UINT32 i = 0 ; i < copySize ; ++i, ++byteCount)
    {
        if(printStatusInterval && (byteCount > printStatusInterval)) {
            InfoPrintf("Read %d bytes from FB, on byte %d of %d.\n", printStatusInterval, i*numRemappers, copySize*numRemappers);
            byteCount = 0;
        }
        for(int j = 0 ; j < numRemappers ; ++j)
        {
            value = MEM_RD08( reinterpret_cast<uintptr_t>(dst[j].GetAddress())+copyOffset[j] + i);
            if (value != imageBuffer[j][copyOffset[j] + i])
            {
                ErrPrintf("Dst surface %d does not equal src surface at offset 0x%08x (src = 0x%02x, dst = 0x%02x) at line %d\n",
                           j, i+copyOffset[j], imageBuffer[j][copyOffset[j] + i], value, __LINE__);
                rtnStatus = 0;
                //break;
            }
        }
    }

    return rtnStatus;
}

//! Initializes the source imageBuffer.
void BlockLinearRemapperTest::InitSrcBuffer(UINT08 *buffer, UINT32 size)
{

    for (UINT32 i = 0; i < size; ++i)
    {
        if(usePositionData) {
            buffer[i] = i;
        }
        else
        {
            buffer[i] = static_cast<UINT08>(RndStream->RandomUnsigned(0, MAX_RANDOM_NUMBER_VALUE));
        }
    }
}

//! \brief Dumps all the test surfaces
//!
//! Dumps all the test surfaces as a raw binary file for debugging purposes.
void BlockLinearRemapperTest::DumpSurfaces(void)
{

    // Backdoor this for speed
    EnableBackdoor();

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        FILE *refOut;
        FILE *pitchOut;
        FILE *blockOut;

        char filename[256];

        if(dumpSurfaces || dumpSurface[i])
        {
            // Backdoor is on so we get raw data.
            sprintf(filename, "reference_remapper_%d.raw", i);
            if(Utility::OpenFile(filename, &refOut, "wb") != OK)
            {
                ErrPrintf(T_NAME"::DumpSurfaces() - Unable to open file %s for writing.\n", filename);
            }

            if (imgSize[i] != fwrite(imageBuffer[i], sizeof(UINT08), imgSize[i], refOut))
            {
                ErrPrintf(T_NAME"::DumpSurfaces() - Failure writing to file %s\n", filename);
            }
            fclose(refOut);

            sprintf(filename, "pitch_remapper_%d.raw", i);
            if(Utility::OpenFile(filename, &pitchOut, "wb") != OK)
            {
                ErrPrintf(T_NAME"::DumpSurfaces() - Unable to open file %s for writing.\n", filename);
            }

            for(UINT32 j = 0 ; j < imgSize[i] ; ++j)
            {
                UINT08 value = MEM_RD08( reinterpret_cast<uintptr_t>(pitchBuffer[i].GetAddress()) + j );
                if (1 != fwrite(&value, sizeof(value), 1, pitchOut))
                {
                    ErrPrintf(T_NAME"::DumpSurfaces() - Failure writing to file %s\n", filename);
                }
            }
            fclose(pitchOut);

            sprintf(filename, "block_remapper_%d.raw", i);
            if(Utility::OpenFile(filename, &blockOut, "wb") != OK)
            {
                ErrPrintf(T_NAME"::DumpSurfaces() - Unable to open file %s for writing.\n", filename);
            }

            for(UINT32 j = 0 ; j < imgSize[i] ; ++j)
            {
                UINT08 value = MEM_RD08( reinterpret_cast<uintptr_t>(blockBuffer[i].GetAddress()) + j );
                if (1 != fwrite(&value, sizeof(value), 1, blockOut))
                {
                    ErrPrintf(T_NAME"::DumpSurfaces() - Failure writing to file %s\n", filename);
                }
            }
            fclose(blockOut);
        }
    }

    // Turn back off the backdoor
    DisableBackdoor();
}

//! The test
//!
//! This tests the host block linear functionality.
//! The procedure is as follows:
//!
//! (1) Setup/Enable Host Block Linear Remappers.
//! (2) Write image data to block buffers through host. 
//! (4) Verify pitch buffers match source.
//! (5) Write new data to pitch buffers.
//! (6) Block Linear DMA Copy to block buffers.
//! (7) Verify block buffers match reference via Host remappers.
//!
//! if (4) and (7) match then the test passes.
void BlockLinearRemapperTest::RunBasicBlockLinearRemapperTest(void)
{
    m_testObject.SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("bl_remapperTest::Run() starts\n");

    int local_status = 1;
    RC rc;

    // Turn off the backdoor
    DisableBackdoor();

    InfoPrintf("Copying surfaces.\n");

    InfoPrintf(T_NAME"::Run() - Setting up host remappers\n");
    // Setup Host Remappers
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        if(SetupHostBLRemapper(i, blockBuffer[i], blockBuffer[i], 0, 0))
        {
            ErrPrintf("Unable to setup the Host BL remapper %d.\n", i);
            m_testObject.SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    InfoPrintf(T_NAME"::Run() - Writing Surfaces from CPU.\n");
    // Write Image
    WriteBuffers(blockBuffer, blockLocation);

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME": Writing buffer semaphore %d.\n", i);
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()), CPU_SEMAPHORE_VALUE);
    }

    InfoPrintf("bl_remapperTest::Run() Doing Block2PitchDma.\n");
    // Do the DMA
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        DoBlock2PitchDma(blockBuffer[i], pitchBuffer[i], semaphoreBuffer[i], i);
    }

    if (rc == RC::TIMEOUT_ERROR)
    {
        ErrPrintf(T_NAME":Run() - TIMEOUT ERROR\n");
        m_testObject.SetStatus(Test::TEST_FAILED);
        return;
    }
    else if (rc != OK)
    {
        ErrPrintf(T_NAME":Run() - Some other error: %s\n", rc.Message());
        m_testObject.SetStatus(Test::TEST_FAILED);
        return;
    }

    InfoPrintf(T_NAME"::Run() - Checking Semaphores.\n");

    // wait for completion of the DMA
    UINT32 semaphoreValue;
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        // Read the Semaphore
        do
        {
            m_testObject.DoYield(); // Let other work get done
            semaphoreValue = MEM_RD32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()));

        } while(semaphoreValue != DMA_SEMAPHORE_RELEASE_VALUE);
    }

    InfoPrintf(T_NAME"::Run() - Checking surfaces.\n");
    // read in the destination data and check it;
    local_status = local_status && CheckBuffers(pitchBuffer);

    if(!local_status)
    {
        ErrPrintf(T_NAME"::Run() - Surface failed comparison.\n");
        m_testObject.SetStatus(Test::TEST_FAILED);

        DumpSurfaces();
        return;
    }

    InfoPrintf(T_NAME"::Run() - Creating a new Image.\n");
    // Alter the imagebuffers
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InitSrcBuffer(imageBuffer[i], imgSize[i]);
    }

    InfoPrintf(T_NAME"::Run() - Writing the buffer to pitch surfaces.\n");
    // Write the new imagebuffer to the pitch surfaces
    WriteBuffers(pitchBuffer, pitchLocation);

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME"::Run() - Writing buffer %d.\n", i);
        // Update the semaphores
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()), CPU_SEMAPHORE_VALUE);
    }

    InfoPrintf(T_NAME"::Run() - Doing pitch to block DMA.\n");
    // Do the DMA
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME"::Run() - Doing DMA %d.\n", i);
        DoPitch2BlockDma(pitchBuffer[i], blockBuffer[i], semaphoreBuffer[i], i);
    }

    InfoPrintf(T_NAME"::Run() - Waiting for DMA to complete.\n");

    InfoPrintf(T_NAME"::Run() - Checking Semaphores.\n");
    // wait for completion of the DMA
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        // Read the Semaphore
        do
        {
            m_testObject.DoYield(); // Let other work get done
            semaphoreValue = MEM_RD32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()));
        } while(semaphoreValue != DMA_SEMAPHORE_RELEASE_VALUE);
    }

    InfoPrintf(T_NAME"::Run() - Check block buffers.\n");
    // read in the destination data and check it;
    local_status = local_status && CheckBuffers(blockBuffer);
    if(!local_status)
    {
        ErrPrintf(T_NAME"::Run() - Surface failed comparison.\n");
        m_testObject.SetStatus(Test::TEST_FAILED);
        return;
    }

    // Turn back on the backdoor
    EnableBackdoor();
    InfoPrintf("Done with bl_remapper test\n");

    if(local_status)
    {
        m_testObject.SetStatus(Test::TEST_SUCCEEDED);
        m_testObject.getStateReport()->crcCheckPassedGold();
    }
    else
    {

        m_testObject.SetStatus(Test::TEST_FAILED);
    }

    return;

}

//! Sets up the test, allocates and initializes surfaces
int BlockLinearRemapperTest::SetupBasicBlockLinearRemapperTest(void)
{
    int local_status = 1;

    DebugPrintf("bl_remapperTest::Setup() starts\n");

    // Add channel data to buffinfo
    m_ch->GetBuffInfo(m_buffInfo, "Channel 0");

    pGpuDev = m_lwgpu->GetGpuDevice();

    // Parse Parameters
    if(!ParseParameters())
    {
        ErrPrintf("bl_remapperTest:Setup(): Invalid Parameters.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Allocate the image buffers
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imageBuffer[i] = new UINT08[imgSize[i]];
        if(!imageBuffer[i])
        {
            ErrPrintf("bl_remapperTest:Setup(): Unable to create imageBuffer.\n");
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }
    }

    // Initialize our surface data.
    RndStream = new RandomStream(seed0, seed1, seed2);
    // Initialize the imgBuffer
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InitSrcBuffer(imageBuffer[i], imgSize[i]);
    }

    // create the copy object
    copyHandle = m_ch->CreateObject(copyClassToUse);
    if (!copyHandle)
    {
        ErrPrintf("Couldn't Create copy class object\n");
        return 0;
    }
    DebugPrintf("Copy class object created \n");

    // Set subchannel
    RC rc;
    CHECK_RC(m_ch->GetSubChannelNumClass(copyHandle, &subChannel, 0));
    CHECK_RC(m_ch->SetObjectRC(subChannel, copyHandle));
    DebugPrintf("Subchannel %d set\n", subChannel);

    // Setup the source and destination surfaces
    if(!SetupSurfaces())
    {
        ErrPrintf("Could not allocate Surfaces.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Setup the source and destination semaphores
    if(!SetupSemaphores())
    {
        ErrPrintf("Could not allocate Semapore.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Callwlate the total size of the surfaces in gobs.
    MASSERT(GOB_WIDTH_BYTES != 0);
    MASSERT(GOB_HEIGHT_IN_LINES != 0);
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imgTotalGobs[i] = (imgPitch[i] / GOB_WIDTH_BYTES +
                           ((imgPitch[i]%GOB_WIDTH_BYTES) ? 1:0)) *
                          (imgHeight[i] / GOB_HEIGHT_IN_LINES +
                           ((imgHeight[i]% GOB_HEIGHT_IN_LINES) ? 1:0)) *
                          imgDepth[i];
    }

    // we need a copy object as well
    DebugPrintf("bl_remapperTest::Setup() creating class objects\n");

    //Setup the state report
    m_testObject.getStateReport()->enable();

    // wait for GR idle
    local_status = local_status && m_ch->WaitForIdle();
    m_testObject.SetStatus(local_status?(Test::TEST_SUCCEEDED):(Test::TEST_FAILED));

    // if unsuccessful, clean up and exit
    if(!local_status)
    {
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return 0;
    }

    DebugPrintf("bl_remapperTest::Setup() class object created\n");
    if(!m_enableBackdoor)
    {
        // Turn off backdoor accesses
        DisableBackdoor();
    }

    DebugPrintf("bl_remapperTest::Setup() done succesfully\n");
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// Burst Test Methods
///////////////////////////////////////////////////////////////////////////////

//! Does a burst write to the passed in surface at the given offest.
//!
//! \param dst MdiagSurf offset corresponding to buffer to write.
//! \param data Data to write to the buffer.
//! \param offset Write is done at address = dst base addr + offset.
//! \param burstSize Size of the burst sent over PCIE.
void BlockLinearRemapperTest::DoBurstWrite(const MdiagSurf &dst, UINT08 *data, UINT32 offset, UINT32 burstSize)
{
    DebugPrintf(T_NAME"::DoBurstWrite() - Writing %d bytes to Surface at: 0x%08x, at offset: 0x%x\n",
                burstSize, dst.GetCtxDmaOffsetGpu(), offset);
    uintptr_t address = reinterpret_cast<uintptr_t>(dst.GetAddress()) + offset;
    Platform::VirtualWr(reinterpret_cast<void*>(address), data, burstSize);
}

//! Does the pitch block burst transfer to the buffer
//!
//! \param dst Array of Surface2D objects to write.
//! \param burstSize Size of the bursts sent over PCIE.
//void BlockLinearRemapperTest::PitchBlockBurstWriteBuffers(Surface *dst, UINT32 burstSize, bool swizzle)
//{
//    MASSERT(dst != NULL);
//    if(dst == NULL)
//    {
//        return;
//    }
//
//    InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing the image buffers.\n");
//    // We want to start the write to the image by half of the burst size.
//    UINT32 burstOffset = burstSize / 2;
//
//    for(int i = 0 ; i < numRemappers ; ++i)
//    {
//        UINT32 blockSurfaceStart = imgSize[i];
//        UINT32 blockSurfaceEnd = blockSurfaceStart + imgSize[i];
//        UINT32 copyOffset = blockSurfaceStart - copySize/2 + burstOffset;
//
//        uintptr_t address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) +
//                         copyOffset;
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Working on buffer and remapper %i.\n", i);
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing to: %p, from: %p, %d bytes.\n", address, &imageBuffer[i][copyOffset], copySize);
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing at offset: 0x%08x.\n", copyOffset);
//        Platform::VirtualWr(reinterpret_cast<void*>(address), &imageBuffer[i][copyOffset], copySize - burstOffset);
//
//        copyOffset = blockSurfaceEnd - copySize/2 + burstOffset;
//        address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset;
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing to: %p, from: %p, %d bytes.\n", address, &imageBuffer[i][copyOffset], copySize);
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing at offset: 0x%08x.\n", copyOffset);
//        Platform::VirtualWr(reinterpret_cast<void*>(address), &imageBuffer[i][copyOffset], copySize - burstOffset);
//    }
//}
//! Does the pitch block burst transfer to the buffer
//!
//! \param dst Array of Surface2D objects to write.
//! \param burstSize Size of the bursts sent over PCIE.
//void BlockLinearRemapperTest::PitchBlockBurstWriteBuffersNoBurst(Surface *dst, UINT32 burstSize)
//{
//    MASSERT(dst != NULL);
//    if(dst == NULL)
//    {
//        return;
//    }
//
//    InfoPrintf(T_NAME"::PitchBlockBurstWriteBuffersNoBurst() - Writing the image buffers.\n");
//    // We want to start the write to the image by half of the burst size.
//    UINT32 burstOffset = burstSize / 2;
//
//    for(int i = 0 ; i < numRemappers ; ++i)
//    {
//        UINT32 blockSurfaceStart = imgSize[i];
//        UINT32 blockSurfaceEnd = blockSurfaceStart + imgSize[i];
//        UINT32 copyOffset = blockSurfaceStart - copySize/2 + burstOffset;
//
//        uintptr_t address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset;
//
//        for(int j = 0 ; j < copySize - burstOffset; ++j) {
//            InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Working on buffer and remapper %i.\n", i);
//            InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing to: %p, from: %p, %d bytes.\n", address, &imageBuffer[i][copyOffset], copySize);
//            InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing at offset: 0x%08x.\n", copyOffset);
//            Platform::VirtualWr(reinterpret_cast<void*>(address), &imageBuffer[i][copyOffset], copySize - burstOffset);
//        }
//
//        copyOffset = blockSurfaceEnd - copySize/2 + burstOffset;
//        address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset;
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing to: %p, from: %p, %d bytes.\n", address, &imageBuffer[i][copyOffset], copySize);
//        InfoPrintf("bl_remapperTest::PitchBlockBurstWriteBuffers() - Writing at offset: 0x%08x.\n", copyOffset);
//        Platform::VirtualWr(reinterpret_cast<void*>(address), &imageBuffer[i][copyOffset], copySize - burstOffset);
//    }
//}

//! Reads the dma buffer and compares it to the reference image
//!
//! \param dst Array of MdiagSurfs to read and compare
//! \param burstSize Size of the bursts sent over PCIE.
//! \return Returns 1 on a match, 0 if no match
int BlockLinearRemapperTest::PitchBlockBurstCheckBuffers(MdiagSurf *dst, UINT32 burstSize)
{
    int rtnStatus = 1;

    MASSERT(dst != NULL);
    if(dst == NULL)
    {
        return 0;
    }

    // read back the dst surface and compare it with the original
    InfoPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - reading back and comparing surfaces\n");

    // We want to start the write to the image by half of the burst size.
    UINT32 burstOffset = burstSize / 2;

    // Allocate space to read back the data, temporary variable that's reused.
    // Alloced/Freed once per call given this method should only be called once per test
    // this shouldn't hurt perf too badly.
    UINT08 *readBuf = new UINT08[copySize];
    MASSERT(readBuf != NULL);
    if(readBuf == NULL)
    {
        ErrPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - Unable to allocate space for temp readback surface.\n");
    }

    for(int i = 0 ; i < numRemappers ; ++i)
    {
        UINT32 blockSurfaceStart = imgSize[i];
        UINT32 blockSurfaceEnd = blockSurfaceStart + imgSize[i];
        UINT32 copyOffset = blockSurfaceStart - copySize/2 + burstOffset;

        uintptr_t address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) +
                         copyOffset;

        DebugPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - Reading from: %p, to: %p, %d bytes.\n", address, readBuf, copySize - burstOffset);

        Platform::VirtualRd(reinterpret_cast<void*>(address), readBuf, copySize - burstOffset);

        DebugPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - Checking read value starting at offset: 0x%08x.\n",copyOffset);
        for( UINT32 j = burstOffset ; j < copySize - burstOffset ; ++j)
        {
            if(readBuf[j] != imageBuffer[i][copyOffset+j])
            {
                ErrPrintf("Dst surface %d does not equal src surface at offset 0x%08x (src = 0x%02x, dst = 0x%02x) at line %d\n",
                           i, j+copyOffset, imageBuffer[i][copyOffset + j], readBuf[j], __LINE__);
                rtnStatus = 0;
                break;
            }
        }

        copyOffset = blockSurfaceEnd - copySize/2 + burstOffset;
        address = reinterpret_cast<uintptr_t>(dst[i].GetAddress()) + copyOffset;
        Platform::VirtualRd(reinterpret_cast<void*>(address), readBuf, copySize - burstOffset);

        DebugPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - Reading from: %p, to: %p, %d bytes.\n", address, readBuf, copySize - burstOffset);
        DebugPrintf("bl_remapperTest::PitchBlockBurstCheckBuffers() - Checking read value starting at offset: 0x%08x.\n",copyOffset);
        for( UINT32 j = burstOffset ; j < copySize - burstOffset ; ++j)
        {
            if(readBuf[j] != imageBuffer[i][copyOffset+j])
            {
                ErrPrintf("Dst surface %d does not equal src surface at offset 0x%08x (src = 0x%02x, dst = 0x%02x) at line %d\n",
                           i, j+copyOffset, imageBuffer[i][copyOffset + j], readBuf[j], __LINE__);
                rtnStatus = 0;
                break;
            }
        }
    }

    if(readBuf)
    {
        delete[] readBuf;
        readBuf = NULL;
    }

    return rtnStatus;
}

//! Setups up a test which tests transaction splitting on pitch/block boundaries
//!
//! Setup ilwolves parsing some parameters, allocating surfaces, and initializing surfaces.
//! See SetupBoundarySplitTestSurfaces for more details on surface allocation.
int BlockLinearRemapperTest::SetupBoundarySplitTest(void)
{
    int local_status = 1;

    DebugPrintf(T_NAME"::SetupBoundarySplitTest() starts\n");

    // Add channel data to buffinfo
    m_ch->GetBuffInfo(m_buffInfo, "Channel 0");

    pGpuDev = m_lwgpu->GetGpuDevice();

    // Parse Parameters
    if(!ParseParameters())
    {
        ErrPrintf(T_NAME"::SetupBoundarySplitTest(): Invalid Parameters.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Set the size for all buffers
    for(int i = 0 ; i < numRemappers ; ++i) {
        burstImgSize[i] = imgSize[i] * 2;
    }

    // Allocate the tmp buffer
    MASSERT(tmpBuffer == NULL);
    tmpBuffer = new UINT08[pcieBurstSize];

    // Allocate the image buffers
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imageBuffer[i] = new UINT08[pcieBurstSize];
        if(!imageBuffer[i])
        {
            ErrPrintf(T_NAME"::SetupBoundarySplitTest(): Unable to create imageBuffer.\n");
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }
    }

    // Initialize our surface data.
    RndStream = new RandomStream(seed0, seed1, seed2);
    // Create a temp buffer.
    // Initialize the imgBuffer
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InitSrcBuffer(imageBuffer[i], pcieBurstSize);
    }

    // Setup the source and destination surfaces
    if(!SetupBoundarySplitSurfaces())
    {
        ErrPrintf("Could not allocate Surfaces.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Setup the source and destination semaphores
    if(!SetupSemaphores())
    {
        ErrPrintf("Could not allocate Semapore.\n");
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }

    // Callwlate the total size of the surfaces in gobs.
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        imgTotalGobs[i] = (imgPitch[i] / GOB_WIDTH_BYTES +
                          ((imgPitch[i]%GOB_WIDTH_BYTES) ? 1:0)) *
                          (imgHeight[i] / GOB_HEIGHT_IN_LINES +
                          ((imgHeight[i]% GOB_HEIGHT_IN_LINES) ? 1:0)) *
                          imgDepth[i];
    }

    // we need a copy object as well
    DebugPrintf(T_NAME"::SetupBoundarySplitTest() creating class objects\n");

    //Setup the state report
    m_testObject.getStateReport()->enable();

    // wait for GR idle
    local_status = local_status && m_ch->WaitForIdle();
    m_testObject.SetStatus(local_status?(Test::TEST_SUCCEEDED):(Test::TEST_FAILED));

    // if unsuccessful, clean up and exit
    if(!local_status)
    {
        m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return 0;
    }

    DebugPrintf(T_NAME"::SetupBoundarySplitTest() class object created\n");

    if(!m_enableBackdoor)
    {
        // Turn off backdoor accesses
        DisableBackdoor();
    }

    DebugPrintf(T_NAME"::SetupBoundarySplitTest() done succesfully\n");
    return 1;
}

//! Runs the Boundary Split test.
//!
//! Test run procedure is as follows.
//! (1) Setup the host block linear remappers, so that the surface to remapped, and the block surface do not overlap.
//! (2) Do a burst write which crosses a pitch/block boundary
//! (3) Turn off the remappers, and read the pitch data + overlap into bl space to make sure nothing was clobberd
//! (4) Read back the block linear data, also read extra data round the block linear data to make sure we didn't do weird clobbering
//! (5) Write some new pitch and block linear data
//! (6) Do a burst read and verify the data is correct
void BlockLinearRemapperTest::RunBoundarySplitTest(void)
{
    m_testObject.SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("bl_remapperTest::Run() starts\n");

    int local_status = 1;

    InfoPrintf("Copying surfaces.\n");

    // Disable the back door
    DisableBackdoor();

    InfoPrintf(T_NAME"::Run() - Setting up host remappers\n");
    // (1) Setup the host block linear remappers, so that the surface to remapped, and the block surface do not overlap.
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        uint offset = imgSize[i];
        if(SetupHostBLRemapper(i, pitchBuffer[i], blockBuffer[i], offset, offset))
        {
            ErrPrintf("Unable to setup the Host BL remapper %d.\n", i);
            m_testObject.SetStatus(Test::TEST_FAILED);
            return;
        }
    }

    InfoPrintf(T_NAME"::Run() - Writing Surfaces from CPU.\n");
    // (2) Do a burst write which crosses a pitch/block boundary
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        UINT32 offset = imgSize[i] - pcieBurstSize/2;
        InfoPrintf(T_NAME"::Run() - Writing surface %d.\n", i);

        InfoPrintf(T_NAME"::Run() - Doing Pitch2Block split.\n", i);
        DoBurstWrite(pitchBuffer[i], imageBuffer[i], offset, pcieBurstSize);

        offset = imgSize[i] + imgSize[i] - pcieBurstSize/2;
        InfoPrintf(T_NAME"::Run() - Doing Block2Pitch split.\n", i);
        DoBurstWrite(pitchBuffer[i], imageBuffer[i], offset, pcieBurstSize);
    }

    // (3) Turn off the remappers, and read the pitch data + overlap into bl space to make sure nothing was clobberd
    // (4) Read back the block linear data, also read extra data round the block linear data to make sure we didn't do weird clobbering
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        DisableRemapper(i);
        if(!VerifyBoundarySplitWrite(i))
        {
            ErrPrintf(T_NAME"::Run() - Error in block linear write boundary split for remapper %d.\n", i);
            local_status = 0;
        }
    }
    // (5) Write some new pitch and block linear data
    // (6) Do a burst read and verify the data is correct
    // TODO FINISH THE SPLIT TEST

    InfoPrintf(T_NAME"::Run() - Checking Semaphores.\n");

    // wait for completion of the DMA
    UINT32 semaphoreValue;
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        // Read the Semaphore
        do
        {
            m_testObject.DoYield(); // Let other work get done
            semaphoreValue =  MEM_RD32(reinterpret_cast<uintptr_t>(semaphoreBuffer[i].GetAddress()));

        } while(semaphoreValue != CPU_SEMAPHORE_VALUE);
    }

    InfoPrintf(T_NAME"::Run() - Checking surfaces.\n");
    // read in the destination data and check it;
    local_status = local_status && PitchBlockBurstCheckBuffers(blockBuffer, pcieBurstSize);

    if(!local_status)
    {
        ErrPrintf(T_NAME"::Run() - Surface failed comparison.\n");
        m_testObject.SetStatus(Test::TEST_FAILED);

        DumpSurfaces();
        return;
    }

    // Turn back on the backdoor
    EnableBackdoor();
    InfoPrintf("Done with bl_remapper test\n");

    if(local_status)
    {
        m_testObject.SetStatus(Test::TEST_SUCCEEDED);
        m_testObject.getStateReport()->crcCheckPassedGold();
    }
    else
    {

        m_testObject.SetStatus(Test::TEST_FAILED);
    }

    return;
}

//! Does all surface allocations for the Boundary split test.
//!
//! Setup ilwolves allocating two surfaces which are 2x the size of the
//! test image.
//! The remappers will be setup so that the first 1/4 of the allocated
//! surface is pitch.
//! Followed by a region of memory, 1/2 the size of the surface
//! which will be configured to be remapped.
//! The last 1/4 of the surface will also be pitch.
//! The remapped region of the first surface will be set as the block linear pitch base.
//! The center of the second surface will be set as the block linear block base.
int BlockLinearRemapperTest::SetupBoundarySplitSurfaces(void)
{
    // Need to allocate space for initial CPU write, one per host remapper
    // Create these in the graphics framebuffer.

    DebugPrintf(T_NAME"::SetupBoundarySplitSurfaces() will create %d pitch buffers to get remapped"
                " buffers\n", numRemappers);
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME"::SetupBoundarySplitSurfaces() creating pitch buffer of size: %dx%dx%d %d bpp %d bytes.\n",
                   imgHeight[i], imgWidth[i], imgDepth[i], imgBpp[i], burstImgSize[i]);
        // Call Setup to set surface size and memory parameters
        pitchBuffer[i].SetWidth(burstImgSize[i]);
        pitchBuffer[i].SetPitch(pitchBuffer[i].GetWidth());
        pitchBuffer[i].SetHeight(1);
        pitchBuffer[i].SetLocation(blockLocation);
        pitchBuffer[i].SetProtect(Memory::ReadWrite);
        pitchBuffer[i].SetColorFormat(ColorUtils::Y8);
        pitchBuffer[i].SetLayout(Surface::Pitch);
        pitchBuffer[i].SetAlignment(512);

        // Set the page kind to that for generic 8-128 bits per sample setting
        // with block-linear
        pitchBuffer[i].SetPteKind(blockSurfacePteKind);

        // Allocate the gpu memory
        if (OK != pitchBuffer[i].Alloc(pGpuDev))
        {
            ErrPrintf("can't create src buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Map the memory into the address space
        if (OK != pitchBuffer[i].Map())
        {
            ErrPrintf("can't map the source buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Bind the surface to a GPU Channel
        if (OK != pitchBuffer[i].BindGpuChannel(m_ch->ChannelHandle()))
        {
            ErrPrintf("can't bind the src buffer %d to channel, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Add surface info to buffInfo
        std::stringstream buffName;
        buffName << "remapSrc[" << i << "]";
        pitchBuffer[i].SetName(buffName.str());
        m_buffInfo->SetRenderSurface(pitchBuffer[i]);

        InfoPrintf("Created and mapped src buffer %d at 0x%x\n",
                   i, pitchBuffer[i].GetCtxDmaOffsetGpu());
    } // end for

    DebugPrintf(T_NAME"::SetupBoundarySplitSurfaces() will create %d block linear"
                " buffers\n", numRemappers);
    for(int i = 0 ; i < numRemappers ; ++i)
    {
        InfoPrintf(T_NAME"::SetupBoundarySplitSurfaces() creating blocklinear buffer of size: %dx%dx%d %d bpp %d bytes.\n",
                   imgHeight[i], imgWidth[i], imgDepth[i], imgBpp[i], burstImgSize[i]);
        // Call Setup to set surface size and memory parameters
        blockBuffer[i].SetWidth(burstImgSize[i]);
        blockBuffer[i].SetPitch(blockBuffer[i].GetWidth());
        blockBuffer[i].SetHeight(1);
        blockBuffer[i].SetLocation(blockLocation);
        blockBuffer[i].SetProtect(Memory::ReadWrite);
        blockBuffer[i].SetColorFormat(ColorUtils::Y8);
        blockBuffer[i].SetLayout(Surface::Pitch);
        blockBuffer[i].SetAlignment(512);

        // Set the page kind to that for generic 8-128 bits per sample setting
        // with block-linear
        blockBuffer[i].SetPteKind(blockSurfacePteKind);

        // Allocate the gpu memory
        if (OK != blockBuffer[i].Alloc(pGpuDev))
        {
            ErrPrintf("can't create dst buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Map the memory into the address space
        if (OK != blockBuffer[i].Map())
        {
            ErrPrintf("can't map the dst buffer %d, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Bind the surface to a GPU Channel
        if (OK != blockBuffer[i].BindGpuChannel(m_ch->ChannelHandle()))
        {
            ErrPrintf("can't bind the dst buffer %d to channel, line %d\n", i, __LINE__);
            m_testObject.SetStatus(Test::TEST_NO_RESOURCES);
            return 0;
        }

        // Add surface info to buffInfo
        std::stringstream buffName;
        buffName << "remapDst[" << i << "]";
        blockBuffer[i].SetName(buffName.str());
        m_buffInfo->SetRenderSurface(blockBuffer[i]);

        InfoPrintf("Created and mapped dst buffer %d at 0x%x\n",
                   i, blockBuffer[i].GetCtxDmaOffsetGpu());
    } // end for

    return 1;
}

//! Colwerts a pitch offset into a block offset.
//!
//! \param offset            Offset in pitch space.
//! \param pitch             Surface pitch in bytes.
//! \param imgHeight         Surface height in lines.
//! \param pitchInGobs       Surface pitch in GOBS.
//! \param imgSizeInGobs     Image size in GOBS.
//! \param imgHeightInGobs   Image height in GOBS.
//! \param imgLogBlockDepth  Log base 2 of the depth of a block in GOBS.
//! \param imgLogBlockHeight Log base 2 of the height of a block in GOBS.
UINT32 BlockLinearRemapperTest::PitchToBlockOffset(
    const UINT32 offset,
    const UINT32 pitch,
    const UINT32 imgHeight,
    const UINT32 pitchInGobs,
    const UINT32 imgSizeInGobs,
    const UINT32 imgHeightInGobs,
    const UINT32 imgLogBlockDepth,
    const UINT32 imgLogBlockHeight
)
{
    UINT32 imgDepth = imgSizeInGobs / (pitchInGobs * imgHeightInGobs);
    UINT32 blockHeightInGobs = (1 << imgLogBlockHeight);
    UINT32 blockHeight = blockHeightInGobs * GOB_HEIGHT_IN_LINES;
    UINT32 imgHeightInBlocks = (imgHeightInGobs % blockHeightInGobs) ? (imgHeightInGobs / blockHeightInGobs + 1) : (imgHeightInGobs / blockHeightInGobs);
    UINT32 blockDepth =  (1 << imgLogBlockDepth);
    UINT32 blockPlaneSize = blockHeightInGobs * GOB_BYTES;
    UINT32 blockSize = blockPlaneSize * blockDepth;
    UINT32 row = (offset / pitch) % imgHeight;
    UINT32 plane = (offset / (pitch * imgHeight));
    UINT32 planeWithinBlock = plane % blockDepth;
    UINT32 blockPlane = plane / blockDepth;
    UINT32 blockColumn = (offset % pitch) / GOB_BYTES;
    UINT32 blockRow = row / blockHeight;
    UINT32 rowWithinBlock = row % blockHeight;

    UINT32 block = (blockPlane * pitchInGobs * imgHeightInBlocks) + (blockRow * pitchInGobs) +  blockColumn;
    UINT32 gobOffset = offset % GOB_WIDTH_BYTES;
    UINT32 blockOffset = (block * blockSize) + (planeWithinBlock * blockPlaneSize) + (rowWithinBlock * GOB_WIDTH_BYTES) + gobOffset;

    MASSERT(plane < imgDepth); // Error if pitch offset translates to block offset outside the image depth
    if(plane < imgDepth) {
        ErrPrintf(T_NAME"::PitchToBlockOffset - pitch offset translates to block offset outside the image depth.\n");
        blockOffset = 0;
    }

    DebugPrintf(T_NAME"::PitchToBlockOffset - BlockLinear Translation - Pitch Offset: 0x%08x Block offset: 0x%08x\n", offset, blockOffset);

    return blockOffset;
}

//! Verifies that a write which splits the pitch/boundary for a remapper worked.
//! This ilwolves reading back the pitch data and additional data after the pitch surface to make sure
//! the split worked properly.
//! Reading back the block data, along with data just before the start of the block surface to make sure nothing is corrupted.
bool BlockLinearRemapperTest::VerifyBoundarySplitWrite(int remapper)
{
    // Read back the pitch data
    memset(tmpBuffer, 0, pcieBurstSize);

    UINT32 offset = imgSize[remapper] - pcieBurstSize/2;
    uintptr_t address = reinterpret_cast<uintptr_t>(pitchBuffer[remapper].GetAddress()) + offset;
    Platform::VirtualRd(reinterpret_cast<void*>(address), tmpBuffer, pcieBurstSize);

    bool mismatch = false;
    // Check the data pitch read back matches our buffer
    for(UINT32 i = 0 ; i < pcieBurstSize/2 ; ++i)
    {
        if(tmpBuffer[i] != imageBuffer[remapper][i])
        {
            mismatch = true;
            ErrPrintf(T_NAME"::VerifyBoundarySplitWrite - Pitch data mismatch on remapper %d, byte 0x%x, expected=0x%x, read=0x%x\n",
                      remapper, i, tmpBuffer[i], imageBuffer[remapper][i]);
        }
    }
    for(UINT32 i = pcieBurstSize/2 ; i < pcieBurstSize ; ++i)
    {
        if(tmpBuffer[i] != 0)
        {
            mismatch = true;
            ErrPrintf(T_NAME"::VerifyBoundarySplitWrite - Write corrupted remaped range in pitch surface, remapper %d, byte 0x%x, value=0x%x.\n",
                      remapper, i, tmpBuffer[i]);
        }
    }

    return !mismatch;
}
