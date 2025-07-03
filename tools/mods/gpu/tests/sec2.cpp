/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014,2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "class/cl95a1.h" // LW95A1_TSEC
#include "gpu/include/gpudev.h"
#include "gputest.h"
#include "gpu/include/gralloc.h"
#include "lwSha256.h"
#include "core/include/platform.h"
#include "tsec_drv.h"
#include "core/include/utility.h"
#include "mods_reg_hal.h"

extern unsigned char gfe_pkey[];
extern unsigned int gfe_pkey_size;
extern unsigned char gfe_pkey_gm206[];
extern unsigned int gfe_pkey_size_gm206;
extern unsigned char gfe_pkey_gp102[];
extern unsigned int gfe_pkey_size_gp102;
extern unsigned char gfe_pkey_tu102[];
extern unsigned int gfe_pkey_size_tu102;

class Sec2 : public GpuTest
{
public:
    Sec2();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    enum
    {
        s_SEC2 = 0
    };

    TsecAlloc m_TsecAlloc;

    GpuTestConfiguration *m_pTestConfig;

    LwRm::Handle m_hCh;
    Channel     *m_pCh;

    Surface2D m_EcidSign;
    Surface2D m_PrivKey;
    Surface2D m_Ecid;
    Surface2D m_Semaphore;

    RC AllocSurface(Surface2D *pSurface, UINT32 Size);
};

JS_CLASS_INHERIT(Sec2, GpuTest, "Sec2 test.");

struct PollSemaphore_Args
{
    UINT32 *Semaphore;
    UINT32  ExpectedValue;
};

static bool GpuPollSemaVal
(
    void * Args
)
{
    PollSemaphore_Args * pArgs = static_cast<PollSemaphore_Args*>(Args);

    return (MEM_RD32(pArgs->Semaphore) == pArgs->ExpectedValue);
}

Sec2::Sec2()
: m_pTestConfig(GetTestConfiguration())
, m_hCh(0)
, m_pCh(NULL)
{
}

bool Sec2::IsSupported()
{
    if ((Platform::GetSimulationMode() != Platform::Hardware) &&
        (Platform::GetSimulationMode() != Platform::RTL))
    {
        Printf(Tee::PriLow, "%s only supported on hardware/RTL\n", GetName().c_str());
        return false;
    }

    if (GetBoundGpuDevice()->HasBug(1746899) &&
        GetBoundGpuSubdevice()->Regs().Test32(MODS_PSEC_SCP_CTL_STAT_DEBUG_MODE_DISABLED))
    {
        // supported only on debug fused boards in GP10x until bug 1746899 is fixed
        Printf(Tee::PriLow, "%s only supported on debug fused GP10x boards\n", GetName().c_str());
        return false;
    }

    if (!m_TsecAlloc.IsSupported(GetBoundGpuDevice()))
    {
        Printf(Tee::PriLow, "%s : Current GPU does not support the Tsec engine\n",
               GetName().c_str());
        return false;
    }

    if (!GetBoundTestDevice()->HasFeature(Device::GPUSUB_TSEC_SUPPORTS_GFE_APPLICATION))
    {
        Printf(Tee::PriLow, "%s : Current GPU TSEC does not support the GFE application\n",
               GetName().c_str());
        return false;
    }
    return true;
}

RC Sec2::AllocSurface(Surface2D *pSurface, UINT32 Size)
{
    RC rc;
    pSurface->SetColorFormat(ColorUtils::Y8);
    pSurface->SetLayout(Surface2D::Pitch);
    pSurface->SetHeight(1);
    pSurface->SetWidth(Size);
    pSurface->SetAlignment(256);
    pSurface->SetLocation(m_pTestConfig->NotifierLocation());
    CHECK_RC(pSurface->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pSurface->Map());

    return rc;
}

RC Sec2::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    CHECK_RC(AllocSurface(&m_EcidSign,
        LW95A1_GFE_READ_ECID_RSA_1024_SIG_SIZE_BYTES));
    CHECK_RC(AllocSurface(&m_PrivKey,
        LW95A1_GFE_READ_ECID_RSA_1024_KEY_SIZE_BYTES));
    CHECK_RC(AllocSurface(&m_Ecid, sizeof(gfe_read_ecid_param)));
    CHECK_RC(AllocSurface(&m_Semaphore, 256));

    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                      &m_hCh,
                                                      &m_TsecAlloc));
    CHECK_RC(m_pCh->SetObject(s_SEC2, m_TsecAlloc.GetHandle()));

    return rc;
}

RC Sec2::Run()
{
    RC rc;

    gfe_read_ecid_param readEcidParam;
    memset(&readEcidParam, 0, sizeof(readEcidParam));

    for (UINT32 snindex = 0;
        snindex < NUMELEMS(readEcidParam.serverNonce);
        snindex++)
    {
        readEcidParam.serverNonce[snindex] = 1 + snindex;
    }
    readEcidParam.programID = 0xabc;
    readEcidParam.sessionID = 0x5aa5;
    readEcidParam.signMode = LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024;
    readEcidParam.retCode = 0xbad00000;

    Platform::VirtualWr(m_Ecid.GetAddress(), &readEcidParam,
        sizeof(readEcidParam));

    if ((GetBoundGpuSubdevice()->DeviceId() ==  Gpu::GM200) ||
        (GetBoundGpuSubdevice()->DeviceId() ==  Gpu::GM204) ||
        (GetBoundGpuSubdevice()->DeviceId() ==  Gpu::GM107))
    {
        MASSERT(gfe_pkey_size == LW95A1_GFE_READ_ECID_RSA_1024_KEY_SIZE_BYTES);
        Platform::VirtualWr(m_PrivKey.GetAddress(), gfe_pkey, gfe_pkey_size);
    }
    else if ((GetBoundGpuSubdevice()->DeviceId() == Gpu::GM206) ||
        (GetBoundGpuSubdevice()->DeviceId() == Gpu::GP100))
    {
        MASSERT(gfe_pkey_size_gm206 == LW95A1_GFE_READ_ECID_RSA_1024_KEY_SIZE_BYTES);
        Platform::VirtualWr(m_PrivKey.GetAddress(), gfe_pkey_gm206, gfe_pkey_size_gm206);
    }
    else if ((GetBoundGpuSubdevice()->DeviceId() >= Gpu::GP102) &&
        (GetBoundGpuSubdevice()->DeviceId() < Gpu::TU102))
    {
        MASSERT(gfe_pkey_size_gp102 == LW95A1_GFE_READ_ECID_RSA_1024_KEY_SIZE_BYTES);
        Platform::VirtualWr(m_PrivKey.GetAddress(), gfe_pkey_gp102, gfe_pkey_size_gp102);
    }
    else if (GetBoundGpuSubdevice()->DeviceId() >= Gpu::TU102)
    {
        MASSERT(gfe_pkey_size_tu102 == LW95A1_GFE_READ_ECID_RSA_1024_KEY_SIZE_BYTES);
        Platform::VirtualWr(m_PrivKey.GetAddress(), gfe_pkey_tu102, gfe_pkey_size_tu102);
    }
    else
    {
        Printf(Tee::PriNormal, "Error: Sec2 GFE test is not supported on this device.\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    CHECK_RC(m_Semaphore.Fill(0));

    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_SET_APPLICATION_ID,
        LW95A1_SET_APPLICATION_ID_ID_GFE));

    // Is this needed?:
    //CHECK_RC(m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0));

    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_GFE_SET_ECID_SIGN_BUF,
        DRF_NUM(95A1, _GFE_SET_ECID_SIGN_BUF, _PARAM_OFFSET,
        UINT32(m_EcidSign.GetCtxDmaOffsetGpu()/256))));
    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_GFE_SET_PRIV_KEY_BUF,
        DRF_NUM(95A1, _GFE_SET_PRIV_KEY_BUF, _PARAM_OFFSET,
        UINT32(m_PrivKey.GetCtxDmaOffsetGpu()/256))));
    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_GFE_READ_ECID,
        DRF_NUM(95A1, _GFE_READ_ECID, _PARAM_OFFSET,
        UINT32(m_Ecid.GetCtxDmaOffsetGpu()/256))));

    UINT64 semaphoreOffsetGpu = m_Semaphore.GetCtxDmaOffsetGpu();
    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_SEMAPHORE_A,
        DRF_NUM(95A1, _SEMAPHORE_A, _UPPER,
            0xFF & LwU64_HI32(semaphoreOffsetGpu))));
    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_SEMAPHORE_B,
        DRF_NUM(95A1, _SEMAPHORE_B, _LOWER, LwU64_LO32(semaphoreOffsetGpu))));
    const UINT32 payloadValue = 0x5aa50001;
    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_SEMAPHORE_C,
        DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, payloadValue)));

    CHECK_RC(m_pCh->Write(s_SEC2, LW95A1_EXELWTE,
        DRF_DEF(95A1, _EXELWTE, _NOTIFY, _ENABLE) |
        DRF_DEF(95A1, _EXELWTE, _NOTIFY_ON, _END) |
        DRF_DEF(95A1, _EXELWTE, _AWAKEN, _DISABLE)));

    CHECK_RC(m_pCh->Flush());

    PollSemaphore_Args completionArgs =
    {
        (UINT32*)m_Semaphore.GetAddress(),
        payloadValue
    };

    CHECK_RC(POLLWRAP_HW(GpuPollSemaVal, &completionArgs,
        m_pTestConfig->TimeoutMs()));

    Platform::VirtualRd(m_Ecid.GetAddress(), &readEcidParam,
        sizeof(readEcidParam));

    if (readEcidParam.retCode != LW95A1_AES_ECB_CRYPT_ERROR_NONE)
    {
        Printf(Tee::PriNormal, "Error: Unexpected retCode = %d\n",
            (UINT32)readEcidParam.retCode);
        return RC::UNEXPECTED_RESULT;
    }

    vector<UINT32> rawEcid;
    CHECK_RC(GetBoundGpuSubdevice()->GetRawEcid(&rawEcid));
    if (rawEcid.size() != 4)
    {
        Printf(Tee::PriNormal, "Error: Unexpected raw ecid size.\n");
        return RC::SOFTWARE_ERROR;
    }

    //Note: The rawEcid data now includes the reserved field which increased its size to 4.
    //      However this algorithm does not account for the increased size so discard the extra
    //      entry which is all zeros anyway.
    UINT32 gfeEcid[3] = {rawEcid[3], rawEcid[2], rawEcid[1]};

    gfe_devInfo ecidCpuHash;
    memset(&ecidCpuHash, 0, sizeof(ecidCpuHash));

    lw_sha256((unsigned char*)&gfeEcid[0], sizeof(gfeEcid),
        &ecidCpuHash.ecidSha2Hash[0]);

    if (memcmp(&readEcidParam.dinfo.ecidSha2Hash[0],
               &ecidCpuHash.ecidSha2Hash[0],
               sizeof(ecidCpuHash.ecidSha2Hash)) != 0)
    {
        Printf(Tee::PriNormal, "Error: Unexpected hash value.\n");

        Printf(Tee::PriNormal, "Expected values:\n");
        for (UINT32 idx = 0; idx < sizeof(ecidCpuHash.ecidSha2Hash); idx++)
        {
            Printf(Tee::PriNormal, " %02x", ecidCpuHash.ecidSha2Hash[idx]);
        }
        Printf(Tee::PriNormal, "\nRead values:\n");
        for (UINT32 idx = 0; idx < sizeof(ecidCpuHash.ecidSha2Hash); idx++)
        {
            Printf(Tee::PriNormal, " %02x",
                readEcidParam.dinfo.ecidSha2Hash[idx]);
        }
        Printf(Tee::PriNormal, "\n");

        return RC::DATA_MISMATCH;
    }

    return rc;
}

RC Sec2::Cleanup()
{
    StickyRC rc;

    m_TsecAlloc.Free();

    rc = GetTestConfiguration()->FreeChannel(m_pCh);
    m_pCh = NULL;
    m_hCh = 0;

    m_EcidSign.Free();
    m_PrivKey.Free();
    m_Ecid.Free();
    m_Semaphore.Free();

    rc = GpuTest::Cleanup();

    return rc;
}
