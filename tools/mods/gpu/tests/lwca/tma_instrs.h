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
 * Made formatting changes and added OCG instrinsics for utmaldg and utmapf
 *
 * xmma/include/xmma/utils.h
 */

#pragma once
#include "tma_types.h"

////////////////////////////////////////////////////////////////////////////////
//
// UTMASTG
//
////////////////////////////////////////////////////////////////////////////////

template <uint8_t DIM, lwdaTmaDescTypev2 DESC_TYPE>
inline __device__ void utmastg
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 smem_ptr,
    int32_t cord_0,
    int32_t cord_1,
    int32_t cord_2,
    int32_t cord_3,
    int32_t cord_4
)
{
}

// 2D + TILED
template<>
inline __device__ void utmastg<2, TENSOR_TILED>
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 smem_ptr,
    int32_t cord_0,
    int32_t cord_1,
    int32_t cord_2,
    int32_t cord_3,
    int32_t cord_4
)
{
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    asm volatile("cp.async.bulk.tensor.2d.global.shared [%0, {%1, %2}], [%3];\n"
                 :
                 : "l"(reinterpret_cast<UINT64>(p_desc)),
                   "r"(cord_0),
                   "r"(cord_1),
                   "r"(smem_ptr)
                 : "memory");
#endif
}

// 3D + TILED
template <>
inline __device__ void utmastg<3, TENSOR_TILED>
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 smem_ptr,
    int32_t cord_0,
    int32_t cord_1,
    int32_t cord_2,
    int32_t cord_3,
    int32_t cord_4
)
{
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    asm volatile("cp.async.bulk.tensor.3d.global.shared [%0, {%1, %2, %3}], [%4];\n"
                 :
                 : "l"(reinterpret_cast<UINT64>(p_desc)),
                   "r"(cord_0),
                   "r"(cord_1),
                   "r"(cord_2),
                   "r"(smem_ptr)
                 : "memory" );
#endif
}

// 4D + TILED
template <>
inline __device__ void utmastg<4, TENSOR_TILED>
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 smem_ptr,
    int32_t cord_0,
    int32_t cord_1,
    int32_t cord_2,
    int32_t cord_3,
    int32_t cord_4
)
{
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    asm volatile("cp.async.bulk.tensor.4d.global.shared [%0, {%1, %2, %3, %4}], [%5];\n"
                 :
                 : "l"(reinterpret_cast<UINT64>(p_desc)),
                   "r"(cord_0),
                   "r"(cord_1),
                   "r"(cord_2),
                   "r"(cord_3),
                   "r"(smem_ptr)
                 : "memory");
#endif
}

// 5D + TILED
template <>
inline __device__ void utmastg<5, TENSOR_TILED>
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 smem_ptr,
    int32_t cord_0,
    int32_t cord_1,
    int32_t cord_2,
    int32_t cord_3,
    int32_t cord_4
)
{
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    asm volatile("cp.async.bulk.tensor.5d.global.shared [%0, {%1, %2, %3, %4, %5}], [%6];\n"
                 :
                 : "l"(reinterpret_cast<UINT64>(p_desc)),
                   "r"(cord_0),
                   "r"(cord_1),
                   "r"(cord_2),
                   "r"(cord_3),
                   "r"(cord_4),
                   "r"(smem_ptr)
                 : "memory");
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// U T M A L D G
//
////////////////////////////////////////////////////////////////////////////////

template <uint8_t DIM, lwdaTmaDescTypev2 DESC_TYPE, int USE_TMA_MULTICAST>
inline __device__ void utmaldg
(
    const lwdaTmaDescv2 *p_desc,
    UINT32 urb0,
    UINT32 urb1,
    int32_t urb2,
    int32_t urb3,
    int32_t urb4,
    int32_t urb5,
    int32_t urb6,
    uint16_t packed_offsets,
    uint16_t mcast_mask
)
{
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    if (!USE_TMA_MULTICAST)
    {
        if (DIM == 2 && DESC_TYPE == TENSOR_TILED)
        {
            asm volatile("cp.async.bulk.tensor.2d.shared.global.mbarrier [%0], [%1, {%2, %3}], [%4];\n"
                         :
                         : "r"(urb0),
                           "l"(reinterpret_cast<UINT64>(p_desc)),
                           "r"(urb2),
                           "r"(urb3),
                           "r"(urb1)
                         : "memory");
        }
        if (DIM == 3 && DESC_TYPE == TENSOR_TILED)
        {
            asm volatile("cp.async.bulk.tensor.3d.shared.global.mbarrier [%0], [%1, {%2, %3, %4}], [%5];\n"
                         :
                         : "r"(urb0),
                           "l"(reinterpret_cast<UINT64>(p_desc)),
                           "r"(urb2),
                           "r"(urb3),
                           "r"(urb4),
                           "r"(urb1)
                         : "memory");
        }
        if (DIM == 4 && DESC_TYPE == TENSOR_TILED)
        {
            asm volatile("cp.async.bulk.tensor.4d.shared.global.mbarrier [%0], [%1, {%2, %3, %4, %5}], [%6];\n"
                         :
                         : "r"(urb0),
                           "l"(reinterpret_cast<UINT64>(p_desc)),
                           "r"(urb2),
                           "r"(urb3),
                           "r"(urb4),
                           "r"(urb5),
                           "r"(urb1)
                         : "memory");
        }
        if (DIM == 5 && DESC_TYPE == TENSOR_TILED)
        {
            asm volatile("cp.async.bulk.tensor.5d.shared.global.mbarrier [%0], [%1, {%2, %3, %4, %5, "
                         "%6}], [%7];\n"
                         :
                         : "r"(urb0),
                           "l"(reinterpret_cast<UINT64>(p_desc)),
                           "r"(urb2),
                           "r"(urb3),
                           "r"(urb4),
                           "r"(urb5),
                           "r"(urb6),
                           "r"(urb1)
                         : "memory");
        }
    }
#endif
}

#if (__LWDA_ARCH__ >= 900) && \
    ((__LWDACC_VER_MAJOR__ > 11) || (__LWDACC_VER_MAJOR__ == 11 && __LWDACC_VER_MINOR__ >= 3))
extern "C"
{
    __device__ void __lw_ptx_builtin_ocg_fence_view_async_shared(void);
    __device__ void __lw_ptx_builtin_ocg_cp_async_commit_bulk_global_shared(void);
    __device__ void __lw_ptx_builtin_ocg_cp_async_commit_bulk_shared_global(void);
    __device__ void __lw_ptx_builtin_ocg_cp_async_wait_bulk_global_shared_read(UINT32 count);
    __device__ void __lw_ptx_builtin_ocg_cp_async_wait_bulk_shared_global_write(UINT32 count);
    __device__ void __lw_ptx_builtin_ocg_cp_async_prefetch_tensor_global_2d(UINT64 tDesc, int32_t c1, int32_t c2, UINT64 desc);
    __device__ void __lw_ptx_builtin_ocg_cp_async_prefetch_tensor_global_3d(UINT64 tDesc, int32_t c1, int32_t c2, int32_t c3, UINT64 desc);
    __device__ void __lw_ptx_builtin_ocg_cp_async_prefetch_tensor_global_4d(UINT64 tDesc, int32_t c1, int32_t c2, int32_t c3, int32_t c4, UINT64 desc);
    __device__ void __lw_ptx_builtin_ocg_cp_async_prefetch_tensor_global_5d(UINT64 tDesc, int32_t c1, int32_t c2, int32_t c3, int32_t c4, int32_t c5, UINT64 desc);
}
#endif

inline __device__ void fence_view_async_shared()
{
    // Issue a shared memory fence for async operations (FENCE.VIEW.ASYNC.S)
    // only compiles on sm90+
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    __lw_ptx_builtin_ocg_fence_view_async_shared();
#endif
}

inline __device__ void tmastg_flush()
{
    // Indicate arrival of warp issuing UTMASTG (UTMACMDFLUSH)
    // only compiles on sm90+
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    __lw_ptx_builtin_ocg_cp_async_commit_bulk_global_shared();
#endif
}

inline __device__ void tmastg_wait(UINT32 count)
{
    // Wait on prior N (count) TMASTG instructions to complete (DEPBAR LE SB count)
    // Only compiles on sm90+
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    __lw_ptx_builtin_ocg_cp_async_wait_bulk_global_shared_read(count);
#endif

}

inline __device__ void tmaldg_flush()
{
    // Indicate arrival of warp issuing UTMALDG (UTMACMDFLUSH)
    // only compiles on sm90+
#if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
    __lw_ptx_builtin_ocg_cp_async_commit_bulk_shared_global();
#endif

}

inline __device__ void tmaldg_wait(UINT32 count)
{
    // Wait on prior N (count) TMALDG instructions to complete
    // Only compiles on sm90+
    #if defined(__LWDA_ARCH__) && __LWDA_ARCH__ >= 900
        __lw_ptx_builtin_ocg_cp_async_wait_bulk_shared_global_write(count);
    #endif
}
