/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DRIVERS_ODP_H
#define DRIVERS_ODP_H

#include <lwmisc.h>
#include <lwtypes.h>
#include <lwrtos.h>

/*!
 * @brief   Enum distinguishing betwen different cases of ODP miss handling.
 *
 *  ODP_STATS_ENTRY_ID_CODE_MISS
 *      Identifies code miss requiring fetching (DMA-in) of the new code page.
 *
 *  ODP_STATS_ENTRY_ID_DATA_MISS_CLEAN
 *      Identifies data miss requiring fetching (DMA-in) of the new data page.
 *
 *  ODP_STATS_ENTRY_ID_DATA_MISS_DIRTY
 *      Identifies data miss requiring both write-back (DMA-out) of a dirty page
 *      selected for the replacement and fetching (DMA-in) of the new data page.
 *
 *  ODP_STATS_ENTRY_ID_MPU_MISS
 *      Identifies miss caused by the absence of the MPU mapping (no DMA required).
 *
 *  ODP_STATS_ENTRY_ID_DMA_SUSPENDED
 *      Identifies miss requiring DMA that cannot be served due to DMaA suspension.
 *
 *  ODP_STATS_ENTRY_ID_COMBINED
 *      Identifies any ODP miss (an aggregate counter, sum of all above misses).
 *
 *  ODP_STATS_ENTRY_ID__COUNT
 *      Specifies total number of supported counters.
 *
 *  ODP_STATS_ENTRY_ID__CAPTURE_TIME
 *      Specifies number of miss types for which we track time statistics.
 *      We do not track time for all miss types due to memory concerns.
 */
typedef enum
{
    ODP_STATS_ENTRY_ID_CODE_MISS = 0,
    ODP_STATS_ENTRY_ID_DATA_MISS_CLEAN,
    ODP_STATS_ENTRY_ID_DATA_MISS_DIRTY,
    ODP_STATS_ENTRY_ID_MPU_MISS,
    ODP_STATS_ENTRY_ID_DMA_SUSPENDED,
    ODP_STATS_ENTRY_ID_COMBINED,
    ODP_STATS_ENTRY_ID__COUNT,
    ODP_STATS_ENTRY_ID__CAPTURE_TIME = ODP_STATS_ENTRY_ID_MPU_MISS,
} ODP_STATS_ENTRY_ID;

/*!
 * @brief   Structure encapsulating all tracked ODP statistics.
 *
 * @note    All times are recorded in GPU architecture dependent HW units and
 *          require scaling before use. This was chosen to keep ODP profiling
 *          code minimal and burden is pushed to an API exposing these data.
 */
typedef struct
{
    //! Counting the ODP event type oclwrences.
    LwU64   missCnt[ODP_STATS_ENTRY_ID__COUNT];

    //! Total time spent servicing given ODP event type (for average callwlation).
    LwU64   timeSum[ODP_STATS_ENTRY_ID__CAPTURE_TIME];

    //! Min time spent servicing given ODP event type.
    LwU32   timeMin[ODP_STATS_ENTRY_ID__CAPTURE_TIME];

    //! Max time spent servicing given ODP event type.
    LwU32   timeMax[ODP_STATS_ENTRY_ID__CAPTURE_TIME];
} ODP_STATS;

/*!
 * @brief   Allowing an external access to ODP stats.
 */
extern ODP_STATS riscvOdpStats;

/*!
 * @brief Initialize ODP subsystem.
 *
 * @param[in] heapSectionIdx     Index of section where the VA->section
 *                               lookup table should get allocated.
 * @param[in] baseAddr           Base address of the ucode in FB/SYSMEM
 * \return True if init succeeded.
 */
sysKERNEL_CODE LwBool odpInit(LwU8 heapSectionIdx, LwU64 baseAddr);

//! @brief Flushes all pages to their backing storage.
void odpFlushAll(void);

//! @brief Flushes all pages to their backing storage, then ilwalidates them.
void odpFlushIlwalidateAll(void);

/*!
 * @brief Flush a range of memory to backing storage.
 *
 * If ptr or length are not aligned to the page size:
 * - Bytes before ptr in the same page will flush.
 * - Bytes after ptr+length in the same page as that address will flush.
 *
 * @param[in] ptr     Memory to flush to backing storage.
 * @param[in] length  Length in bytes of memory to flush.
 */
void odpFlushRange(void *ptr, LwLength length);

/*!
 * @brief Flush and ilwalidate a range of memory.
 *
 * If ptr or length are not aligned to the page size:
 * - Bytes before ptr in the same page will be flushed and ilwalidated.
 * - Bytes after ptr+length in the same page as that address will be flushed and ilwalidated.
 *
 * @param[in] ptr     Memory to flush and ilwalidate.
 * @param[in] length  Length in bytes of memory to flush and ilwalidate.
 */
void odpFlushIlwalidateRange(void *ptr, LwLength length);

/*!
 * @brief Pin a region of memory into ODP.
 *
 * This MUST be called in a critical section context. This does not necessarily
 * pin the MPU mappings for the memory.
 *
 * @param   pBase       Pointer to virtual memory to load and pin.
 * @param   length      Length of memory in bytes to load and pin.
 *
 * @return FLCN_OK on success, error code otherwise.
 */
FLCN_STATUS odpPin(const void *pBase, LwLength length);

/*!
 * @brief Unpin a region of memory from ODP.
 *
 * @param   pBase       Pointer to virtual memory to unpin.
 * @param   length      Length of memory in bytes to unpin.
 *
 * @return FLCN_OK on success, error code otherwise.
 */
FLCN_STATUS odpUnpin(const void *pBase, LwLength length);

/*!
 * @brief Pin a section into ODP.
 *
 * This MUST be called in an atomic context. This does not necessarily
 * pin the MPU mappings for the memory.
 * Note that child sections share their parent's index, so this API
 * can't be used on a child section.
 *
 * @param   index       The index of the section to load and pin.
 *
 * @return FLCN_OK on success, error code otherwise.
 */
FLCN_STATUS odpPinSection(LwU64 index);

/*!
 * @brief Unpin a section from ODP.
 *
 * @param   index       The index of the section to unpin.
 *
 * @return FLCN_OK on success, error code otherwise.
 */
FLCN_STATUS odpUnpinSection(LwU64 index);

/*!
 * @brief Handle Load/Store/Fetch failure.
 *
 * @param[in] virtAddr  Virtual address that failed to access.
 * @return LW_TRUE if the failure was handled. LW_FALSE means continue to next handler.
 */
LwBool odpIsrHandler(LwUPtr virtAddr);

FLCN_STATUS odpLoadPage(const void *pInPage);

/*!
 * @brief Colwert a virtual address to a physical address.
 *
 * This MUST be called in a critical section context, to prevent ODP evictions from impacting the
 * validity of the results. Be very careful!
 *
 * This should generally only be called from mpuVirtToPhys.
 *
 * @param[in] pVirt             Pointer to memory to get the physical address of.
 * @param[in] pPhys             Pointer to variable to be filled with the physical address.
 * @param[in] pBytesRemaining   Pointer to variable to be filled with the remaining number of bytes
 *                              in the ODP page.
 * @return True if the colwersion succeeded. Success is guaranteed if the virtual address is ODP
 *         managed, and failure is guaranteed if not.
 */
LwBool odpVirtToPhys(const void *pVirt, LwU64 *pPhys, LwU64 *pBytesRemaining);

#endif // DRIVERS_ODP_H
