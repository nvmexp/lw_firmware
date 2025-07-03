/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/utils/types.h"
#include "mailbox.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"

#include <stdio.h>

namespace T3dPlugin
{
MailBox::StringPool *MailBox::s_StringPool[MAX_CORES];

bool MailBox::IsGlobalShutdown()
{
    return Platform::PhysRd32(MailBoxGlobalCtrlAddr) == (UINT32)CTRL_DONE;
}

void MailBox::GlobalShutdown()
{
    Platform::PhysWr32(MailBoxGlobalCtrlAddr, (UINT32)CTRL_DONE);
}

class MailBox_impl : public MailBox
{
public:
    friend MailBox *MailBox::Create(UINT32 mailBoxBase, UINT32 coreId, UINT32 threadId);

    explicit MailBox_impl(UINT32 mailBoxBase, UINT32 coreId, UINT32 threadId)
    : MailBox(mailBoxBase, coreId, threadId)
    {
    }

    virtual ~MailBox_impl()
    {
    }

protected:
    virtual void Yield()
    {
        Tasker::Yield();
    }

    virtual void MailBoxRead(UINT32 mailBoxOffset, UINT32 size, void *dst)
    {
        Platform::PhysRd(m_MailBoxBase + mailBoxOffset, dst, size);
    }

    virtual void MailBoxWrite(UINT32 mailBoxOffset, UINT32 size, const void *src)
    {
        Platform::PhysWr(m_MailBoxBase + mailBoxOffset, src, size);
    }
};

MailBox *MailBox::Create(UINT32 mailBoxBase, UINT32 coreId, UINT32 threadId)
{
    MailBox_impl *mailBox = new MailBox_impl(mailBoxBase, coreId, threadId);
    return mailBox;
}

} // namespace T3dPlugin
