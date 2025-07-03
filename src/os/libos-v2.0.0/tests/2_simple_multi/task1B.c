#include "task1b.h"
#include "libos.h"
#include "libos_log.h"

DEFINE_LOG_TASK1B()

void task1B_entry()
{
    construct_log_task1b();
    LOG_TASK1B(LOG_LEVEL_INFO, "Task1B started.\n");
}
