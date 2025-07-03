-- This package provides top level functions of PKC boot procedure.
with Rv_Boot_Plugin_Error_Handling; use Rv_Boot_Plugin_Error_Handling;

package Rv_Boot_Plugin_Selwre_Boot is
    pragma Warnings(Off, "pragma ""No_Inline"" ignored (not yet supported)",
                    Reason => "Tool deficiency leaves no choice but to ignore. Need to file a ticket");

    -- Ininialize phase
    procedure Init_Device_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__init_device_phase",
       Pre => Err_Code = OK,
       No_Inline;

    -- BR implements two options to load the PKC boot parameters
    -- 1. BR program DMA itself.
    -- 2. External entiry load it via PMB interface before booting up RISCV.
    -- This function will check the BCR to know how to load the parameters and load it if necessary.
    -- In either way, the PKC boot parameters will be placed at fixed location.
    --
    -- dkumar => Arthur to file a ticket for "is not modified, coudl be INPUT. We didn't say INPUT or OUTPUT in the first place"
    --
    procedure Load_PKC_Boot_Param_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__load_pkc_boot_param_phase",
       Pre => Err_Code = OK,
       No_Inline;

    -- Sanitize Manifest parameters.
    -- If parameters exceed their legal ranges, the RISCV will be halt.
    procedure Sanitize_Manifest_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__sanitize_manifest_phase",
       Pre => Err_Code = OK,
       No_Inline;

    -- BR implements two options 2 load the FMC, just like how it works for the PKC boot parameters.
    -- 1. BR program DMA itself.
    -- 2. External entiry load it via PMB interface before booting up RISCV.
    -- This function will check the BCR to know how to load the Fmc and load it if necessary.
    -- In either way, the FMC's code and Data will be placed at fixed location.
    --
    -- Load FMC phase is broken up into two parts. The first part loads the data, and can
    -- be overwritten in IMEM by the FMC, the second part loads the code and cannot be overwritten.
    -- This is done to leave maximum space for the FMC.
    procedure Load_Fmc_Data_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__load_fmc_phase1",
       Pre => Err_Code = OK,
       No_Inline;

    procedure Load_Fmc_Code_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__load_fmc_phase2",
       Pre => Err_Code = OK,
       No_Inline;

    -- Do some cleanup work
    procedure Revoke_Resource_Phase (Err_Code : in out Error_Codes) with
       Linker_Section => "__revoke_resource_phase",
       Pre => Err_Code = OK,
       No_Inline;

    -- Setup the trust exelwtion environment, including
    -- 1. Decrypt verified FMC code and Data with AES128-CBC
    -- 2. DecfuseKey if necessary
    -- 3. Setup FMC's exection environment according to the manifest.
    -- 4. Cleanup BR exelwtion environment.
    procedure Configure_Fmc_Elw_Phase (Err_Code : Error_Codes) with
       Linker_Section => "__configure_fmc_elw_phase",
       Pre => Err_Code = OK,
       No_Inline,
       No_Return;

    pragma Warnings(On, "pragma ""No_Inline"" ignored (not yet supported)");
end Rv_Boot_Plugin_Selwre_Boot;
