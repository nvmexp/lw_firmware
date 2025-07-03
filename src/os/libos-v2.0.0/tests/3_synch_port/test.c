#include "../common/test-common.h"
#include "libos.h"
#include "task_test.h"

DEFINE_LOG_TEST()

void task_test_entry()
{
    LwU64 payload = 0;
    LwU32 source_task;

    construct_log_test();

    // Failure case: Attempt to wait on the test_port to which we're not granted privilege
    LOG_TEST(LOG_LEVEL_INFO, "\tNegative test case (no access)\n");
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortSyncRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_ERROR_ILWALID_ACCESS);

    LOG_TEST(LOG_LEVEL_INFO, "\tWaiting for message from test\n");
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortSyncRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    LOG_TEST(LOG_LEVEL_INFO, "\tReceived %016llX\n", payload);

    if (payload == 0xDEADBEEFF00D0001ULL)
        payload = 0xC0DE0001;

    LOG_TEST(LOG_LEVEL_INFO, "\tSending %016llX\n", payload);
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortSyncSend(
                      LIBOS_SHUTTLE_DEFAULT, TASK_INIT_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    LwU64 s;
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortSyncRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    payload++;
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortReply(LIBOS_SHUTTLE_DEFAULT, &payload, sizeof(payload), 0) == LIBOS_OK);
    LOG_TEST(LOG_LEVEL_INFO, "Reply sent.\n");
}
