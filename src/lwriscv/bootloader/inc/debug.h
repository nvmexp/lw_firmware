/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTLOADER_DEBUG_H
#define BOOTLOADER_DEBUG_H

#include <riscvifriscv.h>
#include <lwriscv/print.h>
#include "bootloader.h"

/*!
 * @file   debug.h
 * @brief  Basic debugging features.
 */

typedef struct
{
    LwBool                 enabled;
    LwU8                  *data;
    LW_RISCV_DEBUG_BUFFER *metadata;
} LW_DBG_STATE;

/*!
 * @brief Aborts exelwtion. Will not return.
 *
 * @param[in] code  Return code.
 *
 * @return Does not return.
 */
void dbgExit(LwU64 code)
    __attribute__((noreturn));

#if LWRISCV_DEBUG_PRINT_ENABLED
/*!
 * @brief Initializes debug system, resetting pointers.
 */
void dbgInit(void);

/*!
 * @brief Disables debug system.
 */
void dbgDisable(void);

#else // if !LWRISCV_DEBUG_PRINT_ENABLED
# define dbgInit()
# define dbgDisable()
#endif // if LWRISCV_DEBUG_PRINT_ENABLED

#endif // BOOTLOADER_DEBUG_H
