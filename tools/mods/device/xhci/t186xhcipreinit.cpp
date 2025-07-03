/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/t186preinit.h"
#include "cheetah/include/sysutil.h"
#include "cheetah/dev/clk/clkdev.h"
#include "cheetah/include/devid.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/dtnode.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "cheetah/include/t186uphy.h"
#include "t18x/t186/arxusb_padctl.h"
#include "t18x/t186/arclk_rst.h"
#include "cheetah/include/iommutil.h"
#include <stdio.h>
#include "cheetah/dev/fuse/skut186.h"
#include "xusbpadctrl.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "T186XhciPreInit"

#define CLK_REG_UPDATE(reg,rf,n)                            \
{                                                           \
    CheetAh::RegRd(LW_DEVID_CAR, 0, reg, &regVal, 4);         \
    regVal = FLD_SET_RF_NUM(reg##_##rf##_RANGE, n, regVal); \
    CheetAh::RegWr(LW_DEVID_CAR, 0, reg, regVal, 4);          \
}

#define    XUSB_DEV_XHCI_HSFSPI_COUNT0           0x100
#define    XUSB_DEV_XHCI_HSFSPI_COUNT16          0x19c

namespace
{
UINT32 IsPrivatePlle = 0;
UINT32 IsAdbSafe= 0;

RC T186PadctlClkReset()
{
    UINT32 regVal;
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,CLK_ENB_XUSB,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0,SWR_XUSB_PADCTL_RST,0);
    return OK;
}
RC T186EnPlle()
{
    UINT32 regVal;
    Printf(Tee::PriLow,"Disabling PLLE \n");

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_ENABLE,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC_0,PLLE_IDDQ_SWCTL,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC_0,PLLE_IDDQ_OVERRIDE_VALUE,0x1);

    Printf(Tee::PriLow,"programming PLLE \n");
    CLK_REG_UPDATE( CLK_RST_CONTROLLER_PLLE_MISC_0,PLLE_IDDQ_OVERRIDE_VALUE,0x0);
    Tasker::Sleep(1);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC1_0,PLLE_SDM_RESET,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC1_0,PLLE_EN_DITHER,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC1_0,PLLE_EN_SDM,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC1_0,PLLE_EN_SSC,0);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_MDIV,2);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_NDIV,125);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_PLDIV_CML,24);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_MISC_0,PLLE_LOCK_ENABLE,0x1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_LOCK_OVERRIDE,0x0);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_BYPASS_SS,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCBYP,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_INTERP_RESET,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCMAX,0x29);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCINCINTRV,0x1c);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCINC,1);
    // Disable Center Spread
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCCENTER,0);
    // Enable Down Spread
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCILWERT,0);

    Printf(Tee::PriLow,"Enabling PLLE \n");
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_BASE_0,PLLE_ENABLE,1);

    UINT32 elapseMs = 0;
    do{
        Tasker::Sleep(1); elapseMs++;
        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLE_MISC_0, &regVal, 4);
    }while(FLD_TEST_RF_NUM(CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_RANGE, 0, regVal) && (elapseMs<=2000));
    if (elapseMs > 2000){ Printf(Tee::PriError, "XhciPreInit: PLLE_LOCK set timeout \n"); }

    Printf(Tee::PriLow,"enabling analog spread \n ");

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_BYPASS_SS,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_SSCBYP,0);
    Tasker::Sleep(1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0,PLLE_INTERP_RESET,0);

    return OK;
}
/*
RC T186EnPllRefe()
{
    UINT32 regVal;
    Printf(Tee::PriLow,"Enabling PLLREFE\n");
    CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLREFE_MISC_0, &regVal, 4);
    if (FLD_TEST_RF_NUM(CLK_RST_CONTROLLER_PLLREFE_MISC_0_PLLREFE_LOCK_RANGE, 1, regVal))
    {
        Printf(Tee::PriLow,"PLLREFE alrady clocked\n");
        return OK;
    }
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_MISC_0, PLLREFE_IDDQ, 0x0);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_BASE_0,PLLREFE_ENABLE,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_BASE_0,PLLREFE_DIVM,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_BASE_0,PLLREFE_DIVN,56);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_BASE_0,PLLREFE_DIVP,56);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_PLLREFE_BASE_0,PLLREFE_ENABLE,1);
    UINT32 elapseMs = 0;
    do{
        Tasker::Sleep(1); elapseMs++;
        CheetAh::RegRd(LW_DEVID_CAR, 0, CLK_RST_CONTROLLER_PLLREFE_MISC_0, &regVal, 4);
    }while(FLD_TEST_RF_NUM(CLK_RST_CONTROLLER_PLLREFE_MISC_0_PLLREFE_LOCK_RANGE, 0, regVal) && (elapseMs<=2000));
    if (elapseMs > 2000){ Printf(Tee::PriError, "XhciPreInit: PLLREFE_LOCK set timeout \n"); }
    return OK;
}
*/
RC T186PexPadPllMgmt()
{
    UINT32 regVal;
    Printf(Tee::PriLow,"Enabling Pex pad PLL \n");
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_PEX_USB_PAD_PLL0_MGMT_0,PEX_USB_PAD_PLL0_MGMT_CLK_CE,0x1);
    Printf(Tee::PriLow,"Release UPHY RST and program CAL overrides \n");
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_PEX_USB_UPHY_0,SWR_PEX_USB_UPHY_PLL0_RST,0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_PEX_USB_UPHY_0,SWR_PEX_USB_UPHY_RST,0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_UPHY_0,SWR_UPHY_RST,0x0);
    return OK;
}
RC T186InitClks()
{
    UINT32 regVal;
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,CLK_ENB_XUSB_HOST,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,CLK_ENB_XUSB_DEV,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,CLK_ENB_XUSB_SS,1);
    // XUSB_CORE_HOST
    // SELECT SOURCE = PLLP 408MHZ / 4
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0,XUSB_CORE_HOST_CLK_DIVISOR,0x4);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0,XUSB_CORE_HOST_CLK_SRC,1);
    // XUSB_CORE_DEV
    // SELECT SOURCE = PLLP 408MHz / 4
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_DEV_0,XUSB_CORE_DEV_CLK_DIVISOR,0x4);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_DEV_0,XUSB_CORE_DEV_CLK_SRC,1);
    // FALCON
    // SELECT SOURCE = PLLP 408MHz/2
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0,XUSB_FALCON_CLK_DIVISOR,2);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0,XUSB_FALCON_CLK_SRC,1);
    // SS
    // SELECT SOURCE = HSIC_480 480MHZ / 4
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0,XUSB_SS_CLK_DIVISOR,6);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0,XUSB_SS_CLK_SRC,3);
    // FS
    // SELECT SOURCE = FO_48M / 1
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0,XUSB_FS_CLK_SRC,2);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0,XUSB_FS_CLK_DIVISOR,0);
    return OK;
}
RC T186PadTrack()
{
    RC rc = OK;
    UINT32 regVal;
    Printf(Tee::PriLow, "Pad track \n");
    // Enable Clocks
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_SET_0,SET_CLK_ENB_USB2_TRK,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_SET_0,SET_CLK_ENB_HSIC_TRK,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0,USB2_HSIC_TRK_CLK_DIVISOR,0x6);

    CHECK_RC(XusbPadCtrlMgr::GetPadCtrl()->EnablePadTrack());

    // disable the track clocks
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_CLR_0,CLR_CLK_ENB_USB2_TRK,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_CLR_0,CLR_CLK_ENB_HSIC_TRK,1);

    return rc;
}
RC T186EnUtmipSamplers()
{
    UINT32 regVal;
    Printf(Tee::PriLow,"Enabling UTMIPLL Samplers \n");
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG3_0,UTMIP_PLL_REF_DIS,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0,UTMIPLL_IDDQ_OVERRIDE_VALUE,0);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0, UTMIP_PLL_MDIV,0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0, UTMIP_PLL_NDIV,0x19);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0, UTMIP_PLL_KCP,0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0, UTMIP_PLL_KVCO,0x0 );

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0, UTMIP_FORCE_PLL_ENABLE_POWERUP, 1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0, UTMIP_FORCE_PLL_ENABLE_POWERDOWN,0);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_A_POWERDOWN,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_A_POWERUP,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_B_POWERDOWN,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_B_POWERUP,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_C_POWERDOWN,0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_FORCE_PD_SAMP_C_POWERUP,1);

    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, UTMIP_PLLU_STABLE_COUNT, 0x50);

    Printf(Tee::PriLow," HW sequencer \n");
    //CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0, UTMIP_FORCE_PLL_ENABLE_POWERUP, 0x0);
    //CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0, UTMIP_FORCE_PLL_ENABLE_POWERDOWN, 0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_IDDQ_SWCTL, 0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_IDDQ_PD_INCLUDE, 0x1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_XUSB_PLL_CFG0_0, UTMIPLL_USE_SWITCH_DETECT, 0x1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_XUSB_PLL_CFG0_0, UTMIPLL_CLK_SWITCH_SWCTL, 0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_CLK_ENABLE_SWCTL, 0x0);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_USE_LOCKDET, 0x1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_XUSB_PLL_CFG0_0, UTMIPLL_LOCK_DLY, 0x0);
    Tasker::Sleep(1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_SEQ_ENABLE, 0x1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0, UTMIPLL_SEQ_IN_SWCTL, 0x0);

    return OK;
}
RC T186ReleaseRsts()
{
    UINT32 regVal;
    Printf(Tee::PriLow, "Releasing XUSB resets\n");
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_HOST_RST,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_HOST_RST,0);
    if(!IsAdbSafe)
    {
        CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_DEV_RST,1);
        CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_DEV_RST,0);
    }
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_SS_RST,1);
    CLK_REG_UPDATE(CLK_RST_CONTROLLER_RST_DEV_XUSB_0, SWR_XUSB_SS_RST,0);
    return OK;
}
}

RC T186PreInit::XhciCfgPmu(const vector<string> &ConfigParams)
{
    RC rc = OK;
    CHECK_RC(SysUtil::Board::ApplySettings("XUSB_PWR"));
    return rc;
}

RC T186PreInit::XhciRstrPmu(const vector<string> &ConfigParams)
{
    return OK;
}

// implementation of //dev/stdif/xusb/doc/chip/t186/T186_USB_Boot_LP0_ELPG_ELCG_Programming_Guide.doc
RC T186PreInit::XhciCfgCtrl(const vector<string> &ConfigParams)
{
    LOG_ENT();
    RC rc;
    UINT32 regVal;

    unique_ptr<T186Uphy> pUphy(new T186Uphy());
    UINT32 isDeviceMode = 0;
    UINT32 isForceHS = 0;
    UINT32 isForceFS = 0;
    // bool IsPrivatePlle = false;
    UINT32 forceFsPortIndex = 0;
    string debugExit;
    string pinCtrlName = "pinctrl@3520000";
    string pinCtrlProdNames[] = {"prod_c_utmi0",
                                 "prod_c_utmi1",
                                 "prod_c_utmi2",
                                 "prod_c_ss0",
                                 "prod_c_ss1",
                                 "prod_c_ss2"
                                };

    CHECK_RC(pUphy->BasicInit("HSIO", LW_DEVID_UPHY_LANE, LW_DEVID_UPHY_PLL));
    CHECK_RC(GetOptionalParam(ConfigParams, "DevMode", isDeviceMode, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "Pll", IsPrivatePlle, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "ForceHS", isForceHS, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "ForceFS", isForceFS, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "AdbSafe", IsAdbSafe, 0));
    CHECK_RC(GetOptionalParam(ConfigParams, "Debug", debugExit, 0));

    Printf(Tee::PriNormal, 
           "XHCI PreInit(): %s %s %s %s\n", 
           isDeviceMode ? "Device Mode" : "",
           IsPrivatePlle ? "Private PLLe" : "",
           isForceHS ? "ForceHS" : "",
           IsAdbSafe ? "Adb Safe Mode" : "");

    if(isForceFS)
    {
        CHECK_RC(GetOptionalParam(ConfigParams, "ForceFsPort", forceFsPortIndex, 0));
        Printf(Tee::PriNormal, "XHCI PreInit(): ForceFS - 0x%x \n", forceFsPortIndex);
    }
    if (debugExit == "Debug1")
    {
        Printf(Tee::PriNormal, "  Exit~ at %s \n", debugExit.c_str()); 
        return OK;
    }

    CHECK_RC(IOMMUtil::DisableSmmuForDev("XUSB_HOSTW"));
    CHECK_RC(IOMMUtil::DisableSmmuForDev("XUSB_HOSTR"));
    // === PAD driver switches USB 2.0 related PLLs to under HW control after PAD driver is loaded.
    if (!IsPrivatePlle)
    {
        // Initializing XUSB PADCTL clocks/resets
        ClkMgr::ClkDev()->Enable("XUSB", 1);
        ClkMgr::ClkDev()->ToggleReset("XUSB_PADCTL", 0);
        // Enable plle_refe and  PLLE
        ClkMgr::ClkDev()->Enable("pllrefe_out", 1);
        ClkMgr::ClkDev()->Enable("PLLE", 1);
        // Enabling Pex pad PLL

        ClkMgr::ClkDev()->Enable("pllp", 1);
        ClkMgr::ClkDev()->Enable("PLLP_UPHY", 1);
        // ClkMgr::ClkDev()->Enable("PLLU", 1);        //UTMI
        ClkMgr::ClkDev()->Enable("PEX_USB_PAD0_MGMT", 1);
        Printf(Tee::PriLow,"Release UPHY RST and program CAL overrides \n");
        ClkMgr::ClkDev()->ToggleReset("UPHY", 0);
        ClkMgr::ClkDev()->ToggleReset("PEX_USB_UPHY_PLL0", 0);
        ClkMgr::ClkDev()->ToggleReset("PEX_USB_UPHY", 0);
    }
    else
    {
        T186PadctlClkReset();
        //T186EnPllRefe();
        ClkMgr::ClkDev()->Enable("pllrefe_out", 1); // hybrid
        T186EnPlle();
        T186PexPadPllMgmt();
    }
    CHECK_RC(SysUtil::Board::ApplySettings("XUSB_BRDMUX"));
    //CHECK_RC(pUphy->Init(0));

    CHECK_RC(pUphy->PllSwOverride(0));
    CHECK_RC(pUphy->XhciPllDefaults());
    CHECK_RC(pUphy->Pll0Setup());
    CHECK_RC(pUphy->PllSwOverrideRemove(0));

    // PROD Settings programming - UPHY, see also http://lwbugs/200177072
    CHECK_RC(pUphy->XhciLaneDefaults(0));
    CHECK_RC(pUphy->XhciLaneDefaults(1));
    CHECK_RC(pUphy->XhciLaneDefaults(2));
    // PROD Settings programming ends

    // assign the UPHY lanes to XUSB, PCIE, SATA, or UFS according to the platform specific configuration
    CHECK_RC(pUphy->LaneOwnership(0x7, 0));
    CHECK_RC(pUphy->RemoveClamp(0x7));
    if(isForceHS)
    {
        CHECK_RC(pUphy->LaneForceIddq(0x7, 0));
    }

    CHECK_RC(XusbPadCtrlMgr::GetPadCtrl()->PadInit(isDeviceMode?0:0xff, IsAdbSafe));

    if (DtNode::IsDtProdUpdateEnabled())
    {
        for (unsigned int i=0 ; i<(sizeof(pinCtrlProdNames) / sizeof(pinCtrlProdNames[0])) ; i++)
        {
            DtNode dtNode(pinCtrlName);
            dtNode.ReadDtandUpdateProd(pinCtrlProdNames[i]);
        }
    }

    if (!IsPrivatePlle)
    {
        ClkMgr::ClkDev()->Enable("XUSB_HOST", 1);
        ClkMgr::ClkDev()->Enable("XUSB_DEV", 1);
        ClkMgr::ClkDev()->Enable("XUSB_SS", 1);

        // XUSB_CORE_HOST
        // SELECT SOURCE = PLLP 408MHZ / 4
        // comment out XUSB_CORE_HOST clock as it's done by bpmp
//        CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0,XUSB_CORE_HOST_CLK_DIVISOR,0x4);
//        CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_HOST_0,XUSB_CORE_HOST_CLK_SRC,1);
        // XUSB_CORE_DEV
        // SELECT SOURCE = PLLP 408MHz / 4
        ClkMgr::ClkDev()->SetSrc("XUSB_CORE_DEV", "pllp_xusb");
        ClkMgr::ClkDev()->SetDevRate("XUSB_CORE_DEV", 102*1000*1000, 1);
        // SELECT SOURCE = PLLP 408MHz/2
        // CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FALCON_0,XUSB_FALCON_CLK_SRC,1);
        ClkMgr::ClkDev()->SetSrc("XUSB_FALCON", "pllp_xusb");
        ClkMgr::ClkDev()->SetDevRate("XUSB_FALCON", 204*1000*1000, 1);
        // SELECT SOURCE = HSIC_480 480MHZ / 4

        //comment out XUSB_SS clock as it's done by bpmp
//        CLK_REG_UPDATE(CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0,XUSB_SS_CLK_SRC,3);
//        ClkMgr::ClkDev()->SetDevRate("XUSB_SS", 120*1000*1000, 1);
        // SELECT SOURCE = FO_48M / 1
        ClkMgr::ClkDev()->SetSrc("XUSB_FS", "pllu_48M");
        ClkMgr::ClkDev()->SetDevRate("XUSB_FS", 48*1000*1000, 1);

        // Enabling UTMIPLL Samplers
        ClkMgr::ClkDev()->Enable("UTMIPLL", 1);
        T186ReleaseRsts();
    }
    else
    {
        T186InitClks();
        T186EnUtmipSamplers();
        T186ReleaseRsts();
    }
    T186PadTrack();

    while (forceFsPortIndex)
    {
        if ( ( ( forceFsPortIndex & 0xf ) <= 6 )
            && ( (forceFsPortIndex & 0xf ) >= 4 ) )
        {   // port 1-3 are for SuperSpeed, HS port indexes starts from 4
            // do PD2 override (HS transceivers)
            UINT32 hsPortIndex = (forceFsPortIndex & 0xf) - 4;
            CHECK_RC(XusbPadCtrlMgr::GetPadCtrl()->ForcePortFS(hsPortIndex));
            Printf(Tee::PriNormal, "  Force FS on HS port %d, reset DUT to clear\n", hsPortIndex);
        }
        forceFsPortIndex >>= 4;
    }

    if (isDeviceMode)
    {
        //Fix for HS link stability issue
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, XUSB_DEV_XHCI_HSFSPI_COUNT0,  0x3e8, 4);
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, XUSB_DEV_XHCI_HSFSPI_COUNT16, 0x5840, 4);
    }

    UINT32 pciOffset = 0x8000;
    if (isDeviceMode)
    {   // Enable BusMaster
        CheetAh::RegRd(LW_DEVID_XUSB_DEV, 0, pciOffset + 0x4, &regVal, 4);
        regVal |= 0x6;
        CheetAh::RegWr(LW_DEVID_XUSB_DEV, 0, pciOffset + 0x4, regVal, 4);
    }
    else
    {   // Enable BusMaster
        CheetAh::RegRd(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x4, &regVal, 4);
        regVal |= 0x6;
        CheetAh::RegWr(LW_DEVID_XUSB_HOST, 0, pciOffset + 0x4, regVal, 4);
    }

    LOG_EXT();
    return OK;
}

RC T186PreInit::XhciRstrCtrl(const vector<string> &ConfigParams)
{
    LOG_ENT();

    if (IsPrivatePlle)
    {   // WAR: if we don't enable clk first, disable will cause panic, so we enable them
        ClkMgr::ClkDev()->Enable("PLLE", 1);
        ClkMgr::ClkDev()->Enable("XUSB_SS", 1);
        ClkMgr::ClkDev()->Enable("XUSB_DEV", 1);
        ClkMgr::ClkDev()->Enable("XUSB_HOST", 1);
    }

    ClkMgr::ClkDev()->Enable("PLLE", 0);
    ClkMgr::ClkDev()->Enable("XUSB_SS", 0);
    ClkMgr::ClkDev()->Enable("XUSB_DEV", 0);
    ClkMgr::ClkDev()->Enable("XUSB_HOST", 0);

    IOMMUtil::RestoreSmmuForDev("XUSB_HOSTW");
    IOMMUtil::RestoreSmmuForDev("XUSB_HOSTR");

    LOG_EXT();
    return OK;
}
