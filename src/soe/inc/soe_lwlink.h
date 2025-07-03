/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_LWLINK_H
#define SOE_LWLINK_H

/*!
 * @file   soe_lwlink.h
 * @brief  Common defines
*/

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Arguments for Thermal SECCMD
 *
 * argsType            Thermal SECCMD Type (toggle, status)
 * argsLevel           Thermal Mode Level (L0)
 */
struct SECCMD_THERMAL_ARGS
{
    LwU8 argsType;
    LwU8 argsLevel;
};

/*!
 * Status of Thermal SECCMD 
 *
 * thermalMode        Current Thermal Mode
 * thermalLevel       Current Thermal Level (L0)
 * linkL1statusMask   Mask of links in L1 state
 */
typedef struct seccmd_thermal_status
{
    LwU8 thermalMode;
    LwU8 thermalLevel;
    LwU8 linkL1status;    
} SECCMD_THERMAL_STAT;

typedef union seccmd_args
{
    struct SECCMD_THERMAL_ARGS seccmdThermalArgs;
} SECCMD_ARGS;


#endif // SOE_LWLINK_H
