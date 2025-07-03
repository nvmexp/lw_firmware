-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Prgnlcl; use Dev_Prgnlcl;

package Engine_Constants
is
    Riscv_Irqdeleg_All_Routed_To_S_Mode : constant LW_PRGNLCL_RISCV_IRQDELEG_Register :=
        LW_PRGNLCL_RISCV_IRQDELEG_Register'(Gptmr         => GPTMR_RISCV_SEIP,
                                            Wdtmr         => WDTMR_RISCV_SEIP,
                                            Mthd          => MTHD_RISCV_SEIP,
                                            Ctxsw         => CTXSW_RISCV_SEIP,
                                            Halt          => HALT_RISCV_SEIP,
                                            Exterr        => EXTERR_RISCV_SEIP,
                                            Swgen0        => SWGEN0_RISCV_SEIP,
                                            Swgen1        => SWGEN1_RISCV_SEIP,
                                            Ext_Ctxe      => CTXE_RISCV_SEIP,
                                            Ext_Limitv    => LIMITV_RISCV_SEIP,
                                            Ext_Ecc       => ECC_RISCV_SEIP,
                                            Ext_Second    => SECOND_RISCV_SEIP,
                                            Ext_Therm     => THERM_RISCV_SEIP,
                                            Ext_Miscio    => MISCIO_RISCV_SEIP,
                                            Ext_Rttimer   => RTTIMER_RISCV_SEIP,
                                            Ext_Rsvd8     => RSVD8_RISCV_SEIP,
                                            Dma           => DMA_RISCV_SEIP,
                                            Sha           => SHA_RISCV_SEIP,
                                            Memerr        => MEMERR_RISCV_SEIP,
                                            Icd           => ICD_RISCV_SEIP,
                                            Iopmp         => IOPMP_RISCV_SEIP,
                                            Core_Mismatch => MISMATCH_RISCV_SEIP);

end Engine_Constants;