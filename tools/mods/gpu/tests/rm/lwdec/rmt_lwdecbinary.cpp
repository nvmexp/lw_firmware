/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RM test to run LWDEC uCode binary tests
//
//
//!
//! \file rmt_lwdecbinary.cpp
//! \brief RM test to run LWDEC uCode binary tests.

//!
//! This file runs LWDEC uCode test binaries. These are hardware specific
//! tests used for hardware validation.The test allocates an object of
//! the class, and tries to write a value to a memory and read it back
//! to make sure it went through. Afterwards, it will update a test spec
//! via a system memory allocation. uCode tests are triggered by ilwoking
//! a testing task. Also it provides a scratch local memory for uCode
//! testing task to use.
//!

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cla0b0.h" // LWA0B0_VIDEO_DECODER
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "class/clb6b0.h" // LWB6B0_VIDEO_DECODER
#include "class/clb8b0.h" // LWB8B0_VIDEO_DECODER
#include "class/clc1b0.h" // LWC1B0_VIDEO_DECODER
#include "class/clc2b0.h" // LWC2B0_VIDEO_DECODER
#include "class/clc3b0.h" // LWC3B0_VIDEO_DECODER
#include "class/clc4b0.h" // LWC4B0_VIDEO_DECODER
#include "class/clc6b0.h" // LWC6B0_VIDEO_DECODER
#include "class/clc7b0.h" // LWC7B0_VIDEO_DECODER
#include "class/clc9b0.h" // LWC9B0_VIDEO_DECODER

#include "pascal/gp104/dev_lwdec_pri.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "ucodetestMessageCommon.h"
#include "ucodetestMessage.h"
#include "core/include/memcheck.h"
                          // at DEBUG_TRACE_LEVEL 1

//
// Some local defines
//
#define LWDEC_BINARY__SEM_WR_VAL       0x12345678
#define LWDEC_BINARY__SEM_WR_VAL_INIT  0x87654321
#define LWDEC_BINARY__CTX_DMA_SIZE     0x1000
#define LWDEC_BINARY__SCRATCH_FB_SIZE  0x4000
#define LWDEC_BINARY__SCRATCH_SYS_SIZE 0x4000
#define LWDEC_BINARY__PUSHBUFFER_SIZE  0X2000

#define LWDEC_BINARY__MESSAGE_QUEUE_SIZE  sizeof(MESSAGE_QUEUE)

//
// Test specific defines
//
#define WRITE_READ_TEST__DATA_SIZE     64

//
// Defines related to templates for setup re-usable pushbuffer templates
// for diffferent engines so that we don't need to write test functions.
//

// Macros used for writing templates
#define METHOD_BEGIN {m_pushbufferUsed = 0;}
#define METHOD(name, value)                                                         \
do {                                                                                \
    if ( (m_pushbufferUsed + 1) < LWDEC_BINARY__PUSHBUFFER_SIZE ) {                 \
        m_pushbuffer[m_pushbufferUsed] = (name);                                    \
        m_pushbuffer[m_pushbufferUsed + 1] = (value);                               \
        m_pushbufferUsed += 2;                                                      \
    } else {                                                                        \
        Printf(Tee::PriHigh, "Pushbuffer overflow!!!\n");                           \
        CHECK_RC(RC::SOFTWARE_ERROR);                                               \
    }                                                                               \
} while(0)
#define METHOD_END

//
// Macros used to fill the pushbuffer with template, i.e. templ, for engine id
//
#define FILL_PUSHBUF(id, templ) do { TEMPLATE__##templ (id); } while(0)

//
// Pushbuffer template used for checking if engines are alive.
//
#define TEMPLATE__IS_ALIVE(id)                                                      \
do {                                                                                \
    METHOD_BEGIN                                                                    \
    METHOD((LW##id##_SEMAPHORE_A),                                                  \
           (DRF_NUM(id, _SEMAPHORE_A, _UPPER, (UINT32)(m_ctxDmaGpuAddr >> 32LL)))); \
    METHOD((LW##id##_SEMAPHORE_B),                                                  \
           ((UINT32)(m_ctxDmaGpuAddr & 0xffffffffLL)));                             \
    METHOD((LW##id##_SEMAPHORE_C),                                                  \
           (DRF_NUM(id, _SEMAPHORE_C, _PAYLOAD, LWDEC_BINARY__SEM_WR_VAL)));        \
    METHOD((LW##id##_SEMAPHORE_D),                                                  \
           ((DRF_DEF(id, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |                     \
             DRF_DEF(id, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE))));                  \
    METHOD_END                                                                      \
} while(0)

//
// Pushbuffer template used for run test uCode.
//
// Note:  Hardcode the test application ID to be 0xFF for now.
//
#define TEMPLATE__RUN_TEST_UCODE(id)                                                \
do {                                                                                \
    METHOD_BEGIN                                                                    \
    METHOD((LW##id##_SET_APPLICATION_ID), (0xFF));                                  \
    METHOD((LW##id##_SET_COLOC_DATA_OFFSET),                                        \
           ((UINT32)(m_scratchFBMemGpuAddr)));                                      \
    METHOD((LW##id##_SET_HISTORY_OFFSET),                                           \
           ((UINT32)(m_scratchSysMemGpuAddr)));                                     \
    METHOD((LW##id##_SET_IN_BUF_BASE_OFFSET),                                       \
           ((UINT32)(m_messageQueueFBMemGpuAddr)));                                            \
    METHOD((LW##id##_EXELWTE),                                                      \
           (DRF_DEF(id, _EXELWTE, _NOTIFY, _DISABLE)));                             \
    METHOD_END                                                                      \
} while(0)

//------------------------------------------------------------------------------
//
// Main test class defines.
//
//------------------------------------------------------------------------------
class LwdecBinaryTest: public RmTest
{

public:
    LwdecBinaryTest();
    virtual ~LwdecBinaryTest();

private:
    // Private helpers
    void ConfigTest();
    RC   AllocCtxDma();
    void DeallocCtxDma();
    RC   MapCtxDma();
    void UnmapCtxDma();
    RC   AllocScratch();
    void FreeScratch();
    RC   FlushPushbuffer( UINT32 & );
    RC   IsEngineAlive();
    RC   SendMessage( MESSAGE_QUEUE_PTR pMessageQueue );
    RC   Run_Test_uCode();

public:
    // Test interface functions
    virtual string IsTestSupported();
    virtual RC     Setup();
    virtual RC     Run();
    virtual RC     Cleanup();

private:
    // Class Allocator Handles
    LwRm::Handle      m_hObj;
    GrClassAllocator *m_classAllocator;

    // Context DMA related
    LwRm::Handle m_hVA;                     // Mappable virtual object
    LwRm::Handle m_hCtxDmaMem;              // FB memory of context DMA allocation
    UINT64       m_ctxDmaGpuAddr;           // FB local address of context DMA allocation
    UINT32      *m_ctxDmaCpuAddr;           // Mapped CPU address of context DMA allocation

    // Message Queue in local FB memory
    Surface2D   *m_hMessageQueueFBMem;           // Handle for FB local memory
    UINT64       m_messageQueueFBMemGpuAddr;     // FB local address of FB local memory
    UINT32      *m_messageQueueFBMemCpuAddr;     // Mapped CPU address of FB local memory

    // Scratch local FB memory
    Surface2D   *m_hScratchFBMem;           // Handle for FB local memory
    UINT64       m_scratchFBMemGpuAddr;     // FB local address of FB local memory
    UINT32      *m_scratchFBMemCpuAddr;     // Mapped CPU address of FB local memory

    // Scratch system memory
    Surface2D   *m_hScratchSysMem;          // Handle for FB local memory
    UINT64       m_scratchSysMemGpuAddr;    // FB local address of FB local memory
    UINT32      *m_scratchSysMemCpuAddr;    // Mapped CPU address of FB local memory

    // Test configuration parameters
    FLOAT64      m_TimeoutMs;               // Configured timeout value

    // Subchannel number used for send pushbuffer
    UINT32       m_subch;

    // Define pushbuffer used for send method to engines
    UINT32       m_pushbuffer[LWDEC_BINARY__PUSHBUFFER_SIZE];
    UINT32       m_pushbufferUsed;

};

//------------------------------------------------------------------------------
//! \brief LwdecBinaryTest constructor.
//!
//------------------------------------------------------------------------------
LwdecBinaryTest::LwdecBinaryTest()
{
    m_hObj = 0;

    m_hVA = 0;
    m_hCtxDmaMem = 0;
    m_ctxDmaGpuAddr = 0;
    m_ctxDmaCpuAddr = NULL;

    m_hMessageQueueFBMem = NULL;
    m_messageQueueFBMemGpuAddr = 0;
    m_messageQueueFBMemCpuAddr = 0;

    m_hScratchFBMem = NULL;
    m_scratchFBMemGpuAddr = 0;
    m_scratchFBMemCpuAddr = 0;

    m_hScratchSysMem = NULL;
    m_scratchSysMemGpuAddr = 0;
    m_scratchSysMemCpuAddr = 0;

    m_TimeoutMs = Tasker::NO_TIMEOUT;

    m_subch = 0;

    memset(&(m_pushbuffer[0]), 0, LWDEC_BINARY__PUSHBUFFER_SIZE);
    m_pushbufferUsed = 0;

    m_classAllocator = new LwdecAlloc();

    SetName("LwdecBinaryTest");
}

//------------------------------------------------------------------------------
//! \brief LwdecBinaryTest destructor.
//!
//------------------------------------------------------------------------------
LwdecBinaryTest::~LwdecBinaryTest()
{
    delete m_classAllocator;
}

//------------------------------------------------------------------------------
//! \brief Make sure the class we're testing is supported by RM.
//!
//! \return True
//------------------------------------------------------------------------------
string LwdecBinaryTest::IsTestSupported()
{
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    if (m_classAllocator->IsSupported(pGpuDev))
    {
        Printf(Tee::PriHigh,
               "Test is supported on the current GPU/platform, preparing to run...\n");
    }
    else
    {
        switch(m_classAllocator->GetClass())
        {
            case LWA0B0_VIDEO_DECODER:
                return "Class LWA0B0_VIDEO_DECODER Not supported on GPU";
            case LWB0B0_VIDEO_DECODER:
                return "Class LWB0B0_VIDEO_DECODER Not supported on GPU";
            case LWB6B0_VIDEO_DECODER:
                return "Class LWB6B0_VIDEO_DECODER Not supported on GPU";
            case LWB8B0_VIDEO_DECODER:
                return "Class LWB8B0_VIDEO_DECODER Not supported on GPU";
            case LWC1B0_VIDEO_DECODER:
                return "Class LWC1B0_VIDEO_DECODER Not supported on GPU";
            case LWC2B0_VIDEO_DECODER:
                return "Class LWC2B0_VIDEO_DECODER Not supported on GPU";
            case LWC3B0_VIDEO_DECODER:
                return "Class LWC3B0_VIDEO_DECODER Not supported on GPU";
            case LWC4B0_VIDEO_DECODER:
                return "Class LWC4B0_VIDEO_DECODER Not supported on GPU";
            case LWC6B0_VIDEO_DECODER:
                return "Class LWC6B0_VIDEO_DECODER Not supported on GPU";
            case LWC7B0_VIDEO_DECODER:
                return "Class LWC7B0_VIDEO_DECODER Not supported on GPU";
            case LWC9B0_VIDEO_DECODER:
                return "Class LWC9B0_VIDEO_DECODER Not supported on GPU";

        }
        return "Invalid Class Specified";
    }

    return RUN_RMTEST_TRUE;
}

//------------------------------------------------------------------------------
//! \brief Setup test configurations
//!
//------------------------------------------------------------------------------
void LwdecBinaryTest::ConfigTest()
{
    m_TimeoutMs = m_TestConfig.TimeoutMs();
}

//------------------------------------------------------------------------------
//! \brief Setup context DMA allocation
//!
//------------------------------------------------------------------------------
RC LwdecBinaryTest::AllocCtxDma()
{
    LwRmPtr pLwRm;

    RC      rc = OK;

    CHECK_RC( pLwRm->AllocSystemMemory( &m_hCtxDmaMem,
                                        DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
                                        DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                                        DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
                                        LWDEC_BINARY__CTX_DMA_SIZE,
                                        GetBoundGpuDevice()) );

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC( pLwRm->Alloc(            pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                       &m_hVA,
                                       LW01_MEMORY_VIRTUAL,
                                       &vmparams));

    return rc;
}

RC LwdecBinaryTest::MapCtxDma()
{

    RC      rc = OK;
    LwRmPtr pLwRm;

    CHECK_RC( pLwRm->MapMemoryDma(      m_hVA,
                                        m_hCtxDmaMem,
                                        0,
                                        LWDEC_BINARY__CTX_DMA_SIZE,
                                        LW04_MAP_MEMORY_FLAGS_NONE,
                                        &m_ctxDmaGpuAddr,
                                        GetBoundGpuDevice()) );

    CHECK_RC( pLwRm->MapMemory(         m_hCtxDmaMem,
                                        0,
                                        LWDEC_BINARY__CTX_DMA_SIZE,
                                        (void **)&m_ctxDmaCpuAddr,
                                        0,
                                        GetBoundGpuSubdevice()) );

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free any resources that this test used for context DMA
//!
//! \return void
//------------------------------------------------------------------------------
void LwdecBinaryTest::DeallocCtxDma()
{
    LwRmPtr pLwRm;

    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hCtxDmaMem);

}

void LwdecBinaryTest::UnmapCtxDma()
{
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(    m_hCtxDmaMem,
                           m_ctxDmaCpuAddr,
                           0,
                           GetBoundGpuSubdevice() );

    pLwRm->UnmapMemoryDma( m_hVA,
                           m_hCtxDmaMem,
                           LW04_MAP_MEMORY_FLAGS_NONE,
                           m_ctxDmaGpuAddr,
                           GetBoundGpuDevice() );

}

//------------------------------------------------------------------------------
//! \brief Allocate scratch memories
//!
//! \return RC
//------------------------------------------------------------------------------
RC LwdecBinaryTest::AllocScratch()
{
    RC           rc = OK;
    LwRm        *pLwRm = m_TestConfig.GetRmClient();
    GpuDevice   *pGpuDev = GetBoundGpuDevice();

    m_hMessageQueueFBMem = new Surface2D();
    m_hMessageQueueFBMem->SetLayout(Surface2D::Pitch);
    m_hMessageQueueFBMem->SetPitch(LWDEC_BINARY__MESSAGE_QUEUE_SIZE);
    m_hMessageQueueFBMem->SetColorFormat(ColorUtils::VOID32);
    m_hMessageQueueFBMem->SetWidth(LWDEC_BINARY__MESSAGE_QUEUE_SIZE);
    m_hMessageQueueFBMem->SetHeight(1);
    m_hMessageQueueFBMem->SetLocation(Memory::Fb);
    CHECK_RC(m_hMessageQueueFBMem->Alloc(pGpuDev, pLwRm));
    CHECK_RC(m_hMessageQueueFBMem->Map());
    m_hMessageQueueFBMem->Fill(0);
    m_messageQueueFBMemGpuAddr = m_hMessageQueueFBMem->GetCtxDmaOffsetGpu();
    m_messageQueueFBMemCpuAddr = (UINT32 *)m_hMessageQueueFBMem->GetAddress();

    m_hScratchSysMem = new Surface2D();
    m_hScratchSysMem->SetLayout(Surface2D::Pitch);
    m_hScratchFBMem = new Surface2D();
    m_hScratchFBMem->SetLayout(Surface2D::Pitch);
    m_hScratchFBMem->SetPitch(LWDEC_BINARY__SCRATCH_FB_SIZE);
    m_hScratchFBMem->SetColorFormat(ColorUtils::VOID32);
    m_hScratchFBMem->SetWidth(LWDEC_BINARY__SCRATCH_FB_SIZE);
    m_hScratchFBMem->SetHeight(1);
    m_hScratchFBMem->SetLocation(Memory::Fb);
    CHECK_RC(m_hScratchFBMem->Alloc(pGpuDev, pLwRm));
    CHECK_RC(m_hScratchFBMem->Map());
    m_hScratchFBMem->Fill(0);
    m_scratchFBMemGpuAddr = m_hScratchFBMem->GetCtxDmaOffsetGpu();
    m_scratchFBMemCpuAddr = (UINT32 *)m_hScratchFBMem->GetAddress();

    m_hScratchSysMem = new Surface2D();
    m_hScratchSysMem->SetLayout(Surface2D::Pitch);
    m_hScratchSysMem->SetPitch(LWDEC_BINARY__SCRATCH_SYS_SIZE);
    m_hScratchSysMem->SetColorFormat(ColorUtils::VOID32);
    m_hScratchSysMem->SetWidth(LWDEC_BINARY__SCRATCH_SYS_SIZE);
    m_hScratchSysMem->SetHeight(1);
    m_hScratchSysMem->SetLocation(Memory::Coherent);
    CHECK_RC(m_hScratchSysMem->Alloc(pGpuDev, pLwRm));
    CHECK_RC(m_hScratchSysMem->Map());
    m_hScratchSysMem->Fill(0);
    m_scratchSysMemGpuAddr = m_hScratchSysMem->GetCtxDmaOffsetGpu();
    m_scratchSysMemCpuAddr = (UINT32 *)m_hScratchSysMem->GetAddress();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free scratch buffers
//!
//! \return void
//!
//------------------------------------------------------------------------------
void LwdecBinaryTest::FreeScratch()
{
    if (m_hMessageQueueFBMem)
    {
        m_hMessageQueueFBMem->Free();
        delete m_hMessageQueueFBMem;
        m_hMessageQueueFBMem = NULL;
    }

    if (m_hScratchFBMem)
    {
        m_hScratchFBMem->Free();
        delete m_hScratchFBMem;
        m_hScratchFBMem = NULL;
    }

    if (m_hScratchSysMem)
    {
        m_hScratchSysMem->Free();
        delete m_hScratchSysMem;
        m_hScratchSysMem = NULL;
    }
}

//------------------------------------------------------------------------------
//! \brief Flush content of the pushbuffer that already filled with methods.
//! \inputs subch - sub-channel id
//!
//! \return RC
//------------------------------------------------------------------------------
RC LwdecBinaryTest::FlushPushbuffer( UINT32 &subch )
{
    RC rc = OK;
    UINT32 i = 0;

    for ( i = 0; i < m_pushbufferUsed; i += 2 )
    {
        CHECK_RC(m_pCh->Write(subch, m_pushbuffer[i], m_pushbuffer[i+1]));
    }
    CHECK_RC(m_pCh->Flush());

    return rc;
}

//------------------------------------------------------------------------------
//!
//! \brief Setup all necessary state before running the test.
//!
//! \return RC code
//------------------------------------------------------------------------------
RC LwdecBinaryTest::Setup()
{
    RC           rc = OK;
    LwRmPtr      pLwRm;
    GpuDevice   *pGpuDev = GetBoundGpuDevice();

    CHECK_RC(InitFromJs());

    ConfigTest();

    CHECK_RC(AllocCtxDma());

    CHECK_RC(AllocScratch());

    // Allocate channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));

    CHECK_RC(MapCtxDma());

    CHECK_RC(m_classAllocator->Alloc(m_hCh, pGpuDev));
    m_hObj = m_classAllocator->GetHandle();

    // Binding subchannel
    m_pCh->SetObject(m_subch, m_hObj);

    return rc;
}

//
// Struct used for Poll()
//
struct PollArgs
{
    UINT32 *pollCpuAddr;
    UINT32 value;
};

//-----------------------------------------------------------------------------
//! \brief Poll Function. Explicit function for polling
//!
//! Designed so as to remove the need for waitidle
//-----------------------------------------------------------------------------
static bool Poll(void *pArgs)
{
    UINT32 val;
    PollArgs Args = *(PollArgs *)pArgs;

    val = MEM_RD32(Args.pollCpuAddr);

    if (Args.value == val)
    {
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
//! \brief IsEngineAlive test by writing different value to the same memory
//!        through GPU mapping.
//!
//! \return RC
//-----------------------------------------------------------------------------
RC LwdecBinaryTest::IsEngineAlive()
{
    RC rc = OK;
    UINT32 semVal;
    PollArgs args;

    //
    // Write a value to memory through CPU mapping
    //
    MEM_WR32(m_ctxDmaCpuAddr, LWDEC_BINARY__SEM_WR_VAL_INIT);

    //
    // Fill in methods into pushbuffer
    //
    switch(m_classAllocator->GetClass())
    {
        case LWA0B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(A0B0, IS_ALIVE);
        }
        break;

        case LWB0B0_VIDEO_DECODER:
        case LWB6B0_VIDEO_DECODER:
        case LWB8B0_VIDEO_DECODER:
        case LWC1B0_VIDEO_DECODER:
        case LWC2B0_VIDEO_DECODER:
        case LWC3B0_VIDEO_DECODER:
        case LWC4B0_VIDEO_DECODER:
        case LWC6B0_VIDEO_DECODER:
        case LWC7B0_VIDEO_DECODER:
        case LWC9B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(B0B0, IS_ALIVE);
        }
        break;

        default:
        {
            Printf(Tee::PriHigh,
                   "Class 0x%x not supported.. skipping the  Semaphore Exelwtion\n",
                   m_classAllocator->GetClass());

            return RC::LWRM_ILWALID_CLASS;
        }
    }

    //
    // Flush the pushbuffer to hardware
    //
    CHECK_RC(FlushPushbuffer(m_subch));

    args.value = LWDEC_BINARY__SEM_WR_VAL;
    args.pollCpuAddr = m_ctxDmaCpuAddr;
    rc = POLLWRAP(&Poll, &args, m_TimeoutMs);

    //
    // Read a value from memory through CPU mapping
    //
    semVal = MEM_RD32(m_ctxDmaCpuAddr);

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
              "LwdecBinaryTest: GOT: 0x%x\n", semVal);
        Printf(Tee::PriHigh,
              "                EXPECTED: 0x%x\n", 0x12345678);
        return rc;
    }
    else
    {
        Printf(Tee::PriHigh,
              "LwdecBinaryTest: Read/Write matched as expected.\n");
    }

    return rc;
}

RC LwdecBinaryTest::SendMessage(MESSAGE_QUEUE_PTR pMessageQueue)
{
    unsigned int i = 0;
    RC rc = OK;
    UINT08 *pData = (UINT08 *)pMessageQueue;
    static int messageSequenceNo = 0;

    pMessageQueue->message.header.seq   = messageSequenceNo++;
    pMessageQueue->context.header.state = MESSAGE_STATE__NEW;
    pMessageQueue->context.header.seq   = pMessageQueue->message.header.seq;

    //
    // Update message queue in FB
    //
    for (i = 0; i < sizeof(MESSAGE_QUEUE); i++)
    {
        MEM_WR08((((UINT08 *)m_messageQueueFBMemCpuAddr) + i), *(pData + i));
    }
    //
    // Flush the pushbuffer to hardware
    //
    CHECK_RC(FlushPushbuffer(m_subch));

    //
    // Wait test to finish
    //
    CHECK_RC(m_pCh->WaitIdle(m_TimeoutMs));

    //
    // Update message queue in memroy
    //
    for (i = 0; i < sizeof(MESSAGE_QUEUE); i++)
    {
        *(pData + i) = MEM_RD08(((UINT08 *)m_messageQueueFBMemCpuAddr) + i);
    }

    //
    // Check if result is OK
    //
    if ((pMessageQueue->context.header.state != MESSAGE_STATE__FINISHED) &&
        (pMessageQueue->context.header.id    != pMessageQueue->message.header.id) &&
        (pMessageQueue->context.header.seq   != pMessageQueue->message.header.seq))
    {

        rc = RC::SOFTWARE_ERROR;
    }

    //
    // Ilwalidate the message queue
    //
    pMessageQueue->context.header.state = MESSAGE_STATE__ILWALID;

    //
    // Update message queue in FB
    //
    for (i = 0; i < sizeof(MESSAGE_QUEUE); i++)
    {
        MEM_WR08((((UINT08 *)m_messageQueueFBMemCpuAddr) + i), *(pData + i));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run test uCode to check out specific engine functions
//!
//! \return RC
//------------------------------------------------------------------------------
RC LwdecBinaryTest::Run_Test_uCode()
{
    int i = 0;
    RC rc = OK;

    // Mapping the message queue structure
    MESSAGE_QUEUE          messageQueue;

    MESSAGE_INIT_PTR pMessageInit            = (MESSAGE_INIT_PTR)(&(messageQueue.message));
    MESSAGE_READ_DMEM_PTR  pMessageReadDmem  = (MESSAGE_READ_DMEM_PTR)(&(messageQueue.message));
    MESSAGE_WRITE_DMEM_PTR pMessageWriteDmem = (MESSAGE_WRITE_DMEM_PTR)(&(messageQueue.message));

    MESSAGE_CONTEXT_WRITE_DMEM_PTR pMessageContextWriteDmem = (MESSAGE_CONTEXT_WRITE_DMEM_PTR)(&(messageQueue.context));
    MESSAGE_CONTEXT_READ_DMEM_PTR  pMessageContextReadDmem  = (MESSAGE_CONTEXT_READ_DMEM_PTR)(&(messageQueue.context));

    //
    // Binding scratch memory to the channel.
    //
    m_hScratchFBMem->BindGpuChannel(m_hCh);
    m_hScratchSysMem->BindGpuChannel(m_hCh);

    //
    // Fill in methods into pushbuffer
    //
    switch(m_classAllocator->GetClass())
    {
        case LWA0B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(A0B0, RUN_TEST_UCODE);
        }
        break;

        case LWB0B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(B0B0, RUN_TEST_UCODE);
        }
        break;

        case LWB6B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(B6B0, RUN_TEST_UCODE);
        }
        break;

        case LWB8B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(B8B0, RUN_TEST_UCODE);
        }
        break;

        case LWC1B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C1B0, RUN_TEST_UCODE);
        }
        break;

        case LWC2B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C2B0, RUN_TEST_UCODE);
        }
        break;

        case LWC3B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C3B0, RUN_TEST_UCODE);
        }
        break;

        case LWC4B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C4B0, RUN_TEST_UCODE);
        }
        break;

        case LWC6B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C6B0, RUN_TEST_UCODE);
        }
        break;

        case LWC7B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C7B0, RUN_TEST_UCODE);
        }
        break;

        case LWC9B0_VIDEO_DECODER:
        {
            FILL_PUSHBUF(C9B0, RUN_TEST_UCODE);
        }
        break;

        default:
        {
            Printf(Tee::PriHigh,
                   "Class 0x%x not supported.. skipping run test uCode!\n",
                   m_classAllocator->GetClass());

            return RC::LWRM_ILWALID_CLASS;
        }
    }

    //
    // Fill scratch memory in system and FB local memory
    //
    for(i = 0; i < WRITE_READ_TEST__DATA_SIZE; i++)
    {
        MEM_WR32((m_scratchSysMemCpuAddr+i), i);
        MEM_WR32((m_scratchFBMemCpuAddr+i), 0);
    }

    //
    // Make sure data are coherent for system memory
    //
    m_hScratchSysMem->FlushCpuCache(Surface2D::FlushAndIlwalidate);

    //
    // Initialize message queue with INIT message
    //
    pMessageInit->header.id   = MESSAGE_ID__INIT;
    pMessageInit->header.size = sizeof(MESSAGE_INIT);

    messageQueue.context.header.id   = messageQueue.message.header.id;
    messageQueue.context.header.size = sizeof(MESSAGE_CONTEXT_INIT);

    CHECK_RC(SendMessage(&messageQueue));

    Printf(Tee::PriHigh,
           "LwdecBinaryTest: INIT (scratchPadDmemAddr = %d)!\n",
           messageQueue.messageQueueInfo.scratchPadDmemAddr);

    //
    // Fill target dmem with write message
    //
    pMessageWriteDmem->header.id   = MESSAGE_ID__WRITE_DMEM;
    pMessageWriteDmem->header.size = sizeof(MESSAGE_WRITE_DMEM);
    pMessageWriteDmem->dmemAddr    = messageQueue.messageQueueInfo.scratchPadDmemAddr;
    pMessageWriteDmem->size        = sizeof(UINT32)*WRITE_READ_TEST__DATA_SIZE;

    messageQueue.context.header.id   = messageQueue.message.header.id;
    messageQueue.context.header.size = sizeof(MESSAGE_CONTEXT_WRITE_DMEM);

    CHECK_RC(SendMessage(&messageQueue));

    //
    // Check the output size if OK!
    //
    if ( pMessageContextWriteDmem->sizeOut != (WRITE_READ_TEST__DATA_SIZE * sizeof(UINT32)) )
    {
        Printf(Tee::PriHigh,
               "LwdecBinaryTest: WRITE_DMEM failed (sizeOut = %d)!\n",
               pMessageContextWriteDmem->sizeOut);

        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    //
    // Read dmem to scratch surface
    //
    pMessageReadDmem->header.id   = MESSAGE_ID__READ_DMEM;
    pMessageReadDmem->header.size = sizeof(MESSAGE_READ_DMEM);
    pMessageReadDmem->dmemAddr    = messageQueue.messageQueueInfo.scratchPadDmemAddr;
    pMessageReadDmem->size        = sizeof(UINT32)*WRITE_READ_TEST__DATA_SIZE;

    messageQueue.context.header.id   = messageQueue.message.header.id;
    messageQueue.context.header.size = sizeof(MESSAGE_CONTEXT_READ_DMEM);

    CHECK_RC(SendMessage(&messageQueue));

    //
    // Check the output size if OK!
    //
    if ( pMessageContextReadDmem->sizeIn != (WRITE_READ_TEST__DATA_SIZE * sizeof(UINT32)) )
    {
        Printf(Tee::PriHigh,
               "LwdecBinaryTest: READ_DMEM failed (sizeIn = %d)!\n",
               pMessageContextReadDmem->sizeIn);

        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    //
    // Make sure data are coherent before checking result
    //
    m_hScratchSysMem->FlushCpuCache(Surface2D::FlushAndIlwalidate);

    //
    // Check scratch memory in FB memory
    //
    for(i = 0; i < WRITE_READ_TEST__DATA_SIZE; i++)
    {
        UINT32 dataSysMem = 0;
        UINT32 dataFbMem  = 0;

        dataSysMem = MEM_RD32(m_scratchSysMemCpuAddr+i);
        dataFbMem  = MEM_RD32(m_scratchFBMemCpuAddr+i);
        if (dataSysMem !=  dataFbMem)
        {
               Printf(Tee::PriHigh,
                      "LwdecBinaryTest: READ_DMEM failed (Data mismatch at %d, dataIn=0x%x, dataOut=0x%x)!\n",
                      i, dataSysMem, dataFbMem);

            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    // Run SE test which only GP100 and above have SE engine.
    if (( m_classAllocator->GetClass() == LWB8B0_VIDEO_DECODER ) ||
    	( m_classAllocator->GetClass() == LWC1B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC2B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC3B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC4B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC6B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC7B0_VIDEO_DECODER ) ||
        ( m_classAllocator->GetClass() == LWC9B0_VIDEO_DECODER ))
    {
        MESSAGE_SE_TEST_PTR         pMessageSeTest        = (MESSAGE_SE_TEST_PTR)(&(messageQueue.message));
        MESSAGE_CONTEXT_SE_TEST_PTR pMessageContextSeTest = (MESSAGE_CONTEXT_SE_TEST_PTR)(&(messageQueue.context));

        pMessageSeTest->header.id   = MESSAGE_ID__SE_TEST;
        pMessageSeTest->header.size = sizeof(MESSAGE_SE_TEST);
        pMessageSeTest->id          = 0;
        messageQueue.context.header.id   = messageQueue.message.header.id;
        messageQueue.context.header.size = sizeof(MESSAGE_CONTEXT_SE_TEST);

        CHECK_RC(SendMessage(&messageQueue));
        if ( pMessageContextSeTest->rc != 0 )
        {
            Printf(Tee::PriHigh,
                   "LwdecBinaryTest: SE_TEST failed (rc=0x%x)!\n",
                   pMessageContextSeTest->rc);

            rc = RC::SOFTWARE_ERROR;
            return rc;
        }
    }

    return rc;

}

//------------------------------------------------------------------------------
//! \brief Run test by writing to a memory and reading it back to confirm.
//!
//! \return OK if the test passed, test-specific RC if it failed
//------------------------------------------------------------------------------
RC LwdecBinaryTest::Run()
{
    RC     rc = OK;

    //
    // Check if engine is alive
    //
    CHECK_RC(IsEngineAlive());

    //
    // Looks that the engine is alive. Trigger uCode binary test.
    //
    CHECK_RC(Run_Test_uCode());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Free any resources that this test selwred
//!
//! \return RC
//------------------------------------------------------------------------------
RC LwdecBinaryTest::Cleanup()
{
    LwRmPtr pLwRm;

    UnmapCtxDma();

    if (m_hObj)
    {
        pLwRm->Free(m_hObj);
    }

    m_TestConfig.FreeChannel(m_pCh);

    DeallocCtxDma();

    FreeScratch();

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ LwdecBinaryTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdecBinaryTest, RmTest, "LWDEC BINARY_RM test.");
