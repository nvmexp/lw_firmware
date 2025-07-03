/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  os.c
 * @brief Top-Level interfaces between the GSP application layer and the RTOS/libs.
 */

/* ------------------------- System Includes ------------------------------- */
#include "gspsw.h"
#include "flcnifcmn.h"
#include "gspifcmn.h"
#include "config/g_lsf_hal.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Callback function called by the RTOS to check whether an access using the
 * specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     bubble up the return value from @ref lsfIsDmaIdxPermitted_HAL
 */
sysSYSLIB_CODE LwBool
vApplicationIsDmaIdxPermitted
(
    LwU8 dmaIdx
)
{
    return lsfIsDmaIdxPermitted_HAL(&LsfHal, dmaIdx);
}

/*!
 * @brief colwert the engine-specific RM_GSP_DMAIDX_XYZ to generic RM_FLCN_DMAIDX_XYZ
 *
 * @param[in]      engDmaIdx      RM_GSP_DMAIDX_XYZ to be oclwerted
 * @param[in/out]  pFlcnDmaIndex  The pointer to the colwerted RM_FLCN_DMAIDX_XYZ
 *
 * @return FLCN_OK if the DMAIDX colwersion succeeds.
 * FLCN_ERR_ILWALID_ARGUMENT if any input pointer is NULL or if there is no proper
 * DMAIDX to be colwerted.
 */
sysSYSLIB_CODE FLCN_STATUS
colwertEngDmaIdxToGenericDmaIdx
(
    LwU8             engDmaIdx,
    PRM_FLCN_DMAIDX  pFlcnDmaIndex
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pFlcnDmaIndex == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (engDmaIdx)
    {
        case RM_GSP_DMAIDX_PHYS_VID_FN0:
            *pFlcnDmaIndex = RM_FLCN_DMAIDX_PHYS_VID_FN0;
            break;

        case RM_GSP_DMAIDX_PHYS_SYS_COH_FN0:
            *pFlcnDmaIndex = RM_FLCN_DMAIDX_PHYS_SYS_COH_FN0;
            break;

        case RM_GSP_DMAIDX_PHYS_SYS_NCOH_FN0:
            *pFlcnDmaIndex = RM_FLCN_DMAIDX_PHYS_SYS_NCOH_FN0;
            break;

        case RM_GSP_DMAIDX_VIRT:
            *pFlcnDmaIndex = RM_FLCN_DMAIDX_VIRT;
            break;

        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    return status;
}
