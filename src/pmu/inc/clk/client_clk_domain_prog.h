/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_DOMAIN_PROG_H
#define CLIENT_CLK_DOMAIN_PROG_H

/*!
 * @file client_clk_domain_prog.h
 * @brief @copydoc clk_domain_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_domain.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Client Clock Domain Programmable structure.
 */
typedef struct
{
    /*!
     * CLIENT_CLK_DOMAIN super class.  
     * Must always be the first element in the structure.
     */
    CLIENT_CLK_DOMAIN                                   super;
    /*!
     * Index into the CLIENT CLK_VF_POINTs for the first CLK_VF_POINT 
     * belonging to this domain
     */
    LwU8                                                vfPointIdxFirst;
    /*!
     * Index into the CLIENT CLK_VF_POINTs for the last CLK_VF_POINT 
     * belonging to this domain
     */
    LwU8                                                vfPointIdxLast;

    /*!
     * Frequency OC offset that is applied on all the VF points
     * of given clock domain
     */
    LW2080_CTRL_CLK_FREQ_DELTA                          freqDelta;

    /*!
     * CPM max Freq Offset override value
     */
    LwU16                                               cpmMaxFreqOffsetOverrideMHz;
} CLIENT_CLK_DOMAIN_PROG;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clientClkDomainGrpIfaceModel10ObjSet_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkDomainGrpIfaceModel10ObjSet_PROG");

// CLIENT_CLK_DOMAIN_PROG interfaces

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLIENT_CLK_VF_POINT_PROG_H
