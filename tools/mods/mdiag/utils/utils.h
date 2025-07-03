/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef MDIAG_UTILS_H
#define MDIAG_UTILS_H

#include "types.h"
#include "core/include/memtypes.h"
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include "core/include/rc.h"
#include "gpu/reghal/reghaltable.h"
#include "core/include/lwrm.h"
#include <set>

#define FALCON_NOP_COUNT 32

class GpuSubdevice;
class LwRm;
class GpuDevice;
class MdiagSurf;

string UtilStrTranslate(const string& str, char c1, char c2);

int P4Action(const char *action, const char *filename);

void clean_filename_slashes(char *out, const char *in);
void extract_sub_and_match_list(char *sub, char *match_list, const char *prefix);
char* extract_next_match_string(char *match, char *match_list);
RC PraminPhysRd(UINT64 physAddress, Memory::Location location, UINT32 size, string * pString = NULL);
RC  TagTrepFilePass();

namespace MDiagUtils
{
    enum TestStage
    {
        TestSetupStart,
        TestSetupEnd,
        TestRunStart,
        TestBeforeCrcCheck,
        TestRunEnd,
        TestCleanupStart,
        TestCleanupEnd,
        NumTestStage
    };

    // GetTestStageName(): For debug prints only.
    static inline const char* GetTestStageName(const TestStage stage)
    {
        switch (stage)
        {
            case TestSetupStart:        return "TestSetupStart";
            case TestSetupEnd:          return "TestSetupEnd";
            case TestRunStart:          return "TestRunStart";
            case TestBeforeCrcCheck:    return "TestBeforeCrcCheck";
            case TestRunEnd:            return "TestRunEnd";
            case TestCleanupStart:      return "TestCleanupStart";
            case TestCleanupEnd:        return "TestCleanupEnd";
            default:
                break;
        }
        return "UnknownTestStage";
    }

    RC AllocateChannelBuffers
    (
        GpuDevice *pGpuDevice,
        MdiagSurf *pErrorNotifier,
        MdiagSurf *pPushBuffer,
        MdiagSurf *pGpFifo,
        const char *channelName,
        LwRm::Handle hVaSpace,
        LwRm* pLwRm
    );

#if defined(__WIN64) || defined(_WIN32)
    class ShmLock
    {
    public:
        void Init() {}
        void CleanUp() {}
        void Lock() {}
        void Unlock() {}
    };
#else
    class ShmLock
    {
    public:
        void Init()
        {
            pthread_mutexattr_init(&m_Attr);
            pthread_mutexattr_setpshared(&m_Attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&m_Mutex, &m_Attr);
        }
        void CleanUp()
        {
            pthread_mutex_destroy(&m_Mutex);
            pthread_mutexattr_destroy(&m_Attr);
        }
        void Lock()
        {
            pthread_mutex_lock(&m_Mutex);
        }
        void Unlock()
        {
            pthread_mutex_unlock(&m_Mutex);
        }
    private:
        // Non-copyable
        ShmLock(const ShmLock&);
        ShmLock& operator=(const ShmLock&);

        pthread_mutexattr_t m_Attr;
        pthread_mutex_t m_Mutex;
    };
#endif

    template <typename ElemType, const size_t BufCapacity>
    class LoopBuffer
    {
    public:
        //! This Init() needed when buffer in shared memory, etc,
        // where doesn't have auto ctor.
        void Init()
        {
            m_Write = m_Read = 0;
        }

        //! @return false if buffer is full as failed.
        bool Push(const ElemType& elem)
        {
            if (!IsFull())
            {
                m_Elems[m_Write] = elem;
                m_Write = UpdateOffset(m_Write);
                return true;
            }
            return false;
        }

        //! @return false if buffer is empty as failed.
        bool Pop(ElemType* pElem)
        {
            if (!IsEmpty())
            {
                *pElem = m_Elems[m_Read];
                m_Read = UpdateOffset(m_Read);
                return true;
            }
            return false;
        }

    private:
        size_t Capacity() const
        {
            return sizeof(m_Elems) / sizeof(m_Elems[0]);
        }
        const ElemType* End() const
        {
            return m_Elems + Capacity();
        }
        //! Numer of elems in the buffer.
        size_t Size() const
        {
            return m_Write >= m_Read ? m_Write - m_Read
                : (Capacity() - (m_Read - m_Write));
        }
        bool IsFull() const
        {
            return Size() >= Capacity() - 1;
        }
        bool IsEmpty() const
        {
            return Size() == 0;
        }
        size_t UpdateOffset(size_t off) const
        {
            if (m_Elems + off + 1 < End())
            {
                return ++off;
            }
            return 0;
        }

        ElemType m_Elems[BufCapacity + 1];
        size_t m_Write = 0;
        size_t m_Read = 0;
    };

    class SetMemoryTag
    {
        public:
            SetMemoryTag(GpuDevice* pGpuDevice, const char* tag) : m_pGpuDevice(pGpuDevice)
            {
                MemoryTag(m_pGpuDevice, tag);
            }
            ~SetMemoryTag()
            {
                MemoryTag(m_pGpuDevice, 0);
            }

        private:
            GpuDevice* m_pGpuDevice;
            static void MemoryTag(GpuDevice* pGpuDevice, const char *ss);
    };

    // This class is to represent a segment of memory.
    class RawMemory
    {
    public: 
        RawMemory(LwRm *pLwRm, GpuDevice* pGpuDev) : m_pLwRm(pLwRm), m_pGpuDev(pGpuDev) {};
        ~RawMemory();

        Memory::Location GetLocation() const { return m_Location; }
        UINT64 GetSize() const { return m_Size; }
        PHYSADDR GetPhysAddress() const { return m_PhysAddress; }
        void* GetAddress() const
        {
            MASSERT(nullptr != m_Address);
            return m_Address;
        } 
        LwRm::Handle GetMappedHandle() const { return m_MappedHandle; }

        void SetLocation(Memory::Location location);
        void SetSize(UINT64 size);
        void SetPhysAddress(PHYSADDR physAddress);
        
        RC Map();
        RC Unmap();
        bool IsMapped() const { return m_Address != nullptr; }

    private:
        LwRm  *m_pLwRm;
        GpuDevice* m_pGpuDev;
        Memory::Location m_Location = Memory::Fb;
        UINT64 m_Size = 0x1000; //4k, small page
        PHYSADDR m_PhysAddress = 0;
        void *m_Address = nullptr;
        LwRm::Handle m_MappedHandle = 0;
    };

    UINT32 GetRmNotifierByEngineType(UINT32 engineType, GpuSubdevice *pGpuSubdevice, LwRm* pLwRm);

    LW2080_CTRL_FIFO_MEM_INFO GetInstProperties(GpuSubdevice *pSubGpuDevice, UINT32 hChannel, LwRm* pLwRm);
    RC  TagTrepFilePass();
    bool IsCopyEngine(UINT32 engine);
    bool IsLwDecEngine(UINT32 engine);
    bool IsLwEncEngine(UINT32 engine);
    bool IsOFAEngine(UINT32 engine);
    bool IsLWJPGEngine(UINT32 engine);
    bool IsGraphicsEngine(UINT32 engine);
    bool IsSECEngine(UINT32 engine);
    bool IsSWEngine(UINT32 engine);
    UINT32 GetGrEngineOffset(UINT32 engine);
    UINT32 GetCopyEngineOffset(UINT32 engine);
    UINT32 GetLwDecEngineOffset(UINT32 engine);
    UINT32 GetLwEncEngineOffset(UINT32 engine);
    UINT32 GetLwJpgEngineOffset(UINT32 engine);
    string Eng2Str(UINT32 engine);
    UINT32 GetEngineIdBase(vector<UINT32> classIds, GpuDevice* pGpuDevice, LwRm* pLwRm, bool isGrCE);
    UINT32 GetGrEngineId(UINT32 engineOffset);
    UINT32 GetCopyEngineId(UINT32 engineOffset);
    UINT32 GetLwDecEngineId(UINT32 engineOffset);
    UINT32 GetLwEncEngineId(UINT32 engineOffset);
    UINT32 GetLwJpgEngineId(UINT32 engineOffset);

    RC VirtFunctionReset(UINT32 bus, UINT32 domain, UINT32 fucntion, UINT32 device);
    void ReadFLRPendingBits(vector<UINT32> * data);
    void UpdateFLRPendingBits(UINT32 GFID, vector<UINT32> * data);
    void ClearFLRPendingBits(vector<UINT32> data);
    RC PollSharedMemoryMessage(set<UINT32> &gfids, const string &message);
    RC WaitFlrDone(UINT32 gfid);
    UINT32 GetTailNum(const string& regName);
    RegHalDomain AdjustDomain(RegHalDomain domain, const string& regSpace);
    RegHalDomain GetDomain(const string& regName, const string& regSpace = "");
    UINT32 GetDomainIndex(const string& regSpace);
    UINT32 GetDomainBase(const char* regName, const char* regSpace, GpuSubdevice *pSubdev, LwRm* pLwRm);
    static const string s_LWPgraphGpcPrefix = "LW_PGRAPH_PRI_GPC";
    string GetTestNameFromTraceFile(const string& traceFile);

    bool IsChipTu10x();
    bool IsChipTu11x();
    RC ResetEngine(UINT32 hChannel, UINT32 engineId, LwRm * pLwRm, GpuDevice * pGpuDevice);
    UINT32 GetPhysChannelId(UINT32 vChannelId, LwRm * pLwRm, UINT32 engineId);

    RC PraminPhysRdRaw(UINT64 physAddress, Memory::Location location, UINT64 size, vector<UINT08>* pOut);
    RC PraminPhysRdRaw(GpuSubdevice *pSubDev, UINT64 physAddress, Memory::Location location, UINT64 size, vector<UINT08>* pOut);

    string GetSupportedClassesString(LwRm* pLwRm, GpuDevice* pGpuDevice);
    UINT08 GetIRQType();

    static map<GpuSubdevice*, UINT32> s_Subdev2MMUEngineIdStart;
    RC CacheMMUEngineIdStart(GpuSubdevice* pGpuSubdev);
    UINT32 GetCachedMMUEngineIdStart(GpuSubdevice* pGpuSubdev);

    static set<UINT32> s_RegisteredUtlFaultIndices;
    bool IsFaultIndexRegisteredInUtl(UINT32 index);
    void RegisterFaultIndexInUtl(UINT32 index);

    RC ZeroMem(UINT64 addr, UINT32 len);

} // MDiagUtils

#endif
