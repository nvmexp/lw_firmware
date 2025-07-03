/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2015, 2017-2018, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracesubchan.h
 * @brief  TraceSubChannel: per-channel resources used by Trace3DTest.
 */
#ifndef INCLUDED_TRACESUBCHAN_H
#define INCLUDED_TRACESUBCHAN_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif
#ifndef INCLUDED_TYPES_H
#include "mdiag/utils/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#include "gputrace.h"
#include "tracemod.h"
#include "mdiag/utils/surf_types.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"

class BuffInfo;
typedef vector<TraceSubChannel*>  TraceSubChannelList;

//------------------------------------------------------------------------------
// TraceSubChannel -- holds per-subchannel resources for Trace3DTest.
//
class TraceSubChannel
{
public:
    TraceSubChannel(const string& name, BuffInfo* bif, const ArgReader* params);
    ~TraceSubChannel();

    enum
    {
        UNSPECIFIED_TRACE_SUBCHNUM = 0xFFFFFFFF
    };

    void                SetClass(UINT32 hwclass);
    LWGpuSubChannel*    GetSubCh() { return m_pSubCh; }
    LwRm*               GetLwRmPtr(); 
    UINT32              GetClass() const;
    UINT32              GetSubChNum() const {return m_SubChNum; }
    UINT32              GetTraceSubChNum() const { return m_TraceSubChNum; }
    void                SetTraceSubChNum(UINT32 trchnum) { m_TraceSubChNum = trchnum; }
    void                SetUseTraceSubchNum(bool useTrSubchNum) { m_UseTraceSubchNum = useTrSubchNum; }
    MethodMassager      GetMassager() const;
    string              GetName() const { return m_Name; }
    void                SetCeType(UINT32 traceCeNum) { m_CeType = traceCeNum; }
    void                SetLwEncOffset(UINT32 inst) { m_LwEncOffset = inst; }
    void                SetLwDecOffset(UINT32 inst) { m_LwDecOffset = inst; }
    void                SetLwJpgOffset(UINT32 offset) { m_LwJpgOffset = offset; }
    UINT32              GetCeType() const { return m_CeType; }
    UINT32              GetLwEncOffset() { return m_LwEncOffset; }
    UINT32              GetLwDecOffset() { return m_LwDecOffset; }
    UINT32              GetLwJpgOffset() { return m_LwJpgOffset; }

    RC                  AllocObject(GpuChannelHelper* pchelp);
    RC                  AllocSubChannel(LWGpuResource * pLwGpu);
    void                SetTrustedContext();
    RC                  CtrlTrustedContext();
    void                ForceWfiMethod(WfiMethod m);
    WfiMethod           GetWfiMethod() const;

    RC                  AllocNotifier
                        (
                            const ArgReader * params, 
                            const string& channelName
                        );
    void                ClearNotifier();
    RC                  SendNotifier();
    bool                PollNotifierDone();
    UINT16              PollNotifierValue();

    // If current subchannel is Gr subchannel
    bool                GrSubChannel() const;
    // If current subchannel type is I2MEM.
    bool                I2memSubChannel() const;
    // If current subchannel is compute subchannel
    bool                ComputeSubChannel() const;
    // If current subchannel is video subchannel
    bool                VideoSubChannel() const;
    // If current subchannel doesn't use notifier
    bool                NonNotifySubChannel() const;
    // If current subchannel is copy subchannel
    bool                CopySubChannel() const;

    // Gpu class number is passed around a lot only for the purpose of checking
    // if current channel/test is Fermi or not. GetClassFamily() is defined here
    // to simplify that. The return value usually should compare against
    // LWGpuClasses::GPU_CLASS_FERMI (0x9000).

    // This hack should be considered as a temporary solution. Ultimately each
    // family of gpu class deserves its own derived class.
    static UINT32       GetClassNum() { return s_ClassNum; }

    void SetIsDynamic() { m_IsDynamic = true; }
    bool IsDynamic() const { return m_IsDynamic; }
    void ClearIsValidInParsing() { m_IsValidInParsing = false; }
    bool IsValidInParsing() { return m_IsValidInParsing; }

    LWGpuResource * GetGpuResource() {return m_pLwGpuResource; }
    void SetWasUnspecifiedCE(bool wasUnspecifiedCE) { m_WasUnspecifiedCE = wasUnspecifiedCE; }
    bool GetWasUnspecifiedCE() { return m_WasUnspecifiedCE; }

private:
    RC                  SendMsdecSemaphore(UINT64 offset);
    RC                  SendCeSemaphore(UINT64 offset);
    RC                  AllocNotifier2SubDev
                        (
                            const ArgReader * params, 
                            const string& channelName, 
                            UINT32 subdev
                        );
    RC                  SendNotifier2SubDev(UINT32 subdev);
    RC                  SendIntrNotifier2SubDev(UINT32 subdev);
    RC                  SendNonstallIntr2SubDev(UINT32 subdev);
    bool                PollNotifierDone2SubDev(UINT32 subdev);

    LWGpuSubChannel*    m_pSubCh;
    UINT32              m_HwClass;          // subchannel class number
    int                 m_SubChNum;         // sub channel number allocate by mods
    UINT32              m_TraceSubChNum;    // sub channel number specified in trace
    bool                m_UseTraceSubchNum; // create subch on the num given in trace
    MethodMassager      m_Massager;         // method massaging function to use
    string              m_Name;
    bool                m_TrustedContext;   // object has trusted ctx enabled

    // Resources used for the final wait-for-idle (poll/notify/awaken).
    WfiMethod           m_WfiMethod;
    bool                m_UsingSemaphoreWfi;
    _DMA_TARGET         m_NotifierLoc;
    _PAGE_LAYOUT        m_NotifierLayout;
    vector<unique_ptr<MdiagSurf>>           m_Notifiers;
    vector<pair<ModsEvent*, LwRm::Handle>>  m_ModsEvents;

    BuffInfo*           m_BuffInfo;

    // ce type
    UINT32              m_CeType;

    UINT32 m_LwEncOffset;
    UINT32 m_LwDecOffset;
    UINT32 m_LwJpgOffset;

    LWGpuResource      *m_pLwGpuResource;
    UINT32              m_SubdevCount;
    const ArgReader*    m_Params;

    // gpu class family number to identify which family current running (tesla/fermi)
    static UINT32       s_ClassNum;

    bool m_IsDynamic; // create subchannel during running trace

    // A flag to identify whether the subchannel is valid in parse stage
    // It's invalid after FREE_SUBCHANNEL trace command is parsed.
    bool m_IsValidInParsing;
    bool m_WasUnspecifiedCE;
};

#endif // INCLUDED_TRACECHAN_H

