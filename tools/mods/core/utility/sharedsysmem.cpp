/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/utility/sharedsysmem.h"

#include "core/include/xp.h"
#include <map>
#include <memory>
#include <utility>
#include <boost/filesystem.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

namespace
{
    using shared_memory_object = boost::interprocess::shared_memory_object;
    using mapped_region        = boost::interprocess::mapped_region;
    using interprocess_mutex   = boost::interprocess::interprocess_mutex;

    //! Mutex to prevent two threads from updating the shared-sysmem
    //! data at the same time.
    //!
    void *s_ShmemMutex = nullptr;

    //! If one mods process was launched by another mods process, this
    //! contains the PID of the parent mods process.  Can be used in
    //! conjunction with PidBox to make sure both processes have each
    //! other's PID.
    //!
    UINT32 s_ParentModsPid = 0;

    //! This variable contains the Uuid (determined by random bytes) of the 
    //! parent process. It will be set based on the elw var MODS_PARENTID (set 
    //! by root process). It is added to the shared memory key to uniquely 
    //! identify across docker instances (where two MODS parent processes can 
    //! have same PID)
    string s_RootProcessUuid;

    //! Name/ID of private shared memories used by PrivateShmem class.
    //!
    const char *s_PrivateDataName = "_PrivateData";
    enum { PRIVATE_STATIC_ID, PRIVATE_DIRECTORY_ID };

    //----------------------------------------------------------------
    //! This class holds an allocated or imported shared memory, and
    //! all mappings of the shared memory into local virtual-memory
    //! space.
    //!
    class Shmem
    {
    public:
        class Key;
        Shmem() {}
        virtual ~Shmem() { Free(); }
        RC         Alloc(const string &name, UINT64 id, size_t size);
        RC         Import(UINT32 pid, const string &name, UINT64 id);
        RC         Map(bool readOnly);
        RC         Unmap(void *pAddr);
        void       Free();
        void       CleanupOnCrash();
        void      *GetAddr(bool readOnly) const;
        const Key &GetKey()  const { return m_Key; }
        size_t     GetSize() const { return m_Size; }

        // (uuid, pid, name, id) tuple that uniquely identifies a shared memory
        class Key
        {
        public:
            Key(string uuid, UINT32 pid, string name, UINT64 id)
                { Set(move(uuid), pid, move(name), id); }
            Key() = default;
            void Set(string uuid, UINT32 pid, string name, UINT64 id);
            const string &GetUuid()     const { return m_Uuid; }
            UINT32        GetPid()      const { return m_Pid; }
            const string &GetName()     const { return m_Name; }
            UINT64        GetId()       const { return m_Id; }
            const string &GetFullName() const { return m_FullName; }
        private:
            string m_Uuid;
            UINT32 m_Pid = 0;
            string m_Name;
            UINT64 m_Id = 0;
            string m_FullName;
        };

    protected:
        // Used to initialize shared memory allocated in this PID
        virtual void ConstructMemory(void *pAddr) {}
        virtual void DestructMemory(void *pAddr)  {}

    private:
        enum class Status { FREED, ALLOCATED, IMPORTED };
        Key                  m_Key;
        size_t               m_Size               = 0;
        Status               m_Status             = Status::FREED;
        UINT32               m_ReadOnlyCount      = 0;
        UINT32               m_ReadWriteCount     = 0;
        shared_memory_object m_BoostShmem;
        mapped_region        m_ReadOnlyMapping;
        mapped_region        m_ReadWriteMapping;
    };

    //----------------------------------------------------------------
    //! This class holds all Shmems that were allocated by
    //! SharedSysmem::Alloc() or SharedSysmem::Import().  It allows us
    //! to look them up by uuid/pid/name/id or mapped address, or to find
    //! them via SharedSysmem::Find().
    //!
    class ShmemRegistry
    {
    public:
        RC Register(unique_ptr<Shmem> pShmem);
        RC Unregister(Shmem* pShmem);
        RC UpdateOnMap(Shmem *pShmem, void *pAddr);
        RC UpdateOnUnmap(Shmem *pShmem, void *pAddr);
        Shmem *LookupKey(const string &uuid, UINT32 pid, const string &name, UINT64 id);
        Shmem *LookupAddr(void **ppAddr);
        RC Find(const string &cmp, UINT32 pid, string *pName, UINT64 *pId);
        void Clear();
        void CleanupOnCrash();

    private:
        RC UpdateLocalDirectory(bool isAdd, const Shmem *pShmem);
        RC UpdateRemoteDirectory(UINT32 pid);

        // This struct holds an (address, size) tuple, used to map a
        // range of mapped memory to the Shmem object that owns the
        // memory.  Note that operator<() only returns "true" for
        // non-overlapping ranges, which allows us to use find() to
        // find a range given any address within the range.
        //
        struct AddrRange
        {
            void   *pAddr;
            size_t  size;
            bool operator<(const AddrRange &rhs) const
                { return static_cast<char*>(pAddr) + size <= rhs.pAddr; }
        };
        map<string, unique_ptr<Shmem>> m_KeyToShmem;
        map<AddrRange, Shmem*> m_AddrToShmem;

        // Contains the name/ID for all known Shmems created by any
        // MODS process.  Used by SharedSysmem::Find().  See
        // PrivateDirectory for implementation details.
        //
        struct DirectoryOnPid
        {
            UINT64                    lastCounter = 0;
            UINT64                    lastOffset  = 0;
            set<pair<string, UINT64>> shmemNameIds;
        };
        map<UINT32, DirectoryOnPid> m_Directory;
    };
    ShmemRegistry s_Registry;

    //----------------------------------------------------------------
    //! Wrapper class around private Shmem objects that are used to
    //! implement shared data structures used by the rest of the
    //! SharedSysmem module.
    //!
    //! PrivateShmem objects are not recorded in ShmemRegistry, mostly
    //! because ShmemRegistry uses the PrivateShmem objects to share
    //! the directory used by SharedSysmem::Find().  We would get a
    //! chicken/egg infinite relwrsion if we recorded the PrivateShmem
    //! objects in ShmemRegistry.
    //!
    template<typename T> class PrivateShmem : public Shmem
    {
    public:
        RC AllocAndMapIfNeeded(const string &name, UINT64 id,
                               size_t size = sizeof(T));
        RC ImportAndMap(UINT32 pid, const string &name, UINT64 id)
            { RC rc; CHECK_RC(Import(pid, name, id)); return Map(); }
        T *GetAddr() const { return static_cast<T*>(Shmem::GetAddr(false)); }
        RC Map()           { return Shmem::Map(false); }
        T *operator*()     { return GetAddr(); }
        T *operator->()    { return GetAddr(); }
    protected:
        void ConstructMemory(void *pAddr) { new(pAddr) T(GetSize()); }
        void DestructMemory(void *pAddr)  { static_cast<T*>(pAddr)->~T(); }
    };

    //----------------------------------------------------------------
    //! Shared memory for private fixed-length data used by the
    //! SharedSysmem module.  Once created, this struct stays
    //! allocated until SharedSysmem::Shutdown().
    //!
    struct PrivateStaticData
    {
        PrivateStaticData(size_t size) {}

        // Data used to implement PidBox class
        enum State { CONNECTED, SUCCESS, FAILURE };
        UINT32 pid;           // Set by sender
        bool   pidValid;      // Set by sender
        State  receiverState; // Set by receiver to disconnect
        State  senderState;   // Set by sender to ack disconnection

        // Mutex for s_PrivateDirectory
        interprocess_mutex directoryMutex;
    };
    PrivateShmem<PrivateStaticData> s_PrivateStaticData;

    //----------------------------------------------------------------
    //! Shared memory containing a directory of all Shmem objects
    //! registered with ShmemRegistry on this system.  It gets read by
    //! other MODS processes when they run SharedSysmem::Find().
    //!
    //! This directory can be resized by the owning process, so it is
    //! proctected by an interprocess mutex located in
    //! PrivateStaticData.
    //!
    //! We do not rewrite the entire directory every time a Shmem gets
    //! allocated or freed.  That would be an O(n**2) task.  Instead,
    //! we just append an entry to the end of the directory indicating
    //! whether we're adding or removing a Shmem, and the name/ID of
    //! the shmem being added or removed.  The Entrys are terminated
    //! by an all-zero Entry.  When other processes read the
    //! directory, they just need to read any new Entrys that were
    //! appended since last time they checked.
    //!
    //! When we run out of space to append Entrys, we re-allocate this
    //! struct and increment "counter".  Incrementing "counter" tells
    //! other processes that they should discard their current
    //! directory for this PID and reread from scratch.  When we
    //! re-allocate, we allocate at least twice as much as space as we
    //! lwrrently need, so that it amortizes to constant time (like
    //! std::vector).
    //!
    struct PrivateDirectory
    {
        PrivateDirectory(size_t size);
        struct Entry
        {
            UINT32 entrySize; // Size of this entry, in bytes.
                              // Must be a be multiple of sizeof(UINT64).
            UINT32 isAdd;     // Zero to delete a Shmem, nonzero to add one
            UINT64 id;        // ID of the Shmem being added/deleted
            char   name[1];   // Name of the Shmem, as a null-terminated string.
                              // Note: this is a variable-sized array that
                              // extends to the end of the entry.  It is
                              // declared as a one-byte array to satisfy C/C++
                              // language requirements.  The one-byte
                              // array ensures that sizeof(Entry)+strlen(name)
                              // can hold the entire entry including '\0'.
        };

        UINT64 counter; // Incremented every time we re-allocate this data
        UINT64 size;    // Size of the entries[] array, not including
                        // the terminating all-zero Entry
        char   entries[sizeof(Entry)]; // Dynamic array of Entry structs.
                                       // Declaring this to be sizeof(Entry)
                                       // made it easier for the code to leave
                                       // room for the terminating all-zero
                                       // Entry.
    };
    PrivateShmem<PrivateDirectory> s_PrivateDirectory;

    //----------------------------------------------------------------
    // Returns a timeout that's N times longer than the default
    // timeout (i.e. usually N seconds on HW, or infinite on
    // simulation).
    //
    FLOAT64 GetLongTimeout(FLOAT64 multiplier)
    {
        FLOAT64 timeout = Tasker::GetDefaultTimeoutMs();
        return (timeout == Tasker::NO_TIMEOUT) ? timeout : timeout * multiplier;
    }
}

//--------------------------------------------------------------------
//! Allocate a chunk of shared memory, which is owned by this process
//!
RC Shmem::Alloc(const string &name, UINT64 id, size_t size)
{
    MASSERT(m_Status == Status::FREED);
    MASSERT(size > 0);
    if (m_Status != Status::FREED || size < 1)
        return RC::BAD_PARAMETER;

    Free();
    m_Key.Set(s_RootProcessUuid, Xp::GetPid(), name, id);
    m_Size   = size;
    m_Status = Status::ALLOCATED;
    try
    {
        // Remove possible leftover file from aborted mods run,
        // then allocate new one
        shared_memory_object::remove(m_Key.GetFullName().c_str());
        boost::interprocess::permissions permissions;
        permissions.set_unrestricted();
        m_BoostShmem = shared_memory_object(boost::interprocess::create_only,
                                            m_Key.GetFullName().c_str(),
                                            boost::interprocess::read_write,
                                            permissions);
        m_BoostShmem.truncate(size);
    }
    catch (...)
    {
        Printf(Tee::PriError, "%s: getLasterror = %s\n",
               __FUNCTION__, strerror(errno));
        Free();
        return RC::BAD_PARAMETER;
    }
    return OK;
}

//--------------------------------------------------------------------
//! Import a chunk of shared memory that was allocated by another
//! process
//!
RC Shmem::Import(UINT32 pid, const string &name, UINT64 id)
{
    MASSERT(m_Status == Status::FREED);
    if (m_Status != Status::FREED)
        return RC::BAD_PARAMETER;

    bool failed = false;
    Free();
    m_Key.Set(s_RootProcessUuid, pid, name, id);
    m_Status = Status::IMPORTED;
    try
    {
        m_BoostShmem = shared_memory_object(boost::interprocess::open_only,
                                            m_Key.GetFullName().c_str(),
                                            boost::interprocess::read_write);
        boost::interprocess::offset_t actualSize = 0;
        if (!m_BoostShmem.get_size(actualSize))
            failed = true;
        m_Size = static_cast<size_t>(actualSize);
    }
    catch (...)
    {
        Printf(Tee::PriError, "%s: getLasterror = %s\n",
               __FUNCTION__, strerror(errno));
        failed = true;
    }
    if (failed)
    {
        Free();
        return RC::BAD_PARAMETER;
    }
    return OK;
}

//--------------------------------------------------------------------
//! Map a chunk of shared memory that was created by Alloc() or
//! Import().  A Shmem may be mapped multiple times, both read-only
//! and read-write, but all mappings after the first merely increment
//! a counter.
//!
RC Shmem::Map(bool readOnly)
{
    MASSERT(m_Status != Status::FREED);
    if (m_Status == Status::FREED)
        return RC::BAD_PARAMETER;

    UINT32 *pCount = readOnly ? &m_ReadOnlyCount : &m_ReadWriteCount;
    if (*pCount == 0)
    {
        mapped_region *pMapping =
            readOnly ? &m_ReadOnlyMapping : &m_ReadWriteMapping;
        try
        {
            *pMapping = mapped_region(m_BoostShmem,
                                      readOnly
                                      ? boost::interprocess::read_only
                                      : boost::interprocess::read_write,
                                      0, m_Size);
        }
        catch (...)
        {
            return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
        }
        if (m_Status == Status::ALLOCATED)
        {
            ConstructMemory(pMapping->get_address());
        }
    }
    ++(*pCount);
    return OK;
}

//--------------------------------------------------------------------
// Unmap a chunk of shared memory that was mapped by Map().  The
// shared memory is freed once we have called Unmap() the same number
// of times as Map().
//
// On an OS that maps read-only and read-write mappings to different
// addresses, this method can identify which of m_ReadOnlyMapping or
// m_ReadWriteMapping it should affect.  On an OS that maps both
// read-only and read-write to the same address, this method tears down
// m_ReadOnlyMapping before m_ReadWriteMapping -- not that it makes
// much difference.
//
RC Shmem::Unmap(void *pAddr)
{
    const bool readOnly = (GetAddr(true) == pAddr);
    MASSERT(GetAddr(readOnly) == pAddr);
    MASSERT(m_Status != Status::FREED);
    if (GetAddr(readOnly) != pAddr || m_Status == Status::FREED)
        return RC::BAD_PARAMETER;

    UINT32 *pCount = readOnly ? &m_ReadOnlyCount : &m_ReadWriteCount;
    MASSERT(*pCount > 0);
    if (*pCount > 1)
    {
        --(*pCount);
    }
    else if (m_ReadOnlyCount + m_ReadWriteCount == 1)
    {
        Free();
    }
    else
    {
        mapped_region *pMapping =
            readOnly ? &m_ReadOnlyMapping : &m_ReadWriteMapping;
        *pCount   = 0;
        *pMapping = mapped_region();
    }
    return OK;
}

//--------------------------------------------------------------------
// Free a chunk of shared memory that was initialized by Alloc() or
// Import().  Normally called indirectly when Unmap() brings the
// counter to 0, but often called directly during Shutdown and by
// PrivateShmem owners.
//
// Note that this method mostly returns the Shmem to a quiescent
// state, but does not erase m_Key.  That makes it easier to clean up
// s_Registry afterwards.
//
void Shmem::Free()
{
    if (m_Status == Status::ALLOCATED)
    {
        if (m_ReadWriteCount > 0)
            DestructMemory(m_ReadWriteMapping.get_address());
        else if (m_ReadOnlyCount > 0)
            DestructMemory(m_ReadOnlyMapping.get_address());
        shared_memory_object::remove(m_Key.GetFullName().c_str());
    }
    m_ReadOnlyMapping    = mapped_region();
    m_ReadWriteMapping   = mapped_region();
    m_BoostShmem         = shared_memory_object();
    m_Size               = 0;
    m_Status             = Status::FREED;
    m_ReadOnlyCount      = 0;
    m_ReadWriteCount     = 0;
}

//--------------------------------------------------------------------
//! Do minimal cleanup to avoid leaving artifacts.  This call leaves
//! the system in an inconsistent state, so it should only be used to
//! do last-minute cleanup when mods crashes.  Called by
//! SharedSysmem::CleanupOnCrash().
//!
void Shmem::CleanupOnCrash()
{
    if (m_Status == Status::ALLOCATED)
    {
        shared_memory_object::remove(m_BoostShmem.get_name());
        m_Status = Status::FREED;
    }
}

//--------------------------------------------------------------------
//! Return the address where the shared memory is mapped, or nullptr.
//!
void *Shmem::GetAddr(bool readOnly) const
{
    if (readOnly)
        return m_ReadOnlyCount ? m_ReadOnlyMapping.get_address() : nullptr;
    else
        return m_ReadWriteCount ? m_ReadWriteMapping.get_address() : nullptr;
}

//--------------------------------------------------------------------
//! Set the uuid/PID/name/ID tuple that identifies a Shmem on this 
//! computer
//!
void Shmem::Key::Set(string uuid, UINT32 pid, string name, UINT64 id)
{
    m_Uuid     = move(uuid);
    m_Pid      = pid;
    m_Name     = move(name);
    m_Id       = id;
    m_FullName = Utility::StrPrintf("MODS_%s_%u_%s_%llu",
                                    m_Uuid.c_str(), pid, m_Name.c_str(), id);
}

//--------------------------------------------------------------------
//! Register a new Shmem object that was just created by
//! Shmem::Alloc() or Shmem::Import().  ShmemRegistry assumes
//! ownership of the object after this point.
//!
RC ShmemRegistry::Register(unique_ptr<Shmem> pShmem)
{
    RC rc;
    CHECK_RC(UpdateLocalDirectory(true, pShmem.get()));
    m_KeyToShmem[pShmem->GetKey().GetFullName()] = move(pShmem);
    return OK;
}

//--------------------------------------------------------------------
//! Unregister a Shmem object that was registered but may
//! not yet be mapped.
//!
RC ShmemRegistry::Unregister(Shmem* pShmem)
{
    MASSERT(pShmem);
    RC rc;
    CHECK_RC(UpdateLocalDirectory(false, pShmem));
    m_KeyToShmem.erase(pShmem->GetKey().GetFullName());
    return OK;
}

//--------------------------------------------------------------------
//! Should be called for any Shmem object in the registry after the
//! caller calls Shmem::Map().  Used to update the data needed by
//! LookupAddr().
//!
RC ShmemRegistry::UpdateOnMap(Shmem *pShmem, void *pAddr)
{
    MASSERT(pShmem);
    MASSERT(pAddr);
    Shmem *pExistingShmem = LookupAddr(&pAddr);
    if (pExistingShmem == nullptr)
    {
        AddrRange addrRange = { pAddr, pShmem->GetSize() };
        m_AddrToShmem[addrRange] = pShmem;
    }
    else if (pExistingShmem != pShmem)
    {
        MASSERT(!"Two Shmem objects mapped to same address");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//--------------------------------------------------------------------
//! Should be called for any Shmem object in the registry after the
//! caller calls Shmem::Unmap().  Used to update the data needed by
//! LookupAddr(), and to unregister the Shmem object if we detect that
//! it was freed by Shmem::Unmap().
//!
RC ShmemRegistry::UpdateOnUnmap(Shmem *pShmem, void *pAddr)
{
    MASSERT(pShmem);
    MASSERT(pAddr);
    RC rc;
    void *readOnlyAddr  = pShmem->GetAddr(true);
    void *readWriteAddr = pShmem->GetAddr(false);
    if (readOnlyAddr != pAddr && readWriteAddr != pAddr)
    {
        AddrRange addrRange = { pAddr, 1 };
        m_AddrToShmem.erase(addrRange);
    }
    if (readOnlyAddr == nullptr && readWriteAddr == nullptr)
    {
        CHECK_RC(UpdateLocalDirectory(false, pShmem));
        m_KeyToShmem.erase(pShmem->GetKey().GetFullName());
    }
    return OK;
}

//--------------------------------------------------------------------
//! Return a registered Shmem given its uuid/PID/name/ID.  Return 
//! nullptr if there is no such Shmem.
//!
Shmem *ShmemRegistry::LookupKey(const string& uuid, UINT32 pid, const string &name, UINT64 id)
{
    Shmem::Key key(uuid, pid, name, id);
    auto iter = m_KeyToShmem.find(key.GetFullName());
    return (iter == m_KeyToShmem.end()) ? nullptr : iter->second.get();
}

//--------------------------------------------------------------------
//! Return a registered Shmem given an address that it is mapped to,
//! or nullptr if none.
//!
//! \param ppAddr On entry, *ppAddr is the address that we are
//!     searching for, which may be in the middle of the shared-memory
//!     region.  On successful exit, *ppAddr is the address of the
//!     beginning of the shared-memory region.
//!
Shmem *ShmemRegistry::LookupAddr(void **ppAddr)
{
    MASSERT(ppAddr);
    if (*ppAddr == nullptr)
        return nullptr;
    AddrRange addrRange = { *ppAddr, 1 };
    auto iter = m_AddrToShmem.find(addrRange);
    if (iter == m_AddrToShmem.end())
        return nullptr;
    *ppAddr = iter->first.pAddr;
    return iter->second;
}

//--------------------------------------------------------------------
//! See the description of SharedSysmem::Find() in the .h file.  This
//! method does 99% of the work, except for some mutexing.
//!
RC ShmemRegistry::Find
(
    const string &cmp,
    UINT32        pid,
    string       *pName,
    UINT64       *pId
)
{
    MASSERT(pName);
    MASSERT(pId);
    RC rc;

    // If the Shmem is owned by another process, then update our copy
    // of the other process's directory.
    //
    if (pid != Xp::GetPid())
    {
        CHECK_RC(UpdateRemoteDirectory(pid));
    }

    // Use lower_bound() to get within plus or minus 1 of the desired
    // Shmem, and then increment or decrement the iterator depending
    // on the contents of cmp.
    //
    const auto &nameIds = m_Directory[pid].shmemNameIds;
    const pair<string, UINT64> target(*pName, *pId);
    auto iter = nameIds.lower_bound(target);
    if (cmp == ">")
    {
        if (iter != nameIds.end() && !(target < *iter))
            ++iter;
    }
    else if (cmp == ">=")
    {
        // Do nothing; lower_bound already gave correct result
    }
    else if (cmp == "<")
    {
        if (iter == nameIds.begin())
            iter = nameIds.end();
        else
            --iter;
    }
    else if (cmp == "<=")
    {
        if (iter == nameIds.end() || target < *iter)
        {
            if (iter == nameIds.begin())
                iter = nameIds.end();
            else
                --iter;
        }
    }
    else
    {
        MASSERT(!"Illegal cmp value");
        return RC::SOFTWARE_ERROR;
    }

    // Return the shared memory we found
    //
    if (iter == nameIds.end())
        return RC::CANNOT_GET_ELEMENT;
    *pName = iter->first;
    *pId   = iter->second;
    return OK;
}

//--------------------------------------------------------------------
//! Clear the registry.  Called by SharedSysmem::Shutdown().
//!
void ShmemRegistry::Clear()
{
    m_AddrToShmem.clear();
    m_KeyToShmem.clear();
    m_Directory.clear();
}

//--------------------------------------------------------------------
//! Do minimal cleanup to avoid leaving artifacts.  This call leaves
//! the system in an inconsistent state, so it should only be used to
//! do last-minute cleanup when mods crashes.  Called by
//! SharedSysmem::CleanupOnCrash().
//!
void ShmemRegistry::CleanupOnCrash()
{
    for (auto& iter: m_KeyToShmem)
    {
        Shmem* pShmem = iter.second.get();
        if (pShmem)
        {
            pShmem->CleanupOnCrash();
        }
    }
}

//--------------------------------------------------------------------
//! Append an entry to the end of the s_PrivateDirectory shared memory
//! that we use to publish this process's Shmems to other processes.
//!
RC ShmemRegistry::UpdateLocalDirectory(bool isAdd, const Shmem *pShmem)
{
    MASSERT(pShmem);
    DirectoryOnPid *pDirectoryOnPid = &m_Directory[Xp::GetPid()];
    const pair<string, UINT64> shmemNameId(pShmem->GetKey().GetName(),
                                           pShmem->GetKey().GetId());
    RC rc;

    // Lock the interprocess mutex that protects s_PrivateDirectory
    //
    CHECK_RC(s_PrivateStaticData.AllocAndMapIfNeeded(s_PrivateDataName,
                                                     PRIVATE_STATIC_ID));
    CHECK_RC(Tasker::PollHw(
                GetLongTimeout(300),
                [] { return s_PrivateStaticData->directoryMutex.try_lock(); },
                __FUNCTION__));
    DEFER { s_PrivateStaticData->directoryMutex.unlock(); };

    // Make sure s_PrivateDirectory is allocated and mapped.  This
    // will allocate a minimum-sized PrivateDirectory the first time
    // it is called.
    //
    CHECK_RC(s_PrivateDirectory.AllocAndMapIfNeeded(s_PrivateDataName,
                                                    PRIVATE_DIRECTORY_ID));
    MASSERT(pDirectoryOnPid->lastCounter == s_PrivateDirectory->counter);

    // Update the DirectoryOnPid struct with the new entry.
    //
    if (isAdd)
        pDirectoryOnPid->shmemNameIds.insert(shmemNameId);
    else
        pDirectoryOnPid->shmemNameIds.erase(shmemNameId);

    // If s_PrivateDirectory has enough space for the new entry, then
    // append it.  If not, then re-allocate s_PrivateDirectory and
    // increment the counter.
    //
    size_t newEntrySize = ALIGN_UP(sizeof(PrivateDirectory::Entry) +
                                   pShmem->GetKey().GetName().size(),
                                   sizeof(UINT64));
    if (pDirectoryOnPid->lastOffset + newEntrySize <= s_PrivateDirectory->size)
    {
        // Append the new entry to the end of s_PrivateDirectory
        //
        auto *pEntry = reinterpret_cast<PrivateDirectory::Entry*>(
                &s_PrivateDirectory->entries[pDirectoryOnPid->lastOffset]);
        pEntry->entrySize = static_cast<UINT32>(newEntrySize);
        pEntry->isAdd     = isAdd ? 1 : 0;
        pEntry->id        = pShmem->GetKey().GetId();
        strcpy(pEntry->name, pShmem->GetKey().GetName().c_str());
        pDirectoryOnPid->lastOffset += newEntrySize;
    }
    else
    {
        // We must re-allocate s_PrivateDirectory, so callwlate how
        // much space to allocate.  Allocate twice as much space as we
        // need for the current Entry structs, or 1024 bytes,
        // whichever is bigger.
        //
        size_t allocSize = 0;
        const size_t MIN_ALLOC_SIZE = 1024;
        for (const auto iter: pDirectoryOnPid->shmemNameIds)
        {
            allocSize += ALIGN_UP(sizeof(PrivateDirectory::Entry) +
                                  pShmem->GetKey().GetName().size(),
                                  sizeof(UINT64));
        }
        allocSize = max(MIN_ALLOC_SIZE, 2 * allocSize);
        allocSize += sizeof(PrivateDirectory);

        // Free the current s_PrivateDirectory and re-allocate.  Note
        // that the PrivateDirectory constructor automatically creates
        // the new Entry structs by reading from ShmemRegistry.
        //
        s_PrivateDirectory.Free();
        CHECK_RC(s_PrivateDirectory.AllocAndMapIfNeeded(s_PrivateDataName,
                                                        PRIVATE_DIRECTORY_ID,
                                                        allocSize));

        // Find the end of the Entry structs created by the
        // PrivateDirectory constructor, so that we know where to
        // append the next time we call this method.  Also, increment
        // the counter.
        //
        char *pFirstEntry = &s_PrivateDirectory->entries[0];
        char *pLastEntry  = pFirstEntry;
        size_t entrySize;
        do
        {
            entrySize = reinterpret_cast<
                PrivateDirectory::Entry*>(pLastEntry)->entrySize;
            pLastEntry += entrySize;
        }
        while (entrySize != 0);
        pDirectoryOnPid->lastOffset   = pLastEntry - pFirstEntry;
        pDirectoryOnPid->lastCounter += 1;
        s_PrivateDirectory->counter   = pDirectoryOnPid->lastCounter;
    }
    return OK;
}

//--------------------------------------------------------------------
//! Update our mirror of another process's shared memories, by reading
//! the PrivateDirectory data owned by the other process.
//!
RC ShmemRegistry::UpdateRemoteDirectory(UINT32 pid)
{
    MASSERT(pid != Xp::GetPid());
    DirectoryOnPid *pDirectoryOnPid = &m_Directory[pid];
    RC rc;

    // Lock the interprocess mutex that the remote PrivateDirectory.
    // If the mutex doesn't exist, then presumably the remote process
    // either hasn't initialized yet or has already shut down, so
    // clear our mirror and return OK.
    //
    PrivateShmem<PrivateStaticData> remoteStaticData;
    if (remoteStaticData.ImportAndMap(pid, s_PrivateDataName,
                                      PRIVATE_STATIC_ID) != OK)
    {
        m_Directory.erase(pid);
        return OK;
    }
    CHECK_RC(Tasker::PollHw(
                    GetLongTimeout(300),
                    [&] { return remoteStaticData->directoryMutex.try_lock(); },
                    __FUNCTION__));
    DEFER { remoteStaticData->directoryMutex.unlock(); };

    // Import and map the remote process's PrivateDirectory.  Note
    // that the PrivateShmem<> destructor will unmap on return.  If
    // the PrivateDirectory doesn't exist, clear our mirror and return
    // OK (see above).
    //
    PrivateShmem<PrivateDirectory> remoteDirectory;
    if (remoteDirectory.ImportAndMap(pid, s_PrivateDataName,
                                     PRIVATE_DIRECTORY_ID) != OK)
    {
        m_Directory.erase(pid);
        return OK;
    }

    // If the remote process has updated the couter (i.e. reallocated
    // the PrivateDirectory), then make sure we re-read the new
    // PrivateDirectory from scratch.
    //
    if (pDirectoryOnPid->lastCounter != remoteDirectory->counter)
    {
        MASSERT(pDirectoryOnPid->lastCounter < remoteDirectory->counter);
        pDirectoryOnPid->lastCounter = remoteDirectory->counter;
        pDirectoryOnPid->lastOffset  = 0;
        pDirectoryOnPid->shmemNameIds.clear();
    }

    // Read all new entries and update our mirror.
    //
    char *pEntry = &remoteDirectory->entries[pDirectoryOnPid->lastOffset];
    auto *pDirEntry = reinterpret_cast<PrivateDirectory::Entry*>(pEntry);
    while (pDirEntry->entrySize != 0)
    {
        const pair<string, UINT64> nameId(pDirEntry->name, pDirEntry->id);
        if (pDirEntry->isAdd)
        {
            MASSERT(pDirectoryOnPid->shmemNameIds.count(nameId) == 0);
            pDirectoryOnPid->shmemNameIds.insert(nameId);
        }
        else
        {
            MASSERT(pDirectoryOnPid->shmemNameIds.count(nameId) != 0);
            pDirectoryOnPid->shmemNameIds.erase(nameId);
        }
        pEntry += pDirEntry->entrySize;
        pDirEntry = reinterpret_cast<PrivateDirectory::Entry*>(pEntry);
    }
    pDirectoryOnPid->lastOffset = pEntry - &remoteDirectory->entries[0];
    return OK;
}

//--------------------------------------------------------------------
template<typename T> RC PrivateShmem<T>::AllocAndMapIfNeeded
(
    const string &name,
    UINT64        id,
    size_t        size
)
{
    RC rc;
    if (GetAddr() == nullptr)
    {
        CHECK_RC(Alloc(name, id, size));
        CHECK_RC(Map());
    }
    return OK;
}

//--------------------------------------------------------------------
//! PrivateDirectory constructor.  Fill the PrivateDirectory shared
//! memory with data read from s_Registry.  This should be called the
//! first time we map s_PrivateDirectory after calling Alloc(); it
//! should not be called when we Import() a PrivateDirectory from
//! another process.
//!
//!  The constructor initializes all fields except "counter", which is
//!  either left at 0 after the initial allocation, or set by the
//!  ShmemRegistry::UpdateLocalDirectory() during re-allocation.
//!
PrivateDirectory::PrivateDirectory(size_t allocSize)
{
    MASSERT(allocSize >= sizeof(*this));
    const UINT32 pid = Xp::GetPid();

    memset(this, 0, allocSize);
    size = allocSize - sizeof(*this);
    char *pEntry = &entries[0];

    string name;
    UINT64 id = 0;
    for (RC rc = s_Registry.Find(">=", pid, &name, &id); rc == OK;
         rc = s_Registry.Find(">", pid, &name, &id))
    {
        auto pDirEntry = reinterpret_cast<Entry*>(pEntry);
        pDirEntry->entrySize = static_cast<UINT32>(
                ALIGN_UP(sizeof(Entry) + name.size(), sizeof(UINT64)));
        pDirEntry->isAdd = 1;
        pDirEntry->id    = id;
        strcpy(pDirEntry->name, name.c_str());
        pEntry += pDirEntry->entrySize;
    }
    MASSERT(pEntry <= &entries[0] + size);
}

//--------------------------------------------------------------------
RC SharedSysmem::Initialize()
{
    const UINT32 pid = Xp::GetPid();
    RC rc;
    s_ShmemMutex = Tasker::AllocMutex("s_ShmemMutex", Tasker::mtxLast);

    // Export this process's PID to subprocesses.
    //
    const char *MODS_PPID = "MODS_PPID";
    s_ParentModsPid = atoi(Xp::GetElw(MODS_PPID).c_str());
    CHECK_RC(Xp::SetElw(MODS_PPID, Utility::StrPrintf("%u", pid)));

    // Use Parent's Uuid for child processes
    // For Parent the string will be empty hence it will generate the Uuid and
    // export it to child process via elw var
    const char *MODS_PARENTID = "MODS_PARENTID";
    if (Xp::GetElw(MODS_PARENTID).size() == 0)
    {
        vector<UINT08> uuidBytes;
        uuidBytes.resize(16);
        UINT32 uuidSize = static_cast<UINT32>(uuidBytes.size());
        Utility::GetRandomBytes(&uuidBytes[0], uuidSize);
        s_RootProcessUuid = Utility::FormatBytesToString(&uuidBytes[0], uuidSize);
        CHECK_RC(Xp::SetElw(MODS_PARENTID, s_RootProcessUuid));
    }
    else
    {
        s_RootProcessUuid = Xp::GetElw(MODS_PARENTID);
    }

    return OK;
}

//--------------------------------------------------------------------
void SharedSysmem::Shutdown()
{
    s_Registry.Clear();
    s_PrivateDirectory.Free();
    s_PrivateStaticData.Free();
    Tasker::FreeMutex(s_ShmemMutex);
    s_ShmemMutex = nullptr;
}

//--------------------------------------------------------------------
//! \brief Do minimal cleanup to avoid leaving artifacts.
//!
//! This is similar to SharedSysmem::Shutdown, except that it avoids
//! calling any functions such as "delete" that are unsafe in a signal
//! handler.  It leaves the system in an inconsistent state, so it
//! should only be used to do last-minute cleanup when mods crashes.
//!
void SharedSysmem::CleanupOnCrash()
{
    s_Registry.CleanupOnCrash();
    s_PrivateDirectory.CleanupOnCrash();
    s_PrivateStaticData.CleanupOnCrash();
}

//--------------------------------------------------------------------
RC SharedSysmem::Alloc
(
    const string &name,
    UINT64        id,
    size_t        size,
    bool          readOnly,
    void        **ppAddr
)
{
    MASSERT(ppAddr);
    Tasker::MutexHolder lock(s_ShmemMutex);
    RC rc;

    Shmem *pShmem = s_Registry.LookupKey(s_RootProcessUuid, Xp::GetPid(), name, id);
    if (!pShmem)
    {
        auto shmemHolder = make_unique<Shmem>();
        pShmem = shmemHolder.get();
        rc = pShmem->Alloc(name, id, size);
        if (OK != rc)
        {
            Printf(Tee::PriError,
                   "SharedSysmem::%s: Failed to allocate shared mem for '%s':"
                   " Error message: %s.\n",
                    __FUNCTION__,
                    name.c_str(),
                    rc.Message());
            return rc;
        }
        rc = s_Registry.Register(move(shmemHolder));
        if (OK != rc)
        {
            Printf(Tee::PriError,
                   "SharedSysmem::%s: Failed to register shared mem for '%s':"
                   " Error message: %s.\n",
                    __FUNCTION__,
                    name.c_str(),
                    rc.Message());
            return rc;
        }
    }
    if (size > pShmem->GetSize())
    {
        s_Registry.Unregister(pShmem);
        return RC::BAD_PARAMETER;
    }
    rc = pShmem->Map(readOnly);
    if (OK != rc)
    {
        Printf(Tee::PriError,
               "SharedSysmem::%s: Failed to map shared mem for '%s':"
               " Error message: %s.\n",
                __FUNCTION__,
                name.c_str(),
                rc.Message());
        return rc;
    }
    *ppAddr = pShmem->GetAddr(readOnly);
    rc = s_Registry.UpdateOnMap(pShmem, *ppAddr);
    if (OK != rc)
    {
        Printf(Tee::PriError,
               "SharedSysmem::%s: Call UpdateOnMap failed for shared mem '%s':"
               " Error message: %s.\n",
                __FUNCTION__,
                name.c_str(),
                rc.Message());
    }
    return rc;
}

//--------------------------------------------------------------------
RC SharedSysmem::Import
(
    UINT32        pid,
    const string &name,
    UINT64        id,
    bool          readOnly,
    void        **ppAddr,
    size_t       *pSize
)
{
    MASSERT(ppAddr);
    Tasker::MutexHolder lock(s_ShmemMutex);
    RC rc;

    Shmem *pShmem = s_Registry.LookupKey(s_RootProcessUuid, pid, name, id);
    if (!pShmem)
    {
        auto shmemHolder = make_unique<Shmem>();
        pShmem = shmemHolder.get();
        CHECK_RC(pShmem->Import(pid, name, id));
        CHECK_RC(s_Registry.Register(move(shmemHolder)));
    }
    CHECK_RC(pShmem->Map(readOnly));
    *ppAddr = pShmem->GetAddr(readOnly);
    if (pSize)
        *pSize = pShmem->GetSize();
    CHECK_RC(s_Registry.UpdateOnMap(pShmem, *ppAddr));
    return OK;
}

//--------------------------------------------------------------------
RC SharedSysmem::Free(void *pAddr)
{
    if (pAddr != nullptr)
    {
        Tasker::MutexHolder lock(s_ShmemMutex);
        RC rc;

        Shmem *pShmem = s_Registry.LookupAddr(&pAddr);
        if (!pShmem)
            return RC::BAD_PARAMETER;
        CHECK_RC(pShmem->Unmap(pAddr));
        CHECK_RC(s_Registry.UpdateOnUnmap(pShmem, pAddr));
    }
    return OK;
}

//--------------------------------------------------------------------
RC SharedSysmem::Find
(
    const string &cmp,
    UINT32        pid,
    string       *pName,
    UINT64       *pId
)
{
    MASSERT(pName);
    MASSERT(pId);
    Tasker::MutexHolder lock(s_ShmemMutex);
    RC rc;
    CHECK_RC(s_Registry.Find(cmp, pid, pName, pId));
    return OK;
}

//--------------------------------------------------------------------
UINT32 SharedSysmem::GetParentModsPid()
{
    return s_ParentModsPid;
}

//--------------------------------------------------------------------
SharedSysmem::PidBox::PidBox() :
    m_MutexHolder(s_ShmemMutex),
    m_AbortCheck([]{ return false; })
{
    s_PrivateStaticData.AllocAndMapIfNeeded(s_PrivateDataName,
                                            PRIVATE_STATIC_ID);
    if (s_PrivateStaticData.GetAddr())
    {
        s_PrivateStaticData->pid           = 0;
        s_PrivateStaticData->pidValid      = false;
        s_PrivateStaticData->receiverState = PrivateStaticData::CONNECTED;
        s_PrivateStaticData->senderState   = PrivateStaticData::CONNECTED;
    }
}

//--------------------------------------------------------------------
SharedSysmem::PidBox::~PidBox()
{
    Disconnect();
}

//--------------------------------------------------------------------
RC SharedSysmem::PidBox::ReceivePid(UINT32 *pRemotePid)
{
    MASSERT(pRemotePid);
    RC rc;

    CHECK_RC(s_PrivateStaticData.AllocAndMapIfNeeded(s_PrivateDataName,
                                                     PRIVATE_STATIC_ID));
    rc = Tasker::PollHw(
            GetLongTimeout(300),
            [&] { return s_PrivateStaticData->pidValid || m_AbortCheck(); },
            __FUNCTION__);

    if (rc == OK && !s_PrivateStaticData->pidValid)
    {
        rc = RC::STATUS_INCOMPLETE;
    }
    if (rc != OK)
    {
        s_PrivateStaticData->receiverState = PrivateStaticData::FAILURE;
        return rc;
    }

    *pRemotePid = s_PrivateStaticData->pid;
    return rc;
}

//--------------------------------------------------------------------
RC SharedSysmem::PidBox::Disconnect()
{
    RC rc;

    CHECK_RC(s_PrivateStaticData.AllocAndMapIfNeeded(s_PrivateDataName,
                                                     PRIVATE_STATIC_ID));
    if (s_PrivateStaticData->receiverState == PrivateStaticData::CONNECTED)
    {
        s_PrivateStaticData->receiverState = PrivateStaticData::SUCCESS;
    }
    CHECK_RC(Tasker::PollHw(
            GetLongTimeout(10),
            [&] { return (s_PrivateStaticData->senderState !=
                          PrivateStaticData::CONNECTED) || m_AbortCheck(); },
            __FUNCTION__));

    if (s_PrivateStaticData->senderState == PrivateStaticData::CONNECTED)
    {
        rc = RC::STATUS_INCOMPLETE;
    }
    else if (s_PrivateStaticData->receiverState != PrivateStaticData::SUCCESS ||
             s_PrivateStaticData->senderState != PrivateStaticData::SUCCESS)
    {
        rc = RC::TIMEOUT_ERROR;
    }

    return rc;
}

//--------------------------------------------------------------------
RC SharedSysmem::PidBox::SendPid(UINT32 remotePid)
{
    Tasker::MutexHolder lock(s_ShmemMutex);
    PrivateShmem<PrivateStaticData> remoteStaticData;
    RC rc;

    CHECK_RC(remoteStaticData.ImportAndMap(remotePid, s_PrivateDataName,
                                           PRIVATE_STATIC_ID));
    remoteStaticData->pid      = Xp::GetPid();
    remoteStaticData->pidValid = true;

    rc = Tasker::PollHw(GetLongTimeout(300),
                        [&] { return (remoteStaticData->receiverState !=
                                      PrivateStaticData::CONNECTED); },
                        __FUNCTION__);
    if (rc != OK ||
        remoteStaticData->receiverState != PrivateStaticData::SUCCESS)
    {
        remoteStaticData->senderState = PrivateStaticData::FAILURE;
        return RC::TIMEOUT_ERROR;
    }

    remoteStaticData->senderState = PrivateStaticData::SUCCESS;
    return OK;
}
