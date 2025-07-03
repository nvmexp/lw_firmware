/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "rand.h"

#include <stddef.h>
#include "riscv_scp_internals.h"

static inline void scpStartRand(void)
{
    LwU32 val;
    // Dont use riscv_stx if you are already providing PGSP address
    val = DRF_NUM_SCP(_RNDCTL0, _HOLDOFF_INIT_LOWER, 0xf);
    bar0Write(SCP_REG(_RNDCTL0), val);
    val = DRF_NUM_SCP(_RNDCTL1, _HOLDOFF_INTRA_MASK, 0xf);
    bar0Write(SCP_REG(_RNDCTL1), val);
    val = DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_A, 0x0007) |
          DRF_NUM_SCP(_RNDCTL11, _AUTOCAL_STATIC_TAP_B, 0x0007);
    bar0Write(SCP_REG(_RNDCTL11), val);
    val = DRF_DEF_SCP(_CTL1, _RNG_EN, _ENABLED);
    bar0Write(SCP_REG(_CTL1), val);
}

static inline void scpStopRand(void)
{
    bar0Write(SCP_REG(_CTL1), 0);
}

static inline void scpGetRand128(volatile LwU8 *pData)
{
    LwUPtr physAddr;
    // load rand_ctx to SCP
    physAddr = ((LwUPtr) pData) - LW_RISCV_AMAP_DMEM_START;
    riscv_dmwrite(physAddr, SCP_R5);
    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    riscv_scp_rand(SCP_R4);
    riscv_scp_rand(SCP_R3);
    riscv_scpWait();

    // mixing in previous whitened rand data
    riscv_scp_xor(SCP_R5, SCP_R4);
    // use AES cipher to whiten random data
    riscv_scp_key(SCP_R4);
    riscv_scp_encrypt(SCP_R3, SCP_R3);
    // retrieve random data and update rand_ctx
    riscv_dmread(physAddr, SCP_R3);
    riscv_scpWait();
}

LwU64 rand64(void)
{
    volatile LwU8 scpBuffer[SCP_AES_SIZE_IN_BYTES + SCP_BUF_ALIGNMENT - 1];
    volatile LwU8 *pScpBuffer = (LwU8 *) LW_ALIGN_UP((LwUPtr) scpBuffer, (LwUPtr) SCP_BUF_ALIGNMENT);
    scpStartRand();
    scpGetRand128(pScpBuffer);
    scpStopRand();
    return *(LwU64 *) pScpBuffer;
}
