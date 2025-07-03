/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * Copied from https://gitlab-master.lwpu.com/dlarch-fastkernels/cask_sdk
 * with some formatting changes
 *
 * xmma/include/xmma/hopper/emu/lwda_tma_utils.h
 * xmma/include/xmma/hopper/emu/lwda_tma_utils_internal.h
 */

#pragma once
#include "tma_types.h"

static inline void set_tensor_common_0(lwdaTmaDescv2 *p_desc, UINT64 addr)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled *>(p_desc);
    desc->tensor_common0 = 0;
    desc->tensor_common0 |= (addr);
}

static inline void set_tensor_common_1
(
    lwdaTmaDescv2 *p_desc,
    lwdaTmaDescTypev2 desc_type,
    UINT32 dims,
    lwdaTmaDescFormatv2 format,
    lwdaTmaDescInterleavev2 interleave,
    lwdaTmaDescSwizzlev2 swizzle,
    UINT32 fill,
    UINT32 f32_to_tf32,
    lwdaTmaDescPromotiolw2 promotion
)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled *>(p_desc);

    desc->tensor_common1 = 0;
    desc->tensor_common1 |= desc_type == TENSOR_TILED ? 0x0 : 0x1;

    constexpr UINT32 VERSION_SHIFT = 1;
    constexpr UINT32 VERSION_BITS = 3;
    desc->tensor_common1 |= (1u << VERSION_SHIFT);

    constexpr UINT32 DIM_BITS = 3;
    constexpr UINT32 DIM_SHIFT = VERSION_SHIFT + VERSION_BITS;
    constexpr UINT32 DIM_MASK = (1u << DIM_BITS) - 1;
    desc->tensor_common1 |= ((dims - 1) & DIM_MASK) << DIM_SHIFT;

    constexpr UINT32 FORMAT_BITS = 4;
    constexpr UINT32 FORMAT_SHIFT = DIM_SHIFT + DIM_BITS;
    constexpr UINT32 FORMAT_MASK = (1u << FORMAT_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(format) & FORMAT_MASK) << FORMAT_SHIFT;

    constexpr UINT32 INTERLEAVE_BITS = 2;
    constexpr UINT32 INTERLEAVE_SHIFT = FORMAT_SHIFT + FORMAT_BITS;
    constexpr UINT32 INTERLEAVE_MASK = (1u << INTERLEAVE_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(interleave) & INTERLEAVE_MASK)
                            << INTERLEAVE_SHIFT;

    constexpr UINT32 SWIZZLE_BITS = 2;
    constexpr UINT32 SWIZZLE_SHIFT = INTERLEAVE_SHIFT + INTERLEAVE_BITS;
    constexpr UINT32 SWIZZLE_MASK = (1u << SWIZZLE_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(swizzle) & SWIZZLE_MASK) << SWIZZLE_SHIFT;

    constexpr UINT32 FILL_BITS = 1;
    constexpr UINT32 FILL_SHIFT = SWIZZLE_SHIFT + SWIZZLE_BITS;
    constexpr UINT32 FILL_MASK = (1u << FILL_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(fill) & FILL_MASK) << FILL_SHIFT;

    constexpr UINT32 F32_TO_TF32_BITS = 1;
    constexpr UINT32 F32_TO_TF32_SHIFT = FILL_SHIFT + FILL_BITS;
    constexpr UINT32 F32_TO_TF32_MASK = (1u << F32_TO_TF32_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(f32_to_tf32) & F32_TO_TF32_MASK)
                            << F32_TO_TF32_SHIFT;

    constexpr UINT32 PROMOTION_BITS = 2;
    constexpr UINT32 PROMOTION_SHIFT = F32_TO_TF32_SHIFT + F32_TO_TF32_BITS;
    constexpr UINT32 PROMOTION_MASK = (1u << PROMOTION_BITS) - 1;
    desc->tensor_common1 |= (static_cast<UINT32>(promotion) & PROMOTION_MASK)
                            << PROMOTION_SHIFT;
}

static inline void
set_tensor_stride
(
    lwdaTmaDescv2 *p_desc, UINT64 *p_tensor_stride, UINT32 dims
)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled*>(p_desc);

    constexpr UINT32 TENSOR_STRIDE_UPPER_BITS = 4;
    constexpr UINT32 TENSOR_STRIDE_UPPER_MASK = (1u << TENSOR_STRIDE_UPPER_BITS) - 1;

    for (UINT32 i = 0; i < dims - 1; i++)
    {
        desc->tensor_stride_lower[i] = 0u;
        UINT64 tensor_stride_lower_64b = (p_tensor_stride[i] >> 4) & 0xFFFFFFFFlu;
        desc->tensor_stride_lower[i] = static_cast<UINT32>(tensor_stride_lower_64b);
    }
    desc->tensor_stride_upper = 0u;

    for (UINT32 i = 0; i < dims - 1; i++)
    {
        UINT64 tensor_stride = p_tensor_stride[i];
        tensor_stride = tensor_stride >> 4;
        UINT64 tensor_stride_upper = tensor_stride >> 32;
        UINT32 tensor_stride_upper_32b = static_cast<UINT32>(tensor_stride_upper);
        desc->tensor_stride_upper |= ((tensor_stride_upper_32b & TENSOR_STRIDE_UPPER_MASK)
                                       << (i * TENSOR_STRIDE_UPPER_BITS));
    }
}

static inline void
set_tensor_size
(
    lwdaTmaDescv2 *p_desc,
    UINT32 *p_tensor_size,
    UINT32 dims
)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled*>(p_desc);
    for (UINT32 dim = 0; dim < dims; dim++)
    {
        desc->tensor_size[dim] = p_tensor_size[dim] - 1;
    }
}

static inline void
set_traversal_stride_tiled
(
    lwdaTmaDescv2 *p_desc,
    UINT32 *p_traversal_stride,
    UINT32 dims
)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled *>(p_desc);

    desc->traversal_stride_box_0 = 0;

    constexpr UINT32 TRAVERSAL_STRIDE_BITS = 3;
    constexpr UINT32 TRAVERSAL_STRIDE_MASK = (1u << TRAVERSAL_STRIDE_BITS) - 1;

    for (UINT32 dim = 0; dim < dims; dim++)
    {
        UINT32 traversal_stride = p_traversal_stride[dim] - 1;
        traversal_stride = (traversal_stride & TRAVERSAL_STRIDE_MASK)
                           << (dim * TRAVERSAL_STRIDE_BITS);
        desc->traversal_stride_box_0 |= traversal_stride;
    }
}

static inline void set_box_size
(
    lwdaTmaDescv2 *p_desc,
    UINT32 *p_box_size,
    UINT32 dims
)
{
    lwdaTmaDescTiled *desc = reinterpret_cast<lwdaTmaDescTiled*>(p_desc);

    desc->box_size_end = 0;

    constexpr UINT32 BOX_SIZE_BITS = 8;
    constexpr UINT32 BOX_SIZE_MASK = (1 << BOX_SIZE_BITS) - 1;

    if (dims > 1)
    {
        UINT32 box_size_0 = p_box_size[0] - 1;
        box_size_0 = box_size_0 & BOX_SIZE_MASK;
        box_size_0 = box_size_0 << 24;
        desc->traversal_stride_box_0 |= box_size_0;
    }

    for (UINT32 dim = 1; dim < dims; dim++)
    {
        UINT32 box_size = p_box_size[dim] - 1;
        box_size = box_size & BOX_SIZE_MASK;
        box_size = box_size << ((dim - 1) * BOX_SIZE_BITS);
        desc->box_size_end |= box_size;
    }
}

static inline void lwdaSetTmaTileDescriptorv2
(
    lwdaTmaDescv2 *p_desc,
    const void *p_addr,
    UINT32 dims,
    lwdaTmaDescFormatv2 format,
    lwdaTmaDescInterleavev2 interleave,
    lwdaTmaDescSwizzlev2 swizzle,
    lwdaTmaDescPromotiolw2 promotion,
    UINT32 *p_tensor_size,
    UINT64 *p_tensor_stride,
    UINT32 *p_traversal_stride,
    UINT32 *p_box_size,
    UINT32 fill_oob,
    UINT32 round_to_tf32
)
{

    set_tensor_common_0(p_desc, reinterpret_cast<UINT64>(p_addr));
    set_tensor_common_1
    (
        p_desc,
        TENSOR_TILED,
        dims,
        format,
        interleave,
        swizzle,
        fill_oob,
        round_to_tf32,
        promotion
    );

    set_tensor_stride(p_desc, p_tensor_stride, dims);
    set_tensor_size(p_desc, p_tensor_size, dims);

    set_traversal_stride_tiled(p_desc, p_traversal_stride, dims);

    set_box_size(p_desc, p_box_size, dims);
}
