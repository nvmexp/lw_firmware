/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_SHAREDSYSMEM_H
#define INCLUDED_SHAREDSYSMEM_H

#include "core/include/rc.h"
#include "core/include/tasker.h"

#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#include <functional>
#ifdef _WIN32
#   pragma warning(pop)
#endif

//--------------------------------------------------------------------
// This module maintains shared memory that can be used to communicate
// between two mods processes (i.e. two instances of mods launched by
// two command lines).
//
// Each shared memory allocation is identified by a (pid, name, id)
// tuple.  "pid" is the process id of the MODS program that created
// the memory.  "Name" & "id" are a string and number respectively,
// which are munged together to form a globally-unique identifier.
//
// The module is named "SharedSysmem" to avoid confusion with LWCA
// shared memory, which is usually just called "shared memory" in
// mods.  It is internally implemented with boost::interprocess.
//
namespace SharedSysmem
{
    RC   Initialize();
    void Shutdown();
    void CleanupOnCrash();

    //! Allocate a new chunk of shared memory.  On success, a pointer
    //! to the shared sysmem will be stored in *ppAddr.
    //!
    //! If Alloc is called several times with the same name & id, it
    //! increments a counter and returns a pointer to the same memory.
    //! It is an error to re-alloc memory with a larger size than it
    //! was originally allocated with.
    //!
    RC Alloc(const string &name, UINT64 id, size_t size,
             bool readOnly, void **ppAddr);

    //! Import a chunk of shared memory that was allocated in another
    //! mods process.  On success, a pointer to the shared sysmem will
    //! be stored in *ppAddr, and the size will be stored in pSize if
    //! it is non-null.
    //!
    //! If Import is called several times with the same name & id, it
    //! increments a counter and returns a pointer to the same memory.
    //!
    RC Import(UINT32 pid, const string &name, UINT64 id,
              bool readOnly, void **ppAddr, size_t *pSize);

    //! Free a chunk of shared memory that was created by Alloc() or
    //! Import().  pAddr can point to any address within the memory
    //! chunk.  If memory was re-allocated or re-imported multiple
    //! times, then it must be freed multiple times before it actually
    //! gets deleted.
    //!
    //! Shared memory is owned by the process that allocated it.
    //! Freeing memory that was created by Alloc() will destroy the
    //! memory, but freeing memory that was imported by Import() will
    //! not.
    //!
    RC Free(void *pAddr);

    //! Find the name of the next chunk of shared memory that was
    //! created by Alloc() in any MODS process on this machine.
    //! Shared memories are sorted lexographically by the ordered pair
    //! (name, id).
    //!
    //! This method can be used to iterate up or down through the
    //! shared memories owned by any process, with the caveat that
    //! shared memories may be created or deleted asynchronously, so
    //! the caller must account for that.
    //!
    //! \param cmp Tells which direction to search.  It must be one of
    //!     ">", ">=" to search forwards, or "<" or "<=" to search
    //!     backwards.
    //! \param pid Tells which process to search.
    //! \param *pName On entry, contains the name to start searching
    //!     from.  On exit, contains the name of the next shared
    //!     memory.
    //! \param *pId On entry, contains the ID to start searching from.
    //!     On exit, contains the ID of the next shared memory.
    //! \param OK on success, or CANNOT_GET_ELEMENT if no matching
    //!     element exists.  Other values indicate an error
    //!
    RC Find(const string &cmp, UINT32 pid, string *pName, UINT64 *pId);

    //! If this mods process was launched by another mods process,
    //! return the PID of the parent mods process.  Can be used in
    //! conjunction with PidBox to make sure both processes have each
    //! other's PID.
    //!
    UINT32 GetParentModsPid();

    //! Simple mailbox class to pass the PID from one mods process to
    //! another.  One process must already know the PID of the other;
    //! this class is used to pass a PID in the opposite direction.
    //! Typical usage:
    //! == Parent mods process ==
    //!     {
    //!         PidBox pidBox;
    //!         // launch child mods process
    //!         pidBox.SetAbortCheck([]{return true_to_abort;}) // optional
    //!         CHECK_RC(pidBox.ReceivePid(&childPid));
    //!         // do setup with childPid
    //!         CHECK_RC(pidBox.Disconnect());
    //!     } // End pidBox scope
    //! == Child mods process ==
    //!     CHECK_RC(PidBox::SendPid(GetParentModsPid()));
    //!
    //! Note that the SharedSysmem mutex is locked while PidBox is in
    //! scope, so the scope shouldn't last long.  Also, the sending
    //! process will block in SendPid until PidBox calls Disconnect()
    //! or goes out of scope, which allows the receiving process to do
    //! some setup before the other process resumes.  If the optional
    //! SetAbortCheck() callback returns true, the communication
    //! aborts with RC::STATUS_INCOMPLETE.
    //!
    class PidBox
    {
    public:
        PidBox();
        ~PidBox();
        RC ReceivePid(UINT32 *pRemotePid);
        RC Disconnect();
        static RC SendPid(UINT32 remotePid);
        void SetAbortCheck(function<bool()> func) { m_AbortCheck = func; }
    private:
        Tasker::MutexHolder m_MutexHolder;
        function<bool()>    m_AbortCheck;
    };
}

#endif // !INCLUDED_TASKER_H
