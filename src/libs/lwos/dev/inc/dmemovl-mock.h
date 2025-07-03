/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_DMEMOVL_MOCK_H
#define LWOS_DMEMOVL_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Configuration data for mocking code in @ref dmemovl.c
 */
typedef struct
{
    LwU8 dummy;
} LWOS_DMEMOVL_MOCK_CONFIG;
/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the LWOS Dmemovl mocking configuration
 */
extern LWOS_DMEMOVL_MOCK_CONFIG LwosDmemovlMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes the state of Dmemovl mocking
 */
void lwosDmemovlMockInit(void);

/*!
 * @brief   Gets the number of calls to @ref lwosHeapAllocationsBlock during
 *          test.
 */
LwLength lwosDmemovlMockHeapAllocationsBlockNumCalls(void);

/*!
 * @brief   Cause subsequent alloc calls to fail
 */
void lwosDmemovlMockSetFailAlloc(LwBool failAlloc);

#endif // LWOS_DMEMOVL_MOCK_H
