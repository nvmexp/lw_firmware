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
 * xmma/include/xmma/hopper/emu/lwda_tma_types.h
 */

#pragma once
#include <cstdint>

constexpr UINT64 k_tensor_global_address_mask = ((UINT64(1) << 55) - 1);
constexpr UINT64 k_tensor_stride_mask = ((UINT64(1) << 37) - 1);
constexpr uint8_t k_traversal_stride_mask = ((uint8_t(1) << 4) - 1);

typedef enum
{
    TENSOR_TILED = 0,
    TENSOR_IM2CO
} lwdaTmaDescTypev2;

typedef enum
{
    INTERLEAVE_DISABLED = 0,
    INTERLEAVE_16B,
    INTERLEAVE_32B
} lwdaTmaDescInterleavev2;

typedef enum
{
    SWIZZLE_DISABLED,
    SWIZZLE_32B,
    SWIZZLE_64B,
    SWIZZLE_128B,
    SWIZZLE_MAX
} lwdaTmaDescSwizzlev2;

typedef enum
{
    BARRIER64,
    BARRIER128
} barrier_t;

typedef enum
{
    PROMOTION_DISABLED = 0,
    PROMOTION_64B,
    PROMOTION_128B,
    PROMOTION_256B
} lwdaTmaDescPromotiolw2;

typedef enum
{
    U8 = 0,
    U16,
    U32,
    S32,
    U64,
    S64,
    F16_RN,
    F32_RN,
    F32_FTZ_RN,
    F64_RN,
    BF16_RN,
    FORMAT_MAX
} lwdaTmaDescFormatv2;

typedef struct
{
    UINT64 tensor_common0;
    UINT32 tensor_common1;

    UINT32 tensor_stride_lower[4];  //< 36b of 64b with 4B aligned
    UINT32 tensor_stride_upper;
    UINT32 tensor_size[5];          //< value -1
    UINT32 traversal_stride_box_0;  //< packed 3b (-1)

    UINT32 box_size_end;
} lwdaTmaDescTiled;

typedef struct alignas(64)
{
    UINT64 data[8];
} lwdaTmaDescv2;
