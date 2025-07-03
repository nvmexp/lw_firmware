/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_INTF_DEBUG_H
#define LWRISCV_INTF_DEBUG_H

#include <stdint.h>

#define LWRISCV_DEBUG_BUFFER_MAGIC       0xF007BA11U

typedef struct LWRISCV_DEBUG_BUFFER
{
    /* read offset updated by client RM */
    uint32_t readOffset;

    /* write offset updated by firmware RM */
    uint32_t writeOffset;

    /* buffer size configured by client RM */
    uint32_t bufferSize;

    /* magic number for header validation in lwwatch */
    uint32_t magic;
} LWRISCV_DEBUG_BUFFER;

#endif // LWRISCV_INTF_DEBUG_H
