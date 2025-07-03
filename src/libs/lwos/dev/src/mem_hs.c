/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"
#include "mem_hs.h"
#include "lwostask.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------- Prototypes ------------------------------------- */
static void _libMemHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libMemHs", "start")
    GCC_ATTRIB_USED();

static LwBool _isPtrValid(const void *ptr, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libMemHs", "_isPtrValid");

/* ------------------------- Private Functions ------------------------------ */
/*!
 * _libMemHSEntry
 *
 * @brief Entry function of HS mem library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libMemHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

static LwBool _isPtrValid(const void *ptr, LwU32 size)
{
#ifdef DMEM_VA_SUPPORTED
    LwU32 reg;
    LwU32 vaBound;

    reg = REG_RD32(CSB, CSB_REG(_DMVACTL));

    if ((reg & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        return LW_FALSE;
    }

    vaBound = DRF_VAL(_PFALCON, _FALCON_DMVACTL, _BOUND, reg);

    // Don't permit buffer to span across VA_BOUND limit
    if ((((LwU32)ptr + size) > vaBound) && ((LwU32)ptr < vaBound))
    {
        return LW_FALSE;
    }
#else
extern LwU32    _end_physical_dmem[];
    if (((LwU32)ptr + size) > (LwU32)_end_physical_dmem)
    {
        return LW_FALSE;
    }
#endif
    if (ptr == NULL)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief High secure version of standard memcmp().
 *
 * @param[in]  pS1   Pointer to first memory block.
 * @param[in]  pS2   Pointer to second memory block.
 * @param[in]  size  Number of bytes to be compared.
 *
 * @return 0 if the memory blocks' content are equal; -1 if the input pointer is
 *         invalid. Other value if the memory blocks' content are unequal.
 */
int memcmp_hs
(
    const void *pS1,
    const void *pS2,
    LwU32       size
)
{
    const LwU8 *pSrc1  = (const LwU8*)pS1;
    const LwU8 *pSrc2  = (const LwU8*)pS2;
    int         retVal = 0;
    LwU32       index;

    if (!_isPtrValid(pS1, size) || !_isPtrValid(pS2, size))
    {
        return -1;
    }

    for (index = 0; index < size; index++)
    {
        retVal |= (pSrc1[index] ^ pSrc2[index]);
    }

    return retVal;
}

/*!
 * @brief High secure version of standard memcpy().
 *
 * @param[out]  pDst  Memory block to copy to.
 * @param[in]   pSrc  Memory block to copy from.
 * @param[in]   size  Number of bytes to copy.
 *
 * @return pDst
 */
void *memcpy_hs
(
    void       *pDst,
    const void *pSrc,
    LwU32       size
)
{
    LwU32       *pDst32 = (LwU32*)pDst;
    const LwU32 *pSrc32 = (const LwU32*)pSrc;
    LwU8        *pDst8;
    const LwU8  *pSrc8;

    if (!_isPtrValid(pSrc, size) || !_isPtrValid(pDst, size))
    {
        return NULL;
    }

    // Copy all 32-bit initial aligned data
    if (((LwU32)pDst % sizeof(LwU32) == 0) &&
        ((LwU32)pSrc % sizeof(LwU32) == 0))
    {
        while (size > sizeof(LwU32))
        {
            *pDst32 = *pSrc32;
            pDst32++;
            pSrc32++;
            size -= sizeof(LwU32);
        }
    }

    // Copy remaining data
    pDst8 = (LwU8*)pDst32;
    pSrc8 = (LwU8*)pSrc32;
    while (size > 0)
    {
        *pDst8 = *pSrc8;
        pDst8++;
        pSrc8++;
        size--;
    }

    return pDst;
}

/*!
 * @brief High secure version of standard memset().
 *
 * @param[out]  pDst   Memory block to set.
 * @param[in]   value  Value to set each byte in the memory block to.
 * @param[in]   size   Number of bytes to set.
 *
 * @return pDst
 */
void *memset_hs
(
    void *pDst,
    LwU8  value,
    LwU32 size
)
{
    LwU32 *pDst32 = (LwU32*)pDst;
    LwU8  *pDst8;

    if (!_isPtrValid(pDst, size))
    {
        return NULL;
    }

    // Perform a memset on all 32-bit aligned data
    if ((LwU32)pDst % sizeof(LwU32) == 0)
    {
        LwU32 value32 = (value << 24) | (value << 16) |
                        (value << 8) | value;
        while (size > sizeof(LwU32))
        {
            *pDst32 = value32;
            pDst32++;
            size -= sizeof(LwU32);
        }
    }

    // Perform memset on all remaining data
    pDst8 = (LwU8*)pDst32;
    while (size > 0)
    {
        *pDst8 = value;
        pDst8++;
        size--;
    }

    return pDst;
}
