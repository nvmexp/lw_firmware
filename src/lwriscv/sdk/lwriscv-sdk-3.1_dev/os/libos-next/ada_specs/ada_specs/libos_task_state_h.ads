pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;

package libos_task_state_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  --*
  -- *
  -- * @file libos_task_state.h
  -- *
  -- * @brief Task state shared between user API and kernel.
  -- *
  --  

  --*
  -- * @brief Fields in this structure are readable from user-space
  -- *        with libosTaskStateQuery provided your have debug
  -- *        permissions on the target isolation pasid.
  -- *
  --  

  --* @see riscv-isa.h for register indices  
  --* PC address at time of trap  
  --* Reason for trap
  --     *
  --     * Check interrupt bit REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1ULL)
  --     * to determine exception/interupt.
  --     *
  --     * # Exceptions
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_UCALL - Environment call from user-space
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT - Instruction address fault
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT - Load address fault
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT - Store address fault
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_SAMA_FAULT - Store alignment fault
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_IAMA_FAULT - Instruction alignment fault
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_LAMA_FAULT - Load alignment fault
  --     * # Interrupts
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT - Timer interrupt
  --     *  * LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT - External interrupt
  --      

  --* Faulting load/store/exelwtion address
  --     *
  --     *  @note This address is software corrected on TU11x.
  --     *        @see kernel_trap_dispatch for WAR.
  --      

   type LibosTaskState_array751 is array (0 .. 31) of aliased lwtypes_h.LwU64;
   type LibosTaskState is record
      registers : aliased LibosTaskState_array751;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_task_state.h:32
      xepc : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_task_state.h:35
      xcause : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_task_state.h:54
      xbadaddr : aliased lwtypes_h.LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_task_state.h:61
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_task_state.h:62

end libos_task_state_h;
