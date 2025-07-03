#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#define  LIBOS_SECTION_IMEM_PINNED
#include "../kernel/scheduler-tables.h"

int main() {
  printf("#define SIZEOF_TASK_T %d\n", sizeof(Task));
  printf("#define SIZEOF_SHUTTLE_T %d\n", sizeof(Shuttle));
  printf("#define SIZEOF_MANIFEST_SHUTTLE_T %d\n", sizeof(struct LibosManifestShuttleInstance));
  printf("#define SIZEOF_PORT_T %d\n", sizeof(Port));
  printf("#define SIZEOF_TIMER_T %d\n", sizeof(Timer));
  printf("#define SIZEOF_LOCK_T %d\n", sizeof(Lock));
  printf("#define SIZEOF_MANIFEST_OBJECT_T %d\n", sizeof(struct LibosManifestObjectInstance));
  printf("#define SIZEOF_OBJECT_TABLE_ENTRY_T %d\n", sizeof(ObjectTableEntry));
  printf("#define SIZEOF_MANIFEST_PORT_GRANT_T %d\n", sizeof(struct LibosManifestObjectGrant));
  return 0;
}
