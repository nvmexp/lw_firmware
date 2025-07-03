#include "task2a.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_TASK2A()

void task2A_entry()
{
    construct_log_task2a();
    LOG_TASK2A(LOG_LEVEL_INFO, "Task2A started.\n");
}
