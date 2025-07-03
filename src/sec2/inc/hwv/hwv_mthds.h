/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HWV_MTHDS_H
#define HWV_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lwuproc.h"
#include "sec2_chnmgmt.h"
#include "class/cl95a1.h"
#include "tsec_drv.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief The SEC2 HWV short-hand notation for the LW95A1 HWV class method.
 *
 * @param[in]   mthd    SEC2 HWV short-hand notation.
 *
 * @return  LW95A1 HWV class method.
 */
#define HWV_METHOD_ID(mthd)                                 LW95A1_HWV_##mthd

/*!
 * @brief Colwerts an index in appMthdArray into the LW95A1 HWV method value.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  LW95A1 HWV class method.
 */


#define HWV_GET_METHOD_ID(idx) \
    (HWV_METHOD_ID(DO_COPY) + ((idx) * 4))


/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define HWV_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define HWV_GET_SEC2_METHOD_ID(mthdId)                                    \
         (((HWV_METHOD_ID(mthdId)) - HWV_METHOD_ID(DO_COPY)) / 4)

/*!
 * @brief Obtains Method parameter using METHOD ID
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define HWV_GET_METHOD_PARAM_OFFSET_FROM_ID(mthdId)                       \
         appMthdArray[HWV_GET_SEC2_METHOD_ID(mthdId)]

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS hwvDoMeasureDma(LwU64 inGpuVA256, LwU64 outGpuVA256,
                            LwU32 inOutBufSize, hwv_perf_eval_results *pPerfResults,
                            LwU8 *pHwvBuffer256, LwU32 hwvBufferSize)
    GCC_ATTRIB_SECTION("imem_hwv", "hwvDoMeasureDma");

FLCN_STATUS hwvRunPerfEval(void)
    GCC_ATTRIB_SECTION("imem_hwv", "hwvRunPerfEval");

#endif // HWV_MTHDS_H
