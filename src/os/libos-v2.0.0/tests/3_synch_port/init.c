#include "../common/test-common.h"
#include "debug/elf.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
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

    LwU64 payload, s;
    LwU32 source_task;
    LwU32 completed_size;

    construct_log_init();

    // Failure case: Attempt to wait on the test_port to which we're not granted privilege
    // LOG_INIT(LOG_LEVEL_INFO, "\nAttempting to wait on port we don't own %08X\n", TASK_TEST_PORT);
    // LOG_AND_VALIDATE(LOG_INIT, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT,
    // &reply_port, &source_task, &payload, 0, 0, 0) == LIBOS_ERROR_ILWALID_ACCESS);

    // Edge case:    Attempt to wake up the test with an incoming buffer
    //               Wake up the target task with a buffer, the target task
    //               will still wake up -- but will be notified of the missing payload.
    //
    static char bigbuffer[4096];
    LOG_INIT(LOG_LEVEL_INFO, "Waking up test task w/ superflous payload\n");

    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncSend(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &bigbuffer, 4096, &completed_size,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_ERROR_INCOMPLETE);

    // Send the task a test message
    payload = 0xDEADBEEFF00D0001ULL;
    LOG_INIT(LOG_LEVEL_INFO, "Sending %016llx\n", payload);
    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncSend(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    // Wait for the task to send back a payload
    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    LOG_INIT(LOG_LEVEL_INFO, "First payload = %016llx\n", payload);

    // Ping it back again through an explicit send/recv
    payload = 0xC0DE;
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncSendRecv(
            LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), LIBOS_SHUTTLE_DEFAULT_ALT,
            TASK_INIT_PORT_PRIVATE, &payload, sizeof(payload), &s, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    LOG_INIT(LOG_LEVEL_INFO, "Final value = %016llx\n", payload);
}
