/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/csr.h>
#include <lwriscv/peregrine.h>

#include "tests.h"
#include "util.h"

extern void trap_entry(void);
static void init(void)
{
    uint32_t dmemSize = 0;

    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;
    // configure print to the start of DMEM
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

#if ! defined LWRISCV_IS_ENGINE_minion
    // Configure FBIF mode
    localWrite(LW_PRGNLCL_FBIF_CTL,
                (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                 DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
#if ! defined LWRISCV_IS_ENGINE_lwdec
    // LWDEC doesn't have this register
    localWrite(LW_PRGNLCL_FBIF_CTL2,
                LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
#endif
#endif

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));

    csr_write(LW_RISCV_CSR_MTVEC, (uint64_t) trap_entry);
}

static uint64_t testManifestAD(void)
{

    printf("Checking if A/D pairs were written...");

    if (localRead(LW_PRGNLCL_FALCON_MAILBOX0) != MANIFEST_AD_TEST_MAILBOX0)
    {
        printf("LocalIo register pair write failed.\n");
        return MAKE_DEBUG_NOSUB(TestId_ADPair, 0, MANIFEST_AD_TEST_MAILBOX0, localRead(LW_PRGNLCL_FALCON_MAILBOX0));
    }

    if (localRead(LW_PRGNLCL_FALCON_MAILBOX1) != MANIFEST_AD_TEST_MAILBOX1)
    {
        printf("GlobalIo register pair write failed.\n");
        return MAKE_DEBUG_NOSUB(TestId_ADPair, 1, MANIFEST_AD_TEST_MAILBOX1, localRead(LW_PRGNLCL_FALCON_MAILBOX1));
    }

    printf("Ok\n");
    return PASSING_DEBUG_VALUE;
}


static const TestCallback c_test_callbacks[TestIdCount] = {
  [TestId_ADPair]    = testManifestAD,
  [TestId_Hwcfg]     = testMemConfig,
  [TestId_TcmRanges] = testMemAccess,
  [TestId_MemErrRead]= testReadMemErr,
  [TestId_Irq]       = testIrq_MM,
  [TestId_Cache]     = testCache,
  [TestId_Fence]     = testFence,
};

int main(void)
{
    init();
    printf("Testing...%s\n", ENGINE_NAME);

    // Clear trap recording values
    TRAP_CLEAR();
    bool pass = true;
    for(int i = 0; i < TestIdCount; i++) {
        if (c_test_callbacks[i])
        {
            uint64_t retval = c_test_callbacks[i]();
            if(retval != PASSING_DEBUG_VALUE) {
                csr_write(LW_RISCV_CSR_MSCRATCH, retval);
                pass = false;
                break;
            }
        }
    }
    if(pass) {
      csr_write(LW_RISCV_CSR_MSCRATCH, PASSING_DEBUG_VALUE);
    }

    if (csr_read(LW_RISCV_CSR_MSCRATCH) == PASSING_DEBUG_VALUE)
    {
        printf("Test PASSED.\n");
    } else
    {
        uint64_t r = csr_read(LW_RISCV_CSR_MSCRATCH);
        printf("Test FAILED. Test: %d Subtest: %d IDX: %d Exp: 0x%x Got: 0x%x\n",
               r >> 56,
               (r >> 48) & 0xFF,
               (r >> 40) & 0xFF,
               (r >> 16) & 0xFFFF,
               r & 0xFFFF);
    }
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    while(1);
    return 0;
}
