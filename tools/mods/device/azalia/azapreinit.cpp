/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "cheetah/include/t30preinit.h"
#include "cheetah/include/t114preinit.h"
#include "cheetah/include/t148preinit.h"
#include "cheetah/include/t124preinit.h"
#include "cheetah/include/t132preinit.h"
#include "cheetah/include/t210preinit.h"
#include "cheetah/include/t186preinit.h"
#include "cheetah/include/tegrareg.h"
#include "core/include/platform.h"
#include "cheetah/dev/clk/clkdev.h"
#include "cheetah/dev/pmc/pwrg/pwrgdev.h"
#include "cheetah/include/devid.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/iommutil.h"
#include "core/include/xp.h"
#include "cheetah/include/cheetah.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "T30PreInit"

//Azalia specific defines
#define HDA_CONFIGURATION_0                                     0x180
#define HDA_CONFIGURATION_0_EN_FPCI                             0:0
#define HDA_FPCI_BAR0_0                                         0x80
#define HDA_FPCI_BAR0_0_FPCI_BAR0_START                         31:4
#define HDA_FPCI_BAR0_0_FPCI_BAR0_ACCESS_TYPE                   0:0
#define AZA_CFG_1_0                                             0x00000004
#define AZA_CFG_1_0_IO_SPACE                                    0:0
#define AZA_CFG_1_0_MEMORY_SPACE                                1:1
#define AZA_CFG_1_0_BUS_MASTER                                  2:2
#define AZA_CFG_1_0_SERR                                        8:8
#define AZA_CFG_4_0                                             0x00000010

enum AzaIntf
{
   INTF_NONE,
   INTF_DAP1,
   INTF_DAP2,
   INTF_PE0,
   INTF_PE2
};

UINT32 T30PreInit::s_AzaHandleList[]={0,0,0};

//---------------------------------------------------------------------
// T186 PreInit Functions
//---------------------------------------------------------------------

RC T186PreInit::AzaCfgCtrl(const vector<string> &ConfigParams)
{
    const UINT32 HDA_IPFS_CONFIG = 0x180;
    const UINT32 HDA_CFG_CMD = 0x1004;
    const UINT32 HDA_CFG_BAR0 = 0x1010;

    RC rc = OK;

    CHECK_RC(IOMMUtil::DisableSmmuForDev("HDAR"));
    CHECK_RC(IOMMUtil::DisableSmmuForDev("HDAW"));

    CHECK_RC(Xp::InteractiveFileWrite("/d/bpmp/debug/clk/hda/state", 1));
    CHECK_RC(Xp::InteractiveFileWrite("/d/bpmp/debug/clk/hda2hdmicodec/state", 1));
    CHECK_RC(Xp::InteractiveFileWrite("/d/bpmp/debug/clk/hda2codec_2x/state", 1));

    CheetAh::RegWr(LW_DEVID_CAR,0,0x340000,0,4);
    CheetAh::RegWr(LW_DEVID_CAR,0,0x3d0000,0,4);
    CheetAh::RegWr(LW_DEVID_CAR,0,0x3b0000,1,4);

    // enable PCI access
    UINT32 reg;
    CHECK_RC(CheetAh::RegRd(LW_DEVID_HDA, 0, HDA_IPFS_CONFIG, &reg, 4));
    reg |= RF_SET(HDA_CONFIGURATION_0_EN_FPCI);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_IPFS_CONFIG, reg, 4));

    // enable MEM/IO space and bus master
    UINT32 cfg1 = RF_SET(AZA_CFG_1_0_IO_SPACE) |
                  RF_SET(AZA_CFG_1_0_BUS_MASTER) |
                  RF_SET(AZA_CFG_1_0_MEMORY_SPACE) |
                  RF_SET(AZA_CFG_1_0_SERR);

    CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_CFG_CMD, cfg1, 4);

    UINT32 bar0Offset = 1<<14;
    CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_CFG_BAR0, 0xFFFFFFFF, 4);
    CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_CFG_BAR0, bar0Offset, 4);
    CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_FPCI_BAR0_0, 0x40, 4);
    return OK;
}

RC T186PreInit::AzaRstrCtrl(const vector<string> &ConfigParams)
{
    return OK;
}

DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T186, Aza, CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T186, Aza, RstrPmu)

// End T210 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T210 PreInit Functions
//---------------------------------------------------------------------

DEFINE_DUPLICATE_FUNCTION(T210, T124, Aza, CfgCtrl)
DEFINE_DUPLICATE_FUNCTION(T210, T124, Aza, RstrCtrl)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T210, Aza, CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T210, Aza, RstrPmu)

// End T210 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T132 PreInit Functions
//---------------------------------------------------------------------

DEFINE_DUPLICATE_FUNCTION(T132, T124, Aza, CfgCtrl)
DEFINE_DUPLICATE_FUNCTION(T132, T124, Aza, RstrCtrl)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T132,Aza,CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T132,Aza,RstrPmu)

// End T132 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T124 PreInit Functions
//---------------------------------------------------------------------
RC T124PreInit::AzaRstrCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;

    CHECK_RC(IOMMUtil::RestoreSmmuForDev("HDA"));
    return T114PreInit::AzaRstrCtrl(ConfigParams);
}

RC T124PreInit::AzaCfgCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;
    CHECK_RC(IOMMUtil::DisableSmmuForDev("HDA"));

    string chipName;

    CHECK_RC(CheetAh::GetChipName(&chipName));

    if (chipName != "T214") {
        CHECK_RC(PwrgMgr::PwrgDev()->On("DISA"));
    }

    return T114PreInit::AzaCfgCtrl(ConfigParams);
}

DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T124,Aza,CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T124,Aza,RstrPmu)
// End T124 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T148 PreInit Functions
//---------------------------------------------------------------------
RC T148PreInit::AzaRstrCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;

    CHECK_RC(T114PreInit::AzaRstrCtrl(ConfigParams));

    return rc;
}

RC T148PreInit::AzaCfgCtrl(const vector<string> &ConfigParams)
{
    return T114PreInit::AzaCfgCtrl(ConfigParams);
}

DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T148,Aza,CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T148,Aza,RstrPmu)
// End T148 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T114 PreInit Functions
//---------------------------------------------------------------------
RC T114PreInit::AzaCfgCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;
    UINT32 reg = 0;
    void* virtBase [] = {0};
    Clk   *pClk   = ClkMgr::ClkDev();
    UINT32 fpciBar0=0;

    LOG_ENT();

    CHECK_RC(pClk->SetSrc("HDA", "PLLP_OUT0"));
    CHECK_RC(pClk->SetSrc("HDA2CODEC2X", "PLLP_OUT0"));
    CHECK_RC(pClk->SetDevRate("HDA", 48000000, true));
    CHECK_RC(pClk->SetDevRate("HDA2CODEC2X", 48000000, true));

    CHECK_RC(pClk->Enable("HDA", true));
    CHECK_RC(pClk->Enable("HDA2CODEC2X", true));
    CHECK_RC(pClk->Enable("HDA2HDMI", true));

    CHECK_RC(CheetAh::RegRd(LW_DEVID_HDA, 0, HDA_CONFIGURATION_0,&reg, 4));
    reg |= RF_SET(HDA_CONFIGURATION_0_EN_FPCI);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_CONFIGURATION_0,reg, 4));

    UINT32 bar0Offset = 1<<14;

    CHECK_RC(CheetAh::RegRd(LW_DEVID_HDA, 0, HDA_FPCI_BAR0_0,&fpciBar0, 4));
    fpciBar0 = FLD_SET_RF_NUM(HDA_FPCI_BAR0_0_FPCI_BAR0_ACCESS_TYPE, 0, fpciBar0);
    fpciBar0 = FLD_SET_RF_NUM(HDA_FPCI_BAR0_0_FPCI_BAR0_START, bar0Offset >> 12, fpciBar0);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_FPCI_BAR0_0,fpciBar0, 4));

    UINT32 cfg1 = RF_SET(AZA_CFG_1_0_IO_SPACE) |
                  RF_SET(AZA_CFG_1_0_BUS_MASTER) |
                  RF_SET(AZA_CFG_1_0_MEMORY_SPACE) |
                  RF_SET(AZA_CFG_1_0_SERR);

    Platform::MapDeviceMemory(&virtBase[0], LW_T30_HDA_MM_APERTURE, LW_T30_HDA_MM_SLAB_SIZE, Memory::UC, Memory::ReadWrite);
    MEM_WR32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_1_0, cfg1);
    MEM_WR32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_4_0, bar0Offset);
    reg=MEM_RD32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_4_0);
    if ( bar0Offset != ((reg) & ~0x3ff) )
    {
        Printf(Tee::PriError, "Bar 0 Readback misamtch\n");
        return RC::DATA_MISMATCH;
    }

    Platform::UnMapDeviceMemory(virtBase[0]);

    LOG_EXT();

    return rc;
}

RC T114PreInit::AzaRstrCtrl(const vector<string> &ConfigParams)
{
    return T30PreInit::AzaRstrCtrl(ConfigParams);
}

DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T114,Aza,CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T114,Aza,RstrPmu)
// End T114 PreInit Functions
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// T30 PreInit Functions
//---------------------------------------------------------------------
RC T30PreInit::AzaCfgCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;
    UINT32 reg = 0;
    void* virtBase [] = {0};
    Clk   *pClk   = ClkMgr::ClkDev();
    UINT32 fpciBar0=0;

    LOG_ENT();

    CHECK_RC(pClk->Enable("HDA", true));
    CHECK_RC(pClk->Enable("HDA2CODEC2X", true));
    CHECK_RC(pClk->Enable("HDA2HDMI", true));

    //@todo: Still need this mapping as there seems to be no aperture defined in lwrm_module for LW_T30_HDA_MM_APERTURE+LW_T30_HDA_MM_CONFIG_START_OFFSET
    //try to remove this
    Platform::MapDeviceMemory(&virtBase[0], LW_T30_HDA_MM_APERTURE, LW_T30_HDA_MM_SLAB_SIZE, Memory::UC, Memory::ReadWrite);

    CHECK_RC( pClk->RegWr( 0x10,0xFDFFFFF1)); // CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0
    CHECK_RC( pClk->RegWr( 0x14,0xFEFFF7F7)); // CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0
    CHECK_RC( pClk->RegWr( 0x18,0x75F79BFF)); // CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0
    CHECK_RC( pClk->RegWr( 0x360,0xFFFFFFFF)); // CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0
    CHECK_RC( pClk->RegWr( 0x364,0x00003FFF)); // CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0
    CHECK_RC( pClk->RegWr( 0xD0,0x8002520D)); // CLK_RST_CONTROLLER_PLLD_BASE_0
    CHECK_RC( pClk->RegWr( 0xD0,0xC002520D)); // CLK_RST_CONTROLLER_PLLD_BASE_0
    CHECK_RC( pClk->RegWr( 0xD0,0x4002520D)); // CLK_RST_CONTROLLER_PLLD_BASE_0
    CHECK_RC( pClk->RegWr( 0xDC,0x40000800)); // CLK_RST_CONTROLLER_PLLD_MISC_0
    CHECK_RC( pClk->RegWr( 0xB0,0x80003119)); // CLK_RST_CONTROLLER_PLLA_BASE_0
    CHECK_RC( pClk->RegWr( 0xB0,0xC0003119)); // CLK_RST_CONTROLLER_PLLA_BASE_0
    CHECK_RC( pClk->RegWr( 0xB0,0x40003119)); // CLK_RST_CONTROLLER_PLLA_BASE_0

    CHECK_RC(CheetAh::RegRd(LW_DEVID_HDA, 0, HDA_CONFIGURATION_0,&reg, 4));
    reg |= RF_SET(HDA_CONFIGURATION_0_EN_FPCI);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_CONFIGURATION_0,reg, 4));

    UINT32 bar0Offset = 1<<14;

    CHECK_RC(CheetAh::RegRd(LW_DEVID_HDA, 0, HDA_FPCI_BAR0_0,&fpciBar0, 4));
    fpciBar0 = FLD_SET_RF_NUM(HDA_FPCI_BAR0_0_FPCI_BAR0_ACCESS_TYPE, 0, fpciBar0);
    fpciBar0 = FLD_SET_RF_NUM(HDA_FPCI_BAR0_0_FPCI_BAR0_START, bar0Offset >> 12, fpciBar0);
    CHECK_RC(CheetAh::RegWr(LW_DEVID_HDA, 0, HDA_FPCI_BAR0_0,fpciBar0, 4));

    UINT32 cfg1 = RF_SET(AZA_CFG_1_0_IO_SPACE) |
                  RF_SET(AZA_CFG_1_0_BUS_MASTER) |
                  RF_SET(AZA_CFG_1_0_MEMORY_SPACE) |
                  RF_SET(AZA_CFG_1_0_SERR);

    MEM_WR32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_1_0, cfg1);
    MEM_WR32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_4_0, bar0Offset);
    reg=MEM_RD32(reinterpret_cast<unsigned long>(virtBase[0])+LW_T30_HDA_MM_CONFIG_START_OFFSET + AZA_CFG_4_0);
    if ( bar0Offset != ((reg) & ~0x3ff) )
    {
        Printf(Tee::PriError, "Bar 0 Readback misamtch\n");
        return RC::DATA_MISMATCH;
    }

    Platform::UnMapDeviceMemory(virtBase[0]);

    LOG_EXT();
    return rc;
}

RC T30PreInit::AzaRstrCtrl(const vector<string> &ConfigParams)
{
    RC rc = OK;
    Clk *pClk = ClkMgr::ClkDev();

    LOG_ENT();

    CHECK_RC(pClk->Enable("HDA", false));
    CHECK_RC(pClk->Enable("HDA2CODEC2X", false));
    CHECK_RC(pClk->Enable("HDA2HDMI", false));

    LOG_EXT();
    return rc;
}

DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T30,Aza,CfgPmu)
DEFINE_UNSUPPORTED_FUNCTION_NO_BOARD_ID(T30,Aza,RstrPmu)

// End T30 PreInit Functions
//---------------------------------------------------------------------

