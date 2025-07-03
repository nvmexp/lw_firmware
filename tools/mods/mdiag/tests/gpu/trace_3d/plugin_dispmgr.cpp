/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/utils/types.h"
#include "t3plugin.h"

#include "gpu/include/dispchan.h"
#include "core/include/disp_test.h"
#include "core/include/display.h"
#include "gpu/include/gpudev.h"
#include "trace_3d.h"
#include "core/include/utility.h"

#include <vector>

namespace T3dPlugin
{

// Display manager
class DisplayManagerMods_impl : public DisplayManagerMods
{
public:

    friend DisplayManagerMods *DisplayManagerMods::Create(HostMods * pHost);

    explicit DisplayManagerMods_impl(HostMods *pHost)
    : m_pHost(pHost)
    {
        m_pGpuDevice = pHost->getT3dTest()->GetBoundGpuDevice();
    }

    virtual ~DisplayManagerMods_impl() {}

    virtual void *GetChannelByHandle(UINT32 handle) const
    {
        LwRmPtr pLwRm;
        return (void *)pLwRm->GetDisplayChannel(handle);
    }

    virtual INT32 MethodWrite(void *channel, UINT32 method, UINT32 data)
    {
        return ((DisplayChannel *)channel)->Write(method, data);
    }

    virtual INT32 FlushChannel(void *channel)
    {
        return ((DisplayChannel *)channel)->Flush();
    }

    virtual INT32 CreateContextDma(UINT32 chHandle,
        UINT32 flags, UINT32 size,
        const char *memoryLocation,
        UINT32 *dmaHandle)
    {
        return DispTest::CreateContextDma(chHandle,
            flags, size,
            memoryLocation,
            dmaHandle);
    }

    virtual INT32 GetDmaCtxAddr(UINT32 dmaHandle, void **addr)
    {
        DispTest::DmaContext *dmaCtx = DispTest::GetDmaContext(dmaHandle);
        if (0 == dmaCtx)
        {
            return -1;
        }
        *addr = dmaCtx->Address;
        return 0;
    }

    virtual INT32 GetDmaCtxOffset(UINT32 dmaHandle, UINT64 *offset)
    {
        DispTest::DmaContext *dmaCtx = DispTest::GetDmaContext(dmaHandle);
        if (0 == dmaCtx)
        {
            return -1;
        }
        *offset = dmaCtx->Offset;
        return 0;
    }

    INT32 RmControl5070(UINT32 ctrlCmdNum, void* params, size_t paramsSize)
    {
        Display* pDisplayDev = m_pGpuDevice->GetDisplay();

        if (!pDisplayDev)
        {
            ErrPrintf("%s: Display device has not been created "
                "before this 5070 ctrl call\n", __FUNCTION__);
            return -1;
        }

        RC rc = pDisplayDev->RmControl5070(ctrlCmdNum, params, paramsSize);
        if (OK != rc)
        {
            ErrPrintf("%s: 5070 ctrl call failed. rc message = %s\n",
                __FUNCTION__, rc.Message());
            return -1;
        }

        return 0;
    }

    INT32 GetActiveDisplayIDs(UINT32 *pDisplayIDs, size_t *pSizeInBytes)
    {
        Display* pDisplayDev = m_pGpuDevice->GetDisplay();

        // Parameter sanity check
        //
        if (!pDisplayDev)
        {
            ErrPrintf("%s: Display device has not been created "
                "before this call\n", __FUNCTION__);
            return -1;
        }

        size_t displayIDLimit = (*pSizeInBytes) / sizeof(UINT32);
        if (!pDisplayIDs || !displayIDLimit)
        {
            ErrPrintf("%s: Invalid parameter. Zero displayid buffer pointer or size! "
                "Can't return any display IDs\n", __FUNCTION__);
            return -1;
        }

        // Get dislay IDs
        //
        UINT32 activeDisplayBits = pDisplayDev->Selected();
        UINT32 activeDisplayCnt = Utility::CountBits(activeDisplayBits);

        // Return display IDs and buffer consumed;
        // Return ~0 if buffer is not big enough
        if (activeDisplayCnt > displayIDLimit)
        {
            *pSizeInBytes = ~0;
        }
        else
        {
            displayIDLimit = activeDisplayCnt;
            *pSizeInBytes = displayIDLimit * sizeof(UINT32);
        }

        UINT32 pos = 0;
        while (activeDisplayBits && (pos < displayIDLimit))
        {
            UINT32 remainedBits = activeDisplayBits & (activeDisplayBits - 1);
            pDisplayIDs[pos ++] = activeDisplayBits - remainedBits;
            activeDisplayBits = remainedBits;
        }

        return 0;
    }

    UINT32 GetMasterSubdevice()
    {
        Display* pDisplayDev = m_pGpuDevice->GetDisplay();

        // Parameter sanity check
        //
        if (!pDisplayDev)
        {
            ErrPrintf("%s: Display device has not been created "
                "before this call\n", __FUNCTION__);
            return ~0;
        }

        return pDisplayDev->GetMasterSubdevice();
    }

    INT32 GetConfigParams
    (
        UINT32 displayID,
        DisplayMngCfgParams *pParams
    )
    {
        // Parameter check
        //
        if (!pParams ||
            pParams->TypeID >= DisplayMngCfgParams::ParamTypeLast)
        {
            ErrPrintf("%s: Error - (pParams==NULL)  or (Unsupported ParamType)\n",
                __FUNCTION__);
            return -1;
        }

        return 0;
    }
private:

    HostMods * m_pHost;
    GpuDevice * m_pGpuDevice;
};

DisplayManagerMods * DisplayManagerMods::Create( HostMods * pHost )
{
    DisplayManagerMods_impl * pDisplayManager = new DisplayManagerMods_impl( pHost );
    return pDisplayManager;
}

} // namespace T3dPlugin

