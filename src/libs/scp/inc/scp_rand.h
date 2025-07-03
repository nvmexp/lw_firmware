/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_RAND_H
#define SCP_RAND_H

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "lwostask.h"
#include "scp_common.h"
#include "scp_internals.h"
/* ------------------------ Defines ----------------------------------------- */
#define SCP_RAND_NUM_SIZE_IN_BITS   (128)
#define SCP_RAND_NUM_SIZE_IN_BYTES  (128 / 8)
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// TODO: lvaz: Decide whether to leave this as inline or move it into a suitable overlay

// 'osDmemAddressVa2PaGet( )' is not defined for UPROC => riscv
#ifndef UPROC_RISCV 
static inline void scpGetRand128Inline(LwU8 *pData)
    GCC_ATTRIB_ALWAYSINLINE();

static inline void scpGetRand128Inline
(
    LwU8 *pData
)
{
    LwU32  dataPa;

    // Make sure we're not context-switched while using SCP registers
    lwrtosENTER_CRITICAL();
    {
        dataPa = osDmemAddressVa2PaGet(pData);

        falc_scp_trap(TC_INFINITY);

        // improve RNG quality by collecting entropy across
        // two conselwtive random numbers
        falc_scp_rand(SCP_R4);
        falc_scp_rand(SCP_R3);

        // use AES cipher to whiten random data
        falc_scp_key(SCP_R4);
        falc_scp_encrypt(SCP_R3, SCP_R3);

        // retrieve random data and update rand_ctx
        falc_trapped_dmread(falc_sethi_i(dataPa, SCP_R3));
        falc_dmwait();

        // Reset trap to 0
        falc_scp_trap(0);
    }
    lwrtosEXIT_CRITICAL();
}
#endif

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

void scpStartRand(void)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "startRand");
#else /* ON_DEMAND_PAGING_BLK */
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "startRand");
#endif /* ON_DEMAND_PAGING_BLK */

void scpStopRand(void)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "stopRand");
#else /* ON_DEMAND_PAGING_BLK */
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "stopRand");
#endif /* ON_DEMAND_PAGING_BLK */

void scpGetRand128(LwU8 *pData)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "getRand128");
#else /* ON_DEMAND_PAGING_BLK */
    GCC_ATTRIB_SECTION("imem_libScpRandHs", "getRand128");
#endif /* ON_DEMAND_PAGING_BLK */

#endif // SCP_RAND_H
