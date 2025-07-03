/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdarg.h>
#include "dmvautils.h"
#include "dmvacommon.h"
#include "dmvadma.h"
#include "dmvainstrs.h"
#include "dmvaregs.h"
#include <lwmisc.h>

LwU32 align(LwU32 addr, LwU32 alignment)
{
    return addr - (addr % alignment);
}

LwBool aligned(LwU32 addr, LwU32 alignment)
{
    return addr == align(addr, alignment);
}

LwU32 alignUp(LwU32 addr, LwU32 alignment)
{
    if (aligned(addr, alignment))
    {
        return addr;
    }
    else
    {
        return align(addr + alignment, alignment);
    }
}

LwBool isPow2(LwU32 x)
{
    return !(x & (x - 1));
}

LwU32 log_2(LwU32 x)
{
    LwU32 log2 = 0;
    while (x >>= 1)
    {
        log2++;
    }
    return log2;
}

LwU32 bitsToEncode(LwU32 x)
{
    return log_2(x) + (isPow2(x) ? 0 : 1);
}

void wait(void)
{
    falc_ssetb_i(0);  // set CSW F0 flags
    falc_wait(0);     // suspend exelwtion until next interrupt
}

void halt(void)
{
    // don't send an interrupt to RM when we halt
    DMVAREGWR(IRQMCLR, FLD_SET_DRF(_PFALCON, _FALCON_IRQMCLR, _HALT, _SET, DMVAREGRD(IRQMCLR)));
    while (LW_TRUE)  // colwenience GCC that this function actually doesn't return
    {
        falc_halt();
    }
}

void rmBarrier(void)
{
    wait();
}

// these functions syncronize to rm before a read/write
LwU32 rmRead32(void)
{
    wait();
    return DMVAREGRD(MAILBOX1);
}

void rmWrite32(LwU32 data)
{
    DMVAREGWR(MAILBOX1, data);
    wait();
}

LwU32 rmRequestRead(LwU32 addr)
{
    DMVAREGWR(MAILBOX0, DMVA_REQUEST_PRIV_RW_FROM_RM);
    wait();
    rmWrite32(addr);
    rmWrite32(LW_TRUE);
    return rmRead32();
}

void rmRequestWrite(LwU32 addr, LwU32 data)
{
    DMVAREGWR(MAILBOX0, DMVA_REQUEST_PRIV_RW_FROM_RM);
    wait();
    rmWrite32(addr);
    rmWrite32(LW_FALSE);
    rmWrite32(data);
    rmBarrier();
}

void setStackOverflowProtection(LwU32 bound, LwBool bEnable)
{
    LwU32 stackCfg = FLD_SET_DRF_NUM(_PFALCON, _FALCON_STACKCFG, _BOTTOM, bound / sizeof(LwU32), 0);
    stackCfg = FLD_SET_DRF_NUM(_PFALCON, _FALCON_STACKCFG, _SPEXC, bEnable ? 1 : 0, stackCfg);
    DMVAREGWR(STACKCFG, stackCfg);
}

void programDmcya(void) {
    // enable 24 bit SP
    // enable precise exceptions
    DMVAREGWR(DMCYA, 0x8);
}

void enableDMEMaperture(void)
{
#if defined(DMVA_FALCON_SEC2) || defined(DMVA_FALCON_GSP)
    LwU32 dmemApert = DMVACSBRD(LW_CSEC_FALCON_DMEMAPERT);
    dmemApert = FLD_SET_DRF(_CSEC, _FALCON_DMEMAPERT, _ENABLE, _TRUE, dmemApert);
    DMVACSBWR(LW_CSEC_FALCON_DMEMAPERT, dmemApert);
#elif defined DMVA_FALCON_PMU
    LwU32 dmemApert = DMVACSBRD(LW_CPWR_FALCON_DMEMAPERT);
    dmemApert = FLD_SET_DRF(_CPWR, _FALCON_DMEMAPERT, _ENABLE, _TRUE, dmemApert);
    DMVACSBWR(LW_CPWR_FALCON_DMEMAPERT, dmemApert);
#endif
    // no dmem aperture on lwdec
}

void setupCtxDma(void)
{
    LwU32 fbifctl = DMVACSBRD(CAT(CAT(LW_C, PREFIX), _FBIF_CTL));
    fbifctl = FLD_SET_DRF(_CSEC, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, fbifctl);
    DMVACSBWR(CAT(CAT(LW_C, PREFIX), _FBIF_CTL), fbifctl);

    LwU32 transCfg = DMVACSBRD(CAT(CAT(LW_C, PREFIX), _FBIF_TRANSCFG(0)));
    transCfg = FLD_SET_DRF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, transCfg);
    transCfg = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, transCfg);
    DMVACSBWR(CAT(CAT(LW_C, PREFIX), _FBIF_TRANSCFG(0)), transCfg);
}

void stxAtUcodeLevel(LwU32 *addr, LwU32 data, SecMode ucodeLevel)
{
    UASSERT(ucodeLevel < N_SEC_MODES);
    if (ucodeLevel == SEC_MODE_HS)
    {
        callHS(HS_CSB_WR, addr, data);
    }
    else
    {
        LwU32 oldLevel = getUcodeLevel();
        callHS(HS_SET_LVL, ucodeLevel);
        DMVACSBWR(addr, data);
        callHS(HS_SET_LVL, oldLevel);
    }
}

LwU32 ldxAtUcodeLevel(LwU32 *addr, SecMode ucodeLevel)
{
    UASSERT(ucodeLevel < N_SEC_MODES);
    if (ucodeLevel == SEC_MODE_HS)
    {
        return callHS(HS_CSB_RD, addr);
    }
    else
    {
        LwU32 oldLevel = getUcodeLevel();
        callHS(HS_SET_LVL, ucodeLevel);
        LwU32 ret = DMVACSBRD(addr);
        callHS(HS_SET_LVL, oldLevel);
        return ret;
    }
}

// TODO: add test/subtest counter
// TODO: add macro for rmtest to access registers
// TODO: add dmvactl for loop around tests
// TODO: add constants in place of magic numbers
// TODO: find out if there's a way to remove mark_forward_progress
// TODO: there should be no falc_[dmva] instructions
// TODO: random IDs for each test?
