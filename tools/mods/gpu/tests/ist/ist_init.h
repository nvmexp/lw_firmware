/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

class GpuDevice;

namespace Ist
{
namespace Init
{
    struct InitGpuArgs
    {
        bool isIstMode;
        bool isFbBroken;
        bool isPreIstScriptUsed;
        UINT64 vbiosTimeoutMs;
    };

    /**
     * \brief Enumerate devices and map BAR0
     *        Optionally get a pointer to the gpu device
     * \param[in] isIstMode Determines which register to poll to determine devinit completion
     *            isFbBroken whether the broken Framebuffer flag is passed
     *            isPreIstScriptUsed Decide which register to poll determine IST devinit
     *                               completion
     *            fsArgs is the floorsweeping input argument from the command line
     */
    RC InitGpu
    (
        GpuDevice** ppGpuDevice,
        const InitGpuArgs& initGpuArgs,
        const std::string& fsArgs
    );
} // namespace Init
} // namespace Ist
