/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/t210preinit.h"
#include "cheetah/include/sysutil.h"
#include "cheetah/dev/clk/clkdev.h"
#include "cheetah/include/devid.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "t21x/t210/arxusb_padctl.h"
#include "t21x/t210/arxusb_host.h"
#include "t21x/t210/arclk_rst.h"
#include "t21x/t214/arfuse.h"
#include "cheetah/dev/fuse/tests/skutest.h"
#include "cheetah/include/iommutil.h"
#include "cheetah/dev/pmc/pwrg/pwrgdev.h"
#include <stdio.h>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "T210XhciPreInit"

#define XUSB_PADCTL_PLL_TIMEOUT                     800

// xhci clock registers
#define APBDEV_PMC_PWRGATE_STATUS_0_XUSBC           22
#define APBDEV_PMC_PWRGATE_STATUS_0_XUSBB           21
#define APBDEV_PMC_PWRGATE_STATUS_0_XUSBA           20

#define T214_UPHY_PAD_REG_ADDR_GAP 0x40

#define PADCTL_REG_UPDATE(reg,rf,n)                      \
{                                                        \
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, reg, &val, 4); \
    val = FLD_SET_RF_NUM(reg##_##rf##_RANGE, n, val);    \
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, reg, val, 4);  \
}

namespace
{
    UINT32 IsAdbSafe= 0;
    UINT32 IsForceHS = 0;
    vector<UINT32> vXhciPllCfgData = { 2,0x0, 3,0x7051, 37,0x130, 30,0x17 };
    vector<UINT32> vXhciLaneCfgData = { 1,0x2, 4,0x32, 7,0x22, 53,0x2587, 73,0xFC7,
        82,0x1, 83,0x3C0F, 86,0xC00F, 93,0xFF07, 94,0x141A, 151,0x80 };

    RC PllCtl4Wr(vector<UINT32> * pvCfgAddrData)
    {
        RC rc;
        UINT32 val;

        for (UINT32 i = 0; i < pvCfgAddrData->size(); i += 2)
        {
            val = 0;
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_10_0_PLL0_CFG_ADDR_RANGE, pvCfgAddrData->at(i), val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_10_0_PLL0_CFG_WDATA_RANGE, pvCfgAddrData->at(i+1), val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_10_0_PLL0_CFG_RESET__RANGE, 1, val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_10_0_PLL0_CFG_WS_RANGE, 1, val);
            CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_10_0, val, 4);
        }
        return OK;
    }

    RC LaneCtl2Wr(UINT32 PadIndex, vector<UINT32> * pvCfgAddrData)
    {
        RC rc;
        UINT32 val;

        for (UINT32 i = 0; i < pvCfgAddrData->size(); i += 2)
        {
            val = 0;
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_8_0_CFG_ADDR_RANGE, pvCfgAddrData->at(i), val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_8_0_CFG_WDATA_RANGE, pvCfgAddrData->at(i+1), val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_8_0_CFG_WS_RANGE, 1, val);
            val = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_8_0_CFG_RESET__RANGE, 1, val);
            CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_8_0 + T214_UPHY_PAD_REG_ADDR_GAP * PadIndex, val, 4);
        }
        return OK;
    }

    RC WaitBit(UINT32 Reg,
               UINT32 Bit,
               UINT32 UnexpectedVal,
               string Prompt)
    {
        RC rc;
        UINT32 val;
        UINT32 elapseUs = 0;
        UINT32 timeoutUs = 1000000;     // 1 second
        do{
            Platform::SleepUS(100);
            elapseUs+=100;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, Reg, &val, 4);
        }while(FLD_TEST_RF_NUM(Bit:Bit, UnexpectedVal, val) && (elapseUs<=timeoutUs));
        if (elapseUs > timeoutUs)
        {
            Printf(Tee::PriError, "UPHY PLL %s timeout \n",Prompt.c_str());
            return RC::TIMEOUT_ERROR;
        }
        return OK;
    }

    RC T214UphySetup()
    {
        RC rc;
        UINT32 val;

        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, PLL0_PWR_OVRD, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, PLL0_CAL_OVRD, 1);
        //todo:  RCAL_OVRD not exist in  XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0 but in CTL_8
        //PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, RCAL_OVRD, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_OVRD, 1);

        CHECK_RC(PllCtl4Wr(&vXhciPllCfgData));

        //Skip Pll calibration if it's done upon boot, Bug 1778620
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &val, 4);
        if(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_LOCKDET_STATUS_RANGE, 0, val))
        {
            Printf(Tee::PriLow, "Start PLL0 Calibration \n");
            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, PLL0_IDDQ, 0);
            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, PLL0_SLEEP, 0);

            Tasker::Sleep(1);
            //Start PLL Calibration and wait it done
            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, PLL0_CAL_EN, 1);
            CHECK_RC(WaitBit(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, 0?XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_DONE_RANGE, 0, "CAL_DONE set"));

            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, PLL0_CAL_EN, 0);
            CHECK_RC(WaitBit(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, 0?XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_DONE_RANGE, 1, "CAL_DONE clr"));

            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, PLL0_ENABLE, 1);
            CHECK_RC(WaitBit(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, 0?XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_LOCKDET_STATUS_RANGE, 0, "LOCKDET_STATUS set"));

            Printf(Tee::PriLow, "Start PLL RCAL \n");
            // Perform resistor calibration
            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_EN, 1);
            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_CLK_EN, 1);
            CHECK_RC(WaitBit(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, 0?XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 0, "RCAL_DONE set"));

            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_EN, 0);
            CHECK_RC(WaitBit(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, 0?XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 1, "RCAL_DONE clr"));

            PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_CLK_EN, 0);
        }

        //CHECK_RC(pUphy->PllSwOverrideRemove(0));
        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, PLL0_PWR_OVRD, 0);
        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, PLL0_CAL_OVRD, 0);
        PADCTL_REG_UPDATE(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, PLL0_RCAL_OVRD, 0);
        Printf(Tee::PriLow, "SwOverride removed\n");

        // PROD Settings programming - UPHY, see also http://lwbugs/200177072
        LaneCtl2Wr(1, &vXhciLaneCfgData);
        LaneCtl2Wr(4, &vXhciLaneCfgData);
        LaneCtl2Wr(5, &vXhciLaneCfgData);

        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK5, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE1, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE4, 1);
        PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE5, 1);
        if(IsForceHS)
        {
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1, 0);
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4, 0);
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, FORCE_PCIE_PAD_IDDQ_DISABLE_MASK5, 0);
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE1, 3);
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE4, 3);
            PADCTL_REG_UPDATE(XUSB_PADCTL_USB3_PAD_MUX_0, PCIE_PAD_LANE5, 3);
        }
        // assign the UPHY lanes to XUSB, PCIE, SATA, or UFS according to the platform specific configuration
        //todo: use T210 routine for following?
        /*
        CHECK_RC(pUphy->LaneOwnership(0x7, 0));
        CHECK_RC(pUphy->RemoveClamp(0x7));
        if(isForceHS)
        {
            CHECK_RC(pUphy->LaneForceIddq(0x7, 0));
        }
        */
        return OK;
    }
}

RC T210PreInit::XhciCfgPmu(const vector<string> &ConfigParams)
{
    RC rc = OK;
    CHECK_RC(SysUtil::Board::ApplySettings("USB3_PWR"));
    return rc;
}

RC T210PreInit::XhciRstrPmu(const vector<string> &ConfigParams)
{
    return OK;
}

RC T210PreInit::XhciCfgCtrl(const vector<string> &ConfigParams)
{
    LOG_ENT();
    RC rc;
    UINT32 regVal;

    UINT32 elapseMs = 0;
    UINT32 isPadSata = 0;
    UINT32 isDeviceMode = 0;
    UINT32 isNativePlle = 0;
    UINT32 isSkipClk = 0;
    UINT32 isForceFS = 0;
    IsForceHS = 0;//Global var,set default state for loops
    IsAdbSafe= 0;
    UINT32 forceFsPortIndex = 0;
    //string debugExit;

    CHECK_RC(GetOptionalParam(ConfigParams, "PadSata", isPadSata, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "DevMode", isDeviceMode, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "Plle", isNativePlle, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "SkipClk", isSkipClk, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "ForceHS", IsForceHS, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "ForceFS", isForceFS, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "AdbSafe", IsAdbSafe, 0));
    Printf(Tee::PriNormal, 
           "XHCI PreInit(): %s %s %s %s %s %s\n", 
           isPadSata ? "Sata Pad" : "",
           isDeviceMode ? "Device Mode" : "",
           isNativePlle ? "Native PLLe" : "", 
           isSkipClk ? "WAR - Skip certain CLKs for T210" : "",
           IsForceHS ? "ForceHS" : "",
           IsAdbSafe ? "Adb Safe Mode" : "");
    if(isForceFS)
    {
        CHECK_RC(GetParam(ConfigParams, "ForceFsPort", forceFsPortIndex));
        Printf(Tee::PriNormal, "XHCI PreInit(): ForceFS - 0x%x \n", forceFsPortIndex);
    }

    PwrgMgr::PwrgDev()->Init();
    PwrgMgr::PwrgDev()->On("XUSBA");
    PwrgMgr::PwrgDev()->On("XUSBB");
    PwrgMgr::PwrgDev()->On("XUSBC");
    PwrgMgr::PwrgDev()->Uninit();

    if (!isSkipClk) {
        if (!isNativePlle)
        {   // un-lock the PLLe
            ClkMgr::ClkDev()->Enable("PLLE", 0);
        }

        // un-lock the PCIE PLL
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_ENABLE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_SLEEP_RANGE, 0x3, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_IDDQ_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        // un-lock the SATA PLL
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_ENABLE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_SLEEP_RANGE, 0x3, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_IDDQ_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);
        Tasker::Sleep(10);

        // put XUSB into reset
        if(!IsAdbSafe)
        {
            CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_DEV", 1));
        }
    }
    CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_HOST", 1));
    CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_SS", 1));

    if (!isSkipClk) {
        // following is a hack to enable the PLLP_MISC and HSIC, we should manage to merge it into MODS clk driver
        CHECK_RC(CheetAh::RegRd(LW_DEVID_CAR, 0, 0x680, &regVal, 4));
        regVal = FLD_SET_RF_NUM(29:28, 0x3, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_CAR, 0, 0x680, regVal, 4));
        // clk hack
        CHECK_RC(CheetAh::RegRd(LW_DEVID_CAR, 0, 0x610, &regVal, 4));
        regVal = FLD_SET_RF_NUM(25:25, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(26:26, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_CAR, 0, 0x610, regVal, 4));
    }
    // Unpowergate XUSB
    // see also http://handheld-build.lwpu.com/manuals/t210/arapbpm.html
    CHECK_RC(CheetAh::RegRd(LW_DEVID_PMIF , 0, 0x38, &regVal, 4));
    if ( 0 == (regVal & (1<<APBDEV_PMC_PWRGATE_STATUS_0_XUSBB)) )
    {
        CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x30, 0x100|APBDEV_PMC_PWRGATE_STATUS_0_XUSBB, 4));
    }
    if ( 0 == (regVal & (1<<APBDEV_PMC_PWRGATE_STATUS_0_XUSBC)) )
    {
        CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x30, 0x100|APBDEV_PMC_PWRGATE_STATUS_0_XUSBC, 4));
    }
    if ( 0 == (regVal & (1<<APBDEV_PMC_PWRGATE_STATUS_0_XUSBA)) )
    {
        CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x30, 0x100|APBDEV_PMC_PWRGATE_STATUS_0_XUSBA, 4));
    }
    Tasker::Sleep(10);
    CHECK_RC(CheetAh::RegRd(LW_DEVID_PMIF , 0, 0x38, &regVal, 4));
    Printf(Tee::PriDebug, "PM STATUS = 0x%08x \n", regVal);
    //if (debugExit == "Debug1"){Printf(Tee::PriNormal, "  Exit at %s \n", debugExit.c_str()); return OK;}

    if (!isSkipClk) {
        //Set PLL_REF_E to 672MHZ
        CHECK_RC(ClkMgr::ClkDev()->SetPllRate("PLLRE_OUT", 672*1000*1000));
        //enable PLL_REF_E
        CHECK_RC(ClkMgr::ClkDev()->Enable("PLLRE_OUT", 1));
    }
    if (!isNativePlle)
    {
        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_PTS_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_ENABLE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_IDDQ_SWCTL_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_IDDQ_OVERRIDE_VALUE_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_IDDQ_OVERRIDE_VALUE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);

        Tasker::Sleep(5);
        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC1_0_PLLE_SDM_RESET_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_AUX_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_AUX_0_PLLE_REF_SEL_PLLREFE_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_AUX_0_PLLE_CML0_OEN_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_AUX_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC1_0_PLLE_EN_DITHER_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC1_0_PLLE_EN_SSC_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0_PLLE_SSCBYP_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0_PLLE_BYPASS_SS_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC1_0_PLLE_EN_SDM_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC1_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_SS_CNTL1_0_PLLE_SDM_DIN_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_SS_CNTL1_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_MDIV_RANGE, 0x2, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_NDIV_RANGE, 0x7d, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_PLDIV_CML_RANGE, 0xe, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_ENABLE_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_LOCK_OVERRIDE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_CLKENABLE_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);

        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_ENABLE_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, regVal, 4);
        // PLLE done, todo: merge it into clk driver
        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLLE_LOCK set timeout \n"); }
    }
    else
    {
        //Set PLLE source to PLL_REF_E
        CHECK_RC(ClkMgr::ClkDev()->RegRd(0x48c, &regVal));
        regVal &= ~(1 << 28);
        regVal &= ~(1 << 2);
        CHECK_RC(ClkMgr::ClkDev()->RegWr(0x48c, regVal));

        CHECK_RC(ClkMgr::ClkDev()->SetPllRate("PLLE", 100*1000*1000));
        CHECK_RC(ClkMgr::ClkDev()->Enable("PLLE", 1));
    }

    // PLLU will be put into HW control by kernel, so we don't touch it
    //CHECK_RC(ClkMgr::ClkDev()->Enable("PLLU", 1));
    CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_FS_SRC", 1));
    CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_SS", 1));
    CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_HOST", 1));

    if (!isSkipClk) {
        CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_DEV_SRC", 1));

        CHECK_RC(ClkMgr::ClkDev()->SetSrc("XUSB_FS_SRC", "PLLU_48M"));
        CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_FS_SRC", 48*1000*1000, 1));

        CHECK_RC(ClkMgr::ClkDev()->SetSrc("XUSB_SS_SRC", "PLLU_480M"));        // 480M
        CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_SS", 120*1000*1000, 1));

        UINT64 pllReOut;
        CHECK_RC(ClkMgr::ClkDev()->GetRate("PLL_P_OUT_XUSB", &pllReOut));
        UINT64 xusbHostFreq = 102*1000*1000;
        CHECK_RC(ClkMgr::ClkDev()->SetDevDivisor("XUSB_HOST_SRC", pllReOut/xusbHostFreq));
        //XUSB_HOST_SRC possible src clk: clk_m pll_p_out_xusb pll_re_out
        CHECK_RC(ClkMgr::ClkDev()->SetSrc("XUSB_HOST_SRC", "PLL_P_OUT_XUSB"));      //408M
        CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_HOST", xusbHostFreq, 1));

        UINT64 xusbDevFreq = 102*1000*1000;
        CHECK_RC(ClkMgr::ClkDev()->SetDevDivisor("XUSB_DEV_SRC", pllReOut/xusbDevFreq));
        //XUSB_DEV_SRC possible src clk: clk_m pll_p_out_xusb pll_re_out
        CHECK_RC(ClkMgr::ClkDev()->SetSrc("XUSB_DEV_SRC", "PLL_P_OUT_XUSB"));
        CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_DEV", xusbDevFreq, 1));

        CHECK_RC(ClkMgr::ClkDev()->SetSrc("XUSB_FALCON_SRC", "PLLRE_OUT"));
        // 336M
        CHECK_RC(ClkMgr::ClkDev()->SetDevRate("XUSB_FALCON_SRC", 336*1000*1000, true));
        CHECK_RC(ClkMgr::ClkDev()->Enable("XUSB_FALCON_SRC", 1, 1));

        CHECK_RC(ClkMgr::ClkDev()->Reset("USBD"));
        CHECK_RC(ClkMgr::ClkDev()->Reset("USB2"));

        CHECK_RC(ClkMgr::ClkDev()->ToggleReset("PEX_USB_UPHY", 0));
        CHECK_RC(ClkMgr::ClkDev()->ToggleReset("SATA_USB_UPHY", 0));
        // Set SWR_USB_PADCTL_RST to 0, should merge it into MODS clk driver
        CHECK_RC(CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_RST_DEVICES_W_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_RST_DEVICES_W_0_SWR_XUSB_PADCTL_RST_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_RST_DEVICES_W_0, regVal, 4));
        //Enable the mamagement clk, should merge it into MODS clk driver.
        UINT32 regVal;
        CHECK_RC(CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_SOURCE_PEX_SATA_USB_RX_BYP_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_CLK_SOURCE_PEX_SATA_USB_RX_BYP_0_PEX_USB_PAD_RX_BYP_REFCLK_DIVISOR_RANGE, 0x6, regVal);
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_CLK_SOURCE_PEX_SATA_USB_RX_BYP_0_PEX_USB_PAD_RX_BYP_REFCLK_CE_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_SOURCE_PEX_SATA_USB_RX_BYP_0, regVal, 4));
    }

    if (CheetAh::IsChipT214())
    {
        CHECK_RC(T214UphySetup());
    }
    else
    {
        // see also https://wiki.lwpu.com/engwiki/index.php/User:Gagarwal/T210_USB_BCT
        // and see also //dev/stdif/xusb/doc/chip/t210/T210_USB_Boot_LP0_ELPG_ELCG_Programming_Guide.doc
        // 1.1 Cold Boot, with no Recovery Mode or Boot from USB
        // uphy related sequence
        CHECK_RC(CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_RST_DEVICES_Y_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_RST_DEVICES_Y_0_SWR_PEX_USB_UPHY_RST_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_RST_DEVICES_Y_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_CTRL_RANGE, 0x136, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_5_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_5_0_PLL0_DCO_CTRL_RANGE, 0x2a, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_5_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_PWR_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_IDDQ_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_SLEEP_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4));

        Tasker::Sleep(1);
        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_EN_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_DONE_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLL0_CAL_DONE set timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_EN_RANGE, 0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_2_0_PLL0_CAL_DONE_RANGE, 1, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLL0_CAL_DONE clear timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_EN_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_CLK_EN_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLL0_RCAL_DONE set timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_EN_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 1, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLL0_RCAL_DONE clear timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0_PLL0_RCAL_CLK_EN_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_8_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_ENABLE_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_LOCKDET_STATUS_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: PLL0_LOCKDET_STATUS set timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB3_PAD_MUX_0, &regVal, 4));
        //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_RANGE, 0x0, regVal);
        if (!isPadSata)
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK5_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK6_RANGE, 0x1, regVal);

            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE0_RANGE, 0x1, regVal);  // default SS port 3, HS port
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE1_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE2_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE3_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE4_RANGE, 0x1, regVal);  // default SS port 4, HS port
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE5_RANGE, 0x1, regVal);  // always port 2, HS port 8
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE6_RANGE, 0x1, regVal);  // always port 1, HS port 5
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_SATA_PAD_LANE0_RANGE, 0x2, regVal);
        }
        else
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK5_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK6_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_SATA_PAD_IDDQ_DISABLE_MASK0_RANGE, 0x1, regVal);

            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE0_RANGE, 0x0, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE1_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE2_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE3_RANGE, 0x1, regVal);  // Port 3
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE4_RANGE, 0x3, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE5_RANGE, 0x1, regVal);  // always port 2, HS port 8
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_PCIE_PAD_LANE6_RANGE, 0x1, regVal);  // always port 1, HS port 5
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB3_PAD_MUX_0_SATA_PAD_LANE0_RANGE, 0x1, regVal);  // port 4
        }
        if (IsForceHS)
        {   // this is a hack requested by bug http://lwbugs/1623737
            Printf(Tee::PriLow, "XhciPreInit: Iddq hack. XUSB_PADCTL_USB3_PAD_MUX_0 = 0x%08x ", regVal);
            if(!isPadSata)
            {
                regVal &= ~(0x71<<(0?XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0_RANGE));
            }
            else
            {
                regVal &= ~(0xe8<<(0?XUSB_PADCTL_USB3_PAD_MUX_0_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0_RANGE));
            }
            Printf(Tee::PriLow, "-> 0x%08x \n", regVal);
        }
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB3_PAD_MUX_0, regVal, 4));
        // uphy done
    }

    if (isPadSata)
    {   // setup Sata Pad PLL
        // below two registers come from ASIC, to fix bug http://lwbugs/1612564
        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0_RX_TERM_OVRD_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0, regVal, 4));
        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0_AUX_RX_MODE_OVRD_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_1_0, regVal, 4));
        Printf(Tee::PriLow, "XhciPreInit: XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_4_0 updated \n");    // for visual check purpose

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_FREQ_MDIV_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_FREQ_NDIV_RANGE, 0x19, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0_PLL0_TXCLKREF_SEL_RANGE, 0x2, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0_PLL0_TXCLKREF_EN_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_4_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_CTRL_RANGE, 0x136, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0_PLL0_DCO_CTRL_RANGE, 0x2a, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_5_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_PWR_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_OVRD_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_IDDQ_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_SLEEP_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4));

        Tasker::Sleep(1);
        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_EN_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_DONE_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: S0 PLL0_CAL_DONE set timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_EN_RANGE, 0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_2_0_PLL0_CAL_DONE_RANGE, 1, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: S0 PLL0_CAL_DONE clear timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_EN_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_CLK_EN_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: S0 PLL0_RCAL_DONE set timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_EN_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_DONE_RANGE, 1, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: S0 PLL0_RCAL_DONE clear timeout \n"); }

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0_PLL0_RCAL_CLK_EN_RANGE, 0x0, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_8_0, regVal, 4));

        CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_ENABLE_RANGE, 0x1, regVal);
        CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4));

        elapseMs = 0;
        do{
            Tasker::Sleep(1); elapseMs++;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        }while(FLD_TEST_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_LOCKDET_STATUS_RANGE, 0, regVal) && (elapseMs<=XUSB_PADCTL_PLL_TIMEOUT));
        if (elapseMs > XUSB_PADCTL_PLL_TIMEOUT){ Printf(Tee::PriError, "XhciPreInit: S0 PLL0_LOCKDET_STATUS set timeout \n"); }
    }

    // seems nothing to change
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_TDCD_DBNC_TIMER_0 , regVal, 4));
    // UPHY settings
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x2, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_1_0_TX_DRV_AMP_RANGE , 0x27, regVal);
    }
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_2_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_2_0_RX_CTLE_RANGE , 0xfc, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_2_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_3_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_3_0_RX_DFE_RANGE , 0xC0077F1F, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_3_0, regVal, 4));
    //CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_4_0, &regVal, 4));
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_4_0_RX_CDR_CTRL_RANGE, 0x1C7, regVal);
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_4_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_6_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_6_0_RX_EQ_CTRL_H_RANGE, 0xfcf01368, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD0_ECTL_6_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x2, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_1_0_TX_DRV_AMP_RANGE , 0x27, regVal);
    }
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_2_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_2_0_RX_CTLE_RANGE , 0xfc, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_2_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_3_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_3_0_RX_DFE_RANGE , 0xC0077F1F, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_3_0, regVal, 4));
    //CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_4_0, &regVal, 4));
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_4_0_RX_CDR_CTRL_RANGE, 0x1C7, regVal);
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_4_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_6_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_6_0_RX_EQ_CTRL_H_RANGE, 0xfcf01368, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD1_ECTL_6_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x2, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_1_0_TX_DRV_AMP_RANGE , 0x27, regVal);
    }
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_2_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_2_0_RX_CTLE_RANGE , 0xfc, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_2_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_3_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_3_0_RX_DFE_RANGE , 0xC0077F1F, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_3_0, regVal, 4));
    //CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_4_0, &regVal, 4));
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_4_0_RX_CDR_CTRL_RANGE, 0x1C7, regVal);
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_4_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_6_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_6_0_RX_EQ_CTRL_H_RANGE, 0xfcf01368, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD2_ECTL_6_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_1_0_TX_TERM_CTRL_RANGE , 0x2, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_2_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_2_0_RX_CTLE_RANGE , 0xfc, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_2_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_3_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_3_0_RX_DFE_RANGE , 0xC0077F1F, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_3_0, regVal, 4));
    //CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_4_0, &regVal, 4));
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_4_0_RX_CDR_CTRL_RANGE, 0x1C7, regVal);
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_4_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_6_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_6_0_RX_EQ_CTRL_H_RANGE, 0xfcf01368, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_USB3_PAD3_ECTL_6_0, regVal, 4));

     // assign the USB2.0 ports to XUSB or USB2 (Only OTG and HSIC PAD Port 0 could be assigned to USB2 controller)
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PAD_MUX_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_HSIC_PORT1_CONFIG_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_HSIC_PORT0_CONFIG_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_BIAS_PAD_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_HSIC_PAD_TRK_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_HSIC_PAD_PORT1_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_HSIC_PAD_PORT0_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT3_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT2_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE, 1, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PAD_MUX_0, regVal, 4));

    // port capabilities
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PORT_CAP_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_INTERNAL_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_INTERNAL_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_INTERNAL_RANGE, 0, regVal);
    if (isDeviceMode)
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 2, regVal);
    }
    else
    {
        if(!IsAdbSafe)
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 1, regVal);
        }
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_INTERNAL_RANGE, 0, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PORT_CAP_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_SNPS_OC_MAP_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SNPS_OC_MAP_0_CONTROLLER1_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_SNPS_OC_MAP_0, regVal, 4));

    //disable OC since MODS doesn't handle it
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OC_MAP_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE, 0xf, regVal);   // or 4 if use J8110
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OC_MAP_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_SS_PORT_MAP_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_MAP_0_PORT0_MAP_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_MAP_0_PORT1_MAP_RANGE, 3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_MAP_0_PORT2_MAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_MAP_0_PORT3_MAP_RANGE, 2, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_SS_PORT_MAP_0, regVal, 4));
    if ( !isPadSata )
    {
        CHECK_RC(SysUtil::Board::ApplySettings("XUSB_PORTMAP"));
    }
    else
    {
        CHECK_RC(SysUtil::Board::ApplySettings("XUSB_PORTMAP_SATA"));
    }
    // seems nothing to do with it
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_VBUS_OC_MAP_0, regVal, 4));

    // ELPG settings
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_ELPG_PROGRAM_0_0, 0, 4));
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_ELPG_PROGRAM_1_0, 0, 4));
    // Program PAD done
    CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x34, 1 << APBDEV_PMC_PWRGATE_STATUS_0_XUSBA, 4));
    CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x34, 1 << APBDEV_PMC_PWRGATE_STATUS_0_XUSBB, 4));
    CHECK_RC(CheetAh::RegWr(LW_DEVID_PMIF , 0, 0x34, 1 << APBDEV_PMC_PWRGATE_STATUS_0_XUSBC, 4));

    // see also mc_flush_done of powergate.c in kernel
    // and http://handheld-build.lwpu.com/build_t35lsf/docs/t35/armc.html
    //CHECK_RC(CheetAh::RegRd(LW_DEVID_MC,0,0x200,&regVal,4));
    //regVal &= ~((1<<19)|(1<<20));
    //CHECK_RC(CheetAh::RegWr(LW_DEVID_MC,0,0x200,regVal,4));
    UINT32 fuseUsbCalibExit;
    UINT32 fuseUsbCalib;
    if (CheetAh::IsChipT214())
    {
        SkuTest *pSkuTest = new SkuTest();
        CHECK_RC(pSkuTest->Init());
        CHECK_RC(pSkuTest->ReadFuseValue(FUSE_USB_CALIB_EXT_0, &fuseUsbCalibExit));
        CHECK_RC(pSkuTest->ReadFuseValue(FUSE_USB_CALIB_0, &fuseUsbCalib));
    }

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0xc, regVal); // should we read from fuse?
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_LWRR_LEVEL_RANGE, (fuseUsbCalib>>0)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal, 4);
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0xc, regVal); // should we read from fuse?
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_LWRR_LEVEL_RANGE, (fuseUsbCalib>>11)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal, 4);
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0xc, regVal); // should we read from fuse?
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_LWRR_LEVEL_RANGE, (fuseUsbCalib>>17)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal, 4);
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0xc, regVal); // should we read from fuse?
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_HS_LWRR_LEVEL_RANGE, (fuseUsbCalib>>23)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, regVal, 4);

    // ==== clear powerdown of both OTD pads and bias pads
    // might need to read from fuse

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , &regVal,4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE , 0x8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE, 4, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE , (fuseUsbCalib>>7)&0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE,(fuseUsbCalibExit>>0)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , regVal,4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0 , &regVal,4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE , 0x8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE, 4, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE ,(fuseUsbCalibExit>>5)&0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE,(fuseUsbCalibExit>>0)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal,4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0 , &regVal,4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE , 0x8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE, 4, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE ,(fuseUsbCalibExit>>9)&0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE,(fuseUsbCalibExit>>0)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal,4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0 , &regVal,4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_TERM_RANGE_ADJ_RANGE , 0x8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_RPD_CTRL_RANGE, 4, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_TERM_RANGE_ADJ_RANGE ,(fuseUsbCalibExit>>13)&0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_RPD_CTRL_RANGE,(fuseUsbCalibExit>>0)&0x1f, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0, regVal,4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_PD_RANGE, 0, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_SQUELCH_LEVEL_RANGE, (fuseUsbCalib>>29)&0x7, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_TERM_OFFSET_RANGE, 3, regVal);
    }
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_TX_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_TX_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_RX_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_RX_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_ZI_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_ZI_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_STROBE_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_DATA0_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_DATA0_RANGE, 0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , regVal, 4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD0_CTL_1_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_TX_RTUNEP_RANGE, 8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_TX_SLEW_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_HSIC_OPT_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_RTERM_RANGE, 0x10, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD0_CTL_1_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0_STRB_TRIM_VAL_RANGE, 0x1c, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0  , regVal, 4);

    // CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, 0xFF00FF, 4));

    // by default set to PCIE PAD
    // keep the UPHY in IDDQ and under sleep state before changing the LANE assignments
    for (UINT32 i=0; i<=6; i++)
    {
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0+0x40*i, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_IDDQ_OVRD_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_IDDQ_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_SLEEP_RANGE, 0x3, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_PWR_OVRD_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_IDDQ_OVRD_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_IDDQ_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_SLEEP_RANGE, 0x3, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_PWR_OVRD_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0+0x40*i, regVal, 4);
    }
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_IDDQ_OVRD_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_IDDQ_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_SLEEP_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_PWR_OVRD_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_IDDQ_OVRD_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_IDDQ_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_SLEEP_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_PWR_OVRD_RANGE, 0x1, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0 , regVal, 4);

    if (!isPadSata)
    {
        CHECK_RC(SysUtil::Board::ApplySettings("XUSB_PADMUX"));
    }
    else
    {
        CHECK_RC(SysUtil::Board::ApplySettings("XUSB_PADMUX_SATA"));
    }
    //bring the UPHY out of IDDQ
    for (UINT32 i=0; i<=6; i++)
    {
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0+0x40*i, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_IDDQ_OVRD_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_IDDQ_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_SLEEP_RANGE, 0x3, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_TX_PWR_OVRD_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_IDDQ_OVRD_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_IDDQ_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_SLEEP_RANGE, 0x3, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0_RX_PWR_OVRD_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_P0_CTL_2_0+0x40*i, regVal, 4);
    }
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_IDDQ_OVRD_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_IDDQ_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_SLEEP_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_TX_PWR_OVRD_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_IDDQ_OVRD_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_IDDQ_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_SLEEP_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0_RX_PWR_OVRD_RANGE, 0x0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_MISC_PAD_S0_CTL_2_0 , regVal, 4);

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_VBUS_ID_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_IDDIG_CHNG_INTR_EN_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_SOURCE_SELECT_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_ID_SOURCE_SELECT_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_IDDIG_ST_CHNG_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_VALID_ST_CHNG_RANGE, 0x1, regVal);
    if (isDeviceMode)
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_ID_OVERRIDE_RANGE, 0x8, regVal);
        // regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0x1, regVal); // this better be done after ctrl init
    }
    else
    {
        if(!IsAdbSafe)
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_ID_OVERRIDE_RANGE, 0x0, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0x0, regVal);
        }
    }
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_VBUS_ID_0, regVal, 4));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, &regVal, 4));
    if (isDeviceMode)
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_FIX18_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE, 0x1, regVal);
        if (CheetAh::IsChipT214())
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
        }
    }
    else
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_FIX18_RANGE, 0x1, regVal);
        if (CheetAh::IsChipT214())
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_FIX18_RANGE, 0x0, regVal);
        }
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    }
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_FIX18_RANGE, 0x1, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_FIX18_RANGE, 0x0, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_FIX18_RANGE, 0x1, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_FIX18_RANGE, 0x0, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, regVal, 4));
    CHECK_RC(CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0, &regVal, 4));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_FIX18_RANGE, 0x1, regVal);
    if (CheetAh::IsChipT214())
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_FIX18_RANGE, 0x0, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0, regVal, 4));
    //if(debugExit == "Debug11") {
    //    Printf(Tee::PriNormal, "  Exit afrer set MISC1 \n");
    //    return OK;
    //}

    //xusb_resets
    // release SS from ELPG
    //CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_ELPG_PROGRAM_0, 0x0,4);

    // Reset host partition
    // Reset SS partition
    CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_HOST", 0));
    CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_SS", 0));
    CHECK_RC(ClkMgr::ClkDev()->ToggleReset("XUSB_DEV", 0));

    // do HSIC tracking
    // section 8 in //dev/stdif/xusb/doc/chip/t210/T210_USB_Boot_LP0_ELPG_ELCG_Programming_Guide.doc
/*
1.Program tracking circuit owner (XUSB_PADCTL_USB2_PAD_MUX_0[HSIC_PAD_TRK] = 1 for XUSB)
2.Enable the track clock in CAR (CLK_RST_CONTROLLER_CLK_ENB_Y_SET_0[SET_CLK_ENB_HSIC_TRK]=1                                                                                                   )
  and CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0[USB2_HSIC_TRK_CLK_DIVISOR]=6 for 38.4M osc clock)
3.Program timing parameters (XUSB_PADCTL_HSIC_PAD_TRK_CTL_0[TRK_START_TIMER] = 0x1E and                                                                                                       )
   XUSB_PADCTL_HSIC_PAD_TRK_CTL_0[TRK_DONE_RESET_TIMER] = 0xA)
4.Do tracking (XUSB_PADCTL_HSIC_PAD_TRK_CTL_0[PD_TRK] = 0)
5.Wait for tracking to complete (wait 1ms - this is much more than the actual tracking time, but should be fine)
6.Disable track clock in CAR (CLK_RST_CONTROLLER_CLK_ENB_Y_CLR_0[SET_CLK_ENB_HSIC_TRK]=1)
*/
    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PAD_MUX_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_HSIC_PAD_TRK_RANGE, 0x1, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_PAD_MUX_0, regVal, 4);

    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_CLK_ENB_Y_SET_0_SET_CLK_ENB_HSIC_TRK_RANGE, 0x1, 0);
    CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_ENB_Y_SET_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0_USB2_HSIC_TRK_CLK_DIVISOR_RANGE, 6, regVal);  // for 38.4M
    CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_TRK_START_TIMER_RANGE, 0x1e, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_TRK_DONE_RESET_TIMER_RANGE, 0xa, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_PD_TRK_RANGE, 0x0, regVal);
    CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, regVal, 4);
    Tasker::Sleep(1);

    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_CLK_ENB_Y_CLR_0_CLR_CLK_ENB_HSIC_TRK_RANGE, 0x1, 0);
    CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_CLK_ENB_Y_CLR_0, regVal, 4);
    // HSIC tracking done.

    CHECK_RC(IOMMUtil::DisableSmmuForDev("XUSB_HOST"));
    if(!IsAdbSafe)
    {
        CHECK_RC(IOMMUtil::DisableSmmuForDev("XUSB_DEV"));
    }

    while (forceFsPortIndex)
    {
        if ( ( ( forceFsPortIndex & 0xf ) <= 9 )
            && ( (forceFsPortIndex & 0xf ) >= 5 ) )
        {   // port 1-4 are for SuperSpeed, HS port indexes starts from 5
            // do PD2 override (HS transceivers)
            UINT32 hsPortIndex = (forceFsPortIndex & 0xf) - 5;
            CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 + 0x40*(hsPortIndex), &regVal, 4);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD2_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD2_OVRD_EN_RANGE, 0x1, regVal);
            CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 + 0x40*(hsPortIndex), regVal, 4);
            Printf(Tee::PriNormal, "  Force FS on HS port %d, reset DUT to clear\n", hsPortIndex);
        }
        forceFsPortIndex >>= 4;
    }

    if (isDeviceMode)
    {
        // enable FPCI via IPSF
        UINT32 hostOffset = 0x9000;
        //ref manual https://lwtegra/home/tegra_manuals/html/manuals/t210/arxusb_dev.html
        CheetAh::RegRd(LW_DEVID_XUSB_DEV, 0, hostOffset + 0x180, &regVal, 4);
        regVal = FLD_SET_RF_NUM(0:0, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, hostOffset + 0x180, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_DEV, 0, hostOffset + 0x180, &regVal, 4);

        // setup BAR0
        UINT32 pciOffset = 0x8000;
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, pciOffset + 0x10,  0x700d0000, 4);
        // Enable BusMaster
        CheetAh::RegRd(LW_DEVID_XUSB_DEV, 0, pciOffset + 0x4, &regVal, 4);
        regVal |= 0x6;
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, pciOffset + 0x4, regVal, 4);
    }
    else
    {
        // enable FPCI via IPSF
        UINT32 hostOffset = 0x9000;
        //ref manual https://lwtegra/home/tegra_manuals/html/manuals/t210/arxusb_host.html
        CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, hostOffset + XUSB_HOST_CONFIGURATION_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_HOST_CONFIGURATION_0_EN_FPCI_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, hostOffset + XUSB_HOST_CONFIGURATION_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, hostOffset + XUSB_HOST_CONFIGURATION_0, &regVal, 4);

        // setup BAR0
        UINT32 pciOffset = 0x8000;
        CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x10,  0x70090000, 4);
        // Enable BusMaster
        CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x4, &regVal, 4);
        regVal |= 0x6;
        CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x4, regVal, 4);
        //WAR to disable HSIC wake, bug 1942227
        if(CheetAh::IsChipT214())
        {
            // #define XUSB_CFG_ARU_C11PAGESEL1_0      0x404
            CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x404, &regVal, 4);
            regVal |= 1<<12; //XUSB_HSP0: BIT(12)
            CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x404, regVal, 4);

            // #define XUSB_CFG_HSPX_CORE_CTRL_0       0x600
            CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x600, &regVal, 4);
            regVal &= ~(1<<24);//XUSB_HSIC_PLLCLK_VLD : BIT(24)
            CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x600, regVal, 4);

            CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x404, &regVal, 4);
            regVal &= ~(1<<12);
            CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x404, regVal, 4);
        }
    }

    LOG_EXT();
    return OK;
}

RC T210PreInit::XhciRstrCtrl(const vector<string> &ConfigParams)
{
    LOG_ENT();
    UINT32 regVal;

    bool isSkipClk = false;
    for(UINT32 i = 0; i < ConfigParams.size(); i++)
    {
        if(ConfigParams[i] == "SkipClk") {
            isSkipClk = true;
            Printf(Tee::PriNormal, "XHCI PreInit(): WAR Skip certain CLKs for T210\n");
        }
    }

    // un-lock the PLLE
    /*
    CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_BASE_0_PLLE_ENABLE_RANGE, 0x0, regVal);
    CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_BASE_0, regVal, 4);

    CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLL2E_MISC_0, &regVal, 4);
    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_IDDQ_SWCTL_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_IDDQ_OVERRIDE_VALUE_RANGE, 0x1, regVal);
    CheetAh::RegWr(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, regVal, 4);
    */
    if (!isSkipClk) {
        ClkMgr::ClkDev()->Enable("PLLE", 0);

        // un-lock the PCIE PLL
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_ENABLE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_SLEEP_RANGE, 0x3, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0_PLL0_IDDQ_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_P0_CTL_1_0, regVal, 4);
        // un-lock the SATA PLL
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_ENABLE_RANGE, 0x0, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_SLEEP_RANGE, 0x3, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);
        CheetAh::RegRd(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, &regVal, 4);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0_PLL0_IDDQ_RANGE, 0x1, regVal);
        CheetAh::RegWr(LW_DEVID_XUSB_PADCTL, 0, XUSB_PADCTL_UPHY_PLL_S0_CTL_1_0, regVal, 4);

        ClkMgr::ClkDev()->Enable("XUSB_DEV", 0);
    }

    ClkMgr::ClkDev()->Enable("XUSB_SS", 0);
    ClkMgr::ClkDev()->Enable("XUSB_HOST", 0);

    IOMMUtil::RestoreSmmuForDev("XUSB_HOST");
    if(!IsAdbSafe)
    {
        IOMMUtil::RestoreSmmuForDev("XUSB_DEV");
    }

    LOG_EXT();
    return OK;
}

