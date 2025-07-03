/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef  _POWER_COMMON_KEPLER_H_
#define  _POWER_COMMON_KEPLER_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"
#include "gpu_socv.h"

enum GK1XX_POWER_CLOCK {
    GK1XX_POWER_CLOCK_XTAL_IN,
    GK1XX_POWER_CLOCK_XTALS_IN,
    GK1XX_POWER_CLOCK_SWCLK,
    GK1XX_POWER_CLOCK_XTAL4X,
    GK1XX_POWER_CLOCK_XTAL8X,
    GK1XX_POWER_CLOCK_XTAL16X,
    GK1XX_POWER_CLOCK_PEX_REFCLK_FAST,
    GK1XX_POWER_CLOCK_HOSTCLK_DIV,
    GK1XX_POWER_CLOCK_EXT_REFCLK,
    GK1XX_POWER_CLOCK_LTCPLL_OUT,
    GK1XX_POWER_CLOCK_XBARPLL_OUT,
    GK1XX_POWER_CLOCK_SPPLL0,
    GK1XX_POWER_CLOCK_SPPLL1,
    GK1XX_POWER_CLOCK_SYSPLL_OUT,
    GK1XX_POWER_CLOCK_GPCLDIV_OUT,
    GK1XX_POWER_CLOCK_GPCPLL_VCOOUT,
    GK1XX_POWER_CLOCK_ALT_GPC2CLK,
    GK1XX_POWER_CLOCK_LTCLDIV_OUT,
    GK1XX_POWER_CLOCK_LTCPLL_VCOOUT,
    GK1XX_POWER_CLOCK_ALT_LTC2CLK,
    GK1XX_POWER_CLOCK_XBARLDIV_OUT,
    GK1XX_POWER_CLOCK_XBARPLL_VCOOUT,
    GK1XX_POWER_CLOCK_ALT_XBAR2CLK,
    GK1XX_POWER_CLOCK_VPLL0LDIV_OUT,
    GK1XX_POWER_CLOCK_VPLL0_OUT,
    GK1XX_POWER_CLOCK_ALT_VCLK0,
    GK1XX_POWER_CLOCK_VPLL1LDIV_OUT,
    GK1XX_POWER_CLOCK_VPLL1_OUT,
    GK1XX_POWER_CLOCK_ALT_VCLK1,
    GK1XX_POWER_CLOCK_VPLL2LDIV_OUT,
    GK1XX_POWER_CLOCK_VPLL2_OUT,
    GK1XX_POWER_CLOCK_ALT_VCLK2,
    GK1XX_POWER_CLOCK_VPLL3LDIV_OUT,
    GK1XX_POWER_CLOCK_VPLL3_OUT,
    GK1XX_POWER_CLOCK_ALT_VCLK3
};

//make sure gf1xx_lw_clock_onesrc_name[] is up-to-date
//onesrc clock, OSM output.
enum GK1XX_POWER_ONESRC {
    GK1XX_POWER_ONESRC_GPCPLL_REFCLK,
    GK1XX_POWER_ONESRC_LTCPLL_REFCLK,
    GK1XX_POWER_ONESRC_XBARPLL_REFCLK,
    GK1XX_POWER_ONESRC_SYSPLL_REFCLK,
    GK1XX_POWER_ONESRC_REFMPLL_REFCLK,
    GK1XX_POWER_ONESRC_VPLL0_REFCLK,
    GK1XX_POWER_ONESRC_VPLL1_REFCLK,
    GK1XX_POWER_ONESRC_VPLL2_REFCLK,
    GK1XX_POWER_ONESRC_VPLL3_REFCLK,
    GK1XX_POWER_ONESRC_ALT_GPC2CLK,
    GK1XX_POWER_ONESRC_ALT_LTC2CLK,
    GK1XX_POWER_ONESRC_ALT_XBAR2CLK,
    GK1XX_POWER_ONESRC_ALT_DRAMCLK,
    GK1XX_POWER_ONESRC_ALT_HOSTCLK,
    GK1XX_POWER_ONESRC_ALT_XCLK,
    GK1XX_POWER_ONESRC_ALT_VCLK0,
    GK1XX_POWER_ONESRC_ALT_VCLK1,
    GK1XX_POWER_ONESRC_ALT_VCLK2,
    GK1XX_POWER_ONESRC_ALT_VCLK3,
    GK1XX_POWER_ONESRC_RT_MSDCLK,
    GK1XX_POWER_ONESRC_RT_UTILSCLK,
    GK1XX_POWER_ONESRC_RT_PWRCLK,
    GK1XX_POWER_ONESRC_RT_HUB2CLK,
    GK1XX_POWER_ONESRC_RT_SYS2CLK,
    GK1XX_POWER_ONESRC_RT_DISPCLK,
    GK1XX_POWER_ONESRC_RT_AZA2XCLK,
    GK1XX_POWER_ONESRC_RT_MAUDCLK,
    GK1XX_POWER_ONESRC_RT_DBGPORTCLK
};

//make sure gf1xx_lw_clock_pll_name[] is up-to-date
enum GK1XX_POWER_PLL {
    GK1XX_POWER_PLL_SPPLL0,
    GK1XX_POWER_PLL_SPPLL1,
    GK1XX_POWER_PLL_GPCPLL,
    GK1XX_POWER_PLL_LTCPLL,
    GK1XX_POWER_PLL_XBARPLL,
    GK1XX_POWER_PLL_SYSPLL,
    GK1XX_POWER_PLL_VPLL0,
    GK1XX_POWER_PLL_VPLL1,
    GK1XX_POWER_PLL_VPLL2,
    GK1XX_POWER_PLL_VPLL3,
    GK1XX_POWER_PLL_REFMPLL0,
    GK1XX_POWER_PLL_REFMPLL1,
    GK1XX_POWER_PLL_REFMPLL2,
    GK1XX_POWER_PLL_DRAMPLL00,
    GK1XX_POWER_PLL_DRAMPLL01,
    GK1XX_POWER_PLL_DRAMPLL02,
    GK1XX_POWER_PLL_DRAMPLL03,
    GK1XX_POWER_PLL_DRAMPLL10,
    GK1XX_POWER_PLL_DRAMPLL11,
    GK1XX_POWER_PLL_DRAMPLL12,
    GK1XX_POWER_PLL_DRAMPLL13,
    GK1XX_POWER_PLL_DRAMPLL20,
    GK1XX_POWER_PLL_DRAMPLL21,
    GK1XX_POWER_PLL_DRAMPLL22,
    GK1XX_POWER_PLL_DRAMPLL23,
    GK1XX_POWER_PLL_DRAMPLL30,
    GK1XX_POWER_PLL_DRAMPLL31,
    GK1XX_POWER_PLL_DRAMPLL32,
    GK1XX_POWER_PLL_DRAMPLL33,
    GK1XX_POWER_PLL_DRAMPLL40,
    GK1XX_POWER_PLL_DRAMPLL41,
    GK1XX_POWER_PLL_DRAMPLL42,
    GK1XX_POWER_PLL_DRAMPLL43,
    GK1XX_POWER_PLL_DRAMPLL50,
    GK1XX_POWER_PLL_DRAMPLL51,
    GK1XX_POWER_PLL_DRAMPLL52,
    GK1XX_POWER_PLL_DRAMPLL53,

    GK1XX_POWER_PLL_DRAMDLL0,
    GK1XX_POWER_PLL_DRAMDLL1,
    GK1XX_POWER_PLL_DRAMDLL2,
   // GK1XX_POWER_PLL_DRAMDLL3
};

enum KEPLER_POWER_CLKSRC {
    //    "null",
    KEPLER_POWER_CLKSRC_UNKNOWN = 0,
    KEPLER_POWER_CLKSRC_XTAL,
    KEPLER_POWER_CLKSRC_XTAL_FAST,
    KEPLER_POWER_CLKSRC_XTAL4X,
    KEPLER_POWER_CLKSRC_XTAL8X,
    KEPLER_POWER_CLKSRC_XTAL16X,

    KEPLER_POWER_CLKSRC_TCLK,
    KEPLER_POWER_CLKSRC_SPPLL0,
    KEPLER_POWER_CLKSRC_SPPLL1,
    KEPLER_POWER_CLKSRC_GPCCLK,
    KEPLER_POWER_CLKSRC_GPC2CLK,

    KEPLER_POWER_CLKSRC_GPCCLK_DIV2,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP0,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP1,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP2,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP3,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP4,
    KEPLER_POWER_CLKSRC_LTCCLK_FBP5,
    KEPLER_POWER_CLKSRC_LTC2CLK,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP0,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP1,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP2,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP3,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP4,
    KEPLER_POWER_CLKSRC_LTC2CLK_FBP5,
    KEPLER_POWER_CLKSRC_XBARCLK,
    KEPLER_POWER_CLKSRC_XBAR2CLK,

    KEPLER_POWER_CLKSRC_SYSPLL,
    KEPLER_POWER_CLKSRC_SYSCLK,
    KEPLER_POWER_CLKSRC_SYS2CLK,
    KEPLER_POWER_CLKSRC_HUBCLK,
    KEPLER_POWER_CLKSRC_HUB2CLK,
    KEPLER_POWER_CLKSRC_MSDCLK,

    KEPLER_POWER_CLKSRC_PWRCLK,
    KEPLER_POWER_CLKSRC_UTILSCLK,
    KEPLER_POWER_CLKSRC_PEXCLK,
    KEPLER_POWER_CLKSRC_DISPCLK,
    KEPLER_POWER_CLKSRC_DISPCLK_HEAD,
    KEPLER_POWER_CLKSRC_SPDIFCLK,

    KEPLER_POWER_CLKSRC_AZACLK,
    KEPLER_POWER_CLKSRC_AZA2CLK,
    KEPLER_POWER_CLKSRC_MAUDCLK,
    KEPLER_POWER_CLKSRC_DBGPORTCLK,
    KEPLER_POWER_CLKSRC_VPLL0,

    KEPLER_POWER_CLKSRC_VCLK0,
    KEPLER_POWER_CLKSRC_VPLL1,
    KEPLER_POWER_CLKSRC_VCLK1,
    KEPLER_POWER_CLKSRC_VPLL2,
    KEPLER_POWER_CLKSRC_VCLK2,

    KEPLER_POWER_CLKSRC_VPLL3,
    KEPLER_POWER_CLKSRC_VCLK3,
    KEPLER_POWER_CLKSRC_REFMPLL_REFCLK,
    KEPLER_POWER_CLKSRC_REFMPLL0,
    KEPLER_POWER_CLKSRC_REFMPLL1,
    KEPLER_POWER_CLKSRC_REFMPLL2,

    KEPLER_POWER_CLKSRC_ALT_DRAMCLK,
    KEPLER_POWER_CLKSRC_DRAMPLL0,
    KEPLER_POWER_CLKSRC_DRAMPLL1,
    KEPLER_POWER_CLKSRC_DRAMPLL2,
    KEPLER_POWER_CLKSRC_DRAMPLL3,
    KEPLER_POWER_CLKSRC_DRAMPLL4,
    KEPLER_POWER_CLKSRC_DRAMPLL5,

    KEPLER_POWER_CLKSRC_DRAM0DIV2,
    KEPLER_POWER_CLKSRC_DRAM0DIV4,
    KEPLER_POWER_CLKSRC_DRAM0DIV8,
    KEPLER_POWER_CLKSRC_DRAM1DIV2,
    KEPLER_POWER_CLKSRC_DRAM1DIV4,
    KEPLER_POWER_CLKSRC_DRAM1DIV8,
    KEPLER_POWER_CLKSRC_DRAM2DIV2,
    KEPLER_POWER_CLKSRC_DRAM2DIV4,
    KEPLER_POWER_CLKSRC_DRAM2DIV8,
    KEPLER_POWER_CLKSRC_DRAM3DIV2,
    KEPLER_POWER_CLKSRC_DRAM3DIV4,
    KEPLER_POWER_CLKSRC_DRAM3DIV8,
    KEPLER_POWER_CLKSRC_DRAM4DIV2,
    KEPLER_POWER_CLKSRC_DRAM4DIV4,
    KEPLER_POWER_CLKSRC_DRAM4DIV8,
    KEPLER_POWER_CLKSRC_DRAM5DIV2,
    KEPLER_POWER_CLKSRC_DRAM5DIV4,
    KEPLER_POWER_CLKSRC_DRAM5DIV8,
    KEPLER_POWER_CLKSRC_DRAMPLL_DLL0,
    KEPLER_POWER_CLKSRC_DRAMPLL_DLL1,
    KEPLER_POWER_CLKSRC_DRAMPLL_DLL2,

    KEPLER_POWER_CLKSRC_HOSTCLK,
    KEPLER_POWER_CLKSRC_HOSTCLK_ALT,
    KEPLER_POWER_CLKSRC_XCLK,
    KEPLER_POWER_CLKSRC_XCLK_ALT,
    KEPLER_POWER_CLKSRC_XTXCLK,
    KEPLER_POWER_CLKSRC_NUM,    //"KEPLER_POWER_CLKSRC_NUM" record the items' number of this enum type, please insert item only before this line
};

struct kepler_power_clock_struct{
    string map_name;
    string map_name_clk_count;
    KEPLER_POWER_CLKSRC clksrc;
    kepler_power_clock_struct(string s1, string s2, KEPLER_POWER_CLKSRC clk)
    {
       map_name = s1;
       map_name_clk_count = s2;
       clksrc = clk;
    }
};

class PowerCommonKepler
{
public:

    PowerCommonKepler(LWGpuResource *lwgpu);
    virtual ~PowerCommonKepler();

    virtual SOCV_RC DisableEngineGating(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC PtrimSet(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual int ReadTrimmerVal(string idx_reg, string data_reg, UINT32 idx)=0;
    virtual SOCV_RC WriteTrimmerVal(string idx_reg, string data_reg, UINT32 idx, UINT32 value,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetLinearDivider(GK1XX_POWER_CLOCK clk, GK1XX_POWER_CLOCK clksrc,int p,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllCoeff(GK1XX_POWER_PLL clkpll,int m, int n, int p,FLOAT64 freq_in,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetOneSrc(GK1XX_POWER_CLOCK clksrc,int p, GK1XX_POWER_ONESRC onesrc,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllBypass(GK1XX_POWER_PLL clkpll,bool bypass,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllEnable(GK1XX_POWER_PLL clkpll, bool enable,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllIddq(GK1XX_POWER_PLL clkpll, bool iddq,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetDiv4Mode(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetHostClkAutoMode(bool enable,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllSyncMode(GK1XX_POWER_PLL clkpll, bool sync_mode,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetUpClkMonitor()=0;
    virtual SOCV_RC SetClkPeriod(KEPLER_POWER_CLKSRC Gk1xxClk,FLOAT64 mhz);
    FLOAT64 MhzToPicoSeconds(FLOAT64 mhz);
    virtual SOCV_RC CheckPllLocked(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckPllNoDramLocked(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckRefmpllLocked(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckDrampllLocked(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckRefmpllLocked(UINT32 fs_pattern, FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckDrampllLocked(UINT32 fs_pattern, FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckPllReady(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckSYSClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckGPC0ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckGPC1ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckGPC2ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckGPC3ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckGPC4ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP0ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP1ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP2ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP3ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP4ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckFBP5ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckXBARClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC CheckTOPClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;

    virtual SOCV_RC InitPlls(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC InitPlls_PowerTest(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC InitOneSrcs(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC InitOneSrcs_PowerTest(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC InitClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode)=0;

    //these 2 functions are reloaded for drampll test
    virtual SOCV_RC SetPllCoeff(GK1XX_POWER_PLL clkpll,int m, int n, int p, FLOAT64 freq_in,
            SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetPllEnable(GK1XX_POWER_PLL clkpll, bool enable,
            SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    //InitPllsNoDram is added for initial other plls exclude refmpll&drampll, for drampll test only
    virtual SOCV_RC InitPllsNoDram(FULLCHIP_TEST_RUN_MODE run_mode)=0;

    virtual SOCV_RC SetRefmpllCoeff(int fs_pattern, int m, int n, int p, FLOAT64 freq_in,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetDrampllCoeff(int fs_pattern,int m, int n, int p, FLOAT64 freq_in,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetRefmpllEnable(int fs_pattern,bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetDrampllEnable(int fs_pattern,bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetDramdllEnable(int fs_pattern,bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetRefmpllBypass(int fs_pattern,bool bypass, FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC SetRefmpllSyncMode(int fs_pattern,bool flag, FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual SOCV_RC MclkSwitchToOnesrc(FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual FLOAT64 MclkSwitchToDrampll(FLOAT64 freq_in,int m_refm,int n_refm,int p_refm,int m_dram,int n_dram,int p_dram,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual FLOAT64 MclkSwitchToRefmpll(FLOAT64 freq_in,int m_refm,int n_refm,int p_refm,FULLCHIP_TEST_RUN_MODE run_mode)=0;
    virtual FLOAT64 PllOutFreq(FLOAT64 freq_in,int m, int n, int p);
    virtual FLOAT64 Liner_divider_calc(FLOAT64 freq_in,int p);
    virtual SOCV_RC GenModeSwitch(int gen_speed)=0;

protected:
    size_t m_NumMappedClocks;
    UINT32 m_Chip;
    LWGpuResource* m_lwgpu;
    GpuSubdevice *pSubdev;
    IRegisterMap* m_regMap;
    FLOAT64 clk_periods[KEPLER_POWER_CLKSRC_NUM];
    RandomStream* RndStream;
    SOCV_RC m_RCTemp;
    vector<const char*> kepler_power_clksrc_name;
    vector<const char*> gk1xx_power_clock_name;
    vector<const char*> gk1xx_power_onesrc_name;
    vector<const char*> gk1xx_power_pll_name;
};

#define POWER_COMMON_KEPLER_FOR_CHIP(chip) \
class PowerCommonKepler_##chip : public PowerCommonKepler \
{ \
public: \
        PowerCommonKepler_##chip(LWGpuResource *lwgpu); \
        virtual ~PowerCommonKepler_##chip(); \
        virtual SOCV_RC DisableEngineGating(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC PtrimSet(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual int ReadTrimmerVal(string idx_reg, string data_reg, UINT32 idx); \
        virtual SOCV_RC WriteTrimmerVal(string idx_reg, string data_reg, UINT32 idx, UINT32 value,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetLinearDivider(GK1XX_POWER_CLOCK clk, GK1XX_POWER_CLOCK clksrc,int p,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllCoeff(GK1XX_POWER_PLL clkpll,int m, int n, int p,FLOAT64 freq_in,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetOneSrc(GK1XX_POWER_CLOCK clksrc,int p, GK1XX_POWER_ONESRC onesrc,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllBypass(GK1XX_POWER_PLL clkpll,bool bypass,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllEnable(GK1XX_POWER_PLL clkpll, bool enable,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllIddq(GK1XX_POWER_PLL clkpll, bool iddq,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetDiv4Mode(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetHostClkAutoMode(bool enable,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllSyncMode(GK1XX_POWER_PLL clkpll, bool sync_mode,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetUpClkMonitor(); \
        virtual SOCV_RC CheckPllLocked(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckPllNoDramLocked(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckRefmpllLocked(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckDrampllLocked(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckRefmpllLocked(UINT32 fs_pattern, FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckDrampllLocked(UINT32 fs_pattern, FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckPllReady(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckSYSClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckGPC0ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckGPC1ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckGPC2ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckGPC3ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckGPC4ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP0ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP1ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP2ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP3ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP4ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckFBP5ClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckXBARClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC CheckTOPClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitPlls(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitPlls_PowerTest(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitOneSrcs(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitOneSrcs_PowerTest(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitClkPeriod(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllCoeff(GK1XX_POWER_PLL clkpll,int m, int n, int p,FLOAT64 freq_in,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetPllEnable(GK1XX_POWER_PLL clkpll, bool enable,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC InitPllsNoDram(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetRefmpllCoeff(int fs_pattern, int m, int n, int p, FLOAT64 freq_in,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetDrampllCoeff(int fs_pattern, int m, int n, int p, FLOAT64 freq_in,SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetRefmpllEnable(int fs_pattern, bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetDrampllEnable(int fs_pattern, bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetDramdllEnable(int fs_pattern, bool enable, SOCV_PLL_CAST_TYPE cast_type, SOCV_PLL_SPACE_TYPE space_type,FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetRefmpllBypass(int fs_pattern, bool bypass, FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC SetRefmpllSyncMode(int fs_pattern, bool flag, FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual SOCV_RC MclkSwitchToOnesrc(FULLCHIP_TEST_RUN_MODE run_mode); \
        virtual FLOAT64 MclkSwitchToDrampll(FLOAT64 freq_in,int m_refm,int n_refm,int p_refm,int m_dram,int n_dram,int p_dram,FULLCHIP_TEST_RUN_MODE run_mode);  \
        virtual FLOAT64 MclkSwitchToRefmpll(FLOAT64 freq_in,int m_refm,int n_refm,int p_refm,FULLCHIP_TEST_RUN_MODE run_mode);\
        virtual SOCV_RC GenModeSwitch(int gen_speed); \
};
POWER_COMMON_KEPLER_FOR_CHIP(GK104);
POWER_COMMON_KEPLER_FOR_CHIP(GK107);
POWER_COMMON_KEPLER_FOR_CHIP(GK110);

#define CHECK_POWER_TEST_RC(pll_rc,run_mode) \
    { \
        m_RCTemp = pll_rc; \
        rc = (SOCV_RC)(rc | m_RCTemp); \
        if(SOCV_OK != m_RCTemp) \
        { \
            ErrPrintf("Debug Info:Function:%s :Line %d\n",__FUNCTION__,__LINE__); \
        } \
        if((run_mode) == FAST_MODE) \
            return rc; \
    };

#endif  // _POWER_COMMON_KEPLER_H_

