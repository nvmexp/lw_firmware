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
#include <lwriscv/fence.h>
#include <lwriscv/csr.h>
#include <lwriscv/peregrine.h>

#include "tests.h"
#include "util.h"

#define TEST_KDF 1
#define TEST_DICE 1

void *memset(void *s, int c, unsigned n)
{
    unsigned char *p = s;
    while (n--)
        *p++ = (uint8_t)c;
    return s;
}


extern void trap_entry(void);
#define PRINT_SIZE 1024 * 24
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
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - PRINT_SIZE, PRINT_SIZE, 0, 0);

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

    // permit L3
    csr_set(LW_RISCV_CSR_MSPM,
            DRF_DEF64(_RISCV_CSR, _MSPM, _SPLM, _LEVEL3) |
            DRF_DEF64(_RISCV_CSR, _MSPM, _UPLM, _LEVEL3));

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));

    csr_write(LW_RISCV_CSR_MTVEC, (uint64_t) trap_entry);
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
    unsigned i;
    uint32_t reg;

    printf("KMEM test.\n");

    reg = localRead(LW_PRGNLCL_FALCON_HWCFG2);

    if (!FLD_TEST_DRF(_PRGNLCL, _FALCON_HWCFG2, _KMEM, _ENABLE, reg))
        return MAKE_DEBUG_NOSUB(TestId_KMEM_Config, 0, 0, reg);

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

static bool keyValid(unsigned no)
{
    unsigned slot;

    slot = no / 32;
    no = no % 32;

    if (slot > LW_PRGNLCL_FALCON_KMEM_VALID__SIZE_1)
        return false;
    return ((1U << no) & localRead(LW_PRGNLCL_FALCON_KMEM_VALID(slot))) != 0;
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


static bool readKeySlotProt(unsigned no, uint64_t *val)
{
    uint32_t *v = (uint32_t*)val;

    return readKMEM(0x1000 + no * 8, &v[0]) &&
           readKMEM(0x1000 + no * 8 + 4, &v[1]);
}

static bool writeKeySlotProt(unsigned no, uint64_t val)
{
    printf("Write keyslot: %i @ 0x%x <= 0x%llx\n", no, 0x1000 + no*8, val);
    return writeKMEM(0x1000 + no * 8, LwU64_LO32(val)) &&
           writeKMEM(0x1000 + no * 8 + 4, LwU64_HI32(val));
}

static bool disposeKeySlot(unsigned no)
{
    uint32_t reg;
    localWrite(LW_PRGNLCL_FALCON_KMEM_CTL,
               DRF_DEF(_PRGNLCL_FALCON, _KMEM_CTL, _CMD, _DISPOSE) |
               DRF_NUM(_PRGNLCL_FALCON, _KMEM_CTL, _IDX, no));
    reg = localRead(LW_PRGNLCL_FALCON_KMEM_CTL);
    if (!FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEM_CTL, _RESULT, _DISPOSE_DONE, reg)) {
        printf("Dispose failed for slot %u.\n", no);
    }

    return FLD_TEST_DRF(_PRGNLCL_FALCON, _KMEM_CTL, _RESULT, _DISPOSE_DONE, reg);
}

static bool readKeySlot(uint32_t no, uint32_t *key)
{
    return readKMEM(no * 16, &key[0]) &&
           readKMEM(no * 16 + 4, &key[1]) &&
           readKMEM(no * 16 + 8, &key[2]) &&
           readKMEM(no * 16 + 12, &key[3]);
}

static bool writeKeySlot(uint32_t no, uint32_t *key)
{
    bool ret = writeKMEM(no * 16, key[0]) &&
           writeKMEM(no * 16 + 4, key[1]) &&
           writeKMEM(no * 16 + 8, key[2]) &&
           writeKMEM(no * 16 + 12, key[3]);

    if (!ret)
        printf("Keyslot programming failed: 0x%x\n", (localRead(LW_PRGNLCL_FALCON_KMEMC(0) >> 26) & 0x7));
    return ret;
}


static uint64_t dumpKeyProt(unsigned no)
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
    unsigned i;
    uint64_t prot;

    printf("KMEM dump test.\n");
    setPlmAll(PLM_L3);

    if (TEST_KDF)
    {
        printf("Dumping KDF keyslots...\n");
        dumpKeyProt(41);
        dumpKeyProt(44);
        dumpKeyProt(47);
    }

    if (TEST_DICE)
    {
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
        if (!keyValid(252))
            printf("!VALID ");
        prot = dumpKeyProt(252);
        if (prot & 0xFF)
            printf("READABLE/WRITABLE ");
        if ((prot & (1<<8)))
            printf("Inselwre keyable ");
        if ((prot & (1<<9)) == 0)
            printf("!secure keyable ");
        if ((prot & (1<10)) == 1)
            printf("!DISPOSABLE ");
        if ((prot & (1<<11)))
            printf("PRI accessible ");
        if ((prot & (3<<12)))
            printf("NS/LS Loadable ");
        if ((prot & (1<<14)) == 0)
            printf("!HS Loadable ");
        printf("Done\n");

        if (!keyValid(253))
            printf("!VALID ");
        prot = dumpKeyProt(253);
        if (prot & 0xFF)
            printf("READABLE/WRITABLE ");
        if ((prot & (1<<8)))
            printf("Inselwre keyable ");
        if ((prot & (1<<9)) == 0)
            printf("!secure keyable ");
        if ((prot & (1<10)) == 1)
            printf("!DISPOSABLE ");
        if ((prot & (1<<11)))
            printf("PRI accessible ");
        if ((prot & (3<<12)))
            printf("NS/LS Loadable ");
        if ((prot & (1<<14)) == 0)
            printf("!HS Loadable ");
        printf("Done\n");
    }

    printf("Dumping all valid slots...\n");

    uint64_t last_keyprot = 0;
    for (i=0; i<256; i++)
    {
        if (keyValid(i))
        {
            uint64_t kp = 0;
            readKeySlotProt(i, &kp);
            if (kp != last_keyprot || i > 250)
                dumpKeyProt(i);
            last_keyprot = kp;
        }
    }

    return PASSING_DEBUG_VALUE;
}

static inline uint64_t kmemProtEntry(bool F0,   bool W0,   bool F1, bool W1,
                                     bool F2,   bool W2,   bool F3, bool W3,
                                     bool K0,   bool K3,   bool disposable,
                                     bool priv, bool ns63, bool ls63, bool hs63)
{
    return ((F0 ? 1U : 0U) << 0) |
           ((W0 ? 1U : 0U) << 1) |
           ((F1 ? 1U : 0U) << 2) |
           ((W1 ? 1U : 0U) << 3) |
           ((F2 ? 1U : 0U) << 4) |
           ((W2 ? 1U : 0U) << 5) |
           ((F3 ? 1U : 0U) << 6) |
           ((W3 ? 1U : 0U) << 7) |
           ((K0 ? 1U : 0U) << 8) |
           ((K3 ? 1U : 0U) << 9) |
           ((disposable ? 1U : 0U) << 10) |
           ((priv ? 1U : 0U) << 11) |
           ((ns63 ? 1U : 0U) << 12) |
           ((ls63 ? 1U : 0U) << 13) |
           ((hs63 ? 1U : 0U) << 14) |
           ((0xFFFFU) << 15);
}

static uint64_t testKmemProt(void)
{
    printf("Testing KMEM access protection.\n");
    // Check device map KMEM register protection
    {
        uint32_t group = LW_PRGNLCL_DEVICE_MAP_GROUP_KMEM / LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t fld = LW_PRGNLCL_DEVICE_MAP_GROUP_KMEM % LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));

        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localWrite(LW_PRGNLCL_FALCON_KMEMC(0), 0);
        riscvLwfenceRWIO();
        if (!checkCleanMemerr())
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 0, 0);
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);

        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localRead(LW_PRGNLCL_FALCON_KMEMD(0));
        riscvLwfenceRWIO();
        if (trap_mcause == 0)
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 1, 0);

        // reenable access
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _ENABLE, reg);
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _ENABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
    }

    // Check device map KEY register protection (mask)
    {
        uint32_t group = LW_PRGNLCL_FALCON_KMEM_SLOT_MASK__DEVICE_MAP / LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t fld = LW_PRGNLCL_FALCON_KMEM_SLOT_MASK__DEVICE_MAP % LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1;
        uint32_t reg = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group));
        uint32_t bak = localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK);

        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0x1);
        riscvLwfenceRWIO();
        if (!checkCleanMemerr())
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 2, 0);
        reg = FLD_IDX_SET_DRF_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, fld, _DISABLE, reg);
        localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(group), reg);
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        localRead(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK);
        riscvLwfenceRWIO();
        if (trap_mcause == 0)
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 3, 0);

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
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 4, 0);
        // slot should be locked
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, 0x0);
        if (readKeySlotProt(32, 0))
            return MAKE_DEBUG_NOSUB(TestId_KMEM_PROT, 5, 0, (localRead(LW_PRGNLCL_FALCON_KMEMC(0)) >> 26) & 0x7);

        // restore
        localWrite(LW_PRGNLCL_FALCON_KMEM_SLOT_MASK, reg);
    }

    // check if we can block reading key for each priv level
    {
        const uint32_t keyslot = 199; // 0-191, 252-255 used by brom
        uint32_t key[4] = {0x12341234, 0xabcdabcd, 0xbadbeef, 0xf007ba11};
        uint32_t keyr[4] = {0, };

        setPlmAll(PLM_L3);
        // Create writable key
        if (!writeKeySlotProt(keyslot, kmemProtEntry(true, true, true, true, true, true, true, true, true, true, true, false, false, false, false)))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 6, 0);
        // Fill the key
        if (!writeKeySlot(keyslot, key))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 7, 0);
        // Try reading back key
        if (!readKeySlot(keyslot, keyr) || keyr[0] != key[0] || keyr[1] != key[1] || keyr[2] != key[2] || keyr[3] != key[3])
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 8, 0);
        // get rid of key
        disposeKeySlot(keyslot);
        memset(keyr, 0, sizeof(keyr));

        // Block NS read/write
        if (!writeKeySlotProt(keyslot, kmemProtEntry(false, false, true, true, true, true, true, true, true, true, true, false, false, false, false)))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 9, 0);
        setPlmAll(PLM_L0);
        // Try writing
        if (writeKeySlot(keyslot, key))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 10, 0);
        // Try reading back
        if (readKeySlot(keyslot, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 11, 0);
        setPlmAll(PLM_L3);
        disposeKeySlot(keyslot);
        memset(keyr, 0, sizeof(keyr));

        // Block L1 read/write
        if (!writeKeySlotProt(keyslot, kmemProtEntry(false, false, false, false, true, true, true, true, true, true, true, false, false, false, false)))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 12, 0);
        setPlmAll(PLM_L1);
        // Try writing
        if (writeKeySlot(keyslot, key))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 13, 0);
        // Try reading back
        if (readKeySlot(keyslot, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 14, 0);
        setPlmAll(PLM_L3);
        disposeKeySlot(keyslot);
        memset(keyr, 0, sizeof(keyr));

        // Block L2 read/write
        if (!writeKeySlotProt(keyslot, kmemProtEntry(false, false, false, false, false, false, true, true, true, true, true, false, false, false, false)))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 15, 0);
        setPlmAll(PLM_L2);
        // Try writing
        if (writeKeySlot(keyslot, key))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 16, 0);
        // Try reading back
        if (readKeySlot(keyslot, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 17, 0);
        setPlmAll(PLM_L3);
        disposeKeySlot(keyslot);
        memset(keyr, 0, sizeof(keyr));

        // Block L3 read
        if (!writeKeySlotProt(keyslot, kmemProtEntry(false, false, false, false, false, false, false, true, true, true, true, false, false, false, false)))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 18, 0);
        setPlmAll(PLM_L3);
        dumpKeyProt(keyslot);
        // Try writing
        if (!writeKeySlot(keyslot, key))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 19, 0);
        dumpKeyProt(keyslot);
        // Try reading back - should not happen
        if (readKeySlot(keyslot, keyr) || (keyr[0] == key[0]) || (keyr[1] == key[1]) || (keyr[2] == key[2]) || (keyr[3] == key[3]))
        {
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 20, 0);
        }
        disposeKeySlot(keyslot);
        memset(keyr, 0, sizeof(keyr));
    }

    // in peregrine 2.1 l3 can do disposal, that's feature http://lwbugs/200597029
    if (0)
    {
        // should not be able to read/dispose/clear kdf / dice keys / BROM keys
        unsigned i;
        printf("Trying to dispose keyslots...\n");
        // You shouldn't be albe to dispose brom slots
        for (i=0; i<kmem_brom_slots; ++i)
            if (disposeKeySlot(i))
                return MAKE_DEBUG_NOSUB(TestId_KMEM_PROT, 21, 0, i);
        // You shouldn't be able to dispose KDF/DICE slots
#if TEST_KDF
        if (disposeKeySlot(50))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 22, 0);
#endif

#if TEST_DICE
        if (disposeKeySlot(252))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 23, 0);
        if (disposeKeySlot(253))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 24, 0);
        if (disposeKeySlot(254))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 25, 0);
        if (disposeKeySlot(255))
            return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 26, 0);
#endif
    }

    // check if keyslots are populated
    #if TEST_KDF
    {
        int i;
        for (i=0; i<192; ++i)
        {
            if (!keyValid(i))
                return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 27, i);
        }
    }
    #endif

    #if TEST_DICE
    if (!keyValid(252))
        return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 28, 0);
    if (!keyValid(253))
        return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 28, 1);
    if (!keyValid(254))
        return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 28, 2);
    if (!keyValid(255))
        return MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 28, 3);
    #endif
    return PASSING_DEBUG_VALUE;
}

#if LWRISCV_IS_ENGINE_fsp
static bool fspKeywrapTest(void)
{
    volatile uint32_t *keyglobAddr;
    int i;
    uint32_t dmemSize = 0;

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    printf("FSP keywrap test...");

    keyglobAddr = (volatile uint32_t *)(LW_RISCV_AMAP_DMEM_START + dmemSize - (3 + 1 + 1 +1 + 1 + 9) * 1024);
    printf("Keyglob addr: %p\n", keyglobAddr);

    for (i=0; i<(9 * 1024 / 4); ++i)
    {
        if (keyglobAddr[i] == 0)
        {
            csr_write(LW_RISCV_CSR_MSCRATCH, MAKE_DEBUG_NOVAL(TestId_KMEM_PROT, 1, i));
            return false;
        }
    }

    csr_write(LW_RISCV_CSR_MSCRATCH, (uint64_t)keyglobAddr);

    return true;
}
#endif

static const TestCallback c_test_callbacks[TestIdCount] = {
    [TestId_KMEM_Config] = testKmemConfig,
    [TestId_KMEM_DUMP] = testKmemDump,
    [TestId_KMEM_PROT] = testKmemProt,
};

int main(void)
{
    bool pass = true;

    init();
    printf("Testing...\n");
    csr_write(LW_RISCV_CSR_MSCRATCH, 0);

#if LWRISCV_IS_ENGINE_fsp
    pass = fspKeywrapTest();
#else
    uint32_t reg;
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
#endif
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
