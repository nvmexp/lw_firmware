/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _CHANGRP_H_
#define _CHANGRP_H_

#include "gpu/tests/gputestc.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <map>
#include "core/include/channel.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/tsg.h"
#include "core/include/memoryif.h"

#include "class/cla06c.h"

//! \class ChannelGroup
//! \brief Encapsulates a single Channel Group (TSG)
class ChannelGroup : public Tsg
{
    public:
        ChannelGroup(GpuTestConfiguration *pTestConfig,
                     LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS *channelGroupParams = NULL);

        //! Configure group to use virtual contexts.  Must be called before Alloc
        void SetUseVirtualContext(bool bVirtCtx);
        bool GetUseVirtualContext() { return m_bVirtCtx; };

        //! Sets the location of the virtual context memory
        void SetVirtualContextLoc(Memory::Location loc) { m_ctxLoc = loc; };

        //! Free group's resources.
        virtual void Free();

        //! Allocates a channel to the group and returns a pointer in *ppCh if ppCh is non-null.
        RC AllocChannel(Channel **ppCh = NULL, LwU32 flags = 0);

        //! Allocates a channel to the group with a subcontext
        RC AllocChannelWithSubcontext(Channel **ppCh, Subcontext *pSubctx, LwU32 flags = 0);

        //! Frees a channel from the channel group
        void FreeChannel(Channel *pCh);

        //! Binds the group to a specific engine, input is LW2080_ENGINE_TYPE_*
        RC Bind(LwU32 engineType);

        //! Schedules the group on a runlist determined by the objects allocated on it.
        RC Schedule(bool bEnable = true, bool bSkipSubmit = false);

        //! Flushes pending work to hardware.
        RC Flush();

        //! Waits for all flushed work to be completed by hardware.
        RC WaitIdle();

        //! Preempts the channel group with (optional) wait
        RC Preempt(bool bWaitIdle = true);

        //! \class SplitMethodStream
        //! Simple class to take a method stream and split it between channels
        //! in a group, ensuring method exelwtion order in hardware.
        class SplitMethodStream
        {
            public:
                SplitMethodStream(ChannelGroup *chGrp);
                //! Allo instantiating on a single channel to make leveraging code easier.
                SplitMethodStream(Channel *pCh);
                ~SplitMethodStream();

                //! Writes a method to one of the channels in the stream.  It
                //! is guaranteed that methods will be seen by hardware in the
                //! order they are sent.
                RC Write(LwU32 subCh, LwU32 method, LwU32 data);
                RC WriteMultipleBegin(LwU32 subCh);
                RC WriteMultiple(LwU32 subCh, LwU32 method, LwU32 data);
                RC WriteMultipleEnd(LwU32 subCh);

                //! Writes a method to the designated channel within the channel group
                RC Write(LwRm::Handle hCh, LwU32 subCh, LwU32 method, LwU32 data);
                RC WritePerChannel(LwRm::Handle hCh, LwU32 subCh, LwU32 method, LwU32 data);

            private:
                ChannelGroup *m_chGrp;
                Channel *m_pCh;
                Surface2D m_semaSurf;
                LwU32 m_semaVal;
        };

    private:
        //! Allocates the group's virtual context
        RC AllocEngCtx(Channel *pCh, LwRm::Handle hCh);
        //! Initializes the group's virtual context
        RC InitializeEngCtx();
        //! Promotes the group's virtual context
        RC PromoteEngCtx();
        //! Evict the group's virtual context
        RC EvictEngCtx();
        //! Frees the group's virtual context
        void FreeEngCtx();

        bool m_bVirtCtx;
        bool m_bCtxAllocated;
        bool m_bCtxInitialiized;;
        bool m_bCtxPromoted;;
        Memory::Location m_ctxLoc;

        Surface2D m_engCtxMem;
};

#endif
