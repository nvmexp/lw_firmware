/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_posted_write.c
 * @brief  Contains new functions added for posted write support.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "sec2_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_posted_write.h"
#include "hwproject.h"

/* ------------------------- Macros and Defines ----------------------------- */
// We assume SYS0, GPC and FBP are always present
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_graphics_nobundle.h"

#if LW_SCAL_LITTER_NUM_SYSB != 0
#include "dev_pri_ringstation_sysb.h"
#define SYSB_EXISTS
#endif

#if LW_SCAL_LITTER_NUM_SYSC != 0
#include "dev_pri_ringstation_sysc.h"
#define SYSC_EXISTS
#endif

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Check if interrupts are disabled at POSTED_WRITE_INIT and POSTED_WRITE_END
 *
 * The reason for doing so is because we want to avoid task switching, as this might
 * clear posted write errors.
 *
 * We check this prior to the first posted write in POSTED_WRITE_INIT and then again
 * in POSTED_WRITE_END to avoid silent failures if we exited the critical section
 * somewhere between INIT and END.
 *
 * Note that we can't forcefully enter critical section here because this may be called
 * from within HS mode, and the critical section APIs are not in the HS overlays.
 *
 * @return  FLCN_OK                                     Success (Interrupts are disabled)
 *          FLCN_ERR_POSTED_WRITE_INTERRUPTS_ENABLED    Interrupts are enabled
 */
FLCN_STATUS
sec2CheckInterruptsDisabled_GA10X(void)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU32       csw         = 0xFFFFFFFF;
    LwU32       mask        = ((1<<CSW_FIELD_IE0) | (1<<CSW_FIELD_IE1));

    // Read CSW register
    falc_rspr(csw, CSW);

    // Check if IE0 and IE1 bits are clear, indicating that interrupts are disabled
    if ((csw & mask) != 0)
    {
        return FLCN_ERR_POSTED_WRITE_INTERRUPTS_ENABLED;
    }

    return flcnStatus;
}

/*!
 * @brief Reads, updates and writes back the *_ERROR_PRECEDENCE register of respective PRI hub
 *
 * @param[in] priHubId              Used to distinguish between different pri hubs
 *            bEnable               Used to distinguish between enable / disable of precedence
 *
 * @return  FLCN_OK                                             Success
 *          FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS              Incorrect parameter passed to the function
 *
 */
FLCN_STATUS
sec2ReadUpdateWritePrecedence_GA10X(LwU32 priHubId, LwBool bEnable)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data       = 0;

    if ((bEnable != SEC2_ENABLE_PRECEDENCE) && (bEnable != SEC2_DISABLE_PRECEDENCE))
    {
        return FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS;
    }

    switch(priHubId)
    {
        case SYS0_PRI_HUB_ID:
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRECEDENCE, &data));
            if (bEnable == SEC2_ENABLE_PRECEDENCE)
            {
                data = FLD_SET_DRF(_PPRIV, _SYS, _PRIV_ERROR_PRECEDENCE_EN, _ENABLED, data);
            }
            else
            {
                data = FLD_SET_DRF(_PPRIV, _SYS, _PRIV_ERROR_PRECEDENCE_EN, _DISABLED, data);
            }
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRECEDENCE, data));
            break;

#ifdef SYSB_EXISTS
        case SYSB_PRI_HUB_ID:
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYSB_PRIV_ERROR_PRECEDENCE, &data));
            if (bEnable == SEC2_ENABLE_PRECEDENCE)
            {
                data = FLD_SET_DRF(_PPRIV, _SYSB, _PRIV_ERROR_PRECEDENCE_EN, _ENABLED, data);
            }
            else
            {
                data = FLD_SET_DRF(_PPRIV, _SYSB, _PRIV_ERROR_PRECEDENCE_EN, _DISABLED, data);
            }
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYSB_PRIV_ERROR_PRECEDENCE, data));
            break;
#endif

#ifdef SYSC_EXISTS
        case SYSC_PRI_HUB_ID:
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYSC_PRIV_ERROR_PRECEDENCE, &data));
            if (bEnable == SEC2_ENABLE_PRECEDENCE)
            {
                data = FLD_SET_DRF(_PPRIV, _SYSC, _PRIV_ERROR_PRECEDENCE_EN, _ENABLED, data);
            }
            else
            {
                data = FLD_SET_DRF(_PPRIV, _SYSC, _PRIV_ERROR_PRECEDENCE_EN, _DISABLED, data);
            }
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYSC_PRIV_ERROR_PRECEDENCE, data));
            break;
#endif

        case GPC_PRI_HUB_ID:
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_GPC_PRIV_ERROR_PRECEDENCE, &data));
            if (bEnable == SEC2_ENABLE_PRECEDENCE)
            {
                data = FLD_SET_DRF(_PPRIV, _GPC, _PRIV_ERROR_PRECEDENCE_EN, _ENABLED, data);
            }
            else
            {
                data = FLD_SET_DRF(_PPRIV, _GPC, _PRIV_ERROR_PRECEDENCE_EN, _DISABLED, data);
            }
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_GPC_PRIV_ERROR_PRECEDENCE, data));
            break;

        case FBP_PRI_HUB_ID:
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_FBP_PRIV_ERROR_PRECEDENCE, &data));
            if (bEnable == SEC2_ENABLE_PRECEDENCE)
            {
                data = FLD_SET_DRF(_PPRIV, _FBP, _PRIV_ERROR_PRECEDENCE_EN, _ENABLED, data);
            }
            else
            {
                data = FLD_SET_DRF(_PPRIV, _FBP, _PRIV_ERROR_PRECEDENCE_EN, _DISABLED, data);
            }
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_FBP_PRIV_ERROR_PRECEDENCE, data));
            break;

        default:
            return FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Toggle SEC2 error precedence
 * By disabling it, we also clear outstanding errors.
 * By enabling it, we re-enable SEC2 error precedence.
 *
 * @return  FLCN_OK  Success
 *
 */
FLCN_STATUS
sec2ToggleSec2PrivErrorPrecedence_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Toggle LW_PPRIV_SYS_PRIV_ERROR_PRECEDENCE
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYS0_PRI_HUB_ID, SEC2_DISABLE_PRECEDENCE));
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYS0_PRI_HUB_ID, SEC2_ENABLE_PRECEDENCE));

#ifdef SYSB_EXISTS
    // Toggle LW_PPRIV_SYSB_PRIV_ERROR_PRECEDENCE
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYSB_PRI_HUB_ID, SEC2_DISABLE_PRECEDENCE));
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYSB_PRI_HUB_ID, SEC2_ENABLE_PRECEDENCE));
#endif

#ifdef SYSC_EXISTS
    // Toggle LW_PPRIV_SYSC_PRIV_ERROR_PRECEDENCE
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYSC_PRI_HUB_ID, SEC2_DISABLE_PRECEDENCE));
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, SYSC_PRI_HUB_ID, SEC2_ENABLE_PRECEDENCE));
#endif

    // Toggle LW_PPRIV_GPC_PRIV_ERROR_PRECEDENCE
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, GPC_PRI_HUB_ID, SEC2_DISABLE_PRECEDENCE));
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, GPC_PRI_HUB_ID, SEC2_ENABLE_PRECEDENCE));

    // Toggle LW_PPRIV_FBP_PRIV_ERROR_PRECEDENCE
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, FBP_PRI_HUB_ID, SEC2_DISABLE_PRECEDENCE));
    CHECK_FLCN_STATUS(sec2ReadUpdateWritePrecedence_HAL(&Acr, FBP_PRI_HUB_ID, SEC2_ENABLE_PRECEDENCE));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Flush preceding PRI transactions
 * Flush preceding PRI transactions by writing with ACK to PRI_FENCE registers
 * of all PRI targets.
 *
 * @return  FLCN_OK  Success
 *
 */
FLCN_STATUS
sec2FlushPriTransactions_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_FENCE, LW_PPRIV_SYS_PRI_FENCE_V_0));

#ifdef SYSB_EXISTS
    // We don't have a define for 0x0 for SYSB
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYSB_PRI_FENCE, 0x0));
#endif

#ifdef SYSC_EXISTS
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYSC_PRI_FENCE, LW_PPRIV_SYSC_PRI_FENCE_V_0));
#endif

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_GPC_PRI_FENCE, LW_PPRIV_GPC_PRI_FENCE_V_0));

    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_FBP_PRI_FENCE, LW_PPRIV_FBP_PRI_FENCE_V_0));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Extracts subid from the ERROR_INFO register and checks for posted write failures.
 *
 * In case of error, the error code is extracted from the ERROR_CODE register and written to
 * the returnCodeRegister passed to the function.
 *
 * @param[in] priHubId              Used to distinguish between different pri hubs
 *            clusterIndex          Used to get cluster index for GPC and FBP pri hubs (it is ignored for others)
 *            returnCodeRegister    Register to store error code in case of FLCN_ERR_POSTED_WRITE_FAILURE
 *
 * @return  FLCN_OK                                             Success
 *          FLCN_ERR_POSTED_WRITE_FAILURE                       Posted write failure detected
 *          FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS              Incorrect parameter passed to the function
 *
 */
FLCN_STATUS
sec2CheckForPriError_GA10X(LwU32 priHubId, LwU32 clusterIndex, LwU32 returnCodeRegister)
{
    FLCN_STATUS flcnStatus          = FLCN_OK;
    LwU32       subId               = 0;
    LwU32       errorCode           = 0;
    LwU32       errorInfoRegister   = 0;
    LwU32       errorCodeRegister   = 0;
    LwU32       strideVal           = 0;

    switch(priHubId)
    {
        case SYS0_PRI_HUB_ID:
            errorInfoRegister = LW_PPRIV_SYS_PRIV_ERROR_INFO;
            errorCodeRegister = LW_PPRIV_SYS_PRIV_ERROR_CODE;
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorInfoRegister, &subId));
            subId = DRF_VAL(_PPRIV, _SYS_PRIV_ERROR_INFO, _SUBID, subId);
            break;

#ifdef SYSB_EXISTS
        case SYSB_PRI_HUB_ID:
            errorInfoRegister = LW_PPRIV_SYSB_PRIV_ERROR_INFO;
            errorCodeRegister = LW_PPRIV_SYSB_PRIV_ERROR_CODE;
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorInfoRegister, &subId));
            subId = DRF_VAL(_PPRIV, _SYSB_PRIV_ERROR_INFO, _SUBID, subId);
            break;
#endif

#ifdef SYSC_EXISTS
        case SYSC_PRI_HUB_ID:
            errorInfoRegister = LW_PPRIV_SYSC_PRIV_ERROR_INFO;
            errorCodeRegister = LW_PPRIV_SYSC_PRIV_ERROR_CODE;
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorInfoRegister, &subId));
            subId = DRF_VAL(_PPRIV, _SYSC_PRIV_ERROR_INFO, _SUBID, subId);
            break;
#endif

        case GPC_PRI_HUB_ID:
            strideVal         = (clusterIndex * LW_GPC_PRIV_STRIDE);
            errorInfoRegister = (LW_PPRIV_GPC_GPC0_PRIV_ERROR_INFO + strideVal);
            errorCodeRegister = (LW_PPRIV_GPC_GPC0_PRIV_ERROR_CODE + strideVal);
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorInfoRegister, &subId));
            subId = DRF_VAL(_PPRIV, _GPC_PRIV_ERROR_INFO, _SUBID, subId);
            break;

        case FBP_PRI_HUB_ID:
            strideVal         = (clusterIndex * LW_FBP_PRIV_STRIDE);
            errorInfoRegister = (LW_PPRIV_FBP_FBP0_PRIV_ERROR_INFO + strideVal);
            errorCodeRegister = (LW_PPRIV_FBP_FBP0_PRIV_ERROR_CODE + strideVal);
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorInfoRegister, &subId));
            subId = DRF_VAL(_PPRIV, _FBP_PRIV_ERROR_INFO, _SUBID, subId);
            break;

        default:
            return FLCN_ERR_POSTED_WRITE_INCORRECT_PARAMS;
    }

    if (LW_PPRIV_SYS_PRI_SLAVE_sec_source2sys_pri_hub_pri_SUBID_LO <= subId
        && subId <= LW_PPRIV_SYS_PRI_SLAVE_sec_source2sys_pri_hub_pri_SUBID_HI)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(errorCodeRegister, &errorCode));
        CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(returnCodeRegister, errorCode));
        // 
        // Not checking return code for sec2ToggleSec2PrivErrorPrecedence_HAL since this
        // can't fail lwrrently. Even if it does fail, corrective action is unknown.
        //
        sec2ToggleSec2PrivErrorPrecedence_HAL(&Acr);
        return FLCN_ERR_POSTED_WRITE_FAILURE;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check for non-blocking (posted) writes errors in the entire transaction
 * Check for posted writed errors by checking the value of subid from the ERROR_INFO 
 * register of all PRI targets. An error is detected if the read subid falls in 
 * SEC2's subid range.
 *
 * In case of error, we get the error code from the ERROR_CODE register of the PRI
 * target, save that in a Mailbox register, toggle SEC2 precedence of all PRI targets
 * to clear errors, and return with a failure.
 *
 * @param[in] returnCodeRegister     Register to store error code in case of FLCN_ERR_POSTED_WRITE_FAILURE
 *
 * @return  FLCN_OK                                             Success
 *          FLCN_ERR_POSTED_WRITE_FAILURE                       Posted write failure detected
 *          FLCN_ERR_POSTED_WRITE_PRI_CLUSTER_COUNT_MISMATCH    Mismatch in count of checked PRI clusters
 *                                                              of a PRI target and total count
 *
 */
FLCN_STATUS
sec2CheckNonBlockingWritePriErrors_GA10X(LwU32 returnCodeRegister)
{
    FLCN_STATUS flcnStatus      = FLCN_OK;
    LwU32       fsRegisterVal   = 0;

#define CHECK_IF_ACTIVE_CLUSTERS_ARE_GREATER_THAN_TOTAL(activeCount, totalCount)    \
    do                                                                              \
    {                                                                               \
        if (activeCount > totalCount)                                               \
        {                                                                           \
            sec2ToggleSec2PrivErrorPrecedence_HAL(&Acr);                            \
            return FLCN_ERR_POSTED_WRITE_PRI_CLUSTER_COUNT_MISMATCH;                \
        }                                                                           \
    } while (LW_FALSE)


    // Just to make sure mcheck checks this define, not compiled into actual code
    MCHECK_REG(LW_SCAL_LITTER_NUM_SYSS);
    // Check error info from LW_PPRIV_SYS_PRIV_ERROR_INFO
    CHECK_FLCN_STATUS(sec2CheckForPriError_HAL(&Acr, SYS0_PRI_HUB_ID,
                                                    0x0, returnCodeRegister));

    // Just to make sure mcheck checks this define, not compiled into actual code
    MCHECK_REG(LW_SCAL_LITTER_NUM_SYSB);
#ifdef SYSB_EXISTS
    // Check error info from LW_PPRIV_SYSB_PRIV_ERROR_INFO
    CHECK_FLCN_STATUS(sec2CheckForPriError_HAL(&Acr, SYSB_PRI_HUB_ID,
                                                    0x0, returnCodeRegister));
#endif

    // Just to make sure mcheck checks this define, not compiled into actual code
    MCHECK_REG(LW_SCAL_LITTER_NUM_SYSC);
#ifdef SYSC_EXISTS
    // Check error info from LW_PPRIV_SYSC_PRIV_ERROR_INFO
    CHECK_FLCN_STATUS(sec2CheckForPriError_HAL(&Acr, SYSC_PRI_HUB_ID,
                                                    0x0, returnCodeRegister));
#endif

    // Read LW_PGRAPH_PRI_FECS_FS to extract active GPCs and FBPs ahead
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGRAPH_PRI_FECS_FS, &fsRegisterVal));

    // 
    // Check error info from individual GPC Clusters (LW_PPRIV_GPC_PRIV_ERROR_INFO)
    // LW_PPRIV_GPC_GPC0_PRIV_ERROR_INFO, LW_PPRIV_GPC_GPC1_PRIV_ERROR_INFO... and so on,
    // till number of active clusters for the chip
    //
    LwU32 activeGpcCount = DRF_VAL(_PGRAPH_PRI, _FECS_FS, _NUM_AVAILABLE_GPCS, fsRegisterVal);
    LwU32 gpcIndex = 0;

    CHECK_IF_ACTIVE_CLUSTERS_ARE_GREATER_THAN_TOTAL(activeGpcCount, LW_SCAL_LITTER_NUM_GPCS);

    // Loop from gpcIndex=0 to gpcIndex=(activeGpcCount-1)
    for (gpcIndex = 0; gpcIndex < activeGpcCount; gpcIndex++)
    {
        CHECK_FLCN_STATUS(sec2CheckForPriError_HAL(&Acr, GPC_PRI_HUB_ID,
                                                        gpcIndex, returnCodeRegister));
    }

    // 
    // Check error info from individual FBP Clusters (LW_PPRIV_FBP_PRIV_ERROR_INFO)
    // LW_PPRIV_FBP_FBP0_PRIV_ERROR_INFO, LW_PPRIV_FBP_FBP1_PRIV_ERROR_INFO... and so on
    // till number of active clusters for the chip
    //
    LwU32 activeFbpCount = DRF_VAL(_PGRAPH_PRI, _FECS_FS, _NUM_AVAILABLE_FBPS, fsRegisterVal);
    LwU32 fbpIndex = 0;

    CHECK_IF_ACTIVE_CLUSTERS_ARE_GREATER_THAN_TOTAL(activeFbpCount, LW_SCAL_LITTER_NUM_FBPS);

    // Loop from fbpIndex=0 to fbpIndex=(activeFbpCount-1)
    for (fbpIndex = 0; fbpIndex < activeFbpCount; fbpIndex++)
    {
        CHECK_FLCN_STATUS(sec2CheckForPriError_HAL(&Acr, FBP_PRI_HUB_ID,
                                                        fbpIndex, returnCodeRegister));
    }

#undef CHECK_IF_ACTIVE_CLUSTERS_ARE_GREATER_THAN_TOTAL

ErrorExit:
    return flcnStatus;
}
