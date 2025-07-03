/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "frontdoor.h"

#include "core/include/gpu.h"
#include "core/include/memoryif.h"
#include "core/include/pool.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include "ctrl/ctrl003e.h" // Look up physical address
#include "ctrl/ctrl0041.h" // Look up physical address
#include "ctrl/ctrl2080/ctrl2080bus.h" // RM Bar2 test
#include "device/interface/pcie.h"
#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/dev_lw_xve.h"
#include "fermi/gf100/dev_ram.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "sim/IChip.h"

#include <sstream>
#include <memory>

// RC helper macro, I wish some other file had this
#define CHECK_RC_MSG_ASSERT(f, msg)                                       \
do {                                                                      \
    if(OK != (rc = (f))) {                                                       \
        Printf(4/*Tee::PriNormal*/, "%s: %s at %s %d\n",                    \
            msg, rc.Message(), __FILE__, __LINE__);                       \
        MASSERT(!"Generic MODS assert failure<refer to above message>."); \
    }                                                                     \
} while(0)

// Static helper method
static string getBar0TargetString(uint target) {
    string retString;
    switch(target) {
    case LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM:
        retString = "LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM";
        break;
    case LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT:
        retString = "LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT";
        break;
    case LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT:
        retString = "LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT";
        break;
    }
    return retString;
}
/*
This test is dedicated to testing Bar 1 Read/Write bandwidth.
*/

extern const ParamDecl frontdoor_params[] = {
    //PARAM_SUBDECL(lwgpu_single_params),
    PARAM_SUBDECL(lwgpu_basetest_ctx_params),
    UNSIGNED_PARAM("-bar2_buffer_size",  "Size of the Bar2 test buffer in bytes."),
    UNSIGNED_PARAM("-size_copy_buffers", "Size of the buffers for the bar0/bar1 copy test."),
    UNSIGNED_PARAM("-size_copy",         "Sets the size in DWORDS (4bytes) to write to the buffer in a row."),
    UNSIGNED_PARAM("-stride_copy",       "Sets the stride in DWORDS (4bytes) between a copy buffer copy. Must be a multiple of 4 bytes."),
    UNSIGNED_PARAM("-write_num_copy",        "Number of writes to do to the buffer. stride*size*num should be less than the size of the buffer."),
    UNSIGNED_PARAM("-num_map_buffers",   "Sets the number of buffers to use in the bar1 shared mapping test."),
    UNSIGNED_PARAM("-size_map_buffers",     "Sets the size in bytes of the buffers for the bar1 mapping test."),
    UNSIGNED_PARAM("-stride_map_buffer",            "Sets the stride in DWORDS (4bytes) between a map buffer copy. Must be a multiple of 4 bytes."),
    UNSIGNED_PARAM("-write_size_map_buffer",    "Size in DWORDS (4bytes) to write to the buffer in a row."),
    UNSIGNED_PARAM("-write_num_map_buffer",     "Number of writes to do to the buffer. stride*size*num should be less than the size of the buffer."),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    SIMPLE_PARAM("-no_backdoor",          "Disable Backdoor for entire test. Including surface allocation/setup. By default only the actual surface copying is done frontdoor to speed up init time."),
    SIMPLE_PARAM("-no_rand_data",         "Use byte offset to init the surface instead of random data"),
    SIMPLE_PARAM("-skip_bar2_tests",        "Disables testing of BAR2"),
    SIMPLE_PARAM("-disable_bar2_vbios_test",         "Disables write/read test to VBIOS range of BAR2. Needed to run in a 0FB config."),

    // Surface Location Params
    // Set location of the Bar0/Bar1 copy buffers
    //{ "-copy_buffer_loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
    //                            ParamDecl::GROUP_START),  0, 2, "Memory location for the BAR0/BAR1 test copy buffers, 0=fb, 1=sys_coh, 2=sys_ncoh"},
    //{ "-copy_buffer_fb", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR0/BAR1 copy buffers in the framebuffer"},
    //{ "-copy_buffer_sys_coh", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR0/BAR1 copy buffers in system coherent memory"},
    //{ "-copy_buffer_sys_ncoh", "2", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR0/BAR1 copy buffers in system non-coherent memory"},
    // Surface Kind Param
    //UNSIGNED_PARAM("-copy_buffer_kind", "Specifies page kind for the BAR0/BAR1 copy buffers. See dev_mmu.ref for valid integer values."),

    { "-bar2_buffer_loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                                ParamDecl::GROUP_START),  0, 2, "Memory location for the BAR2 test copy buffers, 0=fb, 1=sys_coh, 2=sys_ncoh"},
    { "-bar2_buffer_fb", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR2 copy buffers in the framebuffer"},
    { "-bar2_buffer_sys_coh", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR2 copy buffers in system coherent memory"},
    { "-bar2_buffer_sys_ncoh", "2", ParamDecl::GROUP_MEMBER, 0, 0, "Put BAR2 copy buffers in system non-coherent memory"},
    // Surface Kind Param
    UNSIGNED_PARAM("-bar2_buffer_kind", "Specifies page kind for the BAR2 copy buffers. See dev_mmu.ref for valid integer values."),

    SIMPLE_PARAM("-do_ifb_test",         "Specifies that in addition to the regular frontdoor test we should do a quick IFB test."),
    // IFB Buffer params
    SIMPLE_PARAM("-ifb_read_fault",   "Specifies that during the IFB test we should cause an IFB read fault. Only recognized with the -do_ifb_test parameter."),
    SIMPLE_PARAM("-ifb_write_fault",  "Specifies that during the IFB test we should cause an IFB write fault. Only recognized with the -do_ifb_test parameter."),
    { "-ifb_buffer_loc", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                                ParamDecl::GROUP_START),  0, 2, "Memory location for the IFB test copy buffers, 0=fb, 1=sys_coh, 2=sys_ncoh"},
    { "-ifb_buffer_fb", "0", ParamDecl::GROUP_MEMBER, 0, 0, "Put IFB copy buffers in the framebuffer"},
    { "-ifb_buffer_sys_coh", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Put IFB copy buffers in system coherent memory"},
    { "-ifb_buffer_sys_ncoh", "2", ParamDecl::GROUP_MEMBER, 0, 0, "Put IFB copy buffers in system non-coherent memory"},
    // Surface Kind Param
    UNSIGNED_PARAM("-ifb_buffer_kind", "Specifies page kind for the IFB copy buffers. See dev_mmu.ref for valid integer values."),
    STRING_PARAM("-interrupt_file", "Specify the interrupt file name used by the trace."),
    STRING_PARAM("-interrupt_dir", "Specify the directory path to the interrupt file name used by the trace"),

    LAST_PARAM
};

//! \brief Initializes a MdiagSurf object.
//!
//! Sets all the necessary paramters to allocate a MdiagSurf object
//! of the given size in the FrameBuffer.
//!
//! \param buffer Reference to the surface we need to allocate.
//! \param size Size in bytes to make the surface.
//! \param memLoc Location to allocate this memory, choices are Memory::Fb, Memory::Coherent, Memory::NonCoherent, Memory::Optimal.
//! \param pageKind Sets the Page Table Entry Kind for this surface. See dev_mmu.ref for valid values.
//!
//! \return Returns 0 on success
int frontdoorTest::SetupSurface( MdiagSurf * buffer, UINT32 size,
                                 Memory::Location memLoc, INT32 pageKind )
{

    MASSERT(buffer != NULL);

    if(buffer == NULL)
    {
        ErrPrintf("frontdoor::SetupSurface - Buffer passed is null.\n");
        return 1;
    }

    buffer->SetWidth(size);
    buffer->SetHeight(1);
    buffer->SetLocation(memLoc);
    buffer->SetPteKind(pageKind);
    buffer->SetColorFormat(ColorUtils::Y8);
    buffer->SetProtect(Memory::ReadWrite);
    buffer->SetAlignment(65536);
    buffer->SetLayout(Surface::Pitch);
    buffer->SetPhysContig(true); // Want all memory physically contiguous if we're using it through BAR0 Window
    // If we're sysmem then we want reflection to test the GPU, if it is in gpu mem then this shouldn't matter
    buffer->SetMemoryMappingMode(Surface::MapReflected);

    buffer->SetPageSize(4);

    if(OK != buffer->Alloc(lwgpu->GetGpuDevice()))
    {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("frontdoor: Cannot create dst buffer.\n");
        return 1;
    }

    if(OK != buffer->Map())
    {
        ErrPrintf("Can't map the source buffer.\n");
        SetStatus(Test::TEST_NO_RESOURCES);
        return 1;
    }

    DebugPrintf( "Surf2D allocated a surface:\n");
    DebugPrintf( "  Size = 0x%08x\n", buffer->GetSize());
    DebugPrintf( "  hMem = 0x%08x\n", buffer->GetMemHandle());
    DebugPrintf( "  hVirtMem = 0x%08x\n", buffer->GetVirtMemHandle());
    DebugPrintf( "  hCtxDmaGpu = 0x%08x\n", buffer->GetCtxDmaHandle());
    DebugPrintf( "  OffsetGpu = 0x%llx\n", buffer->GetCtxDmaOffsetGpu());
    DebugPrintf( "  hCtxDmaIso = 0x%08x\n", buffer->GetCtxDmaHandleIso());
    DebugPrintf( "  OffsetIso = 0x%llx\n", buffer->GetCtxDmaOffsetIso());
    // print this only when we did a VidHeapAlloc
    Printf(Tee::PriDebug, "  VidMemOffset = 0x%08llx\n",
           buffer->GetVidMemOffset());

    // Add surface info to buffInfo
    m_buffInfo->SetRenderSurface(*buffer);

    return 0;
}

frontdoorTest::frontdoorTest(ArgReader *reader) :
    //LWGpuSingleChannelTest(reader),
    LWGpuBaseTest(reader),
    lwgpu(NULL),
    m_buffInfo(NULL),
    noRandomData(false),
    numMapBuffers(0),
    mapWriteStride(0), copyWriteStride(0),
    mapWriteSize(0), copyWriteSize(0),
    mapWriteNum(0), copyWriteNum(0),
    bar2BufferKind(LW_MMU_PTE_KIND_PITCH),
    bar2BufferLocation(Memory::Fb),
    copyBufferSize(0),
    mapBufferSize(0), bar2BufferSize(0),
    mapBufferRef(NULL),
    pGpuSub(NULL),
    seed0(0), seed1(0), seed2(0),
    RndStream(NULL),
    params(reader),
    doBar2Testing(true),
    mapBuffers(NULL),
    ifbBuffer(),
    skipBar2VbiosRangeTest(false),
    doIfbTest(false),
    doIfbReadFault(false),
    doIfbWriteFault(false),
    ifbBufferKind(LW_MMU_PTE_KIND_PITCH),
    ifbBufferLocation(Memory::Fb),
    hIfb(0),
    pIfb(NULL),
    ifbOffset(0)
{

    noRandomData = params->ParamPresent("-no_rand_data") ? true : false;

    m_buffInfo = new BuffInfo();

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);

    bar2BufferSize = params->ParamUnsigned("-bar2_buffer_size", 8192);
    copyBufferSize = params->ParamUnsigned("-size_copy_buffers", 8192);
    copyWriteStride = params->ParamUnsigned("-stride_copy", 0);
    copyWriteSize = params->ParamUnsigned("-size_copy", 1);
    MASSERT((copyWriteStride == 0) || (copyWriteStride >= copyWriteSize));
    if((copyWriteStride != 0) && (copyWriteStride < copyWriteSize)) {
        WarnPrintf("frontdoor: -stride_copy is defined < -size_copy, setting stride size to size_copy. \n");
    }
    UINT32 copySpacing = (copyWriteStride > copyWriteSize) ? copyWriteStride : copyWriteSize;
    copyWriteNum = params->ParamUnsigned("-write_num_copy", copyBufferSize/(copySpacing*4));

    // Setup the map test buffer parameters
    numMapBuffers = params->ParamUnsigned("-num_map_buffers", 4);
    mapBufferSize = params->ParamUnsigned("-size_map_buffers", 1024);
    mapWriteStride = params->ParamUnsigned("-stride_map_buffer", 0);
    mapWriteSize = params->ParamUnsigned("-write_size_map_buffer", 1);
    copySpacing = (mapWriteStride > mapWriteSize) ? mapWriteStride : mapWriteSize;
    MASSERT((mapWriteStride == 0) || (mapWriteStride >= mapWriteSize));
    if((mapWriteStride != 0) && (mapWriteStride < mapWriteSize)) {
        WarnPrintf("frontdoor: -stride_map_buffer is defined < -size_map_buffer, setting stride size to size_map_buffer. \n");
    }
    mapWriteNum = params->ParamUnsigned("-write_num_map_buffer", mapBufferSize/((mapWriteSize+mapWriteStride)*4));

    // Set the bar2 buffer memory location and type
    bar2BufferKind = params->ParamUnsigned("-bar2_buffer_kind", LW_MMU_PTE_KIND_PITCH);

    int tempParam = params->ParamUnsigned("-bar2_buffer_loc", 0);

    switch(tempParam) {
        case 0:
            bar2BufferLocation = Memory::Fb;
            break;
        case 1:
            bar2BufferLocation = Memory::Coherent;
            break;
        case 2:
            bar2BufferLocation = Memory::NonCoherent;
            break;
        default:
            MASSERT(!"Invalid Memory Location Specified");
            break;
    }

    // Set the ifb buffer memory location and type
    ifbBufferKind = params->ParamUnsigned("-ifb_buffer_kind", LW_MMU_PTE_KIND_PITCH);

    switch(tempParam) {
        case 0:
            ifbBufferLocation = Memory::Fb;
            break;
        case 1:
            ifbBufferLocation = Memory::Coherent;
            break;
        case 2:
            ifbBufferLocation = Memory::NonCoherent;
            break;
        default:
            MASSERT(!"Invalid Memory Location Specified");
            break;
    }

    if(params->ParamPresent("-do_ifb_test")) {
        doIfbTest = true;

        if(params->ParamPresent("-ifb_read_fault")) {
            doIfbReadFault = true;
        }
        if(params->ParamPresent("-ifb_write_fault")) {
            doIfbWriteFault = true;
        }

        if (doIfbWriteFault || doIfbReadFault) {
            if ( params->ParamPresent( "-interrupt_file" ) > 0 )
                ifbIntFileName = m_pParams->ParamStr( "-interrupt_file" );
            if ( params->ParamPresent( "-interrupt_dir" ) > 0 )
                ifbIntDir = m_pParams->ParamStr( "-interrupt_dir" );
            else
                ifbIntDir = "./";
            if (ifbIntDir[ifbIntDir.size()-1] != '/')
                ifbIntDir += '/';
            if (ifbIntFileName.size() > 0 && ifbIntFileName[0] != '/')
                ifbIntFileName = ifbIntDir + ifbIntFileName;
        }
    }

    if(params->ParamPresent("-disable_bar2_vbios_test")) {
        skipBar2VbiosRangeTest = true;
    }
    if (params->ParamPresent("-skip_bar2_tests")) {
        doBar2Testing = false;
    }
    memset(testBuffer, 0, sizeof(testBuffer));
    memset(refBuffer, 0, sizeof(refBuffer));

}

frontdoorTest::~frontdoorTest(void)
{

    DestroyMembers();

    pGpuSub = NULL;
}

STD_TEST_FACTORY(frontdoor, frontdoorTest)

static const UINT32 classes_needed[] = {
    GF100_INDIRECT_FRAMEBUFFER
};

const int classes_count = sizeof(classes_needed) / sizeof(classes_needed[0]);

// a little extra setup to be done
int frontdoorTest::Setup(void)
{

    DebugPrintf("frontdoor: Setup() starts\n");

    // Validate test params
    if(!ValidateParams())
    {
        ErrPrintf("frontdoor: Invalid command line parameters.\n");
        return 0;
    }

    if (doIfbTest)
        lwgpu = FindGpuForClass(classes_count, classes_needed);
    else
        lwgpu = FindGpuForClass(0, NULL);
    MASSERT(lwgpu != NULL);

    pGpuSub = lwgpu->GetGpuSubdevice();
    if(params->ParamPresent("-no_backdoor"))
    {
        DisableBackdoor();
    }

    // Setup the RandomStream
    RndStream = new RandomStream(seed0, seed1, seed2);
    if(!RndStream)
    {
       ErrPrintf("frontdoor: Unable to allocate RandomStream, line: %d\n", __LINE__);
       return 0;
    }

    if (!Platform::HasWideMemCopy())
    {
       ErrPrintf("frontdoor: Platform::HasWideMemCopy is false!  Try -force_sse_memcpy\n");
       return 0;
    }

    // Setup a state report, so we can pass gold
    getStateReport()->enable();

    // allocate our src
    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        testBuffer[i] = new UINT08[copyBufferSize];
        refBuffer[i] = new UINT08[copyBufferSize];
        if(!refBuffer[i] || !testBuffer[i])
        {
            ErrPrintf("frontdoor: Unable to allocate buffers, line: %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 0;
        }
    }

    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        InitBuffer(refBuffer[i], copyBufferSize);
    }

    // Allocate the TLB buffers
    InfoPrintf("frontdoor: Allocating buffers to stress TLB.\n");

    mapBufferRef = new UINT08*[numMapBuffers];
    mapBuffers = new MdiagSurf[numMapBuffers];

    memset(mapBufferRef, 0, sizeof(UINT08)*numMapBuffers);

    for(int i = 0 ;  i < numMapBuffers ; ++i)
    {
        DebugPrintf("frontdoor: Allocating mapBuffer surface %d, of %d.\n", i+1, numMapBuffers);
        mapBufferRef[i] = new UINT08[mapBufferSize];

        std::stringstream buffName;
        buffName << "mapBuffers[" << i << "]";
        mapBuffers[i].SetName(buffName.str());

        if(SetupSurface(&mapBuffers[i], mapBufferSize, Memory::Fb, LW_MMU_PTE_KIND_PITCH))
        {
            SetStatus(Test::TEST_NO_RESOURCES);
            ErrPrintf("frontdoor: Unable to allocate surface.\n");
            CleanUp();
            return 0;
        }

        mapBuffers[i].Unmap();  // Mapping/unmapping is part of this test.

        if(!mapBufferRef[i])
        {
            ErrPrintf("frontdoor: Unable to allocate reference buffers, line: %d\n", __LINE__);
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 0;
        }

        InitBuffer(mapBufferRef[i], mapBufferSize );
    }

    InfoPrintf("Setting up Semaphore Buffer.\n");
    semaphoreBuffer.SetName("Semaphore Buffer");
    if(SetupSurface(&semaphoreBuffer, 16, Memory::Optimal, LW_MMU_PTE_KIND_PITCH))
    {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("frontdoor: Unable to allocate semaphore.\n");
        CleanUp();
        return 0;
    }

    InfoPrintf("Creating destination surface of size %d KB.\n", copyBufferSize/1024);

    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        std::stringstream buffName;
        buffName << "barBuffer[" << i << "]";
        barBuffer[i].SetName(buffName.str());

        if(SetupSurface(&barBuffer[i], copyBufferSize, Memory::Fb, LW_MMU_PTE_KIND_PITCH))
        {
            SetStatus(Test::TEST_NO_RESOURCES);
            ErrPrintf("frontdoor: Unable to setup surface %d of %d.\n", i, NUM_BAR_BUFFERS);
            CleanUp();
            return 0;
        }
        InfoPrintf("Created Surface at VidMemOffset 0x%llx"
                   " Mapped at 0x%p \n",
                   barBuffer[i].GetVidMemOffset(), barBuffer[i].GetAddress());
    }

    if (doBar2Testing)
    {
        InfoPrintf("Creating Bar2 Copy Buffers.\n");
        bar2Buffer.SetName("Bar2Buffer");
        if(SetupSurface(&bar2Buffer, bar2BufferSize, bar2BufferLocation, bar2BufferKind))
        {
            SetStatus(Test::TEST_NO_RESOURCES);
            ErrPrintf("frontdoor: Unable to setup bar2 test surface.\n");
            CleanUp();
            return 0;
        }
    }

    if(doIfbTest) {
        if(!SetupIFBTest()) {
            return 0;
            ErrPrintf("frontdoor: Unable to setup IFB test.\n");
        }
    }

    m_buffInfo->Print(nullptr, lwgpu->GetGpuDevice());
    DisableBackdoor();
    DebugPrintf("frontdoor: Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void frontdoorTest::CleanUp(void)
{
    DebugPrintf("frontdoor: CleanUp() starts\n");

    DestroyMembers();

    DebugPrintf("frontdoor: CleanUp() done\n");
}

//! \brief Initializes the buffer with random data.
void frontdoorTest::InitBuffer(UINT08 *buffer, UINT32 size)
{
    for(UINT32 i = 0 ; i < size ; ++i)
    {
        buffer[i] = noRandomData ? i : RndStream->RandomUnsigned(0, 255);
    }
}

//! \brief Compares the two buffers
//! Compares the two buffers, basically strncat with an error message
//!
//! \param buff0 Pointer to first buffer to compare
//! \param buff1 Pointer to 2nd
//! \param stride Stride in DWORDs to check
//! \param writeSize Size in DWORDS of conselwtive comparisons
//! \param count Number of comparisons to do
//! \return true if buffers match, false otherwise
bool frontdoorTest::CompareBuffers(UINT08 *buff0, UINT08 *buff1, UINT32 stride, UINT32 writeSize, UINT32 count)
{

    bool result = true;
    for(UINT32 z = 0 ; z < count ; ++z)
    {
        for(UINT32 offset = z*stride*4; offset < z*stride+writeSize*4 ; offset+=1)
        {
            if(buff0[offset] != buff1[offset])
            {
                ErrPrintf("frontdoor: Error buffer mismatch at byte 0x%08x,"
                          " 0x%02x != 0x%02x.\n", offset, buff0[offset], buff1[offset]);
                result = false;
            }
        }
    }
    return result;
}

//! Run the test
//!
//! This test does a memory write to Bar 0 and Bar1. It then reads the two surfaces back,
//! swapping bars.
//! It also compares Bar2 and Bar0 using an RM API
void frontdoorTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("frontdoor: Run() starts\n");

    bool local_status = true;
    RC rc;

    InfoPrintf("ErrorLogger::StartingTest\n");
    rc = ErrorLogger::StartingTest();
    if (rc != OK)
    {
        ErrPrintf("Error initializing ErrorLogger: %s\n", rc.Message());
        SetStatus(Test::TEST_FAILED);
        return;
    }

    // Do the IFB Test
    if(doIfbTest) {
        InfoPrintf("frontdoor: Running IFB Window test.\n");
        if(!RunIFBTest()) {
            local_status = false;
            ErrPrintf("frontdoor: IFB Window test failed.\n");
        } else {
            InfoPrintf("frontdoor: IFB Window test passed.\n");
        }
    }

    // Setting choose the correct aperture for the BAR0 window
    uint bar0WindowTarget[2] = { GetBar0WindowTarget(barBuffer[0]), GetBar0WindowTarget(barBuffer[1]) };
    DebugPrintf("frontdoor: Buffer barBuffer[0] BAR0 Window Target %s\n", getBar0TargetString(bar0WindowTarget[0]).c_str());
    DebugPrintf("frontdoor: Buffer barBuffer[1] BAR0 Window Target %s\n", getBar0TargetString(bar0WindowTarget[1]).c_str());
    LwU64 bar0offset[2] = {0x0 , 0x0};

    CHECK_RC_MSG_ASSERT(GetBar0WindowOffset(barBuffer[0], bar0offset[0]), "Error getting physical address of surface for bar0 window mapping");
    CHECK_RC_MSG_ASSERT(GetBar0WindowOffset(barBuffer[1], bar0offset[1]), "Error getting physical address of surface for bar0 window mapping");

    // The offset should fit into the expected memory range
    MASSERT(((bar0offset[0] >> 16) & 0xffffffffff000000LL) == 0);
    MASSERT(((bar0offset[1] >> 16) & 0xffffffffff000000LL) == 0);

    RegHal& regHal = pGpuSub->Regs();
    UINT32 baseShift = regHal.LookupAddress(MODS_XAL_EP_BAR0_WINDOW_BASE_SHIFT);
    UINT32 bar0WindowSettings[NUM_BAR_BUFFERS] =
    {
        regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, 
            static_cast<UINT32>(bar0offset[0] >> baseShift)),
        regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, 
            static_cast<UINT32>(bar0offset[1] >> baseShift))
    };

    if (lwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Hopper)
    {
        for (UINT32 i = 0; i < NUM_BAR_BUFFERS; i++)
        {
            bar0WindowSettings[i] |= 
                regHal.SetField(
                    MODS_PBUS_BAR0_WINDOW_TARGET, bar0WindowTarget[i]);
        }
    }

    InfoPrintf("frontdoor: BAR0 offset: 0x%08llx.\n", bar0offset[0]);
    InfoPrintf("frontdoor: BAR0 offset 2: 0x%08llx.\n", bar0offset[1]);
    // Setup BAR0 Window
    UINT32 bar0Address = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
    InfoPrintf("frontdoor: Physical Address of FB Start: 0x%08x.\n", pGpuSub->GetPhysFbBase());

    // Turn off the backdoor
    InfoPrintf("frontdoor: Disabling Backdoor.\n");
    DisableBackdoor();

    InfoPrintf("frontdoor: Writing Bar0 and Bar1.\n");
    // Write Data to BAR0 Window and Bar1
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0WindowSettings[0]);
    UINT32 bar0base = static_cast<UINT32>(bar0offset[0] & 0x000000000000ffffLL);
    for(UINT32 z = 0, i = 0 ; z < copyWriteNum ; ++z, i += copyWriteStride*4)
    {
        DebugPrintf("frontdoor: Writing burst %d to BAR0 and BAR1.\n", i);
        // Mem write was moved out here as a burst write for HUB testing
        DebugPrintf("Writing BAR1.\n");
        Platform::VirtualWr( (static_cast<UINT08*>(barBuffer[1].GetAddress()) + i),
                             refBuffer[1] + i, copyWriteSize*4);

        for( UINT32 numWrites  = 0 ; (numWrites < copyWriteSize) ; ++numWrites)
        {
            UINT32 offset = i + numWrites*4;
            DebugPrintf("Writing BAR0 at offset: 0x%x\n", offset);
            UINT32 packed =  refBuffer[0][offset+3];
            packed = (packed << 8) | refBuffer[0][offset+2];
            packed = (packed << 8) | refBuffer[0][offset+1];
            packed = (packed << 8) | refBuffer[0][offset];

            pGpuSub->RegWr32(DEVICE_BASE(LW_PRAMIN)+bar0base+offset, packed);

        }
    }

    InfoPrintf("frontdoor: Reading Bar1 and Bar0.\n");

    // Read Data from BAR0 and BAR1
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0WindowSettings[1]);
    bar0base = static_cast<UINT32>(bar0offset[1] & 0x000000000000ffffLL);
    for(UINT32 z = 0, i = 0 ; z < copyWriteNum ; ++z, i += copyWriteStride*4)
    {
        // Mem read was moved out here as a burst read for HUB testing
        Platform::VirtualRd( (static_cast<UINT08*>(barBuffer[0].GetAddress()) + i),
                             testBuffer[0] + i, copyWriteSize*4);
        for( UINT32 numWrites  = 0 ; (numWrites < copyWriteSize) ; ++numWrites)
        {
            UINT32 offset = i + numWrites*4;
            DebugPrintf("Reading BAR0 at offset: 0x%x\n", offset);

            UINT32 input = pGpuSub->RegRd32(DEVICE_BASE(LW_PRAMIN) + bar0base + offset);
            testBuffer[1][offset] = input & 0xff;
            testBuffer[1][offset+1] = (input >> 8) & 0xff;
            testBuffer[1][offset+2] = (input >> 16) & 0xff;
            testBuffer[1][offset+3] = (input >> 24) & 0xff;
        }
    }

    InfoPrintf("frontdoor: Comparing Buffers\n");
    // Check BAR1 Data
    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        InfoPrintf("frontdoor: Comparing buffer %d, of %d.\n", i+1, NUM_BAR_BUFFERS);

        if(!CompareBuffers(refBuffer[i], testBuffer[i], copyWriteStride, copyWriteSize, copyWriteNum))
        {
            ErrPrintf("frontdoor: Error comparing buffers, on buffer %d of %d.\n", i+1, NUM_BAR_BUFFERS);
            SetStatus(Test::TEST_FAILED);
            local_status = false;
        }
    }

    if(local_status)
    {
        InfoPrintf("frontdoor: Comparison Passed for Bar0 and Bar1.\n");
    }

    InfoPrintf("Restoring Bar0 Window.\n");
    // Restore BAR0 Window
    regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0Address);

    InfoPrintf("frontdoor: Running TLB test.\n");
    // Run the TLB stress test
    local_status = local_status && RunBar1ToBar0TlbTest();

    InfoPrintf("frontdoor: TLB test complete.\n");

    if (doBar2Testing)
    {
        InfoPrintf("frontdoor: Running Bar2 test.\n");
        if((rc = RunBar2Test()) != OK) {
            rc.Clear();
            local_status = false;
            ErrPrintf("frontdoor: BAR2 test failed.\n");
        } else {
            InfoPrintf("frontdoor: BAR2 test passed.\n");
        }
    }

    // Backdoor can be on for shutdown
    EnableBackdoor();

    if(local_status)
    {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else
    {
        SetStatus(Test::TEST_FAILED);
    }

    if (doIfbTest && (doIfbReadFault || doIfbWriteFault)) {
        if (CheckIntrPassFail() != TestEnums::TEST_SUCCEEDED)
        {
            InfoPrintf("frontdoor: Failed interrupt check\n");
            SetStatus(Test::TEST_FAILED);
        }
    }

    rc = ErrorLogger::TestCompleted();
    if (rc != OK)
    {
        InfoPrintf("Unhandled error(s) detected in ErrorLogger: %s\n", rc.Message());
        SetStatus(Test::TEST_FAILED);
    }
    InfoPrintf("ErrorLogger::TestCompleted\n");

    return;
}

//! \brief Enables the backdoor access
void frontdoorTest::EnableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void frontdoorTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}

//! \brief Runs the Bar1 to Bar0 TLB stress test
bool frontdoorTest::RunBar1ToBar0TlbTest(void)
{

    bool local_status = true;

    // Test Bar0 to Bar1 with overlapped remapping
    // Order is map buffer
    // Write to bar1
    // Unmap buffer
    // Check Bar1 Window
    for(int i = 0 ; i < numMapBuffers ; ++i)
    {
        InfoPrintf("frontdoor: Mapping buffer %d of %d.\n", i, numMapBuffers);
        // Map the buffer
        if(mapBuffers[i].Map() != OK)
        {
            ErrPrintf("frontdoor: Could not map pageBuffer[%d].\n", i);
            local_status = false;
        }

        InfoPrintf("frontdoor: Writing buffer %d of %d.\n", i, numMapBuffers);
        UINT32 packed;
        for(UINT32 k = 0, j = 0 ; k < mapWriteNum ; ++k, j += mapWriteStride*4)
        {
            for(UINT32 z = 0 ; z < mapWriteSize ; ++z, j+=4)
            {
                packed =  mapBufferRef[i][j+3];
                packed = (packed << 8) | mapBufferRef[i][j+2];
                packed = (packed << 8) | mapBufferRef[i][j+1];
                packed = (packed << 8) | mapBufferRef[i][j];

                MEM_WR32(reinterpret_cast<uintptr_t>(mapBuffers[i].GetAddress()) + j, packed);
            }
        }

        InfoPrintf("frontdoor: Unmapping buffer %d of %d.\n", i, numMapBuffers);
        mapBuffers[i].Unmap();

        InfoPrintf("frontdoor: Writing Bar0 Window for buffer %d of %d.\n", i, numMapBuffers);
        LwU64 bar0WindowOffset = 0;
        RC rc;
        CHECK_RC_MSG_ASSERT(GetBar0WindowOffset(mapBuffers[i], bar0WindowOffset), "Unable to get the BAR0 window offset");

        UINT32 bar0base = static_cast<UINT32>(bar0WindowOffset & 0x000000000000ffffLL);

        // Save BAR0 window.
        RegHal& regHal = pGpuSub->Regs();
        UINT32 bar0WindowTemp = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
        UINT32 baseShift = regHal.LookupAddress(MODS_XAL_EP_BAR0_WINDOW_BASE_SHIFT);

        UINT32 newBar0Window = regHal.SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, 
            static_cast<UINT32>(bar0WindowOffset >> baseShift));

        if (lwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Hopper)
        {
            newBar0Window |= 
                regHal.SetField(MODS_PBUS_BAR0_WINDOW_TARGET, 
                    GetBar0WindowTarget(mapBuffers[i]));
        }

        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, newBar0Window);

        InfoPrintf("frontdoor: Reading Values from Bar0 Window for buffer %d of %d.\n", i, numMapBuffers);
        for(UINT32 q = 0, j = 0 ; q < mapWriteNum ; ++q, j += mapWriteStride*4)
        {
            for(UINT32 z = 0 ; z < mapWriteSize ; ++z, j += 4)
            {

                packed = pGpuSub->RegRd32(DEVICE_BASE(LW_PRAMIN) + bar0base + j);
                for(int k = 0 ; k < 4 ; ++k)
                {
                    if(mapBufferRef[i][j+k] != ((packed >> (k*8)) & 0xff))
                    {
                        ErrPrintf("frontdoor: Bar1 to Bar0 mismatch during TLB ilwalidate test.\n"
                                  "frontdoor: Mismatch buffer = %d,  offset = 0x%x, src = %d, dst = %d\n",
                                  i,
                                  j,
                                  mapBufferRef[i][j+k],
                                  ((packed >> (k*8)) & 0xff));
                        local_status = false;
                    }
                }
            }
        }

        // Restore BAR0 window.
        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0WindowTemp);
    }

    InfoPrintf("frontdoor: TLB test passed.\n");

    return local_status;
}

//!
//! Verifies that all the parameters are in the proper range.
//!
//! \return bool true if paramters are valid, false if invalid
//!
bool frontdoorTest::ValidateParams(void)
{
    bool valid = true;

    // Validate our buffers are a DWORD multiple
    if( (copyBufferSize % 4) || (bar2BufferSize % 4) || (mapBufferSize % 4) )
    {
        valid = false;
        ErrPrintf("frontdoorTest: Buffer is not a DWORD multiple.\n");
    }

    // Make sure stride+size * count is less than our buffer size
    UINT32 maxSize = copyWriteStride > copyWriteSize ? copyWriteStride : copyWriteSize;
    if( maxSize*copyWriteNum*4 > copyBufferSize )
    {
        valid = false;
        ErrPrintf("frontdoorTest: Cannot safely perform copy on copy buffer.\n");
    }

    maxSize = mapWriteStride > mapWriteSize ? mapWriteStride : mapWriteSize;
    if( maxSize*mapWriteNum*4 > mapBufferSize )
    {
        valid = false;
        ErrPrintf("frontdoorTest: Cannot safely perform copy on map buffer.\n");
    }

    return valid;
}

//! Destroys all class members
void frontdoorTest::DestroyMembers(void)
{

    if(m_buffInfo) {
        delete m_buffInfo;
        m_buffInfo = NULL;
    }

    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        delete[] testBuffer[i];
        delete[] refBuffer[i];
        testBuffer[i]=NULL;
        refBuffer[i]=NULL;
    }

    if(mapBufferRef)
    {
        for(int i = 0 ; i < numMapBuffers ; ++i)
        {
            if(mapBufferRef[i])
            {
                delete [] mapBufferRef[i];
                mapBufferRef[i] = NULL;
            }
        }

        delete [] mapBufferRef;
        mapBufferRef = NULL;
    }

    for(int i = 0 ; i < NUM_BAR_BUFFERS ; ++i)
    {
        if(barBuffer[i].GetSize())
        {
            barBuffer[i].Unmap();
            barBuffer[i].Free();
        }
    }

    if(semaphoreBuffer.GetSize())
    {
        semaphoreBuffer.Unmap();
        semaphoreBuffer.Free();
    }

    if(doBar2Testing && bar2Buffer.GetSize())
    {
        bar2Buffer.Unmap();
        bar2Buffer.Free();
    }

    if(mapBuffers) {
        for(int i = 0 ; i < numMapBuffers ; ++i)
        {
            if(mapBuffers[i].GetSize())
            {
                //mapBuffers[i].Unmap();
                mapBuffers[i].Free();
            }
        }

        delete [] mapBuffers;
        mapBuffers = NULL;
    }

    if(doIfbTest) {
        CleanupIFBTest();
    }

    if(ifbBuffer.GetSize())
    {
        ifbBuffer.Unmap();
        ifbBuffer.Free();
    }

}

//!
//! Performs a simple test of the IFB by allocating a surface and writing it.
//!
//! All the IFB logic is contained in this method.
//!
bool frontdoorTest::SetupIFBTest()
{
    // Pointer to RM
    RC           rc;
    LwRmPtr      pLwRm;

    // Setup the IFB surface
    ifbBuffer.SetName("ifbBuffer");
    SetupSurface(&ifbBuffer, ifbBufferSize, Memory::Fb, LW_MMU_PTE_KIND_PITCH);

    // Init the surface with all 0's
    Memory::Fill08(ifbBuffer.GetAddress(), 0, ifbBufferSize);

    // Check we have a full set of 0's before mapping IFB
    vector<UINT08> tempBuffer(ifbBufferSize);
    Platform::VirtualRd(ifbBuffer.GetAddress(), &tempBuffer[0], ifbBufferSize);

    bool buffGood = true;
    for(uint i = 0 ; i < ifbBufferSize ; ++i) {
        if(tempBuffer[i] != 0) {
            ErrPrintf( "Error bad IFB Buffer at offset 0x%x, should be 0 got 0x%02x\n",
                    i, tempBuffer[i]);
            buffGood = false;
        }
    }
    if(!buffGood) {
        ErrPrintf("Error initializing IFB Buffer.");
        return false;
    }

    // Allocate RM IFB Object and map it to the ifbBuffer allocated earlier
    rc = pLwRm->Alloc( pLwRm->GetDeviceHandle(lwgpu->GetGpuDevice()),
                       &hIfb,
                       GF100_INDIRECT_FRAMEBUFFER,
                       NULL
                     );

    if(rc != OK) {
        return false;
    }

    rc = pLwRm->MapMemory( hIfb, 0,
                           sizeof(GF100IndirectFramebuffer),
                           (void **) &pIfb,
                           0, lwgpu->GetGpuSubdevice());

    if(rc != OK) {
        return false;
    }

    if (!doIfbWriteFault && !doIfbReadFault) {
        rc = pLwRm->MapMemoryDma( hIfb, ifbBuffer.GetMemHandle(),
                                0, ifbBufferSize, 0, &ifbOffset, lwgpu->GetGpuDevice() );

        DebugPrintf("frontdoor: SetupIFBTest() - ifbOffset: 0x%08x.\n", ifbOffset);
        if( rc != OK ) {
            ErrPrintf("frontdoor: SetupIFBTest() - Failed call to MapMemoryDma for IFB buffer.\n");
            return false;
        }
    }

    return true;
}

//!
//! Performs a simple test of the IFB by allocating a surface and writing it.
//!
//! We first write a set of tests patterns through IFB and then verify
//! the values by reading them back through BAR1.
//!
//!
bool frontdoorTest::RunIFBTest()
{
    InfoPrintf("Starting IFB Test\n");
    DisableBackdoor();

    // Check we have a full set of 0's
    vector<UINT08> tempBuffer(ifbBufferSize);
    Platform::VirtualRd(ifbBuffer.GetAddress(), &tempBuffer[0], ifbBufferSize);

    bool buffGood = true;
    for(uint i = 0 ; i < ifbBufferSize ; ++i) {
        if(tempBuffer[i] != 0) {
            ErrPrintf("Error bad IFB Buffer at offset 0x%x, should be 0 got 0x%02x\n",
                      i, tempBuffer[i]);
            buffGood = false;
        }
    }
    if(!buffGood) {
        ErrPrintf("Error initializing IFB Buffer.");
        return false;
    }

    // Write the simple test pattern
    // Write pattern is 1 byte addresses 0x0-0x3
    // 2 bytes to address 0x5
    // Write 4 bytes to address 0x8
    UINT08 bytePattern[4] = { 0x10, 0x22, 0x33, 0x44 };
    UINT16 shortPattern = 0x5566;
    UINT32 dwordPattern = 0x789abcde;

    InfoPrintf("frontdoor: RunIFBTest() - Writing to IFB\n");
    bool pass = true;

    Platform::VirtualWr32(&(pIfb->RdWrAddrHi), LwU64_HI32(ifbOffset));
    Platform::VirtualWr32(&(pIfb->RdWrAddr),   LwU64_LO32(ifbOffset));

    if (doIfbReadFault || doIfbWriteFault) {
        // Set a known pattern on the ifb surface
        Platform::VirtualWr32(ifbBuffer.GetAddress(), dwordPattern);

        // Here we will cause an IFB read or write fault
        // The fault that comes in as a result of the test will be recorded.
        // At the end of the test we check to make sure the fault actually oclwrred.
        if (doIfbReadFault) {
            uintptr_t readAddr = reinterpret_cast<uintptr_t>(& pIfb->RdWrData);
            Platform::VirtualRd32(reinterpret_cast<void*>(readAddr));
        }
        if (doIfbWriteFault) {
            uintptr_t writeAddr = reinterpret_cast<uintptr_t>(& pIfb->RdWrData);
            Platform::VirtualWr32(reinterpret_cast<void*>(writeAddr), dwordPattern);
        }

        // Now we map in the IFB memory.  After this we will run the normal IFB test.
        // The test should pass with no additional faults.
        LwRmPtr      pLwRm;
        RC rc = pLwRm->MapMemoryDma( hIfb, ifbBuffer.GetMemHandle(),
                                     0, ifbBufferSize, 0, &ifbOffset, lwgpu->GetGpuDevice() );

        if( rc != 0 ) {
            ErrPrintf("frontdoor: RunIFBTest() - Failed call to MapMemoryDma for IFB buffer after an IFB fault.\n");
            return false;
        }

        // Wait for RM to re-bind the block.  We should get back the correct value when it's done
        do {
            Tasker::Yield();
            Platform::VirtualWr32(&(pIfb->RdWrAddr),   LwU64_LO32(ifbOffset));
        } while(Platform::VirtualRd32(&(pIfb->RdWrData)) != dwordPattern);
    }

    // Write through the IFB
    uintptr_t writeAddr = 0;
    for(int i = 0 ; i < 4 ; ++i) {
        Platform::VirtualWr32(&pIfb->RdWrAddr,   LwU64_LO32(ifbOffset));

        writeAddr = reinterpret_cast<uintptr_t>(& pIfb->RdWrData) + i;
        Platform::VirtualWr08(reinterpret_cast<void*>(writeAddr), bytePattern[i]);
        DebugPrintf("frontdoor: RunIFBTest() - Writing 0x%02x through IFB.\n", bytePattern[i]);
    }

    // 16 bit writes are split into two 8 bit writes by chiplib_f
    // Do a 32 bit write and skip testing word writes on fmodel
    //writeAddr = reinterpret_cast<uintptr_t>(& pIfb->RdWrData) + 1;
    //Platform::VirtualWr16(reinterpret_cast<void*>(writeAddr), shortPattern);
    writeAddr = reinterpret_cast<uintptr_t>(& pIfb->RdWrData);
    Platform::VirtualWr32(reinterpret_cast<void*>(writeAddr), (shortPattern << 8));

    Platform::VirtualWr32(reinterpret_cast<void*>(writeAddr), dwordPattern);

    InfoPrintf("frontdoor: RunIFBTest() - Reading IFB\n");
    // Read back and check our short IFB pattern.
    UINT32 readData = Platform::VirtualRd32(ifbBuffer.GetAddress());

    // Figure out if we're on a big endian or little endian system.
    bool bigEndian = (*reinterpret_cast<UINT08*>(&dwordPattern) == 0xde) ? false : true ;

    for(int i = 0 ; i < 4 ; ++i) {
        UINT08 unpacked;
        if(bigEndian) {
            unpacked = (readData >> (24 - i*8)) & 0xff;
        } else {
            unpacked = (readData >> i*8) & 0xff;
        }

        if(unpacked != bytePattern[i]) {
            ErrPrintf("frontdoorTest - IFB Test mismatch on byte pattern. Expected: 0x%02x, Read: 0x%02x.\n",
                      bytePattern[i], unpacked);
            pass = false;
        }
    } // end for( int i = 0 ; i < 4 ; ++i)

    writeAddr = reinterpret_cast<uintptr_t>(ifbBuffer.GetAddress()) + 4;
    readData = Platform::VirtualRd32(reinterpret_cast<void*>(writeAddr));
    DebugPrintf("frontdoorTest - IFB reading short, read 0x%08x.\n", readData);

    UINT16 unpackedShort = (readData & 0xffff00) >> 8;
    if(unpackedShort != shortPattern) {
        ErrPrintf("frontdoorTest - IFB Test mismatch on short pattern. Expected: 0x%04x, Read: 0x%04x.\n",
                  shortPattern, unpackedShort);
        pass = false;
    }

    writeAddr =  reinterpret_cast<uintptr_t>(ifbBuffer.GetAddress()) + 8;
    readData = Platform::VirtualRd32(reinterpret_cast<void*>(writeAddr));
    DebugPrintf("frontdoorTest - IFB reading dword, read 0x%08x.\n", readData);
    if(readData != dwordPattern) {
        ErrPrintf("frontdoorTest - IFB Test mismatch on dword pattern. Expected: 0x%08x, Read: 0x%08x.\n",
                  dwordPattern, readData);
        pass = false;
    }
    EnableBackdoor();

    return pass;
} // end RunIFBTest

//!
//! Cleans up the IFB surfaces and objects
//!
void frontdoorTest::CleanupIFBTest() {

    LwRmPtr pLwRm;

    // Unmap the IFB registers and video memory
    if(ifbOffset != 0) {
        pLwRm->UnmapMemoryDma(hIfb, ifbBuffer.GetMemHandle(), 0, ifbOffset, lwgpu->GetGpuDevice());
        ifbOffset = 0;
    }

    if(pIfb != NULL) {
        pLwRm->UnmapMemory(hIfb,pIfb, 0, lwgpu->GetGpuSubdevice());
        pIfb = NULL;
    }

    // Free IFB Object handle
    if(hIfb != 0) {
        pLwRm->Free(hIfb);
        hIfb = 0;
    }
}

bool IntrErrorFilter(UINT64 intr, const char *lwrErr, const char *expectedErr);
TestEnums::TEST_STATUS frontdoorTest::CheckIntrPassFail()
{
    TestEnums::TEST_STATUS result = TestEnums::TEST_SUCCEEDED;

    if(lwgpu->SkipIntrCheck())
    {
        InfoPrintf("frontdoorTest::CheckIntrPassFail: Skipping intr check per commandline override!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
        return result;
    }

    const char *intr_file = ifbIntFileName.c_str();
    FILE *wasFilePresent = NULL;

    if(ErrorLogger::HasErrors())
    {
        ErrorLogger::InstallErrorFilter(IntrErrorFilter);
        RC rc = ErrorLogger::CompareErrorsWithFile(intr_file, ErrorLogger::Exact);

        if(rc != OK)
        {
            if(Utility::OpenFile(intr_file, &wasFilePresent, "r") != OK)
            {
                ErrPrintf("frontdoorTest::checkIntrPassFail: interrupts detected, but %s file not found.\n", intr_file);
                ErrorLogger::PrintErrors(Tee::PriNormal);
                ErrorLogger::TestUnableToHandleErrors();
            }
            result = TestEnums::TEST_FAILED_CRC;
        }
    }
    else
    {
        // Don't use Utility::OpenFile since it prints an error if it can't
        // find the file...confuses people
        wasFilePresent = fopen(intr_file, "r");
        if(wasFilePresent)
        {
            ErrPrintf("frontdoorTest::checkIntrPassFail: NO error interrupts detected, but %s file exists!\n", intr_file);
            result = TestEnums::TEST_FAILED_CRC;
        }
    }

    if(wasFilePresent) fclose(wasFilePresent);

    return result;
}

uint frontdoorTest::GetBar0WindowTarget(const MdiagSurf& surface) {

    uint target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
    switch(surface.GetLocation()) {
    case Memory::Fb:
        target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
        break;
    case Memory::Coherent:
        target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT;
        break;
    case Memory::NonCoherent:
        target = LW_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT;
        break;
    case Memory::Optimal:
        // Assuming this will be FB
        target = LW_PBUS_BAR0_WINDOW_TARGET_VID_MEM;
        break;
    default:
        MASSERT(0 && "Unknown memory location.");
        break;
    }

    return target;
}

RC frontdoorTest::GetBar0WindowOffset(const MdiagSurf& surface, LwU64& offset) {
    LwRmPtr pLwRm;
    RC rc;
    if(surface.GetLocation() == Memory::Fb) {
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset = offset;
        rc = pLwRm->Control(surface.GetMemHandle(),
                LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                &params,
                sizeof(params));
        offset = params.memOffset;
    } else {
        LW003E_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset = offset;
        rc = pLwRm->Control(surface.GetMemHandle(),
                LW003E_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                &params,
                sizeof(params));
        offset = params.memOffset;
    }

    return OK;
}

// Note this method will clobber the upper 16MB of BAR2 which is hardwired
// into the first 16MB of FB.
RC frontdoorTest::RunBar2Test(void) {
    // Test BAR2
    StickyRC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_BUS_MAP_BAR2_PARAMS bar2Params = {0};
    bar2Params.hMemory = bar2Buffer.GetMemHandle();

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                        LW2080_CTRL_CMD_BUS_MAP_BAR2,
                        &bar2Params,
                        sizeof(bar2Params));
    if(rc != OK) {
        rc.Clear();
        ErrPrintf("frontdoor: Error setting up RM Bar2 test.\n");
        SetStatus(Test::TEST_FAILED);
    }

    LW2080_CTRL_BUS_VERIFY_BAR2_PARAMS bar2VerifyParams = {0};
    bar2VerifyParams.hMemory = bar2Buffer.GetMemHandle();
    bar2VerifyParams.offset  = 0;
    bar2VerifyParams.size    = bar2BufferSize;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                        LW2080_CTRL_CMD_BUS_VERIFY_BAR2,
                        &bar2VerifyParams,
                        sizeof(bar2VerifyParams));

    if(rc != OK) {
        rc.Clear();
        ErrPrintf("frontdoor: Error in RM Bar2 Verify.\n");
        SetStatus(Test::TEST_FAILED);
    } else {
        InfoPrintf("frontdoor: RM Bar2 test passes.\n");
    }

    LW2080_CTRL_BUS_UNMAP_BAR2_PARAMS bar2UnmapParams = {0};
    bar2UnmapParams.hMemory = bar2Buffer.GetMemHandle();
    // Leave backdoor always off to avoid l2 cache issues
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                        LW2080_CTRL_CMD_BUS_UNMAP_BAR2,
                        &bar2UnmapParams,
                        sizeof(bar2UnmapParams));

    if(rc != OK) {
        rc.Clear();
        ErrPrintf("frontdoor: Error in Bar2 Unmap.\n");
        SetStatus(Test::TEST_FAILED);
    }
    // Upper 16MB of BAR2 is not hardwired to top of FB in GEN5
    if (!skipBar2VbiosRangeTest && 
        lwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Hopper)
    {
        rc = RulwbiosRangeBar2Test();
    }

    InfoPrintf("frontdoor: Bar2 test complete.\n");

    return rc;
}

bool frontdoorTest::SetupBar2Address(void** bar2Base) {
    StickyRC rc;

    PHYSADDR bar2PhysBase = 0;
    UINT32 addrLo = 0;
    UINT32 addrHi = 0;
    auto pGpuPcie = pGpuSub->GetInterface<Pcie>();

    // Get the physical address from PCI
    rc = Platform::PciRead32( pGpuPcie->DomainNumber(),
                              pGpuPcie->BusNumber(),
                              pGpuPcie->DeviceNumber(),
                              pGpuPcie->FunctionNumber(),
                              LW_XVE_BAR2_LO,
                              &addrLo);
    CHECK_RC_MSG_ASSERT(rc, "unable to read PCI BAR register.");
    DebugPrintf("frontdoor: LW_XVE_BAR2_LO: 0x%08x.\n", addrLo);
    UINT32 addrMsk = (DRF_MASK(LW_XVE_BAR2_LO_BASE_ADDRESS) << DRF_SHIFT(LW_XVE_BAR2_LO_BASE_ADDRESS));
    addrLo &= addrMsk;
    DebugPrintf("frontdoor: BAR2_LO_BASE_ADDRESS_MASK 0x%08x.\n", addrMsk);

    rc = Platform::PciRead32( pGpuPcie->DomainNumber(),
                              pGpuPcie->BusNumber(),
                              pGpuPcie->DeviceNumber(),
                              pGpuPcie->FunctionNumber(),
                              LW_XVE_BAR2_HI,
                              &addrHi);
    DebugPrintf("frontdoor: LW_XVE_BAR2_HI: 0x%08x.\n", addrHi);
    CHECK_RC_MSG_ASSERT(rc, "unable to read PCI BAR register.");
    bar2PhysBase = addrHi;
    bar2PhysBase = (bar2PhysBase << 32) | addrLo;

    MASSERT(bar2PhysBase != 0);
    if(bar2PhysBase == 0) {
        return false;
    }

    DebugPrintf("frontdoor: BAR2 Physical address at 0x%08x%08x.\n", addrHi, addrLo);

    UINT64 bar2Size = pGpuSub->InstApertureSize();
    // Instance memory should be 32MB or 64MB
    MASSERT(bar2Size == 0x2000000 || bar2Size == 0x4000000);
    if(!((bar2Size == 0x2000000) || (bar2Size == 0x4000000))) {
        return false;
    }

    // Map bar2 into our address space.
    rc = Platform::MapDeviceMemory(bar2Base, bar2PhysBase, bar2Size, Memory::WC, Memory::ReadWrite);
    CHECK_RC_MSG_ASSERT(rc, "Unable to map bar2 into process memory.");
    if(rc != OK) {
        ErrPrintf("Unable to map bar2 into process memory.");
        return false;
    }

    return true;
}

//! Read and fill a buffer from BAR0_WINDOW.
//!
//! \param buffer Pointer to a buffer to fill with read data.
//! \param readSize Number of bytes to read from BAR0 window
void frontdoorTest::Bar0WindowRead(UINT08 * buffer, UINT32 readSize) {
    MASSERT((readSize < 0x100000) && "BAR0_WINDOW is only 1MB");
    if(readSize < 0x100000) {
        UINT32 dwordsToRead = readSize/4 + (readSize % 4 ? 1 : 0);
        for( UINT32 i = 0 ; i < dwordsToRead ; ++i)
        {
            UINT32 offset = i*4;
            DebugPrintf("Reading BAR0 at offset: 0x%x\n", offset);

            UINT32 input = pGpuSub->RegRd32(DEVICE_BASE(LW_PRAMIN) + offset);
            buffer[offset] = input & 0xff;
            if(offset+1 < readSize) {
                buffer[offset+1] = (input >> 8) & 0xff;
            }
            if(offset+2 < readSize) {
                buffer[offset+2] = (input >> 16) & 0xff;
            }
            if(offset+3 < readSize) {
                buffer[offset+3] = (input >> 24) & 0xff;
            }
        }
    }
    return;
}

//! Runs a test on the upper 16MB of BAR2. This test just clobbers the
//! memory at that location which corresponds to the start of the GPU
//! framebuffer.
RC frontdoorTest::RulwbiosRangeBar2Test(void) {

    // Setup physical bar2 test

    // Make sure our test buffers are 16-byte aligned so that Platform::MemCopy
    // will be able to use block SSE load/store operations.
    UINT08* bar2TestBuffer = (UINT08*) Pool::AllocAligned(bar2BufferSize, 16);
    UINT08* bar2RefBuffer  = (UINT08*) Pool::AllocAligned(bar2BufferSize, 16);

    InitBuffer(bar2RefBuffer, bar2BufferSize);

    UINT08 *bar2Base = NULL;
    StickyRC rc = OK;

    if(!SetupBar2Address(reinterpret_cast<void**>(&bar2Base))) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("frontdoor: Unable to setup bar2 base address.\n");
        rc = RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    else
    {
        InfoPrintf("Running upper 16MB of BAR2 test.\n");
        // Test the upper 16MB of BAR2. This space isn't managed by BAR2 so we'll
        // just write to it
        Platform::VirtualWr(bar2Base + UPPER_BAR2_OFFSET, bar2RefBuffer, bar2BufferSize);
        Platform::VirtualRd(bar2Base + UPPER_BAR2_OFFSET, bar2TestBuffer, bar2BufferSize);

        InfoPrintf("frontdoor: Checking BAR2 read of upper 16MB of BAR2.\n");
        // Check if the bar2 buffers match
        bool result = true;
        for(UINT32 offset = 0; offset < bar2BufferSize ; ++offset)
        {
            if(bar2RefBuffer[offset] != bar2TestBuffer[offset])
            {
                ErrPrintf("frontdoor: Error buffer mismatch at byte 0x%08x, expected != actual: 0x%02x != 0x%02x.\n",
                          offset, bar2RefBuffer[offset], bar2TestBuffer[offset]);
                result = false;
            }
        }
        if(result) {
            InfoPrintf("frontdoor: BAR2 read of upper 16MB of BAR2 test passed.\n");
        } else  {
            ErrPrintf("frontdoor: Error in BAR2 VBIOS range.");
            rc = RC::GOLDEN_VALUE_MISCOMPARE;
        }

        // Try reading through BAR0
        InfoPrintf("frontdoor: Doing BAR0 read of upper 16MB of BAR2.\n");
        memset(bar2TestBuffer, 0, bar2BufferSize);
        RegHal& regHal = pGpuSub->Regs();
        UINT32 bar0WindowTemp = regHal.Read32(MODS_XAL_EP_BAR0_WINDOW);
        // Top half of BAR2 always points to the start of the framebuffer.
        UINT32 newBar0Window = 0;
        if (lwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Hopper)
        {
            newBar0Window |= regHal.SetField(
                MODS_PBUS_BAR0_WINDOW_TARGET_VID_MEM);
        }
        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, newBar0Window);
        Bar0WindowRead(bar2TestBuffer, bar2BufferSize);
        regHal.Write32(MODS_XAL_EP_BAR0_WINDOW, bar0WindowTemp);
        InfoPrintf("frontdoor: Checking results of BAR0 read of upper 16MB of BAR2.\n");
        // Check the results
        for(UINT32 offset = 0; offset < bar2BufferSize ; ++offset)
        {
            if(bar2RefBuffer[offset] != bar2TestBuffer[offset])
            {
                ErrPrintf("frontdoor: Error buffer mismatch at byte 0x%08x, expected != actual: 0x%02x != 0x%02x.\n",
                          offset, bar2RefBuffer[offset], bar2TestBuffer[offset]);
                result = false;
            }
        }
        if(result) {
            InfoPrintf("frontdoor: BAR0 read of upper 16MB of BAR2 test passed.\n");
        } else  {
            ErrPrintf("frontdoor: Error in BAR2 VBIOS range.\n");
            rc = RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }
    // Free up the BAR2 base
    if(bar2Base) {
        Platform::UnMapDeviceMemory(bar2Base);
        bar2Base = NULL;
    }
    Pool::Free(bar2TestBuffer);
    Pool::Free(bar2RefBuffer);

    return rc;
}
