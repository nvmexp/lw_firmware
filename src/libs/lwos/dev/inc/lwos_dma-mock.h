/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_DMA_MOCK_H
#define LWOS_DMA_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"
#include "lwrtos.h"
#include "lwos_dma.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief   Maximum number of  calls to a DMA function during a test
 */
#define DMA_MOCK_MAX_ENTRIES                                                8U

/* ------------------------- Type Definitions ------------------------------ */
/*!
 * @brief   Represents a "faked" region of memory for DMA functions (see
 *          @ref dmaRead_MOCK and @ref dmaWrite_MOCK)
 */
typedef struct DMA_MOCK_MEMORY_REGION DMA_MOCK_MEMORY_REGION;
struct DMA_MOCK_MEMORY_REGION
{
    /*!
     * @brief   The memory descriptor to which this memory region applies
     *
     * @note    Multiple regions can be mocked for a single pMemDesc, but they
     *          must have different, non-overlapping offset/size regions. The
     *          mocking support code does not make any effort to enforce the
     *          disjointness; this should be ensured by the testing code.
     */
    RM_FLCN_MEM_DESC *pMemDesc;

    /*!
     * @brief   Pointer to faked data for this memory region
     */
    void *pData;

    /*!
     * @brief   The offset of pData in pMemDesc's memory region
     */
    LwUPtr offsetOfData;

    /*!
     * @brief   The size of pData's memory region
     */
    LwUPtr sizeOfData;

    /*!
     * @brief   The next in a linked list of all mocked memory regions
     */
    DMA_MOCK_MEMORY_REGION *pNext;
};

/*!
 * @brief   Configuration data for DMA mocking
 */
typedef struct
{
    /*!
     * @brief   Mocking configuration for @ref dmaRead
     */
    struct
    {
        /*!
         * @brief   Error code to return instead of mocking functionality
         */
        FLCN_STATUS errorCodes[DMA_MOCK_MAX_ENTRIES];
    } dmaReadConfig;

    /*!
     * @brief   Mocking configuration for @ref dmaWrite
     */
    struct
    {
        /*!
         * @brief   Error code to return instead of mocking functionality
         */
        FLCN_STATUS errorCodes[DMA_MOCK_MAX_ENTRIES];
    } dmaWriteConfig;

    /*!
     * @brief   Head pointer of list of all mocked memory regions
     *
     * @note    The list can contain multiple mocked regions for
     *          each @ref RM_FLCN_MEM_DESC, but the individual regions within
     *          the overall @ref RM_FLCN_MEM_DESC region must be disjoint to
     *          work properly.
     */
    DMA_MOCK_MEMORY_REGION *pHead;
} DMA_MOCK_CONFIG;

/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the DMA mocking configuration
 */
extern DMA_MOCK_CONFIG DmaMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking configuration for DMA
 */
void dmaMockInit(void);

/*!
 * @brief   Inserts a memory region to be mocked
 *
 * @note    This does *not* enforce disjointness of the @ref
 *          DMA_MOCK_MEMORY_REGION regions as they are inserted
 *
 * @param[in,out]   pRegion The region to insert
 */
void dmaMockConfigMemoryRegionInsert(DMA_MOCK_MEMORY_REGION *pRegion);

/*!
 * @brief   Removes a memory region from being mocked
 *
 * @param[in,out]   pRegion The region to remove
 *
 * @note    This function does nothing if pRegion is not in the configuration
 *          list already.
 */
void dmaMockConfigMemoryRegionRemove(DMA_MOCK_MEMORY_REGION *pRegion);

/*!
 * @brief   Adds a return status for an ordered call to @ref dmaRead
 *
 * @param[in]   entry   Call number for which to use status
 * @param[in]   status  Status to return from call
 */
void dmaMockReadAddEntry(LwU8 entry, FLCN_STATUS status);

/*!
 * @brief   Return the number of calls to @ref dmaRead during the test
 *
 * @return  Number of calls to @ref dmaRead
 */
LwLength dmaMockReadNumCalls(void);

/*!
 * @brief   Adds a return status for an ordered call to @ref dmaWrite
 *
 * @param[in]   entry   Call number for which to use status
 * @param[in]   status  Status to return from call
 */
void dmaMockWriteAddEntry(LwU8 entry, FLCN_STATUS status);

/*!
 * @brief   Return the number of calls to @ref dmaWrite during the test
 *
 * @return  Number of calls to @ref dmaWrite
 */
LwLength dmaMockWriteNumCalls(void);

#endif // LWOS_DMA_MOCK_H
