/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PR_MTHDS_H
#define PR_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwuproc.h"
#include "sec2_chnmgmt.h"
#include "class/cl95a1.h"
#include "tsec_drv.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief The SEC2 PLAYREADY short-hand notation for the LW95A1 PLAYREADY class method.
 *
 * @param[in]   mthd    SEC2 PLAYREADY short-hand notation.
 *
 * @return  LW95A1 PLAYREADY class method.
 */
#define PR_METHOD_ID(mthd)                                 LW95A1_PR_##mthd

/*!
 * @brief Colwerts an index into appMthdArray into the LW95A1 PLAYREADY method value.
 *
 * @param[in]   idx     Index into the appMthdArray.
 *
 * @return  LW95A1 PLAYREADY class method.
 */
#define PR_GET_METHOD_ID(idx) \
    (PR_METHOD_ID(FUNCTION_ID) + ((idx) * 4))


/*!
 * @brief Obtains a pointer to the method's parameter.
 *
 * @param[in]   idx     Index into the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define PR_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define PR_GET_SEC2_METHOD_ID(mthdId)                                    \
         (((PR_METHOD_ID(mthdId)) - PR_METHOD_ID(FUNCTION_ID)) / 4)

/*!
 * @brief Obtains Method parameter using METHOD ID
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define PR_GET_METHOD_PARAM_OFFSET_FROM_ID(mthdId)                       \
         appMthdArray[PR_GET_SEC2_METHOD_ID(mthdId)]

/* ------------------------- Prototypes ------------------------------------- */

#endif // PR_MTHDS_H
