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
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <lwmisc.h>
#include "io.h"
#include "lwriscv/fence.h"
#include "common.h"
#include <lwriscv/csr.h>

/********* WARNING
 * How to setup GSC carveout on fpga:
https://confluence.lwpu.com/display/~yichenc/GSC+carveout+for+Orin+TSEC
https://lwtegra/home/tegra_manuals/html/manuals/t234/armc.html

# GSC4
# 0x2c10cfc MC_SELWRITY_CARVEOUT4_BOM_0 - base address
reg(0x2c10cfc, 0xe9600000)
# 02c10cf8 MC_SELWRITY_CARVEOUT4_CFG0_0 - config - see confluence
reg(0x2c10cf8, 0x4002444)
# MC_SELWRITY_CARVEOUT22_SIZE_128KB_0[0x02c10d04] - size (1MB)
reg(0x2c10d04, 0x00000008)
# MC_SELWRITY_CARVEOUT22_CLIENT_ACCESS2_0[0x2c10d10] - engines that can access, tsec only
reg(0x2c10d10, 0x300000)

# GSC12
# MC_SELWRITY_CARVEOUT12_BOM_0[0x2c121e4]
reg(0x2c121e4, 0xe9200000)
# MC_SELWRITY_CARVEOUT12_CFG0_0[0x2c121e0]
reg(0x2c121e0, 0x40067f8)
# MC_SELWRITY_CARVEOUT12_SIZE_128KB_0[0x2c121ec]
reg(0x2c121ec, 0x00000008)
# MC_SELWRITY_CARVEOUT12_CLIENT_ACCESS2_0[0x2c121f8] tsec
reg(0x2c121f8, 0x300000)
# MC_SELWRITY_CARVEOUT12_CLIENT_ACCESS2_0[0x2c12200] SE
reg(0x2c12200, 0x000003)

# GSC22 (david), prevent anyone
# MC_SELWRITY_CARVEOUT22_BOM_0[0x02c12504]
reg(0x02c12504, 0xe9100000)
# MC_SELWRITY_CARVEOUT22_CFG0_0[0x02c12500]
reg(0x02c12500, 0x40067f8)
# MC_SELWRITY_CARVEOUT22_SIZE_128KB_0[0x02c1250c]
reg(0x02c1250c, 0x00000008)
# MC_SELWRITY_CARVEOUT22_CLIENT_ACCESS2_0[0x2c12518], just block all
reg(0x2c12518, 0)
*/

typedef enum TestErrorCode {
    TestErrorCode_Bare_Fetch_Incorrect = 0xFF,
    TestErrorCode_Bare_LdSt_Incorrect = 0xFE,
    TestErrorCode_MPU_Fetch_Incorrect = 0xFD,
    TestErrorCode_MPU_LdSt_Incorrect = 0xFC,
} TestErrorCode;

typedef enum {
    TestId_BareFetch,
    TestId_BareLoadStore,
    TestId_MpuFetch,
    TestId_MpuLoadStore,
    TestIdCount,
} TestId;

//#define RUN_ONE_TEST TestId_BareLoadStore

#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))

extern volatile uint64_t mask_mepc;
extern volatile uint64_t trap_mepc;
extern volatile uint64_t trap_mcause;
extern volatile uint64_t trap_mtval;
extern void try_fetch_int(uintptr_t addr);

static const char *mcauses_text[] = {
    "Instr misaligned",
    "Instr access fault",
    "Illegal instr",
    "Breakpoint",
    "Load misaligned",
    "Load access fault",
    "Store misaligned",
    "Store access fault",
    "ECALL U",
    "ECALL S",
    "RES10",
    "ECALL M",
    "Instr page fault",
    "Load page fault",
    "RES14",
    "Store page fault",
    0
};

#define TRAP_CLEAR() do { \
  trap_mepc = trap_mtval = trap_mcause = 0; \
} while(0)

#define MAKE_DEBUG_VALUE(CODE, IDX, EXPECTVAL, GOTVAL) (\
  ((uint64_t)(CODE) << 56) | \
  ((uint64_t)(IDX) << 40) | \
  ((uint64_t)(EXPECTVAL) << 16) | \
  ((uint64_t)(GOTVAL)) \
)

#define PASSING_DEBUG_VALUE 0xCAFECAFECAFECAFEULL

#define MPRV_ENABLE(MODE) do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    csr_set(LW_RISCV_CSR_MSTATUS, (MODE) | \
                                  DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE)); \
    __asm__ volatile("fence.i"); \
} while(0);

#define MPRV_DISABLE() do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    __asm__ volatile("fence.i"); \
} while(0);

#define PLM_L0 LW_RISCV_CSR_MRSP_MRPL_LEVEL0
#define PLM_L2 LW_RISCV_CSR_MRSP_MRPL_LEVEL2
#define PLM_L3 LW_RISCV_CSR_MRSP_MRPL_LEVEL3

static void setPlm(int m, int s, int u)
{
    // SRSP and RSP are just copies of MRSP
    csr_write(LW_RISCV_CSR_MRSP,
              DRF_NUM(_RISCV_CSR, _MRSP, _MRPL, m) |
              DRF_NUM(_RISCV_CSR, _MRSP, _SRPL, s) |
              DRF_NUM(_RISCV_CSR, _MRSP, _URPL, u)
              );
}

static void setPlmAll(int l)
{
    setPlm(l, l, l);
}

static void setBareGsc(int fetch, int load)
{
    csr_write(LW_RISCV_CSR_MFETCHATTR,
              DRF_NUM(_RISCV_CSR, _MFETCHATTR, _WPR, fetch));
    csr_write(LW_RISCV_CSR_MLDSTATTR,
              DRF_NUM(_RISCV_CSR, _MLDSTATTR, _WPR, load));
}

#define ADDR_PHYS(addr) (LW_RISCV_AMAP_EXTMEM1_START | (2ull<<40) | (addr))
#define ADDR_VIRT(addr) ((addr))
#define ADDR_ILWALID_VIRT(addr) ((addr) + 0x10000000)
#define BASE_GSC4     0xe9600000
#define BASE_GSC12    0xe9200000
#define BASE_GSC22    0xe9100000
// We may need to change this address if we fail tests (or worse start exelwting..)
#define OFFSET_GSC4   (2 * 1024) // GSC4 is 1MB
#define OFFSET_GSC12  (2 * 1024) // GSC12 is 1MB
#define OFFSET_GSC22  (2 * 1024) // GSC22 is 1MB


static bool tryFetch(uintptr_t address)
{
    bool bSuccess;
    printf("IF 0x%llx: ", address);
    TRAP_CLEAR();
    try_fetch_int(address);
    // 2 == illegal instruction, is what we expect.. but GSC read/fetch failures
    // are not reported to risc-v, instead we get 0xFFFF_FFFF and invalid instruction
    bSuccess = ((trap_mcause == 2) && (trap_mtval != 0xFFFFFFFF));
    printf("%s\n", bSuccess ? "Success" : "Failure");
    return bSuccess;
}

// Must do combined load-store because errors are not reported :(
static bool tryLoadStore(uintptr_t address)
{
    volatile uint32_t *ptr = (volatile uint32_t*)address;
    uint32_t prev, tmp;

    printf("LDST 0x%llx: ", address);
    TRAP_CLEAR();
    prev = ptr[0];
    riscvLwfenceRWIO();
    if (trap_mcause != 0)
        goto err;
    TRAP_CLEAR();
    ptr[0] = ~prev;
    riscvLwfenceRWIO();
    if (trap_mcause != 0)
        goto err;
    // Read back, we'll hit error if we always get the same ret code like 0xdeadsec or 0xffffff...
    TRAP_CLEAR();
    tmp = ptr[0];
    riscvLwfenceRWIO();
    if ((trap_mcause != 0) || (tmp != ~prev))
        goto err;

    TRAP_CLEAR();
    ptr[0] = prev;
    riscvLwfenceRWIO();
    if (trap_mcause != 0)
        goto err;
    printf("Success\n");
    return true;
err:
    printf("Failure\n");
    return false;
}

static uint64_t testReturnCode = PASSING_DEBUG_VALUE;
static void logFail(uint64_t code)
{
    printf("F: 0x%llx @0x%llx val 0x%llx\n", code, trap_mepc, trap_mtval);
    if (testReturnCode == PASSING_DEBUG_VALUE) // log first error
        testReturnCode = code;
}


static void mToS(void)
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _SUPERVISOR, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}

static void mToU(void)
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);

    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _USER, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}
#define sToM() do { __asm__ volatile ("ecall"); } while(0)
#define uToM() do { __asm__ volatile ("ecall"); } while(0)

static void writeMpuEntry(int idx, uint64_t pa, uint64_t va, uint64_t attr, uint64_t range)
{
    csr_write(LW_RISCV_CSR_SMPUIDX, idx);
    csr_write(LW_RISCV_CSR_SMPUATTR, attr);
    csr_write(LW_RISCV_CSR_SMPURNG, range);
    csr_write(LW_RISCV_CSR_SMPUPA, pa);
    csr_write(LW_RISCV_CSR_SMPUVA, va | DRF_NUM64(_RISCV,_CSR_SMPUVA,_VLD, 1));
}

void enableMpu(void)
{
    // Enable mpu
    csr_set(LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU));
    __asm__ volatile ("fence.i");
}

void disableMpu(void)
{
    // Enable mpu
    csr_set(LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _BARE));
    __asm__ volatile ("fence.i");
}

static void setupMpu(void)
{
    uint64_t attr =   DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1) |
            DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
            DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
            DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
            DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
            DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1);

    disableMpu();
    // MPU mappings - valid GSC:
    // gsc12
    writeMpuEntry(0, ADDR_PHYS(BASE_GSC12), ADDR_VIRT(BASE_GSC12), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 12), 1024 * 1024);
    // gsc22
    writeMpuEntry(1, ADDR_PHYS(BASE_GSC22), ADDR_VIRT(BASE_GSC22), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 22), 1024 * 1024);
    // gsc4
    writeMpuEntry(2, ADDR_PHYS(BASE_GSC4), ADDR_VIRT(BASE_GSC4), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 4), 1024 * 1024);
    // MPU mappings - GSC mismatch (map incorrect GSC number)
    // gsc12
    writeMpuEntry(3, ADDR_PHYS(BASE_GSC12), ADDR_ILWALID_VIRT(BASE_GSC12), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 4), 1024 * 1024);
    // gsc22
    writeMpuEntry(4, ADDR_PHYS(BASE_GSC22), ADDR_ILWALID_VIRT(BASE_GSC22), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 12), 1024 * 1024);
    // gsc4
    writeMpuEntry(5, ADDR_PHYS(BASE_GSC4), ADDR_ILWALID_VIRT(BASE_GSC4), attr | DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, 22), 1024 * 1024);

    // whole address map
    writeMpuEntry(6, 0, 0, attr, 0xFFFFFFFFFFFFFFFFULL);

}

static uint64_t testBareFetch(void)
{
    bool ret;

    printf("Bare fetch test\n");
    disableMpu();

    ////////////////////////////////////////////////////////////////////////////
    // Bare fetch, GSC 12, should pass
    setPlmAll(PLM_L3);
    setBareGsc(12, 12);
    ret = tryFetch(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 0, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Bare fetch, GSC 22, should not pass, machine
    setBareGsc(22, 22);
    ret = tryFetch(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 1, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Bare fetch, privilege mismatch, machine
    setPlmAll(PLM_L2);
    setBareGsc(4, 4);
    ret = tryFetch(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 2, 0, trap_mcause));

    // Bare fetch, privilege mismatch, user
    mToU();
    ret = tryFetch(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 3, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Bare fetch, correct priv, machine
    setPlm(PLM_L3, PLM_L0, PLM_L0);
    setBareGsc(4, 4);
    ret = tryFetch(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 4, 0, trap_mcause));

    // Bare fetch, correct priv, S
    setPlm(PLM_L0, PLM_L3, PLM_L0);
    mToS();
    ret = tryFetch(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    sToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 5, 0, trap_mcause));

    // Bare fetch, correct priv, U
    setPlm(PLM_L0, PLM_L0, PLM_L3);
    mToU();
    ret = tryFetch(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    uToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 6, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Access to other engines GSC, should not pass, user
    setPlmAll(PLM_L3);
    setBareGsc(22, 22);
    ret = tryFetch(ADDR_PHYS(BASE_GSC22 + OFFSET_GSC22));
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 7, 0, trap_mcause));

    mToU();
    ret = tryFetch(ADDR_PHYS(BASE_GSC22 + OFFSET_GSC22));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_Fetch_Incorrect, 8, 0, trap_mcause));

    return PASSING_DEBUG_VALUE;
} // ok with the table

static uint64_t testBareLoadStore(void)
{
    bool ret;

    printf("Bare load/store test.\n");
    disableMpu();

    ////////////////////////////////////////////////////////////////////////////
    // Bare load, GSC 12, should pass, M
    setPlm(PLM_L2, PLM_L0, PLM_L0);
    setBareGsc(0, 12);
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 0, 0, trap_mcause));
    // S
    setPlm(PLM_L0, PLM_L2, PLM_L0);
    mToS();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 1, 0, trap_mcause));
    // U
    setPlm(PLM_L0, PLM_L0, PLM_L2);
    mToU();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 2, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Bare load, mismatch GSC (22), should not pass
    setPlm(PLM_L2, PLM_L0, PLM_L0);
    setBareGsc(12, 0);
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 3, 0, trap_mcause));
    // S
    setPlm(PLM_L0, PLM_L2, PLM_L0);
    mToS();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 4, 0, trap_mcause));
    // U
    setPlm(PLM_L0, PLM_L0, PLM_L2);
    mToU();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 5, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Bare load, privilege mismatch
    setPlm(PLM_L0, PLM_L2, PLM_L2);
    setBareGsc(0, 4);
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 6, 0, trap_mcause));
    // S
    setPlm(PLM_L2, PLM_L0, PLM_L2);
    mToS();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 7, 0, trap_mcause));
    // U
    setPlm(PLM_L2, PLM_L2, PLM_L0);
    mToU();
    ret = tryLoadStore(ADDR_PHYS(BASE_GSC4 + OFFSET_GSC4));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_Bare_LdSt_Incorrect, 8, 0, trap_mcause));

    setPlmAll(PLM_L3);

    return PASSING_DEBUG_VALUE;
} // ok with the table


static uint64_t testMpuFetch(void)
{
    bool ret;

    printf("MPU fetch test\n");
    enableMpu();
    setPlmAll(PLM_L3);

    ////////////////////////////////////////////////////////////////////////////
    // Fetch, GSC 12, should pass
    setBareGsc(0, 0);
    mToS();
    ret = tryFetch(ADDR_VIRT(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 0, 0, trap_mcause));
    mToU();
    ret = tryFetch(ADDR_VIRT(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 1, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Fetch, GSC ID mismatch, should not pass
    setBareGsc(12, 12);
    mToS();
    ret = tryFetch(ADDR_ILWALID_VIRT(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 2, 0, trap_mcause));
    mToU();
    ret = tryFetch(ADDR_ILWALID_VIRT(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 3, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Privilege mismatch
    setPlm(PLM_L3, PLM_L0, PLM_L3);
    setBareGsc(4, 4);
    mToS();
    ret = tryFetch(ADDR_VIRT(BASE_GSC4 + OFFSET_GSC4));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 4, 0, trap_mcause));
    setPlm(PLM_L3, PLM_L3, PLM_L0);
    mToU();
    ret = tryFetch(ADDR_VIRT(BASE_GSC4 + OFFSET_GSC4));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 5, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // Access to other GSC, should not pass
    setPlmAll(PLM_L3);
    mToS();
    ret = tryFetch(ADDR_VIRT(BASE_GSC22 + OFFSET_GSC22));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 6, 0, trap_mcause));
    mToU();
    ret = tryFetch(ADDR_VIRT(BASE_GSC22 + OFFSET_GSC22));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_Fetch_Incorrect, 7, 0, trap_mcause));

    return PASSING_DEBUG_VALUE;
}

static uint64_t testMpuLoadStore(void)
{

    bool ret;

    printf("MPU load/store test.\n");
    enableMpu();

    ////////////////////////////////////////////////////////////////////////////
    // Bare load, GSC 12, should pass
    setPlm(PLM_L0, PLM_L2, PLM_L0);
    setBareGsc(0, 0);
    // S
    mToS();
    ret = tryLoadStore(ADDR_VIRT(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 0, 0, trap_mcause));
    // U
    setPlm(PLM_L0, PLM_L0, PLM_L2);
    mToU();
    ret = tryLoadStore(ADDR_VIRT(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret != true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 1, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // MPU load, mismatch GSC (22), should not pass
    setPlm(PLM_L0, PLM_L3, PLM_L0);
    setBareGsc(4,4);
    // S
    mToS();
    ret = tryLoadStore(ADDR_ILWALID_VIRT(BASE_GSC12 + OFFSET_GSC12));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 2, 0, trap_mcause));
    // U
    setPlm(PLM_L0, PLM_L0, PLM_L3);
    mToU();
    ret = tryLoadStore(ADDR_ILWALID_VIRT(BASE_GSC12 + OFFSET_GSC12));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 3, 0, trap_mcause));

    ////////////////////////////////////////////////////////////////////////////
    // MPU load, privilege mismatch
    setPlm(PLM_L3, PLM_L0, PLM_L3);
    setBareGsc(0, 0);
    // S
    mToS();
    ret = tryLoadStore(ADDR_VIRT(BASE_GSC4 + OFFSET_GSC4));
    sToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 4, 0, trap_mcause));
    // U
    setPlm(PLM_L3, PLM_L3, PLM_L0);
    mToU();
    ret = tryLoadStore(ADDR_VIRT(BASE_GSC4 + OFFSET_GSC4));
    uToM();
    if (ret == true)
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_MPU_LdSt_Incorrect, 5, 0, trap_mcause));

    setPlmAll(PLM_L3);

    return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
typedef uint64_t (*TestCallback)(void);
static const TestCallback c_test_callbacks[TestIdCount] = {
    [TestId_BareFetch] = testBareFetch,
    [TestId_BareLoadStore] = testBareLoadStore,
    [TestId_MpuFetch] = testMpuFetch,
    [TestId_MpuLoadStore] = testMpuLoadStore,
};

//#define RUN_ONE_TEST MpuTestId_Parallel

void pseudoBrom(void)
{
  localWrite(LW_PRGNLCL_RISCV_DBGCTL, 0x1FFFF);

  csr_write(LW_RISCV_CSR_MDBGCTL, 0x1);
  csr_write(LW_RISCV_CSR_PMPADDR(0), 0x000000000004FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(1), 0x000000000006FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(2), 0x000000000048FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(3), 0x00000000004AFFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(4), 0x7FFFFFFFFFFFFFFF);
  csr_write(LW_RISCV_CSR_PMPCFG0, 0x0f1b1b1b1f);

  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(0), 0x8BBBBBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(1), 0xBBB9BBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(2), 0xBBBBBBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(3), 0xB);

  // configure fbif
  #ifdef LW_PRGNLCL_FBIF_CTL
  localWrite(ENGINE_REG(_FBIF_CTL), (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                                      DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
  localWrite(ENGINE_REG(_FBIF_CTL2), LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
  #endif

  localWrite(ENGINE_REG(_FALCON_LOCKPMB), 0);
  localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN, 0);

  // Permit L3 access because we can
  csr_write(LW_RISCV_CSR_MSPM,
            DRF_DEF(_RISCV_CSR, _MSPM, _MPLM, _LEVEL3) |
            DRF_DEF(_RISCV_CSR, _MSPM, _SPLM, _LEVEL3) |
            DRF_DEF(_RISCV_CSR, _MSPM, _UPLM, _LEVEL3)
            );
  csr_write(LW_RISCV_CSR_SSPM,
            DRF_DEF(_RISCV_CSR, _SSPM, _SPLM, _LEVEL3) |
            DRF_DEF(_RISCV_CSR, _SSPM, _UPLM, _LEVEL3)
            );
  csr_write(LW_RISCV_CSR_SPM,
            DRF_DEF(_RISCV_CSR, _SPM, _UPLM, _LEVEL3)
            );
}

int main() {

  pseudoBrom();
  printInit(2048);
  setupMpu();

  // Clear trap recording values
  TRAP_CLEAR();
  testReturnCode = 0;
#ifdef RUN_ONE_TEST
  c_test_callbacks[RUN_ONE_TEST]();
  csr_write(LW_RISCV_CSR_MSCRATCH, testReturnCode);
#else
  for(int i = 0; i < TestIdCount; i++) {
      if (c_test_callbacks[i])
      {
          c_test_callbacks[i]();
      }
  }
  csr_write(LW_RISCV_CSR_MSCRATCH, testReturnCode);
  printf("All tests finished with result 0x%llx\n", csr_read(LW_RISCV_CSR_MSCRATCH));
#endif
  localWrite(LW_PRGNLCL_RISCV_ICD_CMD, LW_PRGNLCL_RISCV_ICD_CMD_OPC_STOP);
    __asm__ volatile("fence");
  return 0;
}
