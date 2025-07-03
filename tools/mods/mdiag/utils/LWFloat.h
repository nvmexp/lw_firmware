/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _LWFLOAT_H_
#define _LWFLOAT_H_

#define SSE_ROUND_NEAREST 0x0000
#define SSE_ROUND_DOWN    0x2000
#define SSE_ROUND_UP      0x4000
#define SSE_ROUND_ZERO    0x6000
#define SSE_ROUND_MASK    0x6000

#define FP_ROUND_NEAREST 0x0000
#define FP_ROUND_DOWN    0x0400
#define FP_ROUND_UP      0x0800
#define FP_ROUND_ZERO    0x0c00
#define FP_ROUND_MASK    0x0c00

static void init_lwfloat() {
    unsigned int fpcntrl = Cpu::GetFpControlWord();
    fpcntrl = (fpcntrl & ~FP_ROUND_MASK) | FP_ROUND_ZERO;
    Cpu::SetFpControlWord(fpcntrl);

    unsigned int ssecntrl = Cpu::GetMXCSRControlWord();
    ssecntrl = (ssecntrl & ~SSE_ROUND_MASK) | SSE_ROUND_ZERO;
    Cpu::SetMXCSRControlWord(ssecntrl);
}

#endif // _LWFLOAT_H_
