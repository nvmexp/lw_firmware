/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _MAXWELL_RTL_CLOCKS_
#define _MAXWELL_RTL_CLOCKS_

#include "mdiag/lwgpu.h"
#include "pwr_macro.h"

#define NOT_IN_TPC -1
#define NOT_IN_FB_PART -1
#define BLCG 0
#define ELCG 1

enum MAXWELL_CLOCK_SOURCE {
    MAXWELL_CLOCK_SOURCE_UNKNOWN = 0,
    MAXWELL_CLOCK_SOURCE_SYSCLK,
    MAXWELL_CLOCK_SOURCE_GPCCLK,
    MAXWELL_CLOCK_SOURCE_LWDCLK,
    MAXWELL_CLOCK_SOURCE_NUM,    //"MAXWELL_CLOCK_SOURCE_NUM" record the items' number of this enum type, please insert item only before this line
};

typedef struct {
    // new define for maxwell
unsigned long long int    MAXWELL_CLOCK_NOT_GATED           = 0;
unsigned long long int    MAXWELL_CLOCK_GR_ENGINE_GATED     = 0;
unsigned long long int    MAXWELL_CLOCK_CE0_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_CE1_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_CE2_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_CE3_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC_ENGINE_GATED  = 0;
unsigned long long int    MAXWELL_CLOCK_LWENC_ENGINE_GATED  = 0;
unsigned long long int    MAXWELL_CLOCK_OFA_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC2_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWENC2_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG0_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG1_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC1_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_SEC_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_LWENC1_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC3_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG2_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG3_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_CE4_ENGINE_GATED    = 0;       
unsigned long long int    MAXWELL_CLOCK_CE5_ENGINE_GATED    = 0; 
unsigned long long int    MAXWELL_CLOCK_CE6_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_CE7_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC4_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG4_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC5_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG5_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_CE8_ENGINE_GATED    = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC6_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG6_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWDEC7_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_LWJPG7_ENGINE_GATED = 0;
unsigned long long int    MAXWELL_CLOCK_CE9_ENGINE_GATED    = 0;
} MAXWELL_CLOCK_GATING;

enum MAXWELL_FORCE_CLKS_ON {
};

enum MAXWELL_FORCE_FULL_POWER {
    MAXWELL_GR_FORCE_FULL_POWER          = (1 << 1),
    MAXWELL_LWDEC_FORCE_FULL_POWER       = (1 << 2),
    MAXWELL_LWENC_FORCE_FULL_POWER       = (1 << 3),
    MAXWELL_SEC_FORCE_FULL_POWER       = (1 << 4),
    MAXWELL_LWENC1_FORCE_FULL_POWER      = (1 << 5),
    MAXWELL_LWENC2_FORCE_FULL_POWER      = (1 << 6),
    MAXWELL_CE0_FORCE_FULL_POWER         = (1 << 7),
    MAXWELL_CE1_FORCE_FULL_POWER         = (1 << 8),
    MAXWELL_CE2_FORCE_FULL_POWER         = (1 << 9),
    MAXWELL_CE3_FORCE_FULL_POWER         = (1 << 10),
    MAXWELL_CE4_FORCE_FULL_POWER         = (1 << 11),
    MAXWELL_CE5_FORCE_FULL_POWER         = (1 << 12),
    MAXWELL_CE6_FORCE_FULL_POWER         = (1 << 13),
    MAXWELL_CE7_FORCE_FULL_POWER         = (1 << 14),
    MAXWELL_CE8_FORCE_FULL_POWER         = (1 << 15),
    MAXWELL_CE9_FORCE_FULL_POWER         = (1 << 16),
    MAXWELL_LWDEC1_FORCE_FULL_POWER         = (1 << 17),
    MAXWELL_LWDEC2_FORCE_FULL_POWER         = (1 << 18),
    MAXWELL_LWDEC3_FORCE_FULL_POWER         = (1 << 19),
    MAXWELL_LWDEC4_FORCE_FULL_POWER         = (1 << 20),
    MAXWELL_OFA_FORCE_FULL_POWER         = (1 << 21),
    MAXWELL_LWJPG_FORCE_FULL_POWER         = (1 << 22),
};

struct maxwell_rtl_clock_struct{
    const char* map_name_clk_count;
    MAXWELL_CLOCK_SOURCE clksrc;
    unsigned long long int gating_type;
    int force_full_power_type;
};

unsigned NumMappedClocks_maxwell();
maxwell_rtl_clock_struct GetRTLMappedClock_maxwell(unsigned i);
void InitMappedClocks_maxwell(Macro macros, MAXWELL_CLOCK_GATING maxwell_clock_gating);

void SetMaxwellClkGpuResource(LWGpuResource *pLwGpu);
#endif
