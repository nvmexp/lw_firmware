/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   pmu_gid.c
 * @brief  Generates a GPU ID using the ECID as a seed to the SHA-1 hash.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "dev_master.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objpmu.h"
#include "pmu_objecid.h"
#include "pmu_sha1.h"
#include "pmu_oslayer.h"

/* ------------------------- Prototypes ------------------------------------- */

static LwU32
s_GenerateGidSha1CopyFunc
(
    LwU8 *pBuff,
    LwU32 index,
    LwU32 size,
    void *pInfo
) GCC_ATTRIB_USED();

/*!
 * s_GenerateGidSha1CopyFunc - copy data from pInfo to pBuff
 *
 * The SHA1 implementation calls this helper function to request the
 * next block of input data.
 *
 * @param[out]  pBuff   The buffer to copy the new data to.
 * @param[in]   index   The desired offset to begin copying from.
 * @param[in]   size    The requested number of bytes to be copied.
 * @param[in]   info    Pointer to the data passed into GenerateSha1 as pData.
 *
 * @return The actual number of bytes copied into the buffer.
 */
static LwU32
s_GenerateGidSha1CopyFunc
(
    LwU8 *pBuff,
    LwU32 index,
    LwU32 size,
    void *pInfo
)
{
    LwU8 *pBytes = pInfo;
    memcpy(pBuff, pBytes + index, size);
    return size;
}

/*!
 * pmuGenerateGid - generate the GPU ID from the ECID, PMC_BOOT_0, and
 * PMC_BOOT_42.  Write the resulting GPU ID into the region of DMEM
 * managed by RM so that RM can read it after PMU initialization.
 */
void
pmuGenerateGid(void)
{
    typedef struct
    {
        RM_PMU_INIT_MSG_EXTRA_DATA data;
        LwU8                       digest[LW_PMU_SHA1_DIGEST_SIZE];
        struct
        {
            LwU64 ecid[2];   // GPU ECID value
            LwU32 boot0Val;  // LW_PMC_BOOT_0 value (Chip ID information)
            LwU32 boot42Val; // LW_PMC_BOOT_42 value (Chip ID information)
        } seed;
    } GID_CONTEXT;

    FLCN_STATUS  status;
    GID_CONTEXT  gidCtx;

    //
    // if ECID is not enabled, something is very wrong
    //
    if (!PMUCFG_ENGINE_ENABLED(ECID))
    {
        return;
    }

    //
    // construct the ecid HAL
    //
    constructEcid();

    //
    // query the ECID and write it into the GID seed
    //
    status = ecidGetData_HAL(&Ecid, (LwU64 *)gidCtx.seed.ecid);

    if (status != FLCN_OK)
    {
        return;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(sha1));
    {    
        //
        // Generate the signature; use GID_CONTEXT::digest as scratch space to
        // receive the entire SHA-1 digest, and then copy the first four
        // bytes into pGidCtx->data.signature.  Note this signature is simply
        // a handshake to confirm that the SHA-1 implementation is working; that
        // the seed is such an obvious string is not a security concern.
        //
        pmuSha1(gidCtx.digest, (LwU8 *)"Lwpu", 6, s_GenerateGidSha1CopyFunc);
    
        memcpy(gidCtx.data.signature, gidCtx.digest,
               sizeof(gidCtx.data.signature));
    
        //
        // populate the rest of the GPU ID seed
        //
        gidCtx.seed.boot0Val  = REG_RD32(BAR0, LW_PMC_BOOT_0);
        gidCtx.seed.boot42Val = REG_RD32(BAR0, LW_PMC_BOOT_42);
    
        //
        // generate the GPU ID; note that pGidCtx->data.gidData only uses
        // the first 16 bytes of the SHA-1 digest (UUID strings only use the
        // first 16 of the 20-byte SHA-1 digest), so generate the hash
        // into GID_CONTEXT::digest and then copy into pGidCtx->data.gidData
        //
        pmuSha1(gidCtx.digest, &(gidCtx.seed), sizeof(gidCtx.seed),
                s_GenerateGidSha1CopyFunc);
        memcpy(gidCtx.data.gidData, gidCtx.digest,
               sizeof(gidCtx.data.gidData));
    
    
        // Fetch DEVID enc info
        if (FLCN_OK != pmuInitDevidKeys_HAL(&pPmu, &(gidCtx.data.devidInfo)))
        {
            // Continue even if failed.
            PMU_BREAKPOINT();
        }
    }
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(sha1));

    status = ssurfaceWr(&(gidCtx.data),
        LW_OFFSETOF(RM_PMU_SUPER_SURFACE, initMsgExtraData),
        sizeof(RM_PMU_INIT_MSG_EXTRA_DATA));

    if (status != FLCN_OK)
    {
        return;
    }
}
