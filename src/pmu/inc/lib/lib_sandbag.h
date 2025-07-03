/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SANDBAG_H
#define LIB_SANDBAG_H

/*!
 * @file    lib_sandbag.h
 * @copydoc lib_sandbag.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
FLCN_STATUS sandbagInit(void)
    GCC_ATTRIB_SECTION("imem_init", "sandbagInit");
LwBool      sandbagIsRequested(void)
    GCC_ATTRIB_SECTION("imem_resident", "sandbagIsRequested");

#endif // LIB_SANDBAG_H
