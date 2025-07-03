#include "kernel/lwriscv-2.0/sbi.h"
#include "peregrine-headers.h"
#include <lwmisc.h>

#   define Panilwnless(cond)                 \
        if (!(cond))                         \
            SeparationKernelShutdown();      \


static LwU64 init_entry_address = 0ULL;

static inline LwU32 readMMio(LwU32 addr)
{
    volatile LwU32* reg = (volatile LwU32*)((LwU64)addr);
    return *reg;
}

static inline void writeMMio(LwU32 addr, LwU32 data)
{
    volatile LwU32* reg = (volatile LwU32*)((LwU64)addr);
    *reg = data;
}

extern LwU64 fmc_code_size[];

typedef void (*init_partition_entry_func)(LwU64, LwU64, LwU64, LwU64, LwU64, LwU64);

__attribute__((used)) void InitPartitionBridge(LwU64 a0, LwU64 a1, LwU64 a2, LwU64 a3, LwU64 a4, LwU64 a5)
{
    // only run this on first boot
    if (init_entry_address == 0ULL)
    {
        // unlock priv access
        LwU32 lock = readMMio(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN);
        lock &= 0xFFFFFFFEUL;
        writeMMio(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN, lock);

        // read fmc code dma offset
        LwU64 fmc_code_offset = ((LwU64)readMMio(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI) << 32) |
                                 (readMMio(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO));
        fmc_code_offset <<= 8;

        // callwlate elf offset, alignment must match rvmkimg, result must match mods built descriptor.
        LwU64 elf_offset = LW_ALIGN_UP64((fmc_code_offset + (LwU64)&fmc_code_size), 0x1000UL);

        // get physical address, attaching fmc dma source type start offset

        switch (DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                    readMMio(LW_PRGNLCL_RISCV_BCR_DMACFG)))
        {
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
                init_entry_address = LW_RISCV_AMAP_FBGPA_START + elf_offset;
                break;
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:    // fall-thru
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
                init_entry_address = LW_RISCV_AMAP_SYSGPA_START + elf_offset;
                break;
            default:
                // Invalid specification
                break;
        }
    }

    init_partition_entry_func entry = (init_partition_entry_func)(init_entry_address);
    entry(init_entry_address, a1, a2, a3, a4, a5);
}

