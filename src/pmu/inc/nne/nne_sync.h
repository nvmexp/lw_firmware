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
 * @file    nne_sync.h
 *
 * @brief   Declares a data structure for synchronously waiting for inference
 *          completion.
 */

#ifndef NNE_SYNC_H
#define NNE_SYNC_H

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "lwrtos.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * @brief   Parameters to pass to triggering function
 */
typedef struct
{
    /*!
     * @brief   Pointer to location at which to store how long the inference
     *          took.
     */
    LwU64_ALIGN32 *pSyncTimeNs;
} NNE_SYNC_TRIGGER_PARAMS;

/*!
 * @brief   Structure used for synchronizing waits on completion of inferences.
 */
typedef struct
{
    /*!
     * @brief   Whether the task still needs to wait for a completion signal.
     */
    LwBool                      bWaitForSignal;

    /*!
     * @brief   Actual RTOS semaphore used to signal the waiting task.
     */
    LwrtosSemaphoreHandle       semaphore;

    /*!
     * @brief   Cached pointer to the params used to trigger the current
     *          inference.
     *
     * @note    Only truly valid while @ref NNE_SYNC::bWaitForSignal is true.
     */
    NNE_SYNC_TRIGGER_PARAMS    *pParams;
} NNE_SYNC;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * @brief   Initialize the @ref NNE_SYNC structure
 *
 * @param[out]  pSync       @ref NNE_SYNC data structure to initialize
 * @param[in]   ovlIdxDmem  Ovleray index in which to allocate memory
 *
 * @return  FLCN_OK     On success
 * @return  Others      On error
 */
FLCN_STATUS nneSyncInit(NNE_SYNC *pSync, LwU8 ovlIdxDmem)
    GCC_ATTRIB_SECTION("imem_init", "nneSyncInit");

/*!
 * @brief   Trigger the inference and wait for it to complete.

 * @param[in,out]  pSync   @ref NNE_SYNC data structure on which to wait
 */
FLCN_STATUS nneSyncTriggerAndWait(NNE_SYNC *pSync, NNE_SYNC_TRIGGER_PARAMS *pParams, LwUPtr timeout)
    GCC_ATTRIB_SECTION("imem_nne", "nneSyncTriggerAndWait");

/*!
 * @brief   Signal the completion of the inference.
 */
void nneSyncSignal(NNE_SYNC *pSync)
    GCC_ATTRIB_SECTION("imem_nne", "nneSyncSignal");

/*!
 * @copydoc nneSyncSignal
 *
 * @note    Safe to call from an ISR.
 */
void nneSyncSignalFromISR(NNE_SYNC *pSync)
    GCC_ATTRIB_SECTION("imem_resident", "nneSyncSignalFromISR");

#endif // NNE_SYNC_H
