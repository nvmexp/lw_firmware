/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 #ifndef _APM_SPDM_COMMON_H_
 #define _APM_SPDM_COMMON_H_

/* ------------------------- Application Includes --------------------------- */
#include "apm/apm_keys.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define APM_IV_TABLE_COUNT            (512)
#define APM_SPDM_COMMON_MUTEX_TIMEOUT (100)

/* ------------------------ Function Prototypes ----------------------------- */
void apm_spdm_shared_pre_init(void)
    GCC_ATTRIB_SECTION("imem_init", "apm_spdm_shared_pre_init");

//
// NOTE: The following functions will load and attach the apmSpdmShared DMEM
// overlay. Ensure that your task will have enough space for this overlay
// before calling these functions, otherwise overlay load will halt.
// The apmSpdmShared DMEM overlay will be detached on function exit.
//
FLCN_STATUS apm_spdm_shared_write_data_key(LwU8 *pDataKey, LwU32 mutexTimeout)
    GCC_ATTRIB_SECTION("imem_apmSpdmShared", "apm_spdm_shared_write_data_key");

FLCN_STATUS apm_spdm_shared_read_data_key(LwU8 *pDataKeyOut, LwU32 mutexTimeout)
    GCC_ATTRIB_SECTION("imem_apmSpdmShared", "apm_spdm_shared_read_data_key");

FLCN_STATUS apm_spdm_shared_write_iv_table(LwU8 *pIv, LwU32 ivSlot, LwU32  mutexTimeout)
    GCC_ATTRIB_SECTION("imem_apmSpdmShared", "apm_spdm_shared_write_iv_table");

FLCN_STATUS apm_spdm_shared_read_iv_table(LwU8 *pIvOut, LwU32 ivSlot, LwU32  mutexTimeout)
    GCC_ATTRIB_SECTION("imem_apmSpdmShared", "apm_spdm_shared_read_iv_table");

FLCN_STATUS apm_spdm_shared_clear(LwU32  mutexTimeout)
    GCC_ATTRIB_SECTION("imem_apmSpdmShared", "apm_spdm_shared_clear");

 #endif // _APM_SPDM_COMMON_H_
