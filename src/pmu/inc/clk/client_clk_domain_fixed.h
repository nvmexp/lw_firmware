/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_DOMAIN_FIXED_H
#define CLIENT_CLK_DOMAIN_FIXED_H

/*!
 * @file client_clk_domain_fixed.h
 * @brief Fixed (non-programmable) client clock domains
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_domain.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @copydoc ClientClkDomainConstruct_SUPER
 */
#define clientClkDomainGrpIfaceModel10ObjSet_FIXED(pModel10, ppBoardObj, size, pBoardObjDesc) \
    clientClkDomainGrpIfaceModel10ObjSet_SUPER((pModel10), (ppBoardObj), (size), (pBoardObjDesc))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Client Clock Domain - Fixed (non programmable) structure.
 */
typedef struct
{
    /*!
     * CLIENT_CLK_DOMAIN super class.  
     * Must always be the first element in the structure.
     */
    CLIENT_CLK_DOMAIN    super;
} CLIENT_CLK_DOMAIN_FIXED;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLIENT_CLK_DOMAIN_FIXED_H
