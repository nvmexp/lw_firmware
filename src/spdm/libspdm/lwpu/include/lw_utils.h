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
 * @file   lw_utils.h
 * @brief  This file declates various utility macros and functions
 */

#ifndef _LW_UTILS_H_
#define _LW_UTILS_H_

/* ------------------------- Macros and Defines ----------------------------- */
#define FLCN_STATUS_TO_BOOLEAN(status) ((status) == FLCN_OK ? TRUE : FALSE)

// Forked from CHECK_FLCN_STATUS
#define CHECK_SPDM_STATUS(expr)           \
    do {                                  \
        spdmStatus = (expr);              \
        if (spdmStatus != RETURN_SUCCESS) \
        {                                 \
            goto ErrorExit;               \
        }                                 \
    } while (LW_FALSE)

/* --------------------------- Function Prototypes -------------------------- */
boolean change_endianness(OUT uint8 *dst_buffer,
    IN const uint8 *src_buffer, IN uintn buffer_size)
    GCC_ATTRIB_SECTION("imem_libspdm", "change_endianness");

#endif // _LW_UTILS_H_
