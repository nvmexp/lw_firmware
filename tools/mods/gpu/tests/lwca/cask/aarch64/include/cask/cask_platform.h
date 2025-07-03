#ifndef INCLUDE_GUARD_CASK_CASK_PLATFORM_H
#define INCLUDE_GUARD_CASK_CASK_PLATFORM_H


// cask_platform.h organizes platform-specific headers.  The logic
// behind the form, the <platform>_platform_types.h define the types
// used in cask_platform_all.h (platformStream_t, platformError_t).
// <platform>_platform.h 
#ifdef CASK_DX_PLATFORM
#include "dx_platform_types.h"
#else
#include "lwda_platform_types.h"
#endif

#include "cask_platform_all.h"

#ifdef CASK_DX_PLATFORM
#include "dx_platform.h"
#else
#include "lwda_platform.h"
#endif



#endif
