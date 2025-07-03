/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwos_dma-mock.c
 * @brief  Mock implementations of the Falcon DMA routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwos_dma.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwos_ovl_priv.h"
#include "ut_assert.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwos_dma-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global DMA mocking configuration private to the mocking
 */
typedef struct
{
    /*!
     * @brief   Mocking configuration for @ref dmaRead
     */
    struct
    {
        /*!
         * @brief   Number of calls during test
         */
        LwLength numCalls;
    } dmaReadConfig;

    /*!
     * @brief   Mocking configuration for @ref dmaWrite
     */
    struct
    {
        /*!
         * @brief   Number of calls during test
         */
        LwLength numCalls;
    } dmaWriteConfig;
} DMA_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
DMA_MOCK_CONFIG DmaMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of private mocking configuration
 */
static DMA_MOCK_CONFIG_PRIVATE DmaMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief Finds the appropriate pointer for a mocked memory region
 *
 * @param[in]       pMemDesc    The memory descriptor for the DMA
 * @param[in]       offset      The offset into pMemDesc's memory region
 * @param[in]       numBytes    The number of bytes to be used for the transfer
 *
 * @return  Mock DMEM pointer representing data offset bytes into pMemDesc's
 *          region, if valid
 * @return  NULL if no mocked memory region available for combination of
 *          pMemDesc/offset/numBytes
 */
static void *s_dmaMockConfigMemoryRegionFindData(
    RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes);

/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
dmaRead_MOCK
(
    void *pBuf,
    RM_FLCN_MEM_DESC *pMemDesc,
    LwU32 offset,
    LwU32 numBytes
)
{
    void *pData;
    const LwLength lwrrentCall = DmaMockConfigPrivate.dmaReadConfig.numCalls;
    if (lwrrentCall == DMA_MOCK_MAX_ENTRIES)
    {
        UT_FAIL("DMA mocking cannot support enough calls for current test.");
    }
    DmaMockConfigPrivate.dmaReadConfig.numCalls++;

    // Check if we have an error code override
    if (DmaMockConfig.dmaReadConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return DmaMockConfig.dmaReadConfig.errorCodes[lwrrentCall];
    }

    pData = s_dmaMockConfigMemoryRegionFindData(pMemDesc, offset, numBytes);
    if (pData == NULL)
    {
        UT_FAIL("dmaRead_MOCK called without setting up an appropriate mocked memory region.\n");
        return FLCN_ERROR;
    }

    (void)memcpy(pBuf, pData, numBytes);
    return FLCN_OK;
}

FLCN_STATUS
dmaWrite_MOCK
(
    void *pBuf,
    RM_FLCN_MEM_DESC *pMemDesc,
    LwU32 offset,
    LwU32 numBytes
)
{
    void *pData;
    const LwLength lwrrentCall = DmaMockConfigPrivate.dmaWriteConfig.numCalls;
    if (lwrrentCall == DMA_MOCK_MAX_ENTRIES)
    {
        UT_FAIL("DMA mocking cannot support enough calls for current test.");
    }
    DmaMockConfigPrivate.dmaWriteConfig.numCalls++;

    // Check if we have an error code override
    if (DmaMockConfig.dmaWriteConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return DmaMockConfig.dmaWriteConfig.errorCodes[lwrrentCall];
    }

    pData = s_dmaMockConfigMemoryRegionFindData(pMemDesc, offset, numBytes);
    if (pData == NULL)
    {
        return FLCN_ERROR;
    }

    (void)memcpy(pData, pBuf, numBytes);
    return FLCN_OK;
}

void
lwosDmaLockAcquire_MOCK(void)
{
}

void
lwosDmaLockRelease_MOCK(void)
{
}

void
dmaMockInit(void)
{
    LwLength i;

    // No dynamic allocation in UTF, so no way that we're leaking memory here.
    DmaMockConfig.pHead = NULL;

    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(DmaMockConfig.dmaReadConfig.errorCodes);
         i++)
    {
        DmaMockConfig.dmaReadConfig.errorCodes[i] = FLCN_OK;
    }
    DmaMockConfigPrivate.dmaReadConfig.numCalls = 0U;

    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(DmaMockConfig.dmaWriteConfig.errorCodes);
         i++)
    {
        DmaMockConfig.dmaWriteConfig.errorCodes[i] = FLCN_OK;
    }
    DmaMockConfigPrivate.dmaWriteConfig.numCalls = 0U;
}

void
dmaMockConfigMemoryRegionInsert
(
    DMA_MOCK_MEMORY_REGION *pRegion
)
{
    pRegion->pNext = DmaMockConfig.pHead;
    DmaMockConfig.pHead = pRegion;
}

void
dmaMockConfigMemoryRegionRemove
(
    DMA_MOCK_MEMORY_REGION *pRegion
)
{
    DMA_MOCK_MEMORY_REGION **ppPrev;
    DMA_MOCK_MEMORY_REGION *pLwrr;
    for (ppPrev = &DmaMockConfig.pHead, pLwrr = DmaMockConfig.pHead;
         pLwrr != NULL;
         ppPrev = &pLwrr->pNext, pLwrr = pLwrr->pNext)
    {
        if (pLwrr == pRegion)
        {
            *ppPrev = pLwrr->pNext;
        }
    }
}

void
dmaMockReadAddEntry
(
    LwU8 entry,
    FLCN_STATUS status
)
{
    UT_ASSERT_TRUE(entry < DMA_MOCK_MAX_ENTRIES);
    DmaMockConfig.dmaReadConfig.errorCodes[entry] = status;
}

LwLength
dmaMockReadNumCalls(void)
{
    return DmaMockConfigPrivate.dmaReadConfig.numCalls;
}

void
dmaMockWriteAddEntry
(
    LwU8 entry,
    FLCN_STATUS status
)
{
    UT_ASSERT_TRUE(entry < DMA_MOCK_MAX_ENTRIES);
    DmaMockConfig.dmaWriteConfig.errorCodes[entry] = status;
}

LwLength
dmaMockWriteNumCalls(void)
{
    return DmaMockConfigPrivate.dmaWriteConfig.numCalls;
}

/* ------------------------ Static Functions  ------------------------------- */
static void *
s_dmaMockConfigMemoryRegionFindData
(
    RM_FLCN_MEM_DESC *pMemDesc,
    LwU32 offset,
    LwU32 numBytes
)
{
    DMA_MOCK_MEMORY_REGION *pLwrr;
    for (pLwrr = DmaMockConfig.pHead;
         pLwrr != NULL;
         pLwrr = pLwrr->pNext)
    {
        if ((pLwrr->pMemDesc == pMemDesc) &&
            (offset >= pLwrr->offsetOfData) &&
            (offset < pLwrr->offsetOfData + pLwrr->sizeOfData) &&
            (offset + numBytes <= pLwrr->offsetOfData + pLwrr->sizeOfData))
        {
            return ((LwU8 *)pLwrr->pData) + offset - pLwrr->offsetOfData;
        }
    }

    return NULL;
}
