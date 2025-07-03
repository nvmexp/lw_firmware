/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_CSR_H
#define LWRISCV_CSR_H

#if (__riscv_xlen == 32)
#   include LWRISCV32_MANUAL_CSR
#else
#   include LWRISCV64_MANUAL_CSR
#   ifndef LW_RISCV_CSR_MARCHID_ARCH_RST
#       define LW_RISCV_CSR_MARCHID_ARCH_RST           ((LW_RISCV_CSR_MARCHID_MSB_RST         << 63 ) | \
                                                        (LW_RISCV_CSR_MARCHID_RS1_RST         << 16 ) | \
                                                        (LW_RISCV_CSR_MARCHID_CORE_MAJOR_RST  << 12 ) | \
                                                        (LW_RISCV_CSR_MARCHID_CORE_MINOR_RST  <<  8 ))
#       define LW_RISCV_CSR_MARCHID_ARCH_RST__SHIFTMASK 0xFFFFFFFFFFFFFF00ULL
#   else
#       define LW_RISCV_CSR_MARCHID_ARCH_RST__SHIFTMASK 0xFFFFFFFFFFFFFFFFULL
#   endif
#endif // (__riscv_xlen == 32)

// Table of prefixes:
//               LWRISCV_CONFIG_CPU_MODE
//        Macro  | 3 | 2 | 1 | 0 |
// LW_RISCV_CSRX | M |N/A| S |N/A|
// LW_RISCV_CSRU | M |N/A|   |N/A|

#ifndef LWRISCV_CONFIG_CPU_MODE
// If the kernel mode hasn't been defined, X versions default to M mode, and a warning is generated
# warning "LWRISCV_CONFIG_CPU_MODE undefined, LW_RISCV_CSR_X* macros defaulting to M mode"
# define LW_RISCV_CSRX1(_A)            LW_RISCV_CSR_M ## _A
# define LW_RISCV_CSRX2(_A, _B)        LW_RISCV_CSR_M ## _A ## M ## _B
# define LW_RISCV_CSRX3(_A, _B, _C)    LW_RISCV_CSR_M ## _A ## M ## _B ## M ## _C
# define LW_RISCV_CSRU1(_A)            LW_RISCV_CSR_M ## _A
# define LW_RISCV_CSRU2(_A, _B)        LW_RISCV_CSR_M ## _A ## M ## _B
# define LW_RISCV_CSRU3(_A, _B, _C)    LW_RISCV_CSR_M ## _A ## M ## _B ## M ## _C
#elif LWRISCV_CONFIG_CPU_MODE == 3
// Machine mode
# define LW_RISCV_CSRX1(_A)            LW_RISCV_CSR_M ## _A
# define LW_RISCV_CSRX2(_A, _B)        LW_RISCV_CSR_M ## _A ## M ## _B
# define LW_RISCV_CSRX3(_A, _B, _C)    LW_RISCV_CSR_M ## _A ## M ## _B ## M ## _C
# define LW_RISCV_CSRU1(_A)            LW_RISCV_CSR_M ## _A
# define LW_RISCV_CSRU2(_A, _B)        LW_RISCV_CSR_M ## _A ## M ## _B
# define LW_RISCV_CSRU3(_A, _B, _C)    LW_RISCV_CSR_M ## _A ## M ## _B ## M ## _C
#elif LWRISCV_CONFIG_CPU_MODE == 2
// Hypervisor mode
# error "Hypervisor mode unsupported"
#elif LWRISCV_CONFIG_CPU_MODE == 1
// Supervisor mode
# define LW_RISCV_CSRX1(_A)            LW_RISCV_CSR_S ## _A
# define LW_RISCV_CSRX2(_A, _B)        LW_RISCV_CSR_S ## _A ## S ## _B
# define LW_RISCV_CSRX3(_A, _B, _C)    LW_RISCV_CSR_S ## _A ## S ## _B ## S ## _C
# define LW_RISCV_CSRU1(_A)            LW_RISCV_CSR_ ## _A
# define LW_RISCV_CSRU2(_A, _B)        LW_RISCV_CSR_ ## _A ## _B
# define LW_RISCV_CSRU3(_A, _B, _C)    LW_RISCV_CSR_ ## _A ## _B ## _C
#elif LWRISCV_CONFIG_CPU_MODE == 0
// User mode
# error "User mode unsupported"
#endif

//
// On GA10X and these got renamed from UACCESSATTR to MACCESSATTR.
// I believe they are otherwise completely identical.
//
// TODO VLAJA - rename to if LW_RISCV_CSR_MARCH_RST == 0 once manuals change
#ifdef LW_RISCV_CSR_MFETCHATTR
# define LW_RISCV_CSRX_ACCESSATTR(_A) LW_RISCV_CSR_MLDSTATTR ## _A
# define LW_RISCV_CSR_XACCESSATTR LW_RISCV_CSR_MLDSTATTR
#else
# define LW_RISCV_CSRX_ACCESSATTR(_A) LW_RISCV_CSR_UACCESSATTR ## _A
# define LW_RISCV_CSR_XACCESSATTR LW_RISCV_CSR_UACCESSATTR
#endif

#define LW_RISCV_CSR_XACCESSATTR_WPR__SHIFT \
    LW_RISCV_CSRX_ACCESSATTR(_WPR__SHIFT)
#define LW_RISCV_CSR_XACCESSATTR_WPR__SHIFTMASK \
    LW_RISCV_CSRX_ACCESSATTR(_WPR__SHIFTMASK)
#define LW_RISCV_CSR_XACCESSATTR_COHERENT__SHIFTMASK \
    LW_RISCV_CSRX_ACCESSATTR(_COHERENT__SHIFTMASK)
#define LW_RISCV_CSR_XACCESSATTR_CACHEABLE__SHIFTMASK \
    LW_RISCV_CSRX_ACCESSATTR(_CACHEABLE__SHIFTMASK)

//
// Unified CSRs from manuals
//
// We provide X letter which will automatically expand to M/S
//
#define LW_RISCV_CSR_XMPU_PAGE_SIZE             LW_RISCV_CSR_MPU_PAGE_SIZE

#define LW_RISCV_CSR_XCAUSE2                    LW_RISCV_CSRX1(CAUSE2)
#define LW_RISCV_CSR_XTVEC                      LW_RISCV_CSRX1(TVEC)

#define LW_RISCV_CSR_XCOUNTEREN                 LW_RISCV_CSRX1(COUNTEREN)
#define LW_RISCV_CSR_XCOUNTEREN_TM              LW_RISCV_CSRX1(COUNTEREN_TM)
#define LW_RISCV_CSR_XCOUNTEREN_CY              LW_RISCV_CSRX1(COUNTEREN_CY)
#define LW_RISCV_CSR_XCOUNTEREN_IR              LW_RISCV_CSRX1(COUNTEREN_IR)

#define LW_RISCV_CSR_XIE                        LW_RISCV_CSRX1(IE)
#define LW_RISCV_CSR_XIE_XEIE                   LW_RISCV_CSRX2(IE_, EIE)
#define LW_RISCV_CSR_XIE_XTIE                   LW_RISCV_CSRX2(IE_, TIE)
#define LW_RISCV_CSR_XIE_XSIE                   LW_RISCV_CSRX2(IE_, SIE)

#define LW_RISCV_CSR_XMPUATTR                   LW_RISCV_CSRX1(MPUATTR)
#define LW_RISCV_CSR_XMPUIDX                    LW_RISCV_CSRX1(MPUIDX)
#define LW_RISCV_CSR_XMPUIDX2                   LW_RISCV_CSRX1(MPUIDX2)
#define LW_RISCV_CSR_XMPUIDX2_IDX               LW_RISCV_CSRX1(MPUIDX2_IDX)
#define LW_RISCV_CSR_XMPUIDX_INDEX              LW_RISCV_CSRX1(MPUIDX_INDEX)
#define LW_RISCV_CSR_XMPUVA                     LW_RISCV_CSRX1(MPUVA)
#define LW_RISCV_CSR_XMPUPA                     LW_RISCV_CSRX1(MPUPA)
#define LW_RISCV_CSR_XMPUPA_BASE                LW_RISCV_CSRX1(MPUPA_BASE)
#define LW_RISCV_CSR_XMPURNG                    LW_RISCV_CSRX1(MPURNG)
#define LW_RISCV_CSR_XMPURNG_RANGE              LW_RISCV_CSRX1(MPURNG_RANGE)
#define LW_RISCV_CSR_XMPUATTR_WPR               LW_RISCV_CSRX1(MPUATTR_WPR)
#define LW_RISCV_CSR_XMPUATTR_CACHEABLE         LW_RISCV_CSRX1(MPUATTR_CACHEABLE)
#define LW_RISCV_CSR_XMPUATTR_COHERENT          LW_RISCV_CSRX1(MPUATTR_COHERENT)
#define LW_RISCV_CSR_XMPUATTR_L2C_WR            LW_RISCV_CSRX1(MPUATTR_L2C_WR)
#define LW_RISCV_CSR_XMPUATTR_L2C_RD            LW_RISCV_CSRX1(MPUATTR_L2C_RD)
#define LW_RISCV_CSR_XMPUATTR_UR                LW_RISCV_CSRX1(MPUATTR_UR)
#define LW_RISCV_CSR_XMPUATTR_XR                LW_RISCV_CSRX2(MPUATTR_, R)
#define LW_RISCV_CSR_XMPUATTR_UW                LW_RISCV_CSRX1(MPUATTR_UW)
#define LW_RISCV_CSR_XMPUATTR_XW                LW_RISCV_CSRX2(MPUATTR_, W)
#define LW_RISCV_CSR_XMPUATTR_UX                LW_RISCV_CSRX1(MPUATTR_UX)
#define LW_RISCV_CSR_XMPUATTR_XX                LW_RISCV_CSRX2(MPUATTR_, X)
#define LW_RISCV_CSR_XMPUVA_VLD                 LW_RISCV_CSRX1(MPUVA_VLD)

#define LW_RISCV_CSR_XSTATUS                    LW_RISCV_CSRX1(STATUS)
#define LW_RISCV_CSR_XSTATUS_XIE                LW_RISCV_CSRX2(STATUS_, IE)
#define LW_RISCV_CSR_XSTATUS_XIE_ENABLE         LW_RISCV_CSRX2(STATUS_, IE_ENABLE)
#define LW_RISCV_CSR_XSTATUS_XIE_XTIE           LW_RISCV_CSRX3(STATUS_, IE_, TIE)
#define LW_RISCV_CSR_XSTATUS_XPP                LW_RISCV_CSRX2(STATUS_, PP)
#define LW_RISCV_CSR_XSTATUS_FS                 LW_RISCV_CSRX1(STATUS_FS)
#define LW_RISCV_CSR_XSTATUS_FS_OFF             LW_RISCV_CSRX1(STATUS_FS_OFF)
#define LW_RISCV_CSR_XSTATUS_FS_INITIAL         LW_RISCV_CSRX1(STATUS_FS_INITIAL)
#define LW_RISCV_CSR_XSTATUS_FS_CLEAN           LW_RISCV_CSRX1(STATUS_FS_CLEAN)
#define LW_RISCV_CSR_XSTATUS_FS_DIRTY           LW_RISCV_CSRX1(STATUS_FS_DIRTY)

#define LW_RISCV_CSR_XDCACHEOP                  LW_RISCV_CSRU1(DCACHEOP)
#define LW_RISCV_CSR_XDCACHEOP_ADDR             LW_RISCV_CSRU1(DCACHEOP_ADDR)
#define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE        LW_RISCV_CSRU1(DCACHEOP_ADDR_MODE)
#define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_VA     LW_RISCV_CSRU1(DCACHEOP_ADDR_MODE_VA)
#define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_PA     LW_RISCV_CSRU1(DCACHEOP_ADDR_MODE_PA)
#define LW_RISCV_CSR_XDCACHEOP_MODE             LW_RISCV_CSRU1(DCACHEOP_MODE)
#define LW_RISCV_CSR_XDCACHEOP_MODE_ILW_LINE    LW_RISCV_CSRU1(DCACHEOP_MODE_ILW_LINE)
#define LW_RISCV_CSR_XFLUSH                     LW_RISCV_CSRU1(FLUSH)
#define LW_RISCV_CSR_XSYSOPEN                   LW_RISCV_CSRX1(SYSOPEN)
#define LW_RISCV_CSR_XMISCOPEN                  LW_RISCV_CSRX1(MISCOPEN)

#define LW_RISCV_CSR_XCFG                       LW_RISCV_CSRX1(CFG)
#define LW_RISCV_CSR_XCFG_XPOSTIO               LW_RISCV_CSRX2(CFG_, POSTIO)
#define LW_RISCV_CSR_XCFG_XPOSTIO_TRUE          LW_RISCV_CSRX2(CFG_, POSTIO_TRUE)
#define LW_RISCV_CSR_XCFG_XPOSTIO_FALSE         LW_RISCV_CSRX2(CFG_, POSTIO_FALSE)

#define LW_RISCV_CSR_XRSP                       LW_RISCV_CSRX1(RSP)
#define LW_RISCV_CSR_XRSP_XRSEC                 LW_RISCV_CSRX2(RSP_, RSEC)
#define LW_RISCV_CSR_XRSP_XRSEC_INSEC           LW_RISCV_CSRX2(RSP_, RSEC_INSEC)
#define LW_RISCV_CSR_XRSP_XRSEC_SEC             LW_RISCV_CSRX2(RSP_, RSEC_SEC)

#define LW_RISCV_CSR_XSPM                       LW_RISCV_CSRX1(SPM)
#define LW_RISCV_CSR_XSPM_XSECM                 LW_RISCV_CSRX2(SPM_, SECM)
#define LW_RISCV_CSR_XSPM_XSECM_INSEC           LW_RISCV_CSRX2(SPM_, SECM_INSEC)
#define LW_RISCV_CSR_XSPM_XSECM_SEC             LW_RISCV_CSRX2(SPM_, SECM_SEC)


#define csr_read(csrnum) ({ unsigned long __tmp; \
    __asm__ volatile ("csrr %0, %1" : "=r"(__tmp) : "i"(csrnum)); \
    __tmp; })
#define csr_write(csrnum, val) ({ \
    __asm__ volatile ("csrw %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_set(csrnum, val) ({ \
    __asm__ volatile ("csrs %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_clear(csrnum, val) ({ \
    __asm__ volatile ("csrc %0, %1" ::"i"(csrnum), "r"(val)); })
#define csr_read_and_clear(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrc %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "r"(mask)); \
    __tmp; })
#define csr_read_and_set(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrs %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "r"(mask)); \
    __tmp; })
#define csr_read_and_clear_imm(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrc %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "i"(mask)); \
    __tmp; })
#define csr_read_and_set_imm(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrs %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "i"(mask)); \
    __tmp; })
#define csr_read_and_write_imm(csrnum, mask) ({ unsigned long __tmp; \
    __asm__ volatile ("csrrwi %0, %1, %2" : "=r"(__tmp) :"i"(csrnum), "i"(mask)); \
    __tmp; })

#endif // LWRISCV_CSR_H
