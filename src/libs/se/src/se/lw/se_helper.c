/*
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   se_helper.c
 * @brief  Non halified helper functions
 *
 */

#include "lwuproc.h"
#include "se_private.h"
#include "flcnifcmn.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static inline LwBool _isPtrValid(const void *ptr, LwU32 size)
    GCC_ATTRIB_ALWAYSINLINE();
static inline void *_seMemSet(void *pDst, LwU8 value, LwU32 size)
    GCC_ATTRIB_ALWAYSINLINE();
static inline void *_seMemCpy(void *pDst, const void *pSrc, LwU32 size)
    GCC_ATTRIB_ALWAYSINLINE();
static inline void _seSwapEndianness(void *pOutData, const void *pInData, const LwU32 size)
    GCC_ATTRIB_ALWAYSINLINE();

/* ------------------------- Implementation --------------------------------- */

/*!
* @brief Compares big numbers for equality
*
* @param[in]  pX              First number to compare
* @param[in]  pY              Second number to compare     
* @parma[in]  sizeInDwords    Size of numbers to compare in DWORDS
*
* @return LW_TRUE if numbers are equal, LW_FALSE otherwise
*
*/
LwBool
seBigNumberEqual(LwU32 *pX, LwU32 *pY, LwU32 sizeInDwords)
{
    LwU32 i;
    LwBool status = LW_TRUE;

    for (i = 0; i < sizeInDwords; i++)
    {
        if (pX[i] != pY[i])
        {
            status = LW_FALSE;
        }
    }
    return status;
}

/*!
 * @brief Check if memory is within the physical dmem range. 
 *
 * @param[in]   ptr    Start memory address
 * @param[in]   size   Size of the memory
 *
 * @return LW_TRUE   If memory is within the range. 
 *         LW_FALSE  If memory is not within the range or start memory address is NULL  
 */
static inline LwBool
_isPtrValid(const void *ptr, LwU32 size)
{
    extern LwU32    _end_physical_dmem[];

    if (((LwUPtr)ptr + size) > (LwUPtr)_end_physical_dmem)
    {
        return LW_FALSE;
    }

    if (ptr == NULL)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief SE version of memset().
 *
 * @param[out]  pDst   Memory block to set.
 * @param[in]   value  Value to set each byte in the memory block to.
 * @param[in]   size   Number of bytes to set.
 *
 * @return pDst
 */
static inline void
*_seMemSet(void *pDst, LwU8 value, LwU32 size)
{
    LwU32 *pDst32 = (LwU32*)pDst;
    LwU8  *pDst8;

    if (!_isPtrValid(pDst, size))
    {
        return NULL;
    }

    // Perform a memset on all 32-bit aligned data
    if ((LwUPtr)pDst % sizeof(LwU32) == 0)
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

/*!
 * @brief SE Lib version of memcpy()
 *
 * @param[out]  pDst  Memory block to copy to.
 * @param[in]   pSrc  Memory block to copy from.
 * @param[in]   size  Number of bytes to copy.
 * 
 * @return pDst
 */
static inline void
*_seMemCpy(void *pDst, const void *pSrc, LwU32 size)
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
    if (((LwUPtr)pDst % sizeof(LwU32) == 0) &&
        ((LwUPtr)pSrc % sizeof(LwU32) == 0))
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
 * @brief Swaps the Endianess of any given Array
 *
 * @param[in]  pInData   Input array to be swapped for endiness
 * @param[out] pOutData  output array after swapping,
 * @param[in]  size      array size.
 */
static inline void
_seSwapEndianness(void *pOutData, const void  *pInData, const LwU32 size)
{
    LwU32 i;

    if (!_isPtrValid(pInData, size) || !_isPtrValid(pOutData, size))
    {
        return;
    }

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        LwU8 b2 = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[i] = b2;
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/*!
 * @brief SE version of memset() in LS Mode
 *
 * @param[out]  pDst   Memory block to set.
 * @param[in]   value  Value to set each byte in the memory block to.
 * @param[in]   size   Number of bytes to set.
 *
 * @return pDst
 */
void 
*seMemSet(void *pDst, LwU8 value, LwU32 size)
{
    return _seMemSet(pDst, value, size);
}

/*!
 * @brief SE Lib version of memcpy() in LS Mode
 *
 * @param[out]  pDst  Memory block to copy to.
 * @param[in]   pSrc  Memory block to copy from.
 * @param[in]   size  Number of bytes to copy.
 * 
 * @return pDst
 */
void 
*seMemCpy(void *pDst, const void *pSrc, LwU32 size)
{       
    return _seMemCpy(pDst, pSrc, size);
}

/*!
 * @brief Swaps the Endianess of any given Array in LS Mode 
 *
 * @param[in]  pInData   Input array to be swapped for endiness
 * @param[out] pOutData  output array after swapping,
 * @param[in]  size      array size.
 */
void
seSwapEndianness(void *pOutData, const void  *pInData, const LwU32 size)
{
    _seSwapEndianness(pOutData, pInData, size);
}


/*!
 * @brief SE version of memset() in HS Mode
 *
 * @param[out]  pDst   Memory block to set.
 * @param[in]   value  Value to set each byte in the memory block to.
 * @param[in]   size   Number of bytes to set.
 *
 * @return pDst
 */
void 
*seMemSetHs(void *pDst, LwU8 value, LwU32 size)
{
    return _seMemSet(pDst, value, size);
}

/*!
 * @brief SE Lib version of memcpy() in HS Mode
 *
 * @param[out]  pDst  Memory block to copy to.
 * @param[in]   pSrc  Memory block to copy from.
 * @param[in]   size  Number of bytes to copy.
 * 
 * @return pDst
 */
void 
*seMemCpyHs(void *pDst, const void *pSrc, LwU32 size)
{
    return _seMemCpy(pDst, pSrc, size);
}

/*!
 * @brief Swaps the Endianess of any given Array in HS Mode
 *
 * @param[in]  pInData   Input array to be swapped for endiness
 * @param[out] pOutData  output array after swapping,
 * @param[in]  size      array size.
 */
void
seSwapEndiannessHs(void *pOutData, const void  *pInData, const LwU32 size)
{
    _seSwapEndianness(pOutData, pInData, size);
}

/*!
 * @brief Returns the product of two 32-bit unsigned integers
 *        in a 64-bit unsigned integer.
 *
 * @param[in]  a
 *      The first term as a 32-bit unsigned integer
 *
 * @param[in]  b
 *      The second term as a 32-bit unsigned integer
 *
 * @returns The result of a * b as a 64-bit unsigned integer
 *
 */
LwU64
seMulu32Hs(LwU32 a, LwU32 b)
{
    LwU16 a1, a2;
    LwU16 b1, b2;
    LwU32 a2b2, a1b2, a2b1, a1b1;
    LwU64 result;

    /*
     * Falcon has a 16-bit multiplication instruction.
     * Break down the 32-bit multiplication into 16-bit operations.
     */
    a1 = a >> 16;
    a2 = a & 0xffff;

    b1 = b >> 16;
    b2 = b & 0xffff;

    a2b2 = a2 * b2;
    a1b2 = a1 * b2;
    a2b1 = a2 * b1;
    a1b1 = a1 * b1;

    result = (LwU64)a2b2 + ((LwU64)a1b2 << 16) +
            ((LwU64)a2b1 << 16) + ((LwU64)a1b1 << 32);
    return result;
}

