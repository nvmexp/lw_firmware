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

/* Compiler headers */
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
//

/**
 * @brief This is the number of HW MPU entries.
 */
#define LW_RISCV_CSR_MPU_ENTRY_COUNT        (1U << DRF_SIZE(LW_RISCV_CSR_SMPUIDX_INDEX))

/**
 * @brief This is the index of the last HW MPU entry.
 */
#define LW_RISCV_CSR_MPU_ENTRY_MAX          (LW_RISCV_CSR_MPU_ENTRY_COUNT - 1U)

/**
 * @brief This is the page size which all to which all MPU Mappings' virtual address,
 * physical address and size must be aligned.
 */
#define LW_RISCV_CSR_MPU_PAGE_SIZE          (1ULL << DRF_SHIFT64(LW_RISCV_CSR_SMPUPA_BASE))

/**
 * @brief This can be used to mask the lower bits of an address, which are an offset
 * within the MPU page.
 */
#define LW_RISCV_CSR_MPU_PAGE_MASK          ((uint64_t)LW_RISCV_CSR_MPU_PAGE_SIZE - 1ULL)

/**
 * @brief This can be used to mask the upper bits of an address, which constitute an
 * MPU page.
 */
#define LW_RISCV_CSR_MPU_ADDRESS_MASK       DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUPA_BASE)

/**
 * @brief A handle to an MPU Entry. After being reserved by the MPU driver, the client
 * can pass this handle into other MPU driver APIs to perform operations on this MPU entry.
 */
typedef uint32_t mpu_handle_t;

/**
 * @brief Context used by the MPU driver. The client should treat this as an opaque object
 * and pass it into MPU driver APIs without directly touching its contents.
 *
 * ``mpu_entry_count``: The number of MPU entries owned by this S-mode partition. This is an
 * internal field which should not be used by the client.
 *
 * ``mpu_reserved_bitmap``: Used by the driver to track the MPU Entry allocations. This is an
 * internal field which should not be used by the client.
 */
typedef struct {
    uint8_t mpu_entry_count;
    lwriscv_bitmap_element_t mpu_reserved_bitmap[LW_RISCV_CSR_MPU_ENTRY_COUNT / LWRISCV_BITMAP_ELEMENT_SIZE];
} mpu_context_t;

/**
 * @brief Initializes the MPU driver. This function should be called before any other MPU
 * driver APIs to initialize the MPU driver. Some MPU mappings may have been created before
 * the driver initializes (eg: Bootloader), and this function will synchronize the driver's
 * software state with the hardware state.
 *
 * This function should only be called once for each partition, and there should be only
 * a single context for each partition. The caller should not allocate/use multiple contexts.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller.
 *      mpu_init will populate it and the caller will pass it into subsequent MPU driver calls.
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if p_ctx is NULL
 */
LWRV_STATUS mpu_init(mpu_context_t *p_ctx);

/**
 * @brief Enables the MPU. This function should be called in use cases where the MPU hasn't
 * already been enabled by the bootloader.
 *
 * @pre The client must call mpu_init and set up some MPU mappings for
 * code/data prior to calling this. At the very least, the client should set up an identity
 * mapping for the code region calling mpu_enable, to allow code to continue exelwting after
 * the enablement.
 *
 * @return LWRV_OK on success
 */
LWRV_STATUS mpu_enable(void);

/**
 * @brief Reserve an MPU entry.
 *
 * @pre The client must call mpu_init before calling this with the same mpu_context_t.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] search_origin is the lowest MPU index that can be reserved.
 * Valid range: 0 to LW_RISCV_CSR_MPU_ENTRY_MAX
 * @param[out] p_reserved_handle is a pointer to an mpu_handle_t where the reserved MPU Entry will be stored.
 * @return LWRV_OK if allcation succeeds
 *         LWRV_ERR_ILWALID_ARGUMENT if p_ctx or p_reserved_handle is NULL
 *         LWRV_ERR_INSUFFICIENT_RESOURCES if there are no free entries remaining
 *
 */
LWRV_STATUS mpu_reserve_entry(mpu_context_t *p_ctx, uint32_t search_origin, mpu_handle_t *p_reserved_handle);

/**
 * @brief Free an MPU entry.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle is a HW MPU entry. It has been previously reserved by mpu_reserve_entry.
 * @return LWRV_OK if freeing succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not an mpu_handle_t which has been reserved
 * via mpu_reserve_entry and not already freed with a call to mpu_free_entry.
 *
 */
LWRV_STATUS mpu_free_entry(mpu_context_t *p_ctx, mpu_handle_t handle);

/**
 * @brief Program a mapping into an MPU entry. pa and va and rng contain a
 * LW_RISCV_CSR_MPU_PAGE_SIZE-aligned value in bits 10-63. The lower bits contain WPRIs and Valid bits.
 * See the register manual for details.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to write. It has been previously reserved by mpu_reserve_entry.
 * @param[in] va is the virtual address of the mapping, written directly into LW_RISCV_CSR_SMPUVA.
 * Valid range: Any value. Bits 10-63 are the LW_RISCV_CSR_MPU_PAGE_SIZE-aligned address and the rest of
 * the bits are flags in LW_RISCV_CSR_SMPUVA. Typically a client will use the LW_RISCV_CSR_SMPUVA_VLD bit
 * to enable the entry.
 * @param[in] pa is the physical address of the mapping, written directly into LW_RISCV_CSR_SMPUPA.
 * Valid range: Any value. Bits 10-63 are the LW_RISCV_CSR_MPU_PAGE_SIZE-aligned address and the rest of
 * the bits are ignored by the hardware.
 * @param[in] rng is the range (length) of the mapping in bytes.
 * Valid range: Any value. Bits 10-63 are the LW_RISCV_CSR_MPU_PAGE_SIZE-aligned size and the rest of
 * the bits are ignored by the hardware.
 * @param[in] attr are the attribute flags of the mapping, written directly into LW_RISCV_CSR_SMPUATTR.
 * Valid range: Any value. Should be a combination of the LW_RISCV_CSR_SMPUATTR_x flags.
 * @return LWRV_OK if writing succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_write_entry(const mpu_context_t *p_ctx, mpu_handle_t handle, uint64_t va, uint64_t pa, uint64_t rng, uint64_t attr);

/**
 * @brief Read a mapping from an MPU entry. Any of the pointer args may be set to NULL if the caller does not care about reading that value.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to read. It has been previously reserved by mpu_reserve_entry.
 * @param[out] p_va is a pointer to a variable where the va value will be written.
 *      This value is ORd with the enable bit (LW_RISCV_CSR_SMPUVA_VLD)
 * @param[out] p_pa is a pointer to a variable where the pa value will be written
 * @param[out] p_rng is a pointer to a variable where the rng value will be written
 * @param[out] p_attr is a pointer to a variable where the attr value will be written
 * @return LWRV_OK if reading succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_read_entry(const mpu_context_t *p_ctx, mpu_handle_t handle, uint64_t *p_va, uint64_t *p_pa, uint64_t *p_rng, uint64_t *p_attr);

/**
 * @brief Enable an MPU entry. The client does not need to call this if it has already
 * called mpu_write_entry with the SMPUVA_VLD bit set. This API is mostly intended for
 * re-enabling mappings which have been previously disabled via mpu_disable_entry.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry and written it with mpu_write_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to enable. It has been previously reserved by mpu_reserve_entry
 *         and written by mpu_write_entry
 * @return LWRV_OK if enablement succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_enable_entry(const mpu_context_t *p_ctx, mpu_handle_t handle);

/**
 * @brief Disable an MPU entry.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry and written it with mpu_write_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to disable. It has been previously reserved by mpu_reserve_entry,
 *         and enabled with mpu_write_entry or mpu_enable_entry
 * @return LWRV_OK if disablement succeeds
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_disable_entry(const mpu_context_t *p_ctx, mpu_handle_t handle);

/**
 * @brief Check whether the region mapped by an MPU entry has been accessed (read or exelwted)
 * since the last time the Accessed bit was cleared.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to check
 * @param[out] b_accessed a pointer where the function will store whether the entry was accessed:
 *         true if this region was read or exelwted since the last call to mpu_clear_accessed_bit
 *         false if this region was not read or exelwted since the last call to mpu_clear_accessed_bit
 * @return LWRV_OK if access check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx or b_accessed is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_is_accessed(const mpu_context_t *p_ctx, mpu_handle_t handle, bool *b_accessed);

/**
 * @brief Check whether the region mapped by an MPU entry has been written since the last time the
 * Dirty bit was cleared.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to check
 * @param[out] b_dirty a pointer where the function will store whether the entry is dirty:
 *         true if this region was written since the last call to mpu_clear_dirty_bit
 *         false if this region was written since the last call to mpu_clear_dirty_bit
 * @return LWRV_OK if dirty check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx or b_dirty is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_is_dirty(const mpu_context_t *p_ctx, mpu_handle_t handle, bool *b_dirty);

/**
 * @brief Clear the 'accessed' status of an MPU entry.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to clear
 * @return LWRV_OK if dirty check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_clear_accessed_bit(const mpu_context_t *p_ctx, mpu_handle_t handle);

/**
 * @brief Clear the 'dirty' status of an MPU entry.
 *
 * @pre The client must have previously reserved this handle with mpu_reserve_entry
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] handle indicates which entry to clear
 * @return LWRV_OK if dirty check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx is NULL or handle is not a valid reserved handle
 */
LWRV_STATUS mpu_clear_dirty_bit(const mpu_context_t *p_ctx, mpu_handle_t handle);

/**
 * @brief Translate a VA to a PA in software using the lwrrently programmed MPU mappings. The translation
 * is done by reading back the register values which have been programmed into the hardware. This may be
 * an expensive operation.
 *
 * @pre The client must have previously reserved called mpu_init and created some entries.
 *
 * @param[in] p_ctx A context struct which has been allocated by the caller and initialized by mpu_init.
 * @param[in] va is the virtual address to translate
 * @param[out] p_pa is a pointer where the corresponding physical address will be stored
 * @param[in] b_only_enabled if this is True, only the enabled MPU entries are searched,
 *      otherwise, all reserved entries are searched
 * @return LWRV_OK if dirty check succeeded
 *         LWRV_ERROR_ILWALID_ARGUMENT if p_ctx or p_pa is NULL or translation failed
 */
LWRV_STATUS mpu_va_to_pa(const mpu_context_t *p_ctx, uint64_t va, bool b_only_enabled, uint64_t *p_pa);

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/mpu_legacy.h>
#endif //LWRISCV_FEATURE_LEGACY_API

#endif // CPU__RISCV_MPU_H
