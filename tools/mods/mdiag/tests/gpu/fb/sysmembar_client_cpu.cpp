/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2011,2013,2016-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "sysmembar_client_cpu.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "kepler/gk104/dev_flush.h"
#include "kepler/gk104/dev_pbdma.h"
#include "kepler/gk104/dev_fb.h"       // This is really suspect....
#include "sim/IChip.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "class/cla06fsubch.h"
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "gpu/include/gpumgr.h"

/*
This test writes to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
*/

extern const ParamDecl sysmembar_client_cpu_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-width", "Width in bytes of the surface to read/write"),
    UNSIGNED_PARAM("-height", "Height of the surface to read/write"),
    SIMPLE_PARAM("-noflush", "Use no flush"),
    SIMPLE_PARAM("-force_flush", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-no_force_flush", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-skip_xv0", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-no_skip_xv0", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-skip_xv1", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-no_skip_xv1", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-skip_peer", "override default value for LWP_FB_NISO_CFG0"),
    SIMPLE_PARAM("-no_skip_peer", "override default value for LWP_FB_NISO_CFG0"),
    //SIMPLE_PARAM("-useSysmem", "use sysmem for mem2mem copies (default is vidmem)"),
    LAST_PARAM
};

sysmembar_client_cpuTest::sysmembar_client_cpuTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader),
    override_force_flush(0), force_flush(0),
    override_skip_xv0(0), skip_xv0(0),
    override_skip_xv1(0), skip_xv1(0),
    override_skip_peer(0), skip_peer(0)
{
    params = reader;

    pitch = params->ParamUnsigned("-width", DEFAULT_WIDTH);
    height = params->ParamUnsigned("-height", DEFAULT_HEIGHT);
    noflush = params->ParamPresent("-noflush") != 0;
    if (params->ParamPresent("-no_force_flush")) {
        override_force_flush = 1;
    force_flush = 0;
    }
    if (params->ParamPresent("-force_flush")) {
        override_force_flush = 1;
    force_flush = 1;
    }

    if (params->ParamPresent("-no_skip_xv0")) {
        override_skip_xv0 = 1;
    skip_xv0 = 0;
    }
    if (params->ParamPresent("-skip_xv0")) {
        override_skip_xv0 = 1;
    skip_xv0 = 1;
    }

    if (params->ParamPresent("-no_skip_xv1")) {
        override_skip_xv1 = 1;
    skip_xv1 = 0;
    }
    if (params->ParamPresent("-skip_xv1")) {
        override_skip_xv1 = 1;
    skip_xv1 = 1;
    }

    if (params->ParamPresent("-no_skip_peer")) {
        override_skip_peer = 1;
    skip_peer = 0;
    }
    if (params->ParamPresent("-skip_peer")) {
        override_skip_peer = 1;
    skip_peer = 1;
    }

    surfaceSize = pitch * height;
    surfaceBuffer = new unsigned char[surfaceSize];

    mem2memHandle = 0;

    m_DmaCopyAlloc.SetOldestSupportedClass(GF100_DMA_COPY);
}

sysmembar_client_cpuTest::~sysmembar_client_cpuTest(void)
{
    delete surfaceBuffer;
}

STD_TEST_FACTORY(sysmembar_client_cpu, sysmembar_client_cpuTest)

// a little extra setup to be done
int sysmembar_client_cpuTest::Setup(void)
{
    local_status = 1;

    DebugPrintf("sysmembar_client_cpuTest::Setup() starts\n");

    getStateReport()->enable();

    RC rc;
    GpuDevice* pGpuDevice = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);

    if (!SetupLWGpuResource(EngineClasses::GetClassVec("Ce").size(),
            EngineClasses::GetClassVec("Ce").data()))
    {
        return 0;
    }

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        vector<UINT32> supportedCEs;
        CHECK_RC(lwgpu->GetSupportedCeNum(pGpuSubdevice, &supportedCEs, LwRmPtr().Get()));
        MASSERT(supportedCEs.size() != 0);
        engineId = MDiagUtils::GetCopyEngineId(supportedCEs[0]);
    }

    // call parent's Setup first
    if (!SetupSingleChanTest(engineId))
    {
        return 0;
    }

    // we need a mem2mem object as well
    DebugPrintf("sysmembar_client_cpuTest::Setup() creating class objects\n");

    CHECK_RC(m_DmaCopyAlloc.Alloc(ch->ChannelHandle(), lwgpu->GetGpuDevice()));
    mem2memHandle= m_DmaCopyAlloc.GetHandle();
    MASSERT(mem2memHandle);
    subChannel = LWA06F_SUBCHANNEL_COPY_ENGINE;
    if(OK != ch->SetObjectRC(subChannel, mem2memHandle)) {
        ErrPrintf("Couldn't SetObjectRC(subch, mem2memHandle)\n");
        return 0;
    }
    DebugPrintf("Subchannel %d set\n", subChannel);

    // we need a destination surface in FB, let's allocate it.
    DebugPrintf("sysmembar_client_cpuTest::Setup() will create a destination DMA buffer with size of %08x bytes\n",surfaceSize);
    dstBuffer.SetArrayPitch(surfaceSize);
    dstBuffer.SetColorFormat(ColorUtils::Y8);
    dstBuffer.SetForceSizeAlloc(true);
    dstBuffer.SetLocation(Memory::NonCoherent);     // Destination Non-Coherent Sysmem
    dstBuffer.SetGpuCacheMode(Surface::GpuCacheOn);         // L2 Caching

    if (OK != dstBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(dstBuffer);

    if (OK != dstBuffer.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    InfoPrintf("Created and mapped the destination DMA buffer at 0x%x\n",dstBuffer.GetCtxDmaOffsetGpu());

    //Now we need a semaphore Surface
    DebugPrintf("sysmembar_client_cpuTest::Setup() will create a 16 byte semaphore Surface\n");
    semaphoreBuffer.SetArrayPitch(MY_SEMAPHORE_SIZE);
    semaphoreBuffer.SetColorFormat(ColorUtils::Y8);
    semaphoreBuffer.SetForceSizeAlloc(true);
    semaphoreBuffer.SetLocation(Memory::Fb);
    //semaphoreBuffer.SetGpuCaching(true);          // L2 Caching
    semaphoreBuffer.SetGpuCacheMode(Surface::GpuCacheOn);           // L2 Caching
    if (OK != semaphoreBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(semaphoreBuffer);

    if (OK != semaphoreBuffer.Map()) {
        ErrPrintf("can't map the semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    //Now we need a semaphore Surface_src
    DebugPrintf("sysmembar_client_cpuTest::Setup() will create a 16 byte semaphore Surface\n");
    semaphoreBuffer_src.SetArrayPitch(MY_SEMAPHORE_SIZE);
    semaphoreBuffer_src.SetColorFormat(ColorUtils::Y8);
    semaphoreBuffer_src.SetForceSizeAlloc(true);
    semaphoreBuffer_src.SetLocation(Memory::Fb);
    //semaphoreBuffer_src.SetGpuCaching(true);          // No L2 Caching
    semaphoreBuffer_src.SetGpuCacheMode(Surface::GpuCacheOn);
    if (OK != semaphoreBuffer_src.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create semaphore buffer src, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(semaphoreBuffer_src);

    if (OK != semaphoreBuffer_src.Map()) {
        ErrPrintf("can't map the semaphore buffer src, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    // wait for GR idle
    local_status = local_status && ch->WaitForIdle();
    SetStatus(local_status?(Test::TEST_SUCCEEDED):(Test::TEST_FAILED));

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }

    DebugPrintf("sysmembar_client_cpuTest::Setup() class object created\n");

    // Need to allocate source surface. To keep things simpler - we'll allocate the same buffer as destination,
    // but will really use only the necessary part

    //Now we need source surface in AGP or PCI (or LWM again) space
    DebugPrintf("sysmembar_client_cpuTest::Init() will create a source DMA buffer with size of %08x bytes\n",surfaceSize);

    srcBuffer.SetArrayPitch(surfaceSize);
    srcBuffer.SetColorFormat(ColorUtils::Y8);
    srcBuffer.SetForceSizeAlloc(true);
    //srcBuffer.SetLocation(TargetToLocation(target));
    srcBuffer.SetLocation(Memory::Fb);              // Src VIDMEM
    //srcBuffer.SetGpuCaching(true);            // L2 Caching  (Write through)
    srcBuffer.SetGpuCacheMode(Surface::GpuCacheOn);

    if (OK != srcBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(srcBuffer);

    if (OK != srcBuffer.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    InfoPrintf("Created and mapped the source DMA buffer at 0x%x\n",srcBuffer.GetCtxDmaOffsetGpu());

    unsigned niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
    InfoPrintf("NISO_CFG Value = %x -->", niso_cfg_val);
    if (override_force_flush) {
        niso_cfg_val =
        (force_flush )?
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FORCE_SYSMEM_FLUSH,_ENABLED, niso_cfg_val) :
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FORCE_SYSMEM_FLUSH,_DISABLED, niso_cfg_val) ;
    }
    if (override_skip_xv0) {
        niso_cfg_val =
        (skip_xv0 )?
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_FBXV0,_ENABLED, niso_cfg_val) :
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_FBXV0,_DISABLED, niso_cfg_val) ;
    }
    if (override_skip_xv1) {
        niso_cfg_val =
        (skip_xv1 )?
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_FBXV1,_ENABLED, niso_cfg_val) :
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_FBXV1,_DISABLED, niso_cfg_val) ;
    }
    if (override_skip_peer) {
        niso_cfg_val =
        (skip_peer )?
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_PEERS,_ENABLED, niso_cfg_val) :
        FLD_SET_DRF(_PFB,_NISO_CFG0,_FLUSH_SKIP_PEERS,_DISABLED, niso_cfg_val) ;
    }

   if (override_skip_peer || override_skip_xv1 || override_skip_xv0 || override_force_flush) {
        lwgpu->RegWr32(LW_PFB_NISO_CFG0,niso_cfg_val);
   }

    niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
    InfoPrintf(" %x\n", niso_cfg_val);

    DebugPrintf("sysmembar_client_cpuTest::Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void sysmembar_client_cpuTest::CleanUp(void)
{
    DebugPrintf("sysmembar_client_cpuTest::CleanUp() starts\n");

    // free the surface and objects we used for drawing
    if (srcBuffer.GetSize()) {
        srcBuffer.Unmap();
        srcBuffer.Free();
    }
    if (dstBuffer.GetSize()) {
        dstBuffer.Unmap();
        dstBuffer.Free();
    }
    if (semaphoreBuffer.GetSize()) {
        semaphoreBuffer.Unmap();
        semaphoreBuffer.Free();
    }
    if (semaphoreBuffer_src.GetSize()) {
        semaphoreBuffer_src.Unmap();
        semaphoreBuffer_src.Free();
    }

    if ( mem2memHandle ) {
        m_DmaCopyAlloc.Free();
    }

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("sysmembar_client_cpuTest::CleanUp() done\n");
}

// the real test
void sysmembar_client_cpuTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("sysmembar_client_cpuTest::Run() starts\n");

    //UINT64 offset;
    int local_status = 1;

    InfoPrintf("Copying rectangle of size %d:%d\n",pitch,height);

    // Loop the designated number of times
    //for (int i = 0; i < (int)loopCount; ++i) {
    if (true) {

    //-------------------------------------------------------------
    // Initialize Memories
    //-------------------------------------------------------------
        // Initialize our surface data.
        for (int size = 0; size < (int)surfaceSize; size+=4) {
            surfaceBuffer[size] = (char) (size & 0xFF);
            surfaceBuffer[size+1] = (char) (size>>8 & 0xFF);
            surfaceBuffer[size+2] = (char) (size>>16 & 0xFF);
            surfaceBuffer[size+3] = (char) (size>>24 & 0xFF);
        }

        // Initialize the Semaphore
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer.GetAddress()), 0x33);
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer_src.GetAddress()), SEMAPHORE_RELEASE_VALUE);
        // Write the src surface using BAR1 accesses
        WriteSrc();

        unsigned niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
        InfoPrintf("[NISO_CFG0] %x\n", niso_cfg_val);

        // Send down an FB_FLUSH, MODS will use the correct register based on
        // PCIe GEN of HW.
        lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

        niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
        InfoPrintf("[NISO_CFG0] %x\n", niso_cfg_val);

        ch->WaitForIdle();
    //----------------------------------------------------
    // Setup 1st copy
    //----------------------------------------------------
        local_status = local_status && setup_copy1();

    //----------------------------------------------------
    // Setup 2nd copy
    //----------------------------------------------------
        local_status = local_status && setup_copy2();

        //flushStatus = lwgpu->RegRd32(LW_PPBDMA_GP_BASE(0));
        //unsigned flushStatus2 = lwgpu->RegRd32(LW_PPBDMA_GP_BASE_HI(0));
    //InfoPrintf("%X %X\n", flushStatus2, flushStatus);

        ch->MethodFlush();
        //ch->WaitForIdle();

        niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
        InfoPrintf("[NISO_CFG0] %x\n", niso_cfg_val);

        // wait for completion of the DMA
    DisableBackdoor();
        int semaphoreValue;
    //int count =0;
        do {
            // Read the Semaphore
            semaphoreValue = MEM_RD32(reinterpret_cast<uintptr_t>(semaphoreBuffer.GetAddress()));

        } while (semaphoreValue != SEMAPHORE_RELEASE_VALUE);

    //InfoPrintf("SemaporeValue = 0x%x\n", semaphoreValue);
        //semaphoreValue = lwgpu->MemRd32((UINT32)semaphoreBuffer_src.GetAddress());
    //InfoPrintf("SemaporeValue Should be = 0x%x\n", semaphoreValue);

    //EnableBackdoor();

        niso_cfg_val = lwgpu->RegRd32(LW_PFB_NISO_CFG0);
        InfoPrintf("[NISO_CFG0] %x\n", niso_cfg_val);

        // read in the destination data and check it;
        local_status = local_status && ReadDst();

    EnableBackdoor();
    }

    ch->WaitForIdle();
    InfoPrintf("Done with sysmembar_client_cpu test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED);
    }

    return;
}

void sysmembar_client_cpuTest::WriteSrc(void)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;

    InfoPrintf("sysmembar_client_cpuTest::WriteSrc() - writing out src surface\n");
    // write the source surface in reverse order
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        MEM_WR32( reinterpret_cast<uintptr_t>(srcBuffer.GetAddress())+i, data[i / 4] );
    }
}

int sysmembar_client_cpuTest::ReadDst(void)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;
    UINT32 value;
    int rtnStatus = 1;

    // read back the dst surface and compare it with the original
    InfoPrintf("sysmembar_client_cpuTest::ReadDst() - reading back dst surface\n");
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        value = MEM_RD32( reinterpret_cast<uintptr_t>(dstBuffer.GetAddress())+i );
        if (value != data[i/4]) {
        if (!noflush)
                    ErrPrintf("Dst surface does not equal src surface at offset %d (src = 0x%08x, dst = 0x%08x) at line %d\n", i, data[i/4], value, __LINE__);
                rtnStatus = 0;
                break;
        }

    }

    if (noflush)  {
        if (rtnStatus == 1) {
        InfoPrintf("Success with -noflush option. -> This test is not aggressive enough\n.");
    }
    else {
        InfoPrintf("Mismatch with -noflush option. -> This test is aggressive, and should success without noflush option\n.");
    }
    rtnStatus = 1;
    }

    return rtnStatus;
}

//! \brief Enables the backdoor access
void sysmembar_client_cpuTest::EnableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
       // Turn off backdoor accesses
       // bug 527528 - Comments by Joseph Harwood:
       //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
       //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
       //   for other chips, only for itself.
       //
       // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 1);
    }
}

//! \brief Disable the backdoor access
void sysmembar_client_cpuTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
       // Turn off backdoor accesses
       // bug 527528 - Comments by Joseph Harwood:
       //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
       //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
       //   for other chips, only for itself.
       //
       // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 0);
    }
}

int sysmembar_client_cpuTest::setup_copy1(void)
{
        // Set up the src offset
    int local_status = 1;
        UINT32 methodData[4];
    UINT64 offset;

        offset = (UINT64)srcBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_IN_UPPER,2,methodData, INCREMENTING);

        // Set up the dst offset
        offset = (UINT64)dstBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_OUT_UPPER,2,methodData, INCREMENTING);

        // Set up other DMA parameters
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_IN, pitch);
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_OUT, pitch);
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_IN, pitch);
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_COUNT, height);

        // start the DMA of src surface to dst surface
    InfoPrintf("Noflush = %d\n", noflush);
        local_status = local_status &&
        ch->MethodWrite(subChannel, LW90B5_LAUNCH_DMA,
                        DRF_NUM(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_SRC_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_DST_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NON_PIPELINED) |
            (noflush ?
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_FALSE)
            :
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE)
            ) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, LW90B5_LAUNCH_DMA_MULTI_LINE_ENABLE_TRUE));

    return local_status;
}

int sysmembar_client_cpuTest::setup_copy2(void)
{
        // Set up the src offset
    int local_status = 1;
        UINT32 methodData[4];
    UINT64 offset;

        offset = (UINT64)semaphoreBuffer_src.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_IN_UPPER,2,methodData, INCREMENTING);

        // Set up the dst offset
        offset = (UINT64)semaphoreBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_OUT_UPPER,2,methodData, INCREMENTING);

        // Set up other DMA parameters
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_IN, MY_SEMAPHORE_SIZE);
        //local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_IN, pitch);       // -> single line transfer
        //local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_OUT, pitch);
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_COUNT, 1);

        // start the DMA of src surface to dst surface
        local_status = local_status &&
        ch->MethodWrite(subChannel, LW90B5_LAUNCH_DMA,
                        DRF_NUM(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_SRC_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_DST_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NON_PIPELINED) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_FALSE) |       // No flush for 2nd copy
                        DRF_NUM(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, LW90B5_LAUNCH_DMA_MULTI_LINE_ENABLE_FALSE));  // -> Single line transfer

    return local_status;
}
