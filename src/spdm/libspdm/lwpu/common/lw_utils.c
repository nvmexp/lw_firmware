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
 * @brief  This file defines various utility functions for LWPU extensions
 *         of libspdm code, that are not specific to a given ucode.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwtypes.h"
/* ------------------------- Application Includes --------------------------- */
#include "lw_utils.h"

/* ------------------------- Public Functions ------------------------------- */
/**
  Changes the endianness of the buffer.

  @param  dst_buffer   Buffer storing the data to be reversed.
  @param  src_buffer   Destination buffer.
  @param  buffer_size  The size of buffers.

  @return boolean  TRUE if successful, FALSE otherwise.
 */
boolean
change_endianness
(
    OUT uint8       *dst_buffer,
    IN  const uint8 *src_buffer,
    IN  uintn        buffer_size
)
{
    LwU32 idx  = 0;
    LwU32 half = 0;

    if (src_buffer == NULL || dst_buffer == NULL || buffer_size == 0)
    {
        return FALSE;
    }

    //
    // We iterate over half the buffer, swapping with other half.
    // Ensure we copy over middle as well, in case of odd length.
    //
    half = (buffer_size + 1)/2;
    for (idx = 0; idx < half; idx++)
    {
        LwU8 temp = src_buffer[idx];
        dst_buffer[idx] = src_buffer[buffer_size - idx - 1];
        dst_buffer[buffer_size - idx - 1] = temp;
    }

    return TRUE;
}
