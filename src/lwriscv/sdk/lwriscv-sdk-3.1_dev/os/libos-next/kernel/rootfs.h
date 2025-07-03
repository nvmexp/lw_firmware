#include "libbootfs.h"

extern LibosBootFsArchive * rootFs;
LibosStatus         KernelRootfsMountFilesystem(LwU64 physicalAddress);
LibosBootfsRecord * KernelFileOpen(const char * name);
LwU64               KernelFilePhysical(LibosBootfsExelwtableMapping * file);