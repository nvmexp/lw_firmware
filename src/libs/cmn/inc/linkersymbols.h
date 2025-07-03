/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LINKERSYMBOLS_H
#define LINKERSYMBOLS_H

/*!
 * @file    linkersymbols.h
 * @brief   External declarations for linker-exported symbols
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwtypes.h"
/* ------------------------ External Definitions ---------------------------- */
#ifdef DMEM_VA_SUPPORTED
extern LwU32 _dmem_va_bound[];
#endif // DMEM_VA_SUPPORTED

/*!
 * Address of the end of the resident DMEM.
 */
extern LwU32 _end_physical_dmem[];

/*!
 * ISR and ESR stack addresses of the end of the resident DMEM.
 */
extern LwUPtr _isr_stack_start[];
extern LwUPtr _isr_stack_end[]; 
extern LwUPtr _esr_stack_start[];
extern LwUPtr _esr_stack_end[];

#endif // LINKERSYMBOLS_H

