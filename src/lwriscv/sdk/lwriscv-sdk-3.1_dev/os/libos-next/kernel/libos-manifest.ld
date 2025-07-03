  
#if !LIBOS_TINY
# define   MEMORY_MANIFEST
#else
# define   MEMORY_MANIFEST >manifest
#endif


  /* These three handle spaces all start assigning values at 0 */
  OVERLAY 0 :  {
    .TaskNames   {
      /* Force the init task to be the first task */
      KEEP(SORT_BY_NAME(*)(.TaskNames.taskInit))
      KEEP(SORT_BY_NAME(*)(.TaskNames.*))
    } :phdr_manifest 
  
    .ShuttleNames  {
      BYTE(0); /* Don't hand out a zero handle */
      KEEP(SORT_BY_NAME(*)(.ShuttleNames.*))
    }  :phdr_manifest 
  
    .ObjectNames  {
      BYTE(0); /* Don't hand out a zero handle */
      KEEP(SORT_BY_NAME(*)(.ObjectNames.*))
      ASSERT(. < (1 << (8 * LIBOS_CONFIG_ID_SIZE)), "Handles exceeded, set larger LIBOS_CONFIG_ID_SIZE in /config." );
    }  :phdr_manifest 
  }

  .ManifestTasks  : {
    KEEP(*(.ManifestTasks))
  } MEMORY_MANIFEST :phdr_manifest 

  .ManifestPorts  : {
    KEEP(*(.ManifestPorts))
  } MEMORY_MANIFEST :phdr_manifest

  .ManifestTimers  : {
    KEEP(*(.ManifestTimers))
  } MEMORY_MANIFEST :phdr_manifest

  .ManifestLocks  : {
    KEEP(*(.ManifestLocks))
  } MEMORY_MANIFEST :phdr_manifest

  .ManifestShuttles  : {
    KEEP(*(.ManifestShuttles))
  } MEMORY_MANIFEST :phdr_manifest

  .ManifestObjectGrants : {
    KEEP(*(.ManifestObjectGrants))
  } MEMORY_MANIFEST :phdr_manifest
