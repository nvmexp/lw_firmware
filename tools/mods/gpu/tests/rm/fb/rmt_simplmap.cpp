/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// This test uses multiple clients to exercise the mapping of vid and sysmem
// objects into the GPU's (render) address sapce.
//

#include "gpu/tests/rmtest.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/tee.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/utility.h"
#include "random.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/userdalloc.h"
#include "class/cl0000.h" // LW01_NULL_OBJECT
#include "class/cl0002.h" // LW01_CONTEXT_DMA
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0041.h"// MEMORY CONTROLS
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clc397.h" // VOLTA_A
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include "ctrl/ctrl906f.h"  // LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS
#include "ctrl/ctrla06f.h"  // LWA06F_CTRL_CMD_GPFIFO_SCHEDULE
#include "ctrl/ctrlc36f.h"  // LWC36F_CTRL_CMD_GPFIFO_SCHEDULE
#include "class/clc361.h"  // VOLTA_USERMODE_A
#include "class/cla06fsubch.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define SIMPLEMAP_ID_PB_MEM            0xF00D0001
#define SIMPLEMAP_ID_PB_CTX            0xF00D0002
#define SIMPLEMAP_ID_ERR_MEM           0xF00D0003
#define SIMPLEMAP_ID_ERR_CTX           0xF00D0004
#define SIMPLEMAP_ID_NOT_MEM           0xF00D0005
#define SIMPLEMAP_ID_NOT_CTX           0xF00D0006
#define SIMPLEMAP_ID_CHAN              0xF00D0007
#define SIMPLEMAP_ID_GFX               0xF00D0008
#define SIMPLEMAP_ID_3DGFX             0xF00D0009
#define SIMPLEMAP_ID_MEM2_RGN          0xF00D000a
#define SIMPLEMAP_ID_TEST_MEMORY1      0xF00D000e
#define SIMPLEMAP_ID_TEST_MEMORY2      0xF00D000f
#define SIMPLEMAP_ID_TWOD              0xF00D0010
#define SIMPLEMAP_ID_VADDR_A           0xF00D0011
#define SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_1 0xF00D0012
#define SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_2 0xF00D0013
#define SIMPLEMAP_ID_MEMCTRL_TEST_VIDMEM0     0xF00D0014
#define SIMPLEMAP_ID_MEMCTRL_TEST_VIDMEM1     0xF00D0015
#define SIMPLEMAP_ID_MEMCTRL_TEST_SYSMEM0     0xF00D0016
#define SIMPLEMAP_ID_MEMCTRL_TEST_SYSMEM1     0xF00D0017
#define SIMPLEMAP_ID_KEPLER                   0xF00D0019
#define SIMPLEMAP_ID_MAXWELL                  0xF00D0019
#define SIMPLEMAP_ID_VOLTA                    0xF00D0019
#define SIMPLEMAP_ID_USERMODE                 0xF00DC300

#define SIMPLEMAP_SUBCH_TWOD           LWA06F_SUBCHANNEL_2D
#define SIMPLEMAP_SUBCH_KEPLER         LWA06F_SUBCHANNEL_3D
#define SIMPLEMAP_SUBCH_MAXWELL        LWA06F_SUBCHANNEL_3D
#define SIMPLEMAP_SUBCH_VOLTA          LWA06F_SUBCHANNEL_3D

#define PUSHBUFFER_BYTESIZE        (64*1024)

class SimpleMapTest: public RmTest
{
public:

    SimpleMapTest();
    virtual ~SimpleMapTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(StressTime, UINT32);

protected:

    static const LwU32 TestMemSize  = 64*1024;              // 64KB render page
    static const LwU32 TestMemPitch = 1024*sizeof(LwU32);   // 4KB per line
    static const LwU32 TestMemLines = TestMemSize/TestMemPitch;

    enum MemLocation {_sysMem = 0, _vidMem = 1};
    enum MemReferant {_cpuRef = 0, _gpuRef = 1};

    struct Client
    {
        LwRm *pLwRm;
        LwU32 hClient;
        LwU32 hDevice;
        LwU32 hSubdevice;
        LwU32 hVA;
        struct
        {
            void *pushBuffer;
            LwU64 pushBufferLimit, errorBufferLimit;
            void *errorBuffer;
        } pb;

        GpFifoChannel *pChannel;
        void *ControlRegs[Gpu::MaxNumSubdevices+1];

        LwU64 VALimit;
        LwU64 vaTestVidMem; // gpu reference ptr to vidmem
        void * pTestVidMem; // cpu reference ptr to vidmem
        LwU64 vaTestSysMem; // gpu reference ptr to sysmem
        void * pTestSysMem; // cpu reference ptr to sysmem

        LwU32 clearColor;
        LwU32 drawColor;
    };

    Client _client, _clientToo, _clientLonely;
    bool m_bIsSysmemAvailable;

    RC AllocChannel(Client &client);
    RC AllocKeplerObj(Client &client);
    RC AllocVoltaObj(Client &client);
    RC ScheduleClient(Client &client);
    RC AllocClientVASpace(Client &client);
    RC AllocTestMem(Client &client);
    RC MapTestMem(Client &client);
    RC WaitIdleClient(Client &client, MemReferant, LwU32 TimeoutMs);
    RC WaitIdleClientChannel(Client &client, LwU32 TimeoutMs);
    RC FillTestMemLine(Client &client, MemReferant mr, MemLocation ml, LwU32 Line, LwU32 Pattern);
    RC CompareTestMemLine(Client &client, MemReferant mr, MemLocation ml, LwU32 Line, LwU32 Pattern, bool &passes);
    RC CompareTestMemLine(Client &client, MemReferant memRef, MemLocation memLoc, LwU32 line, vector<LwU32> &colorTst, bool &passes);
    RC CopyTestMemLine(Client &client,MemReferant memRef, MemLocation srcMemLoc, LwU32 srcLine, MemLocation dstMemLoc,LwU32 dstLine);
    RC Stress();
    RC StressVirtPhysMapAPI();
    RC TestVASize();

    RC WriteFlush(Client &client);

private:
    Random m_Random;
    UINT32 m_StressTime;
    ThreeDAlloc m_3dAlloc;

    RC VidHeapAllocSize
    (
        UINT32   Owner,
        UINT32   hRoot,
        UINT32   hObjectParent,
        UINT32   Flags,
        UINT32   Attr,
        UINT64   Size,
        UINT64 * pLimit,
        UINT64   Alignment,
        UINT32   ComprCovg,
        UINT32 * pHandle
    );
};

//------------------------------------------------------------------------------
SimpleMapTest::SimpleMapTest() : m_StressTime(0)
{
    SetName("SimpleMapTest");
    m_3dAlloc.SetOldestSupportedClass(KEPLER_A);
    memset(&_client, 0, sizeof(_client));
    memset(&_clientToo, 0, sizeof(_clientToo));
    memset(&_clientLonely, 0, sizeof(_clientLonely));
}

//------------------------------------------------------------------------------
SimpleMapTest::~SimpleMapTest()
{
}

//------------------------------------------------------------------------
string SimpleMapTest::IsTestSupported()
{
    if(m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return RUN_RMTEST_TRUE;
    }

    return "Supported on Fermi+";
}

RC SimpleMapTest::WaitIdleClient(SimpleMapTest::Client &client, MemReferant memReferant, LwU32 TimeoutMs)
{
    if (memReferant == _gpuRef)
    {
        return WaitIdleClientChannel(client, TimeoutMs);
    }
    // we're not multithreaded so this is a nop at the moment
    return OK;
}

RC SimpleMapTest::WaitIdleClientChannel(SimpleMapTest::Client &client, LwU32 TimeoutMs)
{
    RC rc;

    // First do a WaitForDmaPush so that we spend less time inside the RM
    // (beneficial because of RM mutexing)
    rc = LwRmIdleChannels(client.hClient,
                          client.hDevice,
                          SIMPLEMAP_ID_CHAN,
                          1,          // numChannels
                          0, // phClients
                          0, // phDevices
                          0, // phChannels
                          DRF_DEF(OS30, _FLAGS, _BEHAVIOR, _SLEEP) |
                          DRF_DEF(OS30, _FLAGS, _IDLE, _CACHE1) |
                          DRF_DEF(OS30, _FLAGS, _IDLE, _ALL_ENGINES) |
                          DRF_DEF(OS30, _FLAGS, _CHANNEL, _SINGLE),
                          TimeoutMs * 1000);                        // Colwert to us for RM

    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rc) );

 Cleanup:
    return rc;
}

RC SimpleMapTest::AllocChannel(SimpleMapTest::Client &client)
{
    // Allocate a Push Buffer in system memory
    RC rc;
    LwU32 rmrc, chClass;
    LwU64 pbMemAddr = 0;
    LwRmPtr pLwRm;
    UserdAllocPtr pUserdAlloc;

    LwU32 hChannel, ChID;
    LW0080_CTRL_FIFO_GET_CHANNELLIST_PARAMS GetChidParams = {0};

    bool allocedPbMem  = false;
    bool allocedErrMem = false, allocedErrCtx = false;
    bool mappedPbMem   = false, mappedErrMem  = false;
    bool allocedChan   = false;

    UINT32 hErrMem, hPbMem;

    client.pb.pushBufferLimit = PUSHBUFFER_BYTESIZE-1;

    if (m_bIsSysmemAvailable)
    {
        rmrc = LwRmAllocMemory64(client.hClient,
                                 client.hDevice,
                                 SIMPLEMAP_ID_PB_MEM,
                                 LW01_MEMORY_SYSTEM,
                                 DRF_DEF(OS02,_FLAGS,_PHYSICALITY,_NONCONTIGUOUS) |
                                 DRF_DEF(OS02,_FLAGS,_COHERENCY,_CACHED),
                                 (void**)&client.pb.pushBuffer,
                                 &client.pb.pushBufferLimit);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
        allocedPbMem = true;
    }
    else
    {
        // GPU cannot access sysmem, so allocate video memory here
        hPbMem = SIMPLEMAP_ID_PB_MEM;

        CHECK_RC_CLEANUP(VidHeapAllocSize(
            client.hDevice,                   // Owner
            client.hClient,             // hRoot
            client.hDevice,                   // hObjectParent
            0,                          // Flags
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |      // Attr
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS),
            client.pb.pushBufferLimit,  // Size
            NULL,                       // Limit
            0,                          // Alignment
            0,                          // ComprCovg
            &hPbMem));                  // hMemory
        allocedPbMem = true;

        // Now map the memory to get the cpu virtual address
        // Again, call the RMAPI directly so we can provide our client and device handle.
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(LwRmMapMemory(
            client.hClient,                    // client
            client.hDevice,                          // device
            SIMPLEMAP_ID_PB_MEM,               // Allocated memory's handle
            0,                                 // offset into memory = 0
            client.pb.pushBufferLimit,         // length
            (void**)&client.pb.pushBuffer,     // cpu virtual addr to be returned
            0)));                              // flags
        mappedPbMem = true;
    }

    rmrc = LwRmMapMemoryDma(client.hClient,
                            client.hDevice,
                            client.hVA,
                            SIMPLEMAP_ID_PB_MEM,
                            0,
                            client.pb.pushBufferLimit,
                            DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                            DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                            &pbMemAddr);

    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Allocate memory for the error notifier
    client.pb.errorBufferLimit = (4*1024)-1;
    if (m_bIsSysmemAvailable)
    {
        rmrc = LwRmAllocMemory64(client.hClient,
                                 client.hDevice,
                                 SIMPLEMAP_ID_ERR_MEM,
                                 LW01_MEMORY_SYSTEM,
                                 DRF_DEF(OS02,_FLAGS,_PHYSICALITY,_NONCONTIGUOUS),
                                 (void **)&client.pb.errorBuffer,
                                 &client.pb.errorBufferLimit);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
        allocedErrMem = true;
    }
    else
    {
        // GPU cannot access sysmem, so allocate video memory here
        hErrMem = SIMPLEMAP_ID_ERR_MEM;

        CHECK_RC_CLEANUP(VidHeapAllocSize(
            client.hDevice,                   // Owner
            client.hClient,             // hRoot
            client.hDevice,                   // hObjectParent
            0,                          // Flags
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |      // Attr
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS),
            client.pb.errorBufferLimit, // Size
            NULL,                       // Limit
            0,                          // Alignment
            0,                          // ComprCovg
            &hErrMem));                  // hMemory
        allocedErrMem = true;

        // Now map the memory to get the cpu virtual address
        // Again, call the RMAPI directly so we can provide our client and device handle.
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(LwRmMapMemory(
            client.hClient,                    // client
            client.hDevice,                          // device
            SIMPLEMAP_ID_ERR_MEM,              // Allocated memory's handle
            0,                                 // offset into memory = 0
            client.pb.errorBufferLimit,        // length
            (void**)&client.pb.errorBuffer,    // cpu virtual addr to be returned
            0)));                              // flags
        mappedErrMem = true;
    }

    // Allocate Context DMA for the error buffer
    rmrc = LwRmAllocContextDma2(client.hClient,
                                SIMPLEMAP_ID_ERR_CTX,
                                LW01_CONTEXT_DMA,
                                DRF_DEF(OS03,_FLAGS,_ACCESS,_READ_WRITE) |
                                DRF_DEF(OS03,_FLAGS,_HASH_TABLE,_DISABLE),
                                SIMPLEMAP_ID_ERR_MEM,
                                0, sizeof(LwNotification) - 1);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    allocedErrCtx = true;

    CHECK_RC_CLEANUP(pLwRm->GetFirstSupportedClass(m_TestConfig.GetNumFifoClasses(),
                m_TestConfig.GetFifoClasses(), (UINT32*)&chClass, GetBoundGpuDevice()));

    CHECK_RC_CLEANUP(GetBoundGpuDevice()->GetUserdManager()->Get(&pUserdAlloc, chClass, client.pLwRm));

    // Allocate a GPFIFO DMA channel
    LW_CHANNELGPFIFO_ALLOCATION_PARAMETERS GPFifoAllocParams;
    memset(&GPFifoAllocParams, 0, sizeof(GPFifoAllocParams));
    GPFifoAllocParams.hObjectError  = SIMPLEMAP_ID_ERR_CTX;
    GPFifoAllocParams.hObjectBuffer = 0;
    GPFifoAllocParams.gpFifoOffset  = pbMemAddr;
    GPFifoAllocParams.gpFifoEntries = 16;
    GPFifoAllocParams.flags         = 0;
    GPFifoAllocParams.engineType    = LW2080_ENGINE_TYPE_GR(0);

    for (UINT32 sub = 0; sub < GetBoundGpuDevice()->GetNumSubdevices(); sub++)
    {
        GPFifoAllocParams.hUserdMemory[sub] = pUserdAlloc->GetMemHandle();
        GPFifoAllocParams.userdOffset[sub]  = pUserdAlloc->GetMemOffset(sub);
    }

    rmrc = LwRmAlloc(client.hClient, client.hDevice, SIMPLEMAP_ID_CHAN, chClass, (void*)&GPFifoAllocParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    allocedChan = true;

    // This is a spurious free only to test to that we can free then alloc again just like this...
    rmrc = LwRmFree(client.hClient, client.hDevice, SIMPLEMAP_ID_CHAN);
    allocedChan = false;
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Now we really mean it.
    rmrc = LwRmAlloc(client.hClient, client.hDevice, SIMPLEMAP_ID_CHAN, chClass, (void*)&GPFifoAllocParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    allocedChan = true;

    CHECK_RC(pUserdAlloc->Setup(static_cast<LwRm::Handle>(SIMPLEMAP_ID_CHAN)));

    // Get the channel id
    hChannel = SIMPLEMAP_ID_CHAN;
    ChID = 0;
    GetChidParams.numChannels = 1;
    GetChidParams.pChannelHandleList = LW_PTR_TO_LwP64(&hChannel);
    GetChidParams.pChannelList = LW_PTR_TO_LwP64(&ChID);
    rmrc = LwRmControl(client.hClient,
                       client.hDevice,
                       LW0080_CTRL_CMD_FIFO_GET_CHANNELLIST,
                       &GetChidParams,
                       sizeof(GetChidParams));
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Create a MODS GpFifoChannel object to manage this channel.  We need to carve out separate spaces for both the
    // gpfifo entries and pushbuffer proper.  We'll just start the pb itself at 0 and after that have the gpfifo.
    {
        LwU8 *pGpFifo = (LwU8*)client.pb.pushBuffer;
        LwU8 *pPb     = pGpFifo + (GPFifoAllocParams.gpFifoEntries * LW906F_GP_ENTRY__SIZE);

        MASSERT( (client.pb.pushBufferLimit + 1 -
            (GPFifoAllocParams.gpFifoEntries * LW906F_GP_ENTRY__SIZE)) <= 0x00000000FFFFFFFF);

        volatile void * errBuf =
            reinterpret_cast<volatile void *>(client.pb.errorBuffer);

        if (IsClassSupported(AMPERE_CHANNEL_GPFIFO_A) || IsClassSupported(HOPPER_CHANNEL_GPFIFO_A))
        {
            client.pChannel = new AmpereGpFifoChannel(pUserdAlloc,
                                                (void*)pPb,
                                                UINT32(client.pb.pushBufferLimit + 1
                                                    - (GPFifoAllocParams.gpFifoEntries
                                                       * LWC56F_GP_ENTRY__SIZE)),
                                                const_cast<void*>(errBuf),
                                                SIMPLEMAP_ID_CHAN,
                                                ChID,
                                                chClass,
                                                pbMemAddr + GPFifoAllocParams.gpFifoEntries * LWC56F_GP_ENTRY__SIZE, //pb offset
                                                (void*)pGpFifo,
                                                GPFifoAllocParams.gpFifoEntries,
                                                0,
                                                GetBoundGpuDevice(),
                                                client.pLwRm);
        }
        else if (IsClassSupported(VOLTA_CHANNEL_GPFIFO_A) || IsClassSupported(TURING_CHANNEL_GPFIFO_A))
        {
            client.pChannel = new VoltaGpFifoChannel(pUserdAlloc,
                                                (void*)pPb,
                                                UINT32(client.pb.pushBufferLimit + 1
                                                    - (GPFifoAllocParams.gpFifoEntries
                                                       * LWC36F_GP_ENTRY__SIZE)),
                                                const_cast<void*>(errBuf),
                                                SIMPLEMAP_ID_CHAN,
                                                ChID,
                                                chClass,
                                                pbMemAddr + GPFifoAllocParams.gpFifoEntries * LWC36F_GP_ENTRY__SIZE, //pb offset
                                                (void*)pGpFifo,
                                                GPFifoAllocParams.gpFifoEntries,
                                                0,
                                                GetBoundGpuDevice(),
                                                client.pLwRm);
        }
        else if (IsClassSupported(KEPLER_CHANNEL_GPFIFO_A) || IsClassSupported(KEPLER_CHANNEL_GPFIFO_B)  ||
            IsClassSupported(KEPLER_CHANNEL_GPFIFO_C) || IsClassSupported(MAXWELL_CHANNEL_GPFIFO_A) ||
            IsClassSupported(PASCAL_CHANNEL_GPFIFO_A))
        {
            client.pChannel = new KeplerGpFifoChannel(pUserdAlloc,
                                                (void*)pPb,
                                                UINT32(client.pb.pushBufferLimit + 1
                                                    - (GPFifoAllocParams.gpFifoEntries
                                                       * LWA06F_GP_ENTRY__SIZE)),
                                                const_cast<void*>(errBuf),
                                                SIMPLEMAP_ID_CHAN,
                                                ChID,
                                                chClass,
                                                pbMemAddr + GPFifoAllocParams.gpFifoEntries * LWA06F_GP_ENTRY__SIZE, //pb offset
                                                (void*)pGpFifo,
                                                GPFifoAllocParams.gpFifoEntries,
                                                0,
                                                GetBoundGpuDevice(),
                                                client.pLwRm);
        }
        if ( !client.pChannel )
        {
            Printf(Tee::PriNormal, "Failed to alloc GpFifoChannel!\n");
        }
        else
        {
            CHECK_RC_CLEANUP(client.pChannel->Initialize());
        }
    }
    return OK;

 Cleanup:
    if (client.pChannel)
    {
        delete client.pChannel;
    }
    if (allocedChan)
    {
        LwRmFree(client.hClient, client.hDevice, SIMPLEMAP_ID_CHAN);
    }
    if (pUserdAlloc)
    {
        pUserdAlloc.reset();
    }
    if (allocedErrCtx)
    {
        LwRmFree(client.hClient, client.hDevice, SIMPLEMAP_ID_ERR_CTX);
    }
    if ( mappedErrMem )
    {
        LwRmUnmapMemory(client.hClient, client.hDevice, SIMPLEMAP_ID_ERR_MEM, client.pb.errorBuffer, 0);
    }
    if ( allocedErrMem )
    {
        LwRmFree(client.hClient, client.hDevice, SIMPLEMAP_ID_ERR_MEM);
    }
    if ( mappedPbMem )
    {
        LwRmUnmapMemory(client.hClient, client.hDevice, SIMPLEMAP_ID_PB_MEM, client.pb.pushBuffer, 0);
    }
    if ( allocedPbMem )
    {
        LwRmFree(client.hClient, client.hDevice, SIMPLEMAP_ID_PB_MEM);
    }
    return rc;
}

RC SimpleMapTest::WriteFlush(Client &client)
{
    client.pChannel->Write(0, LW906F_FB_FLUSH, 0);
    return OK;
}

RC SimpleMapTest::ScheduleClient(Client &client)
{
    RC rc;
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};
    LWC36F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoVoltaSchedulParams = {0};

    if ((client.pChannel->GetClass() == VOLTA_CHANNEL_GPFIFO_A))
    {
        gpFifoVoltaSchedulParams.bEnable = true;

        // Schedule channel
        CHECK_RC_CLEANUP(LwRmControl(client.hClient,
                             SIMPLEMAP_ID_CHAN,
                             LWC36F_CTRL_CMD_GPFIFO_SCHEDULE,
                             &gpFifoVoltaSchedulParams,
                             sizeof(gpFifoVoltaSchedulParams)));
        client.pChannel->SetAutoSchedule(false);
    }

    else if (client.pChannel->GetClass() >= KEPLER_CHANNEL_GPFIFO_A)
    {
        gpFifoSchedulParams.bEnable = true;

        // Schedule channel
        CHECK_RC_CLEANUP(LwRmControl(client.hClient,
                             SIMPLEMAP_ID_CHAN,
                             LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                             &gpFifoSchedulParams,
                             sizeof(gpFifoSchedulParams)));
        client.pChannel->SetAutoSchedule(false);
    }
 Cleanup:
    return rc;
}

RC SimpleMapTest::AllocKeplerObj(Client &client)
{
    RC rc;
    LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS params = {0};
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};

    if (IsClassSupported(KEPLER_A))
    {
        CHECK_RC_CLEANUP( RmApiStatusToModsRC( LwRmAllocObject(client.hClient,
                                                               SIMPLEMAP_ID_CHAN,
                                                               SIMPLEMAP_ID_KEPLER,
                                                               KEPLER_A) ) );
    }
    else if (IsClassSupported(KEPLER_B))
    {
        CHECK_RC_CLEANUP( RmApiStatusToModsRC( LwRmAllocObject(client.hClient,
                                                               SIMPLEMAP_ID_CHAN,
                                                               SIMPLEMAP_ID_KEPLER,
                                                               KEPLER_B) ) );
    }
    else
    {
        CHECK_RC_CLEANUP( RmApiStatusToModsRC( LwRmAllocObject(client.hClient,
                                                               SIMPLEMAP_ID_CHAN,
                                                               SIMPLEMAP_ID_KEPLER,
                                                               KEPLER_C) ) );
    }
    gpFifoSchedulParams.bEnable = true;

    // Schedule channel
    CHECK_RC_CLEANUP( LwRmControl(client.hClient,
                         SIMPLEMAP_ID_CHAN,
                         LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                         &gpFifoSchedulParams,
                         sizeof(gpFifoSchedulParams)) );

    //
    // Multi-client support is broken in MODS so we have to do the SetObject here
    // (i.e.: FermiGpFifoChannel::SetObject doesn't work). This should've probably
    // been done for AllocFermiObj as well.
    //

    params.hObject = SIMPLEMAP_ID_KEPLER;

    CHECK_RC_CLEANUP( LwRmControl(client.hClient,
                         SIMPLEMAP_ID_CHAN,
                         LW906F_CTRL_GET_CLASS_ENGINEID,
                         &params,
                         sizeof(params)) );

    CHECK_RC_CLEANUP( client.pChannel->Write(SIMPLEMAP_SUBCH_KEPLER, 0, params.classEngineID) );

    CHECK_RC_CLEANUP( client.pChannel->Flush() );
    CHECK_RC_CLEANUP( WaitIdleClientChannel(client, 10000) );

 Cleanup:
    return rc;
}

RC SimpleMapTest::AllocVoltaObj(Client &client)
{
    RC rc;
    LWC36F_CTRL_GET_CLASS_ENGINEID_PARAMS params = {0};
    LWC36F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};

    CHECK_RC_CLEANUP( RmApiStatusToModsRC( LwRmAllocObject(client.hClient,
                                                               SIMPLEMAP_ID_CHAN,
                                                               SIMPLEMAP_ID_VOLTA,
                                                               VOLTA_A) ) );
    gpFifoSchedulParams.bEnable = true;

    // Schedule channel
    CHECK_RC_CLEANUP( LwRmControl(client.hClient,
                         SIMPLEMAP_ID_CHAN,
                         LWC36F_CTRL_CMD_GPFIFO_SCHEDULE,
                         &gpFifoSchedulParams,
                         sizeof(gpFifoSchedulParams)) );

    //
    // Multi-client support is broken in MODS so we have to do the SetObject here
    // (i.e.: FermiGpFifoChannel::SetObject doesn't work). This should've probably
    // been done for AllocFermiObj as well.
    //

    params.hObject = SIMPLEMAP_ID_VOLTA;

    CHECK_RC_CLEANUP( LwRmControl(client.hClient,
                         SIMPLEMAP_ID_CHAN,
                         LWC36F_CTRL_GET_CLASS_ENGINEID,
                         &params,
                         sizeof(params)) );

    CHECK_RC_CLEANUP( client.pChannel->Write(SIMPLEMAP_SUBCH_VOLTA, 0, params.classEngineID) );

    CHECK_RC_CLEANUP( client.pChannel->Flush() );
    CHECK_RC_CLEANUP( WaitIdleClientChannel(client, 10000) );

 Cleanup:
    return rc;
}

RC SimpleMapTest::CompareTestMemLine(Client &client, MemReferant memRef, MemLocation memLoc, LwU32 line, vector<LwU32> &colorTst, bool &passes)
{
    RC rc = OK;

    switch(memRef)
    {
        case _gpuRef:
        {
            //LwU64 ip = (memLoc == _vidMem) ? client.vaTestVidMem : client.vaTestSysMem;
            rc = RC::UNSUPPORTED_FUNCTION; // we don't do this (yet)
        }
        break;

        case _cpuRef:
        {
            LwU32 *ip = (memLoc == _vidMem) ? (LwU32*)client.pTestVidMem : (LwU32*)client.pTestSysMem;
            ip += line*(TestMemPitch/sizeof(LwU32));
            for (LwU32 i = 0; i < TestMemPitch/sizeof(LwU32); i++)
            {
                LwU32 colorRd = MEM_RD32(ip);
                passes = 0;
                for ( vector<LwU32>::iterator colorTst_i = colorTst.begin();
                      (!passes) && (colorTst_i != colorTst.end());
                      colorTst_i++ )
                {
                    passes |= (colorRd == *colorTst_i);
                }
                if ( !passes )
                {
                    Printf(Tee::PriHigh, "CPU read of _vidMem mismatch line 0x%x 0x%x != { ",
                        (UINT32) line, (UINT32) colorRd);
                    for ( vector<LwU32>::iterator it = colorTst.begin();
                          it != colorTst.end();
                          it++)
                        Printf(Tee::PriHigh, "0x%x, ", (UINT32) *it);
                    Printf(Tee::PriHigh, "}\n");
                    return OK; // mismatch isn't an rc failure
                }
                ip++;
            }
        }
        break;
        default:
            rc = RC::SOFTWARE_ERROR;
    }
    CHECK_RC_CLEANUP(rc);

    passes = true;
    return OK;

 Cleanup:
    return rc;
}

RC SimpleMapTest::CompareTestMemLine(Client &client, MemReferant memRef, MemLocation memLoc, LwU32 line, LwU32 colorTst, bool &passes)
{
    vector<LwU32>  __colorTst(1);
    __colorTst[0] = colorTst;
    return CompareTestMemLine(client, memRef, memLoc, line, __colorTst, passes);
}

RC SimpleMapTest::CopyTestMemLine
(
    Client &client,
    MemReferant memRef,
    MemLocation srcMemLoc,
    LwU32 srcLine,
    MemLocation dstMemLoc,
    LwU32 dstLine
)
{
    RC rc = OK;
    // overlapping copy elided
    if ((srcMemLoc == dstMemLoc) && (srcLine == dstLine))
        return RC::UNSUPPORTED_FUNCTION;

    switch ( memRef )
    {
        case _gpuRef:
        {
        }
        break;

        case _cpuRef:
        {
        }
        break;
        default:
            rc = RC::SOFTWARE_ERROR;
    }
    CHECK_RC_CLEANUP( rc );

 Cleanup:
    /*NOT REACHED*/
    return rc;
}

RC SimpleMapTest::FillTestMemLine(Client &client, MemReferant memRef, MemLocation memLoc, LwU32 line, LwU32 color)
{
    RC rc = OK;
    switch(memRef)
    {
        case _gpuRef:
        {
            LwU64 ip = (memLoc == _vidMem) ? client.vaTestVidMem : client.vaTestSysMem;
            // use client's twod subchannel to draw one line in the given "color"
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_SET_DST_OFFSET_UPPER, (LwU32)(ip>>32));
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_SET_DST_OFFSET_LOWER, (LwU32)(ip & (~(LwU32)0)));

            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_MODE, LW502D_RENDER_SOLID_PRIM_MODE_V_LINES);
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
                                   LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8);
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_SET_RENDER_SOLID_PRIM_COLOR, color);

            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), 0);
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), line);
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(1), (TestMemPitch/sizeof(LwU32))); //hopefully endpoint dropped
            client.pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_Y(1), line);
            CHECK_RC_CLEANUP( client.pChannel->Flush() );
        }
        break;
        case _cpuRef:
        {
            LwU32 *ip = (memLoc == _vidMem) ? ((LwU32*)client.pTestVidMem) : ((LwU32*)client.pTestSysMem);
            for (LwU32 i = 0; i < TestMemPitch/sizeof(LwU32); i++)
            {
                MEM_WR32(ip, color);
                ip++;
            }
        }
        break;
        default:
            rc = RC::SOFTWARE_ERROR;
    }
    CHECK_RC_CLEANUP(rc);

 Cleanup:
    return rc;
}

RC SimpleMapTest::MapTestMem(Client &client)
{
    LwU32 rmrc;
    RC rc;
    LwU32 flags = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE);
    LwU64 targetOffset;

    flags = DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE);
    //
    // Map test memory1 (vidmem) into the VA at a specific place: 1/2 VALimit
    //
    client.vaTestVidMem = (client.VALimit + 1)/2;
    targetOffset = client.vaTestVidMem;
    rmrc = LwRmMapMemoryDma(client.hClient, client.hDevice,
                            client.hVA,
                            SIMPLEMAP_ID_TEST_MEMORY1,
                            0,TestMemSize,
                            flags,
                            &client.vaTestVidMem);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    if ( targetOffset != client.vaTestVidMem )
    {
        Printf(Tee::PriHigh, "Failed to MapMemoryDma (vidmem) at absolute offset\n");
        rc = RC::CANNOT_ALLOCATE_MEMORY;
    }
    CHECK_RC_CLEANUP(rc);
    //
    // Map test memory2 (can be sysmem/vidmem) into the VA at a specific place: 3/4 VALimit
    //
    client.vaTestSysMem = (client.VALimit + 1)/4*3;
    targetOffset = client.vaTestSysMem;
    rmrc = LwRmMapMemoryDma(client.hClient, client.hDevice,
                            client.hVA,
                            SIMPLEMAP_ID_TEST_MEMORY2,
                            0,TestMemSize,
                            flags,
                            &client.vaTestSysMem);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    if ( targetOffset != client.vaTestSysMem )
    {
        Printf(Tee::PriHigh, "Failed to MapMemoryDma (sysmem) at absolute offset\n");
        rc = RC::CANNOT_ALLOCATE_MEMORY;
    }
    CHECK_RC_CLEANUP(rc);

    //
    // Map test memory 1 to CPU
    //
    rmrc = LwRmMapMemory(client.hClient, client.hDevice,
                         SIMPLEMAP_ID_TEST_MEMORY1,
                         0,TestMemSize,
                         &client.pTestVidMem,
                         LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    //
    // Map test memory2 (can be sysmem/vidmem) to CPU.  May be reflected(&remapped) from BAR1.
    //
    rmrc = LwRmMapMemory(client.hClient, client.hDevice,
                         SIMPLEMAP_ID_TEST_MEMORY2,
                         0,TestMemSize,
                         &client.pTestSysMem,
                         LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

 Cleanup:
    return rc;
}

RC SimpleMapTest::AllocTestMem(Client &client)
{
    LwU32 rmrc;
    RC rc;
    //
    // vidmem
    //
    {
        UINT32 hTestMem1 = SIMPLEMAP_ID_TEST_MEMORY1;

        CHECK_RC_CLEANUP(VidHeapAllocSize(
            0x61626364,                 // Owner
            client.hClient,             // hRoot
            client.hDevice,                   // hObjectParent
            LWOS32_ALLOC_FLAGS_NO_SCANOUT |
            LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE |
            LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,    // Flags
            DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) | // Attr
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS),
            TestMemSize,                // Size
            NULL,                       // Limit
            64*1024,                    // Alignment
            0,                          // ComprCovg
            &hTestMem1));               // hMemory

    }
    //
    // now sysmem if available, else vidmem
    //
    if (m_bIsSysmemAvailable)
    {
        LwU64 mem2RgnLimit = TestMemSize - 1;
        void *dummyAddress = 0;

        rmrc = LwRmAllocMemory64(client.hClient, client.hDevice,
                                 SIMPLEMAP_ID_TEST_MEMORY2,
                                 LW01_MEMORY_SYSTEM,
                                 ( DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                                   DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP) |
                                   DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) ),
                                 &dummyAddress, &mem2RgnLimit);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    }
    else
    {
        UINT32 hTestMem2 = SIMPLEMAP_ID_TEST_MEMORY2;

        CHECK_RC_CLEANUP(VidHeapAllocSize(
            0x61626364,                 // Owner
            client.hClient,             // hRoot
            client.hDevice,                   // hObjectParent
            LWOS32_ALLOC_FLAGS_NO_SCANOUT |
            LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE |
            LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,    // Flags
            DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) | // Attr
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) |
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB),
            TestMemSize - 1,            // Size
            NULL,                       // Limit
            64*1024,                    // Alignment
            0,                          // ComprCovg
            &hTestMem2));               // hMemory
    }

 Cleanup:
    return rc;
}

RC SimpleMapTest::AllocClientVASpace(Client &client)
{
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    LwU64 semLimit = (1ull << (DRF_SIZE(LWA097_SET_REPORT_SEMAPHORE_A_OFFSET_UPPER) +
                               DRF_SIZE(LWA097_SET_REPORT_SEMAPHORE_B_OFFSET_LOWER))) - 1;
    RC rc;
    LwU32 rmrc;

    client.hVA = 0;
    rmrc = client.pLwRm->Alloc(client.hDevice, &client.hVA, LW01_MEMORY_VIRTUAL, &vmparams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Semaphore releases have 40b VA until Hopper.  Test uses OFFSET_FIXED.
    client.VALimit = min(vmparams.limit, semLimit);

 Cleanup:
    return rc;
}

//------------------------------------------------------------------------------
RC SimpleMapTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());
    // TODO - Fill this in more

    m_bIsSysmemAvailable = GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_SYSMEM);
    return rc;
}

//------------------------------------------------------------------------------
RC SimpleMapTest::Run()
{
    RC rc;
    LwU32 rmrc;
    LwU64 mem2RgnLimit = 0;
    void *dummyAddress = 0;
    UINT32 hMem2 = 0;
    LWOS32_PARAMETERS allocParams = {0};
    LwU64 dmaOffset;
    void *vpSysmemSysmemPtr = 0;
    void *vpVidmemSysmemPtr = 0;
    MemReferant memRefs[2] = {_cpuRef, _gpuRef};
    MemLocation memLocs[2] = {_sysMem, _vidMem};
    LwRmPtr pLwRm;
    LW0080_CTRL_DMA_FLUSH_PARAMS dmaFlushParams = {0};

    static const unsigned int numMemRefs = sizeof(memRefs)/sizeof(memRefs[0]);
    static const unsigned int numMemLocs = sizeof(memLocs)/sizeof(memLocs[0]);

    vector< vector< vector <LwU32> > > colors(2/*sharegroups*/, vector< vector<LwU32> >(numMemRefs, vector<LwU32>(numMemLocs)));

    Printf(Tee::PriNormal, "Starting test.  Seed=0x%x StresTime=%d secs.\n", m_TestConfig.Seed(), m_StressTime);

    m_Random.SeedRandom(m_TestConfig.Seed());

   memset(&_client, 0, sizeof(Client));       // primary client
   memset(&_clientToo, 0, sizeof(Client));    // client we use to verify VA space sharing works
   memset(&_clientLonely, 0, sizeof(Client)); // client we use to verify VA space non-sharing works

    // Using MODS core classes with private roots will fail if the core
    // classes then try to use the LwRm MODS abstrations (which will operate on
    // a different client).  MODS core classes generally assume usage of the
    // LwRm abstraction.  At a minimum, the root, device, and subdevice handles
    // need to be implemented withing the MODS LwRm abstraction in order to
    // use MODS core classes (this particular test uses the MODS channel
    // abstraction which does rely on haveing a valid populated LwRm abstraction)
    _client.pLwRm = LwRmPtr::GetFreeClient();
    if (nullptr == _client.pLwRm)
    {
        Printf(Tee::PriHigh, "Unable to get LwRm pointer for _client!\n");
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }
    CHECK_RC_CLEANUP(_client.pLwRm->AllocRoot());
    _client.hClient = _client.pLwRm->GetClientHandle();

    _clientToo.pLwRm = LwRmPtr::GetFreeClient();
    if (nullptr == _clientToo.pLwRm)
    {
        Printf(Tee::PriHigh, "Unable to get LwRm pointer for _clientToo!\n");
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }
    CHECK_RC_CLEANUP(_clientToo.pLwRm->AllocRoot());
    _clientToo.hClient = _clientToo.pLwRm->GetClientHandle();

    _clientLonely.pLwRm = LwRmPtr::GetFreeClient();
    if (nullptr == _clientLonely.pLwRm)
    {
        Printf(Tee::PriHigh, "Unable to get LwRm pointer for _clientLonely!\n");
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }
    CHECK_RC_CLEANUP(_clientLonely.pLwRm->AllocRoot());
    _clientLonely.hClient = _clientLonely.pLwRm->GetClientHandle();

    //
    // Alloc devices under each client
    //
    LW0080_ALLOC_PARAMETERS allocDeviceParms;
    memset(&allocDeviceParms, 0, sizeof(allocDeviceParms));
    allocDeviceParms.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParms.flags = 0;

    // 'client' gets default share behavior (either new SG or global)
    allocDeviceParms.hClientShare = LW01_NULL_OBJECT;
    CHECK_RC_CLEANUP(_client.pLwRm->AllocDevice(&allocDeviceParms));
    CHECK_RC_CLEANUP(_client.pLwRm->AllocSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                                   GetBoundGpuSubdevice()->GetSubdeviceInst()));
    _client.hDevice = _client.pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    _client.hSubdevice = _client.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    CHECK_RC_CLEANUP(GetBoundGpuDevice()->AllocSingleClientStuff(_client.pLwRm, GetBoundGpuSubdevice()));
    
    // 'clientToo' shares with whatever group 'client' turns up in
    allocDeviceParms.hClientShare = _client.hClient;
    CHECK_RC_CLEANUP(_clientToo.pLwRm->AllocDevice(&allocDeviceParms));
    CHECK_RC_CLEANUP(_clientToo.pLwRm->AllocSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                                      GetBoundGpuSubdevice()->GetSubdeviceInst()));
    _clientToo.hDevice = _clientToo.pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    _clientToo.hSubdevice = _clientToo.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    CHECK_RC_CLEANUP(GetBoundGpuDevice()->AllocSingleClientStuff(_clientToo.pLwRm, GetBoundGpuSubdevice()));
    
    // 'clientLonely' should be in its own SG no matter what
    allocDeviceParms.hClientShare = _clientLonely.hClient;
    CHECK_RC_CLEANUP(_clientLonely.pLwRm->AllocDevice(&allocDeviceParms));
    CHECK_RC_CLEANUP(_clientLonely.pLwRm->AllocSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                                         GetBoundGpuSubdevice()->GetSubdeviceInst()));
    _clientLonely.hDevice = _clientLonely.pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    _clientLonely.hSubdevice = _clientLonely.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    CHECK_RC_CLEANUP(GetBoundGpuDevice()->AllocSingleClientStuff(_clientLonely.pLwRm, GetBoundGpuSubdevice()));
    
    // Create (_gpuRef) VA space for each (client,channel).
    CHECK_RC_CLEANUP( AllocClientVASpace(_client));
    CHECK_RC_CLEANUP( AllocClientVASpace(_clientToo));
    CHECK_RC_CLEANUP( AllocClientVASpace(_clientLonely));

    // Allocate channels for each of the clients
    CHECK_RC_CLEANUP( AllocChannel(_client));
    CHECK_RC_CLEANUP( AllocChannel(_clientToo));
    CHECK_RC_CLEANUP( AllocChannel(_clientLonely));

    // Allocate and map {_cpuRef,_gpuRef} to the test's mem buffers.
    CHECK_RC_CLEANUP( AllocTestMem(_client));
    CHECK_RC_CLEANUP( AllocTestMem(_clientLonely));
    CHECK_RC_CLEANUP( MapTestMem(_client));
    CHECK_RC_CLEANUP( MapTestMem(_clientLonely));

    // clientToo is in the va sharegroup with _client.
    _clientToo.vaTestVidMem = _client.vaTestVidMem;
    _clientToo.vaTestSysMem = _client.vaTestSysMem;
    _clientToo.pTestVidMem = _client.pTestVidMem;
    _clientToo.pTestSysMem = _client.pTestSysMem;

    CHECK_RC_CLEANUP(ScheduleClient(_client));
    CHECK_RC_CLEANUP(ScheduleClient(_clientToo));
    CHECK_RC_CLEANUP(ScheduleClient(_clientLonely));

    // Both client and clientLonely should have the same VA
    if (_client.vaTestVidMem != _clientLonely.vaTestVidMem)
    {
        Printf(Tee::PriHigh, "client and clientLonely should point to the same VA!\n");
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }

    _client.pChannel->SetSemaphoreOffset(_client.vaTestVidMem);
    _client.pChannel->SemaphoreRelease(0x12345678);

    CHECK_RC(WriteFlush(_client));

    CHECK_RC_CLEANUP( _client.pChannel->Flush() );

    CHECK_RC_CLEANUP( WaitIdleClientChannel(_client, 10000) );

    _clientLonely.pChannel->SetSemaphoreOffset(_clientLonely.vaTestVidMem);
    _clientLonely.pChannel->SemaphoreRelease(0x87654321);

    CHECK_RC(WriteFlush(_clientLonely));

    CHECK_RC_CLEANUP( _clientLonely.pChannel->Flush() );

    CHECK_RC_CLEANUP( WaitIdleClientChannel(_clientLonely, 10000) );

    if (MEM_RD32(_client.pTestVidMem) != 0x12345678)
    {
        Printf(Tee::PriHigh, "SimplMapTest: _client semaphore value incorrect (0x%08x != 0x%08x)\n",
                MEM_RD32(_client.pTestVidMem), 0x12345678);
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    if (MEM_RD32(_clientLonely.pTestVidMem) != 0x87654321)
    {
        Printf(Tee::PriHigh, "SimplMapTest: _clientLonely semaphore value incorrect (0x%x != 0x87654321)\n", MEM_RD32(_clientLonely.pTestVidMem));
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

    MEM_WR32(_client.pTestVidMem, 0x11bad00f);

    CHECK_RC_CLEANUP(Platform::FlushCpuWriteCombineBuffer());

    dmaFlushParams.targetUnit = DRF_DEF(0080, _CTRL_DMA, _FLUSH_TARGET_UNIT_FB, _ENABLE);
    rc = pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_DMA_FLUSH,
                        &dmaFlushParams,
                        sizeof (dmaFlushParams));
    CHECK_RC_CLEANUP(rc);

    _clientToo.pChannel->SetSemaphoreOffset(_clientToo.vaTestVidMem);
    _clientToo.pChannel->SemaphoreAcquire(0x11bad00f);
    _clientToo.pChannel->SemaphoreRelease(0xf00dba11);

    CHECK_RC_CLEANUP( _clientToo.pChannel->Flush() );

    CHECK_RC_CLEANUP( WaitIdleClientChannel(_clientToo, 10000) );

    _client.pChannel->SetSemaphoreOffset(_client.vaTestVidMem);
    _client.pChannel->SemaphoreAcquire(0xf00dba11);
    _client.pChannel->SemaphoreRelease(0x11bad00f);

    CHECK_RC(WriteFlush(_client));
    CHECK_RC_CLEANUP( _client.pChannel->Flush() );

    CHECK_RC_CLEANUP( WaitIdleClientChannel(_client, 10000) );

    if (MEM_RD32(_client.pTestVidMem) != 0x11bad00f)
    {
        Printf(Tee::PriHigh, "SimplMapTest: _client semaphore value incorrect (0x%08x != 0x%08x)\n",
                MEM_RD32(_client.pTestVidMem), 0x11bad00f);
        CHECK_RC(RC::DATA_MISMATCH);
    }

    if (MEM_RD32(_clientToo.pTestVidMem) != 0x11bad00f)
    {
        Printf(Tee::PriHigh, "SimplMapTest: _clientToo semaphore value incorrect\n");
        CHECK_RC(RC::DATA_MISMATCH);
    }

    CHECK_RC_CLEANUP( StressVirtPhysMapAPI() );
    CHECK_RC_CLEANUP( TestVASize() );

    //
    // Do some tricky API stuff for grins.
    //
    allocParams.hRoot = _client.hClient;
    allocParams.hObjectParent = _client.hDevice;
    allocParams.function = LWOS32_FUNCTION_ALLOC_SIZE; // in
    allocParams.total  = 0; // out
    allocParams.free   = 0; // out
    allocParams.status = 0; // out
    allocParams.data.AllocSize.owner = 0x61626364;
    allocParams.data.AllocSize.hMemory = 0;// out, we're allowing RM to create the handle.
    allocParams.data.AllocSize.type = LWOS32_TYPE_IMAGE;
    allocParams.data.AllocSize.size = 4*64*1024;
    allocParams.data.AllocSize.alignment = 1;
    allocParams.data.AllocSize.offset = 0; // out
    allocParams.data.AllocSize.address = 0; // out
    allocParams.data.AllocSize.limit = 0; // out
    allocParams.data.AllocSize.attr =
        ( DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
          DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
          DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
          DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
          /*heyhey*/DRF_DEF(OS32, _ATTR, _COMPR, _REQUIRED) |
          DRF_DEF(OS32, _ATTR, _COMPR_COVG, _PROVIDED) |
          DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR) |
          DRF_DEF(OS32, _ATTR, _Z_TYPE, _FIXED) |
          DRF_DEF(OS32, _ATTR, _ZS_PACKING, _Z32) |
          DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
          DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) |
          DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED)
          ); // inout
    allocParams.data.AllocSize.flags =
        (
         LWOS32_ALLOC_FLAGS_NO_SCANOUT |
         LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED
         );
    allocParams.data.AllocSize.partitionStride = 0; // default, inout
    allocParams.data.AllocSize.format = 0; // inout
    allocParams.data.AllocSize.comprCovg = (
                                            DRF_DEF(OS32, _ALLOC, _COMPR_COVG_BITS, _DEFAULT) |
                                            DRF_NUM(OS32, _ALLOC, _COMPR_COVG_MIN, 0) |
                                            DRF_NUM(OS32, _ALLOC, _COMPR_COVG_MAX, LWOS32_ALLOC_COMPR_COVG_SCALE*25) |
                                            DRF_NUM(OS32, _ALLOC, _COMPR_COVG_START, LWOS32_ALLOC_COMPR_COVG_SCALE*50));

    rmrc = LwRmVidHeapControl((void*)&allocParams);
    allocParams.data.AllocSize.hMemory = 0; // pump it so we get a non trivial fboffset (ie. not 0)
    allocParams.data.AllocSize.ctagOffset = 0;
    rmrc = LwRmVidHeapControl((void*)&allocParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // ---------------------------------------------
    // Allocate a sysmem region if possible to plug in, else allocate video memory.
    mem2RgnLimit = allocParams.data.AllocSize.size - 1;

    if (m_bIsSysmemAvailable)
    {
        rmrc = LwRmAllocMemory64(_client.hClient, _client.hDevice, SIMPLEMAP_ID_MEM2_RGN, LW01_MEMORY_SYSTEM,
                                 DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) | DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP),
                                 &dummyAddress, &mem2RgnLimit);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    }
    else
    {
        hMem2 = SIMPLEMAP_ID_MEM2_RGN;
        CHECK_RC_CLEANUP(VidHeapAllocSize(
            0x61626364,                 // Owner
            _client.hClient,            // hRoot
            _client.hDevice,                   // hObjectParent
            LWOS32_ALLOC_FLAGS_NO_SCANOUT |
            LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,        // Flags
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS),
            mem2RgnLimit,               // Size
            NULL,                       // Limit
            0,                          // Alignment
            0,                          // ComprCovg
            &hMem2));                   // hMemory
    }
    // ---------------------------------------------
    // Map memory2 (system/vidmem) memory into the VA
    rmrc = LwRmMapMemoryDma(_client.hClient, _client.hDevice, _client.hVA,
                            SIMPLEMAP_ID_MEM2_RGN,
                            0, allocParams.data.AllocSize.limit + 1,
                            0, &dmaOffset);

    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    Printf(Tee::PriNormal, "%s: Memory 2 (%s) chunk gpu ref offset=0x%llx\n",
        __FUNCTION__, m_bIsSysmemAvailable? "sysmem" : "vidmem", dmaOffset);

    // --------------------------------------------------
    // Map the memory1 (video memory) into the VA
    rmrc = LwRmMapMemoryDma(_client.hClient, _client.hDevice, _client.hVA,
                             allocParams.data.AllocSize.hMemory,
                             0, allocParams.data.AllocSize.limit + 1,
                             0, &dmaOffset);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    Printf(Tee::PriNormal, "%s: Video memory chunk gpu ref offset=0x%llx\n",
        __FUNCTION__, dmaOffset);

    // --------------------------------------------------
    // Map memory2 (system/vidmem) into the CPU's address space.
    rmrc = LwRmMapMemory(_client.hClient, _client.hDevice,
                         SIMPLEMAP_ID_MEM2_RGN,
                         0, allocParams.data.AllocSize.limit + 1,
                         &vpSysmemSysmemPtr,
                         LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    Printf(Tee::PriNormal, "%s: Memory2 (%s) chunk cpu (user) ref offset=%p\n",
        __FUNCTION__, m_bIsSysmemAvailable? "sysmem" : "vidmem", vpSysmemSysmemPtr);

    // --------------------------------------------------
    // Map the video memory into the CPU's address space.
    rmrc = LwRmMapMemory(_client.hClient, _client.hDevice,
                         allocParams.data.AllocSize.hMemory,
                         0, allocParams.data.AllocSize.limit + 1,
                         &vpVidmemSysmemPtr,
                         LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    Printf(Tee::PriNormal, "%s: Video memory chunk cpu (user) ref offset=%p\n",
        __FUNCTION__, vpVidmemSysmemPtr);

    rmrc = LwRmFree(_client.hClient, _client.hDevice, allocParams.data.AllocSize.hMemory);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    if (hMem2)
    {
        rmrc = LwRmFree(_client.hClient, _client.hDevice, hMem2);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    }
 Cleanup:
    // TODO - Fill this in - at least make sure we don't leak news
    if (_client.pChannel)
    {
        delete _client.pChannel;
    }
    if (_clientToo.pChannel)
    {
        delete _clientToo.pChannel;
    }
    if (_clientLonely.pChannel)
    {
        delete _clientLonely.pChannel;
    }

    if ( _clientLonely.pLwRm && !_clientLonely.pLwRm->IsFreed())
    {
        if (_clientLonely.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()))
        {
            _clientLonely.pLwRm->FreeSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                               GetBoundGpuSubdevice()->GetSubdeviceInst());
        }
        if (_clientLonely.pLwRm->GetDeviceHandle(GetBoundGpuDevice()))
        {
            _clientLonely.pLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
        }

        _clientLonely.pLwRm->FreeClient();
    }
    if ( _clientToo.pLwRm && !_clientToo.pLwRm->IsFreed())
    {
        if (_clientToo.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()))
        {
            _clientToo.pLwRm->FreeSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                            GetBoundGpuSubdevice()->GetSubdeviceInst());
        }
        if (_clientToo.pLwRm->GetDeviceHandle(GetBoundGpuDevice()))
        {
            _clientToo.pLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
        }

        _clientToo.pLwRm->FreeClient();
    }
    if ( _client.pLwRm && !_client.pLwRm->IsFreed())
    {
        if (_client.pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()))
        {
            _client.pLwRm->FreeSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                         GetBoundGpuSubdevice()->GetSubdeviceInst());
        }
        if (_client.pLwRm->GetDeviceHandle(GetBoundGpuDevice()))
        {
            _client.pLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
        }

        _client.pLwRm->FreeClient();
    }

    Printf(Tee::PriNormal, "Test %s.\n", (rc == OK)?"passed":"failed");

    return rc;
}

//------------------------------------------------------------------------------
RC SimpleMapTest::Cleanup()
{
    return OK;
}

RC SimpleMapTest::StressVirtPhysMapAPI()
{
    bool passes;
    LwU32 rmrc;
    RC rc;
    LWOS32_PARAMETERS allocParams = {0};
    LWOS32_PARAMETERS freeParams = {0};
    LWOS32_PARAMETERS allocPhysParams = {0};
    LwU64 vaddrA, mappedAddrA;

    // Note the use of _clientLonely here to guarantee we have
    // no sharing of address space going on.  I.e. this sequence
    // of events should not be impacted by other clients' actions
    // (in the global share).

    allocParams.hRoot = _clientLonely.hClient;
    allocParams.hObjectParent = _clientLonely.hDevice;
    allocParams.function = LWOS32_FUNCTION_ALLOC_SIZE; // in
    allocParams.data.AllocSize.owner = 0x61626364;
    allocParams.data.AllocSize.hMemory = SIMPLEMAP_ID_VADDR_A;
    allocParams.data.AllocSize.type = LWOS32_TYPE_IMAGE;
    allocParams.data.AllocSize.size = TestMemSize;
    allocParams.data.AllocSize.alignment = 1;
    allocParams.data.AllocSize.offset = 0; // out
    allocParams.data.AllocSize.address = 0; // out
    allocParams.data.AllocSize.limit = 0; // out
    allocParams.data.AllocSize.attr =
        ( DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
          DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
          DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
          DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT)
          );
    allocParams.data.AllocSize.flags =
        ( LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
          LWOS32_ALLOC_FLAGS_VIRTUAL
          );
    allocParams.data.AllocSize.partitionStride = 0; // default, inout
    allocParams.data.AllocSize.format = 0; // inout
    allocParams.data.AllocSize.ctagOffset = 0;

    rmrc = LwRmVidHeapControl((void*)&allocParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    vaddrA = allocParams.data.AllocSize.offset;

    // Free it for kicks.
    freeParams.hRoot = allocParams.hRoot;
    freeParams.hObjectParent = allocParams.hObjectParent;
    freeParams.function = LWOS32_FUNCTION_FREE;
    freeParams.data.Free.owner = allocParams.data.AllocSize.owner;
    freeParams.data.Free.hMemory = allocParams.data.AllocSize.hMemory;
    freeParams.data.Free.flags = LWOS32_FREE_FLAGS_MEMORY_HANDLE_PROVIDED;

    rmrc = LwRmVidHeapControl((void*)&freeParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Allocate it again.  Should get the same addr back...
    rmrc = LwRmVidHeapControl((void*)&allocParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    passes = vaddrA == allocParams.data.AllocSize.offset;
    CHECK_RC_CLEANUP(passes ? OK : RC::SOFTWARE_ERROR);

    // Allocate some physical backing for the vaddr space we just created.
    // make 1/2 of it vidmem and the other 1/2 sysmem.  we won't try to
    // render to it (yet) so this should work even on an lw50 where that
    // isn't possible...
    allocPhysParams = allocParams;
    allocPhysParams.data.AllocSize.hMemory = SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_1;
    allocPhysParams.data.AllocSize.size = TestMemSize/2;  // <<<<------ 1/2 size
    allocPhysParams.data.AllocSize.attr =
        ( DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
          DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
          DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
          DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
          DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
          DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
          DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
          DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) |
          DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB)
          ); // inout
    allocPhysParams.data.AllocSize.flags =
        ( LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
          LWOS32_ALLOC_FLAGS_NO_SCANOUT |
          LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED );

    rmrc = LwRmVidHeapControl((void*)&allocPhysParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Now map the backing into the space at vaddrA.
    mappedAddrA = 0; // This is 'in' and is the offset into the virt addr space @ object given.
    rmrc = LwRmMapMemoryDma(_clientLonely.hClient, _clientLonely.hDevice,
                            SIMPLEMAP_ID_VADDR_A, // hMemCtx
                            SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_1,
                            0, TestMemSize/2, // <<<<< -------  1/2 size
                            DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE),
                            &mappedAddrA);    // <<<< -------- starting at vaddrA + 0
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    passes = mappedAddrA == vaddrA;
    CHECK_RC_CLEANUP(passes ? OK : RC::SOFTWARE_ERROR);

    // now allocate some system memory (if possible else choose video memory) and map that too at the last 1/2
    allocPhysParams = allocParams;
    allocPhysParams.data.AllocSize.hMemory = SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_2;
    allocPhysParams.data.AllocSize.size = TestMemSize/2;  // <<<<------ 1/2 size
    allocPhysParams.data.AllocSize.attr =
        ( DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
          DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
          DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
          DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
          DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
          DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
          DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED) |
          DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS)
          ); // inout
    if(m_bIsSysmemAvailable)
    {
        allocPhysParams.data.AllocSize.attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);    // <<<< ---- sysmem, ncoh, ncontig
    }
    else
    {
        allocPhysParams.data.AllocSize.attr |= DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM); // <<<< ---- vidmem, ncontig

        // Since we're choosing vidmem, we also need to explicitly mention page size of 4KB because
        // the test memory size is 64K here and we'll be mapping _BACKING_MEMORY_2 at 64K/2 = 32K offset
        // into the test memory. Since VAspace for backing memory will be allocated at page boundary,
        // having a page size greater than 32K(the vaspace offset at which we'll be mapping backing memory)
        // won't work for us. So select the page size = 4KB here for _BACKING_MEMORY_2.
        allocPhysParams.data.AllocSize.attr |= DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    }
   //
   //
    allocPhysParams.data.AllocSize.flags =
        ( LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
          LWOS32_ALLOC_FLAGS_NO_SCANOUT |
          LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED );
    allocParams.data.AllocSize.ctagOffset = 0;

    rmrc = LwRmVidHeapControl((void*)&allocPhysParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Now map the backing into the space at vaddrA + TestMemSize/2.
    mappedAddrA = TestMemSize/2; // This is 'in' and is the offset into the virt addr space @ object given.
    rmrc = LwRmMapMemoryDma(_clientLonely.hClient, _clientLonely.hDevice,
                            SIMPLEMAP_ID_VADDR_A, // hMemCtx
                            SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_2,
                            0, TestMemSize/2, // <<<<< -------  1/2 size
                            DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE),
                            &mappedAddrA);    // <<<< -------- starting at vaddrA + TestMemSize/2
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
    passes = mappedAddrA == (vaddrA + TestMemSize/2);
    CHECK_RC_CLEANUP(passes ? OK : RC::SOFTWARE_ERROR);

    // Unmap the backing physical memory objects
    rmrc = LwRmUnmapMemoryDma(_clientLonely.hClient, _clientLonely.hDevice,
                              SIMPLEMAP_ID_VADDR_A,
                              SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_2,
                              0, vaddrA + TestMemSize/2);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    rmrc = LwRmUnmapMemoryDma(_clientLonely.hClient, _clientLonely.hDevice,
                              SIMPLEMAP_ID_VADDR_A,
                              SIMPLEMAP_ID_VADDR_A_BACKING_MEMORY_1,
                              0, vaddrA);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    // Delete the virtual memory object (frees the VA space).
    freeParams.hRoot = allocParams.hRoot;
    freeParams.hObjectParent = allocParams.hObjectParent;
    freeParams.function = LWOS32_FUNCTION_FREE;
    freeParams.data.Free.owner = allocParams.data.AllocSize.owner;
    freeParams.data.Free.hMemory = allocParams.data.AllocSize.hMemory;
    freeParams.data.Free.flags = LWOS32_FREE_FLAGS_MEMORY_HANDLE_PROVIDED;

    rmrc = LwRmVidHeapControl((void*)&freeParams);
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

 Cleanup:
    return rc;
}

//
// Test allocating RM device with a specified VA limit.  Loop over several
// valid values, allocating the device, then allocating a grows down virtual
// surface. The allocated address should be less than the VA limit.
//
RC SimpleMapTest::TestVASize()
{
    LW0080_ALLOC_PARAMETERS allocDeviceParms = {0};
    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS vaCaps = {0};
    Client client;
    LwU32 vaBits;
    LwU32 rmrc;
    RC rc = OK;

    // Get max VA size from an existing client
    rmrc = LwRmControl(_client.hClient,
                       _client.hDevice,
                       LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
                       (void*)&vaCaps,
                       sizeof(vaCaps));
    CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

    client.hClient = 0;

    // May need 3PDEs on tesla == 1.5GB, rounds to 2GB -> 31b
    for (vaBits = 31; vaBits < vaCaps.vaBitCount; vaBits++)
    {
        client.pLwRm = LwRmPtr::GetFreeClient();
        if (nullptr == client.pLwRm)
        {
            Printf(Tee::PriHigh, "%s: Unable to get LwRm pointer for client!\n", __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
            CHECK_RC_CLEANUP(rc);
        }
        CHECK_RC_CLEANUP(client.pLwRm->AllocRoot());
        client.hClient = client.pLwRm->GetClientHandle();

        // Private address space
        allocDeviceParms.hClientShare = client.hClient;
        allocDeviceParms.deviceId     = 0;
        allocDeviceParms.flags        = LW_DEVICE_ALLOCATION_FLAGS_VASPACE_SIZE;
        allocDeviceParms.vaSpaceSize  = (1ull << vaBits);
        CHECK_RC_CLEANUP(client.pLwRm->AllocDevice(&allocDeviceParms));
        client.hDevice = client.pLwRm->GetDeviceHandle(GetBoundGpuDevice());

        // Alloc handle to VASPACE
        CHECK_RC_CLEANUP( AllocClientVASpace(client) );

        // Allocate a virtual surface with grows down
        // check that the address is less than the capped
        // VASPACE
        LWOS32_PARAMETERS allocParams = {0};

        allocParams.hRoot = client.hClient;
        allocParams.hObjectParent = client.hDevice;
        allocParams.function = LWOS32_FUNCTION_ALLOC_SIZE; // in
        allocParams.data.AllocSize.owner = 0x61626364;
        allocParams.data.AllocSize.hMemory = SIMPLEMAP_ID_VADDR_A;
        allocParams.data.AllocSize.type = LWOS32_TYPE_IMAGE;
        allocParams.data.AllocSize.size = TestMemSize;
        allocParams.data.AllocSize.alignment = 1;
        allocParams.data.AllocSize.offset = 0; // out
        allocParams.data.AllocSize.address = 0; // out
        allocParams.data.AllocSize.limit = 0; // out
        allocParams.data.AllocSize.attr =
            ( DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
              DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
              DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
              DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT)
            );
        allocParams.data.AllocSize.flags =
            ( LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
              LWOS32_ALLOC_FLAGS_VIRTUAL |
              LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN
            );
        allocParams.data.AllocSize.partitionStride = 0; // default, inout
        allocParams.data.AllocSize.format = 0; // inout
        allocParams.data.AllocSize.ctagOffset = 0;

        rmrc = LwRmVidHeapControl((void*)&allocParams);
        CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );

        if (allocParams.data.AllocSize.offset >= allocDeviceParms.vaSpaceSize)
        {
            Printf(Tee::PriHigh, "Bad VA %llx for VA space of %d bits!\n",
                   allocParams.data.AllocSize.offset, vaBits);
            rc = RC::SOFTWARE_ERROR;
            CHECK_RC_CLEANUP(rc);
        }

        if ( client.pLwRm && !client.pLwRm->IsFreed())
        {
            if (client.pLwRm->GetDeviceHandle(GetBoundGpuDevice()))
            {
                client.pLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
            }

            client.pLwRm->FreeClient();
        }
    }

 Cleanup:

    if ( client.pLwRm && !client.pLwRm->IsFreed())
    {
        if (client.pLwRm->GetDeviceHandle(GetBoundGpuDevice()))
        {
            client.pLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
        }

        client.pLwRm->FreeClient();
    }

    return rc;
}

RC SimpleMapTest::Stress()
{
    RC rc = OK;
    UINT32 startTime = Utility::GetSystemTimeInSeconds();

    _client.clearColor = 0x00000000;
    _client.drawColor  = 0x11111111;

    _clientToo.clearColor = 0x22222222;
    _clientToo.drawColor  = 0x33333333;

    _clientLonely.clearColor = 0x44444444;
    _clientLonely.drawColor  = 0x55555555;

    vector<Client *> clients;
    clients.push_back(&_client);
    clients.push_back(&_clientToo);
    clients.push_back(&_clientLonely);

    for ( vector<Client*>::iterator client_i = clients.begin();
          (rc == OK) && (client_i != clients.end());
          client_i++ )
    {

        for ( LwU32 line = 0; line < TestMemLines; line++)
        {
            bool passes;
            CHECK_RC_CLEANUP(rc);
            CHECK_RC_CLEANUP( FillTestMemLine(**client_i, _gpuRef, _vidMem, line, (*client_i)->clearColor));
            CHECK_RC_CLEANUP( WaitIdleClient((**client_i), _gpuRef, 10000/*ms*/));
            CHECK_RC_CLEANUP( CompareTestMemLine(**client_i, _cpuRef, _vidMem, line, (*client_i)->clearColor, passes) );
            rc = passes ? OK : RC::BAD_MEMORY;
        }
    }

    CHECK_RC_CLEANUP(rc);

    //XXX for (LwU32 stressTimes = 0; stressTimes <= m_StressTime; stressTimes++)
    for (LwU32 lwrTime = Utility::GetSystemTimeInSeconds();
         ( lwrTime - startTime ) < m_StressTime;
         lwrTime = Utility::GetSystemTimeInSeconds())
    {

        Printf(Tee::PriNormal, "Starting test stress iteration. ct=0x%x st=0x%x, d=0x%x < 0x%x\n",
            (UINT32) lwrTime, startTime,  (UINT32) lwrTime - startTime, m_StressTime);
        //
        // Draw 100 random lines from each channel.  Don't sync until the end so we hopefully
        // get some non-trivial ctxsw (no rigor here just chance, might want to bump up the # for getting to dma size trigger).
        //
        for ( vector<Client*>::iterator client_i = clients.begin();
              client_i != clients.end();
              client_i++ )
        {
            for ( int i = 0; i < 100; i++)
            {
                LwU32 x[2], y[2];
                x[0] = m_Random.GetRandom(0, TestMemPitch/sizeof(LwU32) - 1);
                x[1] = m_Random.GetRandom(0, TestMemPitch/sizeof(LwU32) - 1);
                y[0] = m_Random.GetRandom(0, TestMemLines-1);
                y[1] = m_Random.GetRandom(0, TestMemLines-1);

                //Printf(Tee::PriNormal, "Line:(0x%x,0x%x)-(0x%x,0x%x)\n", x[0],y[0],x[1],y[1]);
                (*client_i)->pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), x[0]);
                (*client_i)->pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), y[0]);
                (*client_i)->pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(1),x[1]);
                (*client_i)->pChannel->Write(SIMPLEMAP_SUBCH_TWOD, LW502D_RENDER_SOLID_PRIM_POINT_Y(1), y[1]);
                //CHECK_RC_CLEANUP( (*client_i)->pChannel->Flush() );
                //CHECK_RC_CLEANUP(WaitIdleClient((**client_i), _gpuRef, 10000/*ms*/));
            }
        }

        // Flush them all at the same time for grins
        for ( vector<Client*>::iterator client_i = clients.begin();
              client_i != clients.end();
              client_i++ )
        {
            CHECK_RC_CLEANUP( (*client_i)->pChannel->Flush() );
        }

        // Wait on them all before reading back.
        for ( vector<Client*>::iterator client_i = clients.begin();
              client_i != clients.end();
              client_i++ )
        {
            CHECK_RC_CLEANUP(WaitIdleClient((**client_i), _gpuRef, 10000/*ms*/));
        }

        // Specialized loop here to test pass/fail here since we know something
        // about the organization of the sharegroups.
        vector<LwU32> sgATstColors;
        vector<LwU32> sgBTstColors;

        sgATstColors.push_back(_client.clearColor);
        sgATstColors.push_back(_client.drawColor);
        sgATstColors.push_back(_clientToo.clearColor);
        sgATstColors.push_back(_clientToo.drawColor);

        sgBTstColors.push_back(_clientLonely.clearColor);
        sgBTstColors.push_back(_clientLonely.drawColor);

        for ( LwU32 line = 0; line < TestMemLines; line++ )
        {
            bool passes[2];
            CHECK_RC_CLEANUP( CompareTestMemLine(_client, _cpuRef, _vidMem, line, sgATstColors, passes[0]) );
            CHECK_RC_CLEANUP( CompareTestMemLine(_clientLonely, _cpuRef, _vidMem, line, sgBTstColors, passes[1]) );
            rc = (passes[0]&&passes[1]?OK:RC::BAD_MEMORY);
            CHECK_RC_CLEANUP(rc);
        }
    }

 Cleanup:
    return rc;
}

RC SimpleMapTest::VidHeapAllocSize
(
    UINT32   Owner,
    UINT32   hRoot,
    UINT32   hObjectParent,
    UINT32   Flags,
    UINT32   Attr,
    UINT64   Size,
    UINT64 * pLimit,
    UINT64   Alignment,
    UINT32   ComprCovg,
    UINT32 * pHandle
)
{
    RC rc;
    if (!pHandle)
    {
        MASSERT(0);
        return RC::ILWALID_ARGUMENT;
    }
    LWOS32_PARAMETERS params = {0};

    params.hRoot                    = hRoot;
    params.hObjectParent            = hObjectParent;
    params.function                 = LWOS32_FUNCTION_ALLOC_SIZE;
    params.data.AllocSize.owner     = Owner;
    params.data.AllocSize.type      = LWOS32_TYPE_IMAGE;
    params.data.AllocSize.flags     = Flags;
    params.data.AllocSize.size      = Size;
    params.data.AllocSize.alignment = Alignment;
    params.data.AllocSize.attr      = Attr;
    params.data.AllocSize.comprCovg = ComprCovg;
    params.data.AllocSize.width     = 1;
    params.data.AllocSize.height    = 1;

    params.data.AllocSize.hMemory   = *pHandle;
    if (pLimit)
        params.data.AllocSize.limit = *pLimit;
    params.data.AllocSize.flags     = !params.data.AllocSize.hMemory ? Flags :
                                        Flags | LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;

    rc = RmApiStatusToModsRC(LwRmVidHeapControl(&params));
    rc == OK ? *pHandle =  params.data.AllocSize.hMemory : 0;
    if (pLimit)
        *pLimit = params.data.AllocSize.limit;

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SimpleMapTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SimpleMapTest, RmTest, "Simple memory mapping test.");
CLASS_PROP_READWRITE(SimpleMapTest, StressTime,  UINT32,
    "Length of time (seconds) the stress phase runs" );

