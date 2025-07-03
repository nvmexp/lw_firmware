/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2013,2016-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "sysmembar_l2_l2b.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/lwgpu_classes.h"

#include "kepler/gk104/dev_flush.h"

#include "sim/IChip.h"

#include "class/cla06fsubch.h"
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "gpu/include/gpumgr.h"

/*
This test writes to generate a race condition between (loopback) peer-mem write and local mem read to the same vidmem location
*/

extern const ParamDecl sysmembar_l2_l2b_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-width", "Width in bytes of the surface to read/write"),
    UNSIGNED_PARAM("-height", "Height of the surface to read/write"),
    SIMPLE_PARAM("-noflush", "Use no flush"),
    LAST_PARAM
};

sysmembar_l2_l2bTest::sysmembar_l2_l2bTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader)
{
    params = reader;

    pitch = params->ParamUnsigned("-width", DEFAULT_WIDTH);
    height = params->ParamUnsigned("-height", DEFAULT_HEIGHT);
    noflush = params->ParamPresent("-noflush") != 0;

    surfaceSize = pitch * height;
    surfaceBuffer = new unsigned char[surfaceSize];

    mem2memHandle = 0;
    m_DmaCopyAlloc.SetOldestSupportedClass(GF100_DMA_COPY);
}

sysmembar_l2_l2bTest::~sysmembar_l2_l2bTest(void)
{
    delete surfaceBuffer;
}

STD_TEST_FACTORY(sysmembar_l2_l2b, sysmembar_l2_l2bTest)

// a little extra setup to be done
int sysmembar_l2_l2bTest::Setup(void)
{
    //_DMA_TARGET target;
    local_status = 1;

    DebugPrintf("sysmembar_l2_l2bTest::Setup() starts\n");

    getStateReport()->enable();
    subChannel = SUB_CHANNEL;

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
    DebugPrintf("sysmembar_l2_l2bTest::Setup() creating class objects\n");

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
    DebugPrintf("sysmembar_l2_l2bTest::Setup() will create a destination DMA buffer with size of %08x bytes\n",surfaceSize);
    dstBuffer.SetArrayPitch(surfaceSize);
    dstBuffer.SetColorFormat(ColorUtils::Y8);
    dstBuffer.SetForceSizeAlloc(true);
    dstBuffer.SetLocation(Memory::Fb);      // Destination Non-Coherent Sysmem
    dstBuffer.SetGpuCacheMode(Surface::GpuCacheOn);         // L2 Caching

    if (OK != dstBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(dstBuffer);

    if (OK != dstBuffer.MapLoopback()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    if (OK != dstBuffer.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    InfoPrintf("Created and mapped the destination DMA buffer at 0x%x\n",dstBuffer.GetCtxDmaOffsetGpu());

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

    DebugPrintf("sysmembar_l2_l2bTest::Setup() class object created\n");

    // Need to allocate source surface. To keep things simpler - we'll allocate the same buffer as destination,
    // but will really use only the necessary part

    //Now we need source surface in AGP or PCI (or LWM again) space
    DebugPrintf("sysmembar_l2_l2bTest::Init() will create a source DMA buffer with size of %08x bytes\n",surfaceSize);

    srcBuffer.SetArrayPitch(surfaceSize);
    srcBuffer.SetColorFormat(ColorUtils::Y8);
    srcBuffer.SetForceSizeAlloc(true);
    srcBuffer.SetLocation(Memory::Fb);              // Src VIDMEM
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

    dstBuffer2.SetArrayPitch(surfaceSize);          // however we will use just 4B
    dstBuffer2.SetColorFormat(ColorUtils::Y8);
    dstBuffer2.SetForceSizeAlloc(true);
    dstBuffer2.SetLocation(Memory::Fb);             // Src VIDMEM
    dstBuffer2.SetGpuCacheMode(Surface::GpuCacheOn);

    if (OK != dstBuffer2.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(dstBuffer2);

    if (OK != dstBuffer2.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    DebugPrintf("sysmembar_l2_l2bTest::Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void sysmembar_l2_l2bTest::CleanUp(void)
{
    DebugPrintf("sysmembar_l2_l2bTest::CleanUp() starts\n");

    // free the surface and objects we used for drawing
    if (srcBuffer.GetSize()) {
        srcBuffer.Unmap();
        srcBuffer.Free();
    }
    if (dstBuffer.GetSize()) {
        dstBuffer.Unmap();
        dstBuffer.Free();
    }

    if (dstBuffer2.GetSize()) {
        dstBuffer2.Unmap();
        dstBuffer2.Free();
    }

    if ( mem2memHandle ) {
        m_DmaCopyAlloc.Free();
    }

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("sysmembar_l2_l2bTest::CleanUp() done\n");
}

// the real test
void sysmembar_l2_l2bTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("sysmembar_l2_l2bTest::Run() starts\n");

    //UINT64 offset;
    int local_status = 1;

    InfoPrintf("Copying rectangle of size %d:%d\n",pitch,height);

    void* va1;
    UINT32 va2, va3;
    va1 = srcBuffer.GetAddress();
    va2 = (UINT32) srcBuffer.GetVidMemOffset();
    va3 = (UINT32) srcBuffer.GetCtxDmaOffsetGpu();
    InfoPrintf("SRC : GetAddress() = %p, GetVidMemOffset = %X, GetCtxDmaOffset = %X\n", va1, va2,va3);

    va1 = dstBuffer.GetAddress();
    va2 = (UINT32) dstBuffer.GetVidMemOffset();
    va3 = (UINT32) dstBuffer.GetCtxDmaOffsetGpu();
    InfoPrintf("DST : GetAddress() = %p, GetVidMemOffset = %X, GetCtxDmaOffset = %X\n", va1, va2,va3);

    UINT32 va4, va5;
    va4 = (UINT32) dstBuffer.GetGpuVirtAddrPeer(0, lwgpu->GetGpuDevice(), 0);
    va5 = (UINT32) dstBuffer.GetCtxDmaOffsetGpuPeer(0, lwgpu->GetGpuDevice(), 0);
    InfoPrintf("DST : VAPeer= %X, CtxDmaPeer = %X\n", va4, va5);

    // Loop the designated number of times
    if (true) {

    //-------------------------------------------------------------
    // Initialize Memories
    //-------------------------------------------------------------
        // Initialize our surface data.
        for (int size = 0; size < (int)surfaceSize; size+=4) {
        unsigned val = MAGIC_VALUE;
            surfaceBuffer[size+0] = (char) (val & 0xFF);
            surfaceBuffer[size+1] = (char) (val>>8 & 0xFF);
            surfaceBuffer[size+2] = (char) (val>>16 & 0xFF);
            surfaceBuffer[size+3] = (char) (val>>24 & 0xFF);
        }

        // Write the src surface using BAR1 accesses
        WriteSrc();

        // Send down an FB_FLUSH, MODS will use the correct register based on
        // PCIe GEN of HW.
        lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

        ch->WaitForIdle();

    //----------------------------------------------------
    // Setup 1st copy
    //----------------------------------------------------
        local_status = local_status && setup_copy1();

    //----------------------------------------------------
    // Setup 2nd copy
    //----------------------------------------------------
        local_status = local_status && setup_copy2();

        ch->MethodFlush();
        ch->WaitForIdle();

        // read in the destination data and check it;
    DisableBackdoor();

        local_status = local_status && ReadDst();

    EnableBackdoor();
    }

    ch->WaitForIdle();
    InfoPrintf("Done with sysmembar_l2_l2b test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED);
    }

    return;
}

void sysmembar_l2_l2bTest::WriteSrc(void)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;

    InfoPrintf("sysmembar_l2_l2bTest::WriteSrc() - writing out src surface\n");
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        MEM_WR32( reinterpret_cast<uintptr_t>(srcBuffer.GetAddress())+i, data[i / 4] );
        MEM_WR32( reinterpret_cast<uintptr_t>(dstBuffer.GetAddress())+i, 0 );
    }

    MEM_WR32( reinterpret_cast<uintptr_t>(dstBuffer2.GetAddress()), 0xdeadbeef);

}

int sysmembar_l2_l2bTest::ReadDst(void)
{
    UINT32 value;
    int rtnStatus = 1;

    // read back the dst surface and compare it with the original
    InfoPrintf("sysmembar_l2_l2bTest::ReadDst() - reading back dst surface\n");
    value = MEM_RD32( reinterpret_cast<uintptr_t>(srcBuffer.GetAddress()));
    InfoPrintf("src[0].0 value = %x\n",value);
    value = MEM_RD32( reinterpret_cast<uintptr_t>(dstBuffer.GetAddress()));
    InfoPrintf("dst[0].0 value = %x\n",value);

    value = MEM_RD32( reinterpret_cast<uintptr_t>(srcBuffer.GetAddress()) + pitch*height -4);
    InfoPrintf("src[0].last value = %x\n",value);
    value = MEM_RD32( reinterpret_cast<uintptr_t>(dstBuffer.GetAddress()) + pitch*height -4);
    InfoPrintf("dst[0].last value = %x\n",value);
    value = MEM_RD32( reinterpret_cast<uintptr_t>(dstBuffer2.GetAddress()));
    InfoPrintf("dst[1].0 value = %x\n",value);

    value = MEM_RD32( reinterpret_cast<uintptr_t>(dstBuffer2.GetAddress()));
    if (value != MAGIC_VALUE)
    {
        if (noflush)  {
        InfoPrintf("Mismatch with -noflush option. -> This test is aggressive, and should success without noflush option\n.");
        rtnStatus = 1;
    }
    else {

        ErrPrintf("Magic Value = %x, ReadValue = %x \n.", MAGIC_VALUE, value);
        ErrPrintf("Test fail with flush! (sysmembar not worked in peer writing) \n.");
        rtnStatus = 0;
    }
    }
    else {
    rtnStatus = 1;
        if (noflush)  {
        InfoPrintf("Success with -noflush option. -> This test is not aggressive enough.\n");
    } else {
        InfoPrintf("Test success with flush.\n");
    }
    }

    return rtnStatus;
}

//! \brief Enables the backdoor access
void sysmembar_l2_l2bTest::EnableBackdoor(void)
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
void sysmembar_l2_l2bTest::DisableBackdoor(void)
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

int sysmembar_l2_l2bTest::setup_copy1(void)
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
        offset = (UINT64)dstBuffer.GetCtxDmaOffsetGpuPeer(0, lwgpu->GetGpuDevice(), 0);
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
                        DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_PIPELINED) |
            (noflush ?
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_FALSE)
            :
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE)
            ) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, LW90B5_LAUNCH_DMA_MULTI_LINE_ENABLE_TRUE));

    return local_status;
}

int sysmembar_l2_l2bTest::setup_copy2(void)
{
        // Set up the src offset
    int local_status = 1;
        UINT32 methodData[4];
    UINT64 offset;

        offset = (UINT64)dstBuffer.GetCtxDmaOffsetGpu() + pitch*height - 4; // read last 4B of dst buffer
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_IN_UPPER,2,methodData, INCREMENTING);

        // Set up the dst offset
        offset = (UINT64)dstBuffer2.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, LW90B5_OFFSET_OUT_UPPER,2,methodData, INCREMENTING);

        // Set up other DMA parameters
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_LENGTH_IN, 4);       // last 4B
        //local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_IN, pitch);       // -> single line transfer
        //local_status = local_status && ch->MethodWrite(subChannel, LW90B5_PITCH_OUT, pitch);
        local_status = local_status && ch->MethodWrite(subChannel, LW90B5_LINE_COUNT, 1);

        // start the DMA of src surface to dst surface
        local_status = local_status &&
        ch->MethodWrite(subChannel, LW90B5_LAUNCH_DMA,
                        DRF_NUM(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_SRC_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, LW90B5_LAUNCH_DMA_DST_MEMORY_LAYOUT_PITCH) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NON_PIPELINED) |
                        DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE) |         // flush for 2nd copy
                        DRF_NUM(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, LW90B5_LAUNCH_DMA_MULTI_LINE_ENABLE_FALSE));  // -> Single line transfer

    return local_status;
}

