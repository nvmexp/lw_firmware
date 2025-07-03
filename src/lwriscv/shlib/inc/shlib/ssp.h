/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SSP_H
#define SSP_H

extern void *__kernel_stack_chk_guard;
extern void *__task_stack_chk_guard;
extern void *__stack_chk_guard;

LwUPtr getTimeBasedRandomCanary(void);
void __canary_setup(void);

#endif // SSP_H
