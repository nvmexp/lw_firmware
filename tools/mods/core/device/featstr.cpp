/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/device.h"

// Map each feature to a string describing the feature
struct Device::FeatureStr Device::s_FeatureStrings[] =
{
#define DEFINE_GPUSUB_FEATURE(feat, featid, desc) { feat, desc },
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc) { feat, desc },
#define DEFINE_MCP_FEATURE(feat, featid, desc) { feat, desc },
#define DEFINE_SOC_FEATURE(feat, featid, desc) { feat, desc },

#include "core/include/featlist.h"

#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE

    { FEATURE_COUNT, "" }      // Sentinel value for end of feature strings
};

