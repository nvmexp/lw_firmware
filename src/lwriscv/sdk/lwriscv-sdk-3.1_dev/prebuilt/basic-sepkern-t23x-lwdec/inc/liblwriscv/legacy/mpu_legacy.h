/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_MPU_LEGACY_H
#define LIBLWRISCV_MPU_LEGACY_H

/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

typedef mpu_handle_t MpuHandle;

/* Don't bother redefining the struct fields since clients shouldn't be accessing them */
typedef mpu_context_t MpuContext;

#define mpuInit(p_ctx) mpu_init(p_ctx)

#define mpuEnable() mpu_enable()

#define mpuReserveEntry(p_ctx, search_origin, p_reserved_handle) \
    mpu_reserve_entry((p_ctx), (search_origin), (p_reserved_handle))

#define mpuFreeEntry(p_ctx, handle) \
    mpu_free_entry((p_ctx), (handle))

#define mpuWriteEntry(p_ctx, handle, va, pa, rng, attr) \
    mpu_write_entry((p_ctx), (handle), (va), (pa), (rng), (attr))

#define mpuReadEntry(p_ctx, handle, p_va, p_pa, p_rng, p_attr) \
    mpu_read_entry((p_ctx), (handle), (p_va), (p_pa), (p_rng), (p_attr))

#define mpuEnableEntry(p_ctx, handle) \
    mpu_enable_entry((p_ctx), (handle));

#define mpuDisableEntry(p_ctx, handle) \
    mpu_disable_entry((p_ctx), (handle));

#define mpuIsAccessed(p_ctx, handle, b_accessed) \
    mpu_is_accessed((p_ctx), (handle), (b_accessed))

#define mpuIsDirty(p_ctx, handle, b_dirty) \
    mpu_is_dirty((p_ctx), (handle), (b_dirty))

#define mpuClearAccessedBit(p_ctx, handle) \
    mpu_clear_accessed_bit((p_ctx), (handle))

#define mpuClearDirtyBit(p_ctx, handle) \
    mpu_clear_dirty_bit((p_ctx), (handle))

#define mpuVaToPa(p_ctx, va, b_only_enabled, p_pa) \
    mpu_va_to_pa((p_ctx), (va), (b_only_enabled), (p_pa))

#endif // LIBLWRISCV_MPU_LEGACY_H
