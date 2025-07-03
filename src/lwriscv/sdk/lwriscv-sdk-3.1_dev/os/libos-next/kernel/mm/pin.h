#pragma once
#include <lwtypes.h>
#include <libos.h>
#include "librbtree.h"
#include "memoryset.h"
#include "pagestate.h"

LibosStatus KernelMemoryPoolAcquire(LwU64 physicalAddress, LwU64 size);
void        KernelMemoryPoolRelease(LwU64 physicalAddress, LwU64 size);
