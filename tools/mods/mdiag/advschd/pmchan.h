/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Define a PmChannel wrapper around a Channel or LWGpuChannel

#ifndef INCLUDED_PMCHAN_H
#define INCLUDED_PMCHAN_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_CHANNEL_H
#include "core/include/channel.h"
#endif

#include "mdiag/subctx.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "mdiag/channelallocinfo.h"

class PmNonStallInt;
class PmMethodInt;
class PmContextSwitchInt;

DECLARE_MSG_GROUP(Mdiag, Gpu, ChannelOp)

//--------------------------------------------------------------------
//! \brief Abstract wrapper around a Channel or LWGpuChannel
//!
//! This abstract class is designed to let the PolicyManager
//! interface to a Channel (from MODS) or LWGpuChannel (from
//! Mdiag), without having to deal directly with the subclasses.
//!
class PmChannel
{
public:
    PmChannel(PolicyManager *pPolicyManager, PmTest *pTest,
              GpuDevice *pGpuDevice, const string &name,
              bool createdByPM,
              LwRm* pLwRm);
    virtual ~PmChannel();
    enum Type
    {
        PIO,
        GPFIFO
    };

    PolicyManager *GetPolicyManager() const { return m_pPolicyManager; }
    PmTest        *GetTest()          const { return m_pTest; }
    GpuDevice     *GetGpuDevice()     const { return m_pGpuDevice; }
    string         GetName()          const { return m_Name; }
    UINT32         GetChannelNumber() const { return m_ChannelNumber; }
    bool           CreatedByPM()      const { return m_CreatedByPM; }
    RC             SetSubchannelName(UINT32 subch, string name);
    string         GetSubchannelName(UINT32 subch) const;
    RC             SetSubchannelClass(UINT32 subch, UINT32 classNum);
    UINT32         GetSubchannelClass(UINT32 subch) const;
    RC             GetCESubchannelNum(UINT32* pCESubchNum);
    bool           HasGraphicObjClass() const;
    bool           HasComputeObjClass() const;
    RC             EndTest();
    void           SetChannelWrapper(PmChannelWrapper *pPmChannelWrapper);

    virtual RC Write(UINT32 subchannel, UINT32 method, UINT32 data) = 0;
    virtual RC Write(UINT32 subchannel, UINT32 method,
                     UINT32 count, const UINT32 *pData) = 0;
    RC Write(UINT32 subchannel, UINT32 method, UINT32 count, UINT32 data, ...);
    virtual RC SetPrivEnable(bool priv) = 0;
    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma) = 0;
    virtual RC SetSemaphoreOffset(UINT64 Offset) = 0;
    virtual void SetSemaphoreReleaseFlags(UINT32 flags) = 0;
    virtual UINT32 GetSemaphoreReleaseFlags() = 0;
    virtual RC SetSemaphorePayloadSize(Channel::SemaphorePayloadSize size) = 0;
    virtual Channel::SemaphorePayloadSize GetSemaphorePayloadSize() = 0;
    virtual bool Has64bitSemaphores() = 0;
    virtual RC SemaphoreRelease(UINT64 Data) = 0;
    virtual RC SemaphoreReduction(Channel::Reduction Op,
                                  Channel::ReductionType Type, UINT64 Data) = 0;
    virtual RC SemaphoreAcquire(UINT64 Data) = 0;
    virtual RC NonStallInterrupt() = 0;
    virtual RC BeginInsertedMethods() = 0;
    virtual RC EndInsertedMethods() = 0;
    virtual void CancelInsertedMethods() = 0;
    virtual RC Flush() = 0;
    virtual RC WaitForIdle() = 0;
    virtual RC WriteSetSubdevice(UINT32 Subdevice) = 0;
    virtual Channel *GetModsChannel() const = 0;
    virtual LwRm::Handle GetHandle() const = 0;
    virtual LwRm::Handle GetTsgHandle() const = 0;
    virtual Type         GetType()   const = 0;
    virtual UINT32       GetMethodCount() const = 0;
    virtual bool         GetEnabled() const = 0;
    virtual RC           SetEnabled(bool enabled) = 0;
    virtual bool         IsAtsEnabled() const = 0;
    virtual VaSpace*     GetVaSpace() = 0;
    virtual LwRm::Handle GetVaSpaceHandle() const = 0;
    virtual shared_ptr<SubCtx> GetSubCtxInTsg(UINT32 veid) const = 0;
    virtual shared_ptr<SubCtx> GetSubCtx() const = 0;
    virtual LWGpuTsg    *GetTsg() const = 0;
    PmNonStallInt       *GetNonStallInt(const string &intName);
    PmMethodInt         *GetMethodInt();
    PmContextSwitchInt  *GetCtxSwInt();
    LwRm                *GetLwRmPtr() const { return m_pLwRm; }
    virtual UINT32       GetEngineId() const = 0;
    void                 SetPmSmcEngine(PmSmcEngine* pPmSmcEngine) { m_pPmSmcEngine = pPmSmcEngine; }
    virtual PmSmcEngine* GetPmSmcEngine() { return m_pPmSmcEngine; }

    void FreeSubchannels();

    void AddSubchannelHandle(LwRm::Handle handle)
        { m_SubchannelHandles.push_back(handle); };

    RC GetInstBlockInfo();
    UINT64 GetInstPhysAddr();
    UINT32 GetInstAperture();

    virtual UINT32 GetChannelInitMethodCount() const = 0;
    virtual RC RealCreateChannel(UINT32 engineId) = 0;

    //! Semi-RAII class that helps make sure that methods inserted by
    //! PolicyManager are surrounded by BeginInsertedMethods &
    //! EndInsertedMethods, and that they call CancelInsertedMtehods()
    //! on error.
    //!
    //! This class is normally not used directly; use the
    //! PM_BEGIN/END_INSERTED_METHOD() macros instead.
    //!
    class InsertMethods
    {
    public:
        InsertMethods(PmChannel *pChannel);
        ~InsertMethods();
        RC Begin();
        RC End();
    private:
        PmChannel *m_pChannel;
        bool m_InProgress;
    };

private:
    static const UINT32 UNINITIALIZED_INST_APERTURE = _UINT32_MAX;
    PolicyManager    *m_pPolicyManager;
    PmTest           *m_pTest;
    string            m_Name;
    UINT32            m_ChannelNumber;
    struct SubchannelInfo
    {
        SubchannelInfo() : classNum(0) {}
        string name;
        UINT32 classNum;
    } m_SubchannelNames[Channel::NumSubchannels];
    GpuDevice        *m_pGpuDevice;
    PmChannelWrapper *m_pPmChannelWrapper;
    map<string, PmNonStallInt*> m_NonStallInts;
    PmMethodInt      *m_pMethodInt;
    PmContextSwitchInt *m_pCtxSwInt;
    bool m_CreatedByPM;
    vector<LwRm::Handle> m_SubchannelHandles;
    UINT64 m_InstPa; // Instance Memory physical address
    UINT32 m_InstAperture;
    LwRm* m_pLwRm;
    PmSmcEngine* m_pPmSmcEngine;

    UINT32 RmApertureToHwAperture(UINT32 rmAperture);
};

//---------------------------------------------------------------------
//! \brief PmChannel subclass that wraps around a MODS Channel object
//!
class PmChannel_Mods : public PmChannel
{
public:
    PmChannel_Mods(PolicyManager *pPolicyManager, PmTest *pTest,
                   GpuDevice *pGpuDevice, const string &name,
                   bool createdByPm,
                   PmVaSpace* pPmVaSpace, LwRm* pLwRm);
    virtual ~PmChannel_Mods() { }
    virtual RC Write(UINT32 subchannel, UINT32 method, UINT32 data);
    virtual RC Write(UINT32 subchannel, UINT32 method,
                     UINT32 count, const UINT32 *pData);
    virtual RC SetPrivEnable(bool priv);
    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma);
    virtual RC SetSemaphoreOffset(UINT64 Offset);
    virtual void SetSemaphoreReleaseFlags(UINT32 flags);
    virtual UINT32 GetSemaphoreReleaseFlags();
    virtual RC SetSemaphorePayloadSize(Channel::SemaphorePayloadSize size);
    virtual Channel::SemaphorePayloadSize GetSemaphorePayloadSize();
    virtual bool Has64bitSemaphores();
    virtual RC SemaphoreRelease(UINT64 Data);
    virtual RC SemaphoreReduction(Channel::Reduction Op,
                                  Channel::ReductionType Type, UINT64 Data);
    virtual RC SemaphoreAcquire(UINT64 Data);
    virtual RC NonStallInterrupt();
    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();
    virtual RC Flush();
    virtual RC WaitForIdle();
    virtual RC WriteSetSubdevice(UINT32 Subdevice);
    virtual Channel *GetModsChannel() const;
    virtual LwRm::Handle GetHandle() const;
    virtual LwRm::Handle GetTsgHandle() const;
    virtual Type         GetType()   const;
    virtual UINT32       GetMethodCount() const;
    virtual bool         GetEnabled() const;
    virtual RC           SetEnabled(bool enabled);
    virtual bool         IsAtsEnabled() const;
    virtual VaSpace* GetVaSpace();
    virtual LwRm::Handle GetVaSpaceHandle() const;
    virtual shared_ptr<SubCtx> GetSubCtxInTsg(UINT32 veid) const;
    virtual shared_ptr<SubCtx> GetSubCtx() const;
    virtual LWGpuTsg    *GetTsg() const { return nullptr; }
    virtual UINT32 GetChannelInitMethodCount() const { return 0;}
    virtual RC RealCreateChannel(UINT32 engineId);
    virtual void SetChannel(Channel *pChannel);
    virtual UINT32       GetEngineId() const { return m_EngineId; }

private:
    Channel           *m_pChannel;
    Type               m_Type;
    PmVaSpace         *m_pPmVaSpace;
    UINT32                m_EngineId;
    ChannelAllocInfo      m_ChannelAllocInfo;//!< Helper class to print channel info
};

// The PM_BEGIN_INSERTED_METHODS() / PM_END_INSERTED_METHODS() macros
// are designed to be put around PolicyManager code that inserts
// methods into a pushbuffer.  They make it easier to ensure that the
// methods are bracketed by BeginInsertedMethods() and
// EndInsertedMethods, and that the code will call
// CancelInsertedMethods() if an error oclwrs between Begin and End.
//
// These macros assume that:
//
// - There's a one-to-one mapping between each
//   PM_BEGIN_INSERTED_METHODS statement and each
//   PM_END_INSERTED_METHODS.
// - PM_BEGIN_INSERTED_METHODS and PM_END_INSERTED_METHODS are called
//   in the same scope, i.e. don't put one in an "if" statement, and the
//   other outside.
// - All error-handling between PM_BEGIN_INSERTED_METHODS and
//   PM_END_INSERTED_METHODS is handled by CHECK_RC or CHECK_RC_CLEANUP.
//   In particular, don't put a "return" statement between the two
//   macros that returns OK.
//

#define PM_BEGIN_INSERTED_METHODS(pPmChannel)                   \
    {                                                           \
        PmChannel::InsertMethods insertMethods(pPmChannel);     \
        CHECK_RC(insertMethods.Begin());

#define PM_END_INSERTED_METHODS()                               \
        CHECK_RC(insertMethods.End());                          \
    }

#endif // INCLUDED_PMCHAN_H
