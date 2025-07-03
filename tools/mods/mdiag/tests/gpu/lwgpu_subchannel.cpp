/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include <iostream>
#include <map>
#include <algorithm>

#include "lwgpu_subchannel.h"

#include "mdiag/utils/buf.h"
#include "mdiag/utils/types.h"
#include "mdiag/utils/crc.h"

#include "lwgpu_single.h"
#include "mdiag/tests/test.h"
#include "mdiag/tests/testdir.h"

#include "core/include/channel.h"

DEFINE_MSG_GROUP(Mdiag, Gpu, ChannelOp, false);
#define MSGID() __MSGID__(Mdiag, Gpu, ChannelOp)

namespace
{
    class SubChannel : public LWGpuSubChannel
    {
        public:
            SubChannel(GpuChannelHelper*, int, UINT32, UINT32);
            ~SubChannel();

            bool set_object();

            RC MethodWriteRC(UINT32 method, UINT32 data);
            RC MethodWriteN_RC(UINT32 method_start, unsigned count,
                               const UINT32 *datap, MethodType mode = INCREMENTING);
            RC WriteSetSubdevice(UINT32 data);

            RC CallSubroutine(UINT64 get, UINT32 length);
            RC WriteHeader(UINT32 method, UINT32 count);
            RC WriteNonIncHeader(UINT32 method, UINT32 count);

            LWGpuChannel* channel() const { return m_channel_helper->channel(); }
            UINT32 object_handle() const { return m_object_handle; }
            int subchannel_number() const { return m_subchannel; }
            UINT32 get_classid() const { return m_classid; }

            virtual RC GetEngineType(UINT32* pRtnEngineType) const override;

        private:
            GpuChannelHelper *m_channel_helper;
            int m_subchannel;
            UINT32 m_object_handle;
            UINT32 m_classid;
            bool m_object_is_bound;
            bool m_object_is_owned;
    };

    SubChannel::SubChannel(GpuChannelHelper *ch_helper, int subchNum,
                           UINT32 subchClass, UINT32 subchObjHandle) :
        m_channel_helper(ch_helper),
        m_subchannel(subchNum),
        m_object_handle(subchObjHandle),
        m_classid(subchClass),
        m_object_is_bound(false),
        m_object_is_owned(true)
    {
        assert(ch_helper);
    }

    SubChannel::~SubChannel()
    {
        if (m_object_is_owned && m_object_handle &&
            (m_object_is_bound || channel()->GetIsGpuManaged()))
        {
            channel()->DestroyObject(m_object_handle);
        }
        m_channel_helper->reassign_subchannel_number(0, m_subchannel);
    }

    bool SubChannel::set_object()
    {
        assert(m_object_is_owned);
        m_object_is_bound = channel()->SetObject(m_subchannel,m_object_handle) != 0;
        return m_object_is_bound;
    }

    RC SubChannel::MethodWriteRC(UINT32 method, UINT32 data)
    {
        assert(channel());
        DebugPrintf(MSGID(), "%s: SubChannel::MethodWrite : ch=%d subch=%d; addr=0x%04x, data=0x%08x\n", m_channel_helper->testname(),
                channel()->ChannelNum(), m_subchannel, method, data);

        return channel()->MethodWriteRC(m_subchannel, method, data);
    }

    RC SubChannel::MethodWriteN_RC(UINT32 method_start, unsigned count, const UINT32 *datap, MethodType mode)
    {
        assert(channel());

        DebugPrintf(MSGID(), "%s: SubChannel::MethodWriteN: ch=%d subch=%d; addr=0x%04x, data=", m_channel_helper->testname(),
                channel()->ChannelNum(), m_subchannel, method_start);
        for (unsigned i = 0; i < count; ++i)
        {
            RawDbgPrintf(MSGID(), "0x%08x ", *(datap+i));
        }
        RawDbgPrintf(MSGID(), "\n");

        return channel()->MethodWriteN_RC(m_subchannel, method_start, count, datap, mode);
    }

    RC SubChannel::WriteSetSubdevice(UINT32 data)
    {
        assert(channel());
        DebugPrintf(MSGID(), "%s: SubChannel::WriteSetSubdevice(0x%08x) on ch=%d subch=%d\n", m_channel_helper->testname(),
                data, channel()->ChannelNum(), m_subchannel);

        return channel()->WriteSetSubdevice(data);
    }

    RC SubChannel::CallSubroutine(UINT64 get, UINT32 length)
    {
        assert(channel());
        DebugPrintf(MSGID(), "%s: SubChannel::CallSubroutine(0x%llx, 0x%08x) on ch=%d subch=%d\n",
            m_channel_helper->testname(), get, length, channel()->ChannelNum(), m_subchannel);

        Channel *pCh = channel()->GetModsChannel();

        return pCh->CallSubroutine(get, length);
    }

    RC SubChannel::WriteHeader(UINT32 method, UINT32 count)
    {
        assert(channel());
        DebugPrintf(MSGID(), "%s: SubChannel::WriteHeader(0x%08x, 0x%08x) on ch=%d subch=%d\n", m_channel_helper->testname(),
            method, count, channel()->ChannelNum(), m_subchannel);

        Channel *pCh = channel()->GetModsChannel();

        return pCh->WriteHeader(m_subchannel, method, count);
    }

    RC SubChannel::WriteNonIncHeader(UINT32 method, UINT32 count)
    {
        assert(channel());
        DebugPrintf(MSGID(), "%s: SubChannel::WriteNonIncHeader(0x%08x, 0x%08x) on ch=%d subch=%d\n", m_channel_helper->testname(),
            method, count, channel()->ChannelNum(), m_subchannel);

        Channel *pCh = channel()->GetModsChannel();

        return pCh->WriteNonIncHeader(m_subchannel, method, count);
    }

    RC SubChannel::GetEngineType(UINT32* pRtnEngineType) const
    {
        LWGpuChannel* lwgpuChannel = channel();
        assert(lwgpuChannel);

        return lwgpuChannel->GetEngineTypeByHandle(object_handle(), pRtnEngineType);
    }
}

LWGpuSubChannel *mk_SubChannel(GpuChannelHelper *helper, int subch_num,
                               UINT32 subch_class, UINT32 subch_objhandle)
{
    return new SubChannel(helper, subch_num, subch_class, subch_objhandle);
}
