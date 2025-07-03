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

#include "semaphore_bashing.h"
#include "host_test_util.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "class/clc56f.h"
#include "sim/IChip.h"

/*
  This test writes to a surface using BAR1 writes followed by an FB_FLUSH and then reads from another engine
*/

extern const ParamDecl semaphore_bashing_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-loop", "Number of loops to run (default is 1)"),
    UNSIGNED_PARAM("-surfaceWidth", "Width in bytes of the surface to read/write"),
    UNSIGNED_PARAM("-surfaceHeight", "Height of the surface to read/write"),
    UNSIGNED_PARAM("-checkDmaSurfaces", "enable the checking of surface data that is transferred on copies."),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    LAST_PARAM
};

semaphore_bashingTest::semaphore_bashingTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader)
{
    params = reader;

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);
    loopCount = params->ParamUnsigned("-loop", 1);
    surfaceWidth = params->ParamUnsigned("-surfaceWidth", SEM_BASH_DEFAULT_SURFACE_WIDTH);
    surfaceHeight = params->ParamUnsigned("-surfaceHeight", SEM_BASH_DEFAULT_SURFACE_HEIGHT);
    surfaceCheck = (params->ParamUnsigned("-checkDmaSurfaces", 0) == 0) ? false: true;
    dstHeight = surfaceHeight;
    dstPitch = (surfaceWidth + (GOB_WIDTH - 1)) & ~(GOB_WIDTH - 1); // Pitch is in multiples of gob's
    surfaceSize = dstPitch * surfaceHeight;
    surfaceBuffer = new unsigned char[surfaceSize];
    RndStream = new RandomStream(seed0, seed1, seed2);

    copyHandle = 0;
}

semaphore_bashingTest::~semaphore_bashingTest(void)
{
    delete RndStream;
    delete surfaceBuffer;
}

STD_TEST_FACTORY(semaphore_bashing, semaphore_bashingTest)

//! \brief Enables the backdoor access
void semaphore_bashingTest::EnableBackdoor(void)
{
    // Turn off backdoor accesses
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void semaphore_bashingTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}


// a little extra setup to be done
int semaphore_bashingTest::Setup(void)
{
    local_status = 1;
    LwRmPtr pLwRm;

    DebugPrintf("semaphore_bashingTest::Setup() starts\n");

    getStateReport()->enable();

    static vector<UINT32> CEClasses = EngineClasses::GetClassVec("Ce");
    GpuDevice* pGpuDevice = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);
    RC rc;

    CHECK_RC(pLwRm->GetFirstSupportedClass(CEClasses.size(), CEClasses.data(),
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
    DebugPrintf("copy classcreated \n");

    CHECK_RC(ch->GetSubChannelNumClass(copyHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, copyHandle));
    DebugPrintf("Subchannel %d set\n", subChannel);

    // we need a destination surface in FB, let's allocate it.
    DebugPrintf("semaphore_bashingTest::Setup() will create a destination DMA buffer with size of %08x bytes\n",surfaceSize);
    dmaBuffer[0].SetArrayPitch(surfaceSize*2);
    dmaBuffer[0].SetColorFormat(ColorUtils::Y8);
    dmaBuffer[0].SetForceSizeAlloc(true);
    dmaBuffer[0].SetLocation(Memory::Fb);

    if (OK != dmaBuffer[0].Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create DMA buffer 0, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(dmaBuffer[0]);

    if (OK != dmaBuffer[0].Map()) {
        ErrPrintf("can't map DMA buffer 0, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    dmaBuffer[1].SetArrayPitch(surfaceSize*2);
    dmaBuffer[1].SetColorFormat(ColorUtils::Y8);
    dmaBuffer[1].SetForceSizeAlloc(true);
    dmaBuffer[1].SetLocation(Memory::Coherent);

    if (OK != dmaBuffer[1].Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create dma buffer 1, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(dmaBuffer[1]);

    if (OK != dmaBuffer[1].Map()) {
        ErrPrintf("can't map dma buffer 1, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    InfoPrintf("Created and mapped DMA buffer 1 at 0x%x\n",dmaBuffer[1].GetCtxDmaOffsetGpu());

    //Now we need a semaphore Surface
    DebugPrintf("semaphore_bashingTest::Setup() will create a 16 byte semaphore Surface\n");

    sysmemSemaphoreBuffer.SetArrayPitch(SEMAPHORE_SIZE*2);
    sysmemSemaphoreBuffer.SetColorFormat(ColorUtils::Y8);
    sysmemSemaphoreBuffer.SetForceSizeAlloc(true);
    sysmemSemaphoreBuffer.SetLocation(Memory::Fb);
    if (OK != sysmemSemaphoreBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create the sysmem semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(sysmemSemaphoreBuffer);

    if (OK != sysmemSemaphoreBuffer.Map()) {
        ErrPrintf("can't map the sysmem semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    vidmemSemaphoreBuffer.SetArrayPitch(SEMAPHORE_SIZE*2);
    vidmemSemaphoreBuffer.SetColorFormat(ColorUtils::Y8);
    vidmemSemaphoreBuffer.SetForceSizeAlloc(true);
    vidmemSemaphoreBuffer.SetLocation(Memory::Fb);
    if (OK != vidmemSemaphoreBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create the vidmem semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(vidmemSemaphoreBuffer);

    if (OK != vidmemSemaphoreBuffer.Map()) {
        ErrPrintf("can't map the vidmem semaphore buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }

    // we need a copy object as well
    DebugPrintf("semaphore_bashingTest::Setup() creating class objects\n");

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

    DebugPrintf("semaphore_bashingTest::Setup() class object created\n");

    // Need to allocate source surface. To keep things simpler - we'll allocate the same buffer as destination,
    // but will really use only the necessary part

    //Now we need source surface in AGP or PCI (or LWM again) space
    DebugPrintf("semaphore_bashingTest::Init() will create a source DMA buffer with size of %08x bytes\n",surfaceSize);

    InfoPrintf("Created and mapped the vidmem DMA buffer at 0x%x\n",dmaBuffer[0].GetCtxDmaOffsetGpu());
    InfoPrintf("Created and mapped the sysmem DMA buffer at 0x%x\n",dmaBuffer[1].GetCtxDmaOffsetGpu());

    DebugPrintf("semaphore_bashingTest::Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void semaphore_bashingTest::CleanUp(void)
{
    DebugPrintf("semaphore_bashingTest::CleanUp() starts\n");

    // free the surface and objects we used for drawing
    if (dmaBuffer[0].GetSize()) {
        dmaBuffer[0].Unmap();
        dmaBuffer[0].Free();
    }
    if (dmaBuffer[1].GetSize()) {
        dmaBuffer[1].Unmap();
        dmaBuffer[1].Free();
    }
    if (sysmemSemaphoreBuffer.GetSize()) {
        sysmemSemaphoreBuffer.Unmap();
        sysmemSemaphoreBuffer.Free();
    }
    if (vidmemSemaphoreBuffer.GetSize()) {
        vidmemSemaphoreBuffer.Unmap();
        vidmemSemaphoreBuffer.Free();
    }

    if ( copyHandle ) {
        ch->DestroyObject(copyHandle);
    }

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("semaphore_bashingTest::CleanUp() done\n");
}

// the real test
void semaphore_bashingTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);
    UINT32 methodData[5];

    InfoPrintf("semaphore_bashingTest::Run() starts\n");
    DisableBackdoor();

    UINT64 timeStamp = 0;
    UINT64 srcOffset;
    uintptr_t srcAddr;
    UINT64 dstOffset;
    uintptr_t dstAddr;
    UINT64 semOffset;
    uintptr_t semAddr;
    UINT32 semExelwteOptions;
    UINT32 semAcqOp;
    UINT32 semValueLower = 0;
    UINT32 semValueUpper = 0;
    UINT32 semAcqValueLower = 0;
    UINT32 semAcqValueUpper = 0;
    int local_status = 1;

    unique_ptr<host_test_util> util(new host_test_util(copyClassToUse));

    InfoPrintf("Copying rectangle of size %d:%d\n",surfaceWidth,surfaceHeight);

    // Loop the designated number of times
    for (int semLoc = 0; semLoc < semNumLocations; ++semLoc) {
        for (int semPayloadSize = 0; semPayloadSize < semNumSizes; ++semPayloadSize) {
            for (int semTimestamp = 0; semTimestamp < semNumTimestampModes; ++semTimestamp) {
                for (int semControl = 0; semControl < semNumAcqRel; ++semControl) {
                    for (int transferSrcDst = 0; transferSrcDst < trNumCombos; ++transferSrcDst) {
                        for (int semAcqType = 0; semAcqType < semNumAcqTypes; ++semAcqType) {
                            if (surfaceCheck) {
                                // Initialize our surface data.
                                for (int i = 0; i < (int)surfaceSize; ++i) {
                                    surfaceBuffer[i] = (char)RndStream->RandomUnsigned(0, MAX_RANDOM_NUMBER_VALUE);
                                }
                            }

                            // Set up semaphore execute flags this loop of the test
                            if (semPayloadSize == sem32bit) {
                                semExelwteOptions = DRF_NUM(C56F, _SEM_EXELWTE, _PAYLOAD_SIZE,
                                                            LWC56F_SEM_EXELWTE_PAYLOAD_SIZE_32BIT);
                            } else {
                                semExelwteOptions = DRF_NUM(C56F, _SEM_EXELWTE, _PAYLOAD_SIZE,
                                                            LWC56F_SEM_EXELWTE_PAYLOAD_SIZE_64BIT);
                            }

                            if (semTimestamp == semTimestampDis) {
                                semExelwteOptions |= DRF_NUM(C56F, _SEM_EXELWTE, _RELEASE_TIMESTAMP,
                                                             LWC56F_SEM_EXELWTE_RELEASE_TIMESTAMP_DIS);
                            } else {
                                semExelwteOptions |= DRF_NUM(C56F, _SEM_EXELWTE, _RELEASE_TIMESTAMP,
                                                             LWC56F_SEM_EXELWTE_RELEASE_TIMESTAMP_EN);
                            }

                            if (semLoc == semVidMem) {
                                semOffset = (UINT64)vidmemSemaphoreBuffer.GetCtxDmaOffsetGpu();
                                semAddr   = reinterpret_cast<uintptr_t>(vidmemSemaphoreBuffer.GetAddress());
                            } else {
                                semOffset = (UINT64)sysmemSemaphoreBuffer.GetCtxDmaOffsetGpu();
                                semAddr   = reinterpret_cast<uintptr_t>(sysmemSemaphoreBuffer.GetAddress());
                            }

                            switch (semAcqType) {
                                case semAcqTypeEq:
                                default:
                                    semAcqOp = LWC56F_SEM_EXELWTE_OPERATION_ACQUIRE;
                                    semValueLower = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                    semValueUpper = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                    semAcqValueLower = semValueLower;
                                    semAcqValueUpper = semValueUpper;
                                    break;

                                // GEQ with unsigned payload and value. Passes when value >= payload
                                case semAcqTypeStrictGeq:
                                    semAcqOp = LWC56F_SEM_EXELWTE_OPERATION_ACQ_STRICT_GEQ;
                                    semValueLower = RndStream->RandomUnsigned(0, 0x7FFFFFFF);
                                    semValueUpper = RndStream->RandomUnsigned(0, 0x7FFFFFFF);
                                    if (semPayloadSize == sem32bit) {
                                        semAcqValueLower = RndStream->RandomUnsigned(0, semValueLower);
                                    } else {
                                        semAcqValueLower = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                    }
                                    semAcqValueUpper = RndStream->RandomUnsigned(0, semValueUpper);
                                    break;

                                // GEQ with signed payload and value. Passes when value >= payload
                                case semAcqTypeCircGeq:
                                    semAcqOp = LWC56F_SEM_EXELWTE_OPERATION_ACQ_CIRC_GEQ;
                                    semAcqValueUpper = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                    // If we are doing 64-bit, force upper 32 bits only to obey inequality.
                                    if (semPayloadSize == sem32bit) {
                                        semAcqValueLower = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                        semValueLower = (semAcqValueLower >> 31)
                                                           ? semAcqValueLower + RndStream->RandomUnsigned(0, 0x7FFFFFFF)
                                                           : RndStream->RandomUnsigned(semAcqValueLower, 0x7FFFFFFF);
                                        semAcqValueUpper = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                    } else {
                                        semValueLower = RndStream->RandomUnsigned(0, 0xFFFFFFFF);
                                        semAcqValueLower = semValueLower;
                                        semValueUpper = (semAcqValueUpper >> 31)
                                                           ? semAcqValueUpper + RndStream->RandomUnsigned(0, 0x7FFFFFFF)
                                                           : RndStream->RandomUnsigned(semAcqValueUpper, 0x7FFFFFFF);
                                    }
                                    break;

                                case semAcqTypeAnd:
                                    semAcqOp = LWC56F_SEM_EXELWTE_OPERATION_ACQ_AND;
                                    if (semPayloadSize == sem32bit) {
                                        semValueLower = RndStream->RandomUnsigned(0x00010000, 0xFFFFFFFF);
                                        semValueUpper = 0;
                                        semAcqValueUpper = 0;
                                    } else {
                                        semValueLower = 0;
                                        semValueUpper = RndStream->RandomUnsigned(0x00010000, 0xFFFFFFFF);
                                        semAcqValueLower = 0;
                                    }
                                    // Choose a bit at random to set and repeat until the AND of payload and value pass
                                    do {
                                        int bit = RndStream->RandomUnsigned(0, 31);
                                        if (semPayloadSize == sem32bit) {
                                            semAcqValueLower = (1 << bit);
                                        } else {
                                            semAcqValueUpper = (1 << bit);
                                        }
                                    } while (!((semAcqValueLower & semValueLower) | (semAcqValueUpper & semValueUpper)));
                                    break;

                                case semAcqTypeNor:
                                    semAcqOp = LWC56F_SEM_EXELWTE_OPERATION_ACQ_NOR;
                                    if (semPayloadSize == sem32bit) {
                                        semValueLower = RndStream->RandomUnsigned(0x00000000, 0x7FFFFFFF);
                                        semValueUpper = 0xFFFFFFFF;
                                        semAcqValueUpper = 0xFFFFFFFF;
                                    } else {
                                        semValueLower = 0xFFFFFFFF;
                                        semValueUpper = RndStream->RandomUnsigned(0x00000000, 0x7FFFFFFF);
                                        semAcqValueLower = 0xFFFFFFFF;
                                    }
                                    // Choose a bit at random to unset and repeat until the NOR of payload and value pass
                                    do {
                                        int bit = RndStream->RandomUnsigned(0, 31);
                                        if (semPayloadSize == sem32bit) {
                                            semAcqValueLower = ~(1 << bit);
                                        } else {
                                            semAcqValueUpper = ~(1 << bit);
                                        }
                                    } while (!(~(semAcqValueLower | semValueLower) | ~(semAcqValueUpper | semValueUpper)));
                                    break;
                            }

                            // Initialize the semaphore memory location to zero
                            MEM_WR64(semAddr, 0);

                            // Do a semaphore release via host
                            methodData[0] = (UINT32)(semOffset);
                            methodData[1] = (UINT32)(semOffset >> 32);
                            methodData[2] = semValueLower;
                            methodData[3] = semValueUpper;
                            methodData[4] = DRF_NUM(C56F, _SEM_EXELWTE, _OPERATION, LWC56F_SEM_EXELWTE_OPERATION_RELEASE)
                                            | semExelwteOptions;
                            local_status = local_status && ch->MethodWriteN(subChannel, LWC56F_SEM_ADDR_LO, 5, methodData, INCREMENTING);

                            // do a semaphore acquire via host
                            methodData[0] = (UINT32)(semOffset);
                            methodData[1] = (UINT32)(semOffset >> 32);
                            methodData[2] = semAcqValueLower;
                            methodData[3] = semAcqValueUpper;
                            methodData[4] = DRF_NUM(C56F, _SEM_EXELWTE, _OPERATION, semAcqOp)
                                            | semExelwteOptions;
                            local_status = local_status && ch->MethodWriteN(subChannel, LWC56F_SEM_ADDR_LO, 5, methodData, INCREMENTING);
                            ch->MethodFlush();
                            ch->WaitForIdle();

                            if (semTimestamp == semTimestampEn) {
                                timeStamp = ((UINT64)MEM_RD32(semAddr + 12) << 32) | (UINT64)MEM_RD32(semAddr + 8);
                            }

                            if (semControl == semAcqCpuRelCpu || semControl == semAcqGpuRelCpu) {
                                MEM_WR32(semAddr, 0);
                            } else {
                                methodData[0] = (UINT32)(semOffset);
                                methodData[1] = (UINT32)(semOffset >> 32);
                                methodData[2] = 0;
                                methodData[3] = 0;
                                methodData[4] = DRF_NUM(C56F, _SEM_EXELWTE, _OPERATION, LWC56F_SEM_EXELWTE_OPERATION_RELEASE)
                                                | semExelwteOptions;
                                local_status = local_status && ch->MethodWriteN(subChannel, LWC56F_SEM_ADDR_LO, 5, methodData, INCREMENTING);
                            }

                            // Set up a copy DMA
                            if ((transferSrcDst == trVidVid) || (transferSrcDst == trVidSys)) {
                                // Determine the src offset in vidmem
                                srcOffset = (UINT64)dmaBuffer[0].GetCtxDmaOffsetGpu();
                                srcAddr = reinterpret_cast<uintptr_t>(dmaBuffer[0].GetAddress());

                                // Determine the dst offset
                                if (transferSrcDst == trVidVid) {
                                    dstOffset = (UINT64)dmaBuffer[0].GetCtxDmaOffsetGpu() + (UINT64)surfaceSize;
                                    dstAddr = reinterpret_cast<uintptr_t>(dmaBuffer[0].GetAddress());
                                } else {
                                    dstOffset = (UINT64)dmaBuffer[1].GetCtxDmaOffsetGpu();
                                    dstAddr = reinterpret_cast<uintptr_t>(dmaBuffer[1].GetAddress());
                                }
                            } else {
                                // Determine the src offset in sysmem
                                srcOffset = (UINT64)dmaBuffer[1].GetCtxDmaOffsetGpu();
                                srcAddr = reinterpret_cast<uintptr_t>(dmaBuffer[1].GetAddress());

                                // Determine the dst offset
                                if (transferSrcDst == trSysSys) {
                                    dstOffset = (UINT64)dmaBuffer[1].GetCtxDmaOffsetGpu() + (UINT64)surfaceSize;
                                    dstAddr = reinterpret_cast<uintptr_t>(dmaBuffer[1].GetAddress());
                                } else {
                                    dstOffset = (UINT64)dmaBuffer[0].GetCtxDmaOffsetGpu();
                                    dstAddr = reinterpret_cast<uintptr_t>(dmaBuffer[0].GetAddress());
                                }
                            }
                            // Set the source offset in hw
                            methodData[0] = (UINT32)(srcOffset >> 32);
                            methodData[1] = (UINT32)(srcOffset);
                            local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_OFFSET_IN_UPPER), 2, methodData, INCREMENTING);

                            // Set the destination offset in hw
                            methodData[0] = (UINT32)(dstOffset >> 32);
                            methodData[1] = (UINT32)(dstOffset);
                            local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_OFFSET_OUT_UPPER), 2, methodData, INCREMENTING);

                            // Set up other DMA parameters
                            local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_PITCH_IN), dstPitch);
                            local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_PITCH_OUT), dstPitch);
                            local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LINE_LENGTH_IN), dstPitch);
                            local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LINE_COUNT), dstHeight);

                            // Set up the semaphore to signal DMA complete
                            methodData[0] = (UINT32)(semOffset >> 32);
                            methodData[1] = (UINT32)(semOffset);
                            methodData[2] = semValueLower;
                            methodData[3] = semValueUpper;
                            local_status = local_status && ch->MethodWriteN(subChannel, util->host_test_field(CE_METHOD_SET_SEMAPHORE_A), 3, methodData, INCREMENTING);

                            // Write the src surface using BAR1 accesses
                            if (surfaceCheck) {
                                WriteSrc(srcAddr);
                            }

                            // Send down an FB_FLUSH, MODS will use the correct register based on
                            // PCIe GEN of HW.
                            lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

                            local_status = local_status && ch->MethodWrite(subChannel, util->host_test_field(CE_METHOD_LAUNCH_DMA),
                                                                           util->host_test_dma_method(semPayloadSize));
                            ch->MethodFlush();
                            ch->WaitForIdle();

                            if ((semControl == semAcqCpuRelCpu) || (semControl == semAcqCpuRelGpu)) {
                                // use the CPU to poll on the semaphore release value
                                UINT32 rtnSemaphoreValue;
                                do {
                                    rtnSemaphoreValue = MEM_RD32(semAddr);
                                } while (rtnSemaphoreValue != semValueLower);
                            } else {
                                // Do a host semaphore acquire for end-of-dma
                                methodData[0] = (UINT32)(semOffset);
                                methodData[1] = (UINT32)(semOffset >> 32);
                                methodData[2] = semAcqValueLower;
                                methodData[3] = semAcqValueUpper;
                                methodData[4] = DRF_NUM(C56F, _SEM_EXELWTE, _OPERATION, LWC56F_SEM_EXELWTE_OPERATION_RELEASE)
                                                | semExelwteOptions;
                                local_status = local_status && ch->MethodWriteN(subChannel, LWC56F_SEM_ADDR_LO, 5, methodData, INCREMENTING);
                            }
                            ch->MethodFlush();
                            ch->WaitForIdle();

                            if (semTimestamp == semTimestampEn) {
                                local_status = local_status && (timeStamp <= (((UINT64)MEM_RD32(semAddr + 12) << 32) |(UINT64)MEM_RD32(semAddr + 8)));
                            }

                            // read in the destination data and check it;
                            if (surfaceCheck) {
                                local_status = local_status && ReadDst(dstAddr);
                            }

                            // Exit if we were not successful
                            if (!local_status) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    InfoPrintf("Done with semaphore_bashing test\n");
    EnableBackdoor();

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED);
    }

    return;
}

void semaphore_bashingTest::WriteSrc(uintptr_t srcAddr)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;

    InfoPrintf("semaphore_bashingTest::WriteSrc() - writing out src surface\n");
    // write the source surface in reverse order
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        MEM_WR32(srcAddr + i, data[i / 4]);
    }
}

int semaphore_bashingTest::ReadDst(uintptr_t dstAddr)
{
    UINT32 *data = (UINT32 *)surfaceBuffer;
    UINT32 value;
    int rtnStatus = 1;

    // read back the dst surface and compare it with the original
    InfoPrintf("semaphore_bashingTest::ReadDst() - reading back dst surface\n");
    for (int i = (int)surfaceSize - 4; i >= 0; i-=4) {
        value = MEM_RD32(dstAddr + i);
        if (value != data[i/4]) {
            ErrPrintf("Dst surface does not equal src surface at offset %d (src = 0x%08x, dst = 0x%08x) at line %d\n", i, data[i/4], value, __LINE__);
            rtnStatus = 0;
            break;
        }
    }

    return rtnStatus;
}

