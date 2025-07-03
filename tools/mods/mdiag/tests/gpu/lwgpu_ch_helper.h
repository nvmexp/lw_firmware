/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2018, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LWGPU_CH_HELPER_H_
#define _LWGPU_CH_HELPER_H_

class LWGpuResource;
class LWGpuChannel;
class LWGpuSubChannel;

#include "mdiag/tests/test_state_report.h"
class ArgReader;
class Test;

// How a test waits for idle
enum WfiMethod
{
    WFI_POLL,   // wait for idle by polling chip registers
    WFI_NOTIFY, // wait for idle by polling a notifier
    WFI_INTR,   // wait for idle by sleeping until a notifier interrupt arrives
    WFI_SLEEP,  // wait for idle by sleeping 10 seconds
    WFI_HOST,   // wait for idle by host semaphore
    WFI_INTR_NONSTALL// wait for idle by sleeping until a nonstall interrupt arrives
};

class GpuChannelHelper {
public:
    virtual int ParseChannelArgs(const ArgReader* params) = 0;
    virtual int ForceCtxswitch(bool mode) = 0;
    virtual bool IsCtxswitch() = 0;
    virtual int SetupChannelNoPush(LWGpuChannel* ch) = 0;

public:
    virtual ~GpuChannelHelper() {}

    virtual void set_class_ids(unsigned num_classes, const UINT32 class_ids[]) = 0;
    virtual bool acquire_gpu_resource() = 0;
    virtual void release_gpu_resource() = 0;

    virtual bool acquire_channel() = 0;
    virtual void release_channel() = 0;
    virtual bool alloc_channel(UINT32 engineId, UINT32 hwClass = 0) = 0;

    virtual LWGpuSubChannel *create_object(UINT32 id, void *params) = 0;
    virtual void reassign_subchannel_number(LWGpuSubChannel *subch, int old_num) = 0;

    virtual LWGpuResource* gpu_resource() const = 0;
    virtual LWGpuChannel* channel() const = 0;
    virtual const char* testname() const = 0;

    virtual void SetMethodCount(int num_mth) = 0;
    virtual void SetMethodWriteCount(int num_write) = 0;
    virtual void BeginChannelSwitch() = 0;
    virtual void EndChannelSwitch() = 0;

    virtual bool subchannel_switching_enabled() const = 0;
    virtual void notify_method_write(LWGpuChannel* ch, int subch, UINT32 method, MethodType mode) = 0;
    virtual bool has_subchannel_switch_event() const = 0;
    virtual bool next_thread_is_on_same_channel() const = 0;
    virtual int getCannotCtxsw() {return 0;};
    virtual unsigned method_count() const = 0;
    virtual bool is_channel_setup() const = 0;
    virtual void SetSmcEngine(SmcEngine* pSmcEngine) = 0;
    virtual void SetLwRmPtr(LwRm* pLwRm) = 0;

    // TODO: make this an accessor
    static const ParamDecl Params[];

    // get helper associated with Thread::LwrrentThread
    static GpuChannelHelper *LwrrentChannelHelper();

    // return ctxswitch enable status according to cmd parameters
    static bool IsCtxswEnabledByCmdline(const ArgReader* params);
};

GpuChannelHelper *mk_GpuChannelHelper(const char    *name,
                                      LWGpuResource *pLwGpu,
                                      Test *pTest, 
                                      LwRm *pLwRm,
                                      SmcEngine *pSmcEngine = nullptr);

#endif
