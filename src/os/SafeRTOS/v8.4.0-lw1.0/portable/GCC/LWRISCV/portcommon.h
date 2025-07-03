/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef PORTCOMMON_H
#define PORTCOMMON_H

#ifndef __ASSEMBLER__
#include "lwmisc.h"
#include "riscv_csr.h"
#include <lwctassert.h>
#else
#include "riscv_asm_helpers.h"
#endif

/* Various defines which make sure everything is properly aliased. */

/* Offsets for TCB. Cross used in portfeatures.h and portasmmacro.h
 * WARNING: must update when TCB changes in portfeatures.h */
#define TCB_OFFSET_PC                  ( 31 )
#define TCB_OFFSET_SSTATUS             ( 32 )
#define TCB_OFFSET_FLAGS               ( 33 )
#define TCB_OFFSET_SIE                 ( 34 )
#define TCB_CTX_SIZE_WORDS             ( 35 )

/* This is bigger than CTX size because it accesses TCB ulTopOfStackMirror. */
#define TCB_OFFSET_SP_MIRROR           ( 35 )
#define TCB_OFFSET_FPU_CTX_OFFSET      ( 36 )

#define FPU_CTX_OFFSET_FP_STATUS       ( 32 )

#ifdef __ASSEMBLER__
#define LW_RISCV_CSR_SCAUSE_INT_EXPANDED ( 0x8000000000000000U )
#else /* __ASSEMBLER__ */
#define LW_RISCV_CSR_SCAUSE_INT_EXPANDED ( DRF_NUM64( _RISCV_CSR, _SCAUSE, _INT, 0x1 ) )
//ct_asssert( 0x8000000000000000U == LW_RISCV_CSR_SCAUSE_INT_EXPANDED );
#endif /* __ASSEMBLER__ */

#define SCAUSE_SOFTWARE_INTERRUPT       ( LW_RISCV_CSR_SCAUSE_INT_EXPANDED | LW_RISCV_CSR_SCAUSE_EXCODE_S_SWINT )
#define SCAUSE_TIMER_INTERRUPT          ( LW_RISCV_CSR_SCAUSE_INT_EXPANDED | LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT )
#define SCAUSE_EXTERNAL_INTERRUPT       ( LW_RISCV_CSR_SCAUSE_INT_EXPANDED | LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT )

/* Save necessary bits from current status */
#define sstatusReturnBits               ( 0x6120U )
#define sstatusInterruptCheckBits       ( 0x0020U )

#ifdef __ASSEMBLER__

/* When updating, update ct_assert below as well */
#define portSIE_ALL                     ( 0x0222U )

#define portPRIVILEGED_SSTATUS          ( 0x0120U )
#define portPRIVILEGED_NOINTR_SSTATUS   ( 0x0100U )
#define portUNPRIVILEGED_SSTATUS        ( 0x0000U )

#else /* __ASSEMBLER__ */

#define portPRIVILEGED_SSTATUS          ( DRF_DEF64( _RISCV_CSR, _SSTATUS, _SPP, _SUPERVISOR ) | \
                                          DRF_DEF64( _RISCV_CSR, _SSTATUS, _SPIE, _ENABLE ) )

#define portUNPRIVILEGED_SSTATUS        ( DRF_DEF64( _RISCV_CSR, _SSTATUS, _SPP, _USER ) | \
                                          DRF_DEF64( _RISCV_CSR, _SSTATUS, _SPIE, _DISABLE ) )

#define portPRIVILEGED_NOINTR_SSTATUS   ( DRF_DEF64( _RISCV_CSR, _SSTATUS, _SPP, _SUPERVISOR ) )

#define portPRIVILEGED_SSTATUS_MASK     ( DRF_SHIFTMASK64( LW_RISCV_CSR_SSTATUS_SPP  ) | \
                                          DRF_SHIFTMASK64( LW_RISCV_CSR_SSTATUS_SPIE ) )

#define portSIE_ALL                     ( DRF_NUM64( _RISCV_CSR, _SIE, _SEIE, 1 ) | \
                                          DRF_NUM64( _RISCV_CSR, _SIE, _STIE, 1 ) | \
                                          DRF_NUM64( _RISCV_CSR, _SIE, _SSIE, 1 ) )

ct_assert( portSIE_ALL                   == 0x0222U );
ct_assert( portPRIVILEGED_SSTATUS        == 0x0120U );
ct_assert( portPRIVILEGED_NOINTR_SSTATUS == 0x0100U );
ct_assert( portUNPRIVILEGED_SSTATUS      == 0x0000U );

#endif /* __ASSEMBLER__ */

#ifndef __ASSEMBLER__
// Check that ~portPRIVILEGED_SSTATUS can be used to mask out priv
ct_assert( (portUNPRIVILEGED_SSTATUS | (~portPRIVILEGED_SSTATUS_MASK)) == ~portPRIVILEGED_SSTATUS );

// Check that sstatusInterruptCheckBits corresponds to the sPIE bit
ct_assert( portPRIVILEGED_SSTATUS == (portPRIVILEGED_NOINTR_SSTATUS | sstatusInterruptCheckBits) );

// Check that sstatusInterruptCheckBits is set in sstatusReturnBits
ct_assert( (sstatusReturnBits & sstatusInterruptCheckBits) == sstatusInterruptCheckBits );
#endif /* ! defined( __ASSEMBLER__ ) */

#ifndef SAFE_RTOS_BUILD
#error This header file must not be included outside SafeRTOS build.
#endif

#endif /* PORTCOMMON_H */
