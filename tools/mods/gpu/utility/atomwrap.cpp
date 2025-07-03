/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "atomwrap.h"
#include "core/include/utility.h"

//--------------------------------------------------------------------
//! \brief constructor
//!
AtomChannelWrapper::AtomChannelWrapper
(
    Channel *pChannel
) :
    ChannelWrapper(pChannel),
    m_InsertNestingLevel(0),
    m_AtomFsm(pChannel),
    m_Relwrsive(false),
    m_IsAtomExelwtion(false)
{
    MASSERT(pChannel);
}

//--------------------------------------------------------------------
//! \brief Tell whether AtomChannelWrapper supports the indicated channel
//!
//! If this static method returns false, you should not try to
//! construct an AtomChannelWrapper around the indicated channel.
//!
bool AtomChannelWrapper::IsSupported(Channel *pChannel)
{
    return AtomFsm::IsSupported(pChannel);
}

//--------------------------------------------------------------------
//! \brief Return a pointer to the AtomFsm, or NULL.
//!
//! This method is used when the caller needs to "probe ahead" to
//! detect upcoming atoms, before passing the methods to the channel.
//! This method returns a pointer to the internal AtomFsm, or NULL if
//! we're at a stage in which AtomFsm can't be reliably used (e.g. in
//! the middle of inserted methods).
//!
//! The idea is that the caller can copy the AtomFsm, and check for
//! upcoming atom without modifying the ChannelWrapper's AtomFsm.
//!
//! The main purpose of this function is to let trace3d determine
//! whether it can context-switch inside a Channel::Write().  A
//! context switch forces a Flush, which cannot be postponed by
//! AtomChannelWrapper.  This can break a test in runlist mode, since
//! a Flush writes a backend semaphore.
//!
const AtomFsm *AtomChannelWrapper::GetPeekableFsm() const
{
    if (m_InsertNestingLevel > 0)
        return NULL;

    return &m_AtomFsm;
}

//--------------------------------------------------------------------
//! \brief Cancel the pending atom
//!
//! If an atom is lwrrently enqueued, then this method ends the atom
//! and passes it to the wrapped channel.
//!
//! The caller normally doesn't need to call this method explicitly;
//! this class can usually detect the end of an atom without
//! CancelAtom().  The problem is that this class sometimes can't tell
//! that an atom ends until the next method arrives.  For example, a
//! macro can have an arbitrary number of MME_DATA methods, so this
//! class can't tell that the macro is done until it sees a method
//! other than MME_DATA.  If MME_DATA was the last method in the test,
//! then this class won't know that the atom is done.
//!
//! The end-of-test scenario is usually handled by WaitIdle(), which
//! does an implicit CancelAtom().  But trace3d sometimes cirlwmvents
//! WaitIdle(), in which case it needs to explicitly call CancelAtom()
//! at the end of the test.
//!
/* virtual */ RC AtomChannelWrapper::CancelAtom()
{
    return HandleIncomingOp(CANCEL_ATOM, 0, 0, 0, nullptr);
}

//--------------------------------------------------------------------
//! \brief Tell the channel that we are about to insert "extra"
//! methods into the pushbuffer.
//!
//! \sa EndInsertedMethods()
//!
/* virtual */ RC AtomChannelWrapper::BeginInsertedMethods()
{
    RC rc;
    ++m_InsertNestingLevel;
    rc = m_pWrappedChannel->BeginInsertedMethods();
    if (rc != OK)
    {
        --m_InsertNestingLevel;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Tell the channel that we are are done inserting "extra"
//! methods into the pushbuffer for now.
//!
//! \sa BeginInsertedMethods()
//!
/* virtual */ RC AtomChannelWrapper::EndInsertedMethods()
{
    MASSERT(m_InsertNestingLevel > 0);
    RC rc = m_pWrappedChannel->EndInsertedMethods();
    --m_InsertNestingLevel;
    return rc;
}

//--------------------------------------------------------------------
//! Cancel a pending BeginInsertedMethods/EndInsertedMethods in case
//! of error.
//!
//! \sa BeginInsertedMethods()
//! \sa EndInsertedMethods()
//!
/* virtual */ void AtomChannelWrapper::CancelInsertedMethods()
{
    MASSERT(m_InsertNestingLevel > 0);
    m_pWrappedChannel->CancelInsertedMethods();
    --m_InsertNestingLevel;
}

//--------------------------------------------------------------------
//! \brief Set the object on a subchannel
//!
/* virtual */ RC AtomChannelWrapper::SetObject
(
    UINT32 subchannel,
    LwRm::Handle handle
)
{
    m_AtomFsm.SetObject(subchannel, handle);
    return m_pWrappedChannel->SetObject(subchannel, handle);
}

//--------------------------------------------------------------------
//! \brief Unset the object on a subchannel
//!
/* virtual */ RC AtomChannelWrapper::UnsetObject
(
    LwRm::Handle handle
)
{
    m_AtomFsm.UnsetObject(handle);
    return m_pWrappedChannel->UnsetObject(handle);
}

//
// Override of the various Write methods to call HandleIncomingOp().
//

/* virtual */ RC AtomChannelWrapper::Write
(
    UINT32 subch,
    UINT32 method,
    UINT32 data
)
{
    return HandleIncomingOp(WRITE_INC, subch, method, 1, &data);
}

/* virtual */ RC AtomChannelWrapper::Write
(
    UINT32 subch,
    UINT32 method,
    UINT32 count,
    UINT32 data,
    ...
)
{
    MASSERT(count > 0);
    va_list pArgs;
    vector<UINT32> dataVector(count);

    va_start(pArgs, data);
    dataVector[0] = data;
    for (UINT32 ii = 1; ii < count; ++ii)
    {
        dataVector[ii] = va_arg(pArgs, UINT32);
    }
    va_end(pArgs);

    return HandleIncomingOp(WRITE_INC, subch, method, count, &dataVector[0]);
}

/* virtual */ RC AtomChannelWrapper::Write
(
    UINT32 subch,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return HandleIncomingOp(WRITE_INC, subch, method, count, pData);
}

/* virtual */ RC AtomChannelWrapper::WriteNonInc
(
    UINT32 subch,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return HandleIncomingOp(WRITE_NON_INC, subch, method, count, pData);
}

/* virtual */ RC AtomChannelWrapper::WriteHeader
(
    UINT32 subch,
    UINT32 method,
    UINT32 count
)
{
    return HandleIncomingOp(WRITE_INC_HEADER, subch, method, count, nullptr);
}

/* virtual */ RC AtomChannelWrapper::WriteNonIncHeader
(
    UINT32 subch,
    UINT32 method,
    UINT32 count
)
{
    return HandleIncomingOp(WRITE_NON_INC_HEADER, subch,
                            method, count, nullptr);
}

/* virtual */ RC AtomChannelWrapper::WriteIncOnce
(
    UINT32 subch,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return HandleIncomingOp(WRITE_INC_ONCE, subch, method, count, pData);
}

/* virtual */ RC AtomChannelWrapper::WriteImmediate
(
    UINT32 subch,
    UINT32 method,
    UINT32 data
)
{
    return HandleIncomingOp(WRITE_IMM, subch, method, 1, &data);
}

/* virtual */ RC AtomChannelWrapper::WriteNop()
{
    return HandleIncomingOp(WRITE_NOP, 0, 0, 0, nullptr);
}

/* virtual */ RC AtomChannelWrapper::WriteSetSubdevice(UINT32 mask)
{
    return HandleIncomingOp(WRITE_SET_SUBDEVICE, 0, mask, 0, nullptr);
}

/* virtual */ RC AtomChannelWrapper::CallSubroutine
(
    UINT64 offset,
    UINT32 size
)
{
    return HandleIncomingOp(CALL_SUBROUTINE,
                            LwU64_HI32(offset), LwU64_LO32(offset),
                            size, nullptr);
}

/* virtual */ RC AtomChannelWrapper::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    return HandleIncomingOp(INSERT_SUBROUTINE, 0, 0, count, pBuffer);
}

/* virtual */ RC AtomChannelWrapper::SetPrivEnable(bool priv)
{
    return HandleIncomingOp(SET_PRIV_ENABLE, 0, priv, 0, nullptr);
}

/* virtual */ RC AtomChannelWrapper::SetSyncEnable(bool sync)
{
    return HandleIncomingOp(SET_SYNC_ENABLE, 0, sync, 0, nullptr);
}

//--------------------------------------------------------------------
//! \brief Wait for the channel to go idle
//!
/* virtual */ RC AtomChannelWrapper::WaitIdle(FLOAT64 timeoutMs)
{
    RC rc;
    CHECK_RC(CancelAtom());
    CHECK_RC(m_pWrappedChannel->WaitIdle(timeoutMs));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Clear the pushbuffer
//!
//! This cancels any pending queued atomic operations and inserted
//! methods that started before the pushbuffer was erased.
//!
/* virtual */ void AtomChannelWrapper::ClearPushbuffer()
{
    m_pWrappedChannel->ClearPushbuffer();
    m_AtomOps.clear();
    m_AtomFsm.CancelAtom();
}

//--------------------------------------------------------------------
//! \brief Return this object
//!
/* virtual */ AtomChannelWrapper *AtomChannelWrapper::GetAtomChannelWrapper()
{
    return this;
}

bool AtomChannelWrapper::IsAtomExelwtion() const
{
    return m_IsAtomExelwtion;
}

//--------------------------------------------------------------------
//! \brief Execute or enqueue a ChannelOp, as appropriate
//!
//! This subroutine is called by the various Write() subroutines that
//! append to the pushbuffer.  Normally, this subroutine does the same
//! thing as a "normal" write: it appends the method to the
//! pushbuffer.  But if the caller is writing an atomic operation,
//! then this subroutine enqueues the atom as well as any inserted
//! methods that are written in the middle of the atom.  Once the atom
//! is done, it writes the atom followed by the inserted methods.
//!
RC AtomChannelWrapper::HandleIncomingOp
(
    OpType type,
    UINT32 subch,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    RC rc;

    // Sanity-check the arguments
    //
    switch (type)
    {
        case WRITE_INC:
        case WRITE_NON_INC:
        case WRITE_INC_ONCE:
            MASSERT(count > 0);
            MASSERT(pData != nullptr);
            break;
        case WRITE_IMM:
            MASSERT(count == 1);
            MASSERT(pData != nullptr);
            break;
        case WRITE_INC_HEADER:
        case WRITE_NON_INC_HEADER:
            MASSERT(count > 0);
            MASSERT(pData == nullptr);
            break;
        case WRITE_NOP:
        case WRITE_SET_SUBDEVICE:
        case SET_PRIV_ENABLE:
        case SET_SYNC_ENABLE:
        case CANCEL_ATOM:
            MASSERT(count == 0);
            MASSERT(pData == nullptr);
            break;
        case CALL_SUBROUTINE:
            MASSERT(pData == nullptr);
            break;
        case INSERT_SUBROUTINE:
            MASSERT(pData != nullptr);
            break;
    }

    // Append the incoming op to the end of m_IncomingOps, and then
    // process the queue.
    //
    ChannelOp op;
    op.type   = type;
    op.subch  = subch;
    op.method = method;
    op.count  = count;
    if (pData)
    {
        op.data.assign(pData, pData + count);
    }
    op.aType = DISABLE;
    m_IncomingOps.push_back(op);
    CHECK_RC(HandleQueuedIncomingOps());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Handle all ops enqueued by HandleIncomingOp()
//!
//! Most of the time, the m_IncomingOps queue is unnecessary.
//! HandleIncomingOp() pushes one op (eg a Write() method) on the
//! queue, it calls this function, and this function handles the
//! queued op.
//!
//! Things get a bit more complicated when HandleIncomingOp() gets
//! called relwrsively.  The underlying channel may relwrsively do new
//! writes, especially if this class reaches the end of an atom and
//! sends down a bunch of queued methods at once.  If that happens, we
//! want to make sure that we completely finish processing the current
//! op before going onto the next one.
//!
RC AtomChannelWrapper::HandleQueuedIncomingOps()
{
    RC rc;

    // Relwrsion protection, in case the caller is already looping
    // through m_IncomingOps.  Just return in that case; we'll finish
    // processing m_IncomingOps once we return to the caller.
    //
    if (m_Relwrsive)
        return OK;
    Utility::TempValue<bool> setRelwrsive(m_Relwrsive, true);

    while (!m_IncomingOps.empty())
    {
        // Pop one op from the queue
        //
        const ChannelOp op = m_IncomingOps.front();
        m_IncomingOps.pop_front();

        // Determine how we will handle this operation
        //
        bool doEnqueue = false;  // Enqueue this op or execute it immediately
        bool doWriteQueueBefore = false;  // If true, call WriteQueue() first
        bool doWriteQueueAfter = false;   // If true, call WriteQueue() after

        if (m_InsertNestingLevel > 0)
        {
            doEnqueue = false;
            doWriteQueueBefore = false;
            doWriteQueueAfter = false;
        }
        else
        {
            switch (op.type)
            {
                case WRITE_INC:
                case WRITE_NON_INC:
                case WRITE_INC_ONCE:
                case WRITE_IMM:
                    switch (m_AtomFsm.PeekMethod(op.subch, op.method,
                                                 op.data[0]))
                    {
                        case AtomFsm::NOT_IN_ATOM:
                        case AtomFsm::IN_NEW_ATOM:
                            doWriteQueueBefore = !m_AtomOps.empty();
                            break;
                        case AtomFsm::IN_ATOM:
                        case AtomFsm::ATOM_ENDS_AFTER:
                            doWriteQueueBefore = false;
                            break;
                    }
                    doEnqueue = false;
                    for (UINT32 ii = 0; ii < op.count; ++ii)
                    {
                        UINT32 method = op.method + (
                                op.type == WRITE_INC ? 4 * ii :
                                op.type == WRITE_INC_ONCE && ii > 0 ? 4 :
                                0);
                        switch (m_AtomFsm.EatMethod(op.subch, method,
                                                    op.data[ii]))
                        {
                            case AtomFsm::NOT_IN_ATOM:
                                // doEnqueue = <unchanged>;
                                doWriteQueueAfter = doEnqueue;
                                break;
                            case AtomFsm::IN_ATOM:
                                doEnqueue = true;
                                doWriteQueueAfter = false;
                                break;
                            case AtomFsm::IN_NEW_ATOM:
                                doEnqueue = true;
                                doWriteQueueAfter = false;
                                break;
                            case AtomFsm::ATOM_ENDS_AFTER:
                                doEnqueue = true;
                                doWriteQueueAfter = true;
                                break;
                        }
                    }
                    break;
                case WRITE_INC_HEADER:
                case WRITE_NON_INC_HEADER:
                    doEnqueue = true;
                    doWriteQueueAfter = false;
                    switch (m_AtomFsm.EatHeader(
                                    op.subch, op.method, op.count,
                                    op.type == WRITE_NON_INC_HEADER))
                    {
                        case AtomFsm::IN_NEW_ATOM:
                            doWriteQueueBefore = !m_AtomOps.empty();
                            break;
                        case AtomFsm::IN_ATOM:
                            doWriteQueueBefore = false;
                            break;
                        default:
                            // Should be impossible to get here.  A header
                            // and the subsequent data are inherently atomic.
                            MASSERT(!"Illegal case");
                            return RC::SOFTWARE_ERROR;
                    }
                    break;
                case WRITE_NOP:
                case WRITE_SET_SUBDEVICE:
                case CANCEL_ATOM:
                    // These operations can't be part of an atom
                    doEnqueue = false;
                    doWriteQueueBefore = !m_AtomOps.empty();
                    doWriteQueueAfter = false;
                    m_AtomFsm.CancelAtom();
                    break;
                case CALL_SUBROUTINE:
                case INSERT_SUBROUTINE:
                    switch (m_AtomFsm.EatSubroutine(op.type == CALL_SUBROUTINE ?
                                                    op.count :
                                                    op.count * sizeof(UINT32)))
                    {
                        case AtomFsm::NOT_IN_ATOM:
                            doEnqueue = false;
                            doWriteQueueBefore = !m_AtomOps.empty();
                            doWriteQueueAfter = false;
                            break;
                        case AtomFsm::IN_ATOM:
                            doEnqueue = true;
                            doWriteQueueBefore = false;
                            doWriteQueueAfter = false;
                            break;
                        case AtomFsm::ATOM_ENDS_AFTER:
                            doEnqueue = true;
                            doWriteQueueBefore = false;
                            doWriteQueueAfter = true;
                            break;
                        default:
                            // Should be impossible to get here.
                            // IN_NEW_ATOM is only case left, and we
                            // can't detect the start of an atom in a
                            // subroutine because we don't parse it.
                            //
                            MASSERT(!"Illegal case");
                            return RC::SOFTWARE_ERROR;
                    }
                    break;
                case SET_PRIV_ENABLE:
                case SET_SYNC_ENABLE:
                    // Enqueue with pending atom, if any
                    doEnqueue = !m_AtomOps.empty();
                    doWriteQueueBefore = false;
                    doWriteQueueAfter = false;
                    break;
            }
        }

        // Handle the operation, possibly writing the queue before or
        // afterwards if we hit the boundary of an atom.
        //
        if (doWriteQueueBefore)
        {
            CHECK_RC(WriteQueue());
        }

        if (doEnqueue)
        {
            m_AtomOps.push_back(op);
        }
        else
        {
            CHECK_RC(ExelwteOp(op));
        }

        if (doWriteQueueAfter)
        {
            CHECK_RC(WriteQueue());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Execute a ChannelOp
//!
//! This subroutine exelwtes the indicated operation -- usually a
//! Write() -- on the wrapped channel.
//!
RC AtomChannelWrapper::ExelwteOp(const ChannelOp &op)
{
    RC rc;
    Channel *pCh = m_pWrappedChannel;
    switch (op.type)
    {
        case WRITE_INC:
            if (op.count == 1)
                return pCh->Write(op.subch, op.method, op.data[0]);
            else
                return pCh->Write(op.subch, op.method, op.count, &op.data[0]);
        case WRITE_NON_INC:
            return pCh->WriteNonInc(op.subch, op.method,
                                    op.count, &op.data[0]);
        case WRITE_INC_ONCE:
            return pCh->WriteIncOnce(op.subch, op.method,
                                     op.count, &op.data[0]);
        case WRITE_IMM:
            return pCh->WriteImmediate(op.subch, op.method, op.data[0]);
        case WRITE_INC_HEADER:
            return pCh->WriteHeader(op.subch, op.method, op.count);
        case WRITE_NON_INC_HEADER:
            return pCh->WriteNonIncHeader(op.subch, op.method, op.count);
        case WRITE_NOP:
            return pCh->WriteNop();
        case WRITE_SET_SUBDEVICE:
            return pCh->WriteSetSubdevice(op.method);
        case CALL_SUBROUTINE:
            return pCh->CallSubroutine(
                (static_cast<UINT64>(op.subch) << 32) | op.method,
                op.count);
        case INSERT_SUBROUTINE:
            return pCh->InsertSubroutine(&op.data[0], op.count);
        case SET_PRIV_ENABLE:
            return pCh->SetPrivEnable(op.method != 0);
        case SET_SYNC_ENABLE:
            return pCh->SetSyncEnable(op.method != 0);
        case CANCEL_ATOM:
            return OK;  // Do nothing
    }
    MASSERT(!"Should never get here");
    return RC::SOFTWARE_ERROR;
}

//--------------------------------------------------------------------
//! \brief Write the queued atom in m_AtomOps to the pushbuffer
//!
RC AtomChannelWrapper::WriteQueue()
{
    RC rc;
    Channel *pCh = m_pWrappedChannel;

    // Consolidate the methods in m_AtomOps so that the methods get
    // combined into a big InsertSubroutine.
    //
    // This should cause most atoms to get sent as a single
    // InsertSubroutine, which helps guarantee that they're added to
    // the pushBuffer atomically.  But there are some queued calls
    // that can't be part of InsertSubroutine, such as CallSubroutine,
    // SetPrivEnable, and SetSyncEnable, so those atoms will get sent
    // as several calls.
    //
    deque<ChannelOp> consolidatedAtomOps;
    ChannelOp sub;  // Contains methods getting combined into InsertSubroutine
    sub.type   = INSERT_SUBROUTINE;
    sub.subch  = 0;
    sub.method = 0;
    sub.count  = 0;
    sub.aType  = DISABLE;

    for (deque<ChannelOp>::const_iterator pOp = m_AtomOps.begin();
         pOp != m_AtomOps.end(); ++pOp)
    {
        const ChannelOp &op = *pOp;

        // MAX_HEADER & MAX_DATA are used to callwlate how many 32-bit
        // words to reserve in the buffer for WriteExternal*().  We
        // could set these values to 1 for most channel classes, but 4
        // is safer if we ever wrap around a TegraTHIChannel.
        const UINT32 MAX_HEADER = 4;
        const UINT32 MAX_DATA   = 4;

        // Append one operation from m_AtomOps to consolidatedAtomOps
        // and/or sub.
        //
        switch (op.type)
        {
            case WRITE_INC:
            {
                sub.data.resize(sub.count + MAX_HEADER + MAX_DATA * op.count);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternal(&pSubEnd, op.subch, op.method,
                                            op.count, &op.data[0]));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_NON_INC:
            {
                sub.data.resize(sub.count + MAX_HEADER + MAX_DATA * op.count);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternalNonInc(&pSubEnd, op.subch,
                                                  op.method, op.count,
                                                  &op.data[0]));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_INC_ONCE:
            {
                sub.data.resize(sub.count + MAX_HEADER + MAX_DATA * op.count);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternalIncOnce(&pSubEnd, op.subch,
                                                   op.method, op.count,
                                                   &op.data[0]));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_IMM:
            {
                sub.data.resize(sub.count + MAX_HEADER + MAX_DATA * op.count);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternalImmediate(&pSubEnd, op.subch,
                                                     op.method, op.data[0]));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_INC_HEADER:
            {
                sub.data.resize(sub.count + MAX_HEADER);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternal(&pSubEnd, op.subch, op.method,
                                            op.count, nullptr));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_NON_INC_HEADER:
            {
                sub.data.resize(sub.count + MAX_HEADER);
                UINT32 *pSubEnd = &sub.data[sub.count];
                CHECK_RC(pCh->WriteExternalNonInc(&pSubEnd, op.subch,
                                                  op.method, op.count,
                                                  nullptr));
                sub.count = static_cast<UINT32>(pSubEnd - &sub.data[0]);
                break;
            }
            case WRITE_NOP:
            case WRITE_SET_SUBDEVICE:
            {
                // These can't be part of an atom
                MASSERT(!"Illegal case");
                return RC::SOFTWARE_ERROR;
            }
            case CALL_SUBROUTINE:
            {
                if (sub.count > 0)
                {
                    // Append the pending InsertSubroutine before
                    // appending CallSubroutine
                    sub.data.resize(sub.count);
                    sub.aType = ENABLE;
                    consolidatedAtomOps.push_back(sub);
                    sub.count = 0;
                }
                consolidatedAtomOps.push_back(op);
                consolidatedAtomOps.back().aType = ENABLE;
                break;
            }
            case INSERT_SUBROUTINE:
            {
                sub.data.resize(sub.count);
                sub.data.insert(sub.data.end(),
                                &op.data[0], &op.data[op.count]);
                break;
            }
            case SET_PRIV_ENABLE:
            case SET_SYNC_ENABLE:
            {
                // Append to the atom before pending subroutine
                consolidatedAtomOps.push_back(op);
                break;
            }
            case CANCEL_ATOM:
            {
                // Does nothing
                break;
            }
        }
    }
    if (sub.count > 0)
    {
        sub.data.resize(sub.count);
        consolidatedAtomOps.push_back(sub);
    }

    // Clear m_AtomOps, and send the consolidated atom to the wrapped
    // channel.
    //
    m_AtomOps.clear();
    UINT32 oldGpPut = 0;
    GetGpPut(&oldGpPut);
    CHECK_RC(SetExternalGPPutAdvanceMode(true));  // Suppress GPPUT while writing atom

    for (auto pOp = consolidatedAtomOps.begin();
         pOp != consolidatedAtomOps.end(); ++pOp)
    {
        SetAtomExelwtion(pOp->aType);
        rc = ExelwteOp(*pOp);
        SetAtomExelwtion(DISABLE);

        if (OK != rc)
        {
            return rc;
        }
    }

    UINT32 newGpPut = 0;
    GetGpPut(&newGpPut);
    if (newGpPut != oldGpPut)
    {
        // If the atom spans more than one GpEntry, close the current GpEntry
        // to ensure that the next GPPUT update encompasses the entire atom
        //
        CHECK_RC(FinishOpenGpFifoEntry(nullptr));
    }
    
    CHECK_RC(SetExternalGPPutAdvanceMode(false));
    return rc;
}

void AtomChannelWrapper::SetAtomExelwtion(AtomExelwtionType aType)
{
    m_IsAtomExelwtion = (aType == ENABLE) ? true : false;
}
