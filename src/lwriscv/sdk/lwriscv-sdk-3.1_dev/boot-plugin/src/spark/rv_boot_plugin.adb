with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;
with Rv_Boot_Plugin_Error_Handling; use Rv_Boot_Plugin_Error_Handling;

with Rv_Brom_Riscv_Core;
with Rv_Boot_Plugin_Selwre_Boot;
with Rv_Boot_Plugin_Pmp; use Rv_Boot_Plugin_Pmp;

procedure rv_boot_plugin is
    Err_Code : Error_Codes := OK;
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BR_RETCODE_Register);

begin
    Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                  Data => LW_PRGNLCL_RISCV_BR_RETCODE_Register'(Result   => RESULT_RUNNING,
                                                                Phase    => Phase_Codes'Enum_Rep (ENTRY_PHASE),
                                                                Syndrome => Error_Codes'Enum_Rep (OK),
                                                                Info     => Info_Codes'Enum_Rep (INIT)));
    Rv_Boot_Plugin_Pmp.Initialize;

    Operation_Done:
    for Unused_Loop_Var in 1 .. 1 loop

        Rv_Boot_Plugin_Selwre_Boot.Init_Device_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        Rv_Boot_Plugin_Selwre_Boot.Load_PKC_Boot_Param_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        Rv_Boot_Plugin_Selwre_Boot.Sanitize_Manifest_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        Rv_Boot_Plugin_Selwre_Boot.Load_Fmc_Data_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        Rv_Boot_Plugin_Selwre_Boot.Load_Fmc_Code_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        -- open up IMEM temporarily to scrub unused portion
        Rv_Boot_Plugin_Selwre_Boot.Revoke_Resource_Phase (Err_Code => Err_Code);
        exit Operation_Done when Err_Code /= OK;

        -- enable Configure_Fmc_Elw_Phase
        Rv_Boot_Plugin_Selwre_Boot.Configure_Fmc_Elw_Phase (Err_Code => Err_Code);
    end loop Operation_Done;

    Throw_Critical_Error (Pz_Code  => ENTRY_PHASE,
                          Err_Code => CRITICAL_ERROR);
end rv_boot_plugin;
