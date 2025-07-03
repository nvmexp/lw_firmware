/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file     dmemovl.c
 * @copydoc  dmemovl.h
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "lwoslayer.h"
#include "dmemovl.h"
#include "lwostask.h"
#include "lwos_ovl_priv.h"

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Controls code ability to dynamically allocate memory.
 */
UPROC_STATIC LwBool LwosBHeapAllocationsBlocked = LW_FALSE;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Inline Functions ------------------------------- */
/*!
 * @brief   Align-up macro for @ref DmemOvlSize arguments
 *
 * @param[in]   size        Size to align
 * @param[in]   alignment   Alignment to which to align
 *
 * @return  size aligned up to alignment
 */
static inline DmemOvlSize
dmemOvlSizeAlignUp(DmemOvlSize size, DmemOvlSize alignment)
{
    // The _CT macro is used so that the arguments are *not* promoted to LwU32s.
    return LWUPROC_ALIGN_UP_CT(size, alignment);
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Controls code ability to dynamically allocate memory.
 *
 * To improve ucode safety we restrict dynamic memory allocations only to the
 * ucode INIT phase assuring that once INIT is complete we cannot fail due to
 * the lack of memory.
 */
void
lwosHeapAllocationsBlock_IMPL(void)
{
    LwosBHeapAllocationsBlocked = LW_TRUE;
}

/*!
 * @brief   Allocates aligned memory block within requested DMEM overlay.
 *
 * @param[in]   ovlIdx      Index of the target DMEM overlay.
 * @param[in]   size        Size of the requested memory bock.
 * #param[in]   alignment   Requested memory block alignemnt (must be 2^N).
 *
 * @return      Pointer to the allocated DMEM block (or NULL on failure).
 */
void *
lwosMallocAligned_IMPL
(
    LwU8    ovlIdx,
    LwU32   size,
    LwU32   alignment
)
{
    void   *pvReturn    = NULL;

#if !defined(DMEM_VA_SUPPORTED)
    ovlIdx = OVL_INDEX_DMEM(os);
#endif //!defined(DMEM_VA_SUPPORTED)

    // Can't make allocations larger than the maximum supported overlay size
    if (size > DMEM_OVL_SIZE_LIMIT)
    {
        return NULL;
    }

    if (!ONEBITSET(alignment) ||
        (alignment > DMEM_OVL_SIZE_LIMIT))
    {
        return NULL;
    }

    // Ensure that blocks are always aligned to the required number of bytes.
    if (size <= LW_U32_MAX - (lwrtosBYTE_ALIGNMENT - 1U))
    {
        size = lwrtosBYTE_ALIGN(size);
    }
    else
    {
        // lwrtosBYTE_ALIGN would overflow size
        return NULL;
    }

    // This will catch both the case when the caller has supplied a size of 0
    // and the case when the lwrtosBYTE_ALIGN has caused size to overflow.
    if (size == 0U)
    {
        return NULL;
    }

    lwrtosENTER_CRITICAL();
    {
#ifdef HEAP_BLOCKING
        if (LwosBHeapAllocationsBlocked)
        {
            // NJ-TODO: Temporary while cleaning code.
            OS_HALT();
        }
        else
#endif // HEAP_BLOCKING
        {
            LwU32       addrBase   = _dmem_ovl_start_address[ovlIdx];
            DmemOvlSize ovlSizeMax = _dmem_ovl_size_max[ovlIdx];
            DmemOvlSize ovlSizeOld = _dmem_ovl_size_lwrrent[ovlIdx];
            DmemOvlSize ovlSizeNew;
            LwU32       addrReturn;

            ovlSizeNew = dmemOvlSizeAlignUp(ovlSizeOld, (DmemOvlSize)alignment);
            if (ovlSizeNew < ovlSizeOld)
            {
                //
                // LWUPROC_ALIGN_UP has exceeded the limit of DmemOvlSize.
                // Not enough space for this allocation.
                //
                goto dmemOverlayMallocAligned_exit;
            }

            addrReturn = addrBase + ovlSizeNew;

            ovlSizeNew += (DmemOvlSize) size;
            if (ovlSizeNew < size)
            {
                // We have overflowed ovlSizeNew. Not enough space for this allocation.
                goto dmemOverlayMallocAligned_exit;
            }

            // Check there is enough room left for the allocation.
            if (ovlSizeNew <= ovlSizeMax)
            {
                // Return the next free address (aligned up) and set the new size.
                pvReturn = LW_LWUPTR_TO_PTR(addrReturn);
                _dmem_ovl_size_lwrrent[ovlIdx] = ovlSizeNew;

#if !defined(ON_DEMAND_PAGING_BLK) && defined(DMEM_VA_SUPPORTED)
                if (ovlIdx != OVL_INDEX_DMEM(os))
                {
                    //
                    // Allocations within DMEM-VA overlays are allowed only by the
                    // running tasks (once the task scheduler has started), and only
                    // when target overlay is attached to a lwrrently running task.
                    //
                    {
                        LwU8                    index;
                        LwU8                    end;

                        OS_ASSERT(pPrivTcbLwrrent != NULL);

                        index  = pPrivTcbLwrrent->ovlCntImemLS;
#ifdef HS_OVERLAYS_ENABLED
                        index += pPrivTcbLwrrent->ovlCntImemHS;
#endif // HS_OVERLAYS_ENABLED
                        end    = index + pPrivTcbLwrrent->ovlCntDmem;

                        while (index < end)
                        {
                            if (pPrivTcbLwrrent->ovlList[index] == ovlIdx)
                            {
                                break;
                            }

                            index++;
                        }

                        OS_ASSERT(index < end);
                    }

                    // Bring in DMEM block(s) if alloc. spanned acros the page boundary.
                    {
                        LwU32 addrLwrr = LWUPROC_ALIGN_UP(addrBase + ovlSizeOld, DMEM_BLOCK_SIZE);
                        LwU32 addrEnd  = LWUPROC_ALIGN_UP(addrBase + ovlSizeNew, DMEM_BLOCK_SIZE);

                        while (addrLwrr < addrEnd)
                        {
                            falc_setdtag(addrLwrr, lwosOvlDmemFindBestBlock());

                            // Mark the tag valid in array (see bug 1845883).
                            DMTAG_WAR_1845883_VALID_SET(DMEM_ADDR_TO_IDX(addrLwrr));

                            addrLwrr += DMEM_BLOCK_SIZE;
                        }
                    }
               }
#endif // !defined(ON_DEMAND_PAGING_BLK) && defined(DMEM_VA_SUPPORTED)
            }
        }
    }

dmemOverlayMallocAligned_exit:
    lwrtosEXIT_CRITICAL();

    return pvReturn;
}

/*!
 * @brief   Allocates aligned memory block within requested DMEM overlay and
 *          initializes it to zero.
 *
 * @param[in]   ovlIdx      Index of the target DMEM overlay.
 * @param[in]   size        Size of the requested memory bock.
 * #param[in]   alignment   Requested memory block alignemnt (must be 2^N).
 *
 * @return      Pointer to the allocated DMEM block.
 */
void *
lwosCallocAligned_IMPL
(
    LwU8    ovlIdx,
    LwU32   size,
    LwU32   alignment
)
{
    void *pAllocation = lwosMallocAligned_IMPL(ovlIdx, size, alignment);

    // Clear allocated memory.
    if (pAllocation != NULL)
    {
        (void) memset(pAllocation, 0x00, size);
    }

    return pAllocation;
}

/*!
 * @brief   Allocates memory block within requested DMEM overlay.
 *
 * @param[in]   ovlIdx  Index of the target DMEM overlay.
 * @param[in]   size    Size of the requested memory bock.
 *
 * @return      Pointer to the allocated DMEM block.
 */
void *
lwosMalloc_IMPL
(
    LwU8    ovlIdx,
    LwU32   size
)
{
    return lwosMallocAligned_IMPL(ovlIdx, size, lwrtosBYTE_ALIGNMENT);
}

/*!
 * @brief   Allocates memory block within requested DMEM overlay and initializes
 *          it to zero.
 *
 * @param[in]   ovlIdx  Index of the target DMEM overlay.
 * @param[in]   size    Size of the requested memory bock.
 *
 * @return      Pointer to the allocated DMEM block.
 */
void *
lwosCalloc_IMPL
(
    LwU8    ovlIdx,
    LwU32   size
)
{
    return lwosCallocAligned_IMPL(ovlIdx, size, lwrtosBYTE_ALIGNMENT);
}

void *
lwosMallocResident_IMPL
(
    LwU32   size
)
{
    return lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, size);
}

void *
lwosCallocResident_IMPL
(
    LwU32   size
)
{
    return lwosCalloc_IMPL(OVL_INDEX_OS_HEAP, size);
}

/*!
 * @brief   Gets the next free byte of the heap. This is here to
 *          support a legacy use case where parts of the heap were
 *          used temporarily at init. No new use cases of this API
 *          should be introduced.
 *
 * @return  Pointer to the next free byte
 */
LwUPtr
lwosOsHeapGetNextFreeByte(void)
{
    if (_dmem_ovl_start_address[OVL_INDEX_OS_HEAP] <
        (LW_U32_MAX - _dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP]))
    {
        return _dmem_ovl_start_address[OVL_INDEX_OS_HEAP] +
        _dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP];
    }
    else
    {
        OS_HALT();
        return 0;
    }
}

/*!
 * @brief   Gets the free heap size. This is here to
 *          support a legacy use case where parts of the heap were
 *          used temporarily at init. No new use cases of this API
 *          should be introduced.
 *
 * @return  free heap size in bytes
 */
LwU32
lwosOsHeapGetFreeHeap(void)
{
    return ((LwU32)_dmem_ovl_size_max[OVL_INDEX_OS_HEAP]) -
        ((LwU32)_dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP]);
}

/*!
 * @copydoc lwosOsHeapGetFreeHeap
 */
FLCN_STATUS
lwosDataSectionMetadataGet
(
    LwU8   sectionIdx,
    LwU32 *pSizeMax,
    LwU32 *pSizeUsed
)
{
    if (sectionIdx >= OVL_COUNT_DMEM())
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if ((pSizeMax == NULL) || (pSizeUsed == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pSizeMax = _dmem_ovl_size_max[sectionIdx];
    *pSizeUsed = _dmem_ovl_size_lwrrent[sectionIdx];

    return FLCN_OK;
}
