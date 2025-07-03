/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_msgboxgf11x.c
 * @brief   PMU HAL functions for GF11X+ for thermal message box functionality
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_i2cs.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application Includes --------------------------- */
#include "lib_mutex.h"
#include "task_therm.h"
#include "pmu_objic.h"
#include "pmu_objpmgr.h"
#include "rmpbicmdif.h"
#include "dbgprintf.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Initialize the message box interrupt.
 *
 * Route the message box interrupt to the PMU and disable it by default. It
 * can be enabled if RM activates SMBPBI.
 *
 * The default state of these bits out of reset depends on architecture,
 * but we may also run this init on hardware that's not out of reset
 * (starting a new driver's RM/PMU ucode over an old driver's).
 *
 * Default state of _SMBUS_MSGBOX out of reset:
 *   pre-GK11X         => _INTR_EN_0: _ENABLED,  _INTR_ROUTE: _HOST
 *   GK11X_thru_PASCAL => _INTR_EN_0: _ENABLED,  _INTR_ROUTE: _PMU
 *   VOLTA_and_later   => _INTR_EN_0: _DISABLED, _INTR_ROUTE: _PMU
 *
 * State left by orderly shutdown of pre-CL24627480 (pre-r400_00) RM/PMU:
 *   GK10X_and_later   => _INTR_EN_0: _ENABLED,  _INTR_ROUTE: _HOST
 *
 * The latter case is the reason why this HAL is used for all GPUs - we force
 * the default state of the bits to the expected init state, regardless of
 * what the HW defaults are out of reset.
 *
 * Note:  RM lwrrently does not touch LW_THERM_INTR_* registers (and should
 * not do so in the future as well) so we do not have to synchronize RM&PMU on
 * this access.
 */
void
smbpbiInitInterrupt_GMXXX(void)
{
    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0,
             FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _SMBUS_MSGBOX, _DISABLED,
                         REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0)));

    REG_WR32(CSB, LW_CPWR_THERM_INTR_ROUTE,
             FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE,
                         _THERMAL_TRIGGER_SMBUS_MSGBOX, _PMU,
                         REG_RD32(CSB, LW_CPWR_THERM_INTR_ROUTE)));
}

/*!
 * Specific handler-routine for dealing with thermal message box interrupts.
 */
void
smbpbiService_GMXXX(void)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);

    if (FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _THERMAL_TRIGGER_SMBUS_MSGBOX,
                     _PENDING, regVal))
    {
        // Verify _MSGBOX_INTR bit and clear it.
        regVal = REG_RD32(CSB, LW_CPWR_THERM_MSGBOX_COMMAND);

        if (FLD_TEST_DRF(_MSGBOX, _CMD, _INTR, _PENDING, regVal))
        {
            //
            // It's indeed thermal MSGBOX IRQ, clear its rootcause.
            // This write is stalling the pipeline
            // before we issue the next one to _INTR_0 (Bug 1775014)
            //
            regVal = FLD_SET_DRF(_MSGBOX, _CMD, _INTR, _CLEAR, regVal);
            REG_WR32_STALL(CSB, LW_CPWR_THERM_MSGBOX_COMMAND, regVal);

            // Send message to the SMBPBI task to process msgbox request.
            if (PMUCFG_FEATURE_ENABLED(PMUTASK_THERM))
            {
                // Prevent overflow - increment only if not MAX
                if (SmbpbiResident.requestCnt != LW_U8_MAX)
                {
                    // We only allow a single active request
                    if (SmbpbiResident.requestCnt++ == 0)
                    {
                        DISP2THERM_CMD disp2Therm = {{ 0 }};

                        disp2Therm.hdr.unitId = RM_PMU_UNIT_SMBPBI;
                        disp2Therm.hdr.eventType = SMBPBI_EVT_MSGBOX_REQUEST;
                        disp2Therm.smbpbi.irq.msgBoxCmd = regVal;

                        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, THERM),
                                                 &disp2Therm, sizeof(disp2Therm));
                    }
                }
            }
        }

        // Clear it also in the status register, since PMU will handle it.
        REG_WR32(CSB, LW_CPWR_THERM_INTR_0,
            DRF_DEF(_CPWR_THERM, _INTR_0, _THERMAL_TRIGGER_SMBUS_MSGBOX, _CLEAR));
    }
}

/*!
 * @brief   Enable or disable message box interrupt
 *
 * @param[in]   bEnable if LW_TRUE interrupt is enabled
 *                      if LW_FALSE interrupt is disabled
 */
void
smbpbiEnableInterrupt_GMXXX
(
    LwBool bEnable
)
{
    LwU32 intrEn = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);

    if (bEnable)
    {
        intrEn = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _SMBUS_MSGBOX, _ENABLED,
                             intrEn);
    }
    else
    {
        intrEn = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _SMBUS_MSGBOX, _DISABLED,
                             intrEn);
    }

    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, intrEn);
}

/*!
 * Provides read/write access to message box interface by exelwting requested
 * msgbox action:
 *  PMU_SMBPBI_GET_REQUEST    - retrieve msgbox request
 *  PMU_SMBPBI_SET_RESPONSE   - send out msgbox response
 *
 * @param[in,out]   pCmd    buffer that holds msgbox command or NULL
 * @param[in,out]   pData   buffer that holds msgbox command specific data or NULL
 * @param[in]       action  requested action:
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if invalid action
 * @return FLCN_OK                  otherwise
 */
FLCN_STATUS
smbpbiExelwte_GMXXX
(
    LwU32  *pCmd,
    LwU32  *pData,
    LwU32  *pExtData,
    LwU8    action
)
{
    FLCN_STATUS status  = FLCN_ERR_ILWALID_ARGUMENT;

    if (action == PMU_SMBPBI_GET_REQUEST)
    {
        if (pCmd != NULL)
        {
            *pCmd  = REG_RD32(CSB, LW_CPWR_THERM_MSGBOX_COMMAND);
        }

        if (pData != NULL)
        {
            *pData = REG_RD32(CSB, LW_CPWR_THERM_MSGBOX_DATA_IN);
        }

        if (pExtData != NULL)
        {
            *pExtData = REG_RD32(CSB, LW_CPWR_THERM_MSGBOX_DATA_OUT);
        }

        status = FLCN_OK;
    }
    else if (action == PMU_SMBPBI_SET_RESPONSE)
    {
        //
        // Write data first since cmd carries acknowledgment.
        //
        // GF11X message box implementation brought several changes:
        //  - register addresses => that is answered by this HAL call.
        //  - two data registers(IN/OUT) => since their behavior is fully
        //    equivalent we keep using DATA_IN for both in/out operations
        //    and we may separate that in the future if needed.
        //  - MUTEX => since PMU acts as a client there is no PMU action
        //    related to mutex that is used to synchronize requests.
        //
        if (pData != NULL)
        {
            REG_WR32(CSB, LW_CPWR_THERM_MSGBOX_DATA_IN, *pData);
        }

        if (pExtData != NULL)
        {
            REG_WR32(CSB, LW_CPWR_THERM_MSGBOX_DATA_OUT, *pExtData);
        }

        if (pCmd != NULL)
        {
            smbpbiCheckEvents(pCmd);
            REG_WR32(CSB, LW_CPWR_THERM_MSGBOX_COMMAND, *pCmd);
        }

        status = FLCN_OK;
    }

    return status;
}
