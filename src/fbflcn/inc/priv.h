/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include "fbflcn_defines.h"
#include "config/fbfalcon-config.h"
#include "cpu-intrinsics.h"
#include "csb.h"


#ifndef PRIV_H_
#define PRIV_H_

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_HW_BAR0_MASTER))
LW_STATUS _fbflcn2Bar0WaitIdle(LwBool bEnablePrivErrChk);
void _fbflcn2BarRegWr32NonPosted(LwU32 addr, LwU32 data);
void _fbflcn2BarRegWr32Posted(LwU32 addr, LwU32 data);
LwU32 _fbflcn2BarRegRd32(LwU32 addr);
#else
LW_STATUS fbflcn2PrivErrorChkRegWr32NonPosted(LwU32  addr, LwU32  data);
LwU32 fbflcn2PrivErrorChkRegRd32(LwU32  addr);
#endif


#define falc_lwst_ssetb(s)  asm volatile ("ssetb %0      ;"::"n"(s))

//-----------------------------------------------------------------------------
// BAR0 system wide calls outside of fbfalcon/minion and fbpa - require checks
// CSB  registers inside falcon/minion, no need for additional checks - csb.h
//      these calls use _i (immediate) instructions (e.g. stx_i)
// LOG  same as CSB with calls to profiling functions if
//      FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING)
//-----------------------------------------------------------------------------

#define CSB_LWTOFF    0x700

// the bar0 master will not work with CSB address, use a compile time assert to check that the address is valid
#define REJECT_CSB_ADR(_addr)  CASSERT(((_addr) >= CSB_LWTOFF), priv_h_reject_bar0_access_with_csb_address)

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_HW_BAR0_MASTER))
#define BAR0_REG_RD32(_addr)                ({ REJECT_CSB_ADR((_addr)); _fbflcn2BarRegRd32((_addr)); })
#define BAR0_REG_WR32(_addr, _val)          ({ REJECT_CSB_ADR((_addr)); _fbflcn2BarRegWr32Posted((_addr),(_val)); })
#define BAR0_REG_WR32_STALL(_addr, _val)    ({ REJECT_CSB_ADR((_addr)); _fbflcn2BarRegWr32NonPosted((_addr), (_val)); })
#else
#define BAR0_REG_RD32(_addr)                fbflcn2PrivErrorChkRegRd32((_addr))
#define BAR0_REG_WR32(_addr, _val)          CSB_REG_WR32((_addr),(_val))
#define BAR0_REG_WR32_STALL(_addr, _val)    \
    fbflcn2PrivErrorChkRegWr32NonPosted((_addr), (_val))
#endif


/*
 * Priv Profiling tool,  will record priv accesses to a dedicated area in dmem.
 */
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))

#define PRIV_BUFFER_SIZE 0x600
#define PRIV_PROFILING_MAILBOX_PP_START_ADR 11
#define PRIV_PROFILING_MAILBOX_PP_SIZE 12
#define PRIV_PROFILING_MAILBOX_PP_ENTRIES 13

void privprofilingPreRd(LwU32 adr, LwU8 blocking);
void privprofilingPreWr(LwU32 adr, LwU8 blocking);
void privprofilingPost(LwU32 data);
void privreadcheck(LwU32 adr, LwU32 data, LwU32 idx);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_SW_BAR0_MASTER_ENFORCED))
// use do/while for multi-line macros
#define LOG_REG_RD32(_addr)         \
({  \
    privprofilingPreRd((_addr),0);      \
    LwU32 data = BAR0_REG_RD32((_addr)); \
    privprofilingPost(data);            \
    data;                               \
})

#define LOG_REG_WR32(_addr, _val)       \
do {  \
    privprofilingPreWr((_addr), 0);         \
    BAR0_REG_WR32((_addr),(_val));           \
    privprofilingPost((_val));              \
} while (0)

#define LOG_REG_WR32_STALL(_addr, _val) \
do { \
    privprofilingPreWr((_addr), 1);         \
    BAR0_REG_WR32_STALL((_addr),(_val));     \
    privprofilingPost((_val));              \
} while (0)

#else

// use do/while for multi-line macros
#define LOG_REG_RD32(_addr)         \
({  \
    privprofilingPreRd((_addr),0);      \
    LwU32 data = CSB_REG_RD32((_addr)); \
    privprofilingPost(data);            \
    data;                               \
})

#define LOG_REG_WR32(_addr, _val)       \
do {  \
    privprofilingPreWr((_addr), 0);         \
    CSB_REG_WR32((_addr),(_val));           \
    privprofilingPost((_val));              \
} while (0)

#define LOG_REG_WR32_STALL(_addr, _val) \
do { \
    privprofilingPreWr((_addr), 1);         \
    CSB_REG_WR32_STALL((_addr),(_val));     \
    privprofilingPost((_val));              \
} while (0)

#endif // !(FBFALCONCFG_FEATURE_ENABLED(FBFALCON_SW_BAR0_MASTER_ENFORCED))

#else  // !FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING)

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_SW_BAR0_MASTER_ENFORCED))
#define LOG_REG_RD32(_addr)                 BAR0_REG_RD32((_addr))
#define LOG_REG_WR32(_addr, _val)           BAR0_REG_WR32((_addr),(_val))
#define LOG_REG_WR32_STALL(_addr, _val)     BAR0_REG_WR32_STALL((_addr),(_val))
#else
#define LOG_REG_RD32(_addr)                 CSB_REG_RD32((_addr))
#define LOG_REG_WR32(_addr, _val)           CSB_REG_WR32((_addr),(_val))
#define LOG_REG_WR32_STALL(_addr, _val)     CSB_REG_WR32_STALL((_addr),(_val))
#endif

#endif // !FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING)


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))

void privprofilingReset(void);
void privprofilingClean(void);
void privprofilingEnable(void);
void privprofilingDisable(void);

void privprofilingPreWr(LwU32 adr, LwU8 blocking);
void privprofilingPreRd(LwU32 adr, LwU8 blocking);
void privprofilingPost(LwU32 data);


#endif // PRIV_PROFILING

//-----------------------------------------------------------------------------
// LW_CFBFALCON_FIRMWARE_MAILBOX write read functions
//   uses _bus calls (typically CSB or LOG)
//-----------------------------------------------------------------------------
#define FW_MBOX_WR32(_addr,_val)           CSB_REG_WR32(LW_CFBFALCON_FIRMWARE_MAILBOX((_addr)),(_val))
#define FW_MBOX_WR32_STALL(_addr,_val)     CSB_REG_WR32_STALL(LW_CFBFALCON_FIRMWARE_MAILBOX((_addr)),(_val))

#endif /* PRIV_H_ */
