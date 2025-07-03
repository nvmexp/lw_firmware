/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Fast allocation of memory

#include <map>
#include "core/include/pool.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/xp.h"
#include "core/include/tee.h"
#include "core/include/filesink.h"
#include "core/include/platform.h"
#include "core/include/stackdbg.h"
#include "core/include/nativmtx.h"
#include "core/include/utility.h"
#include <string.h>

#if defined(DEBUG) || (DEBUG_TRACE_LEVEL >= 1)
    //
    // Enables overrun/underrun checking, double-free checking,
    // freed-memory overwrites, and internal data structure sanity checking.
    //
    // Note: you can enable this even with a release build of mods, if you
    // want.
    //
    #define POOL_DEBUG 1
#endif

// Private version of assert that doesn't call printf.
// Printf might call new(), and cause an infinite loop...
//
#if defined(POOL_DEBUG)
    #define POOL_ASSERT(must_be_true)      do{ if(!(must_be_true)) Xp::BreakPoint(); } while(0)
#else
    #define POOL_ASSERT(wont_be_evaluated) do{}while(0)
#endif

// define PRINT_LEAK_REPORT: If you want to report the leak and trigger an
//  assertion if there is a leak or track the leak with leak-detector.pl script.
// define PRINT_LEAK_REPORT, PRINT_SEQNUM: If you want to debug the leak report.

//! Only for debugging the leak detector.
//#define PRINT_SEQNUM

//! To print the leak report.
//#define PRINT_LEAK_REPORT

//! To detect negative leaks
//#define DETECT_NEGATIVE_LEAKS

#if defined(POOL_DEBUG)
namespace Utility
{
    namespace Pool
    {
        UINT32 d_SequenceCounter = 0;
        UINT32 d_FirstCheckedSequenceNumber = 0;
    }
}
#endif

#if defined(PRINT_LEAK_REPORT)
namespace Utility
{
    namespace Pool
    {
        UINT32* s_SeqNumbersToBreak = 0;
        UINT32 s_NextSeqNumberToBreak = 0;
        UINT32 s_NumSeqNumbersToBreak = 0;
        UINT32 s_MaxSeqNumbersToBreak = 0;

        // Allocates an array for the specified number of seq numbers
        // on which we will break.
        void AllocSeqNumbers(UINT32 num)
        {
            if (s_SeqNumbersToBreak == 0)
            {
                s_SeqNumbersToBreak = static_cast<UINT32*>(malloc(sizeof(UINT32)*num));
                s_NextSeqNumberToBreak = 0;
                s_NumSeqNumbersToBreak = 0;
                s_MaxSeqNumbersToBreak = num;
            }
            else
            {
                printf("Space for seq numbers already allocated!\n");
            }
        }

        // Adds a sequence number to the array allocated with AllocSeqNumbers().
        // This function must be ilwoked with increasing (sorted) numbers.
        void AddSeqNumberToBreak(UINT32 i)
        {
            if (s_NumSeqNumbersToBreak < s_MaxSeqNumbersToBreak)
            {
                if ((s_NumSeqNumbersToBreak > 0) &&
                    (i <= s_SeqNumbersToBreak[s_NumSeqNumbersToBreak-1]))
                {
                    printf("Seq number %u out of order!\n", i);
                }
                else
                {
                    s_SeqNumbersToBreak[s_NumSeqNumbersToBreak++] = i;
                }
            }
            else
            {
                printf("More seq numbers than allocated!\n");
            }
        }

        // The leak-detector.pl script will place a breakpoint on this function.
        void BreakOnSeqNumber(UINT32 seqNumber)
        {
        }

        // Checks if the current sequence number matches any from the array
        // filled by the AddSeqNumberToBreak(). Typically the leak-detector.pl
        // script will first create the array by calling AddSeqNumberToBreak(),
        // then add a breakpoint on BreakOnSeqNumber(), then print backtraces
        // when the breakpoint is hit.
        void CheckSeqNumber()
        {
            while ((s_NextSeqNumberToBreak < s_MaxSeqNumbersToBreak) &&
                   (d_SequenceCounter > s_SeqNumbersToBreak[s_NextSeqNumberToBreak]))
            {
                s_NextSeqNumberToBreak++;
            }
            if (s_NextSeqNumberToBreak >= s_MaxSeqNumbersToBreak)
            {
#ifdef PRINT_SEQNUM
                printf("seqNum= %u\n", d_SequenceCounter);
#endif
                return;
            }
            if (s_SeqNumbersToBreak[s_NextSeqNumberToBreak] == d_SequenceCounter)
            {
#ifdef PRINT_SEQNUM
                printf("seqNum= %u / next= %u - TRIGGERED\n", d_SequenceCounter,
                       s_SeqNumbersToBreak[s_NextSeqNumberToBreak]);
#endif
                s_NextSeqNumberToBreak++;
                BreakOnSeqNumber(d_SequenceCounter);
                return;
            }
#ifdef PRINT_SEQNUM
            else
            {
                printf("seqNum= %u / next= %u\n", d_SequenceCounter,
                       s_SeqNumbersToBreak[s_NextSeqNumberToBreak]);
            }
#endif
        }
    }
}
#endif

namespace
{
    // We can only use the mutex after construction and before destruction.
    // MutexContainer returns pointer to the mutex only inbetween constructor and destructor
    // and zero before construction and after destruction.
    class MutexContainer
    {
        public:
            MutexContainer()
            {
                s_initialized = true;
            }
            ~MutexContainer()
            {
                s_initialized = false;
            }
            Tasker::NativeMutex* GetMutex()
            {
                return s_initialized ? &m_Mutex : 0;
            }

        private:
            Tasker::NativeMutex m_Mutex;
            static bool s_initialized;
    };

    bool MutexContainer::s_initialized = false;
    MutexContainer s_PoolSentinel;

    // Locks global pool mutex if it's initialized.
    class PoolLock
    {
        public:
            PoolLock()
            : m_Holder(0)
            {
                Tasker::NativeMutex* const pMutex = s_PoolSentinel.GetMutex();
                if (pMutex != 0)
                {
                    m_Holder = new(m_HolderArea) Tasker::NativeMutexHolder(*pMutex);
                }
            }
            ~PoolLock()
            {
                if (m_Holder != 0)
                {
                    m_Holder->~NativeMutexHolder();
                }
            }

        private:
            Tasker::NativeMutexHolder* m_Holder;
            char m_HolderArea[sizeof(Tasker::NativeMutexHolder)];
    };
}

namespace Utility
{
    namespace Pool
    {
        //--------------------------------------------------------------------------
        // Private data:

        bool d_AllocAssert = true;
        bool d_TraceEnabled = false;
        bool d_IsInitialized = false;
        UINT32 d_SizeAligned = 0;
        INT64 d_Threshold = -1;
        INT64 d_Counters[NUM_COUNTERS];

        //
        // How many pool lists, each for items 2x size of the previous pool.
        // The first list is for items of size 1, so the last list is for
        // size (1 << NUM_POOL_LISTS).
        //
        // Allocs larger than the max blocksize are passed to malloc().
        //
        // To preserve alignment, lists 0 to 2 (blocksizes 1,2,4) are not used.
        // Instead, allocs of size 0 to 8 all go in list 3 (blocksize 8).
        //
        const UINT32 NUM_POOL_LISTS = 13;

        //
        // Number of blocks in each pool in each list.
        //
        const UINT32 NUM_BLOCK      = 32;

        //
        // Alignment for the memory pools.
        //
#ifdef LW_64_BITS
        const UINT32 POOL_ALIGNMENT_INDEX = 4;
#else
        const UINT32 POOL_ALIGNMENT_INDEX = 3;
#endif

        const UINT32 POOL_ALIGNMENT      = 1<<POOL_ALIGNMENT_INDEX;

        //
        // Array of pointers to the heads of each pool list.
        //
        struct PoolHeader
        {
            PoolHeader *pPrev;
            PoolHeader *pNext;
            UINT32 Size;     // size (in bytes) of each of the 32 blocks in this pool
            UINT32 FreeMask; // bitmask of which blocks are free
#ifdef LW_64_BITS
            UINT32 AlignPad[2];  // needed to maintain 16-byte alignment
#endif
        };
        struct PoolList
        {
            PoolHeader *pFreeList;  // List of entries with at least one free block,
            // i.e. FreeMask != 0
            PoolHeader *pFullList;  // List of entries with no free blocks,
            // i.e. FreeMask == 0
        };
        PoolList d_PoolLists[NUM_POOL_LISTS];

        //
        // Dummy pool header pointer, to identify the special-case allocs to Free:
        //   d_pAlignedDummy   identifies blocks gotten from AllocAligned as well
        //                     as blocks too big for the pool (which default to use
        //                     AllocAligned.
        //
        PoolHeader * d_pAlignedDummy;

        //
        // For debugging, we decorate each alloc'ed block with a guard band
        // at the beginning and end, to detect when other code writes
        // past the begin or end of its buffer.
        // We store the exact alloc size (may be < block size) so we know
        // where the ending guard band is, when we check it during delete.
        //
        struct BlockHeader
        {
            PoolHeader * MyPool;       // points to the PoolHeader controlling this block
            UINT32       AllocSize;    // num bytes alloced (required for realloc).
#ifdef LW_64_BITS
            UINT32       AlignPad;     // needed to maintain 16-byte alignment
#endif
        };
        struct DebugBlockHeader
        {
            PoolHeader * MyPool;       // points to the PoolHeader controlling this block
            UINT32       AllocSize;    // num bytes alloced (where 2nd guardband starts)
            UINT32       SeqNum;       // to count the allocations and then track down a memory leak
            UINT32       GuardBandA;   // magic value, to detect underruns
#ifdef LW_64_BITS
            UINT32       GuardBandB[3];// (we need to keep 16-byte alignment)
#endif

#if DEBUG_TRACE_LEVEL >= 1
            const char        *FileName;      // file where allocation oclwrred
            DebugBlockHeader  *pPrev;         // link to previous allocated block
            DebugBlockHeader  *pNext;         // link to next allocated block
            CounterIdx         Type;          // type of allocation
            UINT32             LineNumber;    // line within file of allocation
            UINT32             LeakMagic;     // not all allocs are traced, so identify
#ifdef LW_64_BITS
            UINT32             Padding1[3];   // yet more padding for alignment...
#endif
#endif

#if DEBUG_TRACE_LEVEL >= 2
            StackDebug::Handle Stack;         // collect information for stack trace
#ifdef LW_64_BITS
            UINT32             Padding2[2];   // yet more padding for alignment...
#else
            UINT32             Padding2;      // yet more padding for alignment...
#endif
#endif
        };

#define PTR2HEADER(x) (BLOCK_HEADER_TYPE *)((char *)(x) - sizeof(BLOCK_HEADER_TYPE))
#define HEADER2PTR(x) ((char *)(x) + sizeof(BLOCK_HEADER_TYPE))

#if defined(POOL_DEBUG)
        // This isn't used directly due to alignment issues, but we use it for the
        // size of the allocation.
        struct DebugBlockTrailer
        {
            UINT08       GuardBandC[POOL_ALIGNMENT-4];
            UINT08       Sequence[4];
        };
        const UINT32 GUARD_BAND_A_VALUE = 0xd1a6f00d;  // cn u rd ths?
#ifdef LW_64_BITS
        const UINT32 GUARD_BAND_B_VALUE = 0x600dbeef;  // tn u r a 1ee7 d00d.
#endif
#if DEBUG_TRACE_LEVEL >= 1
        const UINT32 LEAK_MAGIC_VALUE   = 0xbadd00d5;  // r u a bad 3nuff d00d?
#endif

        // GUARD_BAND_C_VALUE -> 0x1ee7d00d
        const UINT08 GUARD_BAND_C_VALUE_3 = 0x1e;
        const UINT08 GUARD_BAND_C_VALUE_2 = 0xe7;
        const UINT08 GUARD_BAND_C_VALUE_1 = 0xd0;
        const UINT08 GUARD_BAND_C_VALUE_0 = 0x0d;

#define BLOCK_HEADER_TYPE DebugBlockHeader
        const int BLOCK_TRAILER_SIZE = sizeof(DebugBlockTrailer);

        void WriteDebugGuardBands(DebugBlockHeader *);
        void CheckDebugGuardBands(volatile DebugBlockHeader *);

#if DEBUG_TRACE_LEVEL >= 1
        void PrintDebugTrace(DebugBlockHeader *);
        void DestroyDebugTrace(DebugBlockHeader *);

        DebugBlockHeader * d_pTraceHead = NULL;
#endif

#else
#define BLOCK_HEADER_TYPE BlockHeader
        const int BLOCK_TRAILER_SIZE = 0;

        inline void WriteDebugGuardBands(BlockHeader *) {}
        inline void CheckDebugGuardBands(volatile BlockHeader *) {}
#endif
        const int BLOCK_OVERHEAD = sizeof(BLOCK_HEADER_TYPE) + BLOCK_TRAILER_SIZE;

        //
        // For blocks allocated by AllocAligned, the header has an extra field
        // stored just before the regular header, containing the orignal malloc
        // returned address, and the BH.MyPool pointer always points to AlignedDummy
        // instead of OversizedDummy.
        //
        struct AlignedBlockHeader
        {
            void *      FreePtr;
#if defined(PRINT_LEAK_REPORT)
            AlignedBlockHeader* pPrev;
            AlignedBlockHeader* pNext;
#endif
            BLOCK_HEADER_TYPE BH;
        };
        const int ALIGNED_BLOCK_OVERHEAD = sizeof(AlignedBlockHeader) + BLOCK_TRAILER_SIZE;

#if defined(PRINT_LEAK_REPORT)
        AlignedBlockHeader* d_pFirstAlignedBlock = 0;
#endif

        UINT32 d_AlignedBlockNum;   // how many aligned blocks were allocated

        //--------------------------------------------------------------------------
        // Private functions:

        void Initialize();

        // Get the closest power of 2 for the given size
        UINT32 GetArrayIndex(UINT32 Size);

        // Get the index if block
        UINT32 GetBlockIndex(PoolHeader * pPH, void * pBlock);
        void * GetBlockAddress(PoolHeader * pPH, UINT32 BlockIndex);

        //
        // Allocate a block, mark it used, return its address in pAddress.
        // Returns error if we had to allocate a new pool, and the alloc failed,
        // otherwise returns OK.
        //
        RC FindFreeSpace(UINT32 Index, void ** pAddress);

        char *GetPool(PoolHeader * pPH);

        //
        // Allocate a new pool of 32 blocks, mark them free, add to the list.
        //
        RC AddPool(UINT32 Index);

        // Debugging/info fuctions
        struct PoolStats
        {
            UINT32 numPools;
            UINT32 freeBlocks;
            UINT32 usedBlocks;
            UINT64 freeBytes;
            UINT64 usedBytes;
        };
        PoolStats GetPoolStats(UINT32 Index);
        PoolStats GetTotalPoolStats();
        PoolHeader *GetFirstPoolHeader(UINT32 Index);
        PoolHeader *GetNextPoolHeader(UINT32 Index, PoolHeader *pPrevPoolHeader);
        void LeakReport();
        RC PrintPoolDetail( UINT32 Index );
        RC PrintAllPool();
        RC PrintAllSeq(UINT32 FirstSeq);
        RC SpaceAnalysis( UINT32 Index );
        JSBool UsedBlocksGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
        JSBool FreeBlocksGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
        JSBool UsedBytesGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
        JSBool FreeBytesGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);

        inline void IncrementAllocations(size_t Size)
        {
            if (true == d_TraceEnabled)
            {
                POOL_ASSERT(d_IsInitialized);
                ++d_Counters[CID_ALLOC];
                d_Counters[CID_BYTES] += Size;
                if (d_Counters[CID_BYTES] > d_Counters[CID_MAXBYTES])
                    d_Counters[CID_MAXBYTES] = d_Counters[CID_BYTES];
            }
        }

        inline void DecrementAllocations(size_t Size)
        {
            if (true == d_TraceEnabled)
            {
                POOL_ASSERT(d_IsInitialized);
                --d_Counters[CID_ALLOC];
                d_Counters[CID_BYTES] -= Size;
            }
        }

        //! Insert a node into a doubly-linked list.  The nodes must be
        //! linked by a pPrev and pNext element.
        //!
        //! \param pNode The node to insert.
        //! \param ppHead On entry, *ppHead points at the node at the
        //!     head of the list.  On exit, it points at the new head.
        template<typename T> void InsertIntoList(T *pNode, T **ppHead)
        {
            pNode->pPrev = NULL;
            pNode->pNext = *ppHead;
            if (pNode->pNext)
            {
                pNode->pNext->pPrev = pNode;
            }
            *ppHead = pNode;
        }

        //! Remove a node from a doubly-linked list.  The nodes must be
        //! linked by a pPrev and pNext element.
        //!
        //! \param pNode The node to insert.
        //! \param ppHead On entry, *ppHead points at the node at the
        //!     head of the list.  On exit, it points at the new head.
        template<typename T> void RemoveFromList(T *pNode, T **ppHead)
        {
            if (pNode->pPrev)
            {
                POOL_ASSERT(*ppHead != pNode);
                pNode->pPrev->pNext = pNode->pNext;
            }
            else
            {
                POOL_ASSERT(*ppHead == pNode);
                *ppHead = pNode->pNext;
            }
            if (pNode->pNext)
            {
                pNode->pNext->pPrev = pNode->pPrev;
            }
            pNode->pPrev = NULL;
            pNode->pNext = NULL;
        }

        //! Return the last node in a doubly-linked list
        template<typename T> T *GetTailOfList(T *pHead)
        {
            T *pTail = NULL;
            for (T *pNode = pHead; pNode; pNode = pNode->pNext)
                pTail = pNode;
            return pTail;
        }
    }
}

// Use the new/delete overrides only if MODS allocator is included
#if defined(INCLUDE_MODSALLOC)

// route calls to new/delete through Pool. should this change, keep in mind that
// memcheck.h also defines an overloaded new/delete for tracking line and file

//------------------------------------------------------------------------------
#if defined(MODS_INLINE_OPERATOR_NEW)
void* ModsNew(size_t size)
#else
void * operator new(size_t size)
#endif
{
    void *addr = Pool::Alloc(size);
    POOL_ADDTRACE(addr, size, Pool::CID_NEW);
    return addr;
}

#ifndef MODS_INLINE_OPERATOR_NEW
void * operator new(size_t size, const std::nothrow_t&) throw()
{
    void *addr = Pool::Alloc(size);
    POOL_ADDTRACE(addr, size, Pool::CID_NEW);
    return addr;
}
#endif
//------------------------------------------------------------------------------
#if defined(MODS_INLINE_OPERATOR_NEW)
void* ModsNewArray(size_t size)
#else
void * operator new[](size_t size)
#endif
{
    void *addr = Pool::Alloc(size);
    POOL_ADDTRACE(addr, size, Pool::CID_BLOCK);
    return addr;
}

//------------------------------------------------------------------------------
#if defined(MODS_INLINE_OPERATOR_NEW)
void ModsDelete(void* p)
#else
void operator delete(void * p) throw()
#endif
{
    POOL_REMOVETRACE(p, Pool::CID_NEW);
    Pool::Free(p);
}

#ifndef MODS_INLINE_OPERATOR_NEW
void operator delete(void * p, const std::nothrow_t&) throw()
{
    POOL_REMOVETRACE(p, Pool::CID_NEW);
    Pool::Free(p);
}
#endif
//------------------------------------------------------------------------------
#if defined(MODS_INLINE_OPERATOR_NEW)
void ModsDeleteArray(void* p)
#else
void operator delete[](void * p) throw()
#endif
{
    POOL_REMOVETRACE(p, Pool::CID_BLOCK);
    Pool::Free(p);
}

#endif // defined(INCLUDE_MODSALLOC)

//------------------------------------------------------------------------------
void Utility::Pool::Initialize()
{
    PoolLock Lock;

    // Initialize the pool list pointers to null, pool counts to 0.
    for (UINT32 i = 0; i < NUM_POOL_LISTS; ++i)
    {
        d_PoolLists[i].pFreeList = NULL;
        d_PoolLists[i].pFullList = NULL;
    }

    // Set the dummy pool header ptrs to known values.
    d_pAlignedDummy   = (PoolHeader*)(&d_pAlignedDummy);
    d_AlignedBlockNum = 0;

    d_IsInitialized = true;
    memset(d_Counters, 0, sizeof(INT64) * NUM_COUNTERS);

    // Make sure all debug and release data structures that must be 8 or 16-byte
    // aligned, are 8 or 16-byte aligned.
    POOL_ASSERT(0 == ((POOL_ALIGNMENT-1) & sizeof(PoolHeader)));
    POOL_ASSERT(0 == ((POOL_ALIGNMENT-1) & sizeof(BlockHeader)));
    POOL_ASSERT(0 == ((POOL_ALIGNMENT-1) & sizeof(DebugBlockHeader)));
    POOL_ASSERT(0 == ((POOL_ALIGNMENT-1) & sizeof(DebugBlockTrailer)));
}

//------------------------------------------------------------------------------
UINT32 Utility::Pool::GetArrayIndex(UINT32 Size)
{
    POOL_ASSERT(Size <= (1 << (NUM_POOL_LISTS-1)));

    //
    // Find the index of the first pool list that contains blocks
    // as big or bigger than Size.
    //
    // The list at index 0 contains blocks of size 1 byte.
    //

    UINT32 Index = 0;

    if (0 == (Size & (Size-1)))
    {
        // Size is a power of 2, it will fit exactly.
        Size >>= 1;
    }

    while (Size)
    {
        Index++;
        Size >>= 1;
    }

    //
    // To preserve 8 or 16-byte alignment, force very small allocations to use the
    // same list as 8 or 16-byte blocks.
    //
    if (Index < POOL_ALIGNMENT_INDEX)
        Index = POOL_ALIGNMENT_INDEX;

    return Index;
}

//------------------------------------------------------------------------------
UINT32 Utility::Pool::GetBlockIndex(PoolHeader * pPH, void * pBlock)
{
    // ASSUMES that the managed memory is contiguous with the block header!

    char * pAddr  = (char *)pBlock;
    char * pPool  = GetPool(pPH);
    size_t offset = pAddr - pPool;

    UINT32 ActualBlockSize = pPH->Size + BLOCK_OVERHEAD;

    // pBlock must be inside the pool.

    POOL_ASSERT(pAddr >= pPool);
    POOL_ASSERT(pAddr < (pPool + ActualBlockSize * NUM_BLOCK));

    // pBlock must point to the beginning of a block.

    POOL_ASSERT(0 == offset % ActualBlockSize);

    return UINT32(offset / ActualBlockSize);
}

//------------------------------------------------------------------------------
void * Utility::Pool::GetBlockAddress(PoolHeader * pPH, UINT32 BlockIndex)
{
    POOL_ASSERT(BlockIndex < NUM_BLOCK);

    // ASSUMES that the managed memory is contiguous with the block header!

    char * Pool             = GetPool(pPH);
    UINT32 ActualBlockSize  = pPH->Size + BLOCK_OVERHEAD;

    return Pool + BlockIndex * ActualBlockSize;
}

//------------------------------------------------------------------------------
RC Utility::Pool::FindFreeSpace(UINT32 Index, void ** pAddress)
{
    POOL_ASSERT(Index < NUM_POOL_LISTS);
    POOL_ASSERT(Index >= POOL_ALIGNMENT_INDEX); // smallest blocksize is 8 or 16
    // bytes (1<<3 or 1<<4).

    //
    // If there are no free blocks in the list, alloc a new pool.
    //
    if (NULL == d_PoolLists[Index].pFreeList)
    {
        RC rc = AddPool(Index);
        if (OK != rc)
            return rc;
    }

    //
    // Find a free block
    //
    PoolHeader *pLwrrent = d_PoolLists[Index].pFreeList;
    POOL_ASSERT(pLwrrent != NULL);
    POOL_ASSERT(pLwrrent->FreeMask != 0);

    UINT32 BlockIndex = 0;
    while ((pLwrrent->FreeMask & (1 << BlockIndex)) == 0)
    {
        ++BlockIndex;
    }

    //
    // Mark the block in use.  If all the blocks are now in use, move
    // the pool from pFreeList to pFullList.
    //
    pLwrrent->FreeMask &= ~(1 << BlockIndex);
    if (pLwrrent->FreeMask == 0)
    {
        RemoveFromList(pLwrrent, &d_PoolLists[Index].pFreeList);
        InsertIntoList(pLwrrent, &d_PoolLists[Index].pFullList);
    }

    *pAddress = GetBlockAddress(pLwrrent, BlockIndex);

    return OK;
}

//------------------------------------------------------------------------------
char * Utility::Pool::GetPool(PoolHeader *pPH)
{
    char *Pool = (char *)(pPH + 1);

    // All pools always start on the next POOL_ALIGNMENT-byte position after
    // the end of the pool header
    size_t Offset = ((size_t)Pool & (POOL_ALIGNMENT - 1));
    if (Offset > 0)
    {
        Pool += (POOL_ALIGNMENT - Offset);
    }

    return Pool;
}

//------------------------------------------------------------------------------
RC Utility::Pool::AddPool(UINT32 Index)
{
    POOL_ASSERT(Index < NUM_POOL_LISTS);
    POOL_ASSERT(Index >= POOL_ALIGNMENT_INDEX);

    UINT32 BlockSize        = 1 << Index;
    UINT32 ActualBlockSize  = BlockSize + BLOCK_OVERHEAD;
    UINT32 TotalSize        = sizeof(PoolHeader) + ActualBlockSize * NUM_BLOCK +
        POOL_ALIGNMENT - 1;

    //
    // Allocate memory for the pool
    //
    void * Address = malloc(TotalSize);
    if ( !Address )
    {
        return RC::POOL_NOT_ENOUGH_MEMORY;
    }

    //
    // Initialize Pool Header, add to head of the list.
    //
    PoolHeader * pHeader = (PoolHeader *)Address;
    pHeader->Size        = BlockSize;
    pHeader->FreeMask    = (UINT32)~0;
    InsertIntoList(pHeader, &d_PoolLists[Index].pFreeList);

    //
    // Initialize the blocks in the pool.
    //
    char * Pool = GetPool(pHeader);
    for (UINT32 block = 0; block < NUM_BLOCK; block++)
    {
        BLOCK_HEADER_TYPE * pBlock =
            (BLOCK_HEADER_TYPE *)(Pool + block * ActualBlockSize);
        pBlock->MyPool = pHeader;
    }

    return OK;
}

//------------------------------------------------------------------------------
void * Utility::Pool::Alloc( size_t Size )
{
    if ( !d_IsInitialized )
        Initialize();

    char * Address;

    MASSERT(Size < 0xFFFFFFFF); // Implementation is prepared only for UINT32

    if (Size > (1 << (NUM_POOL_LISTS-1)))
    {
        // Too big.  Use AllocAligned() to be sure the data is allocated on
        // an 8 or 16-byte boundary.
        return AllocAligned(Size, POOL_ALIGNMENT);
    }

    PoolLock Lock;

    // Alloc from a pool.

    UINT32 Index = GetArrayIndex(UINT32(Size));

    // Alloc is called while we are cleaning up after the user aborted
    // the script and therefore we need to check FindFreeSpace RC for
    // a non user abort error.
    RC rc = FindFreeSpace(Index, (void**)&Address);
    if ((rc != OK) && (rc != RC::USER_ABORTED_SCRIPT))
    {
        //
        // I'm putting an assert here, since I don't trust all of mods
        // to check for a failed new().
        // If this causes problems, remove this assert.
        //
        POOL_ASSERT(0);
        return 0;
    }
    POOL_ASSERT(0 == ((POOL_ALIGNMENT-1) & (Address - ((char*)0)))); // 8 or 16-byte aligned

    BLOCK_HEADER_TYPE * pbh = (BLOCK_HEADER_TYPE *)Address;
    pbh->AllocSize       = (UINT32)Size;

    WriteDebugGuardBands(pbh);

    void* tmp = (void *)(Address + sizeof(BLOCK_HEADER_TYPE));
    IncrementAllocations(Size);

    return tmp;
}

//------------------------------------------------------------------------------
void * Utility::Pool::AllocAligned( size_t Size, size_t Align, size_t Offset /* =0 */)
{
    if ( !d_IsInitialized )
        Initialize();

    MASSERT(0 == (Align & (Align-1))); // Align should be a power of 2.
    MASSERT(Offset < Align);           // Offset must be < Align.
    MASSERT(Size < 0xFFFFFFFF); // Implementation is prepared only for UINT32
    MASSERT(Align < 0xFFFFFFFF);
    MASSERT(Offset < 0xFFFFFFFF);

    if ((Size <= (1 << (NUM_POOL_LISTS-1))) &&
        (Align <= 8) && (Offset == 0))
    {
        // Alloc is already 8-byte aligned.
        void * p = Alloc(Size);

        return p;
    }

    PoolLock Lock;

    //
    // Allocate extra space to allow for aligning later.
    //
    char * Address = (char *)malloc(Size + ALIGNED_BLOCK_OVERHEAD + Align - 1);

    if (0 == Address)
    {
        //
        // I'm putting an assert here, since I don't trust all of mods
        // to check for a failed new().
        // If this causes problems, remove this assert.
        //
        if (d_AllocAssert)
        {
            POOL_ASSERT(0);
        }
        return 0;
    }

    //
    // Callwlate the properly aligned return address.
    //
    char * ReturnAddress = Address + sizeof(AlignedBlockHeader);
    size_t LwrOffset     = ((size_t)ReturnAddress) & (Align-1);
    d_SizeAligned += (UINT32) (Size+ALIGNED_BLOCK_OVERHEAD);

    if (LwrOffset > Offset)
        Offset += Align;

    ReturnAddress += (Offset - LwrOffset);

    //
    // Fill in headers.
    //
    AlignedBlockHeader * pabh = (AlignedBlockHeader*)
        (ReturnAddress - sizeof(AlignedBlockHeader));
    pabh->FreePtr      = Address;
    pabh->BH.MyPool    = d_pAlignedDummy;
    pabh->BH.AllocSize = (UINT32)Size;
    d_AlignedBlockNum++;
#if defined(PRINT_LEAK_REPORT)
    InsertIntoList(pabh, &d_pFirstAlignedBlock);
#endif

    WriteDebugGuardBands(&pabh->BH);
    IncrementAllocations(Size);

    return ReturnAddress;
}

//------------------------------------------------------------------------------
void * Utility::Pool::Realloc
(
    volatile void *  OldPtr,
    size_t           NewSize,
    Pool::CounterIdx cid        // = CID_ALLOC
    )
{
    if ( !d_IsInitialized )
        Initialize();

    if (OldPtr && (0 == NewSize))
    {
        POOL_REMOVETRACE(const_cast<void *>(OldPtr), cid);
        Free(OldPtr);
        return 0;
    }
    if (!OldPtr)
    {
        void *addr = Alloc(NewSize);
        POOL_ADDTRACE(addr, NewSize, cid);
        return addr;
    }

    volatile char * Address = (volatile char *)OldPtr;
    volatile BLOCK_HEADER_TYPE * pBH = (volatile BLOCK_HEADER_TYPE *)(Address - sizeof(BLOCK_HEADER_TYPE));

    CheckDebugGuardBands(pBH);

    if(NewSize <= pBH->AllocSize)
    {
        // We can continue to use the old block, no copy required.
        return const_cast<void *>(OldPtr);
    }

    void *NewPtr = Alloc(NewSize);
    if (!NewPtr)
        return NULL;
    POOL_ADDTRACE(NewPtr, NewSize, cid);
    memcpy(NewPtr, const_cast<void *>(OldPtr), pBH->AllocSize);

    // @@@ WAR for GL bug 110587
    // Init the grown region.
    memset((pBH->AllocSize + (char *)NewPtr), 0xEE, NewSize - pBH->AllocSize);

    POOL_REMOVETRACE(const_cast<void *>(OldPtr), cid);
    Free(OldPtr);

    return NewPtr;
}

//------------------------------------------------------------------------------
void Utility::Pool::Free( volatile void * Addr )
{
    if (0 == Addr)
        return;

    char * Address    = (char *)const_cast<void *>(Addr);
    BLOCK_HEADER_TYPE * pBH = (BLOCK_HEADER_TYPE *)(Address - sizeof(BLOCK_HEADER_TYPE));
    PoolHeader * pPH  = pBH->MyPool;
    size_t AllocSize = pBH->AllocSize;

    CheckDebugGuardBands(pBH);

#if defined(POOL_DEBUG)

    //
    // Overwrite the memory with a known pattern, to cause a crash
    // in code that uses freed memory.
    //
    memset(const_cast<void *>(Addr), 0xDD, pBH->AllocSize);

#endif // POOL_DEBUG

    PoolLock Lock;

    if (pPH == d_pAlignedDummy)
    {
        // This block was gotten from AllocAligned.  We need to back up
        // one more void* to get the actual malloc address.
        AlignedBlockHeader * pABH = (AlignedBlockHeader*)
            (Address - sizeof(AlignedBlockHeader));
        d_SizeAligned -= (pABH->BH.AllocSize+ALIGNED_BLOCK_OVERHEAD);
#if defined(PRINT_LEAK_REPORT)
        RemoveFromList(pABH, &d_pFirstAlignedBlock);
#endif
        free(pABH->FreePtr);
        d_AlignedBlockNum--;
    }
    else
    {
        // This block is in a pool.  Find the array index and block index.
        UINT32 ArrayIndex = GetArrayIndex(pPH->Size);
        UINT32 BlockIndex = GetBlockIndex(pPH, (void *)pBH);

        // Check for freeing an already free block.
        POOL_ASSERT(0 == (pPH->FreeMask & (1 << BlockIndex)));

        // Mark the block free.
        UINT32 oldFreeMask = pPH->FreeMask;
        pPH->FreeMask |= (1 << BlockIndex);

        if (oldFreeMask == 0)
        {
            // If the pool used to be full, then move it from pFullList
            // to pFreeList.
            //
            RemoveFromList(pPH, &d_PoolLists[ArrayIndex].pFullList);
            InsertIntoList(pPH, &d_PoolLists[ArrayIndex].pFreeList);
        }
        else if (pPH->FreeMask == (UINT32)~0)
        {
            // If the pool is now empty, consider returning it to the
            // system heap.  But to avoid thrashing, don't leave the
            // list with 0 free blocks.  So check whether pFreeList has
            // at least two pools before freeing one.
            //
            POOL_ASSERT(d_PoolLists[ArrayIndex].pFreeList != NULL);
            if (d_PoolLists[ArrayIndex].pFreeList->pNext != NULL)
            {
                RemoveFromList(pPH, &d_PoolLists[ArrayIndex].pFreeList);
                free(pPH);
            }
        }
    }

    DecrementAllocations(AllocSize);
}

INT64 Utility::Pool::GetCount(CounterIdx type)
{
    return d_Counters[type];
}

#if defined(POOL_DEBUG)
//------------------------------------------------------------------------------
void Utility::Pool::WriteDebugGuardBands(DebugBlockHeader * pbh)
{
#if defined(PRINT_LEAK_REPORT)
    // At the end of the call stack everybody (Alloc and AllocAligned) uses
    // this to check the canaries, so I can check here also the leaks
    CheckSeqNumber();
#endif

    UINT08 * pbt = ((UINT08*)(pbh+1)) + pbh->AllocSize;
    pbh->GuardBandA      = GUARD_BAND_A_VALUE;
#ifdef LW_64_BITS
    pbh->GuardBandB[0]   = GUARD_BAND_B_VALUE;
    pbh->GuardBandB[1]   = GUARD_BAND_B_VALUE;
    pbh->GuardBandB[2]   = GUARD_BAND_B_VALUE;
#endif
    pbt[3]   = GUARD_BAND_C_VALUE_3;
    pbt[2]   = GUARD_BAND_C_VALUE_2;
    pbt[1]   = GUARD_BAND_C_VALUE_1;
    pbt[0]   = GUARD_BAND_C_VALUE_0;

#ifdef DETECT_NEGATIVE_LEAKS
    if (!d_TraceEnabled)
    {
        d_SequenceCounter = 0;
    }
#endif

    pbt[7]   = (UINT08) (d_SequenceCounter >> 24);
    pbt[6]   = (UINT08) (d_SequenceCounter >> 16);
    pbt[5]   = (UINT08) (d_SequenceCounter >> 8);
    pbt[4]   = (UINT08) (d_SequenceCounter);

    pbh->SeqNum = d_SequenceCounter;

    ++d_SequenceCounter;
}

//------------------------------------------------------------------------------
void Utility::Pool::CheckDebugGuardBands(volatile DebugBlockHeader * pBH)
{
    volatile PoolHeader * pPH  = pBH->MyPool;

    // Check for block header overwritten (gba).
    POOL_ASSERT(pBH->GuardBandA == GUARD_BAND_A_VALUE);
#ifdef LW_64_BITS
    POOL_ASSERT(pBH->GuardBandB[0] == GUARD_BAND_B_VALUE);
    POOL_ASSERT(pBH->GuardBandB[1] == GUARD_BAND_B_VALUE);
    POOL_ASSERT(pBH->GuardBandB[2] == GUARD_BAND_B_VALUE);
#endif

    // Check for block header overwritten (size).
    if (pPH == d_pAlignedDummy)
    {
        // Any AllocSize is legal here.
    }
    else
    {
        POOL_ASSERT(pBH->AllocSize <= pBH->MyPool->Size);
    }

    volatile UINT08 * pBT = ((volatile UINT08*)(pBH+1)) + pBH->AllocSize;

    // Check for block trailer overwritten (gbb).
    POOL_ASSERT(pBT[3] == GUARD_BAND_C_VALUE_3);
    POOL_ASSERT(pBT[2] == GUARD_BAND_C_VALUE_2);
    POOL_ASSERT(pBT[1] == GUARD_BAND_C_VALUE_1);
    POOL_ASSERT(pBT[0] == GUARD_BAND_C_VALUE_0);

#ifdef DETECT_NEGATIVE_LEAKS
    if (d_TraceEnabled &&
        (pBT[4] == 0) &&
        (pBT[5] == 0) &&
        (pBT[6] == 0) &&
        (pBT[7] == 0))
    {
        // This will result in negative leak count (or hide real leaks)
        MASSERT(!"Allocation was created when tracing was disabled");
    }
#endif
}
#endif // POOL_DEBUG

//------------------------------------------------------------------------------
//! \brief Get statistics about one pool list
//!
//! Performance note: this method should only be needed for
//! leak-checking at the end of mods, or for debugging.  So it should
//! be faster & cleaner to let this method run in O(n), rather than
//! trying to update the statistics on every alloc & free.
//!
Utility::Pool::PoolStats Utility::Pool::GetPoolStats(UINT32 Index)
{
    UINT32 freeBlocks = 0;
    UINT32 usedBlocks = 0;

    for (const PoolHeader *pNode = d_PoolLists[Index].pFreeList;
         pNode; pNode = pNode->pNext)
    {
        POOL_ASSERT(pNode->FreeMask != 0);
        UINT32 freeBlocksInNode = Utility::CountBits(pNode->FreeMask);
        freeBlocks += freeBlocksInNode;
        usedBlocks += (NUM_BLOCK - freeBlocksInNode);
    }
    for (const PoolHeader *pNode = d_PoolLists[Index].pFullList;
         pNode; pNode = pNode->pNext)
    {
        POOL_ASSERT(pNode->FreeMask == 0);
        usedBlocks += NUM_BLOCK;
    }

    PoolStats poolStats = {0};
    poolStats.numPools = (freeBlocks + usedBlocks) / NUM_BLOCK;
    poolStats.freeBlocks = freeBlocks;
    poolStats.usedBlocks = usedBlocks;
    poolStats.freeBytes = freeBlocks * (1ULL << Index);
    poolStats.usedBytes = usedBlocks * (1ULL << Index);
    return poolStats;
}

//------------------------------------------------------------------------------
//! \brief Get total statistics for all pool lists
//!
Utility::Pool::PoolStats Utility::Pool::GetTotalPoolStats()
{
    PoolStats totalStats = {0};
    for (UINT32 ii = 0; ii < NUM_POOL_LISTS; ++ii)
    {
        PoolStats poolStats = GetPoolStats(ii);
        totalStats.numPools += poolStats.numPools;
        totalStats.freeBlocks += poolStats.freeBlocks;
        totalStats.usedBlocks += poolStats.usedBlocks;
        totalStats.freeBytes += poolStats.freeBytes;
        totalStats.usedBytes += poolStats.usedBytes;
    }
    return totalStats;
}

//------------------------------------------------------------------------------
// Colwenience functions to traverse all PoolHeaders under a PoolList.
// A loop of the form "for (x = GetFirstPoolHeader; x != NULL; x =
// GetNextPoolHeader)" iterates through all members of pFreeList and
// pFullList, in that order.
//
// Should only be needed for debug functions, since ordinary allocs
// and frees shouldn't need to traverse any lists.
//
Utility::Pool::PoolHeader *Utility::Pool::GetFirstPoolHeader(UINT32 Index)
{
    return (d_PoolLists[Index].pFreeList ?
            d_PoolLists[Index].pFreeList :
            d_PoolLists[Index].pFullList);
}
Utility::Pool::PoolHeader *Utility::Pool::GetNextPoolHeader
(
    UINT32 Index,
    PoolHeader *pPrevPoolHeader
)
{
    if (pPrevPoolHeader)
    {
        if (pPrevPoolHeader->pNext)
        {
            // Return next element in list, if any
            return pPrevPoolHeader->pNext;
        }
        else if (pPrevPoolHeader->FreeMask != 0)
        {
            // If we're at the end of pFreeList, then return first
            // element of pFullList
            return d_PoolLists[Index].pFullList;
        }
    }
    return NULL;
}

//------------------------------------------------------------------------------
void Utility::Pool::LeakReport()
{
#if defined(PRINT_LEAK_REPORT)
    // This function CANNOT call Printf, since that would require the use of
    // an object that has already been destructed at this point in time.  It
    // also shoul not print anything unless any leaks have oclwrred.
    UINT32 numValidMemoryLeaks = 0;
    if (d_AlignedBlockNum > 0)
    {
        printf("%d aligned blocks leaked\n", d_AlignedBlockNum);
        for (AlignedBlockHeader* pABH = d_pFirstAlignedBlock;
             pABH; pABH = pABH->pNext)
        {
            const bool unchecked = pABH->BH.SeqNum <
                d_FirstCheckedSequenceNumber;
            if (!unchecked)
            {
                numValidMemoryLeaks++;
            }

            printf("Memory leak%s: aligned block address= %p size= %u seqNum= %u\n",
                   (unchecked ? " (unchecked)" : ""),
                   (char*)(pABH) + sizeof(AlignedBlockHeader),
                   pABH->BH.AllocSize,
                   pABH->BH.SeqNum
                );
        }
        printf("\n");
    }
    for (UINT32 i = 0; i < NUM_POOL_LISTS; i++)
    {
        bool foundFreeEntry = false;

        // Walk the list of pools and print info about each leak
        for (PoolHeader *pLwrrent = GetFirstPoolHeader(i);
             pLwrrent; pLwrrent = GetNextPoolHeader(i, pLwrrent))
        {
            // We allow a single entry on the pool list with all
            // blocks free, since Pool::Free never returns the last
            // pool to the OS
            if (!foundFreeEntry || (pLwrrent->FreeMask == 0xFFFFFFFF))
            {
                foundFreeEntry = true;
                continue;
            }

            void * Addr = (void *)GetPool(pLwrrent);
            printf("Pool leak: header %p, free mask 0x%08x, addr %p, "
                   "block size %d\n",
                   pLwrrent, pLwrrent->FreeMask, Addr, 1 << i);
            for (UINT32 j = 0; j < NUM_BLOCK; j++)
            {
                if (!(pLwrrent->FreeMask & (1 << j))) {
                    // I have to return the pointer to the block not to the
                    // canary
                    char* ptrBase = (char*)(GetBlockAddress(pLwrrent, j));

                    const bool unchecked =
                        ((DebugBlockHeader*)ptrBase)->SeqNum <
                            d_FirstCheckedSequenceNumber;
                    if (!unchecked)
                    {
                        numValidMemoryLeaks++;
                    }

                    printf("Memory leak%s: address= %p size= %d seqNum= %u\n",
                           (unchecked ? " (unchecked)" : ""),
                           (char*)(ptrBase + sizeof(BLOCK_HEADER_TYPE)),
                           1 << i,
                           ((DebugBlockHeader *)(ptrBase))->SeqNum
                        );
                }
            }
        }
        printf("\n");
    }
    MASSERT(numValidMemoryLeaks == 0);
#endif // PRINT_LEAK_REPORT
}

//------------------------------------------------------------------------------
RC Utility::Pool::PrintPoolDetail( UINT32 Index )
{
    PoolStats poolStats = GetPoolStats(Index);
    Printf(Tee::PriNormal, "Pool for Index %i (blocksize %d):\n",
           Index,
           1 << Index
        );
    Printf(Tee::PriNormal, "#Pool %i #Total %i, #Free %i, #Used %i\n",
           poolStats.numPools,
           poolStats.freeBlocks + poolStats.usedBlocks,
           poolStats.freeBlocks,
           poolStats.usedBlocks
        );

    if (poolStats.numPools == 0)
    {
        Printf(Tee::PriNormal, "Empty Pool\n");
    }

    for (PoolHeader *pLwrrent = GetFirstPoolHeader(Index);
         pLwrrent; pLwrrent = GetNextPoolHeader(Index, pLwrrent))
    {
        void * Addr = (void *)GetPool(pLwrrent);

        Printf(Tee::PriNormal, "0x%p\t0x%08x\t0x%p\t0x%08X\n",
               (void*)pLwrrent,
               pLwrrent->FreeMask,
               Addr,
               Platform::VirtualToPhysical32(Addr)
            );
    }
    Printf(Tee::PriNormal, "\n");
    return OK;
}

//------------------------------------------------------------------------------
RC Utility::Pool::PrintAllPool()
{
    Printf(Tee::PriNormal, "All Pool Info:\n");
    Printf(Tee::PriNormal, "Index\t#Size\t#Pool\t#Total\t#Free\t#Used\n");

    for (UINT32 i = 0; i < NUM_POOL_LISTS; i++)
    {
        PoolStats poolStats = GetPoolStats(i);
        Printf(Tee::PriNormal, "%i\t%i\t%i\t%i\t%i\t%i\n",
               i,
               (1 << i),
               poolStats.numPools,
               poolStats.freeBlocks + poolStats.usedBlocks,
               poolStats.freeBlocks,
               poolStats.usedBlocks
            );
    }

    PoolStats totalStats = GetTotalPoolStats();
    Printf(Tee::PriNormal,
           "Total blocks/free/used: %i/%i/%i\n",
           totalStats.freeBlocks + totalStats.usedBlocks,
           totalStats.freeBlocks,
           totalStats.usedBlocks
        );
    Printf(Tee::PriNormal,
           "Total bytes/free/used: %lli/%lli/%lli\n",
           totalStats.freeBytes + totalStats.usedBytes,
           totalStats.freeBytes,
           totalStats.usedBytes
        );
    Printf(Tee::PriNormal, "Num of Aligned Blocks = %i, pDummy 0x%p, size %i\n",
           d_AlignedBlockNum,
           d_pAlignedDummy,
           d_SizeAligned);

    Printf(Tee::PriNormal, "\n");
    return OK;
}

// TestAlloc is used to test if we can allocate given size of memory.
// This function is mainly used for debugging purpose. It can be inserted in
// different parts of MODS to pinpoint the memory alloc bottleneck.
void Utility::Pool::TestAlloc(const char* msg, size_t size)
{
    static int cpt_num = 0;
    Printf(Tee::PriNormal, "CHECKPOINT %i(%s), requested %iM of memory...",
           ++cpt_num, msg, (int)size);
    UINT08 *test_data = (UINT08*)Pool::Alloc(size*1024*1024);

    // Won't come here if allocation fails
    Printf(Tee::PriNormal, "Done\n");
    Pool::PrintAllPool();
    Pool::Free( test_data );
}

bool Utility::Pool::CanAlloc(size_t size)
{
    // This is just a resources check; turn off the assert on failure.
    bool saveAllocAssert = d_AllocAssert;
    d_AllocAssert = false;
    UINT08 *test_data = (UINT08*)Pool::Alloc(size*1024*1024);
    d_AllocAssert = saveAllocAssert;
    if (test_data)
    {
        Pool::Free(test_data);
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------------
RC Utility::Pool::PrintAllSeq(UINT32 FirstSeq)
{
#if defined(POOL_DEBUG)
    Printf(Tee::PriNormal, "Blocks >= seq %d:\n", FirstSeq);
    Printf(Tee::PriNormal, "Seq\tSize\tvaddr\n");

    for (int i = NUM_POOL_LISTS-1; i >= 0; i--)
    {
        for (PoolHeader *pph = d_PoolLists[i].pFreeList;
             pph != NULL; pph = pph->pNext)
        {
            int j;
            for (j = 0; j < (int)NUM_BLOCK; j++)
            {
                if (!(pph->FreeMask & (1<<j)))
                {
                    DebugBlockHeader *  pbh = (DebugBlockHeader *) GetBlockAddress(pph, j);
                    UINT08 * pbt = ((UINT08 *)(pbh+1) + pbh->AllocSize);
                    UINT32 Sequence = (pbt[7] << 24) | (pbt[6] << 16) | (pbt[5] << 8) | (pbt[4]);

                    if (Sequence >= FirstSeq)
                    {
                        Printf(Tee::PriNormal, "%u\t%u\t%p\n",
                               Sequence,
                               pbh->AllocSize,
                               pbh+1
                            );
                    }
                }
            }
        }
    }
#endif
    return OK;
}

//------------------------------------------------------------------------------
RC Utility::Pool::SpaceAnalysis( UINT32 Index )
{
    return OK;
}

//------------------------------------------------------------------------------
#if DEBUG_TRACE_LEVEL >= 1
static volatile unsigned long s_BreakAlloc = (unsigned long)-1;
map<void *, void *> s_NonPoolAllocs; // map real allocation to proxy

//------------------------------------------------------------------------------
void Utility::Pool::PrintDebugTrace(DebugBlockHeader *header)
{
    Printf(Tee::PriNormal, "\tAddr:  %p\n",   HEADER2PTR(header));
    Printf(Tee::PriNormal, "\tSize:  %d\n",   header->AllocSize);
    Printf(Tee::PriNormal, "\tFile:  %s\n",   header->FileName);
    Printf(Tee::PriNormal, "\tLine:  %d\n",   header->LineNumber);
    Printf(Tee::PriNormal, "\tIndx:  %d\n",   header->SeqNum);

#if DEBUG_TRACE_LEVEL >= 2
    // Start at level 2, no need to print Pool::AddTrace or StackDebug::Create
    Printf(Tee::PriNormal, "\tStack:\n");
    size_t levels = StackDebug::GetDepth(header->Stack);
    for (size_t i = 2; i < levels; ++i)
    {
        Printf(Tee::PriNormal, "\t%d: %s\n", (int)(i - 1),
               StackDebug::Decode(header->Stack, i).c_str());
    }
#endif
}

//------------------------------------------------------------------------------
void Utility::Pool::DestroyDebugTrace(DebugBlockHeader *header)
{
#if DEBUG_TRACE_LEVEL >= 2
    if (header)
    {
        StackDebug::Destroy(header->Stack);
    }
#endif
}

//------------------------------------------------------------------------------
void Utility::Pool::AddTrace(void *p, size_t size, const char *file, long line, CounterIdx type)
{
    PoolLock Lock;

    POOL_ASSERT(NULL != p);
    POOL_ASSERT(NULL != file);

    // Some allocations (prior to leak check initialization) must be skipped
    if (false == d_TraceEnabled)
        return;

    if (type != CID_ALLOC)
        ++d_Counters[type];

    // Verify that this address does in fact belong to Pool
    DebugBlockHeader *header = PTR2HEADER(p);
    POOL_ASSERT(header->GuardBandA == GUARD_BAND_A_VALUE);

    // Allow us to easily break on a given allocation index
    POOL_ASSERT(s_BreakAlloc != header->SeqNum);

    // Insert debug trace information into the debug header
    header->AllocSize  = size;
    header->Type       = type;
    header->LineNumber = line;
    header->FileName   = file;
    header->LeakMagic  = LEAK_MAGIC_VALUE;

    /* form a doubly-linked list of allocation traces */
    InsertIntoList(header, &d_pTraceHead);

#if DEBUG_TRACE_LEVEL >= 2
    header->Stack = StackDebug::Create();
#endif
}

//------------------------------------------------------------------------------
void Utility::Pool::RemoveTrace(void *p, CounterIdx type)
{
    PoolLock Lock;

    // Some allocations (prior to leak check initialization) must be skipped
    if ((false == d_TraceEnabled) || (NULL == p))
        return;

    // Verify that this address does in fact belong to Pool
    DebugBlockHeader *header = PTR2HEADER(p);
    POOL_ASSERT(header->GuardBandA == GUARD_BAND_A_VALUE);

    if (LEAK_MAGIC_VALUE != header->LeakMagic)
    {
        // Some allocations may occur before the leak checker initializes,
        // but are freed before leak checker shutdown, since we've verified
        // the guard band values, we can safely assume that is the case here
        return;
    }

    if (header->Type != CID_ALLOC)
        --d_Counters[header->Type];

    // Remove this allocation from the linked list
    RemoveFromList(header, &d_pTraceHead);

    // keep track of any instances of xxx/xxx[] mismatches
    if (((header->Type == CID_NEW) || (header->Type == CID_BLOCK)) &&
        (header->Type != type))
    {
        POOL_ASSERT(!"new/delete mismatch");
    }
    else
    {
        // nothing wrong, destroy the header
        DestroyDebugTrace(header);
    }
}

#endif // DEBUG_TRACE_LEVEL >= 1

//------------------------------------------------------------------------------
void Utility::Pool::LeakCheckInit()
{
    SetTraceEnabled(true);
}

//------------------------------------------------------------------------------
bool Utility::Pool::LeakCheckShutdown()
{
    SetTraceEnabled(false);

    Pool::LeakReport();

#if DEBUG_TRACE_LEVEL >= 1
    const char *labels[] = { "Leaked Allocs", "Leaked news", "Leaked new[]s",
                             "Leaked Externs", "Non-Pool Leaks", "Leaked Bytes",
                             "Maximum Held" };
    for (int i = 0; i < NUM_COUNTERS; ++i)
        Printf(Tee::PriNormal, "%s:\t%lld\n",
               labels[i], Pool::GetCount((Pool::CounterIdx)i));

    // Print the traces in chronological order (tail=>head)
    unsigned long counter = 0;
    for (DebugBlockHeader *header = GetTailOfList(d_pTraceHead);
         header; header = header->pPrev)
    {
        Printf(Tee::PriNormal, "Pool memory leak #%lu:\n", ++counter);
        PrintDebugTrace(header);
        DestroyDebugTrace(header);
    }

#endif // DEBUG_TRACE_LEVEL >= 1

    // Always report the number of leaks in debug spew, regardless of threshold
    INT64 allocs = Pool::GetCount(Pool::CID_ALLOC);
    if (allocs > 0)
    {
        Printf(Tee::PriDebug,
               "%lld leaks detected on MODS shutdown!\n",
               allocs);
    }

    // If the leaks exceed our specified threshold, signal an error
    if (Pool::d_Threshold >= 0)
    {
        // Set an environment variable with # of leaks
        char temp[32];
        sprintf(temp, "%lld,%lld", Pool::d_Threshold, allocs);
        Xp::SetElw("MODS_LEAK", temp);

        if (allocs > Pool::d_Threshold)
        {
            string LwrEC = Xp::GetElw("MODS_EC");
            if ((LwrEC == "") || (atoi(LwrEC.c_str()) == OK))
            {
                sprintf(temp, "%d", RC::EXCEEDED_MEMORY_LEAK_THRESHOLD);
                Xp::SetElw("MODS_EC", temp);
            }

            Printf(Tee::PriAlways,
                   "Leaks (%lld) don't match expected threshold (%lld) .\n",
                   allocs, Pool::d_Threshold);
            Printf(Tee::PriAlways, "                                                       \n"
                   " ###   ###  ######  ###   ###   #####   #####   ##  ## \n"
                   " ## # # ##  ##      ## # # ##  ##   ##  ##   #  ##  ## \n"
                   " ##  #  ##  ####    ##  #  ##  ##   ##  #####    ####  \n"
                   " ##     ##  ##      ##     ##  ##   ##  ##  ##    ##   \n"
                   " ##     ##  ######  ##     ##   #####   ##   ##   ##   \n"
                   "                                                       \n\n"
                   " ##         #######     #####     ##    ##    #######  \n"
                   " ##         ##        ##     ##   ##   ##    ##     ## \n"
                   " ##         ##        ##     ##   ##  ##      ##       \n"
                   " ##         ####      #########   ####          ##     \n"
                   " ##         ##        ##     ##   ##  ##          ##   \n"
                   " ########   ##        ##     ##   ##   ##    ##     ## \n"
                   " ########   #######   ##     ##   ##    ##    #######  \n");

            // Close the file sink so that the leak number print is guaranteed
            // to make it into the log file
            Tee::GetFileSink()->Close();
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
void Utility::Pool::SetTraceEnabled(bool enabled)
{
    d_TraceEnabled = enabled;
#if defined(POOL_DEBUG)
    if (enabled && d_FirstCheckedSequenceNumber == 0)
    {
        d_FirstCheckedSequenceNumber = d_SequenceCounter++;
    }
#endif
}

//------------------------------------------------------------------------------
void Utility::Pool::EnforceThreshold(INT64 threshold)
{
    d_Threshold = threshold;
}

//------------------------------------------------------------------------------
void Utility::Pool::AddNonPoolAlloc(void *p, size_t size)
{
    PROFILE_MALLOC(p, size)

    if ((false == d_TraceEnabled) || (NULL == p))
        return;
    if (!d_IsInitialized)
        Initialize();

    // Increment the normal counters as if we were Pool::Alloc
    PoolLock Lock;
    ++d_Counters[CID_NONPOOL];
    IncrementAllocations(size);

#if DEBUG_TRACE_LEVEL >= 1
    // Non-pool allocations aren't safe to inject with trace information, so we
    // force a dummy pool allocation here to create a proxy trace block instead.
    // Most importantly, this will give us a usable stack trace.
    POOL_ASSERT(s_NonPoolAllocs.find(p) == s_NonPoolAllocs.end());
    void *proxy = Pool::Alloc(1);
    Pool::AddTrace(proxy, 1, "<proxy>", -1, CID_ALLOC);
    s_NonPoolAllocs[p] = proxy;
#endif
}

//------------------------------------------------------------------------------
void Utility::Pool::RemoveNonPoolAlloc(void *p, size_t size)
{
    PROFILE_FREE(p)

    if ((false == d_TraceEnabled) || (NULL == p))
        return;

    // Decrement the normal counters as if we were Pool::Free
    PoolLock Lock;
    --d_Counters[CID_NONPOOL];
    DecrementAllocations(size);

#if DEBUG_TRACE_LEVEL >= 1
    // Free the proxy allocation and remove it from the non-pool alloc map.
    POOL_ASSERT(s_NonPoolAllocs.find(p) != s_NonPoolAllocs.end());
    void *proxy = s_NonPoolAllocs[p];
    Pool::RemoveTrace(proxy, CID_ALLOC);
    Pool::Free(proxy);
    s_NonPoolAllocs.erase(p);
#endif
}

//------------------------------------------------------------------------------
// Pool object, methods, and tests.
//------------------------------------------------------------------------------
JS_CLASS(Pool);

SObject Pool_Object
(
    "Pool",
    PoolClass,
    0,
    0,
    "Fast memory allocation and free."
);

// Properties
static SProperty Pool_UsedBlocks
(
    Pool_Object,
    "UsedBlocks",
    0,
    0,
    Pool::UsedBlocksGetter,
    0,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
    "total blocks in use."
);
static SProperty Pool_FreeBlocks
(
    Pool_Object,
    "FreeBlocks",
    0,
    0,
    Pool::FreeBlocksGetter,
    0,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
    "total blocks owned by Pool but not lwrrently used."
);
static SProperty Pool_UsedBytes
(
    Pool_Object,
    "UsedBytes",
    0,
    0,
    Pool::UsedBytesGetter,
    0,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
    "total bytes in use."
);
static SProperty Pool_FreeBytes
(
    Pool_Object,
    "FreeBytes",
    0,
    0,
    Pool::FreeBytesGetter,
    0,
    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
    "total bytes owned by Pool but not lwrrently used."
);

// Methods and Tests
#ifndef _WIN64
C_(Pool_Alloc);
static SMethod Pool_Alloc
(
    Pool_Object,
    "Alloc",
    C_Pool_Alloc,
    1,
    "Allocate memory."
);

C_(Pool_Free);
static STest Pool_Free
(
    Pool_Object,
    "Free",
    C_Pool_Free,
    1,
    "Free memory allocated with Pool.Alloc()."
);
#endif

C_(Pool_PrintDetail);
static STest Pool_PrintDetail
(
    Pool_Object,
    "PrintDetail",
    C_Pool_PrintDetail,
    1,
    "Print the detail about the pools of the given index."
);

C_(Pool_PrintAll);
static STest Pool_PrintAll
(
    Pool_Object,
    "PrintAll",
    C_Pool_PrintAll,
    0,
    "Print the memory blocks for all range of memory."
);

#if defined(POOL_DEBUG)
C_(Pool_PrintSeq);
static STest Pool_PrintSeq
(
    Pool_Object,
    "PrintSeq",
    C_Pool_PrintSeq,
    1,
    "Print the memory blocks newer than the given sequence number."
);
#endif

// Implementation
#ifndef _WIN64
//------------------------------------------------------------------------------
C_(Pool_Alloc)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    UINT32 Size = 0;
    string usage = "Usage: Pool.Alloc(size)";
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }
    RC rc;
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &Size)))
    {
        pJavaScript->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    void * LineAddr = Pool::Alloc(Size);
    UINT32 PhysAddr;
    if (LineAddr)
        PhysAddr = Platform::VirtualToPhysical32(LineAddr);
    else
        PhysAddr = 0;

    if (OK != (rc = pJavaScript->ToJsval(PhysAddr, pReturlwalue)))
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in Pool.Alloc()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    Printf(Tee::PriLow, "Allocated, Virtual: %p, Phys: %08X\n",
           LineAddr, PhysAddr);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
C_(Pool_Free)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    UINT32 Address = 0;
    string usage = "Usage: Pool.Free(address)";
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }
    RC rc;
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address)))
    {
        pJavaScript->Throw(pContext, rc, usage);
        return JS_FALSE;
    }
    void * VirtualAddr;
    if (Address)
        VirtualAddr = Platform::PhysicalToVirtual(Address);
    else
        VirtualAddr = 0;

    Pool::Free(VirtualAddr);
    RETURN_RC(OK);
}
#endif
//------------------------------------------------------------------------------
C_(Pool_PrintDetail)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    UINT32 Index = 0;
    string usage = "Usage: Pool.PrintDetail(Index)";
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }
    RC rc;
    if (OK !=(rc = pJavaScript->FromJsval(pArguments[0], &Index)))
    {
        pJavaScript->Throw(pContext, rc, usage);
        return JS_FALSE;
    }
    RETURN_RC( Pool::PrintPoolDetail(Index) );
}

//------------------------------------------------------------------------------
C_(Pool_PrintAll)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    // Check the arguments.
    if (NumArguments != 0)
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Usage: Pool.PrintAll()");
        return JS_FALSE;
    }
    RETURN_RC( Pool::PrintAllPool() );
}

//------------------------------------------------------------------------------
#if defined(POOL_DEBUG)
C_(Pool_PrintSeq)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    RC rc = OK;
    UINT32 FirstSeqNum;

    string usage = "Usage: PrintSeq(firstSeqNum)";
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, usage);
        return JS_FALSE;
    }
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &FirstSeqNum)))
    {
        pJavaScript->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    RETURN_RC( Pool::PrintAllSeq(FirstSeqNum) );
}
#endif

//------------------------------------------------------------------------------
JSBool Utility::Pool::UsedBlocksGetter(JSContext *, JSObject *, jsval, jsval *vp)
{
    MASSERT(vp);

    if ( !d_IsInitialized )
        Initialize();

    PoolStats poolStats = GetTotalPoolStats();
    UINT32 total = poolStats.usedBlocks + d_AlignedBlockNum;
    JavaScriptPtr()->ToJsval(total, vp);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
JSBool Utility::Pool::FreeBlocksGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    MASSERT(vp);

    if ( !d_IsInitialized )
        Initialize();

    PoolStats poolStats = GetTotalPoolStats();
    JavaScriptPtr()->ToJsval(poolStats.freeBlocks, vp);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
JSBool Utility::Pool::UsedBytesGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    MASSERT(vp);

    if ( !d_IsInitialized )
        Initialize();

    // Can't add in bytes for Big and Aligned blocks, we don't store that.
    PoolStats poolStats = GetTotalPoolStats();
    JavaScriptPtr()->ToJsval(poolStats.usedBytes, vp);

    return JS_TRUE;
}

//------------------------------------------------------------------------------
JSBool Utility::Pool::FreeBytesGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp)
{
    MASSERT(vp);

    if ( !d_IsInitialized )
        Initialize();

    PoolStats poolStats = GetTotalPoolStats();
    JavaScriptPtr()->ToJsval(poolStats.freeBytes, vp);

    return JS_TRUE;
}
