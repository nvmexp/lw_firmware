/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Display engine VGA hardware coverage per request in bug 1664620

#include "core/include/cnslmgr.h"
#include "core/include/display.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "mods_reg_hal.h"
#include "core/include/platform.h"
#include "core/include/stdoutsink.h"
#include "core/include/utility.h"

#include "ctrl/ctrl0073.h"

class VgaHW : public GpuTest
{
    public:
        VgaHW();
        virtual bool IsSupported();
        virtual RC Setup();
        virtual RC Run();
        virtual RC Cleanup();

    private:
        RC SetVgaMode(UINT32 mode);
        RC CheckScanout();

        Surface2D m_DummyGoldenSurface;
        GpuGoldenSurfaces *m_pGGSurfs;
        Goldelwalues *m_pGolden;

        UINT32 m_VgaBaseOrg;
        Display::VgaTimingMode m_OrigVgaTimingMode;
        bool m_VgaIsEnabled;

        // User parameters:
        UINT32 m_MinLoadvIncrease;
        bool   m_TraceOn;
        bool   m_UseRgCrcs;

    public:
        SETGET_PROP(MinLoadvIncrease, UINT32);
        SETGET_PROP(TraceOn, bool);
        SETGET_PROP(UseRgCrcs, bool);
};

JS_CLASS_INHERIT(VgaHW, GpuTest,
                 "Display engine VGA hardware test.");

CLASS_PROP_READWRITE(VgaHW, MinLoadvIncrease, UINT32,
                     "Check that Loadv value increases by this amount minimum "
                     "after every setmode; value 0 disables this check");

CLASS_PROP_READWRITE(VgaHW, TraceOn, bool,
                     "Enable x86 emulator logging");

CLASS_PROP_READWRITE(VgaHW, UseRgCrcs, bool,
                     "Use the Raster Generator CRCs for verification");

VgaHW::VgaHW()
: m_pGGSurfs(nullptr)
, m_VgaBaseOrg(0)
, m_VgaIsEnabled(false)
, m_MinLoadvIncrease(2)
, m_TraceOn(false)
, m_UseRgCrcs(true) // per display HW, RG CRCs will catch VGA HW defect - see lwbugs/1755407/22
{
    SetName("VgaHW");

    m_pGolden = GetGoldelwalues();
}

bool VgaHW::IsSupported()
{
    if (!Platform::HasClientSideResman())
        return false;

    const Display::ClassFamily& dispClass = GetDisplay()->GetDisplayClassFamily();
    return ((dispClass == Display::EVO || dispClass == Display::LWDISPLAY) &&
            GetBoundGpuSubdevice()->IsPrimary());
}

RC VgaHW::Setup()
{
    RC rc;
    GpuDevice *pDevice = GetBoundGpuDevice();
    Display *pDisplay = GetDisplay();
    UINT32 deviceInst = pDevice->GetDeviceInst();

    m_VgaBaseOrg = GetBoundGpuSubdevice()->Regs().Read32(MODS_PDISP_VGA_BASE);


    CHECK_RC(GpuTest::Setup());

    CHECK_RC(GpuTest::AllocDisplay());

    MASSERT(m_pGGSurfs == NULL);
    m_pGGSurfs = new GpuGoldenSurfaces(pDevice);
    m_DummyGoldenSurface.SetWidth(160);
    m_DummyGoldenSurface.SetHeight(120);
    m_DummyGoldenSurface.SetColorFormat(ColorUtils::R8G8B8A8);
    CHECK_RC(m_DummyGoldenSurface.Alloc(pDevice));

    m_pGGSurfs->AttachSurface(&m_DummyGoldenSurface, "C", pDisplay->GetPrimaryDisplay());
    CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

    CHECK_RC(ConsoleManager::Instance()->PrepareForSuspend(pDevice));

    if (pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
        CHECK_RC(pDisplay->DetachAllDisplays());
    }

    pDisplay->FreeGpuResources(deviceInst);
    pDisplay->FreeRmResources(deviceInst);

    const bool restoreDefaultDisplay = true;
    if (!RmEnableVga(pDevice->GetDeviceInst(), restoreDefaultDisplay))
    {
        Printf(Tee::PriError, "Error: Unable to switch to VGA.");
        return RC::SET_MODE_FAILED;
    }
    m_VgaIsEnabled = true;

    // Force 640x480 timings, and ignore EDID
    m_OrigVgaTimingMode = pDisplay->GetVgaTimingMode();
    if (m_OrigVgaTimingMode != Display::VgaTimingMode::VGA_640x480_TIMING)
        CHECK_RC(pDisplay->SetVgaTimingMode(Display::VgaTimingMode::VGA_640x480_TIMING));

    return rc;
}

RC VgaHW::Cleanup()
{
    StickyRC rc;
    GpuDevice *pDevice = GetBoundGpuDevice();
    Display *pDisplay = GetDisplay();

    if (m_VgaIsEnabled)
    {
        // Restore INT10h to using panel EDID
        if (m_OrigVgaTimingMode != Display::VgaTimingMode::VGA_640x480_TIMING)
        {
            CHECK_RC(pDisplay->SetVgaTimingMode(m_OrigVgaTimingMode));
        }
#ifdef CLIENT_SIDE_RESMAN
        RmDisableVga(pDevice->GetDeviceInst());
        m_VgaIsEnabled = false;
#endif
    }

    rc = pDisplay->SetUseVgaCrc(0, 0, 0);

    if (pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        rc = pDisplay->GetDisplayMgr()->Activate();
    }
    else
    {
        rc = pDisplay->AllocGpuResources();
        rc = pDisplay->DetectRealDisplaysOnAllConnectors();
    }

    GetBoundGpuSubdevice()->Regs().Write32(MODS_PDISP_VGA_BASE, m_VgaBaseOrg);

    rc = ConsoleManager::Instance()->ResumeFromSuspend(pDevice);

    m_pGolden->ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;
    m_DummyGoldenSurface.Free();

    rc = GpuTest::Cleanup();

    return rc;
}

static void RestoreStdout()
{
    Tee::GetStdoutSink()->Enable();
}

RC VgaHW::Run()
{
    const UINT32 VGA_BASE_ALIGNMENT_BITS = 18;
    const UINT32 VGA_BASE_ALIGNMENT = 1 << VGA_BASE_ALIGNMENT_BITS;

    RC rc;
    GpuDevice *pDevice = GetBoundGpuDevice();
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    Display *pDisplay = GetDisplay();

    Surface2D vgaSurface;
    vgaSurface.SetWidth(640);
    vgaSurface.SetHeight(480);
    vgaSurface.SetColorFormat(ColorUtils::A8R8G8B8);
    vgaSurface.SetPitch(4*640);
    vgaSurface.SetType(LWOS32_TYPE_PRIMARY);
    vgaSurface.SetLocation(Memory::Fb);
    vgaSurface.SetProtect(Memory::ReadWrite);
    vgaSurface.SetLayout(Surface2D::Pitch);
    vgaSurface.SetDisplayable(true);
    vgaSurface.SetAlignment(VGA_BASE_ALIGNMENT);
    CHECK_RC(vgaSurface.Alloc(pDevice));
    CHECK_RC(vgaSurface.Map(pDisplay->GetMasterSubdevice()));
    UINT08 *vgaSurfaceBuf = (UINT08*)vgaSurface.GetAddress();

    UINT32 vgaSurfaceBaseValue = 0;
    pSubdevice->Regs().SetField(&vgaSurfaceBaseValue, MODS_PDISP_VGA_BASE_ADDR,
        static_cast<UINT32>(vgaSurface.GetVidMemOffset() >> VGA_BASE_ALIGNMENT_BITS));
    pSubdevice->Regs().SetField(&vgaSurfaceBaseValue,
        MODS_PDISP_VGA_BASE_TARGET_PHYS_LWM);
    pSubdevice->Regs().SetField(&vgaSurfaceBaseValue,
        MODS_PDISP_VGA_BASE_STATUS_VALID);
    pSubdevice->Regs().Write32(MODS_PDISP_VGA_BASE, vgaSurfaceBaseValue);

    CHECK_RC(SetVgaMode(0x3));

    string crcSuffix;
    if (pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        Display::VgaCrcType vgaType = m_UseRgCrcs ?
            Display::VGA_CRC_TYPE_RG : Display::VGA_CRC_TYPE_NONE;

        UINT32 index = 0;
        UINT32 protocol = 0;
        EvoRasterSettings raster;

        if (m_UseRgCrcs)
            CHECK_RC(pDisplay->GetVgaModeDetails(nullptr, nullptr, nullptr, &raster));
        else
            CHECK_RC(pDisplay->GetVgaModeDetails(&vgaType, &index, &protocol, &raster));

        CHECK_RC(pDisplay->SetUseVgaCrc(1 << vgaType, index, protocol));

        string orSuffix;
        if (!m_UseRgCrcs)
        {
            UINT32 orType = LW0073_CTRL_SPECIFIC_OR_TYPE_NONE;
            CHECK_RC(pDisplay->GetOrTypeFromVgaCrcType(vgaType, &orType));

            if (orType == LW0073_CTRL_SPECIFIC_OR_TYPE_NONE)
            {
                Printf(Tee::PriError, "VgaHW: Invalid OR type detected !\n");
                return RC::SOFTWARE_ERROR;
            }

            string orUniqueName;
            CHECK_RC(pDisplay->GetORNameIfUnique(orType, index, protocol, raster, orUniqueName));

            if ((orType == LW0073_CTRL_SPECIFIC_OR_TYPE_SOR) &&
                (protocol == LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B))
            {
                // Testing has shown the CRCs are the same:
                protocol = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A;
            }

            string orSuffix = Utility::StrPrintf("_Type%d_Protocol%d%s",
                                                 orType,
                                                 protocol,
                                                 orUniqueName.c_str());
        }
        else
        {
            orSuffix = "_RG";
        }

        crcSuffix = Utility::StrPrintf("_Width%d_Height%d%s",
            raster.ActiveX,
            raster.ActiveY,
            orSuffix.c_str());
    }
    else
    {
        // TODO: Query core channel head 0 for active raster width/height and
        // add this to the CRC suffix.
        CHECK_RC(pDisplay->SetUseVgaCrc(1 << Display::VGA_CRC_TYPE_RG, 0, 0));
        crcSuffix = "_RG";
    }

    Printf(GetVerbosePrintPri(), "VgaHW: Mode configuration = %s\n", crcSuffix.c_str());

    CHECK_RC(m_pGolden->OverrideSuffix(crcSuffix.c_str()));
    m_pGolden->SetCheckCRCsUnique(true);

    Utility::CleanupOnReturn<void> restoreStdout(&RestoreStdout);
    if (!Tee::GetStdoutSink()->IsEnabled())
    {
        restoreStdout.Cancel();
    }
    else
    {
        Tee::GetStdoutSink()->Disable();
    }

    UINT32 numLoops = GetTestConfiguration()->Loops();
    if (m_pGolden->GetAction() == Goldelwalues::Store)
        numLoops = 1;
    for (UINT32 loopIdx = 0; loopIdx < numLoops; loopIdx++)
    {
        Printf(GetVerbosePrintPri(), "VgaHW: Starting loop idx = %d\n", loopIdx);

        rc.Clear();

        // Text mode first:
        CHECK_RC(SetVgaMode(0x3));

        UINT08 charValue = '0';
        UINT08 attributeValue = 0x8;
        const UINT32 NUM_COL = 80;
        const UINT32 NUM_ROW = 25;
        const UINT32 CELL_SIZE = 8;
        for (UINT32 textY = 0; textY < NUM_ROW; textY++)
        {
            for (UINT32 textX = 0; textX < NUM_COL; textX++)
            {
                vgaSurfaceBuf[(NUM_COL*textY + textX)*CELL_SIZE + 0] = charValue++;
                vgaSurfaceBuf[(NUM_COL*textY + textX)*CELL_SIZE + 1] =
                    (attributeValue++) & 0x7F; // Don't enable blinking as this generates
                                               // even more CRC beyond blinking cursor
            }
            attributeValue += 11; // To test more attibute/char combinations
        }

        CHECK_RC(CheckScanout());

        m_pGolden->SetLoop(0);
        pDisplay->SetExpectBlinkingLwrsor(true);

        CHECK_RC(m_pGolden->Run());

        // Bug 1757451
        // On machines without a monitor connected, we observe 0x118 mode fails
        // with VESA failure error code 0x14F
        // Problem is blind boot resolution is 640x480 while 118 is 1024x768
        // Therefore use 0x12 mode which is 640x480 and compatible with blind boot
        // Should still catch VGA HW defects per VBIOS team, see http://lwbugs/1757451/7

        // Now the graphics mode:
        CHECK_RC(SetVgaMode(0x12));

        CHECK_RC(vgaSurface.FillPattern(1, 1, "gradient", "mfgdiag", "horizontal"));

        CHECK_RC(CheckScanout());

        m_pGolden->SetLoop(1);
        pDisplay->SetExpectBlinkingLwrsor(false);

        CHECK_RC(m_pGolden->Run());

    }

    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));

    return OK;
}

RC VgaHW::SetVgaMode(UINT32 mode)
{
#ifdef CLIENT_SIDE_RESMAN
    const bool windowed = false;
    const bool clearMemory = true;
    UINT32 eax = 0x4F02;
    UINT32 ebx =
          (mode & 0x00003FFF)
        | (windowed ? 0 : 1 << 14)
        | (clearMemory ? 0 : 1 << 15);
    UINT32 ecx = 0, edx = 0;

    RmCallVideoBIOS(GetBoundGpuDevice()->GetDeviceInst(), m_TraceOn,
        &eax, &ebx, &ecx, &edx, NULL);

    // Command completed successfully if AX=0x004F.
    if (0x004F != (eax & 0x0000FFFF))
    {
        Printf(Tee::PriError, "Unable to set VESA mode 0x%x, ax=0x%04x\n",
            mode, eax&0xFFFF);
        return RC::SET_MODE_FAILED;
    }

    return OK;
#else
    return RC::SOFTWARE_ERROR;
#endif
}

struct PollLoadvArgs
{
    ModsGpuRegAddress loadvRegister;
    UINT32 startingValue;
    UINT32 minExpectedValue;
    GpuSubdevice *pSubdevice;
};

static const UINT32 VGA_HEAD_IDX = 0; // Assume int 10h always uses "0";

static bool PollLoadvIncrease(void *args)
{
    PollLoadvArgs * pArgs = static_cast<PollLoadvArgs*>(args);

    UINT32 loadv = pArgs->pSubdevice->Regs().Read32(pArgs->loadvRegister, VGA_HEAD_IDX);

    const UINT32 startingValue = pArgs->startingValue;
    const UINT32 minExpectedValue = pArgs->minExpectedValue;

    if ((minExpectedValue < startingValue) && (loadv >= startingValue))
    {
        return false; // The counter needs to wrap-around first
    }

    return (loadv >= minExpectedValue);
}

RC VgaHW::CheckScanout()
{
    if (m_MinLoadvIncrease == 0)
        return OK;

    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    auto& Regs = pSubdevice->Regs();

    PollLoadvArgs pollArgs;

    if (Regs.IsSupported(MODS_PDISP_PIPE_IN_LOADV_COUNTER)) // EvoDisplay
    {
        pollArgs.loadvRegister = MODS_PDISP_PIPE_IN_LOADV_COUNTER;
    }
    else if (Regs.IsSupported(MODS_PDISP_RG_IN_LOADV_COUNTER)) // LwDisplay
    {
        pollArgs.loadvRegister = MODS_PDISP_RG_IN_LOADV_COUNTER;
    }
    else
    {
        // We don't know which LOADV_COUNTER register to use
        return RC::UNSUPPORTED_FUNCTION;
    }

    pollArgs.startingValue = pSubdevice->Regs().Read32(
        pollArgs.loadvRegister, Display::VGA_HEAD_IDX);
    pollArgs.minExpectedValue = pollArgs.startingValue + m_MinLoadvIncrease;
    pollArgs.pSubdevice = pSubdevice;

    StickyRC rc = POLLWRAP_HW(PollLoadvIncrease, &pollArgs,
        1000*m_MinLoadvIncrease/30 + GetTestConfiguration()->TimeoutMs());

    if (rc != OK)
    {
        Printf(Tee::PriError,
            "Error waiting for LOADV increase from 0x%08x to 0x%08x, current value = 0x%08x.\n",
            pollArgs.startingValue,
            pollArgs.minExpectedValue,
            Regs.Read32(pollArgs.loadvRegister, VGA_HEAD_IDX));
    }

    return rc;
}
