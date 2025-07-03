#include "libos.h"
#include "libos_log.h"
#include "task_test.h"

DEFINE_LOG_TEST()

void task_test_entry()
{
    construct_log_test();

    LOG_TEST(LOG_LEVEL_INFO, "test task starts exelwtion.\n");

    LwU64 payload, msgid, incomingMessageSize;
    libosPortSyncRecv(
        LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), &incomingMessageSize,
        LIBOS_TIMEOUT_INFINITE);

    LOG_TEST(LOG_LEVEL_INFO, "test task receives %llx.\n", payload);

    payload = 0xC0DE0001;

    LOG_TEST(LOG_LEVEL_INFO, "test task sends %llx.\n", payload);
    libosPortSyncSend(
        LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0, LIBOS_TIMEOUT_INFINITE);
}
