/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef VBIOS_H
#define VBIOS_H

/*!
 * @file   vbios_image.h
 * @brief  Interfaces for retrieving data from a VBIOS image.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "data_id_reference_table_addendum.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*-------------------------- Public Functions --------------------------------*/
/*!
 * @brief   Retrieves a pointer to data in the VBIOS
 *
 * @param[in]   pVbios
 * @param[in]   dirtId  DIRT identifier for table from which to retrieve data
 * @param[out]  pData   Pointer into which to store pointer to retrieved data
 * @param[in]   offset  Offset of the data in the table to retrieve
 * @param[in]   size    Size of the data to retrieve from table
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  dirtId is unrecognized; pVbios or
 *                                          ppData is NULL; or offset and size
 *                                          are invalid for the given table.
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  @ref FLCN_ERR_NOT_SUPPORTED     Given dirtId is not supported on
 *                                          this system.
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosImageDataGet(
    const OBJVBIOS *pVbios, LW_GFW_DIRT dirtId, const void **ppData, LwLength offset, LwLength size)
    GCC_ATTRIB_SECTION("imem_libVbiosImage", "vbiosImageDataGet");

/*!
 * @copydoc vbiosImageDataGet
 *
 * @note    This is a wrapper macro for @ref vbiosImageDataGet. It exists
 *          because that function needs to take a pointer to pointer to void,
 *          while we normally want to work with typed pointers. To avoid needing
 *          to manually cast to (void **) every time the function is called,
 *          this macro wraps the call to handle the types correctly.
 */
#define vbiosImageDataGetTyped(_pVbios, _dirtId, _ppData, _offset, _size) \
    ({ \
        __label__ vbiosImageDataGetTyped_exit; \
        FLCN_STATUS vbiosImageDataGetTypedLocalStatus; \
        const void *pVbiosImageDataGetTypedLocalData; \
        typeof(_ppData) ppVbiosImageDataGetTypedLocalData = (_ppData); \
    \
        PMU_ASSERT_TRUE_OR_GOTO(vbiosImageDataGetTypedLocalStatus, \
            (ppVbiosImageDataGetTypedLocalData != NULL), \
            FLCN_ERR_ILWALID_ARGUMENT, \
            vbiosImageDataGetTyped_exit); \
    \
        vbiosImageDataGetTypedLocalStatus = vbiosImageDataGet( \
            (_pVbios), \
            (_dirtId), \
            &pVbiosImageDataGetTypedLocalData, \
            (_offset), \
            (_size)); \
        if (vbiosImageDataGetTypedLocalStatus == FLCN_ERR_NOT_SUPPORTED) \
        { \
            goto vbiosImageDataGetTyped_exit; \
        } \
        \
        PMU_ASSERT_OK_OR_GOTO(vbiosImageDataGetTypedLocalStatus, \
            vbiosImageDataGetTypedLocalStatus, \
            vbiosImageDataGetTyped_exit); \
    \
        *ppVbiosImageDataGetTypedLocalData = pVbiosImageDataGetTypedLocalData; \
    \
    vbiosImageDataGetTyped_exit:\
        vbiosImageDataGetTypedLocalStatus; \
    })

#endif // VBIOS_H
