/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTSTUB_DEBUG_H
#define BOOTSTUB_DEBUG_H

#include <riscvifriscv.h>
#include <lwriscv/print.h>
#include <lwtypes.h>

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
 *
 * @param[out] state    A pointer to a persistent memory location for storing
 *                      the internal debug state.
 */
void dbgInit(LW_DBG_STATE *state);

/*!
 * @brief Disables debug system.
 */
void dbgDisable(void);

#else // if !LWRISCV_DEBUG_PRINT_ENABLED
# define dbgInit(state) ((void)(state))
# define dbgDisable()
#endif // if LWRISCV_DEBUG_PRINT_ENABLED

#endif // BOOTSTUB_DEBUG_H
