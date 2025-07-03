/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_vga.cpp - DispTest class - VGA related methods
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
//      Originally part of "disp_test.cpp" - broken out 25 October 2005
//
//      Written by: Matt Craighead, Larry Coffey, et al
//      Date:       29 July 2004
//
//      Routines in this module:
//      GetFbVgaPhysAddr                Map in VGA backdoor memory
//      ReleaseFbVgaPhysAddr            Release the previously mapped backdoor memory
//
//      IsoInitVGA                      Initilizes VGA/display
//      IsoProgramVPLL                  Programs VPLL coefficients based on desired clock frequency
//      IsoInitLiteVGA                  Light init for VGA tests which does not produce an image
//      IsoShutDowlwGA                  Routine that does various calls required to shutdown vga and ensures that we exit cleanly
//      IsoIndexedRegWrVGA              Routine writes indirectly to I/O port
//      IsoIndexedRegRdVGA              Routine reads indirectly to I/O port
//      IsoAttrRegWrVGA                 Routine writes a value to the VGA attribute controller
//      BackdoorVgaInit                 Initialize backdoor memory VGA accesses
//      BackdoorVgaRelease              Release backdoor VGA memory accesses
//      FillVGABackdoor                 Fill VGA memory from a given file through the backdoor
//      SaveVGABackdoor                 Save VGA memory to a given file from the backdoor
//      BackdoorVgaWr08                 Write a byte through the backdoor to the VGA framebuffer
//      BackdoorVgaRd08                 Read a byte through the backdoor from the VGA framebuffer
//      BackdoorVgaWr16                 Write a word through the backdoor to the VGA framebuffer
//      BackdoorVgaRd16                 Read a word through the backdoor from the VGA framebuffer
//      BackdoorVgaWr32                 Write a dword through the backdoor to the VGA framebuffer
//      BackdoorVgaRd32                 Read a dword through the backdoor from the VGA framebuffer
//      VgaCrlwpdate                    Get the VGA CRCs for both heads of interest CRC event;  update the CRC event lists as appropriate
//
//      ReadCrtc                        Read the CRTC register at Index on Head
//      WriteCrtc                       Write the Crtc register at Index on Head with Data
//      ReadSr                          Read the SR register at Index on Head
//      WriteSr                         Write the SR register at Index on Head with Data
//      ReadAr                          Read the AR register at Index on Head
//      WriteAr                         Write the AR register at Index on Head with Data
//      UpdateNow                       This functions does a read-modify-write for sending an update
//      CalcClockSetup_TwoStage         Helper function to program custom pclk for VGA tests; it will return coefficients that can program within +- .5% of the specified out_clk
//
#include <stdlib.h>
#include "Lwcm.h"
#include "lwcmrsvd.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dispchan.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <errno.h>

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#include "lw50/dev_disp.h"

#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

namespace DispTest
{
    // No local ("static") routines needed

    // No access to DispTest member data needed
}

/****************************************************************************************
 * DispTest::GetFbVgaPhysAddr
 *
 *  Functional Description
 *  - Map in VGA backdoor memory
 ****************************************************************************************/
void* DispTest::GetFbVgaPhysAddr()
{
    UINT08* mapped = NULL;

    LwRmPtr pLwRm;

    pLwRm->MapFbMemory(DispTest::GetBoundGpuSubdevice()->RegRd32(LW_PDISP_VGA_BASE) &0xffffff00,
                       VGA_BACKDOOR_SIZE, (void**)&mapped, 0, DispTest::GetBoundGpuSubdevice());

    Printf(Tee::PriHigh, "GetFbVgaPhysAddr: VGA base mapped to 0x%08llx\n", (UINT64)(LwUPtr)mapped);
    return mapped;
}

/****************************************************************************************
 * DispTest::ReleaseFbVgaPhysAddr
 *
 *  Arguments Description:
 *  - mapped_addr: Previously mapped backdoor memory address
 *
 *  Functional Description
 *  - Release the previously mapped backdoor memory
 ****************************************************************************************/
void DispTest::ReleaseFbVgaPhysAddr(void* mapped_addr)
{
    LwRmPtr pLwRm;
    pLwRm->UnmapFbMemory((void*)mapped_addr, 0, DispTest::GetBoundGpuSubdevice());
    return;
}

/****************************************************************************************
 * DispTest::IsoInitVGA
 *
 *  Arguments Description:
 *  - head: head number
 *  - dac_index: dac number
 *  - raster_width:
 *  - raster_height:
 *  - is_rvga:
 *  - en_legacy_res:
 *  - set_active_rstr:
 *  - set_pclk:
 *  - pclk_freq:
 *  - sim_turbo:
 *  - emu_run:
 *  - timeout_ms:
 *
 *  Functional Description:
 *  - Initilizes VGA/display
 ***************************************************************************************/
RC DispTest::IsoInitVGA(UINT32 head,
                        UINT32 dac_index,
                        UINT32 raster_width,
                        UINT32 raster_height,
                        UINT32 is_rvga,
                        UINT32 en_legacy_res,
                        UINT32 set_active_rstr,
                        UINT32 set_pclk,
                        UINT32 pclk_freq,
                        UINT32 sim_turbo,
                        UINT32 emu_run,
                        FLOAT64 timeout_ms)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::IsoInitVGA - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->IsoInitVGA(head,
                                                dac_index,
                                                raster_width,
                                                raster_height,
                                                is_rvga,
                                                en_legacy_res,
                                                set_active_rstr,
                                                set_pclk,
                                                pclk_freq,
                                                sim_turbo,
                                                emu_run,
                                                timeout_ms));
}

/****************************************************************************************
 * DispTest::VGASetClocks
 *
 *  Arguments Description:
 *  - head: head number
 *  - set_pclk: indicates pclk_period arg was detected
 *  - pclk_freq: pixel clock frequency in kHz
 *  - dac_index: The dac the head is connected to
 *  - sim_turbo: use escapewrites to force the clock
 *  - emu_run: program clocks for emulation runs
 *  - timeout_ms: what timeout to use on the polls
 *
 *  Functional Description:
 *  - performs the register writes to set the clock frequency for vga
 *    The -pclk_period argument is typically used to prevent RG underflows while scaling
 *    is enalbed.  If this argument exists, set_pclk is expected to be non-zero.  This
 *    indicates that the pclk should be configured to pclk_freq.  If this argument was
 *    not specified, then set_pclk will be zero and pclk_freq is a default pclk value.
 *    RTL sims: set_pclk causes the VPLL to be configured to pclk_freq and
 *              LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK_MODE to CLK_LWSTOM.
 *              sim_turbo does the same as set_pclk, with the addition of performing
 *              some EscapeWrites.
 *              If neither set_pclk and sim_turbo are true, the VPLL will not be
 *              programmed and CLOCK_MODE will be set to CLK_25.
 *    Emu sims: The emu_run flag distinguishes between emulation and silicon runs.
 *              set_pclk causes the VPLL pdiv to be hacked for emulation runs.  Since
 *              the -pclk_period argument is typically used to prevent RG underflows,
 *              the smallest pdiv value that keeps emu_pclk less than or equal to
 *              rtl_pclk will be used.  CLOCK_MODE will be set to CLK_LWSTOM.
 *              Otherwise, the VPLL will not be programmed and CLOCK_MODE will be
 *              set to CLK_25.  sim_turbo is ignored for emu runs.
 *     HW sims: CLOCK_MODE will be set to CLK_25.  The VPLL will not be programmed.
 *              This prevents HW from running too fast so that the test is able to
 *              poll deterministically on cursor/text blink phase in order to capture
 *              the correct frame, and to inject Updates fast enough for continuous
 *              capture tests.
 ***************************************************************************************/
RC DispTest::VGASetClocks(UINT32 head,
                          UINT32 set_pclk,
                          UINT32 pclk_freq,
                          UINT32 dac_index,
                          UINT32 sim_turbo,
                          UINT32 emu_run,
                          FLOAT64 timeout_ms)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::VGASetClocks - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->VGASetClocks(head,
                                                  set_pclk,
                                                  pclk_freq,
                                                  dac_index,
                                                  sim_turbo,
                                                  emu_run,
                                                  timeout_ms));
}

/****************************************************************************************
 * DispTest::IsoProgramVPLL
 *
 *  Arguments Description:
 *  - head: head number
 *  - pclk_freq: pixel clock frequency in kHz
 *  - mode: clock mode (ie, which VPLL to program, 25MHz/28MHz/custom)
 *  - emu_run: program clocks for emulation runs
 *
 *  Functional Description:
 *  - callwlates VPLL coefficients and programs VPLL
 ***************************************************************************************/
RC DispTest::IsoProgramVPLL(UINT32 head,
                            UINT32 pclk_freq,
                            UINT32 mode,
                            UINT32 emu_run)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::IsoProgramVPLL - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->IsoProgramVPLL(head,
                                                    pclk_freq,
                                                    mode,
                                                    emu_run));
}

/****************************************************************************************
 * DispTest::IsoInitLiteVGA
 *
 *  Arguments Description:
 *  is_rvga - 1 - rVGA mode
 *            0 - lwGA mode
 *
 *  Functional Description:
 *  - Light init for VGA tests which does not produce an image
 ***************************************************************************************/
RC DispTest::IsoInitLiteVGA(UINT32 is_rvga)
{
    RC rc = OK;

    // Only need to process for ISO displays
    if (!DispTest::GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
        return 0;

    // Enable VGA Subsystem
    CHECK_RC(Platform::PioWrite08(LW_VIO_VSE2,DRF_NUM(_VIO,_VSE2,_VIDSYS_EN,1)));
    // Enable the color index/data crtc registers
    CHECK_RC(Platform::PioWrite08(LW_VIO_MISC__WRITE,
              DRF_NUM(_VIO,_MISC,_IO_ADR,LW_VIO_MISC_IO_ADR_03DX)
              ));
    // Enable Writing to extended CR registers
    CHECK_RC(DispTest::IsoIndexedRegWrVGA(LW_CIO_CRX__COLOR, LW_CIO_CRE_LOCK__INDEX,
            DRF_NUM(_CIO,_CRE_LOCK,_EXT_REG_ACCESS,LW_CIO_CRE_LOCK_EXT_REG_ACCESS_UNLOCK_RW)));
    // Set the VGA_BASE registers so that we can access FB memory
    CHECK_RC(DispTest::RmaRegWr32(
            LW_PDISP_VGA_BASE,
            DRF_NUM(_PDISP, _VGA_BASE, _TARGET, LW_PDISP_VGA_BASE_TARGET_PHYS_LWM) |
            DRF_NUM(_PDISP, _VGA_BASE, _STATUS, LW_PDISP_VGA_BASE_STATUS_VALID) |
            DRF_NUM(_PDISP, _VGA_BASE, _ADDR, LW_PDISP_VGA_BASE_ADDR_INIT)
            ));

    // Turn Off Auto-update and set rVGA/lwGA
    UINT08 value = 0;
    CHECK_RC(DispTest::IsoIndexedRegRdVGA(LW_CIO_CRX__COLOR, LW_CIO_CRE_DISPCTL__INDEX, value));
    value = FLD_SET_DRF_NUM(_CIO, _CRE_DISPCTL, _AUTO_UPDATE_MODE, 0, value);
    value = FLD_SET_DRF_NUM(_CIO, _CRE_DISPCTL, _RVGA_MODE, is_rvga, value);
    CHECK_RC(DispTest::IsoIndexedRegWrVGA(LW_CIO_CRX__COLOR, LW_CIO_CRE_DISPCTL__INDEX, value));

    return OK;
}

/****************************************************************************************
 * DispTest::IsoShutDowlwGA
 *
 *  Arguments Description:
 *  - head: head number
 *  - dac_index: dac number
 *  - timeout_ms:
 *
 *  Functional Description:
 *  - Routine that does various calls required to shutdown vga and ensures that we exit cleanly
 ***************************************************************************************/
RC DispTest::IsoShutDowlwGA(UINT32 head, UINT32 dac_index, FLOAT64 timeout_ms)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::IsoShutDowlwGA - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->IsoShutDowlwGA(head, dac_index, timeout_ms));
}

/****************************************************************************************
 * DispTest::IsoIndexedRegWrVGA
 *
 *  Arguments Description:
 *  - port:
 *  - index:
 *  - data:
 *
 *  Functional Description:
 *  - Routine writes indirectly to I/O port
 ***************************************************************************************/
RC DispTest::IsoIndexedRegWrVGA(UINT16 port, UINT08 index, UINT08 data)
{
    // Use a single 16-bit write to write both index and data
    return Platform::PioWrite16(port, index | (data << 8));
}

/****************************************************************************************
 * DispTest::IsoIndexedRegRdVGA
 *
 *  Arguments Description:
 *  - port:
 *  - index:
 *  - data:
 *
 *  Functional Description:
 *  - Routine reads indirectly to I/O port
 ***************************************************************************************/
RC DispTest::IsoIndexedRegRdVGA(UINT16 port, UINT08 index, UINT08& data)
{
    RC rc;
    UINT08 ret = 0;

    // Write index, then read value
    CHECK_RC(Platform::PioWrite08(port, index));
    CHECK_RC(Platform::PioRead08(port+1, &ret));

    data = ret;

    return OK;
}

/****************************************************************************************
 * DispTest::IsoAttrRegWrVGA
 *
 *  Arguments Description:
 *  - reg:
 *  - val:
 *
 *  Functional Description:
 *  - Routine writes a value to the VGA attribute controller
 ***************************************************************************************/
RC DispTest::IsoAttrRegWrVGA(UINT32 reg, UINT32 val)
{
    UINT08 temp;
    RC r = Platform::PioRead08(0x3da, &temp); // reads from STAT register for color adapter
    if (r != OK)
    {
        Printf(Tee::PriHigh, "*** IsoAttrRegWrVGA Error: couldn't read index from 0x3da\n");
        return 1;
    }
    r = Platform::PioRead08(0x3ba, &temp); // reads from STAT register for mono
    if (r != OK)
    {
        Printf(Tee::PriHigh, "*** IsoAttrRegWrVGA Error: couldn't read index from 0x3ba");
        return 1;
    }
    // one of these STAT reads will reset index/data ff

    r = Platform::PioWrite08(0x3c0, reg); // write the attr register index
    if (r != OK)
    {
        Printf(Tee::PriHigh, "*** IsoAttrRegWrVGA Error: couldn't write the attr register index\n");
        return 1;
    }
    r = Platform::PioWrite08(0x3c0, val); // write the data
    if (r != OK)
    {
        Printf(Tee::PriHigh, "*** IsoAttrRegWrVGA Error: couldn't write %d\n", val);
        return 1;
    }
    r = Platform::PioWrite08(0x3c0, reg | 0x20);
    if (r != OK)
    {
        Printf(Tee::PriHigh, "*** IsoAttrRegWrVGA Error: couldn't write %d\n", reg | 0x20);
        return 1;
    }

    return 0;
}

/****************************************************************************************
 * DispTest::BackdoorVgaInit
 *
 *  Functional Description
 *  - Initialize backdoor memory VGA accesses
 ****************************************************************************************/
RC DispTest::BackdoorVgaInit()
{
    if( LwrrentDevice()->FbVGA_address != 0 )
        Printf(Tee::PriHigh, "NOTE: BackdoorVgaInit found FbVGA_address already set.  No need to call BackdoorVgaInit() multiple times.\n");
    else
        LwrrentDevice()->FbVGA_address = (UINT08*)GetFbVgaPhysAddr();
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaRelease
 *
 *  Functional Description
 *  - Release backdoor VGA memory accesses
 ****************************************************************************************/
RC DispTest::BackdoorVgaRelease()
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "NOTE: BackdoorVgaRelease found FbVGA_address==NULL.  No need to call BackdoorVgaRelease() multiple times.\n");
    }else{
        ReleaseFbVgaPhysAddr(LwrrentDevice()->FbVGA_address);
        LwrrentDevice()->FbVGA_address = 0;
    }
    return OK;
}

/****************************************************************************************
 * DispTest::FillVGABackdoor
 *
 *  Arguments Description:
 *  - filename
 *  - offset
 *  - size
 *
 *  Functional Description
 *  - Fill VGA memory from a given file through the backdoor
 ****************************************************************************************/
RC DispTest::FillVGABackdoor(string filename, UINT32 offset, UINT32 size)
{
    RC rc = OK;
    FileHolder file;
    UINT08* mapped;

    LwRmPtr pLwRm;

    Printf (Tee::PriHigh, "FillVGABackdoor(%s, offset=0x%05x, size=0x%05x)\n",
           filename.c_str(), offset, size);

    // total must be within 16MB
    MASSERT((size+offset) <= VGA_BACKDOOR_SIZE);

    UINT32 ReadValue = 0;
    CHECK_RC( DispTest::RmaRegRd32(LW_PDISP_VGA_BASE, &ReadValue) );
    ReadValue &= DRF_SHIFTMASK(LW_PDISP_VGA_BASE_ADDR);
    pLwRm->MapFbMemory (ReadValue, VGA_BACKDOOR_SIZE, (void**)&mapped, 0, DispTest::GetBoundGpuSubdevice());

    Printf (Tee::PriHigh, "FillVGABackdoor: VGA base mapped to 0x%08llx\n", (UINT64)(LwUPtr)mapped);

    vector<UINT08> buf(size);

    // open the file
    if (OK != file.Open(filename, "rb"))
    {
        Printf (Tee::PriHigh, "FillVGABackdoor: Can't Open File (%s)\n", filename.c_str());
        pLwRm->UnmapFbMemory ((void*)mapped, 0, DispTest::GetBoundGpuSubdevice());
        return rc;
    }

    if (size != fread(&buf[0], 1, size, file.GetFile()))
    {
        rc = RC::FILE_READ_ERROR;
        Printf (Tee::PriHigh, "FillVGABackdoor: Can't Read File (%s)\n", filename.c_str());
    }

    Platform::MemCopy (mapped+offset, &buf[0], size);
    Platform::FlushCpuWriteCombineBuffer ();

    pLwRm->UnmapFbMemory ((void*)mapped, 0, DispTest::GetBoundGpuSubdevice());

    return rc;
}

/****************************************************************************************
 * DispTest::SaveVGABackdoor
 *
 *  Arguments Description:
 *  - filename
 *  - offset
 *  - size
 *
 *  Functional Description
 *  - Save VGA memory to a given file from the backdoor
 ****************************************************************************************/
RC DispTest::SaveVGABackdoor (string filename, UINT32 offset, UINT32 size)
{
    RC rc = OK;
    FileHolder file;
    UINT08* mapped;

    LwRmPtr pLwRm;

    Printf (Tee::PriHigh, "SaveVGABackdoor(%s, offset=0x%05x, size=0x%05x)\n",
           filename.c_str(), offset, size);

    // total must be within 16MB
    MASSERT((size+offset) <= VGA_BACKDOOR_SIZE);

    vector<UINT08> buf(size);

    UINT32 ReadValue = 0;
    CHECK_RC( DispTest::RmaRegRd32(LW_PDISP_VGA_BASE, &ReadValue) );
    ReadValue &= DRF_SHIFTMASK(LW_PDISP_VGA_BASE_ADDR);
    pLwRm->MapFbMemory (ReadValue, VGA_BACKDOOR_SIZE, (void**)&mapped, 0, DispTest::GetBoundGpuSubdevice());

    Printf (Tee::PriHigh, "SaveVGABackdoor: VGA base mapped to 0x%08llx\n", (UINT64)(LwUPtr)mapped);

    // Copy over framebuffer memory and release the fb
    Platform::MemCopy (&buf[0], mapped+offset, size);
    pLwRm->UnmapFbMemory ((void*)mapped, 0, DispTest::GetBoundGpuSubdevice());

    // Open the file
    if (OK != file.Open(filename, "wb"))
    {
        Printf (Tee::PriHigh, "SaveVGABackdoor: Can't Create File (%s)\n", filename.c_str());
        return rc;
    }

    // Write the fb data to the file
    if (size != fwrite (&buf[0], 1, size, file.GetFile()))
    {
        rc = RC::FILE_READ_ERROR;
        Printf (Tee::PriHigh, "SaveVGABackdoor: Can't Write File (%s)\n", filename.c_str());
    }

    return rc;
}

/****************************************************************************************
 * DispTest::BackdoorVgaWr08
 *
 *  Arguments Description:
 *  - offset:
 *  - data:
 *
 *  Functional Description
 *  - Write a byte through the backdoor to the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaWr08(UINT32 offset, UINT08 data)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaWr08 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+1)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(LwrrentDevice()->FbVGA_address+offset, &data, 1);
    Platform::FlushCpuWriteCombineBuffer();
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaRd08
 *
 *  Arguments Description:
 *  - offset:
 *  - pData:
 *
 *  Functional Description
 *  - Read a byte through the backdoor from the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaRd08(UINT32 offset, UINT08 *pData)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaRd08 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+1)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(pData, LwrrentDevice()->FbVGA_address+offset, 1);
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaWr16
 *
 *  Arguments Description:
 *  - offset:
 *  - data:
 *
 *  Functional Description
 *  - Write a word through the backdoor to the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaWr16(UINT32 offset, UINT16 data)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaWr16 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+2)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(LwrrentDevice()->FbVGA_address+offset, &data, 2);
    Platform::FlushCpuWriteCombineBuffer();
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaRd16
 *
 *  Arguments Description:
 *  - offset:
 *  - pData:
 *
 *  Functional Description
 *  - Read a word through the backdoor from the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaRd16(UINT32 offset, UINT16 *pData)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaRd16 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+2)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(pData, LwrrentDevice()->FbVGA_address+offset, 2);
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaWr32
 *
 *  Arguments Description:
 *  - offset:
 *  - data:
 *
 *  Functional Description
 *  - Write a dword through the backdoor to the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaWr32(UINT32 offset, UINT32 data)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaWr32 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+4)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(LwrrentDevice()->FbVGA_address+offset, &data, 4);
    Platform::FlushCpuWriteCombineBuffer();
    return OK;
}

/****************************************************************************************
 * DispTest::BackdoorVgaRd32
 *
 *  Arguments Description:
 *  - offset:
 *  - pData:
 *
 *  Functional Description
 *  - Read a dword through the backdoor from the VGA framebuffer
 ****************************************************************************************/
RC DispTest::BackdoorVgaRd32(UINT32 offset, UINT32 *pData)
{
    if( LwrrentDevice()->FbVGA_address == 0 ){
        Printf(Tee::PriHigh, "ERROR: BackdoorVgaRd32 found FbVGA_address==NULL; call BackdoorVgaInit() once first.\n");
        return 1;
    }
    MASSERT((offset+4)<=VGA_BACKDOOR_SIZE);

    Platform::MemCopy(pData, LwrrentDevice()->FbVGA_address+offset, 4);
    return OK;
}

/****************************************************************************************
 * DispTest::VgaCrlwpdate
 *
 *  Functional Description:
 *  - Get the VGA CRCs for both heads of interest
 *    CRC event.  Update the CRC event lists as appropriate.
 ***************************************************************************************/
RC DispTest::VgaCrlwpdate()
{
    // Only need to process for ISO displays
    if (!DispTest::GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
        return 0;

    RC rc = OK;
    UINT32 PrimCrcStatusAddress   = 0;
    UINT32 PrimCrcStatusBitMask   = 0;
    UINT32 PrimCrcStatusPollValue = 0;
    UINT32 PrimCrcAddress         = 0;

    UINT32 SecCrcStatusAddress   = 0;
    UINT32 SecCrcStatusBitMask   = 0;
    UINT32 SecCrcStatusPollValue = 0;
    UINT32 SecCrcAddress         = 0;

    // read the current time
    UINT32 audit = DispTest::GetBoundGpuSubdevice()->RegRd32(LW_PDISP_AUDIT);
    UINT32 time = REF_VAL(LW_PDISP_AUDIT_TIMESTAMP,audit);

    // scan head notifiers
    list<struct crc_head_info*>::iterator h_iter;
    for (h_iter = LwrrentDevice()->pCrcHeadList->begin(); h_iter != LwrrentDevice()->pCrcHeadList->end(); h_iter++)
    {
        // get info for head notifier
        struct crc_head_info *h_info = *h_iter;
        UINT32 prim_crc_entry    = 0;
        UINT32 sec_crc_entry     = 0;

        // Check if VGA Mode
        if (!h_info ->VgaMode)
        {
            Printf(Tee::PriHigh,"Attempt to use the VgaCrlwpdate routine when head %u is not in VGA mode\n", h_info->head);
            return RC::BAD_PARAMETER;
        }
        else
        {

            // Use the Primary OR_type
            if (h_info->is_active)
            {
                if(!strcmp(h_info->pOr_type->c_str(),"dac"))
                {
                    if (h_info->or_number > 2)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Dac%u\n", h_info->or_number);
                        return RC::BAD_PARAMETER;
                    }
                    PrimCrcStatusAddress    = LW_PDISP_DAC_CRCA(h_info->or_number);
                    PrimCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_DAC_CRCA_VALID);
                    PrimCrcStatusPollValue  = REF_DEF(LW_PDISP_DAC_CRCA_VALID,_TRUE);
                    PrimCrcAddress          = LW_PDISP_DAC_CRCB(h_info->or_number);
                } else if (!strcmp(h_info->pOr_type->c_str(),"sor"))
                {
                    if (h_info->or_number > 1)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Sor%u\n", h_info->or_number);
                        return RC::BAD_PARAMETER;
                    }
                    PrimCrcStatusAddress    = LW_PDISP_SOR_CRCA(h_info->or_number);
                    PrimCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_SOR_CRCA_VALID);
                    PrimCrcStatusPollValue  = REF_DEF(LW_PDISP_SOR_CRCA_VALID,_TRUE);
                    PrimCrcAddress          = LW_PDISP_SOR_CRCB(h_info->or_number);
                } else if (!strcmp(h_info->pOr_type->c_str(),"pior"))
                {
                    if (h_info->or_number > 2)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Pior%u\n", h_info->or_number);
                        return RC::BAD_PARAMETER;
                    }
                    PrimCrcStatusAddress    = LW_PDISP_PIOR_CRCA(h_info->or_number);
                    PrimCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_PIOR_CRCA_VALID);
                    PrimCrcStatusPollValue  = REF_DEF(LW_PDISP_PIOR_CRCA_VALID,_TRUE);
                    PrimCrcAddress          = LW_PDISP_PIOR_CRCB(h_info->or_number);
                } else
                {
                    Printf(Tee::PriHigh,"Attempt to Specify an invalid OR %s%u\n", h_info->pOr_type->c_str(),h_info->or_number);
                    return RC::BAD_PARAMETER;
                }
            }

            // Use the Secondary OR_type
            if (h_info->sec_is_active)
            {
                if(!strcmp(h_info->pSec_or_type->c_str(),"dac")){
                    if (h_info->sec_or_number > 2)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Dac%u\n", h_info->sec_or_number);
                        return RC::BAD_PARAMETER;
                    }
                    SecCrcStatusAddress    = LW_PDISP_DAC_CRCA(h_info->sec_or_number);
                    SecCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_DAC_CRCA_VALID);
                    SecCrcStatusPollValue  = REF_DEF(LW_PDISP_DAC_CRCA_VALID,_TRUE);
                    SecCrcAddress          = LW_PDISP_DAC_CRCB(h_info->sec_or_number);
                } else if (!strcmp(h_info->pSec_or_type->c_str(),"sor")){
                    if (h_info->sec_or_number > 1)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Sor%u\n", h_info->sec_or_number);
                        return RC::BAD_PARAMETER;
                    }
                    SecCrcStatusAddress    = LW_PDISP_SOR_CRCA(h_info->sec_or_number);
                    SecCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_SOR_CRCA_VALID);
                    SecCrcStatusPollValue  = REF_DEF(LW_PDISP_SOR_CRCA_VALID,_TRUE);
                    SecCrcAddress          = LW_PDISP_SOR_CRCB(h_info->sec_or_number);
                } else if (!strcmp(h_info->pSec_or_type->c_str(),"pior")) {
                    if (h_info->sec_or_number > 2)
                    {
                        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR Pior%u\n", h_info->sec_or_number);
                        return RC::BAD_PARAMETER;
                    }
                    SecCrcStatusAddress    = LW_PDISP_PIOR_CRCA(h_info->sec_or_number);
                    SecCrcStatusBitMask    = DRF_SHIFTMASK(LW_PDISP_PIOR_CRCA_VALID);
                    SecCrcStatusPollValue  = REF_DEF(LW_PDISP_PIOR_CRCA_VALID,_TRUE);
                    SecCrcAddress          = LW_PDISP_PIOR_CRCB(h_info->sec_or_number);
                } else {
                    Printf(Tee::PriHigh,"Attempt to Specify an invalid OR %s%u\n", h_info->pSec_or_type->c_str(),h_info->sec_or_number);
                    return RC::BAD_PARAMETER;
                }
            }

            // skip if inactive
            unique_ptr<struct crc_head_event> event(new struct crc_head_event);
            UINT32 event_number = (UINT32)(*LwrrentDevice()->pCrcHeadEventMap)[h_info]->size();
            if (h_info->is_active)
            {
                // Primary OR active, let's poll on the done bit
                rc = PollRegValue(
                        PrimCrcStatusAddress,
                        PrimCrcStatusPollValue,
                        PrimCrcStatusBitMask,
                        10);
                if (rc != OK)
                {
                    Printf(Tee::PriHigh,"Timeout Polling on Done bit in Primary OR CRC for head %u \n", h_info->head);
                    return rc;
                }
                // CRC ready to be read
                prim_crc_entry       = DispTest::GetBoundGpuSubdevice()->RegRd32(PrimCrcAddress);

                // record all events
                // See if we need to Poll for the secondary OR or not
                if (h_info->sec_is_active)
                {
                    rc = PollRegValue(
                            SecCrcStatusAddress,
                            SecCrcStatusPollValue,
                            SecCrcStatusBitMask,
                            10);
                    if (rc != OK)
                    {
                        Printf(Tee::PriHigh,"Timeout Polling on Done bit in secondary OR CRC for head %u \n", h_info->head);
                        return rc;
                    }

                    // CRC ready to be read
                    sec_crc_entry            = DispTest::GetBoundGpuSubdevice()->RegRd32(SecCrcAddress);
                }
                // create head notifier event
                event->primary_output_crc   = prim_crc_entry;
                event->secondary_output_crc = sec_crc_entry;

            } else if (h_info->sec_is_active)
            {
                rc = PollRegValue(
                        SecCrcStatusAddress,
                        SecCrcStatusPollValue,
                        SecCrcStatusBitMask,
                        10);
                if (rc != OK)
                {
                    Printf(Tee::PriHigh,"Timeout Polling on Done bit in secondary OR CRC for head %u \n", h_info->head);
                    return rc;
                }
                sec_crc_entry            = DispTest::GetBoundGpuSubdevice()->RegRd32(SecCrcAddress);
                // create head notifier event
                event->primary_output_crc   = prim_crc_entry;
                event->secondary_output_crc = sec_crc_entry;
            }
            // Finish contructing the event structure
            event->p_info               = h_info;
            event->tag                  = event_number;
            event->time                 = time;
            event->timestamp_mode       = false;
            event->hw_fliplock_mode     = false;
            event->present_interval_met = false;
            event->compositor_crc       = 0;
            // store event

            struct crc_head_event *pEvent = event.release();
            LwrrentDevice()->pCrcHeadEventList->push_back(pEvent);
            ((*LwrrentDevice()->pCrcHeadEventMap)[h_info])->push_back(pEvent);

        }
    }

    return rc;
}

/****************************************************************************************
 * DispTest::ReadCrtc
 *
 *  Arguments Description:
 *  - Index: Index of the Crtc Register to access
 *  - Head : Head to setup
 *  - Rtlwalue: pointer to the Rtlwalue
 *
 *  Functional Description:
 *  - Read the CRTC register at Index on Head
 ****************************************************************************************/
RC DispTest::ReadCrtc(UINT32 Index, UINT32 Head, UINT32 *Rtlwalue)
{
    if (LwrrentDevice()->DoSwapHeads) {
        if (Head == 1) Head = 0;
        else if (Head == 0) Head = 1;
    }
    if (LwrrentDevice()->UseHead) {
        Head = LwrrentDevice()->VgaHead;
    }

    *Rtlwalue = (UINT32)DispTest::GetBoundGpuSubdevice()->ReadCrtc((UINT08)Index,(INT32)Head);
    return OK;
}

/****************************************************************************************
 * DispTest::WriteCrtc
 *
 *  Arguments Description:
 *  - Index: Index of the Crtc Register to access
 *  - Data : Data to be written
 *  - Head : Head to setup
 *
 *  Functional Description:
 *  - Write the Crtc register at Index on Head with Data
 ****************************************************************************************/
RC DispTest::WriteCrtc(UINT32 Index, UINT32 Data, UINT32 Head)
{
    return DispTest::GetBoundGpuSubdevice()->WriteCrtc((UINT08)Index, (UINT08)Data, (INT32)Head);
}

/****************************************************************************************
 * DispTest::ReadSr
 *
 *  Arguments Description:
 *  - Index: Index of the Sr Register to access
 *  - Head : Head to setup
 *  - Rtlwalue: pointer to the Rtlwalue
 *
 *  Functional Description:
 *  - Read the SR register at Index on Head
 ****************************************************************************************/
RC DispTest::ReadSr(UINT32 Index, UINT32 Head, UINT32 *Rtlwalue)
{
    *Rtlwalue = (UINT32)DispTest::GetBoundGpuSubdevice()->ReadSr((UINT08)Index,(INT32)Head);
    return OK;
}

/****************************************************************************************
 * DispTest::WriteSr
 *
 *  Arguments Description:
 *  - Index: Index of the Sr Register to access
 *  - Data : Data to be written
 *  - Head : Head to setup
 *
 *  Functional Description:
 *  - Write the SR register at Index on Head with Data
 ****************************************************************************************/
RC DispTest::WriteSr(UINT32 Index, UINT32 Data, UINT32 Head)
{
    return DispTest::GetBoundGpuSubdevice()->WriteSr((UINT08)Index, (UINT08)Data, (INT32)Head);
}

/****************************************************************************************
 * DispTest::ReadAr
 *
 *  Arguments Description:
 *  - Index: Index of the Ar Register to access
 *  - Head : Head to setup
 *  - Rtlwalue: pointer to the Rtlwalue
 *
 *  Functional Description:
 *  - Read the AR register at Index on Head
 ****************************************************************************************/
RC DispTest::ReadAr(UINT32 Index, UINT32 Head, UINT32 *Rtlwalue)
{
    *Rtlwalue = (UINT32)DispTest::GetBoundGpuSubdevice()->ReadAr((UINT08)Index,(INT32)Head);
    return OK;
}

/****************************************************************************************
 * DispTest::WriteAr
 *
 *  Arguments Description:
 *  - Index: Index of the Ar Register to access
 *  - Data : Data to be written
 *  - Head : Head to setup
 *
 *  Functional Description:
 *  - Write the AR register at Index on Head with Data
 ****************************************************************************************/
RC DispTest::WriteAr(UINT32 Index, UINT32 Data, UINT32 Head)
{
    return DispTest::GetBoundGpuSubdevice()->WriteAr((UINT08)Index, (UINT08)Data, (INT32)Head);
}

/****************************************************************************************
 * DispTest::UpdateNow
 *
 *  Arguments Description:
 *  timeout_ms:
 *
 *  Functional Description:
 *  - This functions does a read-modify-write for sending an update
 ***************************************************************************************/
RC DispTest::UpdateNow(FLOAT64 timeout_ms)
{
    UINT08 temp = 0;
    RC rc;
    UINT08 color = 0;
    rc = Platform::PioRead08(LW_VIO_MISC__READ, &color); CHECK_RC(rc);
    color = DRF_VAL(_VIO, _MISC, _IO_ADR, color);
    UINT16 cr_base = (color == LW_VIO_MISC_IO_ADR_03BX) ? LW_CIO_CRX__MONO : LW_CIO_CRX__COLOR;

    // Poll for UpdateNow to be low
    Platform::PioWrite08(cr_base, LW_CIO_CRE_DISPCTL__INDEX);
    rc = DispTest::PollIORegValue(
            cr_base+1,
            DRF_DEF(_CIO, _CRE_DISPCTL, _UPDATE_NOW, _INIT),
            DRF_MASK(LW_CIO_CRE_DISPCTL_UPDATE_NOW)    << DRF_SHIFT(LW_CIO_CRE_DISPCTL_UPDATE_NOW),
            timeout_ms);
    if (rc != OK) {
        Printf(Tee::PriHigh, "Error [UpdateNow]: Time-out waiting for UpdateNow to go low\n");
    }
    CHECK_RC(rc);

    rc = Platform::PioRead08(cr_base+1, &temp); CHECK_RC(rc);
    // Write bit 0 of CR19 LW_CIO_CRE_DISPCTL__INDEX with 1.
    rc = Platform::PioWrite08(cr_base+1, temp | 0x1); CHECK_RC(rc);

    // Poll for Update Now to be low
    rc = DispTest::PollIORegValue(
            cr_base+1,
            DRF_DEF(_CIO, _CRE_DISPCTL, _UPDATE_NOW, _INIT),
            DRF_MASK(LW_CIO_CRE_DISPCTL_UPDATE_NOW)    << DRF_SHIFT(LW_CIO_CRE_DISPCTL_UPDATE_NOW),
            timeout_ms);
    if (rc != OK) {
        Printf(Tee::PriHigh, "Error [UpdateNow]: Time-out waiting for UpdateNow to go low\n");
    }
    CHECK_RC(rc);

    //New poll for update done added for bug 1196760
    //UPDATE_NOW clears before the update processing is actually complete
    Platform::PioWrite08(cr_base, LW_CIO_CRE_DSI_PSI__INDEX);
    rc = DispTest::PollIORegValue(
            cr_base+1,
            DRF_DEF(_CIO, _CRE_DSI_PSI, _NOTIFIER, _PENDING),
            DRF_MASK(LW_CIO_CRE_DSI_PSI_NOTIFIER)    << DRF_SHIFT(LW_CIO_CRE_DSI_PSI_NOTIFIER),
            timeout_ms);
    if (rc != OK) {
        Printf(Tee::PriHigh, "Error [UpdateNow]: Time-out waiting for Notifier to go high\n");
    }
    CHECK_RC(rc);

    //clear the notifier bit for next time
    rc = Platform::PioWrite08(cr_base+1, DRF_DEF(_CIO,_CRE_DSI_PSI,_NOTIFIER,_RESET)); CHECK_RC(rc);

    rc = Platform::PioRead08(cr_base+1, &temp); CHECK_RC(rc);
    if(temp & DRF_DEF(_CIO,_CRE_DSI_PSI,_NOTIFIER,_RESET))
    {
        Printf(Tee::PriHigh, "Error [UpdateNow]: NOTIFIER did not clear properly!\n");
    }

    return OK;
}

/****************************************************************************************
 * DispTest::CalcClockSetup_TwoStage
 *
 *  Arguments Description:
 *  - pClk
 *
 *  Functional Description
 *  - Helper function to program custom pclk for VGA tests
 *    It will return coefficients that can program within +- .5% of the specified out_clk
 *  - John Weidman gave me this code. He got it from someone in RM group.
 ****************************************************************************************/
RC DispTest::CalcClockSetup_TwoStage(CLKSETUPPARAMS *pClk)
{
    bool  find_best_fit = (pClk->SetupHints & CLK_SETUP_HINTS_FIND_BEST_FIT) != 0;
    UINT32  vclk = (UINT32)pClk->TargetFreq;

    // Fields that we usually get from the VBIOS
    UINT32  crystalFreq =   27000;

    UINT32  FmilwcoA    =  100000;
    UINT32  FmaxVcoA    =  405000;
    UINT32  FmilwcoB    =  400000;
    UINT32  FmaxVcoB    = 1000000;
    UINT32  FminUA      =    3000;
    UINT32  FmaxUA      =   25000;
    UINT32  FminUB      =   35000;
    UINT32  FmaxUB      =  100000;
    UINT32  lowMA       =       2;
    UINT32  highMA      =       9;
    UINT32  lowNA       =       4;
    UINT32  highNA      =     135;
    UINT32  lowMB       =       1;
    UINT32  highMB      =       7;
    UINT32  lowNB       =       4;
    UINT32  highNB      =      31;
    UINT32  maxP        =       7;
    UINT32  minP        =       0;

    // Reset "best" values.
    UINT32  bestDelta   = 0xFFFFFFFF;
    UINT32  bestMA      = 13;
    UINT32  bestNA      = 255;
    UINT32  bestMB      = 4;
    UINT32  bestNB      = 31;
    UINT32  bestClk     = 0;
    UINT32  bestP       = 6;

    UINT32  tgtVcoB;
    UINT32  FvcoA, FvcoB;
    UINT32  lowP, highP;
    UINT32  ma, mb;
    UINT32  nb, na;
    UINT32  p;
    UINT32  lwv;
    UINT32  delta;
    UINT32  tempVar;

    // If target frequency is higher than FmaxVcoB, set FmaxVcoB to target frequency + 0.5%.
    // This is required to handle overclocking.
    tgtVcoB = (vclk << minP) + (vclk << minP) / 200;
    if ( FmaxVcoB < tgtVcoB )
        FmaxVcoB = tgtVcoB;

    // In general, higher FvcoB is better.
    // Pick P so FvcoB is between FmaxVcoB/2 and FmaxVcoB.
    // Callwlate the lowP value first
    //
    FvcoB = FmaxVcoB - (FmaxVcoB / 200);  // FmaxVcoB less 0.5% margin.
    FvcoB /= 2;                           // Divide by 2.  This is the min we want to  use for FvcoB;
    lowP = minP;
    FvcoB >>= lowP;
    while (vclk <= FvcoB )                // Increment P until FvcoB is between FmaxVcoB/2 and FmaxVcoB.
    {
        FvcoB /= 2;
        if ( (++lowP) == maxP )
            break;
    }

    // Callwlate the highP.  Callwlating both lowP and highP covers the corner
    // case where the target frequency is within .5% of the P boundary (bug 79337).
    // Using only one P value could clip the best choice.  In most cases, lowP
    // and highP are the same value.
    //
    FvcoB = FmaxVcoB + (FmaxVcoB / 200);  // FmaxVcoB add 0.5% margin.
    FvcoB /= 2;                           // Divide by 2.  This is the min we want to  use for FvcoB;
    highP = minP;
    FvcoB >>= highP;
    while ( vclk <= FvcoB )               // Increment P until FvcoB is between FmaxVcoB/2 and FmaxVcoB.
    {
        FvcoB /= 2;
        if ( (++highP) == maxP )
            break;
    }

    // This routine loops thru P, MA, MB, and NA looking for the first match
    // (less than 0.5% error).  NB is callwlated.
    //
    for (p = lowP; p <= highP; p++)
    {
        tgtVcoB = vclk << p;

        for (ma = lowMA; ma <= highMA; ma++)
        {
            // make sure FUA respects the constraints
            tempVar = crystalFreq/ma;   // fUA
            if (tempVar < FminUA)
                break;
            if (tempVar > FmaxUA)
                continue;

            for (na = lowNA; na <= highNA; na++)
            {
                FvcoA = crystalFreq * na / ma;
                if ((FvcoA >= FmilwcoA) && (FvcoA <= FmaxVcoA))
                {
                    for (mb = lowMB; mb <= highMB; mb++)
                    {
                        // make sure FUB respects the constraints
                        tempVar = FvcoA/mb;   // fUB
                        if (tempVar < FminUB)
                            break;
                        if (tempVar > FmaxUB)
                            continue;

                        // Callwlate nb.
                        //
                        // Solve for nb, and round instead of floor:
                        //   tgtVcoA = FvcoA * nb / mb;
                        //
                        nb = ((tgtVcoB * mb) + (FvcoA / 2)) / FvcoA;

                        if (nb < lowNB)
                            continue;

                        if (nb > highNB)
                            break;

                        FvcoB = FvcoA * nb / mb;

                        if ( (FvcoB >= FmilwcoB) && (FvcoB <= FmaxVcoB) &&
                             (nb >= 4*mb) && (nb <= 10*mb))
                        {
                            lwv = FvcoB >> p;

                            if (lwv < vclk)
                                delta = vclk - lwv;
                            else
                                delta = lwv - vclk;

                            if (delta < bestDelta)
                            {
                                bestDelta = delta;
                                bestMA    = ma;
                                bestNA    = na;
                                bestMB    = mb;
                                bestNB    = nb;
                                bestP     = p;
                                bestClk   = lwv;
                            }

                            // If frequency is close enough (within 0.5%), stop looking.  Use
                            // integers instead of floats.  We use 218 (.45%) instead of 200
                            // for a little extra margin on the early out.
                            if ( (delta == 0) ||
                                 ( (!find_best_fit) && ((vclk/delta) > 218)) )
                            {
                                // This match is within 0.5%.  It is close enough.  Look no further.
                                // Set ma, na, nb, p to high values to force exit from loops.
                                ma = highMA;
                                na = highNA;
                                mb = highMB;
                                p  = highP;
                            }
                        }
                    }
                }
            }
        }
    }

    //
    // Return the results
    //
    pClk->Ma = bestMA;
    pClk->Na = bestNA;
    pClk->Mb = bestMB;
    pClk->Nb = bestNB;
    pClk->P  = bestP;
    pClk->ActualFreq = bestClk; // (kHz units)

    return OK;
}

//
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
