/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file start.S
 * 
 * Hand-written start-section for the SOE boot loader. Responsible for setting
 * the stack-pointer and calling the entry function in SOE application. The version
 * of this routine provided by gcc clobbers general-purpose register a10 before
 * calling the main function making it impossible to pass command-line arguments (argc
 * and argv) to the SOE boot loader.  This version will not clobber either a10
 * (argc) or a11 (argv).
 */

.section .start, "x", @progbits

.global  __stack
.global   _main
.global   _start

_start:
    mvi   a0  __stack;
    wspr  SP  a0;
    call  _main;
    halt;
