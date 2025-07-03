/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright    2020   by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwlinkga100_only.c
 * @brief   PMU HAL functions for GA100 SMBPBI
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "pmu_objsmbpbi.h"
#include "ioctrl_discovery.h"

/* ------------------------ Application Includes ---------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Static Variables -------------------------------- */
SMBPBI_LWLINK SmbpbiLwlink[LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_GA100]
    GCC_ATTRIB_SECTION("dmem_smbpbiLwlink", "SmbpbiLwlink") = {{{0}}};

/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

/*!
 * @brief This callwlates the base register offset by using the stride between
 *        Lwlink unit registers.
 *
 * @param[in] lwliptNum     Index of top level LWLIPT
 * @param[in] localLinkNum  Index of link in LWLIPT
 * @param[in] unitRegBase   Base unit register offset to access
 *
 * @return Register offset value
 */
LwU32
smbpbiGetLwlinkUnitRegBaseOffset_GA100
(
    LwU32 lwliptNum,
    LwU32 localLinkNum,
    LwU32 unitRegBase
)
{
    LwU32 lwliptStride;
    LwU32 unitRegStride;

    //
    // The link unit strides are the same among all the units, so
    // defaulting to the LWDL unit.
    //
    lwliptStride = LW_DISCOVERY_IOCTRL_UNICAST_1_SW_DEVICE_BASE_LWLDL_0 -
                   LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_0;

    unitRegStride = LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_1 -
                    LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_0;

    return unitRegBase + (lwliptNum * lwliptStride) + (localLinkNum * unitRegStride);
}

/*!
 * @brief Gets the total number of Lwlinks. (LWLIPT * linksPerLwlipt)
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
LwU8
smbpbiGetLwlinkNumLinks_GA100
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_GA100;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the total number of Lwlink LWLIPT's.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
LwU8
smbpbiGetLwlinkNumLwlipt_GA100
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LWLIPT_GA100;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}
