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

// Included libspdm copyright, as file is LW-authored but uses libspdm code.

/*!
 * @file   lw_sec2_rtos.h
 * @brief  General header for functions or structs used to bridge SEC2-RTOS
 *         and SEC2 libspdm implementation. This includes any common shared
 *         structs, or functions required for SEC2-RTOS to initialize
 *         libspdm on SEC2 module.
 */

#ifndef _LW_SEC2_RTOS_H_
#define _LW_SEC2_RTOS_H_

/* --------------------------- System Includes ------------------------------ */
#include "base.h"

/* ------------------------ Prototypes ------------------------------------- */
return_status spdm_sec2_initialize(void *pContext, LwU8 *pPayload,
    LwU32 payloadSize, LwU64 rtsOffset)
    GCC_ATTRIB_SECTION("imem_libspdm", "spdm_sec2_initialize");

#endif // _LW_SEC2_RTOS_H_
