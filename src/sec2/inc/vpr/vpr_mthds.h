/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef VPR_MTHDS_H
#define VPR_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwuproc.h"
#include "sec2_chnmgmt.h"
#include "class/cl95a1.h"

#define VPR_TEST_MODE   1

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Size of the buffer used for DMA transfer of VPR method parameters.
 */
#define CHECK_RC(x)              if ((rc = (x)) != FLCN_OK){goto label_return;}

/*!
 * @brief The SEC2 VPR short-hand notation for the LW95A1 VPR class method.
 *
 * @param[in]   mthd    SEC2 VPR short-hand notation.
 *
 * @return  LW95A1 VPR class method.
 */
#define VPR_METHOD_ID(mthd)                                 LW95A1_VPR_##mthd

/*!
 * @brief Colwerts an index into appMthdArray into the LW95A1 VPR method value.
 *
 * @param[in]   idx     Index into the appMthdArray.
 *
 * @return  LW95A1 VPR class method.
 */
#define VPR_GET_METHOD_ID(idx) \
    (VPR_METHOD_ID(PROGRAM_REGION) + ((idx) * 4))


/*!
 * @brief Obtains a pointer to the method's parameter.
 *
 * @param[in]   idx     Index into the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define VPR_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define VPR_GET_SEC2_METHOD_ID(mthdId)                                    \
         (((VPR_METHOD_ID(mthdId)) - VPR_METHOD_ID(PROGRAM_REGION)) / 4)

/*!
 * @brief Obtains Method parameter using METHOD ID
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define VPR_GET_METHOD_PARAM_OFFSET_FROM_ID(mthdId)                       \
         appMthdArray[VPR_GET_SEC2_METHOD_ID(mthdId)]

/* ------------------------- Prototypes ------------------------------------- */
LwBool vprIsSupportedHS(void)
       GCC_ATTRIB_SECTION("imem_vprHs", "vprIsSupportedHS");
FLCN_STATUS vprCommandProgramRegion(LwU64 startAddr, LwU64 size)
       GCC_ATTRIB_SECTION("imem_vprHs", "vprProgramRegion");
#endif // VPR_MTHDS_H

