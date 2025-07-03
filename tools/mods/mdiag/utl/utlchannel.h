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

#ifndef INCLUDED_UTLCHANNEL_H
#define INCLUDED_UTLCHANNEL_H

#include "core/include/types.h"
#include "core/include/channel.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "utlpython.h"
#include "utltsg.h"
#include "utlmmu.h"
#include "utlsubctx.h"
#include "mdiag/channelallocinfo.h"

class UtlChannelWrapper;
class UtlTest;
class UtlEngine;
class UtlSmcEngine;
class UtlRawMemory;
class UtlGpu;

enum ChannelType
{
    PIO,
    GPFIFO
};

// This class represents the C++ backing for the UTL Python Channel object.
// This class is a wrapper around LWGpuChannel.
//
class UtlChannel
{
public:
    static void Register(py::module module);

    static UtlChannel* Create
    (
        LWGpuChannel* channel,
        const string& name,
        vector<UINT32>* pSubChCEInsts
    );

    ~UtlChannel();

    // This is a dummy class for holding parameters on Python side.
    class ChannelConstants
    {
    };

    using PythonChannelConstants = py::class_<ChannelConstants>;

    static UtlChannel* GetFromChannelPtr(LWGpuChannel* channel);
    static void FreeChannels();
    static void FreeChannel(LWGpuChannel* channel);

    LwRm* GetLwRmPtr() { return m_Channel->GetLwRmPtr(); }
    LwRm::Handle GetHandle() const { return m_Channel->ChannelHandle(); }
    LwRm::Handle GetVaSpaceHandle() const { return m_Channel->GetVASpaceHandle(); }
    LWGpuChannel* GetLwGpuChannel() const { return m_Channel; }
    UtlTsg* GetTsg() const { return m_UtlTsg; }
    void SetTsg(UtlTsg* utlTsg) { m_UtlTsg = utlTsg; }
    UINT32 GetEngineId() { return m_Channel->GetEngineId(); }
    UINT32 GetChannelId();
    UINT32 GetPhysicalChannelId();
    UINT32 GetLogicalCopyEngineId();
    UtlSmcEngine* GetSmcEngine();
    UtlGpu* GetGpu();
    UINT32 GetDoorbellToken();
    bool GetDoorbellRingEnable();
    void SetDoorbellRingEnable(bool enable);
    static UtlChannel* GetChannelFromHandle(LwRm::Handle hObject);
    static UtlChannel* FindFirstEngineChannel(UINT32 engineId);

    // The following functions are called from the Python interpreter. as
    const string& GetName() const;
    int GetMethodCount() const;
    void SetMethodCount(UINT32 methodCount);
    void WriteMethods(py::kwargs kwargs);
    void Flush();
    void WaitForIdle();
    void CreateSubchannel(py::kwargs kwargs);
    static UtlChannel* CreatePy(py::kwargs kwargs);
    void Disable() { m_Channel->StopSendingMethod(); }
    void Reset();
    UtlEngine* GetEngine();
    vector<UINT32> GetSubchannelCEInstances();
    UtlMmu::APERTURE GetGmmuAperture(UINT32 aperture);
    UtlRawMemory* GetInstanceBlock();
    void CheckForErrors();
    void ClearError();
    UINT32 GetClass();
    UtlVaSpace* GetVaSpace(py::kwargs kwargs);
    void SetSubCtx(UtlSubCtx* subCtx) { m_pSubCtx = subCtx; }
    UtlSubCtx* GetSubCtx() { return m_pSubCtx; }
    ChannelType GetChannelType();
    void WriteSetSubdevice(py::kwargs kwargs);

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlChannel() = delete;
    UtlChannel(UtlChannel&) = delete;
    UtlChannel &operator=(UtlChannel&) = delete;

private:
    UtlChannel(LWGpuChannel* channel, const string& name, vector<UINT32>* pSubChCEInsts);
    void PrintChannelInfo();
    GpuDevice* GetGpuDevice();

    LWGpuChannel* m_Channel;
    string m_Name;
    UtlChannelWrapper* m_UtlChannelWrapper;

    // All UtlChannel objects are stored in this map.
    static map<LWGpuChannel*, unique_ptr<UtlChannel>> s_Channels;

    // This map holds pointers to all channels that are created by UTL scripts.
    // The first level of the map is referenced by either a test pointer
    // (for test-specific scripts) or nullptr, for global scripts.
    // The second level of the map is referenced by channel name.
    static map<UtlTest*, map<string, UtlChannel*>> s_ScriptChannels;

    // If this UtlChannel object was created via UTL script, then this
    // pointer will have ownership of the corresponding LWGpuChannel object.
    // If this UtlChannel was created with an existing LWGpuChannel pointer,
    // then this pointer will be null.
    unique_ptr<LWGpuChannel> m_OwnedLWGpuChannel;

    UtlTsg* m_UtlTsg;
    UtlSubCtx* m_pSubCtx;
    UtlEngine* m_pUtlEngine;
    vector<UINT32> m_SubChCEInsts;
    unique_ptr<UtlRawMemory> m_pInstanceBlock;

    unique_ptr<MdiagSurf> m_ErrorNotifier;
    unique_ptr<MdiagSurf> m_PushBuffer;
    unique_ptr<MdiagSurf> m_GpFifo;

    ChannelAllocInfo m_ChannelAllocInfo;
    
    struct SubchannelInfo
    {
        SubchannelInfo() : classNum(0) {}
        string name;
        UINT32 classNum;
    } m_SubchannelNames[Channel::NumSubchannels];
};

#endif
