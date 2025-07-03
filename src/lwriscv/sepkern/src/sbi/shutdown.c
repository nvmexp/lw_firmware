/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include <riscv_sbi.h>
#include "sepkern.h"
#include "engine.h"
#include "reg_access.h"

//! @brief  Shutdown sequence
void shutdown(void)
{
    // Disable S-mode interrupts
    csr_write(LW_RISCV_CSR_SIE, 0x0);

    // Disable M-mode interrupts
    csr_write(LW_RISCV_CSR_MIE, 0x0);

    // Make the RISCV core enter the HALT status
    while (LW_TRUE)
    {
        // Use csrwi because it's the only legal way to write MOPT
        csr_write_immediate(LW_RISCV_CSR_MOPT, LW_RISCV_CSR_MOPT_CMD_HALT);
    }
}

