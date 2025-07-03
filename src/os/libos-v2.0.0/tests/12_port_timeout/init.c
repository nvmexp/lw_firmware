#include "../common/test-common.h"
#include "debug/elf.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp.h"
#include "drivers/common/inc/hwref/turing/tu102/dev_gsp_riscv_csr_64.h"
#include "include/libos_init_daemon.h"
#include "kernel/libos_defines.h"
#include "libos.h"
#include "task_init.h"

DEFINE_LOG_INIT()

static void subtest1()
{
    LwU64 payload;
    LOG_INIT(LOG_LEVEL_INFO, "1. Simple timeout\n");
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, 0, LIBOS_TIMEOUT_INFINITE) ==
            LIBOS_OK);

    // Trivial timeout -- no waiter
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, 0, 0, 0, 0) == LIBOS_ERROR_TIMEOUT);
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, 0, 0, 0, 100) == LIBOS_ERROR_TIMEOUT);

    // Trivial timeout -- no sender
    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0, 0) ==
                      LIBOS_ERROR_TIMEOUT);
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0, 100) ==
            LIBOS_ERROR_TIMEOUT);
}

static void subtest2(LwU32 timeout)
{
    LOG_INIT(LOG_LEVEL_INFO, "2. Send/Recv, ensure remote receives but doesn't reply in time\n");
    LOG_AND_VALIDATE(
        LOG_INIT,
        libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, 0, LIBOS_TIMEOUT_INFINITE) ==
            LIBOS_OK);

    LwU64 msg = 0xC0DE, s;
    libosTaskYield(); // want the other end already waiting

    // This call will transmit but time out before the reply can be received
    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncSendRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &msg, sizeof(msg), LIBOS_SHUTTLE_DEFAULT_ALT,
                      TASK_INIT_PORT_PRIVATE, &msg, sizeof(msg), &s, 0) == LIBOS_ERROR_TIMEOUT);

    LOG_INIT(LOG_LEVEL_INFO, "Complete %08X\n", msg);
}
#if 0

static void subtest3(LwU32 timeout)
{
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    // Pass our reply-port to the second task for later
    LOG_INIT(LOG_LEVEL_INFO, "3. Send timeout, ensure reply-port is closed off\n");
    LwU64 msg = TASK_INIT_PORT_PRIVATE;
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, 0, msg, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    // Send a message with a reply port to the remote task (will immediately time out)
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, TASK_INIT_PORT_PRIVATE, (LwU64)&msg, 0, 0, timeout) == LIBOS_ERROR_TIMEOUT);

    // Wake up the second task and request that it write to the now defunct private port
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT2, 0, msg, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
}

static void subtest4(LwU32 timeout)
{
    // Pass our reply-port to the second task for later
    LOG_INIT(LOG_LEVEL_INFO, "4. Send/recv timeout on send, ensure reply-port is closed off\n");
    LwU64 msg = TASK_INIT_PORT_PRIVATE;
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, 0, msg, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    // Send a message with a reply port to the remote task (will immediately time out)
    LOG_AND_VALIDATE(LOG_INIT, libosPortSendRecv(TASK_TEST_PORT, TASK_INIT_PORT_PRIVATE, &msg, 0, 0, 0, timeout) == LIBOS_OK);

    // Wake up the second task and request that it write to the now defunct private port
    LOG_AND_VALIDATE(LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT2, 0, msg, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
}

#endif

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

    // Wakeup second task
    LOG_AND_VALIDATE(
        LOG_INIT, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, 0, 0, 0, 0) == LIBOS_OK);

    // subtest1();
    subtest2(LIBOS_TIMEOUT_INFINITE);
    /*
    subtest2(1000);
    subtest3(0);
    subtest3(1000);
    subtest4(0);
    subtest4(1000);*/
}
