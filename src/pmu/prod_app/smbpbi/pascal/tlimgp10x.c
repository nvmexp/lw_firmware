/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    tlimgp10x.c
 * @brief   SMBPBI PMU HAL functions for GP10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_smbpbi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Retrieves dedicated overt temperature threshold
 *
 * @param[out]  pLimit  container to hold the result temperature threshold
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   invalid pointer passed
 * @return  FLCN_OK                     succesfull exelwtion
 */
FLCN_STATUS
smbpbiGetLimitShutdn_GP10X
(
    LwTemp  *pLimit
)
{
    LwTemp      negHSOffset;
    FLCN_STATUS status;

    if (pLimit == NULL)
    {
        PMU_HALT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    status = thermSlctGetEventTempThresholdHSOffsetAndSensorId_HAL(&Therm,
                                        RM_PMU_THERM_EVENT_DEDICATED_OVERT,
                                        pLimit, &negHSOffset, NULL);
    if (status == FLCN_OK)
    {
        *pLimit += negHSOffset;
    }

    return status;
}

/*!
 * @brief   Retrieves minimum thermal slowdown threshold temperature
 *
 * @param[out]  pLimit  container to hold the result temperature threshold
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   invalid pointer passed
 * @return  FLCN_OK                     succesfull exelwtion
 */
FLCN_STATUS
smbpbiGetLimitSlowdn_GP10X
(
    LwTemp  *pLimit
)
{
    LwU8        eventId;
    LwTemp      temp;
    LwTemp      limit;
    LwTemp      negHSOffset;
    FLCN_STATUS status;

    if (pLimit == NULL)
    {
        PMU_HALT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Start with something unreasonably high
    limit = RM_PMU_LW_TEMP_127_C;
    for (eventId = RM_PMU_THERM_EVENT_THERMAL_0;
        eventId <= RM_PMU_THERM_EVENT_THERMAL_11; ++eventId)
    {
        status = thermSlctGetEventTempThresholdHSOffsetAndSensorId_HAL(&Therm,
                                                eventId, &temp, &negHSOffset, NULL);

        if ((status == FLCN_OK) && (temp != 0))
        {
            temp += negHSOffset;
            limit = LW_MIN(limit, temp);
        }
    }

    if (RM_PMU_LW_TEMP_127_C == limit)
    {
        // Have not found any limits
        return FLCN_ERR_NOT_SUPPORTED;
    }
    else
    {
        *pLimit = limit;
        return FLCN_OK;
    }
}

/*!
 * @brief   Enable SMBPBI thermal capabilities specific to this HAL
 *
 * @param[out]  pCap    pointer to the capabilities word to be adjusted
 *
 * @return  void
 */
void
smbpbiSetThermCaps_GP10X
(
    LwU32   *pCap
)
{
    *pCap = FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _THERMP_TEMP_SLOWDN, _AVAILABLE, *pCap);
    *pCap = FLD_SET_DRF(_MSGBOX, _DATA_CAP_0, _THERMP_TEMP_SHUTDN, _AVAILABLE, *pCap);
}
