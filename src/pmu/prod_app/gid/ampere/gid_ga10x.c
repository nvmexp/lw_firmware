/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    gid_ga10x.c
 *          GID HAL Functions for GA10X and later GPUs
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_bus.h"
#include "dev_gsp.h"


/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "lwoslayer.h"
#include "scp_crypt.h"

#if UPROC_RISCV
#include "riscv_scp_internals.h"
#else
#include "scp_internals.h"
#endif // UPROC_RISCV

#include "config/g_pmu_private.h"
#include "scp_rand.h"
#include "scp_common.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*
 * Using macros for both KM and SALT instead of initializing global variables to ensure
 * this gets into code section.
 * TODO: Assign these to global variables and explicitly move into code section to make
 * it easier for future changes.
 */
#define DEVID_KM_DWORD_0 0x8502C30B
#define DEVID_KM_DWORD_1 0xEC5D952E
#define DEVID_KM_DWORD_2 0x6B982A89
#define DEVID_KM_DWORD_3 0x20D30714

#define DEVID_SALT_DWORD_0 0x59AAE274
#define DEVID_SALT_DWORD_1 0x7EE02B46

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initializing keys required for DEVID name decryption.
 * Two elements are generated from this function:
 * --> Encrypted devId
 * --> Derived key using devId and SALT
 * Please refer to https://confluence.lwpu.com/pages/viewpage.action?pageId=115943060
 */

FLCN_STATUS
pmuInitDevidKeys_GA10X(RM_PMU_DEVID_ENC_INFO *pDevidInfoOut)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       devId  = 0;
    LwU8        km[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        iv[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        devIdEnc[SCP_AES_SIZE_IN_BYTES * 2];
    LwU8        devIdDer[SCP_AES_SIZE_IN_BYTES * 2];
    LwU32      *pKm = NULL;
    LwU32      *pIv = NULL;
    LwU32      *pDevIdEnc = NULL;
    LwU32      *pDevIdDer = NULL;

    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        return pmuInitDevidKeys_GM20X(pDevidInfoOut);
    }

    // Required for correctness of ALIGN_UP below
    ct_assert(SCP_BUF_ALIGNMENT <= SCP_AES_SIZE_IN_BYTES);
    ct_assert(RM_PMU_DEVID_INFO_SIZE_BYTES == SCP_AES_SIZE_IN_BYTES);

    pKm       = PMU_ALIGN_UP_PTR(km, SCP_BUF_ALIGNMENT);
    pIv       = PMU_ALIGN_UP_PTR(iv, SCP_BUF_ALIGNMENT);
    pDevIdEnc = PMU_ALIGN_UP_PTR(devIdEnc, SCP_BUF_ALIGNMENT);
    pDevIdDer = PMU_ALIGN_UP_PTR(devIdDer, SCP_BUF_ALIGNMENT);

    // Read DEVID
    devId = Pmu.gpuDevId;

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

    // Using pDevIdEnc as both input and output buffer
    pDevIdEnc[0] = devId;
    pDevIdEnc[1] = 0;
    pDevIdEnc[2] = 0;
    pDevIdEnc[3] = 0;

    // Encrypt devId
    if ((flcnStatus = scpCbcCryptWithKey((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)pDevIdEnc,
             SCP_AES_SIZE_IN_BYTES, (LwU8*)pDevIdEnc, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub devId if encryption fails
        pDevIdEnc[0] = 0;
        goto pmuInitDevidKeys_GA10X_Exit;
    }

    // Get derived key using devId and salt. Using pDevIdDer for both input and output
    pDevIdDer[0] = DEVID_SALT_DWORD_0;
    pDevIdDer[1] = DEVID_SALT_DWORD_1;
    pDevIdDer[2] = devId;
    pDevIdDer[3] = 0;
    // Get derived key
    if ((flcnStatus = scpCbcCryptWithKey((LwU8*)pKm, LW_TRUE, LW_FALSE, (LwU8*)pDevIdDer,
             SCP_AES_SIZE_IN_BYTES, (LwU8*)pDevIdDer, (LwU8*)pIv)) != FLCN_OK)
    {
        // Scrub SALT if encryption fails
        pDevIdDer[0] = 0;
        pDevIdDer[1] = 0;
        pDevIdDer[2] = 0;
        goto pmuInitDevidKeys_GA10X_Exit;
    }

    memcpy(pDevidInfoOut->devidEnc, pDevIdEnc, RM_PMU_DEVID_INFO_SIZE_BYTES);
    memcpy(pDevidInfoOut->devidDerivedKey, pDevIdDer, RM_PMU_DEVID_INFO_SIZE_BYTES);

pmuInitDevidKeys_GA10X_Exit:
    // Scrub pKm
    memset(pKm, 0x0, SCP_AES_SIZE_IN_BYTES);
    return flcnStatus;
}

