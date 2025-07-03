/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef DRIVERS_MPU_H
#define DRIVERS_MPU_H

#include <lwmisc.h>
#include <lwtypes.h>
#include <lwrtos.h>

//! @brief Index to an added MPU entry.
typedef LwS32 MpuHandle;

/*!
 * @brief Tracking structure for each task's MPU entries.
 *
 * User code must not be allowed to write to this structure, or its referred pointers.
 */
typedef struct TaskMpuInfo {
  /*!
   * @brief Array of MpuHandles that are attached to the task.
   *
   * This should be allocated (dynamically or statically) at task creation.
   */
  MpuHandle *pHandles;
  //! @brief Size of the array pHandles.
  LwLength maxCount;
  //! @brief Number of assigned handles.
  LwLength count;
  //! @brief Mapping reference count.
  LwLength refCount;
} TaskMpuInfo;

//! @brief Invalid MPU handle.
#define MpuHandleIlwalid ((MpuHandle)-1)

//! @brief Tracking structure for each MPU entry.
typedef struct {
  //! @brief Value to write into xMPUPA CSR.
  LwU64 physAddr;
  //! @brief Value to write into xMPUVA CSR.
  LwU64 virtAddr;
  //! @brief Value to write into xMPURNG CSR.
  LwU64 length;
  //! @brief Value to write into xMPUATTR CSR.
  LwU64 attr;

  /*!
   * @brief Number of tasks this entry is attached to.
   *
   * If 0, this is a kernel mapping.
   */
  LwU16 taskCount;

  /*!
   * @brief Entry in hardware MPU.
   *
   * 0xFFFF if not present in MPU. This should only be possible when mpuRefCount is zero.
   */
  LwU16 mpuIndex;

  /*!
   * @brief Reference count.
   *
   * Number of software references to the entry (including internally).
   * When this hits zero, this entry will be deleted.
   */
  LwU16 refCount;

  /*!
   * @brief Hardware reference count.
   *
   * Number of times the entry has been mapped.
   *
   * When this value is 0, there are no active mappings of this entry.
   * A non-zero value indicates the entry is actively mapped, and requires this
   * many unmappings to be removed from the MPU.
   */
  LwU16 mpuRefCount;
} MpuEntry;

///////////////////////////////// KERNEL API //////////////////////////////////
/*!
 * @brief Initializes the MPU subsystem.
 *
 * @return True on success, otherwise false.
 */
LwBool mpuInit(void);

//! @brief Dump the current MPU state via printf.
void mpuDump(void);

/*!
 * @brief Reserve one entry in the physical MPU.
 *
 * @param[in] searchOrigin  Lowest MPU index that can be reserved.
 * @return MPU entry index, or negative if no entries could be allocated.
 */
int mpuReserveEntry(unsigned int searchOrigin);

/*!
 * @brief Writes an MPU entry to the physical MPU.
 *
 * This should only be used on manually managed entries that were reserved using
 * mpuReserveEntries(). This MUST be called from critical section.
 *
 * @param[in] index Which entry to modify.
 * @param[in] va    XMPUVA value to write.
 * @param[in] pa    XMPUPA value to write.
 * @param[in] rng   XMPURNG value to write.
 * @param[in] attr  XMPUATTR value to write.
 */
void mpuWriteRaw(unsigned int index, LwU64 va, LwU64 pa, LwU64 rng, LwU64 attr);

/*!
 * @brief Remove an MPU entry.
 *
 * This should only be used on manually managed entries that were reserved using
 * mpuReserveEntries(). This MUST be called from critical section.
 *
 * @param[in] index Which entry to remove.
 */
void mpuRemoveRaw(unsigned int index);

/*!
 * @brief Check if an MPU entry is dirty.
 *
 * @param[in] index of MPU entry to check.
 */
LwBool mpuIsDirty(unsigned int index);

/*!
 * @brief Reset the MPU dirty state.
 *
 * @param[in] index of MPU entry to reset dirty state for.
 */
void mpuDirtyReset(unsigned int index);

/*!
 * @brief Allocate a new (not mapped) MPU entry.
 *
 * The entry is allocated with a reference count of 1.
 *
 * If this mapping already exists, it will be de-duplicated, saving resources.
 * If a different existing entry shares the same virtAddr, MpuHandleIlwalid is returned.
 *
 * @param[in] physAddr  Base physical address of the the mapping. Supports load bases.
 * @param[in] virtAddr  Base virtual address of the mapping.
 * @param[in] length    Length in bytes of the mapping.
 * @param[in] attr      MPU attributes of the mapping.
 * @return Handle to the MPU entry, or MpuHandleIlwalid on error.
 */
MpuHandle mpuEntryNew(LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr);

/*!
 * @brief Take an MPU entry.
 *
 * Increments the reference count by 1.
 *
 * @param[in] hnd   Handle to the MPU entry to take.
 */
LwBool mpuEntryTake(MpuHandle hnd);

/*!
 * @brief Release an MPU entry.
 *
 * Decrements the reference count by 1.
 * If the resulting count is 0, the entry is freed.
 *
 * @param[in] hnd   Handle to the MPU entry to release.
 */
LwBool mpuEntryRelease(MpuHandle hnd);

/*!
 * @brief Maps the MPU entry into the physical MPU.
 *
 * This additionally takes the entry.
 *
 * This function may fail due to the hardware MPU entries all being oclwpied.
 *
 * @param[in] hnd   Handle of MPU entry to map.
 * @return True if the mapping succeeded, false otherwise.
 */
LwBool mpuEntryMap(MpuHandle hnd);

/*!
 * @brief Unmaps the MPU entry from the physical MPU.
 *
 * This additionally releases the entry.
 *
 * @param[in] hnd   Handle of MPU entry to unmap.
 * @return True if the unmapping succeeded, false if the entry was not mapped.
 */
LwBool mpuEntryUnmap(MpuHandle hnd);

/*!
 * @brief Look up an MPU entry for a given virtual address.
 *
 * This is a building block for VA-to-PA translation.
 *
 * @param[in] virtAddr        Virtual address located inside an MPU entry.
 * @param[in] bAllowUnmapped  If true, includes entries that are not lwrrently mapped.
 * @param[in] pEntry          Pointer to an MpuEntry to be filled with the found entry.
 * @return True if the lookup was successful, otherwise false.
 */
LwBool mpuEntryFind(LwUPtr virtAddr, LwBool bAllowUnmapped, MpuEntry *pEntry);

/*!
 * @brief Colwert a virtual address to a physical address.
 *
 * @param[in] pVirt             Pointer to memory to get the physical address of.
 * @param[in] pPhys             Pointer to variable to be filled with the physical address.
 * @param[in] pBytesRemaining   Pointer to variable to be filled with the remaining number of bytes in
 *                              the MPU entry.
 * @return True if the colwersion succeeded, otherwise false.
 */
LwBool mpuVirtToPhys(const void *pVirt, LwUPtr *pPhys, LwU64 *pBytesRemaining);

////////////////////////////////// TASK API ///////////////////////////////////
/*!
 * @brief Initialize a task's MPU information.
 *
 * @param[in] pTaskMpu  Task's MPU information to initialize.
 * @param[in] pHandles  Pointer to backing storage for mapped handle list.
 * @param[in] maxCount  Size of the array pHandles, in MpuHandle entries.
 */
void mpuTaskInit(TaskMpuInfo *pTaskMpu, MpuHandle *pHandles, LwLength maxCount);

/*!
 * @brief Adds an MPU entry to a task's list of mapped entries.
 *
 * This should be called only before a task is initialized, to set up initial mappings.
 *
 * @param[in] pTaskMpu  Task's MPU info to attach the entry to.
 * @param[in] hnd       Handle to the MPU entry to map.
 * @return True if the entry was added, false if the task's MPU entry list is full.
 */
LwBool mpuTaskEntryAddPreInit(TaskMpuInfo *pTaskMpu, MpuHandle hnd);

/*!
 * @brief Adds an MPU entry to a task's list of mapped entries.
 *
 * This additionally takes the entry.
 *
 * @param[in] task  Task to attach the entry to.
 * @param[in] hnd   Handle to the MPU entry to map.
 * @return True if the entry was added, false if the task's MPU entry list is full.
 */
LwBool mpuTaskEntryAdd(LwrtosTaskHandle task, MpuHandle hnd);

/*!
 * @brief Removes an MPU entry to a task's list of mapped entries.
 *
 * This additionally unmaps (if necessary) and releases the entry.
 * This also compacts the task's MPU entry handle list.
 *
 * @param[in] task  Task to remove the entry from.
 * @param[in] hnd   Handle to the MPU entry to unmap.
 * @return True if the entry was removed, false if the entry was not in the list.
 */
LwBool mpuTaskEntryRemove(LwrtosTaskHandle task, MpuHandle hnd);

/*!
 * @brief Maps all entries in a task's list.
 *
 * @param[in] task  Task whose entries will be mapped.
 * @return True if the mapping succeeded, false otherwise.
 */
LwBool mpuTaskMap(LwrtosTaskHandle task);

/*!
 * @brief Unmaps all entries in a task's list.
 *
 * @param[in] task  Task whose entries will be unmapped.
 * @return True if the unmapping succeeded, false if the task was not mapped.
 */
LwBool mpuTaskUnmap(LwrtosTaskHandle task);

/*!
 * @brief Checks whether a task has an entry in its list.
 *
 * @param[in] task  Task to check.
 * @param[in] hnd   Handle to the MPU entry to check.
 * @return True if the entry is in the task's list, false otherwise.
 */
LwBool mpuTaskEntryCheck(LwrtosTaskHandle task, MpuHandle hnd);

///////////////////////////////// SIMPLE API //////////////////////////////////
/*!
 * @brief Allocate a new MPU entry, and map it into a task.
 *
 * This acts like calling mpuEntryNew -> mpuTaskEntryAdd -> mpuEntryRelease.
 *
 * @param[in] task      Task to attach the new entry to.
 * @param[in] physAddr  Base physical address of the the mapping. Supports load bases.
 * @param[in] virtAddr  Base virtual address of the mapping.
 * @param[in] length    Length in bytes of the mapping.
 * @param[in] attr      MPU attributes of the mapping.
 * @return virtAddr as a usable pointer, or NULL on error.
 */
void *mpuTaskEntryCreate(LwrtosTaskHandle task, LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr);

/*!
 * @brief Allocate a new MPU entry, and map it into kernel.
 *
 * This acts like calling mpuEntryNew -> mpuEntryMap -> mpuEntryRelease.
 *
 * @param[in] physAddr  Base physical address of the the mapping. Supports load bases.
 * @param[in] virtAddr  Base virtual address of the mapping.
 * @param[in] length    Length in bytes of the mapping.
 * @param[in] attr      MPU attributes of the mapping.
 * @return virtAddr as a usable pointer, or NULL on error.
 */
void *mpuKernelEntryCreate(LwU64 physAddr, LwUPtr virtAddr, LwLength length, LwU64 attr);

/*!
 * @brief Notify MPU code that we are entering or exiting a low power state in which
 * we cannot access external IO (FB/SYSMEM).
 *
 * @param[in] bGated      LW_TRUE if we are entering a low power state
 *                        LW_FALSE if we are exiting a low power state
 */
void mpuNotifyFbhubGated(LwBool bGated);
#endif // DRIVERS_MPU_H
