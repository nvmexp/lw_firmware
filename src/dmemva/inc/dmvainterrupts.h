/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_INTERRUPTS_H
#define DMVA_INTERRUPTS_H

void installInterruptHandler(void (*func)(void));
void installExceptionHandler(void (*func)(void));

void defaultInterruptHandler(void);
void defaultExceptionHandler(void);

#endif
