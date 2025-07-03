/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    mpu.c
 * @brief   MPU management
 */

#define SAFE_RTOS_BUILD
/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>

#include <riscv_csr.h>
#include <engine.h>
#include <lwriscv-mpu.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <SafeRTOS.h>
#include <portfeatures.h>
#include <task.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include "drivers/drivers.h"
#include "drivers/mm.h"
#include "drivers/odp.h"
#include "config/g_profile.h"

/* ------------------------ Optimization settings -------------------------- */
// Set higher optimization level here because it strongly impacts context switch performance.
#pragma GCC optimize ("O3")

/*!
 * @brief Force inlining of a function.
 *
 * lwtypes.h only defines FORCEINLINE when not in debug build.
 * For such a performance-critical module, we actually want it.
 */
#undef LW_FORCEINLINE
#define LW_FORCEINLINE __inline__ __attribute__((__always_inline__))

/*!
 * @brief Disable stack protection on a function.
 *
 * Disabling the stack protector on specific functions dramatically improves
 * performance, if the function is called often. Over-using this decreases
 * performance sometimes (???)
 */
#define GCC_ATTRIB_SSP_DISABLE __attribute__((__optimize__("O3", "no-stack-protector")))

/* ------------------------ Local defines ---------------------------------- */

//! @brief If non-zero, profiling variables will be filled.
#define MPU_PROFILING 0

//! @brief Special MPU index for an unmapped entry.
#define MPU_INDEX_UNMAPPED 0xFFFF

/*!
 * @brief Number of 'software' MPU entries.
 *
 * This is the total number of entries which can exist at any moment in time.
 */
#ifdef PROFILE_MPU_ENTRY_COUNT
#define MAX_SOFT_MPU_ENTRIES PROFILE_MPU_ENTRY_COUNT
#else
// Default to 128 entries, a reasonable amount
#define MAX_SOFT_MPU_ENTRIES 128
#endif

/*!
 * @brief The type used for each element of a bitmap
 */
#define BITMAP_ELEMENT_TYPE LwU64

/*!
 * @brief Number of bits per array element of _mpuEnableBitmap.
 *
 * 8 bits per byte, bitmap elements are LwU64 typed.
 */
#define BITMAP_ELEMENT_SIZE (sizeof(BITMAP_ELEMENT_TYPE) * 8)

// Enforce that BITMAP_ELEMENT_SIZE is a power of two (for performance guarantee)
ct_assert(ONEBITSET(BITMAP_ELEMENT_SIZE));

/*!
 * @brief Get the handle from an MpuEntry pointer.
 *
 * @param[in] ENT MpuEntry of which to get the MpuHandle.
 * @return MpuHandle corresponding to the entry.
 */
#define HANDLE_FROM_ENTRY(ENT) ((MpuHandle)((ENT) - &_mpuEntries[0]))

/*!
 * @brief Validate that an MpuHandle is valid.
 *
 * @param[in] HND Handle to test validity of.
 * @return True if the handle is valid, otherwise false.
 */
#define VALID_HANDLE(HND) (((HND) >= 0) && ((HND) < MAX_SOFT_MPU_ENTRIES))

/* ------------------------ Local variables -------------------------------- */

/*!
 * @brief Pointer to most recently mapped-in task from context switch.
 *
 * This is used for mapping the current task out on context switch.
 */
static xTCB *pLastMappedTask = NULL;

//! @brief Backing MPU entry storage
static MpuEntry _mpuEntries[MAX_SOFT_MPU_ENTRIES];

/*!
 * @brief Free handle pool storage.
 *
 * The handle pool is essentially a stack, where alloc is a pop and free is a push.
 */
static MpuHandle _mpuEntryFreePool[MAX_SOFT_MPU_ENTRIES];

//! @brief Free handle pool remaining count
static LwLength _mpuEntryFreePoolCount;

/*!
 * @brief Hardware MPU enable bitmap.
 *
 * For each bit, where bit 0 is entry 0, and index 0 is entries 0-63:
 *   bit=0: The mapping is disabled, and may be reassigned.
 *   bit=1: The mapping is enabled, and may not be reassigned.
 */
static BITMAP_ELEMENT_TYPE _mpuEnableBitmap[LW_RISCV_CSR_MPU_ENTRY_COUNT / BITMAP_ELEMENT_SIZE];

/*!
 * @brief Mapping of hardware MPU entries to MpuEntrys.
 *
 * When the value is NULL, there is no associated MPU handle.
 */
static MpuEntry *_mpuMapping[LW_RISCV_CSR_MPU_ENTRY_COUNT];

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
/*!
 * @brief a bitmap of the SW MPU entries which have been disabled by Fbhub gating.
 *
 * For each bit, where bit 0 is entry 0, and index 0 is entries 0-63:
 *   bit=0: The entry is not disabled by Fbhub gating
 *   bit=1: The entry is disabled by Fbhub gating
 */
static BITMAP_ELEMENT_TYPE _mpuFbhubSuspendBitmap[MAX_SOFT_MPU_ENTRIES / BITMAP_ELEMENT_SIZE];
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
//------------------------------------------------------------------------------

/*!
 * @brief Check if addr + size is in range of min--max
 *
 * @param[in] min   the start of the range
 * @param[in] max   the end of the range (min + total size of range)
 * @param[in] addr  address of the region to check
 * @param[in] size  size of the region to check
 *
 * @return LW_TRUE  if the region defined by addr and size is inside the range,
 *         LW_FALSE otherwise.
 */
static LwBool _inRange(LwU64 min, LwU64 max, LwU64 addr, LwU64 size);

LW_FORCEINLINE GCC_ATTRIB_SSP_DISABLE
static void _bitmapSetBit(BITMAP_ELEMENT_TYPE bm[], LwU32 index)
{
    bm[index / BITMAP_ELEMENT_SIZE] |= ((BITMAP_ELEMENT_TYPE)1 << (index % BITMAP_ELEMENT_SIZE));
}

LW_FORCEINLINE GCC_ATTRIB_SSP_DISABLE
static void _bitmapClearBit(BITMAP_ELEMENT_TYPE bm[], LwU32 index)
{
    bm[index / BITMAP_ELEMENT_SIZE] &= ~((BITMAP_ELEMENT_TYPE)1 << (index % BITMAP_ELEMENT_SIZE));
}

LW_FORCEINLINE GCC_ATTRIB_SSP_DISABLE
static LwBool _bitmapTestBit(BITMAP_ELEMENT_TYPE bm[], LwU32 index)
{
    return (((bm[index / BITMAP_ELEMENT_SIZE] >> (index % BITMAP_ELEMENT_SIZE)) & 1) == 1);
}

/*!
 * @brief Return TaskMpuInfo for a given task.
 *
 * @param[in] task Task for which to get the MPU information pointer.
 * @return The associated TaskMpuInfo pointer.
 */
LW_FORCEINLINE GCC_ATTRIB_SSP_DISABLE
static TaskMpuInfo *_mpuTaskMpuInfoGet(LwrtosTaskHandle task)
{
    return (TaskMpuInfo*)(taskGET_TCB_FROM_HANDLE(task)->pxMpuInfo);
}

/*!
 * @brief Allocate a handle.
 *
 * @return An unused handle, or MpuHandleIlwalid if there are no more free handles.
 */
static MpuHandle _mpuHandleAlloc(void)
{
    if (_mpuEntryFreePoolCount == 0)
    {
        return MpuHandleIlwalid;
    }

    _mpuEntryFreePoolCount--;
    MpuHandle hnd = _mpuEntryFreePool[_mpuEntryFreePoolCount];

    // Overwrite the just allocated handle in the pool, to try to catch corruption better.
    _mpuEntryFreePool[_mpuEntryFreePoolCount] = MpuHandleIlwalid;

    return hnd;
}

/*!
 * @brief Free a handle.
 *
 * @param[in] hnd Handle to free.
 * @return LW_FALSE if the free would be invalid. LW_TRUE otherwise.
 */
static LwBool _mpuHandleFree(MpuHandle hnd)
{
    if (_mpuEntryFreePoolCount == MAX_SOFT_MPU_ENTRIES)
    {
        return LW_FALSE;
    }
    _mpuEntryFreePool[_mpuEntryFreePoolCount++] = hnd;
    return LW_TRUE;
}

/*!
 * @brief Select an MPU index.
 *
 * Doesn't de-duplicate changes to the index... for performance.
 *
 * @param[in] idx MPU index to select.
 */
LW_FORCEINLINE
static void _mpuIndexSelect(int idx)
{
    csr_write(LW_RISCV_CSR_SMPUIDX, idx);
}

/*!
 * @brief Commit an MPU entry to hardware.
 *
 * @param[in] pEnt Entry to commit.
 */
GCC_ATTRIB_SSP_DISABLE
static void _mpuEntryCommit(MpuEntry *pEnt)
{
    LwU64 va = pEnt->virtAddr;

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
    if (fbhubGatedGet())
    {
        // Leave the _VLD flag unset. When we exit Fbhub Gating, it will be set.
        // Set the bit in _mpuEnableBitmap regardless so we don't repurpose this slot.
        _bitmapSetBit(_mpuFbhubSuspendBitmap, pEnt-_mpuEntries);
    }
    else
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
    {
        va |= DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
    }

    // Write entire entry into MPU
    mpuWriteRaw(pEnt->mpuIndex, va, pEnt->physAddr, pEnt->length, pEnt->attr);

    // Mark oclwpied
    _bitmapSetBit(_mpuEnableBitmap, pEnt->mpuIndex);
}

/*!
 * @brief Enable an already present MPU entry in hardware.
 *
 * @param[in] pEnt Entry to enable.
 */
GCC_ATTRIB_SSP_DISABLE
static void _mpuEntryRecommit(MpuEntry *pEnt)
{
#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
    if (fbhubGatedGet())
    {
        // Leave the _VLD flag unset. When we exit Fbhub Gating, it will be set.
        // Set the bit in _mpuEnableBitmap regardless so we don't repurpose this slot.
        _bitmapSetBit(_mpuFbhubSuspendBitmap, pEnt-_mpuEntries);
    }
    else
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
    {
        // Enable MPU entry
        _mpuIndexSelect(pEnt->mpuIndex);
        csr_set(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    }

    // Mark oclwpied
    _bitmapSetBit(_mpuEnableBitmap, pEnt->mpuIndex);
}

/*!
 * @brief Disable an MPU entry in hardware.
 *
 * @param[in] pEnt Entry to disable.
 */
static void _mpuEntryDisable(MpuEntry *pEnt)
{
    // Disable MPU entry
    _mpuIndexSelect(pEnt->mpuIndex);
    csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

    // Mark for repurposing
    _bitmapSetBit(_mpuEnableBitmap, pEnt->mpuIndex);
}

/*!
 * @brief Take an MPU entry.
 *
 * Increments the reference count by 1.
 *
 * @param[in] pEnt  MPU entry to take.
 * @return LW_FALSE if the entry has been taken too many times. LW_TRUE otherwise.
 */
GCC_ATTRIB_SSP_DISABLE
static LwBool _mpuEntryTake(MpuEntry *pEnt)
{
    if (pEnt->refCount == (typeof(pEnt->refCount))-1)
    {
        return LW_FALSE;
    }
    pEnt->refCount++;
    return LW_TRUE;
}

/*!
 * @brief Release an MPU entry.
 *
 * Decrements the reference count by 1.
 * If the resulting count is 0, the entry is freed.
 *
 * @param[in] pEnt  MPU entry to release.
 * @return LW_FALSE if the entry already has no references, or couldn't be freed. LW_TRUE otherwise.
 */
static LwBool _mpuEntryRelease(MpuEntry *pEnt)
{
    if (pEnt->refCount == 0)
    {
        return LW_FALSE;
    }
    pEnt->refCount--;

    if (pEnt->refCount != 0)
    {
        // Still referenced, don't free
        return LW_TRUE;
    }

    if (pEnt->mpuIndex != MPU_INDEX_UNMAPPED)
    {
        /* MPU entry will already be unmapped, due to policy of MPU mapping increasing ref count.
         * This is why we don't need to change _mpuEnableBitmap.
         *
         * We still need to remove from _mpuMapping though.
         */
        _mpuMapping[pEnt->mpuIndex] = NULL;
    }
    // Free the handle.
    return _mpuHandleFree(HANDLE_FROM_ENTRY(pEnt));
}

/*!
 * @brief Allocate a new (not mapped) MPU entry.
 *
 * The entry is allocated with a reference count of 1.
 *
 * @param[in] physAddr  Base physical address of the the mapping. Supports load bases.
 * @param[in] virtAddr  Base virtual address of the mapping.
 * @param[in] length    Length in bytes of the mapping.
 * @param[in] attr      MPU attributes of the mapping.
 * @return Handle to the MPU entry, or MpuHandleIlwalid on error.
 */
static MpuHandle _mpuEntryNewWithoutValidation(LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr)
{
    MpuHandle hnd = _mpuHandleAlloc();

    if (hnd == MpuHandleIlwalid)
    {
        return MpuHandleIlwalid;
    }

    if ((LWRISCV_MPU_FBHUB_ALLOWED == 0) &&
        (
#ifdef LW_RISCV_AMAP_SYSGPA_START
            _inRange(LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_END,
                physAddr, length) ||
#else
            LW_FALSE ||
#endif // LW_RISCV_AMAP_SYSGPA_START
#ifdef LW_RISCV_AMAP_FBGPA_START
            _inRange(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_END,
                physAddr, length) ||
#else
            LW_FALSE ||
#endif // LW_RISCV_AMAP_FBGPA_START
#ifdef LW_RISCV_AMAP_GVA_START
            _inRange(LW_RISCV_AMAP_GVA_START, LW_RISCV_AMAP_GVA_END,
                physAddr, length)
#else
            LW_FALSE
#endif // LW_RISCV_AMAP_GVA_START
        ))
    {
        //
        // Never allow creation of software-backed MPU entries for FB or SYSMEM
        // if LWRISCV_MPU_FBHUB_ALLOWED == 0 (it would be good to do the same check
        // in mpuWriteRaw, but mpuWriteRaw is optimized for speed so we have to trust
        // the API user, e.g. ODP, on using it).
        // This check has to be performed even if we're not doing "normal"
        // MPU validation.
        //
        return MpuHandleIlwalid;
    }

    MpuEntry *pEnt = &_mpuEntries[hnd];

    pEnt->physAddr = physAddr;
    pEnt->virtAddr = virtAddr;
    pEnt->length = length;
    pEnt->attr = attr;
    pEnt->taskCount = 0;
    pEnt->mpuIndex = MPU_INDEX_UNMAPPED;
    pEnt->refCount = 1;
    pEnt->mpuRefCount = 0;

    return hnd;
}

/*!
 * @brief Count trailing number of 1 bits.
 *
 * @param[in] val   Number to count trailing ones.
 * @return Number of trailing ones.
 */
LW_FORCEINLINE
static int _countTrailingOnes(LwU64 val)
{
    val = ~val;
    val = LOWESTBIT(val);
    return BIT_IDX_64(val);
}

/* Optimization note:
 *
 * Lwrrently, this searches for the first repurpose-able entry.
 * For less MPU thrashing, we would want to search for the first unmapped entry,
 * _then_ if there aren't any, find first repurpose-able entry.
 *
 * However, that also means new entries are more likely to go later in the MPU table,
 * which will cause worse performance on access.
 */
GCC_ATTRIB_SSP_DISABLE
static LwBool _mpuEntryMap(MpuEntry *pEnt)
{
    if (pEnt->mpuRefCount == (typeof(pEnt->mpuRefCount))-1)
    {
        return LW_FALSE;
    }

    if (!_mpuEntryTake(pEnt))
    {
        return LW_FALSE;
    }

    // Assign a new MPU index if we've been evicted
    if (pEnt->mpuIndex == MPU_INDEX_UNMAPPED)
    {
        int mpuIndex = 0;
        int bitmapIndex = 0;
        // Coarse search for free MPU index
        for (; mpuIndex < LW_RISCV_CSR_MPU_ENTRY_COUNT; bitmapIndex++, mpuIndex += BITMAP_ELEMENT_SIZE)
        {
            if (_mpuEnableBitmap[bitmapIndex] != ~(BITMAP_ELEMENT_TYPE)0)
            {
                break;
            }
        }
        // Out of hardware entries
        if (mpuIndex >= LW_RISCV_CSR_MPU_ENTRY_COUNT)
        {
            _mpuEntryRelease(pEnt);
            return LW_FALSE;
        }
        // Find first free bit (count trailing 1s)
        mpuIndex += _countTrailingOnes(_mpuEnableBitmap[bitmapIndex]);

        // If another handle was mapped here, evict it.
        if (_mpuMapping[mpuIndex] != NULL)
        {
            if (_mpuMapping[mpuIndex]->mpuRefCount != 0)
            {
                dbgPrintf(LEVEL_ERROR,
                          "Tried to evict a still-relevant MPU entry %d (%d)!\n",
                          mpuIndex, HANDLE_FROM_ENTRY(_mpuMapping[mpuIndex]));
                if (LWRISCV_MPU_DUMP_ENABLED == 1)
                {
                    mpuDump();
                }
                appHalt();
            }
            _mpuMapping[mpuIndex]->mpuIndex = MPU_INDEX_UNMAPPED;
        }

        // Track physical MPU usage
        pEnt->mpuIndex = mpuIndex;
        _mpuMapping[mpuIndex] = pEnt;

        // Load entry into physical MPU
        _mpuEntryCommit(pEnt);
    }
    else if (pEnt->mpuRefCount == 0)
    {
        // MPU entry needs to be re-enabled
        _mpuEntryRecommit(pEnt);
    }

    pEnt->mpuRefCount++;
    return LW_TRUE;
}

/*!
 * @brief Unmaps the MPU entry from the physical MPU.
 *
 * This additionally releases the entry.
 *
 * @param[in] hnd   Handle of MPU entry to unmap.
 * @return True if the unmapping succeeded, false if the entry was not mapped.
 */
static LwBool _mpuEntryUnmap(MpuEntry *pEnt)
{
    // Check if the entry was mapped in the first place
    if (pEnt->mpuRefCount == 0)
    {
        return LW_FALSE;
    }

    pEnt->mpuRefCount--;
    if (pEnt->mpuRefCount == 0)
    {
        // Entry no longer mapped, remove from MPU
        _mpuEntryDisable(pEnt);

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
        // If unmap gets called while in Fbhub gating, clear this flag
        _bitmapClearBit(_mpuFbhubSuspendBitmap, pEnt-_mpuEntries);
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
    }

    _mpuEntryRelease(pEnt);
    return LW_TRUE;
}

/*!
 * @brief Maps all entries in a task's list.
 *
 * @param[in] pInfo Task's MPU information.
 * @return True if the mapping succeeded, false otherwise.
 */
GCC_ATTRIB_SSP_DISABLE
static LwBool _mpuTaskMap(TaskMpuInfo *pInfo)
{
    if (pInfo == NULL)
    {
        // No MPU configuration for this task
        return LW_FALSE;
    }

    // Map the task if not previously mapped.
    if (pInfo->refCount == 0)
    {
        for (LwLength i = 0; i < pInfo->count; i++)
        {
            _mpuEntryMap(&_mpuEntries[pInfo->pHandles[i]]);
        }
    }
    else if (pInfo->refCount == (typeof(pInfo->refCount))-1)
    {
        // Too many references
        return LW_FALSE;
    }
    pInfo->refCount++;
    return LW_TRUE;
}

/*!
 * @brief Unmaps all entries in a task's list.
 *
 * @param[in] pInfo Task's MPU information.
 * @return True if the unmapping succeeded, false if the task was not mapped.
 */
static LwBool _mpuTaskUnmap(TaskMpuInfo *pInfo)
{
    if (pInfo == NULL)
    {
        // No MPU configuration for this task
        return LW_FALSE;
    }

    if (pInfo->refCount == 0)
    {
        // Wasn't mapped yet
        return LW_FALSE;
    }

    pInfo->refCount--;
    // Unmap the task if no longer mapped.
    if (pInfo->refCount == 0)
    {
        for (LwLength i = 0; i < pInfo->count; i++)
        {
            _mpuEntryUnmap(&_mpuEntries[pInfo->pHandles[i]]);
        }
    }
    return LW_TRUE;
}

//------------------------------------------------------------------------------
#if MPU_PROFILING
static volatile int s_ctxsw_which = 0;
static volatile LwU64 s_ctxsw_time[2][3] = {0};
#endif
void lwrtosHookConfigMpuForTask(void)
{
    // No need to re-map if it's the same task
    if (pLastMappedTask == pxLwrrentTCB)
    {
        return;
    }

#if MPU_PROFILING
    s_ctxsw_which ^= 1;
    s_ctxsw_time[s_ctxsw_which][0] = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
#endif
    // Unmap previously active task
    if (pLastMappedTask != NULL)
    {
        _mpuTaskUnmap(_mpuTaskMpuInfoGet(pLastMappedTask));
    }
#if MPU_PROFILING
    s_ctxsw_time[s_ctxsw_which][1] = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
#endif

    pLastMappedTask = pxLwrrentTCB;
    _mpuTaskMap(_mpuTaskMpuInfoGet(pxLwrrentTCB));
#if MPU_PROFILING
    s_ctxsw_time[s_ctxsw_which][2] = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
    dbgPrintf(LEVEL_DEBUG, "CTXSW unmap:%d map:%d\n",
           s_ctxsw_time[s_ctxsw_which][1] - s_ctxsw_time[s_ctxsw_which][0],
           s_ctxsw_time[s_ctxsw_which][2] - s_ctxsw_time[s_ctxsw_which][1]);
#endif
}

void lwrtosHookTaskMap(struct TaskMpuInfo *pMpuInfo)
{
    _mpuTaskMap(pMpuInfo);
}

void lwrtosHookTaskUnmap(struct TaskMpuInfo *pMpuInfo)
{
    _mpuTaskUnmap(pMpuInfo);
}

//------------------------------------------------------------------------------
LwBool mpuInit(void)
{
    // Initialize allocation pool
    for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
    {
        _mpuEntryFreePool[i] = (MpuHandle)i;
    }
    _mpuEntryFreePoolCount = MAX_SOFT_MPU_ENTRIES;

    // last entries are set by bootloader and are not needed, disable them
    for (int i = 0; i < LW_RISCV_MPU_BOOTLOADER_RSVD; ++i)
    {
        _mpuIndexSelect(LW_RISCV_CSR_MPU_ENTRY_MAX - i);
        csr_write(LW_RISCV_CSR_SMPUVA, 0);
    }

    // Initialize MPU mapping bitmap to all-oclwpied
    memset(_mpuEnableBitmap, ~0, sizeof(_mpuEnableBitmap));

    // Build entries for existing MPU settings
    for (int i = 0; i < LW_RISCV_CSR_MPU_ENTRY_COUNT; i++)
    {
        // Check if the MPU entry has already been set
        _mpuIndexSelect(i);
        LwU64 virtAddr = csr_read(LW_RISCV_CSR_SMPUVA);
        if (FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 0, virtAddr))
        {
            // Invalid entry, mark for reuse
            _mpuMapping[i] = NULL;
            _bitmapClearBit(_mpuEnableBitmap, i);
            continue;
        }

        virtAddr &= ~LW_RISCV_CSR_MPU_PAGE_MASK;
        LwU64 physAddr = csr_read(LW_RISCV_CSR_SMPUPA) & ~LW_RISCV_CSR_MPU_PAGE_MASK;
        LwU64 length = csr_read(LW_RISCV_CSR_SMPURNG) & ~LW_RISCV_CSR_MPU_PAGE_MASK;
        LwU64 attr = csr_read(LW_RISCV_CSR_SMPUATTR);

        MpuHandle hnd = _mpuEntryNewWithoutValidation(physAddr, virtAddr, length, attr);
        if (hnd == MpuHandleIlwalid)
        {
            return LW_FALSE;
        }

        MpuEntry *pEnt = &_mpuEntries[hnd];

        // Track physical MPU usage
        pEnt->mpuRefCount = 1;
        pEnt->mpuIndex = i;
        _mpuMapping[i] = pEnt;
    }
    return LW_TRUE;
}

int mpuReserveEntry(unsigned int searchOrigin)
{
    for (unsigned int i = searchOrigin; i < LW_RISCV_CSR_MPU_ENTRY_COUNT; i++)
    {
        if (_bitmapTestBit(_mpuEnableBitmap, i) == LW_FALSE)
        {
            _bitmapSetBit(_mpuEnableBitmap, i);
            return i;
        }
    }
    return -1;
}

void mpuWriteRaw(unsigned int index, LwU64 va, LwU64 pa, LwU64 rng, LwU64 attr)
{
    _mpuIndexSelect(index);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
    csr_write(LW_RISCV_CSR_SMPUPA, pa);
    csr_write(LW_RISCV_CSR_SMPURNG, rng);
    csr_write(LW_RISCV_CSR_SMPUATTR, attr);
    csr_write(LW_RISCV_CSR_SMPUVA, va);
}

void mpuRemoveRaw(unsigned int index)
{
    _mpuIndexSelect(index);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
}

LwBool mpuIsDirty(unsigned int index)
{
    _mpuIndexSelect(index);
    return !FLD_TEST_DRF(_RISCV_CSR, _SMPUVA, _D, _RST, csr_read(LW_RISCV_CSR_SMPUVA));
}

void mpuDirtyReset(unsigned int index)
{
    _mpuIndexSelect(index);
    csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM(_RISCV_CSR, _SMPUVA, _D, 1));
}

MpuHandle mpuEntryNew(LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr)
{
    // Sanitize inputs
    if (physAddr & LW_RISCV_CSR_MPU_PAGE_MASK)
    {
        return MpuHandleIlwalid;
    }
    if (virtAddr & LW_RISCV_CSR_MPU_PAGE_MASK)
    {
        return MpuHandleIlwalid;
    }
    if (length & LW_RISCV_CSR_MPU_PAGE_MASK)
    {
        return MpuHandleIlwalid;
    }

    LwU64 entVaFirst = virtAddr;
    LwU64 entVaLast = entVaFirst + length;

    lwrtosENTER_CRITICAL();

    // De-duplicate the entry, and check for overlaps
    for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
    {
        MpuEntry *pCheck = &_mpuEntries[i];
        if (pCheck->refCount == 0)
        {
            // Entry is unused
            continue;
        }

        if ((pCheck->virtAddr == virtAddr) &&
            (pCheck->physAddr == physAddr) &&
            (pCheck->length == length) &&
            (pCheck->attr == attr))
        {
            // Duplicate entry, use the existing handle.
            _mpuEntryTake(pCheck);
            lwrtosEXIT_CRITICAL();
            return i;
        }
        else
        {
            // Check if the new mapping would overlap this entry in VA
            LwU64 checkVaFirst = pCheck->virtAddr;
            LwU64 checkVaLast = checkVaFirst + pCheck->length;
            if ((checkVaFirst <   entVaLast) &&
                (  entVaFirst < checkVaLast))
            {
                // Overlapping VA, not permitted
                lwrtosEXIT_CRITICAL();
                return MpuHandleIlwalid;
            }
        }
    }

    // Create a new entry if the above passed
    MpuHandle hnd = _mpuEntryNewWithoutValidation(physAddr, virtAddr, length, attr);
    lwrtosEXIT_CRITICAL();
    return hnd;
}

LwBool mpuEntryTake(MpuHandle hnd)
{
    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    LwBool ret = _mpuEntryTake(&_mpuEntries[hnd]);
    lwrtosEXIT_CRITICAL();
    return ret;
}

LwBool mpuEntryRelease(MpuHandle hnd)
{
    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    LwBool ret = _mpuEntryRelease(&_mpuEntries[hnd]);
    lwrtosEXIT_CRITICAL();
    return ret;
}

LwBool mpuEntryMap(MpuHandle hnd)
{
    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    LwBool status = _mpuEntryMap(&_mpuEntries[hnd]);
    lwrtosEXIT_CRITICAL();
    return status;
}

LwBool mpuEntryUnmap(MpuHandle hnd)
{
    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    LwBool status = _mpuEntryUnmap(&_mpuEntries[hnd]);
    lwrtosEXIT_CRITICAL();
    return status;
}

LwBool mpuEntryFind(LwUPtr virtAddr, LwBool bAllowUnmapped, MpuEntry *pEntry)
{
    LwBool found = LW_FALSE;
    lwrtosENTER_CRITICAL();
    for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
    {
        MpuEntry *pEnt = &_mpuEntries[i];
        if (pEnt->refCount == 0)
        {
            // Entry is unused
            continue;
        }

        if (!bAllowUnmapped && (pEnt->mpuRefCount == 0))
        {
            continue;
        }

        // Check if virtAddr is within [pEnt->virtAddr, pEnt->virtAddr + pEnt->length)
        LwU64 entFirst = pEnt->virtAddr;
        LwU64 entLast = entFirst + pEnt->length;
        if ((virtAddr >= entFirst) && (virtAddr < entLast))
        {
            // Found the entry, copy it and return.
            *pEntry = *pEnt;
            found = LW_TRUE;
            break;
        }
    }
    lwrtosEXIT_CRITICAL();
    return found;
}

/*!
 * @see mpuVirtToPhys
 *
 * A syscall handler entry-point for mpuVirtToPhys. It allows
 * mpuVirtToPhys to assume that none of the arguments it receives
 * in this call-chain are in paged memory.
 *
 * This is important to ensure that mpuVirtToPhys doesn't generate
 * any page-faults, since it's not allowed to do nested traps
 * when called as an optimized syscall.
 *
 * This function should only be called from asm, and thus it has no
 * forward declarations in header files.
 *
 * Note that in this function, and everything called by it,
 * there's no access to the context in gp, since optimized syscalls
 * don't have a dedicated context!
 *
 * This function's API is modified from mpuVirtToPhys.
 * Instead of returning a status code, it returns the phys addr,
 * and NULL if it fails. It also does not take pBytesRemaining.
 *
 * MMINTS-TODO: unify the API of these functions in the future (at least partially)!
 *
 * @param[in] pVirt  Pointer to memory to get the physical address of.
 * @return           the physical address if the colwersion succeeded, otherwise NULL.
 */
LwUPtr mpuVirtToPhysSyscallHandler_NoContext(const void *pVirt)
{
    //
    // Syscalls shouldn't cause page faults, so this wrapper
    // allocates args to mpuVirtToPhys on the stack (assuming this is
    // the resident system stack).
    //
    LwUPtr physAddr;

    if (mpuVirtToPhys(pVirt, &physAddr, NULL)) // this API disallows pBytesRemaining
    {
        return physAddr;
    }
    else
    {
        return (LwUPtr)NULL;
    }
}

//
// NOTE: this function gets called from an optimized syscall,
// so you should NEVER try to access context in gp from it!
//
LwBool mpuVirtToPhys(const void *pVirt, LwUPtr *pPhys, LwU64 *pBytesRemaining)
{
    MpuEntry ent;
    LwS64 offset;

#ifdef ODP_ENABLED
    //
    // Check if this is ODP. Since ODP uses lookup tables,
    // this call is very fast, so it is better to put it in early.
    //
    if (odpVirtToPhys(pVirt, pPhys, pBytesRemaining))
    {
        return LW_TRUE;
    }
#endif // ODP_ENABLED

    if (!mpuEntryFind((LwUPtr)pVirt, LW_TRUE, &ent))
    {
        return LW_FALSE;
    }

    offset = ((LwUPtr)pVirt) - ent.virtAddr;

    if (pPhys != NULL)
    {
        *pPhys = ent.physAddr + offset;
    }

    if (pBytesRemaining != NULL)
    {
        *pBytesRemaining = ent.length - offset;
    }

    return LW_TRUE;
}

//------------------------------------------------------------------------------
void mpuTaskInit(TaskMpuInfo *pTaskMpu, MpuHandle *pHandles, LwLength maxCount)
{
    if (pTaskMpu == NULL)
    {
        return;
    }
    pTaskMpu->pHandles = pHandles;
    pTaskMpu->maxCount = maxCount;
    pTaskMpu->count = 0;
    pTaskMpu->refCount = 0;
}

LwBool mpuTaskEntryAddPreInit(TaskMpuInfo *pTaskMpu, MpuHandle hnd)
{
    if (pTaskMpu == NULL)
    {
        return LW_FALSE;
    }
    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    // All of the task's MPU entries are used.
    if (pTaskMpu->count == pTaskMpu->maxCount)
    {
        lwrtosEXIT_CRITICAL();
        return LW_FALSE;
    }

    // Add reference by task list.
    MpuEntry *pEnt = &_mpuEntries[hnd];
    if (!_mpuEntryTake(pEnt))
    {
        lwrtosEXIT_CRITICAL();
        return LW_FALSE;
    }
    pEnt->taskCount++;
    pTaskMpu->pHandles[pTaskMpu->count++] = hnd;

    // Check if this task is lwrrently mapped in.
    if (pTaskMpu->refCount != 0)
    {
        // If so, we need to map this entry.
        mpuEntryMap(hnd);
    }
    lwrtosEXIT_CRITICAL();
    return LW_TRUE;
}

LwBool mpuTaskEntryAdd(LwrtosTaskHandle task, MpuHandle hnd)
{
    return mpuTaskEntryAddPreInit(_mpuTaskMpuInfoGet(task), hnd);
}

LwBool mpuTaskEntryRemove(LwrtosTaskHandle task, MpuHandle hnd)
{
    TaskMpuInfo *pInfo = _mpuTaskMpuInfoGet(task);
    if (pInfo == NULL)
    {
        return LW_FALSE;
    }

    if (!VALID_HANDLE(hnd))
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();

    // Find the entry in the handle list (necessary for compaction).
    LwLength idx = 0;
    for (; idx < pInfo->count; idx++)
    {
        if (pInfo->pHandles[idx] == hnd)
        {
            break;
        }
    }
    if (idx == pInfo->count)
    {
        // Handle not found.
        lwrtosEXIT_CRITICAL();
        return LW_FALSE;
    }

    MpuEntry *pEnt = &_mpuEntries[hnd];
    pEnt->taskCount--;

    // Check if this task is lwrrently mapped in.
    if (pInfo->refCount != 0)
    {
        // If so, we need to unmap this entry.
        _mpuEntryUnmap(pEnt);
    }

    // Compact the handles list.
    pInfo->count--;
    for (; idx < pInfo->count; idx++)
    {
        pInfo->pHandles[idx] = pInfo->pHandles[idx + 1];
    }
    // Clean up the extra handle.
    pInfo->pHandles[idx] = MpuHandleIlwalid;

    // No longer referenced by task list
    _mpuEntryRelease(pEnt);
    lwrtosEXIT_CRITICAL();
    return LW_TRUE;
}

LwBool mpuTaskMap(LwrtosTaskHandle task)
{
    lwrtosENTER_CRITICAL();
    LwBool status = _mpuTaskMap(_mpuTaskMpuInfoGet(task));
    lwrtosEXIT_CRITICAL();
    return status;
}

LwBool mpuTaskUnmap(LwrtosTaskHandle task)
{
    lwrtosENTER_CRITICAL();
    LwBool status = _mpuTaskUnmap(_mpuTaskMpuInfoGet(task));
    lwrtosEXIT_CRITICAL();
    return status;
}

LwBool mpuTaskEntryCheck(LwrtosTaskHandle task, MpuHandle hnd)
{
    TaskMpuInfo *pInfo = _mpuTaskMpuInfoGet(task);
    if (pInfo == NULL)
    {
        return LW_FALSE;
    }

    lwrtosENTER_CRITICAL();
    for (LwLength i = 0; i < pInfo->count; i++)
    {
        if (pInfo->pHandles[i] == hnd)
        {
            lwrtosEXIT_CRITICAL();
            return LW_TRUE;
        }
    }
    lwrtosEXIT_CRITICAL();
    return LW_FALSE;
}

//------------------------------------------------------------------------------
void *mpuTaskEntryCreate(LwrtosTaskHandle task, LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr)
{
    MpuHandle mpuHnd = mpuEntryNew(physAddr, virtAddr, length, attr);
    if (mpuHnd == MpuHandleIlwalid)
    {
        return NULL;
    }

    if (!mpuTaskEntryAdd(task, mpuHnd))
    {
        // Return NULL if add failed
        virtAddr = (LwUPtr)NULL;
    }
    mpuEntryRelease(mpuHnd);
    return (void*)virtAddr;
}

void *mpuKernelEntryCreate(LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr)
{
    MpuHandle mpuHnd = mpuEntryNew(physAddr, virtAddr, length, attr);
    if (mpuHnd == MpuHandleIlwalid)
    {
        return NULL;
    }

    if (!mpuEntryMap(mpuHnd))
    {
        // Return NULL if map failed
        virtAddr = (LwUPtr)NULL;
    }
    mpuEntryRelease(mpuHnd);
    return (void*)virtAddr;
}

//------------------------------------------------------------------------------
static LwBool _inRange(LwU64 min, LwU64 max, LwU64 addr, LwU64 size)
{
    if (size == 0U)
    {
        return LW_FALSE;
    }

    if (((addr + size) <= min) || ((addr + size) > max))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

static const char *_mpuTargetString(LwU64 base, LwU64 size)
{
    static char buf[40];
    char *p = buf;

    // We use memcpy to avoid adding strcat/strcpy to kernel
    if (!size)
    {
        memcpy(buf, "Unknown", 8);
        return buf;
    }

    if (_inRange(LW_RISCV_AMAP_IMEM_START, LW_RISCV_AMAP_IMEM_START + LW_RISCV_AMAP_IMEM_SIZE, base, size))
    {
        memcpy(p, "ITCM ", 5);
        p += 5;
    }

    if (_inRange(LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_START + LW_RISCV_AMAP_DMEM_SIZE, base, size))
    {
        memcpy(p, "DTCM ", 5);
        p += 5;
    }

#ifdef EMEM_SUPPORTED
    if (_inRange(LW_RISCV_AMAP_EMEM_START, LW_RISCV_AMAP_EMEM_START + LW_RISCV_AMAP_EMEM_SIZE, base, size))
    {
        memcpy(p, "EMEM ", 5);
        p += 5;
    }
#endif

#ifdef LW_RISCV_AMAP_PRIV_START
    if (_inRange(LW_RISCV_AMAP_PRIV_START, LW_RISCV_AMAP_PRIV_START + LW_RISCV_AMAP_PRIV_SIZE, base, size))
    {
        memcpy(p, "PRIV ", 5);
        p += 5;
    }
#endif // LW_RISCV_AMAP_PRIV_START

#ifdef LW_RISCV_AMAP_FBGPA_START
    if (_inRange(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_START + LW_RISCV_AMAP_FBGPA_SIZE, base, size))
    {
        memcpy(p, "FBGPA ", 6);
        p += 6;
    }
#endif // LW_RISCV_AMAP_FBGPA_START

#ifdef LW_RISCV_AMAP_SYSGPA_START
    if (_inRange(LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_START + LW_RISCV_AMAP_SYSGPA_SIZE, base, size))
    {
        memcpy(p, "SYSGPA ", 7);
        p += 7;
    }
#endif // LW_RISCV_AMAP_SYSGPA_START

#ifdef LW_RISCV_AMAP_GVA_START
    if (_inRange(LW_RISCV_AMAP_GVA_START, LW_RISCV_AMAP_GVA_START + LW_RISCV_AMAP_GVA_SIZE, base, size))
    {
        memcpy(p, "GVA", 3);
        p += 3;
    }
#endif // LW_RISCV_AMAP_GVA_START

    *p = 0;

    return buf;
}

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
void mpuNotifyFbhubGated(LwBool bGated)
{
    lwrtosENTER_CRITICAL();

    if (bGated)
    {
        for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
        {
            MpuEntry *pEnt = &_mpuEntries[i];

            if (pEnt->mpuRefCount > 0 &&
                (_inRange(LW_RISCV_AMAP_SYSGPA_START,
                        LW_RISCV_AMAP_SYSGPA_START + LW_RISCV_AMAP_SYSGPA_SIZE,
                        pEnt->physAddr, pEnt->length) ||
                _inRange(LW_RISCV_AMAP_FBGPA_START,
                        LW_RISCV_AMAP_FBGPA_START + LW_RISCV_AMAP_FBGPA_SIZE,
                        pEnt->physAddr, pEnt->length)))
            {
                dbgPrintf(LEVEL_INFO, "mpuNotifyFbhubGated: disable mapping to PA %p\n", pEnt->physAddr);

                if (pEnt->mpuIndex == MPU_INDEX_UNMAPPED)
                {
                    dbgPrintf(LEVEL_ERROR, "mpuNotifyFbhubGated: Inconsistant state in MPU entry %d\n", i);
                    lwrtosEXIT_CRITICAL();
                    kernelVerboseCrash(0);
                    return;
                }

                // Disable MPU entry in HW
                _mpuIndexSelect(pEnt->mpuIndex);
                csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

                // Don't clear the bit in _mpuEnableBitmap[] - we don't want this
                // entry to be repurposed by some other mapping.

                _bitmapSetBit(_mpuFbhubSuspendBitmap, i);
            }
        }
    }
    else
    {
        for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
        {
            MpuEntry *pEnt = &_mpuEntries[i];
            if (_bitmapTestBit(_mpuFbhubSuspendBitmap, i))
            {
                dbgPrintf(LEVEL_INFO, "mpuNotifyFbhubGated: Re-enable mapping to PA %p\n", pEnt->physAddr);

                if (pEnt->mpuRefCount == 0 || pEnt->mpuIndex == MPU_INDEX_UNMAPPED)
                {
                    dbgPrintf(LEVEL_ERROR, "mpuNotifyFbhubGated: Inconsistant state in MPU entry %d\n", i);
                    lwrtosEXIT_CRITICAL();
                    kernelVerboseCrash(0);
                    return;
                }

                // Disable MPU entry in HW
                _mpuIndexSelect(pEnt->mpuIndex);
                csr_set(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

                _bitmapClearBit(_mpuFbhubSuspendBitmap, i);
            }
        }
    }

    lwrtosEXIT_CRITICAL();
}
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)

void mpuDump(void)
{
    LwU32 reg;

    dbgPrintf(LEVEL_ALWAYS,
              "MPU dump (enabled entries only, all numbers are hex):\n");
    dbgPrintf(LEVEL_ALWAYS,
              " # VA Start -  VA End  ->  PHYS  Size Cached?\n");

    for (reg = 0; reg < LW_RISCV_CSR_MPU_ENTRY_COUNT; ++reg)
    {
        LwU64 mmpuva, mmpupa, mmpurng, mmpuattr;

        _mpuIndexSelect(reg);
        mmpuva = csr_read(LW_RISCV_CSR_SMPUVA);
        if (!DRF_VAL64(_RISCV_CSR, _SMPUVA, _VLD, mmpuva))
        {
            continue;
        }

        mmpuva &= LW_RISCV_CSR_MPU_ADDRESS_MASK;
        mmpupa = csr_read(LW_RISCV_CSR_SMPUPA) & LW_RISCV_CSR_MPU_ADDRESS_MASK;
        mmpurng = csr_read(LW_RISCV_CSR_SMPURNG) & LW_RISCV_CSR_MPU_ADDRESS_MASK;
        mmpuattr = csr_read(LW_RISCV_CSR_SMPUATTR);

        dbgPrintf(LEVEL_ALWAYS, "%2x %8llx - %8llx -> %6llx %4llx %c u:%c%c%c k:%c%c%c tgt: %s\n",
               reg,
               mmpuva,
               mmpuva + mmpurng,
               mmpupa,
               mmpurng,
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, mmpuattr) ? 'Y' : 'N',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _UR, mmpuattr) ? 'r' : '-',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _UW, mmpuattr) ? 'w' : '-',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _UX, mmpuattr) ? 'x' : '-',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _SR, mmpuattr) ? 'r' : '-',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _SW, mmpuattr) ? 'w' : '-',
               DRF_VAL64(_RISCV_CSR, _SMPUATTR, _SX, mmpuattr) ? 'x' : '-',
               _mpuTargetString(mmpupa, mmpurng));
    }

    dbgPrintf(LEVEL_ALWAYS, "\nReservation (0~%d):", LW_RISCV_CSR_MPU_ENTRY_COUNT);
    for (int i = 0; i < LW_RISCV_CSR_MPU_ENTRY_COUNT; i++)
    {
        if ((i % 8) == 0)
        {
            dbgPrintf(LEVEL_ALWAYS, " ");
        }
        dbgPrintf(LEVEL_ALWAYS, "%u", _bitmapTestBit(_mpuEnableBitmap, i));
    }
    dbgPrintf(LEVEL_ALWAYS, "\nSoftware MPU list:\n"
              "  #  task# ref# mpuidx mpuref %sVA => PA (SIZE) ATTR\n",
#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
              "susp? "
#else
              ""
#endif
                  );
    for (int i = 0; i < MAX_SOFT_MPU_ENTRIES; i++)
    {
        MpuEntry *pEnt = &_mpuEntries[i];
        if (pEnt->refCount == 0)
        {
            continue;
        }
        dbgPrintf(LEVEL_ALWAYS, "[%3d] %2d %2d 0x%2X %2d %s%llX => %llX (%llX) 0x%llX\n",
                  i,
                  pEnt->taskCount, pEnt->refCount, pEnt->mpuIndex, pEnt->mpuRefCount,
#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
                  _bitmapTestBit(_mpuFbhubSuspendBitmap, i) ? "Y ":"N ",
#else
                  "",
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
                  pEnt->virtAddr, pEnt->physAddr, pEnt->length, pEnt->attr);
    }
}
