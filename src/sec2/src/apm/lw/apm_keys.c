/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file   apm_keys.c
 * @brief  Global variables defining keys and IVs for APM.
 *         APM-TODO CONFCOMP-377: Remove these keys once no longer needed.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "apm/apm_keys.h"

/* ------------------------- Global Variables ------------------------------- */
//
// Struct which (in)directly stores APM encryption and decryption IVs.
// Will be valid upon channel context load. Data is resident to ensure
// task_chnmgmt can access it upon channel switch.
//
SELWRE_CHANNEL_IV_CTX g_selwreIvCtx
    GCC_ATTRIB_ALIGN(APM_IV_ALIGNMENT);

// APM-TODO CONFCOMP-377: Remove the below hardcoded values.
const LwU8 g_apmSymmetricKey[APM_KEY_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmSymmetricKey")
    GCC_ATTRIB_ALIGN(APM_KEY_ALIGNMENT) =
{
    0xB5, 0xCC, 0xA0, 0x41, 0x4B, 0x6C, 0x67, 0xC3,
    0xBE, 0x36, 0xF4, 0xFC, 0x13, 0xC5, 0x3E, 0x85,
}; 

const LwU8 g_apmIV[APM_IV_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("dmem_apm", "g_apmIV")
    GCC_ATTRIB_ALIGN(APM_IV_ALIGNMENT) =
{
    0x00, 0x00, 0x00, 0x00, 0x03, 0x25, 0x07, 0xCB,
    0xFA, 0x22, 0x2D, 0xFC, 0x0F, 0xA1, 0xBA, 0xDD,
}; 

