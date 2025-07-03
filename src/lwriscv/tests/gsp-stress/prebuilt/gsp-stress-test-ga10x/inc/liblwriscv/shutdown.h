/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_SHUTDOWN_H
#define LIBLWRISCV_SHUTDOWN_H

// Panic, fatal
void riscvPanic(void) __attribute__((noreturn));

// Gentle shutdown (we need to do it properly!)
void riscvShutdown(void) __attribute__((noreturn));

#endif // LIBLWRISCV_SHUTDOWN_H
