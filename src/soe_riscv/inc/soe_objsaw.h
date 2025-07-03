/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJSAW_H
#define SOE_OBJSAW_H

/*!
 * @file    soe_objsaw.h 
 * @copydoc soe_objsaw.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern LwU32                 sawBase;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define SAW_REG_RD32(addr)                     bar0Read(sawBase + addr)
#define SAW_REG_WR32(addr, data)               bar0Write(sawBase + addr, data)
/* ------------------------ Public Functions ------------------------------- */

#endif // SOE_OBJSAW_H
