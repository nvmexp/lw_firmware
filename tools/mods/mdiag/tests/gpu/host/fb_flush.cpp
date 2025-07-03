/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2010-2013,2016-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"
#include <algorithm>
#include "mdiag/utils/lwgpu_classes.h"
#include "fb_flush.h"
#include "host_test_util.h"

#include "class/cl906f.h"

/*
This test writes to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
*/

extern const ParamDecl fb_flush_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-loop", "Number of loops to run (default is 1)"),
    UNSIGNED_PARAM("-surfaceWidth", "Width in bytes of the surface to read/write"),
    UNSIGNED_PARAM("-surfaceHeight", "Height of the surface to read/write"),
    SIMPLE_PARAM("-useSysmem", "use sysmem for copies (default is vidmem)"),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    LAST_PARAM
};

fb_flushTest::fb_flushTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader)
{
    params = reader;

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);
    useSysMem = params->ParamPresent("-useSysmem") != 0;
    loopCount = params->ParamUnsigned("-loop", 1);
    surfaceWidth = params->ParamUnsigned("-surfaceWidth", DEFAULT_SURFACE_WIDTH);
    surfaceHeight = params->ParamUnsigned("-surfaceHeight", DEFAULT_SURFACE_HEIGHT);
    surfaceSize = surfaceWidth * surfaceHeight;
    surfaceBuffer = new unsigned char[surfaceSize];
    RndStream = new RandomStream(seed0, seed1, seed2);

    copyHandle = 0;
}

fb_flushTest::~fb_flushTest(void)
{
    delete RndStream;
    delete surfaceBuffer;
}

STD_TEST_FACTORY(fb_flush, fb_flushTest)

// a little extra setup to be done
int fb_flushTest::Setup(void)
{
    _DMA_TARGET target;
    local_status = 1;
    LwRmPtr pLwRm;

    DebugPrintf("fb_flushTest::Setup() starts\n");

    getStateReport()->enable();
    dstHeight = surfaceHeight;
    dstPitch = (surfaceWidth + (GOB_WIDTH - 1)) & ~(GOB_WIDTH - 1); // Pitch is in multiples of gob's

    if (useSysMem) {
        target = _DMA_TARGET_COHERENT;
    } else {
        target = _DMA_TARGET_VIDEO;
    }

    GpuDevice* pGpuDevice = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);
    RC rc;

    vector<UINT32> ceClasses = EngineClasses::GetClassVec("Ce");
    CHECK_RC(pLwRm->GetFirstSupportedClass(ceClasses.size(), ceClasses.data(), 
        &copyClassToUse, pGpuDevice));

    if (!SetupLWGpuResource(1, &copyClassToUse))
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
        return(0);
    }

    copyHandle = ch->CreateObject(copyClassToUse);
    if (!copyHandle) {
        ErrPrintf("Couldn't CreateObject(copy class)\n");
        return 0;
    }
    DebugPrintf("copy class created \n");

    CHECK_RC(ch->GetSubChannelNumClass(copyHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, copyHandle));
    DebugPrintf("Subchannel %d set\n", subChannel);

    // we need a destination surface in FB, let's allocate it.
    DebugPrintf("fb_flushTest::Setup() will create a destination DMA buffer with size of %08x bytes\n",surfaceSize);
    dstBuffer.SetArrayPitch(surfaceSize);
    dstBuffer.SetColorFormat(ColorUtils::Y8);
    dstBuffer.SetForceSizeAlloc(true);
    dstBuffer.SetLocation(TargetToLocation(target));

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
    DebugPrintf("fb_flushTest::Setup() will create a 16 byte semaphore Surface\n");

    semaphoreBuffer.SetArrayPitch(SEMAPHORE_SIZE);
    semaphoreBuffer.SetColorFormat(ColorUtils::Y8);
    semaphoreBuffer.SetForceSizeAlloc(true);
    semaphoreBuffer.SetLocation(Memory::Fb);
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

    // we need a copy object as well
    DebugPrintf("fb_flushTest::Setup() creating class objects\n");

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

    DebugPrintf("fb_flushTest::Setup() class object created\n");

    // Need to allocate source surface. To keep things simpler - we'll allocate the same buffer as destination,
    // but will really use only the necessary part

    //Now we need source surface in AGP or PCI (or LWM again) space
    DebugPrintf("fb_flushTest::Init() will create a source DMA buffer with size of %08x bytes\n",surfaceSize);

    srcBuffer.SetArrayPitch(surfaceSize);
    srcBuffer.SetColorFormat(ColorUtils::Y8);
    srcBuffer.SetForceSizeAlloc(true);
    srcBuffer.SetLocation(TargetToLocation(target));

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

    DebugPrintf("fb_flushTest::Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void fb_flushTest::CleanUp(void)
{
    DebugPrintf("fb_flushTest::CleanUp() starts\n");

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

    if ( copyHandle ) {
        ch->DestroyObject(copyHandle);
    }

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("fb_flushTest::CleanUp() done\n");
}

// the real test
void fb_flushTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);
    UINT32 methodData[4];

    InfoPrintf("fb_flushTest::Run() starts\n");

    UINT64 offset;
    int local_status = 1;

    unique_ptr<host_test_util> util(new host_test_util(copyClassToUse));

    InfoPrintf("Copying rectangle of size %d:%d\n",surfaceWidth,surfaceHeight);

    // Loop the designated number of times
    for (int i = 0; i < (int)loopCount; ++i) {
        // Initialize our surface data.
        for (int size = 0; size < (int)surfaceSize; ++size) {
            surfaceBuffer[size] = (char)RndStream->RandomUnsigned(0, MAX_RANDOM_NUMBER_VALUE);
        }

        // Initialize the Semaphore
        MEM_WR32(reinterpret_cast<uintptr_t>(semaphoreBuffer.GetAddress()), 0x33);

        // Set up a mem-to-mem DMA

        // Set up the src offset
        offset = (UINT64)srcBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_OFFSET_IN_UPPER), 2, methodData, INCREMENTING);

        // Set up the dst offset
        offset = (UINT64)dstBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_OFFSET_OUT_UPPER), 2, methodData, INCREMENTING);

        // Set up other DMA parameters
        local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_PITCH_IN),dstPitch);
        local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_PITCH_OUT), dstPitch);
        local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LINE_LENGTH_IN), dstPitch);
        local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LINE_COUNT), dstHeight);

        // Set up the semaphore to signal DMA complete
        offset = (UINT64)semaphoreBuffer.GetCtxDmaOffsetGpu();
        methodData[0] = (UINT32)(offset >> 32);
        methodData[1] = (UINT32)(offset);
        methodData[2] = SEMAPHORE_RELEASE_VALUE;
        local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_SET_SEMAPHORE_A), 3, methodData, INCREMENTING);

        // Write the src surface using BAR1 accesses
        WriteSrc();

        // Send down an FB_FLUSH, MODS will use the correct register based on
        // PCIe GEN of HW.
        lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

        // start the DMA of src surface to dst surface
        local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LAUNCH_DMA), util->host_test_dma_method());
        // Send down an in-band FB_FLUSH message
        local_status = local_status && ch->MethodWrite(subChannel, LW906F_FB_FLUSH, 0); // what do I do with this for pascal, c064 says FB_FLUSH is deprecated
        ch->MethodFlush();
        ch->WaitForIdle();

        // wait for completion of the DMA
        int semaphoreValue;
        do {
            // Read the Semaphore
            semaphoreValue = MEM_RD32(reinterpret_cast<uintptr_t>(semaphoreBuffer.GetAddress()));
        } while (semaphoreValue != SEMAPHORE_RELEASE_VALUE);

        // read in the destination data and check it;
        local_status = local_status && ReadDst();

        // Exit if we were not successful
        if (!local_status) {
            break;
        }
    }

    InfoPrintf("Done with fb_flush test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED);
    }

    return;
}

void fb_flushTest::WriteSrc(void)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;

    InfoPrintf("fb_flushTest::WriteSrc() - writing out src surface\n");
    // write the source surface in reverse order
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        MEM_WR32(reinterpret_cast<uintptr_t>(srcBuffer.GetAddress())+i, data[i / 4] );
    }
}

int fb_flushTest::ReadDst(void)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;
    UINT32 value;
    int rtnStatus = 1;

    // read back the dst surface and compare it with the original
    InfoPrintf("fb_flushTest::ReadDst() - reading back dst surface\n");
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        value = MEM_RD32(reinterpret_cast<uintptr_t>(dstBuffer.GetAddress())+i);
        if (value != data[i/4]) {
            ErrPrintf("Dst surface does not equal src surface at offset %d (src = 0x%08x, dst = 0x%08x) at line %d\n", i, data[i/4], value, __LINE__);
            rtnStatus = 0;
            break;
        }
    }

    return rtnStatus;
}

