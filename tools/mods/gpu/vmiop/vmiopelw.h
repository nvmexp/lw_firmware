/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/lwrm.h"
#include "core/include/xp.h"
#include "core/include/cpu.h"
#include "core/include/tasker.h"
#include "gpu/reghal/reghal.h"
#include <type_traits>

class VmiopElwManager;

class VmiopElw
{
public:
    VmiopElw(VmiopElwManager *pVmiopElwMgr, UINT32 gfid, UINT32 seqId) :
        m_pVmiopElwMgr(pVmiopElwMgr), m_Gfid(gfid), m_SeqId(seqId) {}
    VmiopElw()                           = delete;
    VmiopElw(const VmiopElw&)            = delete;
    VmiopElw& operator=(const VmiopElw&) = delete;
    virtual ~VmiopElw() { Shutdown(); }

    virtual RC   Initialize();
    virtual RC   Shutdown();
    UINT32       GetGfid()   const { return m_Gfid; }
    UINT32       GetSeqId()  const { return m_SeqId; }
    UINT32       GetPcieDomain()   { InitBdf(); return m_PcieDomain; }
    UINT32       GetPcieBus()      { InitBdf(); return m_PcieBus; }
    UINT32       GetPcieDevice()   { InitBdf(); return m_PcieDevice; }
    UINT32       GetPcieFunction() { InitBdf(); return m_PcieFunction; }
    LwRm::Handle GetPluginHandle() { InitBdf(); return m_PluginHandle; }

    RC         SetRemotePid(UINT32 pid);
    RC         RunRemoteProcess(const string& cmdLine);
    bool       IsRemoteProcessRunning() const;
    RC         WaitRemoteProcessDone() const;
    void       KillRemoteProcess() const;

    virtual RC SendReceivePcieBarInfo(UINT32 barIndex,
                                      PHYSADDR* pBarBase, UINT64* pBarSize) = 0;
    virtual RC FindVfEmuRegister(PHYSADDR physAddr, UINT32* pRegister) = 0;


    //! After RPC doorbell implemented on hw, these interfaces
    //! for mailbox code can be removed:
    //!      CallPluginRegRead/Write()
    //!      CallPluginPfRegRead()
    //!      CallPluginPfRegIsSupported()
    //!      CallPluginPfRegTest32()
    //!      CallPluginSyncPfVfs()
    virtual RC CallPluginRegWrite(UINT32 dataOffset,
                                  UINT32 dataSize,
                                  const void* pData) = 0;
    virtual RC CallPluginRegRead(UINT32 dataOffset,
                                 UINT32 dataSize,
                                 void* pData) = 0;
    virtual RC ProcessRpcMailbox()  { return RC::OK; }
    virtual RC ProcessModsChannel() { return RC::OK; }
    virtual RC ProcessingRequests() { return RC::OK; }

    // Log test status on VFs
    virtual RC CallPluginLogTestStart(UINT32 gfid, INT32 testNumber)= 0;
    virtual RC CallPluginLogTestEnd(UINT32 gfid, INT32 testNumber, UINT32 errorCode) = 0;

    virtual RC CallPluginPfRegRead(ModsGpuRegAddress regAddr, UINT32* pVal) = 0;
    virtual RC CallPluginPfRegIsSupported(ModsGpuRegAddress regAddr, bool* pIsSupported) = 0;
    virtual RC CallPluginSyncPfVfs(UINT32 timeoutMs, bool syncPf) = 0;
    virtual RC CallPluginLocalGpcToHwGpc(UINT32 smcEngineIdx,
                                         UINT32 localGpcNum, UINT32* pHwGpcNum) = 0;
    virtual RC CallPluginGetTpcMask(UINT32 hwGpcNum, UINT32* pTpcMask) = 0;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param field Register field.
    //! \param regIndexes Register indexes.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    virtual RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        RegHal::ArrayIndexes regIndexes,
        UINT32 value,
        bool* pResult
    ) = 0;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param value Register value. Corresponding register will be checked.
    //! \param regIndexes Register indexes.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    virtual RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        RegHal::ArrayIndexes regIndexes,
        bool* pResult
    ) = 0;

    // Two possible PmAction orders in current FLR tests.
    // 1. Notify VF FLR before vFLR.
    //    Action.SendProcEvent("flr_test_starting", VF.SeqId(1));
    //    Action.VirtFunctionLevelReset(VF.SeqId(1))
    // 2. Notify VF FLR after vFLR.
    //    Action.VirtFunctionLevelReset(VF.SeqId(1))
    //    Action.SendProcEvent("flr_test_starting", VF.SeqId(1));
    //
    // Need two flags, one flag FlrStarted to let VmiopElw know vFLR started,
    // the other flag ExitStarted to unblock vFLR when it waiting for VmiopElwExit in the above 2nd order,
    // otherwise VF won't get "flr_test_starting" from PF because PF is waiting for VF exit too.

    // FLR started or not.
    virtual void StartFlr() {}
    virtual bool IsFlrStarted() const { return false; }
    virtual void StartExit() {}
    virtual bool IsExitStarted() const { return false; }

    struct PcieBarInfo
    {
        struct Bar
        {
            PHYSADDR base;
            UINT64 size;
        };
        static const UINT32 NUM_BARS = 3; //!< Bar0-2
        Bar bar[NUM_BARS];
    };

    struct RpcMailbox
    {
        enum class State : UINT32
        {
            DONE,
            WRITE,
            READ,
            REQUEST_FAILED,
            ABORT,
        };
        UINT32 offset;
        UINT32 size;
        static constexpr UINT32 DATA_MAX_SIZE = 1024;
        UINT08 data[DATA_MAX_SIZE];

        // Only access state variable through functions to ensure atomic access and memory fence
        // It's the responsibility of the programmer to ensure that:
        //
        // 1. GetState() is called before all reads in the transaction
        // 2. Commit() is called after all writes in the transaction
        using utype = std::underlying_type<State>::type;
        State GetState()
        {
            return static_cast<State>(Cpu::AtomicRead(reinterpret_cast<utype*>(&state)));
        }
        void Commit(State newState)
        {
            Cpu::AtomicWrite(reinterpret_cast<utype*>(&state), static_cast<utype>(newState));
        }

        private:
            State state = State::DONE;
    };
    struct ModsChannel
    {
        enum class State : UINT32
        {
            INIT,
            VF_REQUEST_SENT,
            PF_REQUEST_FAILURE,
            PF_REQUEST_SUCCESS,
            ABORT,
        };
        enum class Request : UINT32
        {
            // VF to PF requests
            PF_REG_READ,         //!< Read PF register
            PF_REG_IS_SUPPORTED, //!< Check if PF register is supported
            PF_REG_TEST_32,      //!< Test if 32-bit PF register has value
            SYNC_PF_VF,
            LOG_TEST,
            LOCAL_GPC_TO_HW_GPC,
            GET_TPC_MASK,
        };

        //!
        //! \brief Payload for PF_REG_READ request.
        //!
        //! \see Request
        //!
        struct RegData
        {
            ModsGpuRegAddress addr; //!< [in] Address of register to read.
            UINT32 value;           //!< [out] Value read from register.
        };

        //!
        //! \brief Payload for PF_REG_IS_SUPPORTED.
        //!
        //! \see Request
        //!
        struct RegIsSupported
        {
            ModsGpuRegAddress addr; //!< [in] Address of register to check.
            bool isSupported;       //!< [out] True if register is supported.
        };

        //!
        //! \brief Base type of the payload for PF_REG_TEST_32.
        //!
        //! \see Request
        //! \see RegTest32RegField
        //! \see RegTest32RegValue
        //!
        struct RegTest32
        {
            //! Max reghal indicies that can be sent in a payload.
            static constexpr UINT32 MAX_REGHAL_INDEXES = 10;

            enum class Variant : UINT08
            {
                REG_FIELD,        //!< Takes a register field.
                REG_VALUE         //!< Takes a register value.
            };

            Variant type;

            explicit RegTest32(Variant t) : type(t) {}
        };

        static_assert(std::is_same<RegHal::ArrayIndexes_t::value_type, UINT32>::value,
                      "RegHal register index type mismatch");

        //!
        //! \brief Payload for variant of PF_REG_TEST_32 that takes a register
        //! field.
        //!
        //! \see Request
        //!
        struct RegTest32RegField : RegTest32
        {
            UINT32                            domain  = 0;     //!< [in] Register domain.
            ModsGpuRegField                   field   = {};    //!< [in] Register field.
            UINT32                            numIndexes = 0;  //!< [in] Number of valid register indices.
            array<UINT32, MAX_REGHAL_INDEXES> indexes = {};    //!< [in] Register indices.
            UINT32                            value   = 0;     //!< [in] Value to check against.
            bool                              result  = false; //!< [out] True if register had the value.

            RegTest32RegField() : RegTest32(Variant::REG_FIELD) {}
        };

        //!
        //! \brief Payload for variant of PF_REG_TEST_32 that takes a register
        //! value.
        //!
        //! \see Request
        //!
        struct RegTest32RegValue : RegTest32
        {
            UINT32                            domain  = 0;     //!< [in] Register domain.
            ModsGpuRegValue                   value   = {};    //!< [in] Register value.
            UINT32                            numIndexes = 0;  //!< [in] Number of valid register indices.
            array<UINT32, MAX_REGHAL_INDEXES> indexes = {};    //!< [in] Register indices.
            bool                              result  = false; //!< [out] True if register had the value.

            RegTest32RegValue() : RegTest32(Variant::REG_VALUE) {}
        };

        //!
        //! \brief Payload for SYNC_PF_VF request.
        //!
        //! \see Request
        //!
        struct SyncPfVfs
        {
            UINT32 timeoutMs;
            bool syncPf;
        };
        struct TestLog
        {
            UINT32 gfid;
            INT32 testNumber;
            UINT32 errorCode;
            bool bIsTestStart;
        };
        struct LocalGpcToHwGpc
        {
            UINT32 smcEngineIdx;
            UINT32 localGpcNum;
            UINT32 hwGpcNum;
        };
        struct GetTpcMask 
        {
            UINT32 hwGpcNum;
            UINT32 tpcMask;
        };
        Request request;
        UINT32 size;
        static constexpr UINT32 DATA_MAX_SIZE = 1024;
        UINT08 data[DATA_MAX_SIZE];

        // Only access state variable through functions to ensure atomic access and memory fence
        // It's the responsibility of the programmer to ensure that:
        //
        // 1. GetState() is called before all reads in the transaction
        // 2. Commit() is called after all writes in the transaction
        using utype = std::underlying_type<State>::type;
        State GetState()
        {
            return static_cast<State>(Cpu::AtomicRead(reinterpret_cast<utype*>(&state)));
        }

        RC Commit(State newState)
        {
            if (size > DATA_MAX_SIZE)
            {
                Printf(Tee::PriError, "Exceeded maximum channel payload size:\n\tgiven: %u, max: %u\n", size, DATA_MAX_SIZE);
                return RC::SOFTWARE_ERROR;
            }

            Cpu::AtomicWrite(reinterpret_cast<utype*>(&state), static_cast<utype>(newState));

            return RC::OK;
        }

        private:
            State state = State::INIT;
    };

    void AbortRpcMailbox();
    RC AbortModsChannel();

protected:
    VmiopElwManager *GetVmiopElwMgr() const { return m_pVmiopElwMgr; }
    virtual RC CreateVmiopConfigValues() { return OK; }
    virtual RC CreateVmiopConfigValue(bool isPluginSpecific, const char *key,
                                      const string &value) { return OK; }

    virtual RC SetElw(Xp::Process *pProcess) { return RC::SOFTWARE_ERROR; };
    enum ProcSharedBufType
    {
        PCIE_BAR_INFO,
        RPC_MAILBOX,
        TEST_LOGGER,
        MODS_CHAN,
        NUM_PROC_SHARED_BUF_TYPES // Must be last
    };
    struct ProcSharedBufDesc
    {
        void*  pBuf = nullptr;
        string bufName = "";
        size_t bufSize = 0;
    };
    vector<ProcSharedBufDesc> m_ProcSharedBufs;

    RC CreateProcSharedBuf(UINT32 index, const char* name, size_t size);
    RC DestroyProcSharedBufs();
    virtual RC MapProcSharedBuf(ProcSharedBufDesc *pDesc) = 0;

private:
    void InitBdf();
    VmiopElwManager *m_pVmiopElwMgr;
    UINT32       m_Gfid;
    UINT32       m_SeqId;
    UINT32       m_PcieDomain   = _UINT32_MAX;
    UINT32       m_PcieBus      = _UINT32_MAX;
    UINT32       m_PcieDevice   = _UINT32_MAX;
    UINT32       m_PcieFunction = _UINT32_MAX;
    LwRm::Handle m_PluginHandle = 0;
    UINT32       m_RemotePid    = 0;
    unique_ptr<Xp::Process> m_pRemoteProcess;
};

class VmiopElwPhysFun : virtual public VmiopElw
{
public:
    VmiopElwPhysFun(VmiopElwManager *pVmiopElwMgr, UINT32 gfid, UINT32 seqId) :
        VmiopElw(pVmiopElwMgr, gfid, seqId) {}
    RC Initialize() override;

    RC SendReceivePcieBarInfo(UINT32 barIndex,
                              PHYSADDR* pBarBase, UINT64* pBarSize) override;
    RC FindVfEmuRegister(PHYSADDR physAddr, UINT32* pRegister) override;

    RC CallPluginRegWrite(UINT32 dataOffset,
                          UINT32 dataSize,
                          const void* pData) override;
    RC CallPluginRegRead(UINT32 dataOffset,
                         UINT32 dataSize,
                         void* pData) override;
    RC ProcessRpcMailbox() override;
    RC ProcessModsChannel() override;
    RC ProcessingRequests() override;

    RC CallPluginLogTestStart(UINT32 gfid, INT32 testNumber) override;
    RC CallPluginLogTestEnd(UINT32 gfid, INT32 testNumber, UINT32 errorCode) override;

    RC CallPluginPfRegRead(ModsGpuRegAddress regAddr, UINT32* pVal) override;
    RC CallPluginPfRegIsSupported(ModsGpuRegAddress regAddr, bool* pIsSupported) override;
    RC CallPluginSyncPfVfs(UINT32 timeoutMs, bool syncPf) override;
    virtual RC CallPluginLocalGpcToHwGpc(UINT32 smcEngineIdx,
                                         UINT32 localGpcNum, UINT32* pHwGpcNum) override;
    virtual RC CallPluginGetTpcMask(UINT32 hwGpcNum, UINT32* pTpcMask) override;

    RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        RegHal::ArrayIndexes regIndexes,
        UINT32 value,
        bool* pResult
    ) override;
    RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        RegHal::ArrayIndexes regIndexes,
        bool* pResult
    ) override;

    void StartFlr() override;
    bool IsFlrStarted() const override { return m_bFlrStarted; }
    void StartExit() override;
    bool IsExitStarted() const override { return m_bExitStarted; }

protected:
    RC MapProcSharedBuf(ProcSharedBufDesc *pDesc) override;
    RC CreateVmiopConfigValues() override;
    RC CreateVmiopConfigValue(bool isPluginSpecific, const char *key,
                              const string &value) override;
    virtual RC SetElw(Xp::Process *pProcess) override;

private:
    bool m_bFlrStarted = false;
    bool m_bExitStarted = false;
};

class VmiopElwVirtFun: virtual public VmiopElw
{
public:
    VmiopElwVirtFun(VmiopElwManager *pVmiopElwMgr, UINT32 gfid, UINT32 seqId) :
        VmiopElw(pVmiopElwMgr, gfid, seqId) {}
    RC Initialize() override;

    RC SendReceivePcieBarInfo(UINT32 barIndex,
                              PHYSADDR* pBarBase, UINT64* pBarSize) override;
    RC FindVfEmuRegister(PHYSADDR physAddr, UINT32* pRegister) override;

    RC CallPluginRegWrite(UINT32 dataOffset,
                          UINT32 dataSize,
                          const void* pData) override;
    RC CallPluginRegRead(UINT32 dataOffset,
                         UINT32 dataSize,
                         void* pData) override;

    RC CallPluginLogTestStart(UINT32 gfid, INT32 testNumber) override;
    RC CallPluginLogTestEnd(UINT32 gfid, INT32 testNumber, UINT32 errorCode) override;

    RC CallPluginPfRegRead(ModsGpuRegAddress regAddr, UINT32* pVal) override;
    RC CallPluginPfRegIsSupported(ModsGpuRegAddress regAddr, bool* pIsSupported) override;
    RC CallPluginSyncPfVfs(UINT32 timeoutMs, bool syncPf) override;
    virtual RC CallPluginLocalGpcToHwGpc(UINT32 smcEngineIdx,
                                         UINT32 localGpcNum, UINT32* pHwGpcNum) override;
    virtual RC CallPluginGetTpcMask(UINT32 hwGpcNum, UINT32* pTpcMask) override;

    RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        RegHal::ArrayIndexes regIndexes,
        UINT32 value,
        bool* pResult
    ) override;
    RC CallPluginPfRegTest32
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        RegHal::ArrayIndexes regIndexes,
        bool* pResult
    ) override;

    // Mutex for accessing the MODS VF <-> PF channel
    Tasker::Mutex m_pVfModsChannelMutex =
        Tasker::CreateMutex("VfModsChannel", Tasker::mtxUnchecked);

protected:
    RC MapProcSharedBuf(ProcSharedBufDesc *pDesc) override;

private:
    RC GetRpcMailbox(RpcMailbox **ppRpcMailbox, UINT32 dataSize);
    RC GetModsChan(ModsChannel **ppChan);
    RC WaitForRpc(RpcMailbox *pMailbox);
    RC WaitForModsChan(ModsChannel* pChan, FLOAT64 timeoutMs);
    // Cached values used to optimize FindVfEmuRegister()
    PHYSADDR m_VgpuEmuRange[2]      = {0, 0};
    PHYSADDR m_Bar0OverflowRange[2] = {0, 0};
};
