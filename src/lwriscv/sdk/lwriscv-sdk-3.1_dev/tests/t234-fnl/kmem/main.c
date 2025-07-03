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

typedef enum TestErrorCode {
    TestErrorCode_KMEMConfig= 0xFF,
    TestErrorCode_KMEM_DUMP = 0xFE,
    TestErrorCode_KDF= 0xFD,
    TestErrorCode_DICE= 0xFC,
    TestErrorCode_KMEM_PROT = 0xFB,
} TestErrorCode;

typedef enum {
    TestId_KMEM_Config,
    TestId_KMEM_DUMP,
    TestId_KDF,
    TestId_DICE,
    TestId_KMEM_PROT,
    TestIdCount,
} TestId;


//#define RUN_ONE_TEST TestId_KMEM_DUMP

#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))

extern volatile uint64_t mask_mepc;
extern volatile uint64_t trap_mepc;
extern volatile uint64_t trap_mcause;
extern volatile uint64_t trap_mtval;
extern void try_fetch_int(uintptr_t addr);

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
#define PLM_L1 LW_RISCV_CSR_MRSP_MRPL_LEVEL1
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

void *memset(void *s, int c, unsigned n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (uint8_t)c;
    return s;
}

// returns true if MEMERR was assert
static bool checkCleanMemerr(void)
{
    uint32_t reg = localRead(LW_PRGNLCL_FALCON_IRQSTAT);
    localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, 0);
    return FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _MEMERR, _TRUE, reg);
}

static uint32_t kmem_numslot, kmem_slot_len, kmem_protinfo_len, kmem_brom_slots;

static uint64_t testKmemConfig(void)
{
    int i;
    uint32_t reg;

    printf("KMEM test.\n");

    reg = localRead(LW_PRGNLCL_FALCON_HWCFG2);

    if (!FLD_TEST_DRF(_PRGNLCL, _FALCON_HWCFG2, _KMEM, _ENABLE, reg))
        logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEMConfig, 0, 0, reg));

    printf("KMEM PLM: 0x%x\n", localRead(LW_PRGNLCL_FALCON_KMEM_PRIV_LEVEL_MASK));
    printf("KMEM Base: 0x%x\n", localRead(LW_PRGNLCL_FALCON_KMEMBASE));
    printf("KMEM CFGz: 0x%x: %u slots x %u bytes, prot %u bytes, %u brom slots.\n",
           reg, kmem_numslot, kmem_slot_len, kmem_protinfo_len, kmem_brom_slots);
    printf("KMEM Slot mask: 0x%x\n", localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK));

    printf("KMEM Valid ");
    for (i=0; i<LW_PRGNLCL_FALCON_KMEM_VALID__SIZE_1; ++i)
    {
        printf("%x ", localRead(LW_PRGNLCL_FALCON_KMEM_VALID(i)));
    }
    printf("\n");

    return PASSING_DEBUG_VALUE;
}

static bool keyValid(int no)
{
    int slot;

    slot = no / 32;
    no = no % 32;

    if (slot > LW_PRGNLCL_FALCON_KMEM_VALID__SIZE_1)
        return false;
    return ((1 << no) & localRead(LW_PRGNLCL_FALCON_KMEM_VALID(slot))) != 0;
}

// offset in bytes, not words
static bool readKMEM(uint32_t offset, uint32_t *val)
{
    uint32_t v;

    localWrite(LW_PRGNLCL_FALCON_KMEMC(0),
               DRF_NUM(_PRGNLCL_FALCON, _KMEMC, _OFFS, offset >> 2));

    v = localRead(LW_PRGNLCL_FALCON_KMEMD(0));

    if (val)
        *val = v;

    return FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEMC, _ERR, _SUCCESS, localRead(LW_PRGNLCL_FALCON_KMEMC(0)));
}

static bool writeKMEM(uint32_t offset, uint32_t val)
{
    localWrite(LW_PRGNLCL_FALCON_KMEMC(0),
               DRF_NUM(_PRGNLCL_FALCON, _KMEMC, _OFFS, offset >> 2));

    localWrite(LW_PRGNLCL_FALCON_KMEMD(0), val);

    return FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEMC, _ERR, _SUCCESS, localRead(LW_PRGNLCL_FALCON_KMEMC(0)));
}


static bool readKeySlotProt(int no, uint64_t *val)
{
    uint32_t *v = (uint32_t*)val;

    return readKMEM(0x1000 + no * 8, &v[0]) &&
           readKMEM(0x1000 + no * 8 + 4, &v[1]);
}

static bool writeKeySlotProt(int no, uint64_t val)
{
    printf("Write keyslot: %i @ 0x%x <= 0x%llx\n", no, 0x1000 + no*8, val);
    return writeKMEM(0x1000 + no * 8, LwU64_LO32(val)) &&
           writeKMEM(0x1000 + no * 8 + 4, LwU64_HI32(val));
}

static bool disposeKeySlot(int no)
{
    uint32_t reg;
    localWrite(LW_PRGNLCL_FALCON_KMEM_CTL,
               DRF_DEF(_PRGNLCL_FALCON, _KMEM_CTL, _CMD, _DISPOSE) |
               DRF_NUM(_PRGNLCL_FALCON, _KMEM_CTL, _IDX, no));
    reg = localRead(LW_PRGNLCL_FALCON_KMEM_CTL);
    if (!FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEM_CTL, _RESULT, _DISPOSE_DONE, reg))
        printf("Dispose failed for slot %i.\n", no);

    return FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEM_CTL, _RESULT, _DISPOSE_DONE, reg);
}

static bool readKeySlot(int no, uint32_t *key)
{
    return readKMEM(no * 16, &key[0]) &&
           readKMEM(no * 16 + 4, &key[1]) &&
           readKMEM(no * 16 + 8, &key[2]) &&
           readKMEM(no * 16 + 12, &key[3]);
}

static bool writeKeySlot(int no, uint32_t *key)
{
    bool ret = writeKMEM(no * 16, key[0]) &&
           writeKMEM(no * 16 + 4, key[1]) &&
           writeKMEM(no * 16 + 8, key[2]) &&
           writeKMEM(no * 16 + 12, key[3]);

    if (!ret)
        printf("Keyslot programming failed: 0x%x\n", (localRead(LW_PRGNLCL_FALCON_KMEMC(0) >> 26) & 0x7));
    return ret;
}


static uint64_t dumpKeyProt(int no)
{
    uint64_t prot;

    if (readKeySlotProt(no, &prot))
    {
        printf("%i: %llx 0:%c%c 1:%c%c 2:%c%c 3:%c%c K_NS:%c K_HS: %c D: %c P:%c 63:0%c 2%c 3%c m:0x%x\n", no, prot,
               prot & (0x1 << 0) ? 'R' : '-',
               prot & (0x1 << 1) ? 'W' : '-',
               prot & (0x1 << 2) ? 'R' : '-',
               prot & (0x1 << 3) ? 'W' : '-',
               prot & (0x1 << 4) ? 'R' : '-',
               prot & (0x1 << 5) ? 'W' : '-',
               prot & (0x1 << 6) ? 'R' : '-',
               prot & (0x1 << 7) ? 'W' : '-',
               prot & (0x1 << 8) ? 'Y' : 'N',
               prot & (0x1 << 9) ? 'Y' : 'N',
               prot & (0x1 << 10) ? 'Y' : 'N',
               prot & (0x1 << 11) ? 'Y' : 'N',
               prot & (0x1 << 12) ? 'Y' : 'N',
               prot & (0x1 << 13) ? 'Y' : 'N',
               prot & (0x1 << 14) ? 'Y' : 'N',
               (prot >> 15) & 0xFFFF);
        return prot;
    } else
    {
        printf("%i: %llx E:%x\n", no, prot, DRF_VAL(_PRGNLCL_FALCON, _KMEMC, _ERR, localRead(LW_PRGNLCL_FALCON_KMEMC(0))));
    }
    return 0;
}

static uint64_t testKmemDump(void)
{
    int i;
    uint64_t prot;

    printf("KMEM dump test.\n");
    setPlmAll(PLM_L3);

    printf("Dumping KDF keyslots...\n");
    dumpKeyProt(41);
    dumpKeyProt(44);
    dumpKeyProt(47);
    printf("Dumping DICE keyslots...\n");
    dumpKeyProt(50);
    // Check protection on DICE keyslots, spec says what it should be set to
    printf("Checking DICE keyslots... \n");

    /* From bootrom design spec (note: some of those fields don't exist)
     * Level 0/1/2/3 non-writable, (W0/W1/W2 = 0/0/0, W3 = 0)
     * All-levels non-readable (F0/F1/F2/F3 = 0/0/0/0)
     * Secure keyable (K3 = 1) -> Ks
     * Inselwre non-keyable (K0 = 0) -> Ki
     * Disposable = 1
     * Priv = 0
     * HS_63loadable =1, LS_63loadable/NS_63loadable = 0
     * All ucode IDs allowed
     */
    printf("252: ");
    if (!keyValid(252))
        printf("!VALID ");
    prot = dumpKeyProt(252);
    if (prot & 0xFF)
        printf("READABLE/WRITABLE! ");
    if ((prot & (1<<8)))
        printf("Inselwre keyable ");
    if ((prot & (1<<9)))
        printf("!secure keyable ");
    if ((prot & (1<10)) == 0)
        printf("!DISPOSABLE ");
    if ((prot & (1<<11)))
        printf("PRI accessible ");
    if ((prot & (3<<12)))
        printf("NS/LS Loadable ");
    if ((prot & (1<<14)) == 0)
        printf("!HS Loadable ");
    printf("Done\n");

    printf("253: ");
    if (!keyValid(253))
        printf("!VALID ");
    prot = dumpKeyProt(253);
    if (prot & 0xFF)
        printf("READABLE/WRITABLE! ");
    if ((prot & (1<<8)))
        printf("Inselwre keyable ");
    if ((prot & (1<<9)))
        printf("!secure keyable ");
    if ((prot & (1<10)) == 0)
        printf("!DISPOSABLE ");
    if ((prot & (1<<11)))
        printf("PRI accessible ");
    if ((prot & (3<<12)))
        printf("NS/LS Loadable ");
    if ((prot & (1<<14)) == 0)
        printf("!HS Loadable ");
    printf("Done\n");

    printf("Dumping other slots...\n");

    for (i=0; i<32; i++)
    {
        dumpKeyProt(i);
    }

    return PASSING_DEBUG_VALUE;
}

static inline uint64_t kmemProtEntry(bool F0,   bool W0,   bool F1, bool W1,
                                     bool F2,   bool W2,   bool F3, bool W3,
                                     bool K0,   bool K3,   bool disposable,
                                     bool priv, bool ns63, bool ls63, bool hs63)
{
    return ((F0 ? 1 : 0) << 0) |
           ((W0 ? 1 : 0) << 1) |
           ((F1 ? 1 : 0) << 2) |
           ((W1 ? 1 : 0) << 3) |
           ((F2 ? 1 : 0) << 4) |
           ((W2 ? 1 : 0) << 5) |
           ((F3 ? 1 : 0) << 6) |
           ((W3 ? 1 : 0) << 7) |
           ((K0 ? 1 : 0) << 8) |
           ((K3 ? 1 : 0) << 9) |
           ((disposable ? 1 : 0) << 10) |
           ((priv ? 1 : 0) << 11) |
           ((ns63 ? 1 : 0) << 12) |
           ((ls63 ? 1 : 0) << 13) |
           ((hs63 ? 1 : 0) << 14) |
           ((0xFFFF) << 15);
}

static uint64_t testKmemProt(void)
{
    printf("Testing KMEM access protection.\n");
    // Check device map KMEM register protection
    {
        int group = LW_PRGNLCL_DEVICE_MAP_GROUP_KMEM / LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        int fld = LW_PRGNLCL_DEVICE_MAP_GROUP_KMEM % LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));

        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localWrite(LW_PRGNLCL_FALCON_KMEMC(0), 0);
        riscvLwfenceRWIO();
        if (!checkCleanMemerr())
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 0, 0, 0));
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localRead(LW_PRGNLCL_FALCON_KMEMD(0));
        riscvLwfenceRWIO();
        if (trap_mcause == 0)
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 1, 0, 0));

        // reenable access
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _ENABLE, reg);
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _ENABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
    }

    // Check device map KEY register protection (mask)
    {
        int group = LW_PRGNLCL_FALCON_KMEM_SLOT_MASK__DEVICE_MAP / LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        int fld = LW_PRGNLCL_FALCON_KMEM_SLOT_MASK__DEVICE_MAP % LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));
        uint32_t bak = localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK);

        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0x1);
        riscvLwfenceRWIO();
        if (!checkCleanMemerr())
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 2, 0, 0));
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK);
        riscvLwfenceRWIO();
        if (trap_mcause == 0)
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 3, 0, 0));

        // reenable access
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _ENABLE, reg);
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _ENABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, bak);
    }


    // Check if we can lock some slots with FALCON_KEY_SLOT_MASK
    {
        uint32_t reg = localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK);

        // each bit == 8 keys
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0xFF);
        // slot should be unlocked
        if (!readKeySlotProt(32, 0))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 4, 0, (localRead(LW_PRGNLCL_FALCON_KMEMC(0)) >> 26) & 0x7));
        // slot should be locked
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0x0);
        if (readKeySlotProt(32, 0))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 5, 0, (localRead(LW_PRGNLCL_FALCON_KMEMC(0)) >> 26) & 0x7));

        // restore
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, reg);
    }

    // check if we can block reading key for each priv level
    {
        uint32_t key[4] = {0x12341234, 0xabcdabcd, 0xbadbeef, 0xf007ba11};
        uint32_t keyr[4] = {0, };

        setPlmAll(PLM_L3);
        // Create writable key
        if (!writeKeySlotProt(42, kmemProtEntry(true, true, true, true, true, true, true, true, true, true, true, false, false, false, false)))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 6, 0, 0));
        // Fill the key
        if (!writeKeySlot(42, key))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 7, 0, 0));
        // Try reading back key
        if (!readKeySlot(42, keyr) || keyr[0] != key[0] || keyr[1] != key[1] || keyr[2] != key[2] || keyr[3] != key[3])
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 8, 0, 0));
        // get rid of key
        disposeKeySlot(42);
        memset(keyr, 0, sizeof(keyr));

        // Block NS read/write
        if (!writeKeySlotProt(42, kmemProtEntry(false, false, true, true, true, true, true, true, true, true, true, false, false, false, false)))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 9, 0, 0));
        setPlmAll(PLM_L0);
        // Try writing
        if (writeKeySlot(42, key))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 10, 0, 0));
        // Try reading back
        if (readKeySlot(42, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 11, 0, 0));
        setPlmAll(PLM_L3);
        disposeKeySlot(42);
        memset(keyr, 0, sizeof(keyr));

        // Block L1 read/write
        if (!writeKeySlotProt(42, kmemProtEntry(false, false, false, false, true, true, true, true, true, true, true, false, false, false, false)))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 12, 0, 0));
        setPlmAll(PLM_L1);
        // Try writing
        if (writeKeySlot(42, key))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 13, 0, 0));
        // Try reading back
        if (readKeySlot(42, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 14, 0, 0));
        setPlmAll(PLM_L3);
        disposeKeySlot(42);
        memset(keyr, 0, sizeof(keyr));

        // Block L2 read/write
        if (!writeKeySlotProt(42, kmemProtEntry(false, false, false, false, false, false, true, true, true, true, true, false, false, false, false)))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 15, 0, 0));
        setPlmAll(PLM_L2);
        // Try writing
        if (writeKeySlot(42, key))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 16, 0, 0));
        // Try reading back
        if (readKeySlot(42, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 17, 0, 0));
        setPlmAll(PLM_L3);
        disposeKeySlot(42);
        memset(keyr, 0, sizeof(keyr));

        // Block L3 read
        if (!writeKeySlotProt(42, kmemProtEntry(false, false, false, false, false, false, false, true, true, true, true, false, false, false, false)))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 18, 0, 0));
        setPlmAll(PLM_L3);
        dumpKeyProt(42);
        // Try writing
        if (!writeKeySlot(42, key))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 19, 0, 0));
        dumpKeyProt(42);
        // Try reading back - should not happen
        if (readKeySlot(42, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
        {
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 20, 0, 0));
        }
        disposeKeySlot(42);
        memset(keyr, 0, sizeof(keyr));
    }

    {
        // should not be able to read/dispose/clear kdf / dice keys / BROM keys
        unsigned i;
        // You shouldn't be albe to dispose brom slots
        for (i=0; i<kmem_brom_slots; ++i)
            if (disposeKeySlot(i))
                logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 21, 0, i));
        // You shouldn't be able to dispose KDF/DICE slots
        if (disposeKeySlot(50))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 22, 0, 0));
        if (disposeKeySlot(252))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 23, 0, 0));
        if (disposeKeySlot(253))
            logFail(MAKE_DEBUG_VALUE(TestErrorCode_KMEM_PROT, 24, 0, 0));
    }

    return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
typedef uint64_t (*TestCallback)(void);
static const TestCallback c_test_callbacks[TestIdCount] = {
    [TestId_KMEM_Config] = testKmemConfig,
    [TestId_KMEM_DUMP] = testKmemDump,
    [TestId_KMEM_PROT] = testKmemProt,
};

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

  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(0), 0x83333333);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(1), 0x33393333);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(2), 0x33333333);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(3), 0x3);

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

int main()
{
    uint32_t reg;

    pseudoBrom();
    printInit(1024 * 8);

    printf("Probing kmem...\n");
    reg = localRead(LW_PRGNLCL_FALCON_KMEMCFG);
    kmem_numslot = DRF_VAL(_PRGNLCL, _FALCON_KMEMCFG, _NUMOFKSLT, reg);
    kmem_slot_len = DRF_VAL(_PRGNLCL, _FALCON_KMEMCFG, _KSLT_LENGTH, reg);
    kmem_protinfo_len = DRF_VAL(_PRGNLCL, _FALCON_KMEMCFG, _PROTECTINFO_LENGTH, reg);
    kmem_brom_slots = DRF_VAL(_PRGNLCL, _FALCON_KMEMCFG, _BROM_USED_SLOTS, reg);

    printf("Setting kmem slot mask...\n");
    localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0xFFFFFFFF);

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
    printf("All tests finished. Check log for DICE/KDF errors.");
#endif
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, LW_PRGNLCL_RISCV_ICD_CMD_OPC_STOP);
    __asm__ volatile("fence");
    return 0;
}

