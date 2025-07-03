/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWRISCV_BITMAP_H
#define LWRISCV_BITMAP_H

#include <stdint.h>
#include <lwmisc.h>

/*!
 * @brief The type used for each element of a bitmap
 */
typedef uintptr_t lwriscv_bitmap_element_t;

/*!
 * @brief Number of bits per array element of lwriscv_bitmap_element_t array.
 *
 * 8 bits per byte, bitmap elements are uint64_t typed.
 */
#define LWRISCV_BITMAP_ELEMENT_SIZE (sizeof(lwriscv_bitmap_element_t) * 8)

/*!
 * @brief Utility functions to set/clear/test a bit in a bitmap
 *
 * @param bm the bitmap in which to set/clear/test a bit
 * @param index the index of the bit to set/clear/test
 *
 * @note It is up to the caller to ensure that index is within the bounds of bm.
 *      bm must be at least the size of index / LWRISCV_BITMAP_ELEMENT_SIZE.
 */
LW_FORCEINLINE
static void bitmap_set_bit(lwriscv_bitmap_element_t bm[], uint32_t index)
{
    bm[index / LWRISCV_BITMAP_ELEMENT_SIZE] |= LWBIT_TYPE(index % LWRISCV_BITMAP_ELEMENT_SIZE, lwriscv_bitmap_element_t);
}

LW_FORCEINLINE
static void bitmap_clear_bit(lwriscv_bitmap_element_t bm[], uint32_t index)
{
    bm[index / LWRISCV_BITMAP_ELEMENT_SIZE] &= ~(LWBIT_TYPE(index % LWRISCV_BITMAP_ELEMENT_SIZE, lwriscv_bitmap_element_t));
}

LW_FORCEINLINE
static bool bitmap_test_bit(const lwriscv_bitmap_element_t bm[], uint32_t index)
{
    return (((bm[index / LWRISCV_BITMAP_ELEMENT_SIZE] >> (index % LWRISCV_BITMAP_ELEMENT_SIZE)) & 1) == 1);
}

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/bitmap_legacy.h>
#endif //LWRISCV_FEATURE_LEGACY_API

#endif // LWRISCV_BITMAP_H
