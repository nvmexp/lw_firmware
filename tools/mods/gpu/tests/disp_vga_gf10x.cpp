/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
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
//      Branched from "disp_vga.cpp" - broken out 10 September 2008
//
//      Written by: Matt Craighead, Larry Coffey, et al
//      Date:       29 July 2004
//
//      Routines in this module:
//
//      VGASetClocks_gf10x                    Programs VPLL coefficients based on desired clock frequency for VGA
//      IsoProgramVPLL_gf10x                  Programs VPLL coefficients based on desired clock frequency
//      IsoCalcVPLLCoeff_gf10x                Callwlate the and set VPLL coefficients

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
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>
#include <list>
#include <map>
#include <errno.h>

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#include "fermi/gf100/dev_ext_devices.h"
#include "fermi/gf100/dev_disp.h"
#include "fermi/gf100/dev_trim.h"

#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

/****************************************************************************************
 * DispTest::VGASetClocks_gf10x
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
 *   This version of VGASetClocks is for the 40nm VPLLs in fermi.
 ***************************************************************************************/
RC DispTest::VGASetClocks_gf10x(UINT32 head,
                          UINT32 set_pclk,
                          UINT32 pclk_freq,
                          UINT32 dac_index,
                          UINT32 sim_turbo,
                          UINT32 emu_run,
                          FLOAT64 timeout_ms)
{
    RC rc = OK;
    UINT08 data_byte = 0;
    UINT32 pdiv_value = 0;
    UINT32 frequency = 0;
    FLOAT32 frequency_f = 0;
    UINT32 crystal_freq = 0;
    UINT32 vpll_hi, vpll_lo;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    if (!pSubdev->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
        return 0;

    crystal_freq = pSubdev->RegRd32(LW_PEXTDEV_BOOT_0);
    crystal_freq = (DRF_VAL(_PEXTDEV,_BOOT_0,_STRAP_CRYSTAL, crystal_freq) == LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL_27000K ) ? 27000 : 25000;
    Printf(Tee::PriNormal, "DispTest::VGASetClocks_gf10x: Crystal Frequency Value = %d \n",crystal_freq);

    // perform EscapeWrites for sim_turbo in RTL mode
    if ( (sim_turbo!=0) && (Platform::GetSimulationMode() == Platform::RTL) ) {
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Setting turbo mode for disp clock model : pclk = %d kHz\n", pclk_freq);
        // Do the an escape write to the disp clk model (lneft 03/07/06)
        Platform::EscapeWrite("sim_turbo", 0, 4, 1);
        Platform::EscapeWrite("turbo_pclk_freq", 0, 4, pclk_freq);
    }

    //Enable VPLL
    UINT32 vpll_config;
    CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG(head), &vpll_config));
    Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: LW_PDISP_CLK_REM_VPLL_CFG(%d) == 0x%x\n", head, vpll_config);
    vpll_config = FLD_SET_DRF(_PDISP, _CLK_REM_VPLL_CFG, _ENABLE, _YES,
                              vpll_config);
    Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: LW_PDISP_CLK_REM_VPLL_CFG(%d) --> 0x%x\n", head, vpll_config);
    CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_CFG(head), vpll_config));

    // select CLOCK_MODE for LW_VIO_MISC and LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK
    // CLOCK_MODE is CLK_LWSTOM if set_pclk is non-zero in RTL/emu sims, or sim_turbo is non-zero in RTL sims
    // otherwise, CLOCK_MODE is CLK_25
    if ( ((set_pclk!=0)  && (Platform::GetSimulationMode() == Platform::Hardware) && (emu_run!=0)) ||
         ((set_pclk!=0)  && (Platform::GetSimulationMode() == Platform::RTL)) ||
         ((sim_turbo!=0) && (Platform::GetSimulationMode() == Platform::RTL)) ) {
        // custom clock
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Setting custom clock mode for Head %d\n", head);
        rc = Platform::PioRead08(LW_VIO_MISC__READ, &data_byte); CHECK_RC(rc);
        data_byte = FLD_SET_DRF(_VIO, _MISC, _CLK_SEL, _LWSTOM, data_byte);
        rc = Platform::PioWrite08(LW_VIO_MISC__WRITE, data_byte); CHECK_RC(rc);
        rc = DispTest::RmaRegWr32(
            LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK(head),
            DRF_NUM(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_FREQUENCY, pclk_freq) |
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_MODE,_CLK_LWSTOM) |
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_ADJ1000DIV1001,_FALSE) |
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_NOT_DRIVER,_TRUE)); CHECK_RC(rc);
    } else {
        // default 25 MHz clock
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Setting default 25 MHz clock mode for Head %d\n", head);
        rc = Platform::PioRead08(LW_VIO_MISC__READ, &data_byte); CHECK_RC(rc);
        data_byte = FLD_SET_DRF(_VIO, _MISC, _CLK_SEL, _25_180MHZ, data_byte);
        rc = Platform::PioWrite08(LW_VIO_MISC__WRITE, data_byte); CHECK_RC(rc);
        rc = DispTest::RmaRegWr32(
            LW_PDISP_VGA_HEAD_SET_PIXEL_CLOCK(head),
            DRF_NUM(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_FREQUENCY, pclk_freq) |  // dont care
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_MODE,_CLK_25) |
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_ADJ1000DIV1001,_FALSE) |
            DRF_DEF(_PDISP,_VGA_HEAD_SET_PIXEL_CLOCK,_NOT_DRIVER,_TRUE)); CHECK_RC(rc);
    }

    Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: set div-by ratios to 1 for Head %d and Dac %d\n", head, dac_index);
    // Set the divide by ratios to 1 because the output of the VPLL-PDIV for us is already 25MHz.
    // We don't want to slow things down any further.
    CHECK_RC(DispTest::RmaRegWr32(
            LW_PDISP_CLK_REM_DAC(dac_index),
            DRF_DEF (_PDISP, _CLK_REM_DAC, _ZONE1_DIV, _BY_1 ) |
            DRF_DEF (_PDISP, _CLK_REM_DAC, _ZONE2_DIV, _BY_1 ) |
            DRF_DEF (_PDISP, _CLK_REM_DAC, _ZONE3_DIV, _BY_1 ) |
            DRF_DEF (_PDISP, _CLK_REM_DAC, _ZONE4_DIV, _BY_1 )
            ));
    CHECK_RC(DispTest::RmaRegWr32(
            LW_PDISP_CLK_REM_RG (head),
            DRF_DEF (_PDISP, _CLK_REM_RG, _DIV, _BY_1 )
            ));

    // Configure the VPLL registers LW_PDISP_CLK_REM_VPLL_25_DIV and LW_PDISP_CLK_REM_VPLL_28_DIV
    // Note: In emulation, the VPLLs are not implemented.  Instead, the output of the VPLL
    // is typically (assumed to be) conifgured to 1/2 of dispclk frequency.  Therefore, the only
    // thing that still needs programming are the pdiv value of the VPLL.
    if ( (emu_run!=0) && (Platform::GetSimulationMode() == Platform::Hardware) ) {
        // RTL sims typically has a dispclk period at 2.48 ns (approx 403 MHz).  To get to
        // a 25 MHz clock frequency, the closest pdiv would be divide BY_16.  Since VPLL_HI is
        // already 1/2 of dispclk frequency, we just need to configure pdiv to divide BY_8.
        // However, to speed up emulation time, we will override it to divide BY_1 (approx 200 MHz).
        // If an RG underflow oclwrs, the test can always specify -pclk_period to select a slower
        // custom pclk.
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Adjusting 25/28 MHz VPLL pdiv values for emulation.\n");
        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_DIV,
            DRF_DEF(_PDISP,_CLK_REM_VPLL_25_DIV,_NDIV,_INIT) |
            DRF_DEF(_PDISP,_CLK_REM_VPLL_25_DIV,_MDIV,_INIT) |
            DRF_NUM(_PDISP,_CLK_REM_VPLL_25_DIV,_PLDIV,1)));
        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_DIV,
            DRF_DEF(_PDISP,_CLK_REM_VPLL_28_DIV,_NDIV,_INIT) |
            DRF_DEF(_PDISP,_CLK_REM_VPLL_28_DIV,_MDIV,_INIT) |
            DRF_NUM(_PDISP,_CLK_REM_VPLL_28_DIV,_PLDIV,1)));

        // If set_pclk is non-zero, that means that -pclk_period was most likely required to
        // prevent RG underflows.  Therefore, configure the VPLL pdiv to generate a relative
        // pclk frequency that is less than the specified pclk frequency.
        if (set_pclk!=0) {
            pdiv_value = 0;
            frequency = 200000;  // frequency = approx 1/2 of dispclk freq
            while (frequency > pclk_freq) {
              pdiv_value += 1;
              frequency /= 2;
              // max value of pdiv is divide by 2^6
              if ( pdiv_value == 6 ) break;
            }

            // program VPLL pdiv
            Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Adjusting Head %d custom VPLL pdiv value for emulation", head);
            Printf(Tee::PriNormal, " (desired frequency %d kHz, configured frequency %d kHz)\n", pclk_freq, frequency);
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_COEFF(head),
                DRF_DEF(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV,_INIT) |
                DRF_DEF(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV,_INIT) |
                DRF_NUM(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV,pdiv_value)));
            //why don't the 25 and 28 registers get set here too?
        }

    } else if ( ((set_pclk!=0)  && (Platform::GetSimulationMode() == Platform::RTL)) ||
                ((sim_turbo!=0) && (Platform::GetSimulationMode() == Platform::RTL)) ) {

        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Calling DispTest::IsoCalcVPLLCoeff_gf10x to configure Head %d VPLL\n", head);
        rc = DispTest::IsoCalcVPLLCoeff_gf10x(head, pclk_freq, LW_VIO_MISC_CLK_SEL_LWSTOM);
        CHECK_RC(rc);

        UINT32 vpll_config;
        UINT32 vpll_ldiv;
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_COEFF(head), &vpll_lo));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_SSD0(head), &vpll_hi));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_ldiv));
        if(DRF_VAL(_PDISP,_CLK_REM_VPLL_CFG2,_SSD_EN_SDM,vpll_config) == LW_PDISP_CLK_REM_VPLL_CFG2_SSD_EN_SDM_YES)
        {
            Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: SDM enabled\n");
            FLOAT32 numerator = FLOAT32(DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV, vpll_lo) + 0.5
                                + 4 * DRF_VAL(_PDISP,_CLK_REM_VPLL_SSD0,_SDM_DIN,vpll_hi) * (2^-15));

            frequency_f = (crystal_freq * numerator
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV, vpll_lo)
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV, vpll_lo) );
        }
        else
        {
            Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: SDM disabled\n");
            frequency_f = FLOAT32(crystal_freq * DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV, vpll_lo)
                                      / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV, vpll_lo)
                                      / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV, vpll_lo));
        }
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: (desired frquency %d kHz, configured frequency (assuming %d MHz refclk) %f kHz)\n", pclk_freq, (crystal_freq/1000), frequency_f);
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: LW_PDISP_CLK_REM_VPLL_COEFF(%d) = 0x%08x\n", head, vpll_lo);
        Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: LW_PDISP_CLK_REM_VPLL_SSD0(%d) = 0x%08x\n", head, vpll_hi);

        // There are cases where we want to speed up RTL sim using -sim_turbo.  With the Display testbench
        // clock model, an escape write configures the VPLL for the desired speed and the clock mode is
        // ignored (ie, the selection between 25 MHz, 28 MHz, and custom coefficients are ignored).  But
        // with real RTL clocks or fullchip testbench, the escape write is not suported.  In this case,
        // only the custom VPLL coefficients would have been configured and the 25 MHz and 28 MHz
        // coefficients would be left in their INIT values.  If an rVGA test programs LW_VIO_MISC for legacy
        // VGA operation, only the 25 MHz and 28 MHz clock mode are known to the test.  Therefore, the fast
        // clock would not be selected and the simulation could not take advantage of the faster sim time.
        // To take advantage of the faster sim time, we will overwrite the INIT values of the 25 MHz and
        // 28 MHz VPLL coefficients if -sim_turbo was detected.
        if (sim_turbo!=0) {
            Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Detected -sim_turbo in RTL sims.  Copying Head %d VPLL coefficients into 25 MHz and 28 MHz VPLL coefficients\n", head);
            Printf(Tee::PriNormal, "[VGASetClocks_gf10x]: Overwriting VGA clock settings due to sim_turbo.\n");
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_DIV, vpll_lo));
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_SDM, vpll_hi));
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_DIV, vpll_lo));
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_SDM, vpll_hi));
        }
    }

    return OK;
}

/****************************************************************************************
 * DispTest::IsoProgramVPLL_gf10x
 *
 *  Arguments Description:
 *  - head: head number
 *  - pclk_freq: pixel clock frequency in kHz
 *  - mode: clock mode (ie, which VPLL to program, 25MHz/28MHz/custom)
 *  - emu_run: program clocks for emulation runs
 *
 *  Functional Description:
 *  - callwlates VPLL coefficients and programs VPLL
 *   This version of IsoProgramVPLL is for the 40nm VPLLs.
 ***************************************************************************************/
RC DispTest::IsoProgramVPLL_gf10x(UINT32 head,
                            UINT32 pclk_freq,
                            UINT32 mode,
                            UINT32 emu_run)
{
    RC rc = OK;
    UINT32 pdiv_value = 0;
    UINT32 frequency = 0;
    UINT32 crystal_freq = 0;
    UINT32 vpll_hi, vpll_lo;
    UINT32 vpll_config;
    FLOAT32 frequency_f;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    // Only need to process for ISO displays
    if (!pSubdev->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
        return 0;

    crystal_freq = pSubdev->RegRd32(LW_PEXTDEV_BOOT_0);
    crystal_freq = (DRF_VAL(_PEXTDEV,_BOOT_0,_STRAP_CRYSTAL, crystal_freq) == LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL_27000K ) ? 27000 : 25000;
    Printf(Tee::PriNormal, "DispTest::IsoProgramVPLL_gf10x: Crystal Frequency Value = %d \n",crystal_freq);

    // Note: In emulation, the VPLLs are not implemented.
    if ( (emu_run!=0) && (Platform::GetSimulationMode() == Platform::Hardware) ) {

        // callwlate pdiv that gets close to, but not over, the desired frequency
        pdiv_value = 0;
        frequency = 200000;  // frequency = approx 1/2 of dispclk freq
        while (frequency > pclk_freq) {
          pdiv_value += 1;
          frequency /= 2;
          // max value of pdiv is divide by 2^6
          if ( pdiv_value == 6 ) break;
        }

        // program 25 MHz VPLL pdiv
        if (mode == LW_VIO_MISC_CLK_SEL_25_180MHZ) {
            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: Adjusting 25 MHz VPLL pdiv value for emulation");
            Printf(Tee::PriNormal, " (desired frequency %d kHz, configured frequency %d kHz)\n", pclk_freq, frequency);
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_DIV,
                DRF_DEF(_PDISP,_CLK_REM_VPLL_25_DIV,_NDIV,_INIT) |
                DRF_DEF(_PDISP,_CLK_REM_VPLL_25_DIV,_MDIV,_INIT) |
                DRF_NUM(_PDISP,_CLK_REM_VPLL_25_DIV,_PLDIV,pdiv_value)));

        // program 28 MHz VPLL pdiv
        } else if (mode == LW_VIO_MISC_CLK_SEL_28_325MHZ) {
            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: Adjusting 28 MHz VPLL pdiv value for emulation");
            Printf(Tee::PriNormal, " (desired frequency %d kHz, configured frequency %d kHz)\n", pclk_freq, frequency);
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_DIV,
                DRF_DEF(_PDISP,_CLK_REM_VPLL_28_DIV,_NDIV,_INIT) |
                DRF_DEF(_PDISP,_CLK_REM_VPLL_28_DIV,_MDIV,_INIT) |
                DRF_NUM(_PDISP,_CLK_REM_VPLL_28_DIV,_PLDIV,pdiv_value)));

        // program custom VPLL pdiv
        } else if (mode == LW_VIO_MISC_CLK_SEL_LWSTOM) {
            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: Adjusting Head %d custom VPLL pdiv value for emulation", head);
            Printf(Tee::PriNormal, " (desired frequency %d kHz, configured frequency %d kHz)\n", pclk_freq, frequency);
            CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_COEFF(head),
                DRF_DEF(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV,_INIT) |
                DRF_DEF(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV,_INIT) |
                DRF_NUM(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV,pdiv_value)));

        // unknown mode
        } else {
            Printf(Tee::PriHigh, "ERROR [IsoProgramVPLL_gf10x]: Cannot configure VPLL pdiv for emulation due to unknown mode = %d\n", mode);
            return ~OK;
        }

    } else {

        if ((mode == LW_VIO_MISC_CLK_SEL_LWSTOM) || (mode == LW_VIO_MISC_CLK_SEL_25_180MHZ) || (mode == LW_VIO_MISC_CLK_SEL_28_325MHZ))  {

            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: Calling DispTest::IsoCalcVPLLCoeff_gf10x to configure Head %d VPLL\n", head);
            rc = DispTest::IsoCalcVPLLCoeff_gf10x(head, pclk_freq, mode);
            CHECK_RC(rc);

            //Doublecheck the frequency programmed
            //TODO
            /*
            if(mode == LW_VIO_MISC_CLK_SEL_25_180MHZ)
            {
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_25_DIV, &vpll_lo));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_25_SDM, &vpll_hi));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
            }
            else if(mode == LW_VIO_MISC_CLK_SEL_28_325MHZ)
            {
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_28_DIV, &vpll_lo));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_28_SDM, &vpll_hi));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
            }
            else
            {
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_COEFF(head), &vpll_lo));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_SSD0(head), &vpll_hi));
                CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
            }
            */

            CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_COEFF(head), &vpll_lo));
            CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_SSD0(head), &vpll_hi));
            CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
            if(DRF_VAL(_PDISP,_CLK_REM_VPLL_CFG2,_SSD_EN_SDM,vpll_config) == LW_PDISP_CLK_REM_VPLL_CFG2_SSD_EN_SDM_YES)
            {
                Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: SDM enabled\n");
                FLOAT32 numerator = FLOAT32(DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV, vpll_lo) + 0.5
                    + 4 * DRF_VAL(_PDISP,_CLK_REM_VPLL_SSD0,_SDM_DIN,vpll_hi) * (2^-15));

                frequency_f = (crystal_freq * numerator
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV, vpll_lo)
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV, vpll_lo));
            }
            else
            {
                Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: SDM disabled\n");
                frequency_f = FLOAT32(crystal_freq * DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV, vpll_lo)
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV, vpll_lo)
                        / DRF_VAL(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV, vpll_lo));
            }

            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: (desired frquency %d kHz, configured frequency (assuming %d MHz refclk) %f kHz)\n", pclk_freq, (crystal_freq/1000),
                    frequency_f);
            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: LW_PDISP_CLK_REM_VPLL_COEFF(%d) = 0x%08x\n", head, vpll_lo);
            Printf(Tee::PriNormal, "[IsoProgramVPLL_gf10x]: LW_PDISP_CLK_REM_VPLL_SSD0(%d) = 0x%08x\n", head, vpll_hi);

        } else {

            Printf(Tee::PriHigh, "ERROR [IsoProgramVPLL_gf10x]: Cannot configure VPLL due to unknown mode = %d\n", mode);
            return ~OK;
        }

    }

    return OK;
}

/****************************************************************************************
 * DispTest::IsoCalcVPLLCoeff_gf10x
 *
 *  Arguments Description:
 *  - head: head number
 *  - pclk_freq: pixel clock frequency in kHz
 *  - mode: VGA clock mode (CUSTOM, 25, or 28)
 *
 *  Functional Description:
 *  - callwlates VPLL coefficients
 *   This version is for the 40nm VPLLs.
 ***************************************************************************************/
RC DispTest::IsoCalcVPLLCoeff_gf10x(UINT32 head, UINT32 pclk_freq, UINT32 mode)
{
    RC rc = OK;
    UINT32 crystal_freq = 0;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();

    // Only need to process for ISO displays
    if (!pSubdev->HasFeature(Device::GPUSUB_HAS_ISO_DISPLAY))
        return 0;

    crystal_freq = pSubdev->RegRd32(LW_PEXTDEV_BOOT_0);
    crystal_freq = (DRF_VAL(_PEXTDEV,_BOOT_0,_STRAP_CRYSTAL, crystal_freq) == LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL_27000K ) ? 27000 : 25000;
    Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: Crystal Frequency Value = %d \n",crystal_freq);

    UINT32  ref_clk;
    //UINT32  vclk,  // same as pclk_freq
    UINT32 tgtVco;
    UINT32  bestDelta, bestM, bestN, bestPL, bestSDM;
    UINT32  lwv, delta, m, n, n2, sdm, pl;
    UINT32  Fvco;
    UINT32  fUA;

    // Variables for PLL restriction values from VBIOS (except for LDIV)
    UINT32  lowM, highM;
    UINT32  lowN, highN;
    UINT32  lowPL, highPL;
    UINT32  minPL, maxPL;
    UINT32  FmaxVco, Fmilwco;
    UINT32  FminU, FmaxU;

    UINT32 find_best_fit = 1; //?? what should this be
    UINT32 bSDM          = 1;

    //
    // Here we set the target frequency as the clk target, instead of pll.
    // That way the M/N/"P" algorithm will solve M/N/LDIV for us. M/N are inside
    // the PLL while LDIV is outside
    //

    //clk_in = thisClk->hal.clkGetSrcFreqKHz(pGpu, thisClk, pPllSetup->pllSrc);

    // Hard Limits - come from the VBIOS
    Fmilwco = 500000; //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_milwcoA);
    FmaxVco = 1000000;//thisClk->clkGetAttr(thisClk, whichPll, pllAttr_maxVcoA);
    FminU   = 25000;  //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_minUA);
    FmaxU   = 50000;  //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_maxUA);
    lowM    = 1;      //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_minMA);
    highM   = 255;    //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_maxMA);
    lowN    = 8;      //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_minNA);
    highN   = 255;    //thisClk->clkGetAttr(thisClk, whichPll, pllAttr_maxNA);
    minPL   = 1;
    maxPL   = 63;
    //srcDivMin = LW_PTRIM_SYS_CORE_REFCLK_GPCPLL_VCODIV_BY1;
    //srcDivMax = LW_PTRIM_SYS_CORE_REFCLK_GPCPLL_VCODIV_BY31;

    Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: Fmilwco=%08d FmaxVco=%08d FminU=%08d FmaxU=%08d\n", Fmilwco, FmaxVco, FminU, FmaxU);
    Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: lowM   =%08d highM  =%08d lowN   =%08d highN  =%08d minPL =%08d maxPL = %08d\n",lowM, highM, lowN, highN, minPL, maxPL);

    // Reset "best" values.
    bestDelta   = 0xFFFFFFFF;
    bestM       = highM;
    bestN       = lowN;
    bestPL      = minPL;
    bestSDM     = 0;

    // If target frequency is higher than FmaxVco[1], set FmaxVco[1] to target frequency + 2%.
    // This is required to handle overclocking.
    tgtVco = pclk_freq + pclk_freq / 50;
    ref_clk = crystal_freq;
    if ( FmaxVco < tgtVco )
        FmaxVco = tgtVco;

    // High linear divider is the largest value where we can still hit the target frequency.
    highPL = (FmaxVco + tgtVco - 1) / tgtVco;
    //highPL = LW_MIN (highPL, maxPL);
    //highPL = LW_MAX (highPL, minPL);
    if (highPL > maxPL) {
      highPL = maxPL;
    }
    if (highPL < minPL) {
      highPL = minPL;
    }

    lowPL = Fmilwco / tgtVco;
    //lowPL = LW_MIN (lowPL, maxPL);
    //lowPL = LW_MAX (lowPL, minPL);
    if (lowPL > maxPL) {
      lowPL = maxPL;
    }
    if (lowPL < minPL) {
      lowPL = minPL;
    }

    for (pl = highPL; pl >= lowPL;  pl--)
    {

      // Get the actual required target VCO freq(not the clock freq!)
      tgtVco = pclk_freq * pl;

      for (m = lowM; m <= highM; m++)
      {
    // make sure FUA respects the constraints
    fUA = ref_clk / m;

    if (fUA < FminU)
      break;

    if (fUA > FmaxU)
      continue;

    //
    // Callwlate n.
    //
    if (bSDM)
    {
      // Using SDM.  Solve for n rounded down with SDM=0.
      //   tgtVco = ref_clk * (n + .5 + SDM/8192) / (m * fracDivide);
      //
      n =  ((tgtVco * m) - (ref_clk / 2)) / ref_clk;
      n2 = n;
    }
    else
    {
      // Not using SDM.  Solve for n, first rounded down and then rounded up.
      //   tgtVco = ref_clk * n / (m * fracDivide);
      //
      n =  (tgtVco * m) / ref_clk;
      n2 = ((tgtVco * m) + (ref_clk - 1)) / ref_clk;
    }

    if (n > highN)
      break;

    for (; n <= n2; n++)
    {
      if (n < lowN)
        continue;

      if (n > highN)
        break;

      if (bSDM)
      {
        // Sigma-Delta Modulator
        sdm = (UINT32)((((tgtVco * m) * (LwU64)8192) + (ref_clk / 2)) / ref_clk) - (n * 8192) - 4096;
        Fvco = (UINT32)((ref_clk * (LwU64)(n * 8192 + 4096 + sdm)) / (m * 8192));
      }
      else
      {
        sdm = 0;
        Fvco = ref_clk * n / m;
      }

      if ( (Fvco >= Fmilwco) && (Fvco <= FmaxVco) )
      {
        lwv = (Fvco + (pl / 2)) / pl;
        //delta = LW_MAX( lwv, pclk_freq ) - LW_MIN( lwv, pclk_freq );
        if (lwv < pclk_freq)
          delta = pclk_freq - lwv;
        else
          delta = lwv - pclk_freq;

        if (delta < bestDelta)
        {
          bestDelta = delta;
          bestM     = m;
          bestN     = n;
          bestPL    = pl;
          bestSDM   = sdm;
        }

        //
        // If frequency is close enough (within 0.5%), stop looking.  Use
        // integers instead of floats.  We use 218 (.45%) instead of 200
        // for a little extra margin on the early out.
        //
        if ( (delta == 0) ||
        ( (!find_best_fit) && ((pclk_freq/delta) > 218)) )
        {
          // This match is within 0.5%.  It is close enough.
          // Look no further.
          // Set m/n/p/pl to values that force exit from loops.
          m = highM;
          n = highN;
          pl = lowPL;
        }
      }

      //Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: ldiv   =%08d tgtVco =%08d m = %d fUA =%08d bSDM = %d n = %d n2 = %d sdm = %08d  Fvco = %08d lwv = %08d delta = %08d \n", ldiv, tgtVco, m, fUA, bSDM, n, n2, sdm, Fvco, lwv, delta);
      //Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: bestDelta = %08d bestM = %08d bestN = %08d bestLDIV = %08d bestClk = %08d bestSDM = %08d\n", bestDelta, bestM, bestN, bestLDIV, bestClk, bestSDM);
    }
      }
        }

    if (bestDelta == 0xFFFFFFFF) {
      Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x: Error : bestDelta is still 0xFFFFFFFF\n");
      return ~OK;
    }
    else {
      Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x : Callwlated coefficients for target frequency %d ref_clk (%d)- M(%d) N(%d) SDM(%d) PL(%d)\n", pclk_freq, ref_clk, bestM, bestN, bestSDM, bestPL);
      if(mode == LW_VIO_MISC_CLK_SEL_25_180MHZ)
      {
        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_DIV,
          DRF_NUM(_PDISP,_CLK_REM_VPLL_25_DIV,_MDIV,bestM) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_25_DIV,_PLDIV,bestPL) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_25_DIV,_NDIV,bestN) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_CFG2(head),
          DRF_DEF(_PDISP,_CLK_REM_VPLL_CFG2,_SSD_EN_SDM,_YES) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_25_SDM,
          DRF_NUM(_PDISP,_CLK_REM_VPLL_25_SDM,_DIN,bestSDM) ));

        UINT32 vpll_lo, vpll_hi, vpll_config;
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_25_DIV, &vpll_lo));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_25_SDM, &vpll_hi));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));

        Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x : LW_PDISP_CLK_REM_VPLL_25_DIV = 0x%x LW_PDISP_CLK_REM_VPLL_25_SDM = 0x%x LW_PDISP_CLK_REM_VPLL_CFG2 = 0x%x \n", vpll_lo, vpll_hi, vpll_config);
      }
      else if(mode == LW_VIO_MISC_CLK_SEL_28_325MHZ)
      {
        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_DIV,
          DRF_NUM(_PDISP,_CLK_REM_VPLL_28_DIV,_MDIV,bestM) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_28_DIV,_PLDIV,bestPL) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_28_DIV,_NDIV,bestN) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_CFG2(head),
          DRF_DEF(_PDISP,_CLK_REM_VPLL_CFG2,_SSD_EN_SDM,_YES) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_28_SDM,
          DRF_NUM(_PDISP,_CLK_REM_VPLL_28_SDM,_DIN,bestSDM) ));

        UINT32 vpll_lo, vpll_hi, vpll_config;
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_28_DIV, &vpll_lo));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_28_SDM, &vpll_hi));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));

        Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x : LW_PDISP_CLK_REM_VPLL_28_DIV = 0x%x LW_PDISP_CLK_REM_VPLL_28_SDM = 0x%x LW_PDISP_CLK_REM_VPLL_CFG2 = 0x%x \n", vpll_lo, vpll_hi, vpll_config);
      }
      else if(mode == LW_VIO_MISC_CLK_SEL_LWSTOM)
      {
        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_COEFF(head),
          DRF_NUM(_PDISP,_CLK_REM_VPLL_COEFF,_MDIV,bestM) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_COEFF,_PLDIV,bestPL) |
          DRF_NUM(_PDISP,_CLK_REM_VPLL_COEFF,_NDIV,bestN) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_CFG2(head),
          DRF_DEF(_PDISP,_CLK_REM_VPLL_CFG2,_SSD_EN_SDM,_YES) ));

        CHECK_RC(DispTest::RmaRegWr32(LW_PDISP_CLK_REM_VPLL_SSD0(head),
          DRF_NUM(_PDISP,_CLK_REM_VPLL_SSD0,_SDM_DIN,bestSDM) ));

        //if (head==0) {
        //  CHECK_RC(DispTest::RmaRegWr32(LW_PVTRIM_SYS_DISP_VCLK0_CMOS(0),
        //        DRF_NUM(_PVTRIM,_SYS_DISP_VCLK0_CMOS,_VCODIV,bestLDIV)));
        //}
        //else {
        //  CHECK_RC(DispTest::RmaRegWr32(LW_PVTRIM_SYS_DISP_VCLK1_CMOS(0),
        //        DRF_NUM(_PVTRIM,_SYS_DISP_VCLK0_CMOS,_VCODIV,bestLDIV)));
        //}

        UINT32 vpll_lo, vpll_hi, vpll_config;
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_COEFF(head), &vpll_lo));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_SSD0(head), &vpll_hi));
        CHECK_RC(DispTest::RmaRegRd32(LW_PDISP_CLK_REM_VPLL_CFG2(head), &vpll_config));
        //if (head==0) {
        //CHECK_RC(DispTest::RmaRegRd32(LW_PVTRIM_SYS_DISP_VCLK0_CMOS(head), &vclk_cmos));
        //} else {
        //CHECK_RC(DispTest::RmaRegRd32(LW_PVTRIM_SYS_DISP_VCLK1_CMOS(head), &vclk_cmos));
        //}

        Printf(Tee::PriNormal, "DispTest::IsoCalcVPLLCoeff_gf10x : LW_PDISP_CLK_REM_VPLL_COEFF = 0x%x LW_PDISP_CLK_REM_VPLL_SSD0 = 0x%x LW_PDISP_CLK_REM_VPLL_CFG2 = 0x%x \n", vpll_lo, vpll_hi, vpll_config);
      }
      return OK;
    }
}
