/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dmemovl-mock.c
 * @brief  Mock implementations of the DMEM overlay routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "dmemovl-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global dispatch mocking configuration private to the mocking
 */
typedef struct 
{
    /*!
     * @brief   Private configuration data for @ref lwosHeapAllocationsBlock
     */
    struct
    {
        /*!
         * @brief   Number of calls to @ref lwosHeapAllocationsBlock
         */
        LwLength numCalls;
    } lwosHeapAllocationsBlockConfig;

    /*!
    * @brief   when this flag is set, allocations will fail.
    */
    LwBool failAlloc;
} LWOS_DMEMOVL_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
LWOS_DMEMOVL_MOCK_CONFIG LwosDmemovlMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of the private LWOS Dmemovl mocking configuration
 */
static LWOS_DMEMOVL_MOCK_CONFIG_PRIVATE LwosDmemovlMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
lwosHeapAllocationsBlock_MOCK(void)
{
    LwosDmemovlMockConfigPrivate.lwosHeapAllocationsBlockConfig.numCalls++;
}

void *
lwosCallocResident_MOCK(LwU32 size)
{
    if(!LwosDmemovlMockConfigPrivate.failAlloc)
    {
        return lwosCallocResident_IMPL(size);
    }
    else
    {
        return NULL;
    }
}

void
lwosDmemovlMockInit(void)
{
    LwosDmemovlMockConfigPrivate.lwosHeapAllocationsBlockConfig.numCalls = 0U;
}

LwLength
lwosDmemovlMockHeapAllocationsBlockNumCalls(void)
{
    return LwosDmemovlMockConfigPrivate.lwosHeapAllocationsBlockConfig.numCalls;
}

void
lwosDmemovlMockSetFailAlloc(LwBool failAlloc)
{
    LwosDmemovlMockConfigPrivate.failAlloc = failAlloc;
}


/* ------------------------- Static Functions  ------------------------------ */
