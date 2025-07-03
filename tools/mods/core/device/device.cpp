/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2014,2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/device.h"
#include "core/include/version.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include <map>

#define BUGS_PER_LINE       4

bool Device::s_FeaturesValid = false;

//! Empty constructor for the base class.
Device::Device()
{
    if (!s_FeaturesValid)
        ValidateFeatures();
}

//! Virtual destructor for the base class.
Device::~Device()
{
}

//! Use HasBug to check if the device requires a workaround for this
//! HW or SW bug.  Please use actual lwpu bug-database numbers.

/* virtual */
bool Device::HasBug(UINT32 bugNum) const
{
    return m_BugSet.find(bugNum) != m_BugSet.end();
}

//! Call SetHasBug after creating a device, to fill in the appropriate bugs.
/* virtual */
void Device::SetHasBug(UINT32 bugNum)
{
    m_BugSet.insert(bugNum);
}

//! Call ClearHasBug if for some reason a bug WAR is no longer required.
/* virtual */
void Device::ClearHasBug(UINT32 bugNum)
{
    m_BugSet.erase(bugNum);
}

//! Print out the bug list formatted nicely.
/* virtual */
void Device::PrintBugs()
{
    // Only print the bugs/features for an internal build
    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        set<UINT32>::iterator bugs;
        UINT32 bugCount = 0;
        string strHeader = m_Name + " Bugs     : ";

        // Print the header string
        Printf(Tee::PriLow, "%s", strHeader.c_str());

        // Print out the bug numbers
        for (bugs = m_BugSet.begin(); bugs != m_BugSet.end(); bugs++, bugCount++)
        {
            // Keep everything column aligned
            if ((bugs != m_BugSet.begin()) && (0 == (bugCount % BUGS_PER_LINE)))
                Printf(Tee::PriLow, "%*s", (int)strHeader.length(), " ");

            Printf(Tee::PriLow, "%6d  ", *bugs);

            // Print the return here instead of above so that the timestamp
            // gets appended to all the lines and keeps the columns correct
            if ((BUGS_PER_LINE - 1) == (bugCount % BUGS_PER_LINE))
                Printf(Tee::PriLow, "\n");
        }

        if ((0 != (bugCount % BUGS_PER_LINE)) || (0 == bugCount))
            Printf(Tee::PriLow, "\n");
    }
}

namespace
{
    map<Device::LwDeviceId, string> s_DeviceIdToString =
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        { Device::GpuId, #GpuId },
#define DEFINE_X86_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        { Device::GpuId, #GpuId },
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_OBS_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_X86_GPU
#undef DEFINE_NEW_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
        { Device::LwId, #LwId },
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"
#undef DEFINE_OBS_LWL_DEV
#undef DEFINE_LWL_DEV
        { Device::ILWALID_DEVICE, "Invalid" }
    };

    map<string, Device::LwDeviceId> s_StringToDeviceId =
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        { #GpuId, Device::GpuId },
#define DEFINE_X86_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
        { #GpuId, Device::GpuId },
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_OBS_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_X86_GPU
#undef DEFINE_NEW_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
        { #LwId, Device::LwId },
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"
#undef DEFINE_OBS_LWL_DEV
#undef DEFINE_LWL_DEV
        { "Invalid", Device::ILWALID_DEVICE }
    };
};

/* static */ string Device::DeviceIdToString(LwDeviceId id)
{
    auto itr = s_DeviceIdToString.find(id);
    if (itr == s_DeviceIdToString.end())
        return s_DeviceIdToString[Device::ILWALID_DEVICE];
    return itr->second;
}

/* static */ Device::LwDeviceId Device::StringToDeviceId(const string& s)
{
    auto itr = s_StringToDeviceId.find(s);
    if (itr == s_StringToDeviceId.end())
        return Device::ILWALID_DEVICE;
    return itr->second;
}

//! Use HasFeature to check if the device implements a particular feature
/* virtual */
bool Device::HasFeature(Feature feature) const
{
    return m_FeatureSet.find(feature) != m_FeatureSet.end();
}

//! Call SetHasFeature after creating a device, to fill in the appropriate
//! features.
/* virtual */
void Device::SetHasFeature(Feature feature)
{
    m_FeatureSet.insert(feature);
}

//! Call ClearHasFeature if for some reason a feature that exists in a previous
//! incarnation of the device does not exist in a subsequent incarnation.
/* virtual */
void Device::ClearHasFeature(Feature feature)
{
    m_FeatureSet.erase(feature);
}

//! Print out the feature list formatted nicely.
/* virtual */
void Device::PrintFeatures()
{
    // Only print the bugs/features for an internal build
    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        set<UINT32>::iterator features;
        string strHeader = m_Name + " Features : ";
        string strFeature;

        // Print the header
        Printf(Tee::PriLow, "%s", strHeader.c_str());

        // For each feature in the set
        UINT32 featureCount = 0;
        for (features = m_FeatureSet.begin();
                features != m_FeatureSet.end(); features++, featureCount++)
        {
            // Keep the features aligned
            if (features != m_FeatureSet.begin())
                Printf(Tee::PriLow, "%*s", (int)strHeader.length(), " ");

            strFeature = FeatureString((Device::Feature)*features);
            if (strFeature == "Unknown Feature")
            {
                Printf(Tee::PriLow, "%s (%d)\n", strFeature.c_str(), *features);
            }
            else
            {
                Printf(Tee::PriLow, "%s\n", strFeature.c_str());
            }
        }
        if (0 == featureCount)
            Printf(Tee::PriLow, "\n");
    }
}

//! Find the feature string associated with a particular feature.
/* virtual */
string Device::FeatureString(Feature feature)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return "Unknown Feature";

    UINT32 featIdx = 0;

    while (s_FeatureStrings[featIdx].feature != FEATURE_COUNT)
    {
        if (s_FeatureStrings[featIdx].feature == feature)
        {
            return s_FeatureStrings[featIdx].featureStr;
        }

        featIdx++;
    }

    return "Unknown Feature";
}

//! Validate the features to ensure that there are no duplicates.
/* virtual */
void Device::ValidateFeatures()
{
    // Crate an array of the features using symbols
    Feature features[] =
    {
#define DEFINE_GPUSUB_FEATURE(feat, featid, desc) feat,
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc) feat,
#define DEFINE_MCP_FEATURE(feat, featid, desc) feat,
#define DEFINE_SOC_FEATURE(feat, featid, desc) feat,

#include "core/include/featlist.h"

#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE
    };
    UINT32 featureCount = sizeof(features) / sizeof(Feature);
    UINT32 duplicateCount = 0;

    // For each feature
    for (UINT32 lwrFeature = 0; lwrFeature < (featureCount - 1); lwrFeature++)
    {
        // Check it against the remaining features to determine if there are
        // duplicate features, if so, print an error and assert
        for (UINT32 chkFeature = (lwrFeature + 1);
              chkFeature < featureCount;
              chkFeature++)
        {
            if (features[lwrFeature] == features[chkFeature])
            {
                ++duplicateCount;
                Printf(Tee::PriAlways,
                       "Device::ValidateFeatures - ");
                switch(features[lwrFeature] & FEATURE_TYPE_MASK)
                {
                    case GPUSUB_FEATURE_TYPE:
                        Printf(Tee::PriAlways,
                               "GPU Subdevice Feature %d is duplicated\n",
                               features[lwrFeature] & ~FEATURE_TYPE_MASK);
                        break;
                    case GPUDEV_FEATURE_TYPE:
                        Printf(Tee::PriAlways,
                               "GPU Device Feature %d is duplicated\n",
                               features[lwrFeature] & ~FEATURE_TYPE_MASK);
                        break;
                    case MCP_FEATURE_TYPE:
                        Printf(Tee::PriAlways,
                               "MCP Feature %d is duplicated\n",
                               features[lwrFeature] & ~FEATURE_TYPE_MASK);
                        break;
                    case SOC_FEATURE_TYPE:
                        Printf(Tee::PriAlways,
                               "SOC Feature %d is duplicated\n",
                               features[lwrFeature] & ~FEATURE_TYPE_MASK);
                        break;
                    default:
                        MASSERT(!"Invalid Feature Type\n");
                        break;
                }
            }
        }
    }

    // Ensure that there are no duplicate features.
    MASSERT(duplicateCount == 0);

    // If exelwtion reaches this point then the features are valid
    s_FeaturesValid = true;
}

//! Perform an escape read/write into the simulator
/* virtual */
int Device::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    return Platform::EscapeWrite(Path, Index, Size, Value);
}

/* virtual */
int Device::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
    return Platform::EscapeRead(Path, Index, Size, Value);
}

JS_CLASS(DeviceConst);
static SObject DeviceConst_Object
(
    "DeviceConst",
    DeviceConstClass,
    0,
    0,
    "DeviceConst JS Object"
);

#define DEFINE_GPUSUB_FEATURE( feat, featid, desc ) \
    PROP_CONST( DeviceConst, feat, Device::feat );
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc) \
    PROP_CONST( DeviceConst, feat, Device::feat );
#define DEFINE_MCP_FEATURE(feat, featid, desc) \
    PROP_CONST( DeviceConst, feat, Device::feat );
#define DEFINE_SOC_FEATURE(feat, featid, desc) \
    PROP_CONST( DeviceConst, feat, Device::feat );
#include "core/include/featlist.h"
#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE

#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
    PROP_CONST(DeviceConst, GpuId, Device::GpuId);
#define DEFINE_X86_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
    PROP_CONST(DeviceConst, GpuId, Device::GpuId);
#define DEFINE_ARM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, ...) \
    PROP_CONST(DeviceConst, GpuId, Device::GpuId);
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
#undef DEFINE_X86_GPU
#undef DEFINE_ARM_GPU

#define DEFINE_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
    PROP_CONST(DeviceConst, LwId, Device::LwId);
#define DEFINE_OBS_LWL_DEV(...) // don't care
#define DEFINE_ARM_MFG_LWL_DEV(DevIdStart, DevIdEnd, ChipId, LwId, ...) \
    PROP_CONST(DeviceConst, LwId, Device::LwId);
#include "core/include/lwlinklist.h"
#undef DEFINE_ARM_MFG_LWL_DEV
#undef DEFINE_OBS_LWL_DEV
#undef DEFINE_LWL_DEV

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(DeviceConst, DeviceIdToString, 1, "Get string representation of DeviceId")
{
    STEST_HEADER(1, 1, "Usage: DeviceConst.DeviceIdToString(deviceId);");
    STEST_ARG(0, UINT32, deviceId);

    string idStr = Device::DeviceIdToString(static_cast<Device::LwDeviceId>(deviceId));
    if (OK != pJavaScript->ToJsval(idStr, pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in DeviceConst.DeiceIdToString()");
        return JS_FALSE;
    }
    return JS_TRUE;
}
