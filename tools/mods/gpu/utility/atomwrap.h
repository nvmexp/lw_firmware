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

#ifndef INCLUDED_ATOMWRAP_H
#define INCLUDED_ATOMWRAP_H

#include "atomfsm.h"
#include "core/include/chanwrap.h"
#include <deque>

//--------------------------------------------------------------------
//! \brief Prevent inserted methods from breaking atomic operations
//!
//! This channel wrapper uses an AtomFsm to parse the method stream,
//! looking for atomic operations.  An "atomic operation" is a series
//! of methods that are required to stay together; see atomfsm.h for
//! more details.  This class enqueues the atom in a buffer, and sends
//! the entire buffer at once to the wrapped channel when the atom is
//! done.
//!
//! Any "inserted methods" that arrive in the middle of the atom are
//! passed straight to the wrapped channel.  Inserted methods should
//! be surrounded by BeginInsertedMethods and EndInsertedMethods().
//! They are normally inserted by the Runlist object (which inserts a
//! backend sepaphore after each runlist segment) or by PolicyManager.
//! Inserted methods aren't part of the original trace, and have to be
//! reordered either before or after the atom (lwrrently before) in
//! order to preserve the atomic operation.
//!
//! This class lwrrently assumes that no atomic methods occur between
//! BeginInsertedMethods and EndInsertedMethods(), which simplifies
//! the logic.
//!
class AtomChannelWrapper : public ChannelWrapper
{
public:
    AtomChannelWrapper(Channel *pChannel);
    static bool    IsSupported(Channel *pChannel);
    const AtomFsm *GetPeekableFsm() const;
    RC             CancelAtom();

    virtual RC   BeginInsertedMethods();
    virtual RC   EndInsertedMethods();
    virtual void CancelInsertedMethods();
    virtual RC   SetObject(UINT32 subchannel, LwRm::Handle handle);
    virtual RC   UnsetObject(LwRm::Handle handle);
    virtual RC   Write(UINT32 subch, UINT32 method, UINT32 data);
    virtual RC   Write(UINT32 subch, UINT32 method, UINT32 count,
                       UINT32 data, ...);
    virtual RC   Write(UINT32 subch, UINT32 method, UINT32 count,
                       const UINT32 *pData);
    virtual RC   WriteNonInc(UINT32 subch, UINT32 method, UINT32 count,
                             const UINT32 *pData);
    virtual RC   WriteHeader(UINT32 subch, UINT32 method, UINT32 count);
    virtual RC   WriteNonIncHeader(UINT32 subch, UINT32 method, UINT32 count);
    virtual RC   WriteIncOnce(UINT32 subch, UINT32 method, UINT32 count,
                              const UINT32 *pData);
    virtual RC   WriteImmediate(UINT32 subch, UINT32 method, UINT32 data);
    virtual RC   WriteNop();
    virtual RC   WriteSetSubdevice(UINT32 mask);
    virtual RC   CallSubroutine(UINT64 offset, UINT32 size);
    virtual RC   InsertSubroutine(const UINT32 *pBuffer, UINT32 count);
    virtual RC   WaitIdle(FLOAT64 timeoutMs);
    virtual void ClearPushbuffer();
    virtual RC   SetPrivEnable(bool priv);
    virtual RC   SetSyncEnable(bool sync);
    virtual AtomChannelWrapper *GetAtomChannelWrapper();
    
    bool IsAtomExelwtion() const;

private:
    //! The different types of queueable channel operations
    enum OpType
    {
        WRITE_INC,            //!< Incrementing method
        WRITE_NON_INC,        //!< Non-incrementing method
        WRITE_INC_ONCE,       //!< Increment-once method
        WRITE_IMM,            //!< Immediate method
        WRITE_INC_HEADER,     //!< Incrementing header
        WRITE_NON_INC_HEADER, //!< Non-incrementing header
        WRITE_NOP,            //!< Channel::WriteNop
        WRITE_SET_SUBDEVICE,  //!< Channel::SetWriteSubdevice
        CALL_SUBROUTINE,      //!< Channel::CallSubroutine
        INSERT_SUBROUTINE,    //!< Channel::InsertSubroutine
        SET_PRIV_ENABLE,      //!< Channel::SetPrivEnable
        SET_SYNC_ENABLE,      //!< Channel::SetSyncEnable
        CANCEL_ATOM           //!< Ends the current atom, if any
    };

    enum AtomExelwtionType
    {
        DISABLE,
        ENABLE              
    };

    //! If any channel operations get inserted in the middle of an
    //! atomic operation, this struct is used to enqueue the inserted
    //! operations until after the atomic operation.
    struct ChannelOp
    {
        OpType            type;   //!< Operation type (WRITE_INC, SET_PRIV, etc.)
        UINT32            subch;  //!< Subchannel
        UINT32            method; //!< Method or value
        UINT32            count;  //!< Data count to put in the header
        vector<UINT32>    data;   //!< Data to write
        AtomExelwtionType aType;  //!< AtomExelwtion type(DISABLE by default)
    };

    deque<ChannelOp> m_IncomingOps; //!< Methods that haven't been processed yet
    deque<ChannelOp> m_AtomOps;     //!< Methods in a pending atom
    UINT32 m_InsertNestingLevel;    //!< Num times BeginInsertedMethods() called
    AtomFsm m_AtomFsm;              //!< State machine to detect atom
    bool m_Relwrsive;               //!< True if HandleIncomingOp relwrses
    
    //!< SetAtomExelwtion sets it true to prevent inserting any host semaphore method
    //!< between method header and method data when sending methods to pushbuffer.
    bool m_IsAtomExelwtion;

    RC HandleIncomingOp(OpType type, UINT32 subch, UINT32 method,
                        UINT32 count, const UINT32 *pData);
    RC HandleQueuedIncomingOps();
    RC HandleOneQueuedIncomingOp();
    RC ExelwteOp(const ChannelOp &op);
    RC WriteQueue();

    void SetAtomExelwtion(AtomExelwtionType aType);
};

#endif // INCLUDED_ATOMWRAP_H
