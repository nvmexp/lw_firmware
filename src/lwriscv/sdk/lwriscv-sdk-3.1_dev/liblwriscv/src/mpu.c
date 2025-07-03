/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwmisc.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/mpu.h>
#include <lwriscv/csr.h>

//
// each value of XMPUIDX allows us to access this many MPU entries via
// LW_RISCV_CSR_SMPUACC and LW_RISCV_CSR_SMPUDTY
//
#define MPUIDX2_ENTRIES_PER_INDEX DRF_SIZE(LW_RISCV_CSR_SMPUACC_ACC)

/**
 * @brief Select a HW MPU Entry. subsequent accesses to SMPUVA, SMPUPA,
 * SMPURNG, and SMPUATTR will access the selected entry.
 *
 * @param[in]  index index of the HW MPU entry to select.
 *
 * @return None
 */
static void mpu_idx_select(uint64_t index)
{
    csr_write(LW_RISCV_CSR_SMPUIDX,
              DRF_NUM64(_RISCV, _CSR_SMPUIDX, _INDEX, index));
}

/**
 * @brief Select a HW MPU Entry block. subsequent accesses to SMPUACC
 * and SMPUDTY will access the selected entry.
 *
 * @param[in]  index start index of the HW MPU entry block to select.
 *
 * @return None
 */
static void mpu_idx2_select(uint8_t index)
{
    uint8_t reg = index / MPUIDX2_ENTRIES_PER_INDEX;

    csr_write(LW_RISCV_CSR_SMPUIDX2, DRF_NUM64(_RISCV, _CSR_SMPUIDX2, _IDX, reg));
}

LWRV_STATUS
mpu_init(mpu_context_t *p_ctx)
{
    LWRV_STATUS status = LWRV_OK;
    uint64_t smpuctl;
    uint8_t idx;

    if (p_ctx == NULL)
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    (void) memset(p_ctx, 0, sizeof(mpu_context_t));

    //
    // Read the entry count. The number of entries could be
    // limited by M-mode code if some entries are used by another partition
    //
    smpuctl = csr_read(LW_RISCV_CSR_SMPUCTL);
    p_ctx->mpu_entry_count = (uint8_t) DRF_VAL64(_RISCV_CSR, _SMPUCTL, _ENTRY_COUNT, smpuctl);

    // Build entries for existing MPU settings
    for (idx = 0; idx < p_ctx->mpu_entry_count; idx++)
    {
        // Check if the MPU entry has already been set
        mpu_idx_select(idx);
        uint64_t virt_addr = csr_read(LW_RISCV_CSR_SMPUVA);
        if (FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1U, virt_addr))
        {
            bitmap_set_bit(p_ctx->mpu_reserved_bitmap, idx);
        }
    }

out:
    return status;
}

LWRV_STATUS
mpu_enable(void)
{
    csr_write(LW_RISCV_CSR_SATP,
        DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));
    return LWRV_OK;
}

LWRV_STATUS
mpu_reserve_entry(mpu_context_t *p_ctx, uint32_t search_origin, mpu_handle_t *p_reserved_handle)
{
    LWRV_STATUS status = LWRV_ERR_INSUFFICIENT_RESOURCES;
    uint32_t idx;

    if ((p_ctx == NULL) || (p_reserved_handle == NULL))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    for (idx = search_origin; idx < p_ctx->mpu_entry_count; idx++)
    {
        if (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, idx))
        {
            bitmap_set_bit(p_ctx->mpu_reserved_bitmap, idx);
            *p_reserved_handle = idx;
            status = LWRV_OK;
            goto out;
        }
    }

out:
    return status;
}

LWRV_STATUS
mpu_free_entry(mpu_context_t *p_ctx, mpu_handle_t handle)
{
    LWRV_STATUS status = LWRV_OK;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    bitmap_clear_bit(p_ctx->mpu_reserved_bitmap, handle);

out:
    return status;
}

LWRV_STATUS
mpu_write_entry
(
    const mpu_context_t *p_ctx,
    mpu_handle_t handle,
    uint64_t va,
    uint64_t pa,
    uint64_t rng,
    uint64_t attr
)
{
    LWRV_STATUS status = LWRV_OK;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx_select(handle);
    csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA, pa);
    csr_write(LW_RISCV_CSR_SMPURNG, rng);
    csr_write(LW_RISCV_CSR_SMPUATTR, attr);
    csr_write(LW_RISCV_CSR_SMPUVA, va);

out:
    return status;
}

LWRV_STATUS
mpu_read_entry
(
    const mpu_context_t *p_ctx,
    mpu_handle_t handle,
    uint64_t *p_va,
    uint64_t *p_pa,
    uint64_t *p_rng,
    uint64_t *p_attr
)
{
    LWRV_STATUS status = LWRV_OK;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx_select(handle);
    if (p_va != NULL)
    {
        *p_va = csr_read(LW_RISCV_CSR_SMPUVA);
    }

    if (p_pa != NULL)
    {
        *p_pa = csr_read(LW_RISCV_CSR_SMPUPA);
    }

    if (p_rng != NULL)
    {
        *p_rng = csr_read(LW_RISCV_CSR_SMPURNG);
    }

    if (p_attr != NULL)
    {
        *p_attr = csr_read(LW_RISCV_CSR_SMPUATTR);
    }

out:
    return status;
}

LWRV_STATUS
mpu_enable_entry(const mpu_context_t *p_ctx, mpu_handle_t handle)
{
    LWRV_STATUS status = LWRV_OK;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx_select(handle);
    csr_set(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

out:
    return status;
}

LWRV_STATUS
mpu_disable_entry(const mpu_context_t *p_ctx, mpu_handle_t handle)
{
    LWRV_STATUS status = LWRV_OK;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx_select(handle);
    csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

out:
    return status;
}

LWRV_STATUS
mpu_is_accessed(const mpu_context_t *p_ctx, mpu_handle_t handle, bool *b_accessed)
{
    LWRV_STATUS status = LWRV_OK;
    uint64_t smpuacc;
    uint8_t idx_to_test;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)) ||
        (b_accessed == NULL))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx2_select((uint8_t)handle);
    smpuacc = csr_read(LW_RISCV_CSR_SMPUACC);
    idx_to_test = (uint8_t) handle % MPUIDX2_ENTRIES_PER_INDEX;
    *b_accessed = ((smpuacc & LWBIT64(idx_to_test)) != 0U);

out:
    return status;
}

LWRV_STATUS
mpu_is_dirty(const mpu_context_t *p_ctx, mpu_handle_t handle, bool *b_dirty)
{
    LWRV_STATUS status = LWRV_OK;
    uint64_t smpudty;
    uint8_t idx_to_test;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)) ||
        (b_dirty == NULL))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx2_select((uint8_t)handle);
    smpudty = csr_read(LW_RISCV_CSR_SMPUDTY);
    idx_to_test = (uint8_t) handle % MPUIDX2_ENTRIES_PER_INDEX;
    *b_dirty = ((smpudty & LWBIT64(idx_to_test)) != 0U);

out:
    return status;
}

LWRV_STATUS
mpu_clear_accessed_bit(const mpu_context_t *p_ctx, mpu_handle_t handle)
{
    LWRV_STATUS status = LWRV_OK;
    uint8_t idx_to_clear;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx2_select((uint8_t)handle);
    idx_to_clear = (uint8_t) handle % MPUIDX2_ENTRIES_PER_INDEX;
    csr_clear(LW_RISCV_CSR_SMPUACC, LWBIT64(idx_to_clear));

out:
    return status;
}

LWRV_STATUS
mpu_clear_dirty_bit(const mpu_context_t *p_ctx, mpu_handle_t handle)
{
    LWRV_STATUS status = LWRV_OK;
    uint8_t idx_to_clear;

    if ((p_ctx == NULL) ||
        (handle >= p_ctx->mpu_entry_count) ||
        (!bitmap_test_bit(p_ctx->mpu_reserved_bitmap, handle)))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    mpu_idx2_select((uint8_t)handle);
    idx_to_clear = (uint8_t) handle % MPUIDX2_ENTRIES_PER_INDEX;
    csr_clear(LW_RISCV_CSR_SMPUDTY, LWBIT64(idx_to_clear));

out:
    return status;
}

LWRV_STATUS
mpu_va_to_pa(const mpu_context_t *p_ctx, uint64_t va, bool b_only_enabled, uint64_t *p_pa)
{
    LWRV_STATUS status = LWRV_ERR_ILWALID_ARGUMENT;
    uint8_t idx;

    if ((p_ctx == NULL) || (p_pa == NULL))
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    for (idx = 0; idx < p_ctx->mpu_entry_count; idx++)
    {
        if (bitmap_test_bit(p_ctx->mpu_reserved_bitmap, idx))
        {
            mpu_idx_select(idx);
            uint64_t va_reg = csr_read(LW_RISCV_CSR_SMPUVA);
            uint64_t va_base = va_reg & ~LW_RISCV_CSR_MPU_PAGE_MASK;
            uint64_t rng;

            //If entry is disabled or va is below range, don't check this entry
            if ((b_only_enabled && FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 0U, va_reg)) ||
                (va_base > va))
            {
                continue;
            }

            rng = csr_read(LW_RISCV_CSR_SMPURNG) & ~LW_RISCV_CSR_MPU_PAGE_MASK;

            // check for overflow in va_base + rng (CERT-C). Shoudln't happen in a sanely-programmed entry.
            if ((UINT64_MAX - va_base) < rng)
            {
                continue;
            }

            if (va < (va_base + rng))
            {
                //Do a manual translation
                uint64_t pa_base = csr_read(LW_RISCV_CSR_SMPUPA) & ~LW_RISCV_CSR_MPU_PAGE_MASK;

                // check for overflow in (va - va_base) + pa_base; (CERT-C). Shoudln't happen in a sanely-programmed entry.
                if ((UINT64_MAX - pa_base) < (va - va_base))
                {
                    continue;
                }

                *p_pa = (va - va_base) + pa_base;
                status = LWRV_OK;
                break;
            }
        }
    }

out:
    return status;
}
