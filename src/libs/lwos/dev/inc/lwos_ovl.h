/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_H
#define LWOS_OVL_H

/*!
 * @file    lwos_ovl.h
 * @copydoc lwos_ovl_cmn.c
 * @copydoc lwos_ovl_imem.c
 * @copydoc lwos_ovl_dmem.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwos_ovl_imem.h"
#if !defined(PMU_RTOS)
#include "lwos_ovl_imem_hs.h"
#endif
#include "lwos_ovl_dmem.h"
#include "lwos_ovl_desc.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
LwBool  lwosTaskOverlayAttach       (LwrtosTaskHandle pxTask, LwU8 ovlIdx, LwBool bDmem, LwBool bHS);
void    lwosTaskOverlayAttachAndFail(LwrtosTaskHandle pxTask, LwU8 ovlIdx, LwBool bDmem, LwBool bHS);
LwBool  lwosTaskOverlayDetach       (LwrtosTaskHandle pxTask, LwU8 ovlIdx, LwBool bDmem, LwBool bHS);
void    lwosTaskOverlayDetachAndFail(LwrtosTaskHandle pxTask, LwU8 ovlIdx, LwBool bDmem, LwBool bHS);

#endif // LWOS_OVL_H
