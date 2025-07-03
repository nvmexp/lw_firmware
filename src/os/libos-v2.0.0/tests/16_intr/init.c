#include "debug/elf.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "libos_log.h"
#include "task_init.h"

DEFINE_LOG_INIT()

void task_init_entry(LwU64 elfPhysicalBase)
{
    // TASK_INIT_FB gives the init task read-only access to FB addresses (VA=PA)
    elf64_header *elf = (elf64_header *)elfPhysicalBase;

    // Locate the manifest for the init arguments (external memory regions)
    LwU64 memory_region_init_arguments_count = 0;
    libos_manifest_init_arg_memory_region_t *memory_region_init_arguments =
        lib_init_phdr_init_arg_memory_regions(elf, &memory_region_init_arguments_count);

    // Read the init message mailbox from RM
    LwU64 mailbox                            = *(volatile LwU64 *)(TASK_INIT_PRIV + LW_PGSP_FALCON_MAILBOX0);
    LibosMemoryRegionInitArgument *init_args = (LibosMemoryRegionInitArgument *)mailbox;

    // Pre-map the init tasks log through the TASK_INIT_FB aperture
    lib_init_premap_log(init_args);

    // Size and allocate the pagetables
    lib_init_pagetable_initialize(elf, elfPhysicalBase);

    // Populate pagetable entries
    while (init_args && init_args->id8)
    {
        elf64_phdr *phdrs = (elf64_phdr *)((LwU8 *)elf + elf->phoff);

        for (LwU64 i = 0u; i < memory_region_init_arguments_count; i++)
        {
            if (memory_region_init_arguments[i].id8 == init_args->id8)
            {
                LwU64 index = memory_region_init_arguments[i].index;
                // LOG_INIT(LOG_LEVEL_INFO, "   Mapping external buffer id8=0x%016x va=0x%016x
                // pa=0x%016x size=0x%08x.\n", init_args->id8, phdrs[index].vaddr, init_args->pa,
                // init_args->size);
                lib_init_pagetable_map(elf, index, init_args->pa, init_args->size);
                break;
            }
        }

        ++init_args;
    }
    LOG_INIT(LOG_LEVEL_INFO, "External memory regions wired.\n");

    construct_log_init();
    // Start the subsidiary tasks
    LOG_INIT(LOG_LEVEL_INFO, "Queuing test application.\n");
    libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TEST_APP_PORT, 0, 0, 0, LIBOS_TIMEOUT_INFINITE);

    LOG_INIT(LOG_LEVEL_INFO, "Init exiting.\n");
}
