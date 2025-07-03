#include "../common/test-common.h"
#include "libos.h"
#include "task_init_interface.h"
#include "task_test.h"

DEFINE_LOG_TEST()

__attribute__((noinline)) LibosStatus task_init_util_command(LwU8 cmd)
{
    // Send utility command
    task_init_util_payload payload;
    LwU64 payload_size;
    LibosStatus status = LIBOS_OK;

    payload.command = cmd;
    LOG_AND_VALIDATE_EQUAL(
        LOG_TEST, status, LIBOS_OK,
        libosPortSyncSendRecv(
            LIBOS_SHUTTLE_DEFAULT,     // sendShuttle
            TASK_INIT_UTIL_PORT,       // sendPort
            &payload,                  // sendPayload
            sizeof payload,            // sendPayloadSize
            LIBOS_SHUTTLE_DEFAULT_ALT, // recvShuttle
            TASK_TEST_REPLY,           // recvPort
            &payload,                  // recvPayload
            sizeof payload,            // recvPayloadCapacity
            &payload_size,             // completedSize
            LIBOS_TIMEOUT_INFINITE));  // timeoutNs

    return status;
}

__attribute__((noinline)) LibosStatus b(LwU32 num)
{
    LibosStatus status = LIBOS_OK;

    switch (num)
    {
    case 1:
        LOG_TEST(LOG_LEVEL_INFO, "\tTest task sending TASK_INIT_UTIL_CMD_STATE\n");
        status = task_init_util_command(TASK_INIT_UTIL_CMD_STATE);
        break;

    case 2:
    case 6:
        LOG_TEST(LOG_LEVEL_INFO, "\tTest task sending TASK_INIT_UTIL_CMD_STACK\n");
        status = task_init_util_command(TASK_INIT_UTIL_CMD_STACK);
        break;

    case 3:
        LOG_TEST(LOG_LEVEL_INFO, "\tTest task sending TASK_INIT_UTIL_CMD_STATE_AND_STACK\n");
        status = task_init_util_command(TASK_INIT_UTIL_CMD_STATE_AND_STACK);
        break;

    case 4:
    case 5:
        // Failure case: Attempt to wait on the test_port to which we're not granted privilege
        LOG_TEST(
            LOG_LEVEL_INFO,
            "\tTest task about to breakpoint with callstack including a, b.(a0 == 0xdeadb00f)\n");
        __asm(R"(
                li a0, 0xdeadb00f
                ebreak
                )" ::);
        break;

    default:
        // do nothing
        break;
    }

    return status;
}

__attribute__((noinline)) void a()
{
    for (volatile LwU32 i = 0; i < 7; i++)
    {
        (void)b(i);
    }
}

void task_test_entry()
{
    construct_log_test();
    LOG_TEST(LOG_LEVEL_INFO, "\tStarting test task...\n");
    a();

    LOG_TEST(
        LOG_LEVEL_INFO,
        "\tTest task about to exit (lwrrently a branch to address 0 -- we should see a crash)\n");
}
