#include "kernel.h"
#include "task.h"
#include "sched/timer.h"
#include "sched/port.h"
#include "sched/lock.h"

#define EXPORT_DEFINE(name, value) asm volatile("\n#define " #name " %0\n" : : "i" (value))

int main() {  
  EXPORT_DEFINE(ASM_SYSCALL_LIMIT, LIBOS_SYSCALL_LIMIT);
  EXPORT_DEFINE(SIZEOF_TASK_T, sizeof(Task));
  EXPORT_DEFINE(SIZEOF_SHUTTLE_T, sizeof(Shuttle));
  EXPORT_DEFINE(SIZEOF_MANIFEST_SHUTTLE_T, sizeof(struct LibosManifestShuttleInstance));
  EXPORT_DEFINE(SIZEOF_PORT_T, sizeof(Port));
  EXPORT_DEFINE(SIZEOF_TIMER_T, sizeof(Timer));
  EXPORT_DEFINE(SIZEOF_LOCK_T, sizeof(Lock));
  EXPORT_DEFINE(SIZEOF_MANIFEST_OBJECT_T, sizeof(struct LibosManifestObjectInstance));
  EXPORT_DEFINE(SIZEOF_OBJECT_TABLE_ENTRY_T, sizeof(ObjectTableEntry));
  EXPORT_DEFINE(SIZEOF_MANIFEST_PORT_GRANT_T, sizeof(struct LibosManifestObjectGrant));
  EXPORT_DEFINE(OFFSETOF_TASK_XEPC, offsetof(Task, state.xepc));
  EXPORT_DEFINE(OFFSETOF_TASK_XCAUSE, offsetof(Task, state.xcause));
  EXPORT_DEFINE(OFFSETOF_TASK_XBADADDR, offsetof(Task, state.xbadaddr));
  EXPORT_DEFINE(OFFSETOF_TASK_REGISTERS, offsetof(Task, state.registers));
  EXPORT_DEFINE(OFFSETOF_TASK_XEPC, offsetof(Task, state.xepc));
#if !LIBOS_TINY
  EXPORT_DEFINE(OFFSETOF_TASK_XSTATUS, offsetof(Task, state.xstatus));
  EXPORT_DEFINE(OFFSETOF_TASK_RADIX, offsetof(Task, state.radix));
#endif
  return 0;
}
