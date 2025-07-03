/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PROFILEKEY_H
#define INCLUDED_PROFILEKEY_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_TYPES_H
#include "mdiag/utils/types.h"
#endif

#include "mdiag/utils/CrcInfo.h"
#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/resource/lwgpu/SurfaceReader.h"

#include "GpuVerif.h"

class ProfileKey
{
public:
    static bool Generate
    (
        GpuVerif*            verifier,
        const char*           suffix,      // crc suffix (eg., color or zeta)
        const CheckDmaBuffer* pCheck,      // check dma buffer
        int                   ColorBuffer, // is this for a color buffer?
        bool                  search,      // search the crc chain or grab first element
        UINT32                gpuSubdevice, // subdevice for generating the string
        string&               profileKey,  // crc profile key to be generated
        bool                  ignoreLocalLead = true // ignore local lead crc's
    );

    static void CleanUpProfile(GpuVerif* verifier, UINT32 gpuSubdevice);

private:
    static bool BuildKeyColorZ(GpuVerif* verifier, int ColorBuffer, UINT32 subdev, string& tempKey);
};

#endif /* INCLUDED_PROFILEKEY_H */
