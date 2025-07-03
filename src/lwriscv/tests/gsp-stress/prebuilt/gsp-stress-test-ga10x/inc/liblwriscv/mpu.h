/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_MPU_H
#define LIBLWRISCV_MPU_H
#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/bitmap.h>
#include <lwriscv/status.h>
#include <lwriscv/csr.h>
#include <lwmisc.h>

//
// maximum number of entries provided by hardware.
// The actual number of entries available in S mode may be smaller
//
#define LW_RISCV_CSR_MPU_ENTRY_COUNT        (1 << DRF_SIZE(LW_RISCV_CSR_XMPUIDX_INDEX))
#define LW_RISCV_CSR_MPU_ENTRY_MAX          (LW_RISCV_CSR_MPU_ENTRY_COUNT - 1)

// Generic mask for addresses - VA, PA, RANGE
#define LW_RISCV_CSR_MPU_PAGE_SIZE          (1 << DRF_SHIFT64(LW_RISCV_CSR_XMPUPA_BASE))
#define LW_RISCV_CSR_MPU_PAGE_MASK          ((uint64_t)LW_RISCV_CSR_MPU_PAGE_SIZE - 1)
#define LW_RISCV_CSR_MPU_ADDRESS_MASK       DRF_SHIFTMASK64(LW_RISCV_CSR_XMPUPA_BASE)

typedef uint32_t MpuHandle;

/*!
 * \brief Context used by the MPU driver. Client should treat this as an opaque object
 * and pass it into MPU driver APIs without directly touching its contents.
 */
typedef struct _MpuContext {
    uint8_t mpuEntryCount;
    LWRISCV_BITMAP_ELEMENT mpuReservedBitmap[LW_RISCV_CSR_MPU_ENTRY_COUNT / LWRISCV_BITMAP_ELEMENT_SIZE];
} MpuContext;

/*!
 * \brief Initialize the MPU driver
 *
 * @param pCtx A context struct which has been allocated by the caller.
 *      mpuInit will populate it and the caller will pass it into subsequent MPU driver calls.
 * @return LWRV_OK on success
 *         LWRV_ERR_... on failure
 *
 * @note This function will be called before any other MPU driver APIs to initialize the
 * MPU driver. Some MPU mappings may have been created before the driver initializes (eg:
 * Bootloader), and this function will synchronize the driver's software state with the
 * hardware state.
 *
 * This function should only be called ONCE for each partition, and there should be only
 * a single context for each partition. The caller should not allocate/use multiple contexts.
 */
LWRV_STATUS mpuInit(MpuContext *pCtx);

/*!
 * \brief Enable the MPU
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_... on failure
 *
 * @note This function will be called in use cases where the MPU hasn't already been
 * initialized by the bootloader. The client must set up some MPU mappings for code/data
 * prior to calling this. At the very least, the client should set up an identity mapping
 * for the code region calling mpuEnable, to allow code to continue exelwting after the
 * enablement.
 */
LWRV_STATUS mpuEnable(void);

/*!
 * \brief Reserve an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param searchOrigin is the lowest MPU index that can be reserved
 * @param pReservedHandle is a pointer to an MpuHandle where the reserved MPU Entry will be stored
 * @return LWRV_OK if allcation succeeds
 *         LWRV_ERR_INSUFFICIENT_RESOURCES if there are no free entries remaining
 *
 */
LWRV_STATUS mpuReserveEntry(MpuContext *pCtx, uint32_t searchOrigin, MpuHandle *pReservedHandle);

/*!
 * \brief Free an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle is a HW MPU entry. It has been previously reserved by mpuReserveEntry
 * @return LWRV_OK if freeing succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle is not an MpuHandle which has been reserved via mpuReserveEntry
 *
 */
LWRV_STATUS mpuFreeEntry(MpuContext *pCtx, MpuHandle handle);

/*!
 * \brief Program a mapping into an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to write. It has been previously reserved by mpuReserveEntry
 * @param va is the virtual address of the mapping ORd with the enable bit (LW_RISCV_CSR_XMPUVA_VLD)
 * @param pa is the physical address of the mapping
 * @param rng is the range (length) of the mapping in bytes
 * @param attr are the attribute flags of the mapping
 * @return LWRV_OK if writing succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 *         LWRV_ERR_OUT_OF_RANGE if handle is not a valid HW MPU handle
 *
 * @note pa and va and rng contain a LW_RISCV_CSR_MPU_PAGE_SIZE-aligned value in bits 10-63.
 *       The lower bits contain WPRIs and Valid bits. See the register manual for details.
 */
LWRV_STATUS mpuWriteEntry(MpuContext *pCtx, MpuHandle handle, uint64_t va, uint64_t pa, uint64_t rng, uint64_t attr);

/*!
 * \brief Read a mapping from an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to read. It has been previously reserved by mpuReserveEntry.
 * @param va is a pointer to a variable where the va value will be written. This value is ORd with the enable bit (LW_RISCV_CSR_XMPUVA_VLD)
 * @param pa is a pointer to a variable where the pa value will be written
 * @param rng is a pointer to a variable where the rng value will be written
 * @param attr is a pointer to a variable where the attr value will be written
 * @return LWRV_OK if reading succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 *         LWRV_ERR_OUT_OF_RANGE if handle is not a valid HW MPU handle
 *
 * @note Any of the pointer args may be set to NULL if the caller does not care about reading that value
 */
LWRV_STATUS mpuReadEntry(MpuContext *pCtx, MpuHandle handle, uint64_t *pVa, uint64_t *pPa, uint64_t *pRng, uint64_t *pAttr);

/*!
 * \brief Enable an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to enable. It has been previously reserved by mpuReserveEntry
 *         and written by mpuWriteEntry
 * @return LWRV_OK if enablement succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 *         LWRV_ERR_OUT_OF_RANGE if handle is not a valid HW MPU handle
 *
 * @note The client does not need to call this if it has already called mpuWriteEntry(enable = true).
 * This API is mostly intended for re-enabling mappings which have been previously disabled via mpuDisableEntry.
 */
LWRV_STATUS mpuEnableEntry(MpuContext *pCtx, MpuHandle handle);

/*!
 * \brief Disable an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to disable. It has been previously reserved by mpuReserveEntry,
 *         and enabled with mpuWriteEntry or mpuEnableEntry
 * @return LWRV_OK if disablement succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 *         LWRV_ERR_OUT_OF_RANGE if handle is not a valid HW MPU handle
 */
LWRV_STATUS mpuDisableEntry(MpuContext *pCtx, MpuHandle handle);

/*!
 * \brief Check whether the region mapped by an MPU entry has been accessed (read or exelwted)
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to check
 * @param bAccessed a pointer where the function will store whether the entry was accessed:
 *         true if this region was read or exelwted since the last call to mpuClearAccessedBit
 *         false if this region was not read or exelwted since the last call to mpuClearAccessedBit
 * @return LWRV_OK if access check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 */
LWRV_STATUS mpuIsAccessed(MpuContext *pCtx, MpuHandle handle, bool *bAccessed);

/*!
 * \brief Check whether the region mapped by an MPU entry has been written
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to check
 * @param bDirty a pointer where the function will store whether the entry is dirty:
 *         true if this region was written since the last call to mpuClearDirtyBit
 *         false if this region was written since the last call to mpuClearDirtyBit
 * @return LWRV_OK if dirty check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if handle has not been reserved via mpuReserveEntry
 */
LWRV_STATUS mpuIsDirty(MpuContext *pCtx, MpuHandle handle, bool *bDirty);

/*!
 * \brief Clear the 'accessed' status of an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to clear
 */
LWRV_STATUS mpuClearAccessedBit(MpuContext *pCtx, MpuHandle handle);

/*!
 * \brief Clear the 'dirty' status of an MPU entry
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param handle indicates which entry to clear
 */
LWRV_STATUS mpuClearDirtyBit(MpuContext *pCtx, MpuHandle handle);

/*!
 * \brief Translate a VA to a PA
 *
 * @param pCtx A context struct which has been allocated by the caller and initialized by mpuInit.
 * @param va is the virtual address to translate
 * @param pa is a pointer where the corresponding physical address will be stored
 * @param justEnabled if this is True, only the enabled MPU entries are searched,
 *      otherwise, all reserved entries are searched
 *
 * @note The translation is done by reading backte register values which have been
 * programmed into the hardware. This may be an expensive operation.
 */
LWRV_STATUS mpuVaToPa(MpuContext *pCtx, uint64_t va, bool bOnlyEnabled, uint64_t *pPa);

#endif // LIBLWRISCV_MPU_H
