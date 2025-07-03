/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "core/include/utility.h"
#include "gpu/utility/semawrap.h"

#include "mdiag/utl/utlchannel.h"
#include "mdiag/utl/utlchanwrap.h"
#include "mdiag/utl/utlevent.h"

// Value that can be used for non-methods that still need to be counted
// for UTL method events.  This value is chosen so that all values between
// NO_METHOD and (NO_METHOD+maxCount) are all too large to be valid methods.
static UINT32 NO_METHOD = 0xf0000000;

UtlChannelWrapper::UtlChannelWrapper
(
    Channel* channel
)
    : ChannelWrapper(channel)
{
    MASSERT(IsSupported(channel));
}

bool UtlChannelWrapper::IsSupported(Channel* channel)
{
    UINT32 gpPut;

    // Only GpFifo channels are supported.
    if (channel->GetGpPut(&gpPut) == OK)
    {
        return true;
    }

    return false;
}

RC UtlChannelWrapper::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    RC rc;
    bool skipWrite = false;

    CHECK_RC(TriggerEvent(subchannel, method, data, false, 1, 1, &skipWrite));
    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->Write(subchannel, method, data));
    }
    ++m_TotalMethodsWritten;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        ++m_TotalTestMethodsWritten;
    }
    CHECK_RC(TriggerEvent(subchannel, method, data, true, 1, 1, &skipWrite));

    return rc;
}

RC UtlChannelWrapper::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32* dataArray
)
{
    RC rc;
    UINT32 i;
    bool skipWrite = false;
    
    for (i = 0; i < count; ++i)
    {
        CHECK_RC(TriggerEvent(
                subchannel,
                method + i * 4,
                dataArray[i],
                false,
                i + 1,
                count,
                &skipWrite));
    }

    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->Write(
                subchannel,
                method,
                count,
                dataArray));
    }
    m_TotalMethodsWritten += count;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        m_TotalTestMethodsWritten += count;
    }

    for (i = 0; i < count; ++i)
    {
        CHECK_RC(TriggerEvent(
                subchannel,
                method + i * 4,
                dataArray[i],
                true,
                i + 1,
                count,
                &skipWrite));
    }

    return rc;
}

RC UtlChannelWrapper::WriteNonInc
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32* dataArray
)
{
    RC rc;
    UINT32 i;
    bool skipWrite = false;

    for (i = 0; i < count; ++i)
    {
        CHECK_RC(TriggerEvent(
                subchannel,
                method,
                dataArray[i],
                false,
                i + 1,
                count,
                &skipWrite));
    }

    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->WriteNonInc(
                subchannel,
                method,
                count,
                dataArray));
    }
    m_TotalMethodsWritten += count;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        m_TotalTestMethodsWritten += count;
    }

    for (i = 0; i < count; ++i)
    {
        CHECK_RC(TriggerEvent(
                subchannel,
                method,
                dataArray[i],
                true,
                i + 1,
                count,
                &skipWrite));
    }

    return rc;
}

RC UtlChannelWrapper::WriteIncOnce
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32* dataArray
)
{
    RC rc;
    UINT32 i;
    bool skipWrite = false;

    for (i = 0; i < count; ++i)
    {
        UINT32 actualMethod = (i == 0) ? method : method + 4;
        CHECK_RC(TriggerEvent(
                subchannel,
                actualMethod,
                dataArray[i],
                false,
                i + 1,
                count,
                &skipWrite));
    }

    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->WriteIncOnce(
                subchannel,
                method,
                count,
                dataArray));
    }

    m_TotalMethodsWritten += count;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        m_TotalTestMethodsWritten += count;
    }

    for (i = 0; i < count; ++i)
    {
        UINT32 actualMethod = (i == 0) ? method : method + 4;
        CHECK_RC(TriggerEvent(
                subchannel,
                actualMethod,
                dataArray[i],
                true,
                i + 1,
                count,
                &skipWrite));
    }

    return rc;
}

RC UtlChannelWrapper::WriteImmediate
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    RC rc;
    bool skipWrite = false;

    CHECK_RC(TriggerEvent(subchannel, method, data, false, 1, 1, &skipWrite));
    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->WriteImmediate(subchannel, method, data));
    }
    ++m_TotalMethodsWritten;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        ++m_TotalTestMethodsWritten;
    }
    CHECK_RC(TriggerEvent(subchannel, method, data, true, 1, 1, &skipWrite));

    return rc;
}

RC UtlChannelWrapper::BeginInsertedMethods()
{
    RC rc;

    CHECK_RC(m_pWrappedChannel->BeginInsertedMethods());
    ++m_NestedInsertedMethods;

    return rc;
}

RC UtlChannelWrapper::EndInsertedMethods()
{
    MASSERT(m_NestedInsertedMethods > 0);
    --m_NestedInsertedMethods;

    return m_pWrappedChannel->EndInsertedMethods();
}

void UtlChannelWrapper::CancelInsertedMethods()
{
    MASSERT(m_NestedInsertedMethods > 0);
    --m_NestedInsertedMethods;
    m_pWrappedChannel->CancelInsertedMethods();
}

RC UtlChannelWrapper::WriteHeader
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count
)
{
    RC rc;

    CHECK_RC(m_pWrappedChannel->WriteHeader(subchannel, method, count));

    return rc;
}

RC UtlChannelWrapper::WriteNonIncHeader
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count
)
{
    RC rc;

    CHECK_RC(m_pWrappedChannel->WriteNonIncHeader(subchannel, method, count));

    return rc;
}

RC UtlChannelWrapper::WriteNop()
{
    RC rc;
    const UINT32 data = 0;
    bool skipWrite = false;

    CHECK_RC(TriggerEvent(0, NO_METHOD, data, false, 1, 1, &skipWrite));
    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->WriteNop());
    }
    ++m_TotalMethodsWritten;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        ++m_TotalTestMethodsWritten;
    }
    CHECK_RC(TriggerEvent(0, NO_METHOD, data, true, 1, 1, &skipWrite));

    return rc;
}

RC UtlChannelWrapper::WriteSetSubdevice(UINT32 mask)
{
    RC rc;
    const UINT32 data = 0;
    bool skipWrite = false;

    CHECK_RC(TriggerEvent(0, NO_METHOD, data, false, 1, 1, &skipWrite));
    if (!skipWrite)
    {
        CHECK_RC(m_pWrappedChannel->WriteSetSubdevice(mask));
    }
    ++m_TotalMethodsWritten;
    if ((m_NestedInsertedMethods == 0) &&
        GetUtlChannel() &&
        GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods())
    {
        ++m_TotalTestMethodsWritten;
    }
    CHECK_RC(TriggerEvent(0, NO_METHOD, data, true, 1, 1, &skipWrite));

    return rc;
}

// PK: This function was copied from PmChannelWrapper.  No idea why
// the function needs to be overridden as unsupported.
//
RC UtlChannelWrapper::SetCachedPut(UINT32)
{
    return RC::UNSUPPORTED_FUNCTION;
}

// PK: This function was copied from PmChannelWrapper.  No idea why
// the function needs to be overridden as unsupported.
//
RC UtlChannelWrapper::SetPut(UINT32)
{
    return RC::UNSUPPORTED_FUNCTION;
}

UtlChannelWrapper* UtlChannelWrapper::GetUtlChannelWrapper()
{
    return this;
}

// This function triggers a UtlMethodWriteEvent.
//
RC UtlChannelWrapper::TriggerEvent
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data,
    bool afterWrite,
    UINT32 groupIndex,
    UINT32 groupCount,
    bool* skipWrite
)
{
    RC rc;

    // Do nothing if this is an inserted method, rather than part of
    // the actual test.
    // or
    // The channel is an internal MODS channel (not registered with UTL)
    if (m_NestedInsertedMethods > 0 ||
        GetUtlChannel() == nullptr)
    {
        return OK;
    }

    // Check to make sure this function isn't called relwrsively,
    // which could theoretically cause an infinite loop.
    MASSERT(!m_Relwrsive);
    m_Relwrsive = true;

    Utility::CleanupValue<bool> m_RelwrsiveCleanup(m_Relwrsive, false);

    UtlMethodWriteEvent event;
    event.m_AfterWrite = afterWrite;
    event.m_Channel = GetUtlChannel();
    event.m_Method = method;
    event.m_Data = data;

    // Get the HW class.  Methods < 0x100 are host methods, in which
    // case the HW class is the channel class.  Methods >= 0x100 are
    // "ordinary" methods, in which case we need to query the
    // SemaphoreChannelWrapper to find the HW class associated with the
    // subchannel.

    if (method < 0x100)
    {
        event.m_HwClass = GetClass();
    }
    else
    {
        SemaphoreChannelWrapper* semaphoreChannelWrapper =
            GetWrapper()->GetSemaphoreChannelWrapper();
        MASSERT(semaphoreChannelWrapper);

        event.m_HwClass = semaphoreChannelWrapper->GetClassOnSubch(subchannel);
    }

    event.m_MethodsWritten = m_TotalMethodsWritten;
    event.m_TestMethodsWritten = m_TotalTestMethodsWritten;
    event.m_IsTestMethod = GetUtlChannel()->GetLwGpuChannel()->IsSendingTestMethods();
    event.m_GroupIndex = groupIndex;
    event.m_GroupCount = groupCount;

    try
    {
        event.Trigger();
    }

    // The Python interpreter can only return errors via exception.
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    if (event.m_SkipWrite)
    {
        *skipWrite = true;

        if (afterWrite)
        {
            ErrPrintf("Can't skip a method after it's already been written (event.afterWrite = True).");
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}
