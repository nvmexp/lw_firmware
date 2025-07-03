/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libSPDM copyright, as file is LW-authored but uses libSPDM code.

/*!
 * @file   lw_apm_rts.h
 * @brief  This file declates the macros for manipulating the RTS addresses,
 *         as well as functions to read RTS (Root of trust for storage)
 */

#ifndef _LW_APM_RTS_H_
#define _LW_APM_RTS_H_

/* ------------------------- Application Includes --------------------------- */
#include "lib_intfcshahw.h"
#include "apm_rts.h"

/* ------------------------- Prototypes ------------------------------------- */
void apm_rts_offset_initialize(uint64 rts_offset)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_rts_offset_initialize");

uint64 apm_get_rts_offset(void)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_get_rts_offset");

boolean apm_rts_read_msr_group(IN uint32 msr_idx, OUT PAPM_MSR_GROUP dst)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_rts_read_msr_group");

boolean apm_rts_read_msr(IN uint32 msr_idx, OUT PAPM_MSR dst,
    IN boolean read_shadow)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_rts_read_msr");

return_status apm_rts_get_msr_type(IN uint32 msr_idx, OUT uint8 *msr_type)
    GCC_ATTRIB_SECTION("imem_libspdm", "apm_rts_get_msr_type");

#endif //_LW_APM_RTS_H_
