/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef H_LWGPU_SUBCHANNEL_H
#define H_LWGPU_SUBCHANNEL_H

#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/lwgpu_channel.h"

class ArgReader;
class LWGpuResource;
class LWGpuChannel;
class LWGpuSubChannel;
class GpuChannelHelper;

LWGpuSubChannel *mk_SubChannel(GpuChannelHelper *helper, int subch_num,
                               UINT32 subch_class, UINT32 subch_objhandle);

class LWGpuSubChannel
{
public:
    virtual ~LWGpuSubChannel() {}

    virtual bool set_object() = 0; // uses obj created with create_object

    virtual RC MethodWriteRC(UINT32 method, UINT32 data) = 0;
    virtual RC MethodWriteN_RC(UINT32 method_start, unsigned count,
                               const UINT32 *datap, MethodType mode = INCREMENTING) = 0;
    virtual RC WriteSetSubdevice(UINT32 data) = 0;

    virtual RC CallSubroutine(UINT64 get, UINT32 length) = 0;
    virtual RC WriteHeader(UINT32 method, UINT32 count) = 0;
    virtual RC WriteNonIncHeader(UINT32 method, UINT32 count) = 0;

    virtual LWGpuChannel* channel() const = 0;
    virtual UINT32 object_handle() const = 0;
    virtual UINT32 get_classid() const = 0;
    virtual int subchannel_number() const = 0;

    virtual RC GetEngineType(UINT32* pRtnEngineType) const = 0;
};

extern LWGpuSubChannel* get_subchannel_object(LWGpuChannel* ch, int subch);

#endif
