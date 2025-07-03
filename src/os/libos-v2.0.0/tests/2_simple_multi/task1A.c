#include "task1a.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_TASK1A()

void task1A_entry()
{
    construct_log_task1a();
    LOG_TASK1A(LOG_LEVEL_INFO, "Task1A started.\n");
}
