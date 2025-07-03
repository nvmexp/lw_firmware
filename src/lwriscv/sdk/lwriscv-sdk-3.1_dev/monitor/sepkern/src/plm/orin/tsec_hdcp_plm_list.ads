-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;    use Lw_Types;
with PLM_Types;   use PLM_Types;
with Dev_Prgnlcl; use Dev_Prgnlcl;

package PLM_List
is

    LW_PPRIV_SYS_PRI_SOURCE_ID_SEC2 : constant LwU20 := 16#2#;

    PLM_AD_Pair : constant PLM_Array_Type := (
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_AMAP_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_BOOTVEC_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_DBGCTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_DIODT_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_DIODTA_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_DMA_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_L0R_L3W_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_DMEM_PRIV_LEVEL_MASK_Address,
            Data    => (
                Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED,
                Read_Violation       => VIOLATION_REPORT_ERROR,
                Write_Violation      => VIOLATION_REPORT_ERROR,
                Source_Read_Control  => CONTROL_BLOCKED,
                Source_Write_Control => CONTROL_BLOCKED,
                Source_Enable        => LW_PPRIV_SYS_PRI_SOURCE_ID_SEC2
            )
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_L0R_L3W_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_IMEM_PRIV_LEVEL_MASK_Address,
            Data    => (
                Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED,
                Read_Violation       => VIOLATION_REPORT_ERROR,
                Write_Violation      => VIOLATION_REPORT_ERROR,
                Source_Read_Control  => CONTROL_BLOCKED,
                Source_Write_Control => CONTROL_BLOCKED,
                Source_Enable        => LW_PPRIV_SYS_PRI_SOURCE_ID_SEC2
            )
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_IRQSCMASK_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_RO_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK_0_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_PRIVSTATE_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_SCTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_SHA_RAL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_TMR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_TRACEBUF_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_WDTMR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_RO_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_BCR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_RO_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_BOOTVEC_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_DBGCTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_RO_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_MSIP_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_LWCONFIG_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_KMEM_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_ERR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_RO_ALL_SOURCE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_PICSCMASK_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_MSPM_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_HSCTL_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_BR_PARAMS_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_CSBERR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_EXCI_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_EXTERR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_ENG_STATE_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_BRKPT_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_ICD_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_IDLESTATE_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_RSTAT0_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_SP_MIN_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        ),
        PLM_Pair'(
            Address => LW_PRGNLCL_FALCON_SVEC_SPR_PRIV_LEVEL_MASK_Address,
            Data    => PRIV_LEVEL_MASK_ALL_DISABLE
        )
        );

end PLM_List;
