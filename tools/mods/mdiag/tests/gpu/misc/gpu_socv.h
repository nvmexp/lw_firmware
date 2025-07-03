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

#ifndef _GPU_SOCV_H_
#define _GPU_SOCV_H_

enum SOCV_ENGINE_TYPE {
    ENGINE_TYPE_GR_ENG,
    ENGINE_TYPE_MSPDEC_ENG,
    ENGINE_TYPE_MSPPP_ENG,
    ENGINE_TYPE_MSVLD_ENG,
    ENGINE_TYPE_HOST_ENG,
    ENGINE_TYPE_LTC_ENG,
    ENGINE_TYPE_FBH_ISO_ENG,
    ENGINE_TYPE_FBH_NISO_ENG,
    ENGINE_TYPE_FBPA_ENG,
    ENGINE_TYPE_XVE_ENG,
    ENGINE_TYPE_XBAR_ENG,
};

enum SOCV_ENGINE_MODE {
    SOCV_ENGINE_CG_FULLPOWER, // No engine-level clock gating
    SOCV_ENGINE_CG_AUTOMATIC, // Engine & block-level clock gate when no activity
    SOCV_ENGINE_CG_DISABLED,  // Gate engine unconditionally
};

//TODO:the following 4 enums have been defined in pll_fermi_common.h, include "pll_fermi_common.h" to avoid redefinition. Better to define new type for kepler.
enum SOCV_PLL_POWER_MODE {
    SOCV_PLL_POWER_ON,
    SOCV_PLL_POWER_OFF,
};

enum SOCV_PLL_FREQ_MODE {
    SOCV_PLL_FREQ_NORMINAL,
    SOCV_PLL_FREQ_RANDOM,
    SOCV_PLL_FREQ_RANDOM_LT_1G,
};

enum SOCV_PLL_CAST_TYPE {
    SOCV_PLL_UNICAST,
    SOCV_PLL_BCAST,
};

enum SOCV_PLL_SPACE_TYPE {
    SOCV_PTRIM_SPACE,
    SOCV_FBPA_SPACE,
};

enum FULLCHIP_TEST_RUN_MODE {
    FAST_MODE,
    REGRESS_MODE,
};

enum SOCV_RC {
    SOCV_OK = 0,
    SETREG_ERROR,           // 1
    GETREG_ERROR,           // 2
    REG_NOT_DEFINED,        // 3
    FIELD_NOT_DEFINED,      // 4
    CHECK_CLKPERIOD_ERR,    // 5
    SOFTWARE_ERROR,         // 6
    TEST_SANITY_ERROR,      // 7
    SOCV_TEST_FAILED,       // 8
    SOCV_VALUE_NOT_MATCH,   // 9
    SRC_FOUND_ERROR,        // 10
    //TODO
};
#endif
