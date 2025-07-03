#include "task2b.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_TASK2B()

void task2B_entry()
{
    construct_log_task2b();
    LOG_TASK2B(LOG_LEVEL_INFO, "Task2B started.\n");
}
