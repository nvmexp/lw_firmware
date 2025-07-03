/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvainterrupts.h"
#include "dmvaregs.h"
#include "dmvautils.h"
#include "dmvacommon.h"
#include <lwtypes.h>
#include <lwmisc.h>

void defaultInterruptHandler(void)
{
    DMVAREGWR(IRQSCLR, FLD_SET_DRF(_PFALCON, _FALCON_IRQSCLR, _SWGEN0, _SET, 0));
}

void defaultExceptionHandler(void)
{
    DMVAREGWR(MAILBOX0, DMVA_UNEXPECTED_EXCEPTION);
    DMVAREGWR(MAILBOX1, DMVAREGRD(EXCI));
    halt();
}

static void (*pInterruptHandler)(void);
void interruptEntry(void);

asm("_interruptEntry:"
    "pushm a15;"
    "rspr a9 CSW;"
    "push a9;"
    "mvi a9 _pInterruptHandler;"
    "ldd.w a9 a9;"
    "call a9;"
    "pop a9;"
    "wspr CSW a9;"
    "mvi a9 0x18;"
    "sclrb a9;"
    "popm a15 0x0;"
    "reti;");

static void (*pExceptionHandler)(void);
void exceptionEntry(void);

asm("_exceptionEntry:"
    "pushm a15;"
    "rspr a9 CSW;"
    "push a9;"
    "mvi a9 _pExceptionHandler;"
    "ldd.w a9 a9;"
    "call a9;"
    "pop a9;"
    "wspr CSW a9;"
    "mvi a9 0x18;"
    "sclrb a9;"
    "popm a15 0x0;"
    "reti;");

void installExceptionHandler(void (*func)(void))
{
    falc_sclrb_i(24);
    pExceptionHandler = func;
    falc_wspr(EV, exceptionEntry);
}

void installInterruptHandler(void (*func)(void))
{
    pInterruptHandler = func;
    falc_wspr(IV0, interruptEntry);

    LwU32 irqDest = DMVAREGRD(IRQDEST);
    irqDest = FLD_SET_DRF(_PFALCON, _FALCON_IRQDEST, _HOST_SWGEN0, _FALCON, irqDest);
    irqDest = FLD_SET_DRF(_PFALCON, _FALCON_IRQDEST, _TARGET_SWGEN0, _FALCON_IRQ0, irqDest);
    DMVAREGWR(IRQDEST, irqDest);

    LwU32 irqMode = DMVAREGRD(IRQMODE);
    irqMode = FLD_SET_DRF(_PFALCON, _FALCON_IRQMODE, _LVL_SWGEN0, _FALSE, irqMode);
    DMVAREGWR(IRQMODE, irqMode);

    DMVAREGWR(IRQMSET, FLD_SET_DRF(_PFALCON, _FALCON_IRQMSET, _SWGEN0, _SET, 0));

    falc_ssetb_i(16);
}
