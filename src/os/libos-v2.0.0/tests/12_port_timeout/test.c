#include "../common/test-common.h"
#include "libos.h"
#include "task_test.h"

DEFINE_LOG_TEST()

static void subtest1()
{
    LOG_AND_VALIDATE(
        LOG_TEST,
        libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, 0, LIBOS_TIMEOUT_INFINITE) ==
            LIBOS_OK);
}

static void subtest2(LwU32 timeout)
{
    LOG_AND_VALIDATE(
        LOG_TEST,
        libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, 0, LIBOS_TIMEOUT_INFINITE) ==
            LIBOS_OK);

    // Step 1 Response
    LwU64 s, payload;
    LOG_AND_VALIDATE(
        LOG_TEST, libosPortSyncRecv(
                      LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, &payload, sizeof(payload), 0,
                      LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    payload++;
    libosTaskYield();

    // This reply should fail as the caller is no longer waiting
    LOG_AND_VALIDATE(
        LOG_TEST,
        libosPortReply(LIBOS_SHUTTLE_DEFAULT, &payload, sizeof(payload), 0) == LIBOS_ERROR_ILWALID_ACCESS);
    LOG_TEST(LOG_LEVEL_INFO, "Reply sent.\n");
}

#if 0
static void subtest3()
{
    LwU64 replyPortPrivateHandle, junk, payload;

    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, &payload, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, 0, 0, &replyPortPrivateHandle, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT2, 0, 0, &junk, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, replyPortPrivateHandle, 0, 0, 0, 0, 0) == LIBOS_ERROR_ILWALID_ACCESS);
}

static void subtest4()
{
    LwU64 replyPortPrivateHandle, junk, payload;

    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT_START, 0, 0, &payload, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);

    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT, 0, 0, &replyPortPrivateHandle, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncRecv(LIBOS_SHUTTLE_DEFAULT, TASK_TEST_PORT2, 0, 0, &junk, 0, 0, LIBOS_TIMEOUT_INFINITE) == LIBOS_OK);
    LOG_AND_VALIDATE(LOG_TEST, libosPortSyncSend(LIBOS_SHUTTLE_DEFAULT, replyPortPrivateHandle, 0, 0, 0, 0, 0) == LIBOS_ERROR_ILWALID_ACCESS);
}
#endif

void task_test_entry()
{

    construct_log_test();

    // subtest1();
    subtest2(0);
    /*
    subtest2(1000);
    subtest3();
    subtest3();
    subtest4();
    subtest4();*/
}
