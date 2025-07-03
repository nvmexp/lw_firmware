/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_partition_entry.c
 */
/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acr_objacr.h"
#include "config/g_acr_private.h"
#include "config/g_hal_register.h"

/* ------------------------ Application Includes --------------------------- */
#include <partitions.h>
#include <liblwriscv/scp.h>
#include <liblwriscv/libc.h>
#include "tegra_cdev.h"
#include "gspifacr.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_gsp_prgnlcl.h"
#include "dev_gsp.h"
#include <lwriscv/csr.h>
#include "rpc.h"
#include <liblwriscv/shutdown.h>

/* ------------------------ Function Prototypes ---------------------------- */
static ACR_STATUS _acrPartitionEntryChecks(LwU64 cmdId)         ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrLibLwRiscvDriverInit(LwU64 cmdId)         ATTR_OVLY(OVL_NAME);
static void _acrPartitionExit(LwU64 partition_origin);

/* ------------------------ Global Variables ------------------------------- */
LwU64                g_partition_origin                         ATTR_OVLY(".data");
LwU32               *pDmem                                      ATTR_OVLY(".data");
LIBRARY_STATUS_INFO  libInitStatus                              ATTR_OVLY(".data");

/* ------------------------ Static Variables ------------------------------- */
static ACR_STATUS _acrPartitionEntryChecks(LwU64 cmdId)         ATTR_OVLY(OVL_NAME);
static ACR_STATUS _acrLibLwRiscvDriverInit(LwU64 cmdId)         ATTR_OVLY(OVL_NAME);
static void _acrPartitionExit(LwU64 partition_origin);

/* ------------------------ External Definitions --------------------------- */
extern void partition_acr_scrub_dmem(void);

/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */


PARTITION_EXPORT void acrPartitionEntry(LwU64 partitionOrigin)
{
    ACR_STATUS            status      = ACR_OK;
    LWRV_STATUS           lwrvStatus  = LWRV_OK;
    LwU32                 errInfo     = 0;
    PRM_GSP_ACR_CMD       pRpcAcrCmd  = rpcGetRequestAcr();
    PRM_GSP_ACR_MSG       pRpcAcrMsg  = rpcGetReplyAcr();
    LwU8                  cmdId       = pRpcAcrCmd->cmdType;
    PRM_GSP_ACR_MSG_STATUS pAcrStatus = (PRM_GSP_ACR_MSG_STATUS)&pRpcAcrMsg->acrStatus;
    pAcrStatus->msgType               = cmdId;
    pAcrStatus->errorCode             = status;
    pAcrStatus->errorInfo             = errInfo;
    pAcrStatus->libLwRiscvError       = lwrvStatus;
    g_partition_origin                = partitionOrigin;

    pPrintf("ACR partition entry triggered. Command Type is : %d\n",cmdId);

    // Entry checks for command type
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrPartitionEntryChecks(cmdId));

    // Handle received command
    switch(cmdId)
    {
        // BOOT GSP RM command
        case RM_GSP_ACR_CMD_ID_BOOT_GSP_RM:
        {
            PRM_GSP_ACR_CMD_BOOT_GSP_RM  pbootGspRm    = (PRM_GSP_ACR_CMD_BOOT_GSP_RM)&pRpcAcrCmd->bootGspRm;

#if ACRCFG_FEATURE_ENABLED(GSPRM_BOOT)
            pPrintf("BOOT_GSP_RM command recieved\n");
            status = acrCmdBootGspRm(&pbootGspRm->gspRmDesc, &errInfo);
#elif ACRCFG_FEATURE_ENABLED(GSPRM_PROXY_BOOT)
            pPrintf("BOOT_GSP_RM_PROXY command recieved\n");
            status = acrCmdBootGspRmProxy(&pbootGspRm->gspRmProxyDesc, &errInfo);
#else
            status = ACR_ERROR_ILWALID_OPERATION;
#endif

            pPrintf("BOOT_GSP_RM command processed with result %d\n", status);
            g_partition_origin = PARTITION_ID_RM;
        }
        break;

        // Lock WPR and LS signature verification
        case RM_GSP_ACR_CMD_ID_LOCK_WPR:
        {
            PRM_GSP_ACR_CMD_LOCK_WPR  plockWprDetails = (PRM_GSP_ACR_CMD_LOCK_WPR)&pRpcAcrCmd->lockWprDetails;
            LwU64                     lockWprAddr     = 0;

            LwU64_ALIGN32_UNPACK(&lockWprAddr, &plockWprDetails->wprAddressFb);
            LwU64 *pUnsafeAcrDesc = (LwU64 *)(lockWprAddr);

            pPrintf("LOCK_WPR command received. lockWprAddr=0x%llx\n", lockWprAddr);

            status = acrCmdLockWpr(pUnsafeAcrDesc, &errInfo);
        }
        break;

        // LS engine boot
        case RM_GSP_ACR_CMD_ID_BOOTSTRAP_ENGINE:
        {
            PRM_GSP_ACR_CMD_BOOTSTRAP_ENGINE  pcmdBootEngine = (PRM_GSP_ACR_CMD_BOOTSTRAP_ENGINE)&pRpcAcrCmd->bootstrapEngine;

            pPrintf("LS_BOOT command received: engineId=0x%x, engineInstance=0x%x, engineIndexMask=0x%x, boot_flags=0x%x\n",
                    pcmdBootEngine->engineId, pcmdBootEngine->engineInstance, pcmdBootEngine->engineIndexMask, pcmdBootEngine->boot_flags);

            status = acrCmdLsBoot(pcmdBootEngine->engineId,
                                  pcmdBootEngine->engineInstance,
                                  pcmdBootEngine->engineIndexMask,
                                  pcmdBootEngine->boot_flags);

            // Updating message structure for BOOTSTRAP_ENGINE
            PRM_GSP_ACR_MSG_BOOTSTRAP_ENGINE  pmsgEngine = (PRM_GSP_ACR_MSG_BOOTSTRAP_ENGINE)&pRpcAcrMsg->msgEngine;

            // Updating command specific error codes 
            pmsgEngine->engineId        = pcmdBootEngine->engineId;
            pmsgEngine->engineInstance  = pcmdBootEngine->engineInstance;
        }
        break;

        // Unlock WPR region
        case RM_GSP_ACR_CMD_ID_UNLOCK_WPR:
        {
            pPrintf("UNLOCK_WPR command received\n");

            status = acrCmdUnlockWpr();
        }
        break;

        // Shutdown ACR
        case RM_GSP_ACR_CMD_ID_SHUTDOWN:
        {
            pPrintf("SHUTDOWN command received\n");

            status = acrCmdShutdown();
        }
        break;

        // Invalid command
        default:
        {
            status = ACR_ERROR_ILWALID_OPERATION;
            goto Cleanup;
        }
        break;    
    }

Cleanup:

    //
    // If any if the above operations failed, populate the scratch register to indicate that
    // the current command has failed. Whenever the next command will arrive, it'll know that
    // the previous command failed and it wont proceed further. 
    //
    if ((status != ACR_OK) && (cmdId != RM_GSP_ACR_CMD_ID_BOOTSTRAP_ENGINE))
    {
        ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0(0), ACR_ERROR_PREVIOUS_COMMAND_FAILED);
    }

    //
    // Updating message structure for ACR errors
    // msgType is already updated above
    // 
    pAcrStatus->errorCode       = status;
    pAcrStatus->errorInfo       = errInfo;

    pPrintf("Command 0x%x processing done. status=%d\n", cmdId, status);

    _acrPartitionExit(partitionOrigin);
}

/*
 * @brief :   Function contains ACR entry point checks.
 * @param[in] cmdId  Commands recieved from RM.
 * @return    ACR_OK if successful else return ACR failure status.
 *
 * More details are here:
 * https://confluence.lwpu.com/pages/viewpage.action?pageId=788405760
*/
ACR_STATUS
_acrPartitionEntryChecks
(
    LwU64 cmdId
)
{
    LwU32        regval;
    ACR_STATUS   status = ACR_OK;

#ifndef ACR_FMODEL_BUILD
    // Check if the correct build is running
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckIfBuildIsSupported_HAL());
#endif // ACR_FMODEL_BUILD

    // Verify if binary is running on the expected engine
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckIfEngineIsSupported_HAL());

    // Return error if Priv Sec is disabled
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckIfPrivSecEnabledOnProd_HAL());

    // Make sure SELWRE_LOCK state in HW is compatible with ACR such that ACR should run fine
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrValidateSelwreLockHwStateCompatibilityWithACR_HAL());

    // Revocation check
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckFuseRevocation_HAL());

    // Check for exceptions before running task
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckRiscvExceptions_HAL());

#ifndef ACR_FMODEL_BUILD
    // Check if GSP Reset PLM is L3 protected and Source isolated to GSP.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckEngineResetPlms_HAL());
#endif // ACR_FMODEL_BUILD

    // TODO:  To update this as part of supporting GSP-RM boot command.
    if (cmdId == RM_GSP_ACR_CMD_ID_LOCK_WPR)
    {

        //
        // TODO: Move the exception error code status register to SELWRE_SCRATCH Register.
        // Enable Source isolation to GSP for exception error code status register
        //
        regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK);
        regval = FLD_SET_DRF_NUM( _PRGNLCL, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP), regval);
        ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, regval);

        // TODO: Enable SSP and init the stack_guard with random number generator.
     }

    // Library Init
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLibLwRiscvDriverInit(cmdId));

    return status;
}

/*
 * @brief :   Function contains liblwriscv drivers init calls.
 * @param[in] cmdId  CommandId recieved from RM.
 * @return    ACR_OK if driver init is success else ACR failure status.
*/
ACR_STATUS
_acrLibLwRiscvDriverInit
(
    LwU64 cmdId
)
{
    status_t     statusCcc  = ERR_GENERIC;

    if ((cmdId == RM_GSP_ACR_CMD_ID_LOCK_WPR) || (cmdId == RM_GSP_ACR_CMD_ID_BOOT_GSP_RM))
    {
        if (libInitStatus.bCryptoDeviceInitialized == LW_FALSE)
        {
            // init_crypto_devices() only needs to call once before multi RSA operation
            CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(init_crypto_devices(), ACR_ERROR_LWPKA_INIT_CRYPTO_DEVICE_FAILED);
            libInitStatus.bCryptoDeviceInitialized = LW_TRUE;
        }

        // Init the SCP driver.
        CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(scpInit(SCP_INIT_FLAG_DEFAULT), ACR_ERROR_SCP_LIB_INIT_FAILED);

        libInitStatus.bScpInitialized = LW_TRUE;
    }
    return ACR_OK;
}

/*
 * @brief :   Function contains liblwriscv drivers unload calls.
 * @return    ACR_OK if driver unload is success else ACR failure status.
*/
ACR_STATUS
_acrLibLwRiscvDriverUnload(void)
{
    ACR_STATUS   status     = ACR_OK;

    if (libInitStatus.bScpInitialized == LW_TRUE)
    {
        // Library specific cleanups
        CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(scpShutdown(), ACR_ERROR_SCP_LIB_SHUTDOWN_FAILED);
        libInitStatus.bScpInitialized = LW_FALSE;
    }

    return status;
}

/*
 * @brief :   Function contains ACR exit point checks and cleanup.
 * @param[in] status  acr partition commands return status.
 * @param[in] errInfo acr partition commands errinfo status.
 * @return    Switch to no return 
 *
 * More details are here:
 * https://confluence.lwpu.com/pages/viewpage.action?pageId=788405760
*/
void _acrPartitionExit
(
    LwU64  partition_origin
)
{
    PRM_GSP_ACR_CMD        pRpcAcrCmd         = rpcGetRequestAcr();
    PRM_GSP_ACR_MSG        pRpcAcrMsg         = rpcGetReplyAcr();
    PRM_GSP_ACR_MSG_STATUS pAcrStatus         = (PRM_GSP_ACR_MSG_STATUS)&pRpcAcrMsg->acrStatus;
    ACR_STATUS             driverUnloadStatus = ACR_OK;

    // Unload liblwriscv library. 
    driverUnloadStatus = _acrLibLwRiscvDriverUnload();
    if ((pAcrStatus->errorCode == ACR_OK) && (driverUnloadStatus != ACR_OK))
    {
        pAcrStatus->errorCode = driverUnloadStatus;
    }

#ifdef ACR_GSPRM_BUILD
    LwBool bShutdown = (pAcrStatus->errorCode == ACR_OK) &&
                       (pRpcAcrCmd->cmdType == RM_GSP_ACR_CMD_ID_SHUTDOWN);
#else
    LwBool bShutdown = (pAcrStatus->errorCode == ACR_OK) &&
                       (pRpcAcrCmd->cmdType == RM_GSP_ACR_CMD_ID_UNLOCK_WPR);
#endif

    if (bShutdown)
    {
        acrSetFspToResetGsp_HAL();
    }

    // Scrub stack and bss.
    partition_acr_scrub_dmem();

    if (bShutdown)
    {
        riscvShutdown();
    }
    else
    {
        partitionSwitchNoreturn(0, 0, 0, 0, PARTITION_ID_ACR, g_partition_origin);
    }
}

/*
 * @brief: Function writes error code and call partition exit.
 * @return no return call partition Switch
*/
void acrExceptionHandler(void)
{
    LwU32  regval;

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0(0));

    // Check if exception is already raised then don't call acrPartitionExit()
    if (regval == ACR_ERROR_RISCV_EXCEPTION_FAILURE)
    {
        return;
    }
 
    // TODO: Write to secure scratch register.
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0(0), ACR_ERROR_RISCV_EXCEPTION_FAILURE);

    PRM_GSP_ACR_MSG        pRpcAcrMsg            = rpcGetReplyAcr();
    PRM_GSP_ACR_MSG_STATUS pAcrStatus            = (PRM_GSP_ACR_MSG_STATUS)&pRpcAcrMsg->acrStatus;
    pAcrStatus->errorCode                        = ACR_ERROR_RISCV_EXCEPTION_FAILURE;

    // Call partition exit from trap handler and stop processing further acr requests.
    _acrPartitionExit(g_partition_origin);
}

/*
 * @brief: Function Handle RISCV Exceptions
 * @return no return call partition Switch
*/
void partition_acr_trap_handler(void)
{
    pPrintf("In Partition acr trap handler at 0x%lx, cause 0x%lx val 0x%lx\n",
            csr_read(LW_RISCV_CSR_SEPC),
            csr_read(LW_RISCV_CSR_SCAUSE),
            csr_read(LW_RISCV_CSR_STVAL));

    acrExceptionHandler();
}
