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
 * @file   lw_cc_rts.h
 * @brief  This file declates the macros for manipulating the RTS addresses,
 *         as well as functions to read RTS (Root of trust for storage)
 */

#ifndef _LW_CC_RTS_H_
#define _LW_CC_RTS_H_

/* ------------------------- Application Includes --------------------------- */

/* ------------------------- Prototypes ------------------------------------- */

boolean
cc_rts_read_msr
(
    IN uint8   msr_idx,
    OUT void  *pdst,
    IN boolean read_shadow
) GCC_ATTRIB_SECTION("imem_libspdm", "cc_rts_read_msr");

return_status
cc_rts_get_msr_type
(
    IN  uint8 msr_idx,
    OUT uint8 *msr_type
) GCC_ATTRIB_SECTION("imem_libspdm", "cc_rts_get_msr_type");
#endif //_LW_CC_RTS_H_

