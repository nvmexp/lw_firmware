/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_common_hs.c
 * @brief  Common HS routines for all tasks
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "sec2_gpuarch.h"
#include "common_hslib.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void   _sec2LibCommonHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "start")
    GCC_ATTRIB_USED();

static void   _sec2LibVprPollicyHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libVprPolicyHs", "start")
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
_sec2LibCommonHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/*!
 * @brief Entry function of the VPR Policy HS lib. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_sec2LibVprPollicyHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/* ------------------------- Public Functions ------------------------------- */

