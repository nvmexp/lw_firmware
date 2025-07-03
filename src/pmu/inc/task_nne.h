/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_nne.h
 * @brief  Common header for all task Neural Net Engine (NNE) files.
 *
 * Detailed documentation is located at: [1]
 *
 * @section References
 * @ref "../../../drivers/resman/arch/lwalloc/common/inc/rmpmucmdif.h"
 */
//! [1] https://confluence.lwpu.com/display/RMPER/Neural+Net+Engine+%28NNE%29+Tables+1.0+-+VBIOS+Specification

#ifndef TASK_NNE_H
#define TASK_NNE_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "flcntypes.h"
#include "nne/nne_desc_client.h"
#include "lwrtos.h"
#include "unit_api.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Forward Definitions  -------------------------- */
typedef struct DISPATCH_NNE_NNE_DESC_INFERENCE DISPATCH_NNE_NNE_DESC_INFERENCE;

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Command to run inference on NNE.
 */
struct DISPATCH_NNE_NNE_DESC_INFERENCE
{
    /*!
     * Must be first element of the structure.
     */
    DISP2UNIT_HDR             hdr;

    /*!
     * Specifies the DMEM overlay containing @ref pInference.
     */
    LwU8                      dmemOvl;

    /*!
     * Handle of queue the inference completion notification should be sent to.
     * Separate copy cached outside of NNE_DESC_INFERENCE so that it can be
     * accessed without attaching the DMEM overlay.
     */
    LwrtosQueueHandle         syncQueueHandle;

    /*!
     * Structure holding all inputs/outputs for a PMU client's
     * NNE invocation.
     */
    NNE_DESC_INFERENCE       *pInference;
};

/*!
 * A union of all available commands to NNE.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * @copydoc DISPATCH_NNE_NNE_DESC_INFERENCE
     */
    DISPATCH_NNE_NNE_DESC_INFERENCE   inference;
} DISPATCH_NNE;

/* ------------------------- Defines --------------------------------------- */
#define NNE_EVENT_ID_NNE_DESC_INFERENCE                                       \
    (DISP2UNIT_EVT__COUNT + 0U)

#endif // TASK_NNE_H
