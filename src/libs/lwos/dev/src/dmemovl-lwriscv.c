/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "lwrtos.h"
/* ------------------------ Application includes ---------------------------- */
#include "dmemovl.h"
#include <lwriscv/print.h>
#include "config/g_sections_riscv.h"
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief  Section index to make resident heap allocation.
 */
#ifdef PMU_RTOS
#define RESIDENT_HEAP_ID  UPROC_SECTION_REF(dmem_osHeap)
#elif GSP_RTOS
#define RESIDENT_HEAP_ID  UPROC_SECTION_REF(kernelData)
#elif SEC2_RTOS
#define RESIDENT_HEAP_ID  UPROC_SECTION_REF(kernelData)
#elif SOE_RTOS
#define RESIDENT_HEAP_ID  UPROC_SECTION_REF(kernelData)
#endif
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief  Controls code ability to runtime memory allocation.
 */
LwBool LwosBHeapAllocationsBlocked = LW_FALSE;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
lwosHeapAllocationsBlock(void)
{
    LwosBHeapAllocationsBlocked = LW_TRUE;
}

sysSHARED_CODE void *
lwosMallocAligned(LwU8 sectionIdx, LwU32 size, LwU32 alignment)
{
#ifdef FREEABLE_HEAP
    return pvPortMallocFreeable(size);
#else
    void *pvReturn = NULL;

    if (!ONEBITSET(alignment))
    {
        goto dmemOverlayMallocAligned_exit;
    }

    // Ensure that blocks are always aligned to the required number of bytes.
    size = lwrtosBYTE_ALIGN(size);
    lwrtosENTER_CRITICAL();
    {
        if (!LwosBHeapAllocationsBlocked)
        {
            LwUPtr   addrBase   = SectionDescStartVirtual[sectionIdx];
            LwLength ovlSizeMax = SectionDescMaxSize[sectionIdx];
            LwLength ovlSizeOld = SectionDescHeapSize[sectionIdx];
            LwLength ovlSizeNew;
            LwUPtr   addrReturn;

            ovlSizeNew = LW_ALIGN_UP(ovlSizeOld, alignment);
            addrReturn = addrBase + ovlSizeNew;
            ovlSizeNew += size;

            // Check there is enough room left for the allocation.
            if (ovlSizeNew <= ovlSizeMax)
            {
                // Return the start VA
                pvReturn = (void *)addrReturn;
                // Set the new size.
                SectionDescHeapSize[sectionIdx] = ovlSizeNew;
            }
            else
            {
                dbgPrintf(LEVEL_ALWAYS, "Allocation failed in Section=%d, newSize=%llx, oldSize=%llx, maxSize=%llx\n",
                    sectionIdx, ovlSizeNew, ovlSizeOld, ovlSizeMax);
            }
        }
        else
        {
            dbgPrintf(LEVEL_ALWAYS, "Allocation after INIT in section=%d!!!\n", sectionIdx);
        }
    }
    lwrtosEXIT_CRITICAL();

dmemOverlayMallocAligned_exit:
    return pvReturn;
#endif
}

sysSHARED_CODE void *
lwosCallocAligned(LwU8 sectionIdx, LwU32 size, LwU32 alignment)
{
    void *pAllocation = lwosMallocAligned(sectionIdx, size, alignment);

    if (pAllocation != NULL)
    {
        memset(pAllocation, 0, size);
    }

    return pAllocation;
}

sysSHARED_CODE void *
lwosMalloc(LwU8 sectionIdx, LwU32 size)
{
    return lwosMallocAligned(sectionIdx, size, lwrtosBYTE_ALIGNMENT);
}

sysSHARED_CODE void *
lwosCalloc(LwU8 sectionIdx, LwU32 size)
{
    return lwosCallocAligned(sectionIdx, size, lwrtosBYTE_ALIGNMENT);
}

void *
lwosMallocResident(LwU32 size)
{
    return lwosMalloc(RESIDENT_HEAP_ID, size);
}

void *
lwosCallocResident(LwU32 size)
{
    return lwosCalloc(RESIDENT_HEAP_ID, size);
}

LwUPtr
lwosOsHeapGetNextFreeByte(void)
{
    return (SectionDescStartVirtual[RESIDENT_HEAP_ID] +
        SectionDescHeapSize[RESIDENT_HEAP_ID]);
}

LwU32
lwosOsHeapGetFreeHeap(void)
{
    return (LwU32)(SectionDescMaxSize[RESIDENT_HEAP_ID] -
        SectionDescHeapSize[RESIDENT_HEAP_ID]);
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
    if (sectionIdx >= UPROC_DATA_SECTION_COUNT)
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    if ((pSizeMax == NULL) || (pSizeUsed == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pSizeMax = SectionDescMaxSize[sectionIdx];
    *pSizeUsed = SectionDescHeapSize[sectionIdx];

    return FLCN_OK;
}
