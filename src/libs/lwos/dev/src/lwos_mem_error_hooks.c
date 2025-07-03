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
 * @file    lwos_mem_error_hooks.c
 * @brief   Hooks for memory errors. Used when ODP is not enabled.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwrtosHooks.h"

/* ------------------------ Public Functions -------------------------------- */
#if !defined(ON_DEMAND_PAGING_BLK) || defined(UTF_UCODE_BUILD)
/*!
 * @brief      Handler for when a IMEM miss oclwrs.
 *
 * @details    No IMEM miss should occur when using a fully resident layout or 
 *             overlays for memory management. As such, halt the Falcon when a
 *             miss oclwrs as the Falcon is in an error state.
 */
void
lwrtosHookImemMiss(void)
{
#ifdef SAFERTOS
    lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32) FLCN_ERR_OUT_OF_RANGE);
#else //SAFERTOS
    OS_HALT();
#endif //SAFERTOS
}

/*!
 * @brief      Handler for when a DMEM miss oclwrs.
 *
 * @details    No DMEM miss should occur when using a fully resident layout or 
 *             overlays for memory management. As such, halt the Falcon when a
 *             miss oclwrs as the Falcon is in an error state.
 */
void
lwrtosHookDmemMiss(void)
{
#ifdef SAFERTOS
    lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32) FLCN_ERR_OUT_OF_RANGE);
#else //SAFERTOS
    OS_HALT();
#endif //SAFERTOS
}
#endif // ON_DEMAND_PAGING_BLK || UTF_UCODE_BUILD
