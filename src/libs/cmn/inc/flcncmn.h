/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  flcncmn.h
 * @brief Master header file defining common definations across all falcons.
 * It defines based on for which falocn code is being compiled
 */

#ifndef FLCNCMN_H
#define FLCNCMN_H

/* --------------------------- System includes ----------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwoslayer.h"

/* ------------------------- Macros and Defines ---------------------------- */
#define FLCN_ASSERT(cond)                           OS_ASSERT(cond)

#if defined(DPU_RTOS)
    /*
    * Note: include csb.h with <angle brackets> so that when we do a unit test
    * build, it includes the unit test version of csb.h. Using "" would cause
    * The csb.h in the same directory as this header to take precedence.
    */
    #include <csb.h>

    #define DISP_REG_RD32(addr)                     REG_RD32(CSB, addr)
    #define DISP_REG_RD32_ERR_CHK(addr, pVal)       { *pVal = REG_RD32(CSB, addr); }
    #define DISP_REG_WR32(addr, val)                REG_WR32(CSB, addr, val)
    // CHECK_FLCN_STATUS(FLCN_OK) is to solve ugly ErrorExit label defined but not used error for preVolta.
    #define DISP_REG_RD32_ERR_EXIT(addr, pVal)      { DISP_REG_RD32_ERR_CHK(addr, pVal); CHECK_FLCN_STATUS(FLCN_OK); }
    #define DISP_REG_WR32_ERR_EXIT(addr, val)       { DISP_REG_WR32(addr, val); CHECK_FLCN_STATUS(FLCN_OK); }

    #define PMGR_REG_RD32(addr)                     REG_RD32(CSB, addr)
    #define PMGR_REG_RD32_ERR_CHK(addr, pVal)       { *pVal = REG_RD32(CSB, addr); }
    #define PMGR_REG_WR32(addr, val)                REG_WR32(CSB, addr, val)

    #define PMGR_REG_RD32_ERR_EXIT(addr, pVal)      { PMGR_REG_RD32_ERR_CHK(addr, pVal); CHECK_FLCN_STATUS(FLCN_OK); }
    #define PMGR_REG_WR32_ERR_EXIT(addr, val)       { PMGR_REG_WR32(addr, val); CHECK_FLCN_STATUS(FLCN_OK); }

#elif (defined(GSPLITE_RTOS) && !defined(GSP_RTOS)) || defined(SOE_RTOS)
    // TODO: change to use HW reg access library interface directly.
    extern LwU32 libInterfaceRegRd32(LwU32 addr);
    extern FLCN_STATUS libInterfaceRegRd32ErrChk(LwU32 addr, LwU32* pVal);
    extern FLCN_STATUS libInterfaceRegRd32ErrChkHs(LwU32 addr, LwU32* pVal);
    extern FLCN_STATUS libInterfaceRegWr32ErrChk(LwU32 addr, LwU32 val);
    extern FLCN_STATUS libInterfaceRegWr32ErrChkHs(LwU32 addr, LwU32 val);
    extern FLCN_STATUS libInterfaceCsbRegRd32ErrChk(LwU32 addr, LwU32* pVal);
    extern FLCN_STATUS libInterfaceCsbRegWr32ErrChk(LwU32 addr, LwU32 val);
    extern FLCN_STATUS libInterfaceCsbRegRd32ErrChkHs(LwU32 addr, LwU32* pVal);
    extern FLCN_STATUS libInterfaceCsbRegWr32ErrChkHs(LwU32 addr, LwU32 val);

    #define DISP_REG_RD32_ERR_CHK(addr, pVal)       libInterfaceRegRd32ErrChk(addr, pVal)
    #define DISP_REG_RD32_ERR_CHK_HS(addr, pVal)    libInterfaceRegRd32ErrChkHs(addr, pVal)
    #define DISP_REG_WR32_ERR_CHK(addr, val)        libInterfaceRegWr32ErrChk(addr, val)
    #define DISP_REG_WR32_ERR_CHK_HS(addr, val)     libInterfaceRegWr32ErrChkHs(addr, val)

    #define PMGR_REG_RD32(addr)                     libInterfaceRegRd32(addr)
    #define PMGR_REG_RD32_ERR_CHK(addr, pVal)       libInterfaceRegRd32ErrChk(addr, pVal)
    #define PMGR_REG_WR32(addr, val)                libInterfaceRegWr32ErrChk(addr, val)

    #define PMGR_REG_RD32_ERR_EXIT(addr, pVal)      CHECK_FLCN_STATUS(PMGR_REG_RD32_ERR_CHK(addr, pVal));
    #define PMGR_REG_WR32_ERR_EXIT(addr, val)       CHECK_FLCN_STATUS(PMGR_REG_WR32(addr, val));
#elif defined(GSP_RTOS)
    //
    // GSP RISC-V (HS Falcon)
    // TODO - implement wrapper functions for CHEETAH, GPU and use simple define to different wrapper funcs.    
    //

#if USE_CBB
    // For CheetAh build.
    #include "dev_disp.h"
    #include "address_map_new.h"

    extern FLCN_STATUS dioReadReg(LwU32 address, LwU32* pData);
    extern FLCN_STATUS dioWriteReg(LwU32 address, LwU32 data);

    #define DISP_REG_RD32_ERR_CHK(addr, pVal)                       dioReadReg(((LW_ADDRESS_MAP_DISP_BASE) + addr - (DRF_BASE(LW_PDISP))), pVal)
    #define DISP_REG_WR32_ERR_CHK(addr, val)                        dioWriteReg(((LW_ADDRESS_MAP_DISP_BASE) + addr - (DRF_BASE(LW_PDISP))), val)
    static LW_FORCEINLINE LwU32 DISP_REG_RD32(LwU32 addr)           { LwU32 val; (void)dioReadReg(((LW_ADDRESS_MAP_DISP_BASE) + addr - (DRF_BASE(LW_PDISP))), &val); return val; }
    static LW_FORCEINLINE void DISP_REG_WR32(LwU32 addr, LwU32 val) { (void)dioWriteReg(((LW_ADDRESS_MAP_DISP_BASE) + addr - (DRF_BASE(LW_PDISP))), val); return; }

    #define DISP_REG_RD32_ERR_CHK_HS(addr, pVal)                    DISP_REG_RD32_ERR_CHK(addr, pVal)
    #define DISP_REG_WR32_ERR_CHK_HS(addr, val)                     DISP_REG_WR32_ERR_CHK(addr, val)
#else
    // BAR0, for GPU build.
    #include "gsp_bar0.h"

    static LW_FORCEINLINE FLCN_STATUS DISP_REG_RD32_ERR_CHK(LwU32 addr, LwU32 *pVal)    { *pVal = REG_RD32(BAR0, addr); return FLCN_OK; }
    static LW_FORCEINLINE FLCN_STATUS DISP_REG_WR32_ERR_CHK(LwU32 addr, LwU32 val)      { REG_WR32(BAR0, addr, val); return FLCN_OK; }

    #ifdef UPROC_RISCV
    static LW_FORCEINLINE FLCN_STATUS DISP_REG_RD32_ERR_CHK_HS(LwU32 addr, LwU32 *pVal)
    { 

        *pVal =  *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + addr);
        return FLCN_OK; 
    }
    static LW_FORCEINLINE FLCN_STATUS DISP_REG_WR32_ERR_CHK_HS(LwU32 addr, LwU32 val)
    { 
        *(volatile LwU32*)(LW_RISCV_AMAP_PRIV_START + addr) = val;
        return FLCN_OK; 
    }
    #else
    static LW_FORCEINLINE FLCN_STATUS DISP_REG_RD32_ERR_CHK_HS(LwU32 addr, LwU32 *pVal)
    { 
        // TODO: implement gspBar0RegRd32_TU10X for TU10X GSP RISC-V HS-Falcon.
        return FLCN_OK; 
    }
    static LW_FORCEINLINE FLCN_STATUS DISP_REG_WR32_ERR_CHK_HS(LwU32 addr, LwU32 val)
    { 
        // TODO: implement gspBar0RegWr32_TU10X for TU10X GSP RISC-V HS-Falcon.
        return FLCN_OK; 
    }
    #endif // UPROC_RISCV

    #define DISP_REG_RD32(addr)                     REG_RD32(BAR0, addr)
    #define DISP_REG_WR32(addr, val)                REG_WR32(BAR0, addr, val)

    #define PMGR_REG_RD32(addr)                     REG_RD32(BAR0, addr)
    #define PMGR_REG_RD32_ERR_CHK(addr, pVal)       DISP_REG_RD32_ERR_CHK(addr, pVal)
    #define PMGR_REG_WR32(addr, val)                REG_WR32(BAR0, addr, val)

    #define PMGR_REG_RD32_ERR_EXIT(addr, pVal)      { PMGR_REG_RD32_ERR_CHK(addr, pVal); CHECK_FLCN_STATUS(FLCN_OK); }
    #define PMGR_REG_WR32_ERR_EXIT(addr, val)       { PMGR_REG_WR32(addr, val); CHECK_FLCN_STATUS(FLCN_OK); }
#endif // USE_CBB

#endif

// TODO:DIO functions should be used from lwriscv/inc/engine.h
#ifdef HDCP_TEGRA_BUILD
extern FLCN_STATUS dioReadReg(LwU32 address, LwU32* pData);
extern FLCN_STATUS dioWriteReg(LwU32 address, LwU32 data);
#endif

#endif // FLCNCMN_H

