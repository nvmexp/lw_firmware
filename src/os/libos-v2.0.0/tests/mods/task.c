#include "flcnretval.h"
#include "lwtypes.h"
#include "riscvifriscv.h"
#include "rmgspcmdif.h"

#include "debug/elf.h"
#include "drivers/common/inc/hwref/ampere/ga102/dev_gsp.h"
#include "drivers/common/inc/hwref/ampere/ga102/lw_gsp_riscv_address_map.h"
#include "drivers/common/inc/hwref/ampere/ga102/dev_gsp_riscv_csr_64.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "libos_log.h"
#include "task_init.h"

DEFINE_LOG_INIT()

static volatile LwU32 * const pMailbox0 = (volatile LwU32 *) (LW_RISCV_AMAP_PRIV_START + LW_PGSP_FALCON_MAILBOX0);
static volatile LwU32 * const pMailbox1 = (volatile LwU32 *) (LW_RISCV_AMAP_PRIV_START + LW_PGSP_FALCON_MAILBOX1);

static inline void memcpy(void *_pDst, void *_pSrc, LwU64 size)
{
    LwU8 *pDst = _pDst;
    LwU8 *pSrc = _pSrc;
    LwU64 i;
    for (i = 0; i < size; i++)
        pDst[i] = pSrc[i];
}

static inline void halt()
{
    for (;;);
}

static inline void pass()
{
    *pMailbox0 = 0xdeadbeef;
    *pMailbox1 = 0xcafebabe;
    halt();
}


static LibosMemoryRegionInitArgument *initArgsFindFirst
(
    LibosMemoryRegionInitArgument *initArgs,
    LwU64 id8
)
{
    LwU64 initArgsAddr = (LwU64) initArgs;

    // The init args should be within a 4K page.
    LwU64 nextPageStart = (initArgsAddr + 4096) & (~4095);
    int entries = (nextPageStart - initArgsAddr) / sizeof(LibosMemoryRegionInitArgument);
    int i;

    for (i = 0; i < entries; i++)
    {
        if (initArgs[i].id8 == 0)
        {
            // Reached the end of the array.
            return 0;
        }
        else if (initArgs[i].id8 == id8)
        {
            return initArgs + i;
        }
    }

    // The init args do not stop at 4K page boundary.
    halt();
}

void task_init_entry(LwU64 elfPhysicalBase)
{
    LwU64 mailbox = (((LwU64) *pMailbox1) << 32) | *pMailbox0;
    RM_GSP_BOOT_PARAMS *pBootArgsUnsafe =
        (RM_GSP_BOOT_PARAMS *) (mailbox - LW_OFFSETOF(LW_RISCV_BOOT_PARAMS, libos));
    RM_GSP_BOOT_PARAMS bootArgs;
    if (pBootArgsUnsafe->riscv.bl.size != sizeof(RM_GSP_BOOT_PARAMS))
        halt();
    memcpy(&bootArgs, pBootArgsUnsafe, sizeof(RM_GSP_BOOT_PARAMS));
    if (bootArgs.riscv.bl.version != RM_RISCV_BOOTLDR_VERSION)
        halt();

    LibosMemoryRegionInitArgument *initArgs =
        initArgsFindFirst(bootArgs.riscv.libos.initArgs, LwU32_BUILD('R', 'U', 'N', ' '));
    if (initArgs == 0)
        halt();

    elf64_header *elf = (elf64_header *) initArgs->pa;
    LwU8 *elfIdent = elf->ident;
    // Check the magic number of the libos ELF
    if (elfIdent[0] != 0x7F || elfIdent[1] != 'E' ||
        elfIdent[2] != 'L'  || elfIdent[3] != 'F')
        halt();

    pass();
}
