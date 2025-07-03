/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmchannel.h
 * @brief @copydoc thrmchannel.c
 */

#ifndef THRMCHANNEL_H
#define THRMCHANNEL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "ctrl/ctrl2080/ctrl2080thermal.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuiftherm.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_THERM_CHANNEL \
    (&(Therm.channels.super))

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define THERM_CHANNEL_INDEX_IS_VALID(_objIdx) \
    BOARDOBJGRP_IS_VALID(THERM_CHANNEL, _objIdx)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define THERM_CHANNEL_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(THERM_CHANNEL, (_objIdx)))

/*!
 * @brief   List of overlay descriptors required by thermal channel code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_THERM_CHANNEL             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)            \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_CHANNEL THERM_CHANNEL, THERM_CHANNEL_BASE;

// THERM_CHANNEL interfaces

/*!
 * @brief Obtain temperature of the appropriate THERM_CHANNEL.
 *
 * @param[in]  pChannel     THERM_CHANNEL object pointer
 * @param[out] pLwTemp      Current temperature
 *
 * @return FLCN_OK
 *     Temperature successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     If interface isn't supported.
 * @return Other unexpected errors
 *     Unexpected errors propagated from other functions.
 * 
 * @extends BOARDOBJ
 */
#define ThermChannelTempValueGet(fname) FLCN_STATUS (fname)(THERM_CHANNEL *pChannel, LwTemp *pLwTemp)

/*!
 * @brief Extends BOARDOBJ providing attributes common to all THERM_CHANNEL.
 */
struct THERM_CHANNEL
{
    /*!
     * @brief Super class. This should always be the first member!
     * @copydoc BOARDOBJ
     */
    BOARDOBJ    super;

    /*!
     * @brief Temperature scaling
     */
    LwSFXP8_8   scaling;

    /*!
     * @brief SW Temperature offset
     */
    LwSFXP8_8   offsetSw;

    /*!
     * @brief Min supported temperature after scaling and offset adjustment
     */
    LwTemp      lwTempMin;

    /*!
     * @brief Max supported temperature after scaling and offset adjustment
     */
    LwTemp      lwTempMax;

    /*!
     * Temp Simulation Data Structure
     */
    struct {
        /*!
         * Boolean which indicates if TempSim is supported.
         */
        LwBool bSupported;

        /*!
         * Boolean which indicates if TempSim Enable is requested.
         */
        LwBool bEnabled;

        /*!
         * Temperature to be Simulated.
         */
        LwTemp targetTemp;
    } tempSim;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet           (thermChannelGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermChannelGrpIfaceModel10ObjSetImpl_SUPER");

BoardObjGrpIfaceModel10CmdHandler       (thermChannelBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermChannelBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10CmdHandler       (thermChannelBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermChannelBoardObjGrpIfaceModel10GetStatus");

BoardObjIfaceModel10GetStatus               (thermThermChannelIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermThermChannelIfaceModel10GetStatus");

BoardObjIfaceModel10GetStatus               (thermChannelIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermChannelIfaceModel10GetStatus_SUPER");

ThermChannelTempValueGet    (thermChannelTempValueGet)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermChannelTempValueGet");

/*!
 * @brief Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (thermThermChannelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermThermChannelIfaceModel10SetEntry");

/* ------------------------ Include Derived Types -------------------------- */
#include "therm/thrmchannel_device.h"

#endif // THRMCHANNEL_H
