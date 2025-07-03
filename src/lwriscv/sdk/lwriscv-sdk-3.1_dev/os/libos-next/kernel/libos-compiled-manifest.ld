
#if !LIBOS_TINY
# define   MEMORY_DMEM
#else
# define   MEMORY_DMEM >dmem
#endif

  .CompiledTaskArray : ALIGN(1024) {
    /* Kernel header is entry point */
    taskinit_and_kernel_data = . ;

    taskTable = . ;
    TaskInit = . ;
    . = . + SIZEOF_TASK_T * SIZEOF(.TaskNames);
  } MEMORY_DMEM :phdr_data

  .CompiledShuttleTable : ALIGN(8) {
    shuttleTable = . ;

    . = . + (1<<(LOG2CEIL(
    SIZEOF(.TaskNames) * 2 +
    SIZEOF(.ManifestShuttles) / SIZEOF_MANIFEST_SHUTTLE_T))) * SIZEOF_SHUTTLE_T;
    NotaddrShuttleTableMask = ABSOLUTE((. - shuttleTable) / SIZEOF_SHUTTLE_T) - 1;
  } MEMORY_DMEM :phdr_data

  .CompiledPortArray : ALIGN(8) {
    portArray = . ;
    . = . + SIZEOF(.ManifestPorts) * SIZEOF_PORT_T / SIZEOF_MANIFEST_OBJECT_T;
  } MEMORY_DMEM :phdr_data

  .CompiledTimerArray : ALIGN(8) {
    timerArray = . ;
    . = . + SIZEOF(.ManifestTimers) * SIZEOF_TIMER_T / SIZEOF_MANIFEST_OBJECT_T;
  } MEMORY_DMEM :phdr_data

  .CompiledLockArray : ALIGN(8) {
    lockArray = . ;
    . = . + SIZEOF(.ManifestLocks) * SIZEOF_LOCK_T / SIZEOF_MANIFEST_OBJECT_T;
    lockArrayEnd = . ;
  } MEMORY_DMEM :phdr_data

  .CompiledObjectTable : ALIGN(8) {
    objectTable = . ;
    . = . + (1<<LOG2CEIL(SIZEOF(.ManifestObjectGrants) / SIZEOF_MANIFEST_PORT_GRANT_T)) * SIZEOF_OBJECT_TABLE_ENTRY_T;
    NotaddrPortTableMask = ABSOLUTE((1<<LOG2CEIL(SIZEOF(.ManifestObjectGrants) / SIZEOF_MANIFEST_PORT_GRANT_T))-1);
  } MEMORY_DMEM :phdr_data
