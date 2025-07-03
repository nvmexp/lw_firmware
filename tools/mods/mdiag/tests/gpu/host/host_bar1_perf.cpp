/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "host_bar1_perf.h"
#include "mdiag/tests/gpu/host/fermi/fermi_host_bar1_perf.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "sim/IChip.h"
#include "class/cl9068.h" // GF100_INDIRECT_FRAMEBUFFER this class isn't needed, but should exist on gf100 or newer chips

#include "mdiag/utils/buffinfo.h"
#include <sstream>

/*
This test is dedicated to testing Bar 1 Read/Write bandwidth.
*/

extern const ParamDecl host_bar1_perf_params[] = {
    PARAM_SUBDECL(lwgpu_basetest_ctx_params),
    UNSIGNED_PARAM("-copy_size",          "Sets the size of the copy in bytes."),
    UNSIGNED_PARAM("-burst_size",         "Sets a burst size for the copy in bytes. The test will do copy_size/burst_size copies. burst_size must evenly divide copy_size. The default burst_sze = copy_size"),
    UNSIGNED_PARAM("-burst_yield_time",   "Minimum time in usec to yield the CPU between bursts. A burst_yield_time of 0 disables this yield. Default: 0."),
    UNSIGNED_PARAM("-sysmem_page_size", "Sysmem page size. Default: 4KB. Valid page sizes: 4KB, 64KB, 2048KB."),
    STRING_PARAM("-pm_file", "Specifies performance monitor trigger file"),
    SIMPLE_PARAM("-read", "Perform a memory read"),
    SIMPLE_PARAM("-write", "Perform a memory write"),
    SIMPLE_PARAM("-bl", "Enables Block Linear Remapper"),
    SIMPLE_PARAM("-no_backdoor", "Disable the backdoor access"),
    SIMPLE_PARAM("-no_l2_flush", "Disable the l2 flush between the end of the test and CRC checking."),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    LAST_PARAM
};

host_bar1_perfTest::host_bar1_perfTest(ArgReader *reader) :
    LWGpuBaseTest(reader), lwgpu(NULL), srcData(NULL),
    m_buffInfo(NULL), burstSize(0), burstYieldTime(0), m_gpuTestObj(nullptr)
{

    params = reader;

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);

    srcSize   = params->ParamUnsigned("-copy_size", 8192);
    burstSize = params->ParamUnsigned("-burst_size", srcSize);
    burstYieldTime = params->ParamUnsigned("-burst_yield_time", 0);
    srcData = NULL;
    refBuffer = NULL;

    if(params->ParamPresent("-read"))
    {
        read = true;
    }
    else
    {
        read = false;
    }

    if(params->ParamPresent("-write"))
    {
        write = true;
    }
    else
    {
        write = false;
    }

    if(params->ParamPresent("-no_backdoor"))
    {
        paramNoBackdoor = true;
    }
    else
    {
        paramNoBackdoor = false;
    }

    if(params->ParamPresent("-bl"))
    {
        useBlRemapper = true;
    }
    else
    {
       useBlRemapper = false;
    }
}

host_bar1_perfTest::~host_bar1_perfTest(void)
{
    // again, nothing special
    if(srcData) delete[] srcData;
}

STD_TEST_FACTORY(host_bar1_perf, host_bar1_perfTest)

//static const UINT32 classes_needed[] = {
//    GF100_INDIRECT_FRAMEBUFFER // This test lwrrently requires no class, so this code is commented out
//};

//const int classes_count = sizeof(classes_needed) / sizeof(classes_needed[0]);

// a little extra setup to be done
int host_bar1_perfTest::Setup(void)
{
    local_status = 1;
    InfoPrintf("host_bar1_perf: Setup() starts\n");

    if( !write && !read)
    {
        ErrPrintf("Must specify -write or -read");
        return 0;
    }

    if(write && read)
    {
        ErrPrintf("Cannot specify both -write and -read");
        return 0;
    }

    //lwgpu = FindGpuForClass(classes_count, classes_needed);
    lwgpu = FindGpuForClass(0, NULL);
    MASSERT(lwgpu != NULL);

    m_buffInfo = new BuffInfo();
    if(!m_buffInfo) {
        return 0;
    }

    // TODO: Get the GPU family and allocate the appropriate test object.
    // For now just instantiate a fermi object.
    m_gpuTestObj.reset(new FermiHostBar1PerfTest(lwgpu));

    if(m_gpuTestObj.get() == 0) {
        return 0;
    }

    // Back these up first so we restore proper values in CleanUp
    if(useBlRemapper)
    {
        m_gpuTestObj->BackupBlRemapperRegisters(0);
    }

    // Setup the RandomStream
    RndStream = new RandomStream(seed0, seed1, seed2);
    if(!RndStream)
    {
       ErrPrintf("host_bar1_perf: Unable to allocate RandomStream, line: %d\n", __LINE__);
       return 0;
    }

    // allocate our src
    srcData = new UINT08[srcSize];
    refBuffer = new UINT08[srcSize];
    if(!refBuffer || !srcData)
    {
        ErrPrintf("host_bar1_perf: Unable to allocate buffers, line: %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }

    InitSrcBuffer(srcSize);

    // Set up the targetBuffer surface
    if(OK != SetupTargetBuffer(targetBuffer)) {
        CleanUp();
        return 0;
    }

    if(useBlRemapper)
    {
        if(!SetupBlSurface(blSurface))
        {
            ErrPrintf("Failed to initialize BL Surface.\n");
            SetStatus(Test::TEST_NO_RESOURCES);
            CleanUp();
            return 0;
        }
        m_gpuTestObj->SetupHostBlRemapper(0, targetBuffer, blSurface, srcSize);
    }

    UINT08 *initVal = new UINT08[srcSize];
    for(UINT32 i = 0 ; i < srcSize ; ++i) {
        initVal[i] = 0x0;
    }

    // Turn off backdoor accesses
    DisableBackdoor();

    UINT32 numCopies = srcSize/burstSize;
    // Clear target buffer
    for(UINT32 i = 0 ; i < numCopies; ++i)
    {
        InfoPrintf("host_bar1_perf: sending %d bytes intialization write 0x%x from addr : 0x%x\n",
                   numCopies, initVal, reinterpret_cast<intptr_t>(targetBuffer.GetAddress()) + i*burstSize);
        Platform::VirtualWr(reinterpret_cast<void*>(reinterpret_cast<intptr_t>(targetBuffer.GetAddress()) + i), &initVal[i*burstSize], burstSize);
    }

    while( 0 != Platform::VirtualRd08(reinterpret_cast<void*>(reinterpret_cast<intptr_t>(targetBuffer.GetAddress()) + srcSize - 1))) {
        InfoPrintf("host_bar1_perf: Initialization to the target buffer is not yet seen.\n");
        Yield();
    };

    //Setup the state report
    getStateReport()->enable();

    if(SetupPerfmon())
    {
        ErrPrintf("Failed to initialize PM\n");
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }

    // Print out the allocated buffers
    m_buffInfo->Print(nullptr, lwgpu->GetGpuDevice());
    InfoPrintf("host_bar1_perf: Setup() done succesfully\n");
    return(1);
}

//! \brief Set's up the TargetBuffer
RC host_bar1_perfTest::SetupTargetBuffer(MdiagSurf &targetBuffer)
{
    RC rc = OK;
    uint const pageSize = params->ParamUnsigned("-sysmem_page_size", 4);

    InfoPrintf("Creating destination surface of size %d KB.\n", srcSize/1024);
    targetBuffer.SetPageSize(pageSize);
    targetBuffer.SetWidth(srcSize);
    targetBuffer.SetHeight(1);
    targetBuffer.SetLocation(Memory::Fb);
    targetBuffer.SetColorFormat(ColorUtils::Y8);
    targetBuffer.SetProtect(Memory::ReadWrite);
    targetBuffer.SetLayout(Surface::Pitch);

    if(OK != (rc = targetBuffer.Alloc(lwgpu->GetGpuDevice())))
    {
        ErrPrintf("host_bar1_perf: Cannot create dst buffer.\n");
        SetStatus(Test::TEST_NO_RESOURCES);
        return rc;
    }

    if(OK != (rc = targetBuffer.Map()))
    {
        ErrPrintf("Can't map the source buffer.\n");
        SetStatus(Test::TEST_NO_RESOURCES);
        return rc;
    }

    // Add surface info to buffInfo
    std::stringstream buffName;
    buffName << "TargetBuffer";
    targetBuffer.SetName(buffName.str());
    m_buffInfo->SetRenderSurface(targetBuffer);

    return OK;
}

//! \brief Set's up a dummy BL Surface where the data will actually be written
UINT32 host_bar1_perfTest::SetupBlSurface(MdiagSurf &surface)
{

    InfoPrintf("Creating destination surface of size %d KB.\n", srcSize/1024);
    surface.SetWidth(m_gpuTestObj->GetPitch());
    surface.SetHeight(m_gpuTestObj->blSurfaceSize(srcSize) / m_gpuTestObj->GetPitch());
    surface.SetLocation(Memory::Fb);
    surface.SetColorFormat(ColorUtils::Y8);
    surface.SetProtect(Memory::ReadWrite);
    surface.SetLayout(Surface::BlockLinear);

    if(OK != surface.Alloc(lwgpu->GetGpuDevice()))
    {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("host_bar1_perf: Cannot create bl buffer.\n");
        CleanUp();
        return 0;
    }

    if(OK != surface.Map())
    {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Can't map the bl buffer.\n");
        CleanUp();
        return 0;
    }

    // Add surface info to buffInfo
    std::stringstream buffName;
    buffName << "Dummy BL Surface";
    surface.SetName(buffName.str());
    m_buffInfo->SetRenderSurface(surface);

    return 1;
}

// Setup the perf monitors
int host_bar1_perfTest::SetupPerfmon(void)
{
    if(params->ParamPresent("-pm_file"))
    {
        perfTest = true;

        const char* trgFilename = params->ParamStr("-pm_file");
        DebugPrintf("host_bar1_perf: Perfmon experiment filename %s\n", trgFilename);

        // Setup the Performance Monitor
        lwgpu->CreatePerformanceMonitor();

        if (!lwgpu->PerfMonInitialize("STANDARD", trgFilename))
        {
            ErrPrintf("Initialize PM failed!\n");

            return 1;
        }
    }
    else
    {
        perfTest = false;
    }

    InfoPrintf("host_bar1_perfTest::SetupPM DONE.\n");

    return 0;
}

// a little extra cleanup to be done
void host_bar1_perfTest::CleanUp(void)
{
    InfoPrintf("host_bar1_perf: CleanUp() starts\n");

    if(srcData) {
        delete[] srcData;
        srcData=NULL;
    }
    delete[] refBuffer;
    refBuffer=NULL;

    // free the surface and objects we used for drawing
    if ( targetBuffer.GetSize())
    {
        targetBuffer.Unmap();
        targetBuffer.Free();
    }

    if( blSurface.GetSize())
    {
        blSurface.Free();
    }

    if( m_buffInfo ) {
        delete m_buffInfo;
        m_buffInfo = NULL;
    }

    if(useBlRemapper)
    {
        m_gpuTestObj->RestoreBlRemapperRegisters(0);
    }

    InfoPrintf("host_bar1_perf: CleanUp() done\n");
}

// Initialize the src sufrace
void host_bar1_perfTest::InitSrcBuffer(UINT32 size)
{
    for(UINT32 i = 0 ; i < size ; ++i)
    {
        srcData[i] = RndStream->RandomUnsigned() % 250;
        refBuffer[i] = srcData[i];
    }
}

UINT32 host_bar1_perfTest::pitch2block_swizzle(UINT32 plOffset) {
    uint blockHeightInGobs = 2;//set as 1, but the number is power of 2.

    uint imgPitch    = m_gpuTestObj->GetPitch();
    uint gobWidth    = m_gpuTestObj->GetGobBytesWidth();
    uint gobHeight   = m_gpuTestObj->GetGobHeight();
    uint blockDepth  = m_gpuTestObj->GetDepth();
    uint blockWidth  = gobWidth;
    uint blockHeight = blockHeightInGobs * gobHeight;

    uint blockSize   = blockWidth * blockHeight * blockDepth;
    uint plPlaneSize = imgPitch   * blockHeight;
    uint blPlaneSize = imgPitch   * blockHeight * blockDepth;

    uint plane          = (plOffset / plPlaneSize);
    uint block          = (plOffset % imgPitch) / gobWidth;
    uint rowWithinBlock = (plOffset / imgPitch) % blockHeight;
    uint gobOffset      = (plOffset % gobWidth);

    //InfoPrintf("host_bar1_perf: pitch2block_swizzle. plOffset:%u plane:%u blPlaneSize:%u block:%u "
    //           "blockSize:%u rowWithinBlock:%u gobWidth:%u gobOffset:%u \n",
    //           plOffset, plane, blPlaneSize, block, blockSize, rowWithinBlock, gobWidth, gobOffset);

    UINT32 blOffset = (plane * blPlaneSize) + (block * blockSize) + (rowWithinBlock * gobWidth) + gobOffset;

    return blOffset;
}

UINT32 host_bar1_perfTest::block2pitch_swizzle(UINT32 blOffset) {
    uint blockHeightInGobs = 2;//set as 1, but the number is power of 2.

    uint imgPitch    = m_gpuTestObj->GetPitch();
    uint gobWidth    = m_gpuTestObj->GetGobBytesWidth();
    uint gobHeight   = m_gpuTestObj->GetGobHeight();
    uint blockDepth  = m_gpuTestObj->GetDepth();
    uint blockWidth  = gobWidth;
    uint blockHeight = blockHeightInGobs *  gobHeight;

    uint blockSize   = blockWidth * blockHeight * blockDepth;
    uint plPlaneSize = imgPitch   * blockHeight;
    uint blPlaneSize = imgPitch   * blockHeight * blockDepth;

    uint plane           = (blOffset / blPlaneSize);
    uint block           = (blOffset % blPlaneSize) / blockSize;
    uint rowWithinBlock  = (blOffset % blockSize) / gobWidth;
    uint gobOffset       = (blOffset % gobWidth);

    UINT32 plOffset = (plane * plPlaneSize) + (block * gobWidth) + (rowWithinBlock * imgPitch) + gobOffset;

    return plOffset;
}

// Checks the destination surface
bool host_bar1_perfTest::CheckDst(void)
{
    InfoPrintf("host_bar1_perf: Flushing surface to FB.\n");
    FlushL2Cache();

    InfoPrintf("host_bar1_perf: Checking Destination Surface. base address:0x%x\n", blSurface.GetAddress());

    bool match = true;

    if(useBlRemapper) { // Swizzle the address as required to match the BL remapper
        bool readBurstIsGob = true; // Either a GOB size burst or a single line in a GOB (GOB width)

        if(readBurstIsGob) {
            UINT08 *readData = new UINT08[512]; //GOB size
            uint imgPitch    = m_gpuTestObj->GetPitch();
            uint gobWidth    = m_gpuTestObj->GetGobBytesWidth();
            uint gobHeight   = m_gpuTestObj->GetGobHeight();
            uint gobSize     = m_gpuTestObj->GetGobBytesWidth() * m_gpuTestObj->GetGobHeight();
            uint imgHeightInGobs = m_gpuTestObj->blImgHeightInGobs(srcSize);

            for(UINT32 y = 0; y < imgHeightInGobs; y++) {
                for(UINT32 x = 0; x < imgPitch; x=x+gobWidth) {
                    UINT32 blOffset = pitch2block_swizzle(y*(imgPitch*gobHeight)+x);
                    void* address = reinterpret_cast<void*>(reinterpret_cast<UINT64>(blSurface.GetAddress()) + blOffset);
                    InfoPrintf("host_bar1_perf: Reading %d bytes from address 0x%x\n", gobSize, address);
                    Platform::VirtualRd(address, readData, gobSize);

                    for (UINT32 j=0; j<gobSize; ++j) {
                        uint plOffset = block2pitch_swizzle(blOffset+j);
                        if(plOffset < srcSize) {
                            if( readData[j] != refBuffer[plOffset]) {
                                InfoPrintf("host_bar1_perf: Destination Surface failed match at "
                                           "offset %d, expected=0x%02x, read=0x%02x.\n", plOffset, refBuffer[plOffset], readData[j] );
                                match = false;
                            }
                            else {
                                refBuffer[plOffset] = 255; //generated random number is always less than 250
                            }
                        }
                    }
                }
            }

            for(uint i=0; i<srcSize; ++i) {
                MASSERT(refBuffer[i] == 255);
            }
        }

        else { // Reading sequentially using the pitch surface offset
            UINT32 readSize = m_gpuTestObj->GetGobBytesWidth();
            UINT32 numReads = srcSize/readSize;
            UINT08 *readData = new UINT08[64]; //64 is GOB width in bytes

            for(UINT32 i = 0; i < numReads ; i++) {
                UINT32 blOffset = pitch2block_swizzle(i*readSize);
                MASSERT(i*readSize == block2pitch_swizzle(blOffset));
                void* address = reinterpret_cast<void*>(reinterpret_cast<UINT64>(blSurface.GetAddress()) + blOffset);
                InfoPrintf("host_bar1_perf: Reading %d bytes from address 0x%x\n", readSize, address);
                Platform::VirtualRd(address, readData, readSize);

                for (UINT32 j=0; j<readSize; ++j) {
                    if( readData[j] != refBuffer[i*readSize+j]) {
                        InfoPrintf("host_bar1_perf: Destination Surface failed match at "
                                   "offset %d, expected=0x%02x, read=0x%02x.\n", i*readSize+j, refBuffer[ i*readSize+j], readData[j] );
                        match = false;
                    }
                }
            }
        }
    }
    else { //No BL remapping
        void* address = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(targetBuffer.GetAddress()));
        UINT08 *readData = new UINT08[srcSize];
        Platform::VirtualRd(address, readData, srcSize);
        for(UINT32 offset = 0; offset < srcSize ; offset++) {
            if( readData[offset] != refBuffer[offset]) {
                InfoPrintf("host_bar1_perf: Destination Surface failed match at offset %d, expected=0x%02x, read=0x%02x.\n",
                           offset, refBuffer[offset], readData[offset] );
                match = false;
            }
        }
    }

    if(match)
    {
        InfoPrintf("host_bar1_perf: Destination Surface passed match.\n");
    }

    return match;
}

// Checks the cpu surface
bool host_bar1_perfTest::CheckCPU(void)
{
    InfoPrintf("host_bar1_perf: Checking Source Surface.\n");
    bool match = true;

    for(UINT32 i = 0 ; i < srcSize ; ++i)
    {
        if(srcData[i] != 0x0)
        {
            InfoPrintf("host_bar1_perf: Source surface failed match at offset %d, expected=0x%02x, read=0x%02x.\n", i, 0x0, srcData[i] );
            match = false;
        }
    }

    if(match)
    {
        InfoPrintf("host_bar1_perf: Source surface passed match.\n");
    }

    return match;
}

// the real test
void host_bar1_perfTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("host_bar1_perf: Run() starts\n");

    int local_status = 0;

    // Turn off backdoor accesses
    DisableBackdoor();

    // Start the PerfMonitor
    if(perfTest)
    {
        // Only used by sillicon
        lwgpu->PerfMonStart(NULL, 0, 0);
    }

    // On RTL MODS isn't ilwolved.
    // Write a register to trigger the PMs
    DebugPrintf("host_bar1_perf: Writing LW_PERF_PMASYS_TRIGGER_GLOBAL to start PM_TRIGGERS.\n");
    m_gpuTestObj->StartPmTriggers();

    if(write)
    {
        InfoPrintf("Writing random data to surface of size %d KB\n", srcSize/1024);

        // Copy to the buffer
        UINT32 numCopies = srcSize/burstSize;
        for(UINT32 i = 0 ; i < numCopies ; ++i)
        {
            uintptr_t address = reinterpret_cast<uintptr_t>(targetBuffer.GetAddress()) + i*burstSize;
            Platform::VirtualWr(reinterpret_cast<void*>(address), &srcData[i*burstSize], burstSize);
            if(burstYieldTime != 0)
            {
                Yield(burstYieldTime);
            }
        }
    }
    else if (read)
    {

        InfoPrintf("Reading from surface of size %d KB\n", srcSize/1024);

        // Copy the buffer
        UINT32 numCopies = srcSize/burstSize;
        for(UINT32 i = 0 ; i < numCopies ; ++i)
        {
            uintptr_t address = reinterpret_cast<uintptr_t>(targetBuffer.GetAddress()) + i*burstSize;
            Platform::VirtualRd(reinterpret_cast<void*>(address), &srcData[i*burstSize], burstSize);
            if(burstYieldTime != 0)
            {
                Yield(burstYieldTime);
            }
        }
    }

    DebugPrintf("host_bar1_perf: Writing LW_PERF_PMASYS_TRIGGER_GLOBAL to stop PM_TRIGGERS.\n");
    // Write a register to trigger the PMs
    m_gpuTestObj->StopPmTriggers();

    if(perfTest)
    {
        lwgpu->PerfMonEnd(NULL, 0, 0);
    }

    InfoPrintf("host_bar1_perf: Done copying data.\n");

    InfoPrintf("host_bar1_perf: Waiting for the channel to go idle.\n");

    // Turn back on the backdoor post read/write
    //EnableBackdoor();

    if(write)
    {
        local_status = CheckDst();
    }
    else if(read)
    {
        local_status = CheckCPU();
    }

    if(local_status)
    {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    }
    else
    {
        SetStatus(Test::TEST_FAILED);
    }

    return;
}

//! \brief Enables the backdoor access
void host_bar1_perfTest::EnableBackdoor() const
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void host_bar1_perfTest::DisableBackdoor() const
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}

void host_bar1_perfTest::FlushL2Cache() const
{
    RC rc;

    LW0080_CTRL_DMA_FLUSH_PARAMS dmaParams;
    memset(&dmaParams, 0, sizeof(dmaParams));
    dmaParams.targetUnit = DRF_DEF(0080_CTRL_DMA_FLUSH, _TARGET_UNIT, _FB, _ENABLE);
    if (!params->ParamPresent("-no_l2_flush"))
    {
        dmaParams.targetUnit |= DRF_DEF(0080_CTRL_DMA_FLUSH, _TARGET_UNIT, _L2, _ENABLE);
        DebugPrintf("host_bar1_perf: Flushing L2...\n");
    }

    LwRmPtr pLwRm;
    DebugPrintf("host_bar1_perf: Sending UFLUSH...\n");
    rc = pLwRm->Control(
        pLwRm->GetDeviceHandle(lwgpu->GetGpuDevice()),
        LW0080_CTRL_CMD_DMA_FLUSH,
        &dmaParams,
        sizeof(dmaParams));

    if (rc != OK)
    {
        ErrPrintf("Error flushing l2 cache, message: %s\n", rc.Message());
        // Nothing to really be done
    }
}

///////////////////////////////////////////////////////////////////////////////
// HostBar1PerfTest methods
///////////////////////////////////////////////////////////////////////////////
//! Constructor
//!
//! \param lwgpu Pointer to an LWGpuResource object to use for memory/register access. This pointer will not be freed by HostBar1PerfTest
HostBar1PerfTest::HostBar1PerfTest(LWGpuResource * lwgpu) : backupBlRegister(),
    lwgpu(lwgpu), GOB_BYTES_WIDTH(0), GOB_HEIGHT(0), GOB_BYTES(0), IMG_PITCH(256), DEPTH(1)
{
}

//! Destructor
HostBar1PerfTest::~HostBar1PerfTest(void)
{
    lwgpu = NULL;
}
