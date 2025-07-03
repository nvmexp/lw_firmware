/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_register_access_gh100_only.c
 */
#include "acr.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_gpc.h"


#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/*
 * @brief Return the target mask register details for GPCCS
 * @param[in] falconInstance            : Engine instance id to be used for SMC use case
 * @param[out] pTargetMaskRegister      : Holds the value of TargetMask Register to be used
 * @param[out] pTargetMaskPlmRegister   : Holds the value of TargeMaskPLM register to be used 
 * @param[out] pTargetMaskIndexRegister : Holds the value of TargetMaskIndex register to be used
 *
 * @return ACR_OK if sucessfull else ACR error code.
 * 
 */
ACR_STATUS
acrlibGetGpccsTargetMaskRegisterDetails_GH100
(
    LwU32 falconInstance,
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    if (pTargetMaskRegister == NULL || pTargetMaskPlmRegister == NULL || pTargetMaskIndexRegister ==NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    switch (falconInstance)
    {
        case 0:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC0_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC0_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC0_TARGET_MASK_INDEX;
             break;
        case 1:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC1_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC1_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC1_TARGET_MASK_INDEX;
             break;
        case 2:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC2_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC2_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC2_TARGET_MASK_INDEX;
             break;
        case 3:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC3_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC3_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC3_TARGET_MASK_INDEX;
             break;
        case 4:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC4_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC4_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC4_TARGET_MASK_INDEX;
             break;
        case 5:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC5_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC5_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC5_TARGET_MASK_INDEX;
             break;
        case 6:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC6_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC6_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC6_TARGET_MASK_INDEX;
             break;
        case 7:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC7_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC7_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC7_TARGET_MASK_INDEX;
             break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    return ACR_OK;
}


/*!
 * @brief Get the target mask index for FECS engine
 * @param[in] falconInstance    : Engine instance id to be used for SMC use case
 * @param[out] pTargetMaskIndex : Target Mask index to be used
 * @returns ACR_STATUS, ACR_OK if sucessfull else ERROR
 */
ACR_STATUS
acrlibGetFecsTargetMaskIndex_GH100
(
    LwU32 falconInstance, 
    LwU32 *pTargetMaskIndex
)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconInstance)
    {
        case 0:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri0; 
            break;
        case 1:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri1; 
            break;
        case 2:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri2; 
            break;
        case 3:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri3; 
            break;
        case 4:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri4; 
            break;
        case 5:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri5; 
            break;
        case 6:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri6; 
            break;
        case 7:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri7; 
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
        
    }
    
    return ACR_OK;
}
