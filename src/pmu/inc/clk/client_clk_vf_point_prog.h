/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_VF_POINT_PROG_H
#define CLIENT_CLK_VF_POINT_PROG_H

/*!
 * @file client_clk_vf_point_prog.h
 * @brief @copydoc client_clk_vf_point_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_vf_point.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @copydoc ClientClkVfPointProg
 */
#define clientClkVfPointGrpIfaceModel10ObjSet_PROG(pModel10, ppBoardObj, size, pBoardObjDesc) \
    clientClkVfPointGrpIfaceModel10ObjSet_SUPER((pModel10), (ppBoardObj), (size), (pBoardObjDesc))

/*!
 * @copydoc ClientClkVfPointProg
 */
#define clientClkVfPointIfaceModel10GetStatus_PROG(pModel10, pPayload)                       \
    clientClkVfPointIfaceModel10GetStatus_SUPER((pModel10), (pPayload))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Client Clock VF Point Prog structure.
 */
typedef struct
{
    /*!
     * CLIENT_CLK_VF_POINT super class.  Must always be the first element in the structure.
     */
    CLIENT_CLK_VF_POINT    super;
} CLIENT_CLK_VF_POINT_PROG;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_vf_point_prog_20.h"

#endif // CLIENT_CLK_VF_POINT_PROG_H_
