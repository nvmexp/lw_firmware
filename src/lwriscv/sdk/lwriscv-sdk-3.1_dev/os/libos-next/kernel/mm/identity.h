#pragma once
#include "kernel.h"
#include <lwmisc.h>

void * KernelPhysicalIdentityMap(LwU64 fbAddress);

LwU64 KernelPhysicalIdentityReverseMap(void * addr);