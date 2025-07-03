#include "flcnretval.h"
#include "lwtypes.h"
#include "riscvifriscv.h"
#include "rmgspcmdif.h"

#include "debug/elf.h"
#include "ampere/ga102/dev_gsp.h"
#include "ampere/ga102/dev_gsp_riscv_csr_64.h"
#include "ampere/ga102/lw_gsp_riscv_address_map.h"
#include "ampere/ga102/dev_riscv_addendum.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "libos_log.h"
#include "task_init.h"

DEFINE_LOG_INIT()

static volatile LwU32 * const pMailbox0 = (volatile LwU32 *) (LW_RISCV_AMAP_PRIV_START + CHIP(_FALCON_MAILBOX0));
static volatile LwU32 * const pMailbox1 = (volatile LwU32 *) (LW_RISCV_AMAP_PRIV_START + CHIP(_FALCON_MAILBOX1));

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

    LibosMemoryInitArgument initArgs = bootArgs.riscv.libos.initArgs[0];
    if (initArgs.id8 != LwU32_BUILD('E', 'L', 'F', '_'))
        halt();
    LibosElf64Header *elf = (LibosElf64Header *) initArgs.pa;
    LwU8 *elfIdent = elf->ident;
    // Check the magic number of the libos ELF
    if (elfIdent[0] != 0x7F || elfIdent[1] != 'E' ||
        elfIdent[2] != 'L'  || elfIdent[3] != 'F')
        halt();

    pass();
}
