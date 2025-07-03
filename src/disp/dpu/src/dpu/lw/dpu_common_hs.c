/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_common_hs.c
 * @brief  Common HS routines for all tasks
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dpu_gpuarch.h"
#include "common_hslib.h"

/* ------------------------- Application Includes --------------------------- */
#include "dpu_objdpu.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void   _dpuLibCommonHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "start")
    GCC_ATTRIB_USED();

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Entry function of the HS common lib. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_dpuLibCommonHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/* ------------------------- Public Functions ------------------------------- */

