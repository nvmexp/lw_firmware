/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_DOMAIN_H
#define CLIENT_CLK_DOMAIN_H

/*!
 * @file client_clk_domain.h
 * @brief @copydoc client_clk_domain.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLIENT_CLK_DOMAINS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_CLK_DOMAIN                         \
    (&Clk.clientClkDomains.super.super)

/*!
 * Macro to get BOARDOBJ pointer from CLIENT_CLK_DOMAINS BOARDOBJGRP.
 *
 * @param[in]  idx         CLIENT_CLK_DOMAIN index.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLIENT_CLK_DOMAIN_GET(idx)                                          \
    (BOARDOBJGRP_OBJ_GET(CLIENT_CLK_DOMAIN, (_idx)))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Client clock domain structure
 * 
 * Information about a CLIENT_CLK_DOMAIN as exported to end-user
 * APIs for OC and other types of frequency control
 * 
 */
typedef struct
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ        super;

    /*!
     * Index of corresponding CLK_DOMAIN.
     * 
     * @note This is for RM internal use only. Do not export this value to 
     * end-user facing RMCTRLs!
     */
    LwU8            domainIdx;
} CLIENT_CLK_DOMAIN;

/*!
 * The set of CLIENT_CLK_DOMAINs which will be exported to end-users.
 * 
 * Implements BOARDOBJGRP_E32.
 * 
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class representing the set of all client
     * clock domains. 
     * Must always be first element in the structure.
     */
    BOARDOBJGRP_E32 super;
} CLIENT_CLK_DOMAINS;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clientClkDomainBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkDomainBoardObjGrpIfaceModel10Set");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clientClkDomainGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkDomainGrpIfaceModel10ObjSet_SUPER");

// CLIENT_CLK_DOMAIN interfaces

// CLIENT_CLK_DOMAINS interfaces

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_domain_fixed.h"
#include "clk/client_clk_domain_prog.h"

#endif // CLIENT_CLK_DOMAIN_H
