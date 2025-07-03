/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMEMOVL_H
#define DMEMOVL_H

/*!
 * @file     dmemovl.h
 * @brief    Heap allocator
 * @details  It is compile time configurable to either
 *           1. freeable heap allocator for kernel
 *           2. non-freeable per overlay (FLCN) / section (RISCV) allocator.
 *
 *           We don't support freeable heap allocation on falcon (FLCN).
 *           So the implementations of the library are in dmemovl.c for FLCN and
 *           dmemovl-lwriscv.c for RISCV. With RISCV, if FREEABLE_HEAP is not
 *           set, the purpose of the library is set to allow non-freeable
 *           per overlay/section heap allocations.
 *
 *           Overlay is a concept on FLCN where imem and dmem are explicitly
 *           loaded and unloaded by SW prior access. For RISCV, overlay is not
 *           needed since RISCV is able to access all possible memory surface in
 *           its own memory space. But we have many existing code using overlays
 *           so in order to be backward compatible, we retain the concept.
 *           Overlay index on FLCN is comparable to section index on RISCV.
 */

/* ------------------------- System Includes -------------------------------- */
#include <lwmisc.h>
#include "lwuproc.h"
#include "flcnretval.h"

/* ------------------------- Defines ---------------------------------------- */

/*!
 * @brief   A wrapper around a typename to work around Coverity flagging _type*
 *          as a violation of MISRA rule 20.7, where it flagged _type for not
 *          being parenthesized in the typecast.
 *
 * @param[in]   _type the type to wrap.
 */
#define COVERITY_TYPE_EXPR_FIX(_type) _type

#define lwosMallocType(_ovl,_cnt,_type)                                        \
    ((COVERITY_TYPE_EXPR_FIX(_type)*)lwosMalloc((_ovl), ((LwU32)_cnt) * sizeof(_type)))

#define lwosCallocType(_ovl,_cnt,_type)                                        \
    ((COVERITY_TYPE_EXPR_FIX(_type)*)lwosCalloc((_ovl), ((LwU32)_cnt) * sizeof(_type)))

#define lwosMallocResidentType(_cnt,_type)                                     \
    ((COVERITY_TYPE_EXPR_FIX(_type)*)lwosMallocResident(((LwU32)_cnt) * sizeof(_type)))

#define lwosCallocResidentType(_cnt,_type)                                     \
    ((COVERITY_TYPE_EXPR_FIX(_type)*)lwosCallocResident(((LwU32)_cnt) * sizeof(_type)))

/* ------------------------- Function Prototypes ---------------------------- */
#define  lwosHeapAllocationsBlock(...) MOCKABLE(lwosHeapAllocationsBlock)(__VA_ARGS__)
/*!
 * @brief    Turn off the ability to dynamically allocate memory.
 *
 * @details  To improve ucode safety we restrict dynamic memory allocations only
 *           to the ucode INIT phase assuring that once INIT is complete we
 *           cannot fail due to the lack of memory.
 */
void lwosHeapAllocationsBlock(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtRpc", "lwosHeapAllocationsBlock");

#define lwosMallocAligned(...) MOCKABLE(lwosMallocAligned)(__VA_ARGS__)
/*!
 * @brief      Allocates aligned memory block within requested DMEM
 *             overlay/section. In a fully-resident layout, ovlIdx is
 *             ignored and the allocation is made from the resident heap.
 *
 * @param[in]  ovlIdx     Index of the target DMEM overlay/section.
 * @param[in]  size       Size of the requested memory bock.
 * @param[in]  alignment  Requested memory block alignment (must be 2^N).
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosMallocAligned(LwU8 ovlIdx, LwU32 size, LwU32 alignment)
    GCC_ATTRIB_SECTION("imem_resident", "lwosMallocAligned");

#define lwosCallocAligned(...) MOCKABLE(lwosCallocAligned)(__VA_ARGS__)
/*!
 * @brief      Allocates aligned memory block within requested DMEM
 *             overlay/section and initializes it to zero.
 *
 * @details    Please refer to @ref lwosMallocAligned for more details.
 *
 * @param[in]  ovlIdx     Index of the target DMEM overlay/section.
 * @param[in]  size       Size of the requested memory bock.
 * @param[in]  alignment  Requested memory block alignment (must be 2^N).
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosCallocAligned(LwU8 ovlIdx, LwU32 size, LwU32 alignment)
    GCC_ATTRIB_SECTION("imem_resident", "lwosCallocAligned");

#define lwosMalloc(...) MOCKABLE(lwosMalloc)(__VA_ARGS__)
/*!
 * @brief      Allocates memory block within requested DMEM overlay/section.
 *
 * @details    Please refer to @ref lwosMallocAligned for more details.
 *
 * @param[in]  ovlIdx     Index of the target DMEM overlay/section.
 * @param[in]  size       Size of the requested memory bock.
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosMalloc(LwU8 ovlIdx, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "lwosMalloc");

#define lwosCalloc(...) MOCKABLE(lwosCalloc)(__VA_ARGS__)
/*!
 * @brief      Allocates memory block within requested DMEM overlay/section and
 *             initializes it to zero.
 *
 * @details    Please refer to @ref lwosMallocAligned for more details.
 *
 * @param[in]  ovlIdx     Index of the target DMEM overlay/section.
 * @param[in]  size       Size of the requested memory bock.
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosCalloc(LwU8 ovlIdx, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "lwosCalloc");

#define lwosMallocResident(...) MOCKABLE(lwosMallocResident)(__VA_ARGS__)
/*!
 * @brief      Allocates memory block within resident DMEM overlay/section.
 *
 * @details    The allocation is made from the resident heap. Please refer to
 *             @ref lwosMallocAligned for more details.
 *
 * @param[in]  size       Size of the requested memory bock.
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosMallocResident(LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "lwosMallocResident");

#define lwosCallocResident(...) MOCKABLE(lwosCallocResident)(__VA_ARGS__)
/*!
 * @brief      Allocates memory block within resident DMEM overlay/section and
 *             initializes it to zero.
 *
 * @details    Please refer to @ref lwosMallocResident for more details.
 *
 * @param[in]  size       Size of the requested memory bock.
 *
 * @return     Start virtual address to the allocated DMEM block on success.
 * @return     NULL on failure.
 */
void *lwosCallocResident(LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "lwosCallocResident");

/*!
 * @brief      [Deprecated] Gets the next free byte of the heap.
 * @details    This is here to support a legacy use case where parts of the heap
 *             were used temporarily at init. No new use cases of this API
 *             should be introduced.
 *
 * @return     Pointer to the next free byte.
 */
LwUPtr lwosOsHeapGetNextFreeByte(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosOsHeapGetNextFreeByte");

/*!
 * @brief      [Deprecated] Gets the free heap size.
 * @details    This is here to support a legacy use case where parts of the heap
 *             were used temporarily at init. No new use cases of this API
 *             should be introduced.
 *
 * @return     Free heap size in bytes.
 */
LwU32 lwosOsHeapGetFreeHeap(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosOsHeapGetFreeHeap");

/*!
 * @brief   Retrieve data section metadata for a given overlay index.
 *
 * @param[in]   sectionIdx  Index of the requested data section
 * @param[out]  pSizeMax    Buffer to hold the section's max size
 * @param[out]  pSizeUsed   Buffer to hold the section's used size
 *
 * @return  FLCN_OK                     uppon sucessfull retrieval
 * @return  FLCN_ERR_ILWALID_INDEX      when @ref ovlIdx exceeds section count
 * @return  FLCN_ERR_ILWALID_ARGUMENT   when NULL pointers passed
 */
FLCN_STATUS lwosDataSectionMetadataGet(LwU8 sectionIdx, LwU32 *pSizeMax, LwU32 *pSizeUsed)
    GCC_ATTRIB_SECTION("imem_resident", "lwosDataSectionMetadataGet");

#endif // DMEMOVL_H
