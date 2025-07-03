  
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
  } >manifest :phdr_manifest 

  .ManifestPorts  : {
    KEEP(*(.ManifestPorts))
  } >manifest :phdr_manifest

  .ManifestTimers  : {
    KEEP(*(.ManifestTimers))
  } >manifest :phdr_manifest

  .ManifestLocks  : {
    KEEP(*(.ManifestLocks))
  } >manifest :phdr_manifest

  .ManifestShuttles  : {
    KEEP(*(.ManifestShuttles))
  } >manifest :phdr_manifest

  .ManifestObjectGrants : {
    KEEP(*(.ManifestObjectGrants))
  } >manifest :phdr_manifest
