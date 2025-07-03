with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;

package Rv_Boot_Plugin_Error_Handling is

    type Error_Codes is (
                         INIT,
                         DMA_FB_ADDRESS_ERROR,
                         DMA_NACK_ERROR,
                         DMA_LOCK_ERROR,
                         DMA_TARGET_ERROR,
                         SHA_ACQUIRE_MUTEX_ERROR,
                         SHA_EXELWTION_ERROR,
                         SHA_RESET_ERROR,
                         DIO_READ_ERROR,
                         DIO_WRITE_ERROR,
                         SE_PDI_ILWALID_ERROR,
                         SE_PKEYHASH_ILWALID_ERROR,
                         SW_PKEY_DIGEST_ERROR,
                         SE_PKA_RETURN_CODE_ERROR,
                         SE_PKA_RESET_ERROR,
                         SE_PKA_DMEM_PARITY_ERROR,
                         SCP_LOAD_SECRET_ERROR,
                         SCP_TRAPPED_DMA_NOT_ALIGNED_ERROR,
                         MANIFEST_CODE_SIZE_ERROR,
                         MANIFEST_CORE_PMP_RESERVATION_ERROR,
                         MANIFEST_DATA_SIZE_ERROR,
                         MANIFEST_DEVICEMAP_BR_UNLOCK_ERROR,
                         MANIFEST_FAMILY_ID_ERROR,
                         MANIFEST_MSPM_VALUE_ERROR,
                         MANIFEST_PAD_INFO_MASK_ERROR,
                         MANIFEST_REG_PAIR_ADDRESS_ERROR,
                         MANIFEST_REG_PAIR_ENTRY_NUM_ERROR,
                         MANIFEST_SECRET_MASK_ERROR,
                         MANIFEST_SECRET_MASK_LOCK_ERROR,
                         MANIFEST_SIGNATURE_ERROR,
                         MANIFEST_UCODE_ID_ERROR,
                         MANIFEST_UCODE_VERSION_ERROR,
                         FMC_DIGEST_ERROR,
                         FUSEKEY_BAD_HEADER_ERROR,
                         FUSEKEY_KEYGLOB_ILWALID_ERROR,
                         FUSEKEY_PROTECT_INFO_ERROR,
                         FUSEKEY_SIGNATURE_ERROR,
                         FUSEKEY_UDS_KDK_ILWALID_ERROR,
                         FUSEKEY_IK_ILWALID_ERROR,
                         FUSEKEY_UDS_KDK_TAG_ERROR,
                         FUSEKEY_IK_TAG_ERROR,
                         KMEM_DISPOSE_KSLOT_ERROR,
                         KMEM_KEY_SLOT_K3_ERROR,
                         KMEM_LOAD_KSLOT2SCP_ERROR,
                         KMEM_READ_ERROR,
                         KMEM_WRITE_ERROR,
                         IOPMP_ERROR,
                         MMIO_ERROR,
                         CRITICAL_ERROR,
                         OK
                        ) with Size => 10;

    for Error_Codes use (
                         INIT                                => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_INIT,
                         DMA_FB_ADDRESS_ERROR                => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DMA_FB_ADDRESS_ERROR,
                         DMA_NACK_ERROR                      => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DMA_NACK_ERROR,
                         DMA_LOCK_ERROR                      => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DMA_LOCK_ERROR,
                         DMA_TARGET_ERROR                    => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DMA_TARGET_ERROR,
                         SHA_ACQUIRE_MUTEX_ERROR             => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SHA_ACQUIRE_MUTEX_ERROR,
                         SHA_EXELWTION_ERROR                 => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SHA_EXELWTION_ERROR,
                         SHA_RESET_ERROR                     => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SHA_RESET_ERROR,
                         DIO_READ_ERROR                      => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DIO_READ_ERROR,
                         DIO_WRITE_ERROR                     => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_DIO_WRITE_ERROR,
                         SE_PDI_ILWALID_ERROR                => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SE_PDI_ILWALID_ERROR,
                         SE_PKEYHASH_ILWALID_ERROR           => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SE_PKEYHASH_ILWALID_ERROR,
                         SW_PKEY_DIGEST_ERROR                => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SW_PKEY_DIGEST_ERROR,
                         SE_PKA_RETURN_CODE_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SE_PKA_RETURN_CODE_ERROR,
                         SE_PKA_RESET_ERROR                  => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SE_PKA_RESET_ERROR,
                         SE_PKA_DMEM_PARITY_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SE_PKA_DMEM_PARITY_ERROR,
                         SCP_LOAD_SECRET_ERROR               => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SCP_LOAD_SECRET_ERROR,
                         SCP_TRAPPED_DMA_NOT_ALIGNED_ERROR   => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_SCP_TRAPPED_DMA_NOT_ALIGNED_ERROR,
                         MANIFEST_CODE_SIZE_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_CODE_SIZE_ERROR,
                         MANIFEST_CORE_PMP_RESERVATION_ERROR => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_CORE_PMP_RESERVATION_ERROR,
                         MANIFEST_DATA_SIZE_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_DATA_SIZE_ERROR,
                         MANIFEST_DEVICEMAP_BR_UNLOCK_ERROR  => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_DEVICEMAP_BR_UNLOCK_ERROR,
                         MANIFEST_FAMILY_ID_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_FAMILY_ID_ERROR,
                         MANIFEST_MSPM_VALUE_ERROR           => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_MSPM_VALUE_ERROR,
                         MANIFEST_PAD_INFO_MASK_ERROR        => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_PAD_INFO_MASK_ERROR,
                         MANIFEST_REG_PAIR_ADDRESS_ERROR     => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_REG_PAIR_ADDRESS_ERROR,
                         MANIFEST_REG_PAIR_ENTRY_NUM_ERROR   => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_REG_PAIR_ENTRY_NUM_ERROR,
                         MANIFEST_SECRET_MASK_ERROR          => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_SECRET_MASK_ERROR,
                         MANIFEST_SECRET_MASK_LOCK_ERROR     => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_SECRET_MASK_LOCK_ERROR,
                         MANIFEST_SIGNATURE_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_SIGNATURE_ERROR,
                         MANIFEST_UCODE_ID_ERROR             => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_UCODE_ID_ERROR,
                         MANIFEST_UCODE_VERSION_ERROR        => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MANIFEST_UCODE_VERSION_ERROR,
                         FMC_DIGEST_ERROR                    => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FMC_DIGEST_ERROR,
                         FUSEKEY_BAD_HEADER_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_BAD_HEADER_ERROR,
                         FUSEKEY_KEYGLOB_ILWALID_ERROR       => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_KEYGLOB_ILWALID_ERROR,
                         FUSEKEY_PROTECT_INFO_ERROR          => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_PROTECT_INFO_ERROR,
                         FUSEKEY_SIGNATURE_ERROR             => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_SIGNATURE_ERROR,
                         FUSEKEY_UDS_KDK_ILWALID_ERROR => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_UDS_KDK_ILWALID_ERROR,
                         FUSEKEY_IK_ILWALID_ERROR => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_IK_PVT_ILWALID_ERROR,
                         FUSEKEY_UDS_KDK_TAG_ERROR =>  LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_UDS_KDK_TAG_ERROR,
                         FUSEKEY_IK_TAG_ERROR =>  LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_FUSEKEY_IK_PVT_TAG_ERROR,
                         KMEM_DISPOSE_KSLOT_ERROR            => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_KMEM_DISPOSE_KSLOT_ERROR,
                         KMEM_KEY_SLOT_K3_ERROR              => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_KMEM_KEY_SLOT_K3_ERROR,
                         KMEM_LOAD_KSLOT2SCP_ERROR           => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_KMEM_LOAD_KSLOT2SCP_ERROR,
                         KMEM_READ_ERROR                     => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_KMEM_READ_ERROR,
                         KMEM_WRITE_ERROR                    => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_KMEM_WRITE_ERROR,
                         IOPMP_ERROR                         => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_IOPMP_ERROR,
                         MMIO_ERROR                          => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_MMIO_ERROR,
                         CRITICAL_ERROR                      => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_CRITICAL_ERROR,
                         OK                                  => LW_PRGNLCL_RISCV_BR_RETCODE_SYNDROME_OK
                        );


    -- Override last_chance_handler
    -- pragma Warnings (Off, "subprogram ""Last_Chance_Handler"" has no effect", Reason => "Unsure why SPARK is unhappy despite SPARK_Mode being Off on AP_HALT");
    procedure Last_Chance_Handler with No_Return;
    --pragma Warnings (On, "subprogram ""Last_Chance_Handler"" has no effect");
    pragma Export (C, Last_Chance_Handler, "__gnat_last_chance_handler");

    type Phase_Codes is (
                         ENTRY_PHASE,
                         INIT_DEVICE_PHASE,
                         LOAD_PKC_BOOT_PARAM_PHASE,
                         SANITIZE_MANIFEST_PHASE,
                         LOAD_FMC_PHASE,
                         REVOKE_RESOURCE_PHASE,
                         CONFIGURE_FMC_ELW_PHASE
                        ) with Size => 6;

    for Phase_Codes use (
                         ENTRY_PHASE                     => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_ENTRY,
                         INIT_DEVICE_PHASE               => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_INIT_DEVICE,
                         LOAD_PKC_BOOT_PARAM_PHASE       => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_LOAD_PKC_BOOT_PARAM,
                         SANITIZE_MANIFEST_PHASE         => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_SANITIZE_MANIFEST,
                         LOAD_FMC_PHASE                  => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_LOAD_FMC,
                         REVOKE_RESOURCE_PHASE           => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_REVOKE_RESOURCE,
                         CONFIGURE_FMC_ELW_PHASE         => LW_PRGNLCL_RISCV_BR_RETCODE_PHASE_CONFIGURE_FMC_ELW
                         );

    type Result_Codes is new LW_PRGNLCL_RISCV_BR_RETCODE_RESULT_Field;

    type Info_Codes is (
                        INIT,
                        DMA_WAIT_FOR_IDLE_HANG,
                        -- TRAPPED_DMA_HANG,
                        UCODE_VERSION_READ_HANG
                       ) with Size => 14;

    for Info_Codes use (
                        INIT                          => LW_PRGNLCL_RISCV_BR_RETCODE_INFO_INIT,
                        DMA_WAIT_FOR_IDLE_HANG        => LW_PRGNLCL_RISCV_BR_RETCODE_INFO_FBDMA_HANG,
                        -- TRAPPED_DMA_HANG              => LW_PRGNLCL_RISCV_BR_RETCODE_INFO_TRAPPED_DMA_HANG,
                        UCODE_VERSION_READ_HANG       => 16#2000#
                       );

    -- Clean up resources and halt the core.
    pragma Warnings(Off, "pragma ""No_Inline"" ignored (not yet supported)", Reason => "Tool deficiency leaves no choice but to ignore. Need to file a ticket");
    procedure Trap_Handler with No_Inline, No_Return;
    pragma Warnings(On, "pragma ""No_Inline"" ignored (not yet supported)");

    -- Return corresponding error code if any Store instruction fail or any memory access errro happen
    -- Return OK if no error found
    procedure Check_Mmio_Or_Io_Pmp_Error (Err_Code : out Error_Codes) with
        Inline_Always;

    -- If any critical error is seen, this procedure must be called in order to jump to Trap_Handler
    procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes) with
        Pre => Err_Code /= OK,
        Inline_Always,
        No_Return;

    -- Call this procedure at the very beginning of each phase.
    procedure Phase_Entrance_Check (Pz_Code : Phase_Codes; Err_Code : Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    -- Call this procedure at the end of each phase.
    procedure Phase_Exit_Check (Pz_Code : Phase_Codes; Err_Code : Error_Codes) with
        Inline_Always;

    -- Log info codes only, the program will continue to run.
    -- Info Codes are one hot encoded.
    procedure Log_Info (Info_Code : Info_Codes) with
        Pre => Info_Code /= INIT,
        Inline_Always;

    procedure Log_Pass with
        Inline_Always;

end Rv_Boot_Plugin_Error_Handling;
