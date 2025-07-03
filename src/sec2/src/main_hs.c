/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main_hs.c
 * @brief  HS routines for initialization
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_hs.h"
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "scp_rand.h"
#include "scp_internals.h"
#include "scp_crypt.h"
#include "sec2_objapm.h"
#include "apm/sec2_apmdefines.h"
#include "config/g_sec2_private.h"
#include "config/g_apm_private.h"
/* ------------------------- Macros and Defines ----------------------------- */

/*
 * Using macros for both KM and SALT instead of initializing global variables to ensure 
 * this gets into code section.
 * TODO: Assign these to global variables and explicitly move into code section to make
 * it easier for future changes.
 */
#define DEVID_KM_DWORD_0 0x8502c30b
#define DEVID_KM_DWORD_1 0xec5d952e
#define DEVID_KM_DWORD_2 0x6b982a89
#define DEVID_KM_DWORD_3 0x20d30714

#define DEVID_SALT_DWORD_0 0x59aae274
#define DEVID_SALT_DWORD_1 0x7ee02b46

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

// Global variable to hold encrypted DEVID. Needs to be aligned.
LwU8 g_devIdEnc[SCP_AES_SIZE_IN_BYTES] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0x0};
// Global variable for derived key based on devid and Salt. Needs to be aligned.
LwU8 g_devIdDer[SCP_AES_SIZE_IN_BYTES] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0x0};

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
FLCN_STATUS initHsEntry(void)
    GCC_ATTRIB_SECTION("imem_initHs", "start");
FLCN_STATUS initDevidKeys(void)
    GCC_ATTRIB_SECTION("imem_initHs", "initDevidKeys");
/* ------------------------- Public Functions ------------------------------- */

#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
FLCN_STATUS
InitSec2HS(void)
{
    FLCN_STATUS  status = FLCN_ERR_ILLEGAL_OPERATION;

#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
    // Load libScpCryptHs HS lib overlay
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif // SEC2_ENCRYPT_DEVID

    osHSModeSetup(OVL_INDEX_IMEM(initHs), LW_FALSE, NULL, 0, HASH_SAVING_DISABLE);

    // Do any initialization that must be done in HS mode
    status = initHsEntry();

    osHSModeCleanup(LW_FALSE, NULL, 0);

#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
    // Detach libScpCryptHs HS lib overlay
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif // SEC2_ENCRYPT_DEVID

    return status;
}
#endif

#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
/*!
 * Initializing keys required for DEVID name decryption.
 * Two elements are generated from this function:
 * --> Encrypted devid
 * --> Derived key using devid and SALT
 * Please refer to https://confluence.lwpu.com/pages/viewpage.action?pageId=115943060
 */
FLCN_STATUS
initDevidKeys(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       devid  = 0;
    LwU8        km[(SCP_AES_SIZE_IN_BYTES * 2) + 1];
    LwU8        iv[SCP_AES_SIZE_IN_BYTES * 2];
    LwU32       *pKm = NULL;
    LwU32       *pIv = NULL;

    // Required for correctness of ALIGN_UP below
    ct_assert(SCP_BUF_ALIGNMENT <= SCP_AES_SIZE_IN_BYTES);

    pKm = (LwU32 *) (LW_ALIGN_UP(((LwU32) km), SCP_BUF_ALIGNMENT));
    pIv = (LwU32 *) (LW_ALIGN_UP(((LwU32) iv), SCP_BUF_ALIGNMENT));

    // Read DEVID
    CHECK_FLCN_STATUS(sec2ReadDeviceIdHs_HAL(&Sec2, &devid));

    //
    // Copy KM. Using direct assignment instead of memcpy to avoid including mem_hs overlay
    // NOTE: Needs scrubbing before exiting this function
    //
    pKm[0] = DEVID_KM_DWORD_0;
    pKm[1] = DEVID_KM_DWORD_1;
    pKm[2] = DEVID_KM_DWORD_2;
    pKm[3] = DEVID_KM_DWORD_3;

    // Init IV to 0 (IV is expected to be 0)
    pIv[0] = pIv[1] = pIv[2] = pIv[3] = 0;

    // Using g_devIdEnc as both input and output buffer
    ((LwU32 *)g_devIdEnc)[0] = devid;
    ((LwU32 *)g_devIdEnc)[1] = 0;
    ((LwU32 *)g_devIdEnc)[2] = 0;
    ((LwU32 *)g_devIdEnc)[3] = 0;

    // Load SCP CRYPT HS overlay
    OS_SEC_HS_LIB_VALIDATE(libScpCryptHs, HASH_SAVING_DISABLE);

    // Encrypt devid
    if ((flcnStatus = scpCbcCryptWithKey((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)g_devIdEnc, 
             SCP_AES_SIZE_IN_BYTES, (LwU8*)g_devIdEnc, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub devid if encryption fails
        ((LwU32 *)g_devIdEnc)[0] = 0;
        goto ErrorExit;
    }

    // Get derived key using devid and salt. Using g_devIdDer for both input and output
    ((LwU32 *)g_devIdDer)[0] = DEVID_SALT_DWORD_0;
    ((LwU32 *)g_devIdDer)[1] = DEVID_SALT_DWORD_1;
    ((LwU32 *)g_devIdDer)[2] = devid;
    ((LwU32 *)g_devIdDer)[3] = 0;

    // Get derived key
    if ((flcnStatus = scpCbcCryptWithKey((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)g_devIdDer, 
             SCP_AES_SIZE_IN_BYTES, (LwU8*)g_devIdDer, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub SALT if encryption fails
        ((LwU32 *)g_devIdDer)[0] = 0;
        ((LwU32 *)g_devIdDer)[1] = 0;
        ((LwU32 *)g_devIdDer)[2] = 0;
        goto ErrorExit;
    }

ErrorExit:
    // Scrub pKm and devid
    pKm[0] = pKm[1] = pKm[2] = pKm[3] = 0;
    devid = 0;

    return flcnStatus;
}
#endif

#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
/*!
 * The purpose of this function is to start the SCP RNG so that it
 * can be used by LS mode.
 * This is done by EXIT_HS() which starts the SCP RNG
 */
FLCN_STATUS
initHsEntry(void)
{
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS status = FLCN_ERR_ILLEGAL_OPERATION;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    if ((status = sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    //
    // Remove GSP from Source Enable mask of the PRIV Error PLM and forcibly re-enable 
    // SEC2 error precedence. (BUG 3055098)
    //
    if ((status = sec2ReconfigurePrivErrorPlmAndPrecedence_HAL(&Sec2)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
    // Disabling initDevidKeys support until it becomes POR
    if ((status = initDevidKeys()) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }
#endif

    // WAR to allow CPU(level0) to access FBPA_PM registers for devtools to work, Bug 2369597
    if ((status = sec2ProvideAccessOfFbpaRegistersToCpuWARBug2369597Hs_HAL(&Sec2)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    //
    // WAR to update TSOSC settings for Bug 2369687(TU102), 2379506(TU106), and 2460727(TU106)
    // This is functional fix, not security, and is required to run only on TU102 and TU106
    // It needs to be done in SEC2-RTOS because we need to override settings done by VBIOS
    // since SEC2-RTOS runs after both cold-boot FWSEC and GC6 UDE i.e. vbios HS
    //
    if ((status = sec2UpdateTsoscSettingsWARBug2369687And2379506And2460727Hs_HAL(&Sec2)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    if (SEC2CFG_FEATURE_ENABLED(SEC2_GPC_RG_SUPPORT))
    {
        // Configure the PLM settings of registers used for PMU-SEC2 communication by GPC-RG
        if ((status = sec2GpcRgRegPlmConfigHs_HAL(&Sec2)) != FLCN_OK)
        {
            goto initHsEntry_exit;
        }
    }

    // WAR to lower PLMs of TSTG registers and Isolate to PMU and FECS (Bug 2735125)
    if ((status = sec2ProvideAccessOfTSTGRegistersToPMUAndFecsWARBug2735125HS_HAL(&Sec2)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))

    // Enable PLM protections on Scratch GROUP0
    if ((status = apmUpdateScratchGroup0PlmHs_HAL(&ApmHal)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    // Initialize the SCRATCH register with invalid status
    if ((status = apmUpdateScratchWithStatus_HAL(&ApmHal, LW_SEC2_APM_STATUS_ILWALID)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    // Check if APM is enabled via InfoROM and SKU matches
    if ((status = apmCheckIfEnabledHs_HAL(&ApmHal)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    // Enable necessary protections
    if ((status = apmEnableProtectionsHs_HAL(&ApmHal)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

    // Update SCRATCH with valid status
    if ((status = apmUpdateScratchWithStatus_HAL(&ApmHal, LW_SEC2_APM_STATUS_VALID)) != FLCN_OK)
    {
        goto initHsEntry_exit;
    }

#endif    

initHsEntry_exit:
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    if (status != FLCN_OK)
    {
        sec2WriteStatusToMailbox0HS(status);
        SEC2_HALT();
    }
#endif    
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return status;
}
#endif // (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
