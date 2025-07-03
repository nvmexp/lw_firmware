/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_BITMAP_LEGACY_H
#define LIBLWRISCV_BITMAP_LEGACY_H

/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

typedef lwriscv_bitmap_element_t LWRISCV_BITMAP_ELEMENT;

#define bitmapSetBit(bm, index) \
    bitmap_set_bit(bm, index)

#define bitmapClearBit(bm, index) \
    bitmap_clear_bit(bm, index)

#define bitmapTestBit(bm, index) \
    bitmap_test_bit(bm, index)

#endif // LIBLWRISCV_BITMAP_LEGACY_H
