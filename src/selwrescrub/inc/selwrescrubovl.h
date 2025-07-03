/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubovl.h
 * @brief: function prototypes and placement in overlays are defined in this
 *          file
 */

#ifndef SELWRESCRUBOVL_H
#define SELWRESCRUBOVL_H
/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "rmselwrescrubif.h"
#include "selwrescrubutils.h"
#include "selwrescrubtypes.h"
#include "rmselwrescrubif.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Overlays
#define NONSELWRE_OVL                                       ".text"
#define SELWRE_OVL_ENTRY                                    ".imem_selwrescrubinit"
#define SELWRE_OVL                                          ".imem_selwrescrub"
/* ------------------------- Prototypes ------------------------------------- */
void               start(void)                                                                 ATTR_OVLY(".start");
int                main (void)                                                                 ATTR_OVLY(NONSELWRE_OVL);
void               selwrescrubEntryWrapper(void)                                               ATTR_OVLY(SELWRE_OVL_ENTRY);
void               selwrescrubExceptionHandlerSelwreHang(void)                                 ATTR_OVLY(SELWRE_OVL);
void               selwrescrubEntry(void)                                                      ATTR_OVLY(SELWRE_OVL);
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#endif // SELWRESCRUBOVL_H

