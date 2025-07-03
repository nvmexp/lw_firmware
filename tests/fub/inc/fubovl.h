/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fubovl.h
 * @brief: function prototypes and placement in overlays are defined in this file
 */

#ifndef _FUBOVL_H_
#define _FUBOVL_H_
/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "fubutils.h"
#include "rmfubif.h"
#include "lwuproc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
int          main (void)                                                  ATTR_OVLY(".text");
void         fubEntryWrapper(void)                                        ATTR_OVLY(".imem_fubinit");
void         fubEntry(void)                                               ATTR_OVLY(".imem_fub");
void         __stack_chk_fail(void)                                       ATTR_OVLY(".imem_fub");
void         fubExceptionHandlerSelwreHang(void)                          ATTR_OVLY(".imem_fub");
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#endif // _FUBOVL_H_
