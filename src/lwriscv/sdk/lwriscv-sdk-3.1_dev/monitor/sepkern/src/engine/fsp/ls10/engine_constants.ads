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
                                            Ext_Extirq1   => EXTIRQ1_RISCV_SEIP,
                                            Ext_Extirq2   => EXTIRQ2_RISCV_SEIP,
                                            Ext_Extirq3   => EXTIRQ3_RISCV_SEIP,
                                            Ext_Extirq4   => EXTIRQ4_RISCV_SEIP,
                                            Ext_Extirq5   => EXTIRQ5_RISCV_SEIP,
                                            Ext_Extirq6   => EXTIRQ6_RISCV_SEIP,
                                            Ext_Extirq7   => EXTIRQ7_RISCV_SEIP,
                                            Ext_Extirq8   => EXTIRQ8_RISCV_SEIP,
                                            Dma           => DMA_RISCV_SEIP,
                                            Sha           => SHA_RISCV_SEIP,
                                            Memerr        => MEMERR_RISCV_SEIP,
                                            Ctxsw_Error   => ERROR_RISCV_SEIP,
                                            Gdma          => GDMA_RISCV_SEIP,
                                            Icd           => ICD_RISCV_SEIP,
                                            Iopmp         => IOPMP_RISCV_SEIP,
                                            Core_Mismatch => MISMATCH_RISCV_SEIP,
                                            Se_Sap        => SAP_RISCV_SEIP,
                                            Se_Pka        => PKA_RISCV_SEIP,
                                            Se_Rng        => RNG_RISCV_SEIP,
                                            Se_Keymover   => KEYMOVER_RISCV_SEIP);

end Engine_Constants;