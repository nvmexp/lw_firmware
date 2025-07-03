/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main.c
 * @brief  Test application main.
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <dev_fbif_v4.h>
#include <dev_fbif_addendum.h>
#include <lw_riscv_address_map.h>
#include "io.h"
#include "csr.h"
#include "debug.h"

volatile LwU32 buf[20][4];
#define BASE LW_RISCV_AMAP_EMEM_START
// Way behind end of memory
#define OFF LW_RISCV_AMAP_EMEM_SIZE/2

void exception2(void)
{
    enterIcd();
}

void exception(void)
{
    csr_write(LW_RISCV_CSR_MTVEC, (LwU64) exception2);

    printf("Exception %llx at %p val %llx!\n",
           csr_read(LW_RISCV_CSR_MCAUSE),
           csr_read(LW_RISCV_CSR_MEPC),
           csr_read(LW_RISCV_CSR_MTVAL));

    enterIcd();
}

int main()
{
    // configure fbif
    bar0Write(ENGINE_REG(_FBIF_CTL),
                  (DRF_DEF(_PFALCON_FBIF, _CTL, _ENABLE, _TRUE) |
                   DRF_DEF(_PFALCON_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    bar0Write(ENGINE_REG(_FBIF_CTL2),
                 LW_PFALCON_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
    riscvWrite(LW_PRISCV_RISCV_DBGCTL, 0xFFFF);
    csr_write(LW_RISCV_CSR_MDBGCTL, 1);
    csr_write(LW_RISCV_CSR_MTVEC, (LwU64) exception);
    bar0Write(ENGINE_REG(_FALCON_LOCKPMB), 0);
    riscvWrite(LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN, 0);
    printf("GA10X RISCV testapp...\n");

    enterIcd();
    return 0;
}
