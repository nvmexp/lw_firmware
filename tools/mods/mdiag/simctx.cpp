/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <map>
#include "testapicontext.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/modsdrv.h"
#include "hwref/g00x/g000/armiscreg.h"
#include "hwref/hopper/gh100/address_map_new.h"
#include "mdiag/sysspec.h"

extern UINT64 GetSysmemSizeForPAddr(PHYSADDR addr);

static TestApi::TestApiCtx *s_pTestApiCtxObj = nullptr;
static map<UINT64, void*> s_physAddrDescMap;

static void TestApiPrint(const char* msg ...)
{
    DebugPrintf("%s: %s\n", __FUNCTION__, msg);
}

static void TestApiExit(int code)
{
    DebugPrintf("%s\n", __FUNCTION__);
    exit(-1);
}

static void TestApiEnterCritical()
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::DisableInterrupts();
}

static void TestApiExitCritical()
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::EnableInterrupts();
}

template <typename T>
struct MemOpFuncPtr
{
    using PhysRdFunc = T (*)(UINT64);
};

template <typename T>
static T TestApiMemRd(UINT64 address, typename MemOpFuncPtr<T>::PhysRdFunc PhysRd)
{
    T value = PhysRd(address);

    if (Tasker::CanThreadYield())
    {
        Tasker::Yield();
    }

    return value;
}

static uint8_t TestApiMemRd08(UINT64 adr) 
{
    return TestApiMemRd<UINT08>(adr, Platform::PhysRd08);
}

static void TestApiMemWr08(UINT64 adr, uint8_t data) 
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysWr08(adr, data);
}

static uint16_t TestApiMemRd16(UINT64 adr) 
{
    return TestApiMemRd<UINT16>(adr, Platform::PhysRd16);
}

static void TestApiMemWr16(UINT64 adr, uint16_t data)
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysWr16(adr, data);
}

static UINT32 TestApiMemRd32(UINT64 adr) 
{
    return TestApiMemRd<UINT32>(adr, Platform::PhysRd32);
}

static void TestApiMemWr32(UINT64 adr, UINT32 data)
{
    DebugPrintf("%s: address: 0x%llx, data:0x%x\n", __FUNCTION__, adr, data);
    Platform::PhysWr32(adr, data);
}

static UINT64 TestApiMemRd64(UINT64 adr)
{
    return TestApiMemRd<UINT64>(adr, Platform::PhysRd64);
}

static void TestApiMemWr64(UINT64 adr, UINT64 data)
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysWr64(adr, data);
}

static void TestApiMemReadBlock(UINT64 adr, uint8_t *ptr, UINT32 len)
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysRd(adr, ptr, len);
}

static void TestApiMemWriteBlock(UINT64 adr, const uint8_t *data, UINT32 len) 
{
    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysWr(adr, data, len);
}

static void TestApiMemReadBurst(UINT64 adr, uint8_t *ptr, uint8_t type, UINT32 len, UINT32 size)
{
    if (type != 0)
    {
        MASSERT(!"TestApiMemReadBurst lwrrently can only support type 0-FIXED");
    }

    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysRd(adr, ptr, len*size);
}

static void TestApiMemWriteBurst(UINT64 adr, const uint8_t *data, uint8_t type, UINT32 len, UINT32 size)
{

    if (type != 0)
    {
        MASSERT(!"TestApiMemWriteBurst lwrrently can only support type 0-FIXED");
    }

    DebugPrintf("%s\n", __FUNCTION__);
    Platform::PhysWr(adr, data, len*size);
}

//Register read write functions with colwersion to backdoor mem transactions in case of emem access
static uint8_t TestApiRegRd08(uintptr_t adr) 
{
    DebugPrintf("%s\n", __FUNCTION__);
    return TestApiMemRd08(adr);
}

static void TestApiRegWr08(uintptr_t adr, uint8_t data) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
    TestApiMemWr08(adr, data);
}

static uint16_t TestApiRegRd16(uintptr_t adr) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return TestApiMemRd16(adr);
}

static void TestApiRegWr16(uintptr_t adr, uint16_t data) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
    TestApiMemWr16(adr, data);
}

static UINT32 TestApiRegRd32(uintptr_t adr) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
     return TestApiMemRd32(adr);
}

static void TestApiRegWr32(uintptr_t adr, UINT32 data) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
    TestApiMemWr32(adr, data);
}

static int TestApiGetGPVarStr( const char *gpstr, char *retstr ) 
{
    DebugPrintf("Warning: unimplemented function %s\n", __FUNCTION__ );
    return RT_ERROR; 
}

static int TestApiSetGPVarStr( const char *gpstr, char *retstr ) 
{
    DebugPrintf("Warning: unimplemented function %s\n", __FUNCTION__ );
    return RT_ERROR;
}

static int TestApiGetGPVarDouble( const char *gpstr, double *retval ) 
{
    DebugPrintf("Warning: unimplemented function %s\n", __FUNCTION__ );
    return RT_ERROR; 
}

static int TestApiSetGPVarDouble( const char *gpstr, double setval ) 
{
    DebugPrintf("Warning: unimplemented function %s\n", __FUNCTION__ );
    return RT_ERROR; 
}

//synchronized read throught xhost
static RT_RC TestApiXhostRdSync32(UINT32 *value_ptr)
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return RT_OK;
}


//synchronized write throught xhost
static RT_RC TestApiXhostWrSync32(UINT32 value)
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return RT_OK;
}

//Here the alignment spesifies the number of low-order bits that must be zero.
static UINT64 TestApiMalloc(RT_MEM_TARGET target, UINT32 sz, UINT32 alignment)
{
    DebugPrintf("%s\n",  __FUNCTION__);

    UINT64 physAddress = 0;
    void *desc = 0;
    if (target == RT_VPRMEM)
    {
        LwU64 sysMemOffset = 0;
        void *pDescriptor  = 0;
        ModsDrvAllocPagesIlwPR((size_t)sz, &physAddress, &pDescriptor, &sysMemOffset);
    }
    else
    {
        if (OK != Platform::AllocPagesAligned(sz, &desc, 1 << alignment, 47, Memory::UC, 0))
        {
            TestApiPrint("%s: alloc pages fail!\n", __FUNCTION__);
            MASSERT(0);
        }
        physAddress = Platform::GetPhysicalAddress(desc, 0);
    }

    s_physAddrDescMap.insert(pair<UINT64, void *>(physAddress, desc));
    return physAddress;
}

static UINT32 TestApiFree(UINT64 adr)
{
    DebugPrintf("%s\n",  __FUNCTION__);

    auto itor = s_physAddrDescMap.find(adr);
    if (itor != s_physAddrDescMap.end())
    {
        //do nothing for vpr object.
        if (itor->second)
        {
            Platform::FreePages(itor->second);
        }

        s_physAddrDescMap.erase(itor);
    }
    else
    {
        TestApiPrint("%s: The adr 0x%llx is not allocated by mods!\n", __FUNCTION__, adr);
    }

    return RT_OK;
}

static void TestApiFreeTargetMemory(RT_MEM_TARGET, UINT64 ptr) 
{
    TestApiFree(ptr);
}

static void TestApiInitVpr(UINT32 sz) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
}

static UINT64 TestApiGetMemSize (RT_MEM_TARGET target) 
{
    DebugPrintf("%s\n",  __FUNCTION__);
#ifdef SIM_BUILD
    return GetSysmemSizeForPAddr(0);
#else
    return 0;
#endif
}

static RT_RC TestApiRegisterIntr (UINT32 id , ISR callback)
{ 
    DebugPrintf("%s\n",  __FUNCTION__);
    Platform::HookIRQ(id, callback, 0);
    return RT_OK;
}

static RT_RC TestApiUnRegisterIntr (UINT32 id) 
{ 
    DebugPrintf("%s\n",  __FUNCTION__);
    Platform::UnhookIRQ(id, 0, 0);
    return RT_OK;
}

static void TestApiSetLwrrentThreadIdRef(const UINT32 &thread_id_ref)
{
    DebugPrintf("%s\n",  __FUNCTION__);
}

static bool TestApiIsMemBdEnabled()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return false;
}

static UINT32  TestApiGetProt()
{
    UINT32 protLvl = 0;
    
    if (0 == Platform::EscapeRead("protection_level", 0, sizeof(UINT32), &protLvl))
    {
        DebugPrintf("%s: protection_level:%u\n",  __FUNCTION__, protLvl);
    }
    else
    {
        MASSERT(!"TestApiGetProt: Failed to get protection level by EscapeRead!");
    }

    return protLvl;
}

static void TestApiSetProt(UINT32 protLvl)
{
    if (0 == Platform::EscapeWrite("protection_level", 0, sizeof(UINT32), protLvl))
    {
        DebugPrintf("%s: protection_level:%u\n",  __FUNCTION__, protLvl);
    }
    else
    {
        MASSERT(!"TestApiSetProt: Failed to set protection level by EscapeWrite!");
    }
}

static bool TestApiMutexFree(void*)
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return true;
}

static void TestApiMutexLockBlocking(void*)
{
    DebugPrintf("%s\n",  __FUNCTION__);
}

static bool TestApiMutexLockNonBlocking(void*)
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return true;
}

static void TestApiMutexWait(void*)
{
    DebugPrintf("%s\n",  __FUNCTION__);
}

static bool TestApiMutexUnlock(void* )
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return true;
}

static void TestApiHostBarrier()
{
    DebugPrintf("%s\n",  __FUNCTION__);
}

static void TestApiWaitNS(UINT64 ns)
{
    DebugPrintf("%s: wait %llu nanoseconds!\n",  __FUNCTION__, ns);
    UINT64 tmp = Platform::GetTimeNS() + ns;
    do
    {
        Tasker::Yield();
    }
    while (tmp > Platform::GetTimeNS());
}

static void TestApiYield()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    Tasker::Yield();
}

static UINT64 TestApiGetTime()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return Platform::GetSimulatorTime();
}

enum EventAddr
{
    StartStageEventAddr = LW_ADDRESS_MAP_MISC_BASE + MISCREG_MISC_SPARE_REG_0,
    EndStageEventAddr = LW_ADDRESS_MAP_MISC_BASE + MISCREG_CPU5_TRANSACTOR_SCRATCH_0
};

enum EventValue 
{
    OFF = 0,
    ON = 1,
};

void TestApiInitEvent()
{
    Platform::PhysWr32(StartStageEventAddr, OFF);
    Platform::PhysWr32(EndStageEventAddr, OFF);
}

void TestApiSetEvent(EventId id)
{
    UINT32 address = (id == StartStageEvent) ? StartStageEventAddr : EndStageEventAddr;

    Platform::PhysWr32(address, ON);
}

struct EventPollArgs
{
    UINT64 address;
    bool   result;
};

static bool IsEventSet(void *args)
{
    EventPollArgs *pArgs = (EventPollArgs*)args;
    UINT64 address = pArgs->address;
    UINT32 value = Platform::PhysRd32(address);
    if (value == ON)
    {
        pArgs->result = true;
        DebugPrintf("%s: Sync event is set.\n", __FUNCTION__);
    }

    Platform::ClockSimulator(100);
    return pArgs->result;
}

static UINT64 s_DefaultTimeoutMs = 50000;
bool TestApiWaitOnEvent(EventId id, UINT64 timeout = s_DefaultTimeoutMs)
{
    UINT32 address = (id == StartStageEvent) ? StartStageEventAddr : EndStageEventAddr;

    EventPollArgs pollArgs{address, false};
    POLLWRAP_HW(IsEventSet, &pollArgs, LwU64_LO32(timeout));
    return pollArgs.result;
}

static int TestApiEscapeReadBuffer(UINT32 gpuId, const char *path, UINT32 index, size_t size, void *buffer)
{
    return Platform::GpuEscapeReadBuffer(gpuId, path, index, size, buffer);
}

static int TestApiEscapeWriteBuffer(UINT32 gpuId, const char *path, UINT32 index, size_t size, void *buffer)
{
    return Platform::GpuEscapeWriteBuffer(gpuId, path, index, size, buffer);
}

TestApi::RT_SIM_MODE TestApiGetSimulationMode()
{
    return static_cast<TestApi::RT_SIM_MODE>(Platform::GetSimulationMode());
}

// Route TestApiCtx calls to sim/platform I/F
static void TestApiInit () 
{
    // Allocate the TestAPI object
    DebugPrintf("%s\n",  __FUNCTION__);
    s_pTestApiCtxObj                   = new TestApi::TestApiCtx;

    s_pTestApiCtxObj->RegRead08        = &TestApiRegRd08;
    s_pTestApiCtxObj->RegRead16        = &TestApiRegRd16;
    s_pTestApiCtxObj->RegRead32        = &TestApiRegRd32;
    s_pTestApiCtxObj->RegWrite08       = &TestApiRegWr08;
    s_pTestApiCtxObj->RegWrite16       = &TestApiRegWr16;
    s_pTestApiCtxObj->RegWrite32       = &TestApiRegWr32;

    s_pTestApiCtxObj->GetGPVarStr      = &TestApiGetGPVarStr;
    s_pTestApiCtxObj->SetGPVarStr      = &TestApiSetGPVarStr;
    s_pTestApiCtxObj->GetGPVarDouble   = &TestApiGetGPVarDouble;
    s_pTestApiCtxObj->SetGPVarDouble   = &TestApiSetGPVarDouble;

    s_pTestApiCtxObj->MemRead08        = &TestApiMemRd08;
    s_pTestApiCtxObj->MemRead16        = &TestApiMemRd16;
    s_pTestApiCtxObj->MemRead32        = &TestApiMemRd32;
    s_pTestApiCtxObj->MemRead64        = &TestApiMemRd64;
    s_pTestApiCtxObj->MemReadBlock     = &TestApiMemReadBlock;
    s_pTestApiCtxObj->MemReadBurst     = &TestApiMemReadBurst;
    s_pTestApiCtxObj->MemCrc           = nullptr;

    s_pTestApiCtxObj->MemWrite08       = &TestApiMemWr08;
    s_pTestApiCtxObj->MemWrite16       = &TestApiMemWr16;
    s_pTestApiCtxObj->MemWrite32       = &TestApiMemWr32;
    s_pTestApiCtxObj->MemWrite64       = &TestApiMemWr64;
    s_pTestApiCtxObj->MemWriteBlock    = &TestApiMemWriteBlock;
    s_pTestApiCtxObj->MemWriteBurst    = &TestApiMemWriteBurst;
    s_pTestApiCtxObj->IsMemBdEnabled   = &TestApiIsMemBdEnabled;

    s_pTestApiCtxObj->MemBdRead32      = &TestApiMemRd32;
    s_pTestApiCtxObj->MemBdRead64      = &TestApiMemRd64;
    s_pTestApiCtxObj->MemBdWrite32     = &TestApiMemWr32;
    s_pTestApiCtxObj->MemBdReadBlock   = &TestApiMemReadBlock;
    s_pTestApiCtxObj->MemBdWriteBlock  = &TestApiMemWriteBlock;
    s_pTestApiCtxObj->MemBdWrite64     = &TestApiMemWr64;

    s_pTestApiCtxObj->HMemBdRead32     = &TestApiMemRd32;
    s_pTestApiCtxObj->HMemBdWrite32    = &TestApiMemWr32;

    // Keep the interface aligned between lwmobile and mdiag
    // But we don't need to implement it in MODS
    s_pTestApiCtxObj->SetStreamID = nullptr;
    s_pTestApiCtxObj->GetStreamID = nullptr;
    s_pTestApiCtxObj->SetSelwreContext = nullptr;
    s_pTestApiCtxObj->GetSelwreContext = nullptr;
    s_pTestApiCtxObj->SetSubStreamID = nullptr;
    s_pTestApiCtxObj->GetSubStreamID = nullptr;
    s_pTestApiCtxObj->SetIOCohHint = nullptr;
    s_pTestApiCtxObj->GetIOCohHint = nullptr;
    s_pTestApiCtxObj->SetVPRHint = nullptr;
    s_pTestApiCtxObj->GetVPRHint = nullptr;
    s_pTestApiCtxObj->SetATSTranslationHint = nullptr;
    s_pTestApiCtxObj->GetATSTranslationHint = nullptr;
    s_pTestApiCtxObj->SetGSCRequest = nullptr;
    s_pTestApiCtxObj->GetGSCRequest = nullptr;
    s_pTestApiCtxObj->SetAtomicRequest = nullptr;
    s_pTestApiCtxObj->GetAtomicRequest = nullptr;
    s_pTestApiCtxObj->SetCBBMasterID = nullptr;
    s_pTestApiCtxObj->GetCBBMasterID = nullptr;
    s_pTestApiCtxObj->SetCBBVQC = nullptr;
    s_pTestApiCtxObj->GetCBBVQC = nullptr;
    s_pTestApiCtxObj->SetCBBGroupSec = nullptr;
    s_pTestApiCtxObj->GetCBBGroupSec = nullptr;
    s_pTestApiCtxObj->SetCBBFalconSec = nullptr;
    s_pTestApiCtxObj->GetCBBFalconSec = nullptr;


    s_pTestApiCtxObj->Malloc           = &TestApiMalloc;
    s_pTestApiCtxObj->Free             = &TestApiFree;
    s_pTestApiCtxObj->FreeTargetMemory = &TestApiFreeTargetMemory;
    s_pTestApiCtxObj->GetMemSize       = &TestApiGetMemSize;
    s_pTestApiCtxObj->InitVprMem       = TestApiInitVpr;
    s_pTestApiCtxObj->InitTzMem        = TestApiInitVpr; 

    s_pTestApiCtxObj->RegisterIntr     = &TestApiRegisterIntr;
    s_pTestApiCtxObj->UnRegisterIntr   = &TestApiUnRegisterIntr;
    s_pTestApiCtxObj->RdCPUSync32      = TestApiXhostRdSync32;
    s_pTestApiCtxObj->WrCPUSync32      = TestApiXhostWrSync32;

    s_pTestApiCtxObj->Exit             = &TestApiExit;
    s_pTestApiCtxObj->Poll             = nullptr;
    s_pTestApiCtxObj->PrintMsg         = TestApiPrint;
    s_pTestApiCtxObj->SetThreadIdRef   = TestApiSetLwrrentThreadIdRef;
    s_pTestApiCtxObj->SimEnterCritical = &TestApiEnterCritical;
    s_pTestApiCtxObj->SimExitCritical = &TestApiExitCritical;

    s_pTestApiCtxObj->GetArgs          = nullptr;

    s_pTestApiCtxObj->ParseIntArg      = nullptr;
    s_pTestApiCtxObj->ParseCharArg     = nullptr;

    s_pTestApiCtxObj->AllocateBbcMemRegions = nullptr;

    s_pTestApiCtxObj->rtprintlevel     = 0;
    s_pTestApiCtxObj->bDumpTransaction     = LW_FALSE;

    //unclear how tz works on Hopper
    s_pTestApiCtxObj->GetProt = TestApiGetProt;
    s_pTestApiCtxObj->SetProt = TestApiSetProt;
    s_pTestApiCtxObj->isMutexFree = TestApiMutexFree;
    s_pTestApiCtxObj->mutexLockBlocking = TestApiMutexLockBlocking;
    s_pTestApiCtxObj->mutexLockNonblocking = TestApiMutexLockNonBlocking;
    s_pTestApiCtxObj->mutexWait = TestApiMutexWait;
    s_pTestApiCtxObj->mutexUnlock = TestApiMutexUnlock;
    s_pTestApiCtxObj->HostBarrier = TestApiHostBarrier;
    s_pTestApiCtxObj->WaitNS = &TestApiWaitNS;
    s_pTestApiCtxObj->Yield = &TestApiYield;
    s_pTestApiCtxObj->GetTime = &TestApiGetTime;
    s_pTestApiCtxObj->SetEvent= TestApiSetEvent;
    s_pTestApiCtxObj->WaitOnEvent= TestApiWaitOnEvent;
    s_pTestApiCtxObj->EscapeReadBuffer = TestApiEscapeReadBuffer;
    s_pTestApiCtxObj->EscapeWriteBuffer = TestApiEscapeWriteBuffer;
    s_pTestApiCtxObj->GetSimulationMode = TestApiGetSimulationMode;
    TestApiPrint("s_pTestApiCtxObj init done!!!\n");
}

//Return testapicontext ptr for rtapi test
void* MdiagTestApiGetContext()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    if (s_pTestApiCtxObj == nullptr)
    {
        TestApiInit();
    }
    return static_cast<void*>(s_pTestApiCtxObj);
}

void TestApiRelease()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    if (s_pTestApiCtxObj != nullptr)
    {
        delete s_pTestApiCtxObj;
        s_pTestApiCtxObj = nullptr;
    }
}

void* TestApi::TestApiGetContext()
{
    DebugPrintf("%s\n",  __FUNCTION__);
    return MdiagTestApiGetContext();
}
