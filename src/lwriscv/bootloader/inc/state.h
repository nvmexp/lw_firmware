/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef STATE_H
#define STATE_H

#include "debug.h"
#include "elf.h"

typedef struct
{
    LW_ELF_STATE elfState;
    LW_DBG_STATE dbgState;
} LW_BOOTLOADER_STATE;

#define GET_STATE() ({LW_BOOTLOADER_STATE *state; asm __volatile__("add %0, zero, tp":"=r"(state) : :); state;})
#define SET_STATE(X) ({ asm __volatile__("add tp, zero, %0": :"r"(X) : "tp"); })

#define GET_ELF_STATE() (&(GET_STATE()->elfState))
#define GET_DBG_STATE() (&(GET_STATE()->dbgState))

#endif // STATE_H
