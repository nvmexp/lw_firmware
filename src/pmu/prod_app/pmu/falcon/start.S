/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file start.S
 * 
 * Hand-written start-section for the PMU application. Responsible for setting
 * the stack-pointer and calling the main function. The version of this routine
 * provided by gcc clobbers general-purpose register a10 before calling the
 * pmuMain function making it impossible to pass command-line arguments argc and
 * argv) to the PMU application.  This version will not clobber either a10
 * (argc) or a11 (argv).
 */

.section .start, "x", @progbits

.global  __stack
.global   _pmuMain
.global   _start

_start:
    mvi   a0  __stack;
    sethi a0  0x0;
    wspr  SP  a0;
    call  _pmuMain;
    halt;

