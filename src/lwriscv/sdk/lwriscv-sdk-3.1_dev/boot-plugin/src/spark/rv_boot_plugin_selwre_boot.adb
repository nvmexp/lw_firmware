with Lw_Types; use Lw_Types;
with Lw_Types.Shift_Left_Op;use Lw_Types.Shift_Left_Op;
with Lw_Types.Lw_Array; use Lw_Types.Lw_Array;

with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;
with Rv_Brom_Types; use Rv_Brom_Types;
with Rv_Brom_Types.Manifest; use Rv_Brom_Types.Manifest;

with Rv_Boot_Plugin_Sw_Interface; use Rv_Boot_Plugin_Sw_Interface;

with Rv_Brom_Dma; use Rv_Brom_Dma;
with Rv_Brom_Riscv_Core;
with Rv_Boot_Plugin_Pmp;

package body Rv_Boot_Plugin_Selwre_Boot is

    -- Define read procedure of BCRs
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_CTRL_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMACFG_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_LO_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_HI_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_LO_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_HI_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_LO_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_HI_Register);

    -- To concatenate the BCR_DMAADDR_PUBKEY/FMCCODE/FMCDATA/PUBKEY LO and HI registers value into one 64bit address.
    function Make_Bcr_External_Address (Addr_Lo : LwU32; Addr_Hi : External_Memory_Addr_Hi) return Rv_Brom_Dma.External_Memory_Address is
        (Rv_Brom_Dma.External_Memory_Address (Lw_Shift_Left (Value  => LwU64 (Addr_Hi), Amount => 40) or
                                              Lw_Shift_Left (Value  => LwU64 (Addr_Lo), Amount => 8)))
        with Post => Make_Bcr_External_Address'Result mod 256 = 0,
        Inline_Always;

    -- Check BCR_CTRL register to see if we need to fetch manifest, fmc code/data from external.
    function Need_Fetch_Br_From_External return HS_BOOL with Inline_Always;

    -- Check BCR_CTRL and BCR_DMACFG registers to see if we need to do pointer-walking DMA.
    function Is_Pointer_Walking_Enabled return HS_BOOL with Inline_Always;

    -- Sanitize BCRCFG.Lock/BCRCFG.Target and use them if valid
    -- Note: the BCRCFG_SEC.Wprid and Gscid won't be sanitized
    procedure Sanitize_And_Set_Bcr_Dma_Cfg (Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    function Get_Bcr_Dma_Addr_Pkc_Param return Rv_Brom_Dma.External_Memory_Address with
        Post => Get_Bcr_Dma_Addr_Pkc_Param'Result mod 256 = 0,
        Inline_Always;
    function Get_Bcr_Dma_Addr_Fmc_Code return Rv_Brom_Dma.External_Memory_Address with
        Post => Get_Bcr_Dma_Addr_Fmc_Code'Result mod 256 = 0,
        Inline_Always;
    function Get_Bcr_Dma_Addr_Fmc_Data return Rv_Brom_Dma.External_Memory_Address with
        Post => Get_Bcr_Dma_Addr_Fmc_Data'Result mod 256 = 0,
        Inline_Always;
    function Get_Bcr_Dma_Addr_Stage2_Pkc_Param return Rv_Brom_Dma.External_Memory_Address with
        Post => Get_Bcr_Dma_Addr_Stage2_Pkc_Param'Result mod 256 = 0,
        Inline_Always;

    procedure Load_Fmc_Code (Err_Code : in out Error_Codes) with
      Pre => Err_Code = OK,
      Inline_Always;

    procedure Load_Fmc_Data (Err_Code : in out Error_Codes) with
      Pre => Err_Code = OK,
      Inline_Always;

    procedure Init_Device_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := INIT_DEVICE_PHASE;
    begin
        Phase_Entrance_Check (Pz_Code => THIS_PHASE,
                              Err_Code => Err_Code);
        Operation_Done :
        for Unused_Loop_Var in 1 .. 1 loop

            Rv_Brom_Dma.Initialize(Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            if Need_Fetch_Br_From_External = HS_TRUE then
                Sanitize_And_Set_Bcr_Dma_Cfg (Err_Code => Err_Code);
            elsif Need_Fetch_Br_From_External = HS_FALSE then
                null;
            else
                -- FI_COUNTERMEASURE: redundant condition check
                Rv_Brom_Riscv_Core.Halt;
            end if;
        end loop Operation_Done;

        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Err_Code);
    end Init_Device_Phase;

    function Need_Fetch_Br_From_External return HS_BOOL is
        Bcr_Ctrl : LW_PRGNLCL_RISCV_BCR_CTRL_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_CTRL_Address,
                      Data => Bcr_Ctrl);
        return (if Bcr_Ctrl.Brfetch = BRFETCH_TRUE then HS_TRUE else HS_FALSE);
    end Need_Fetch_Br_From_External;

    function Is_Pointer_Walking_Enabled return HS_BOOL is
        Bcr_Ctrl  : LW_PRGNLCL_RISCV_BCR_CTRL_Register;
        Bcr_Dmacfg : LW_PRGNLCL_RISCV_BCR_DMACFG_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_CTRL_Address,
                      Data => Bcr_Ctrl);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMACFG_Address,
                      Data => Bcr_Dmacfg);
        return (if Bcr_Ctrl.Brfetch = BRFETCH_TRUE and then Bcr_Dmacfg.Pointer_Walking = WALKING_TRUE then HS_TRUE else HS_FALSE);
    end Is_Pointer_Walking_Enabled;

    procedure Sanitize_And_Set_Bcr_Dma_Cfg (Err_Code : in out Error_Codes) is
        Cfg : LW_PRGNLCL_RISCV_BCR_DMACFG_Register;
        Cfg_Selwre : LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register;
    begin
        Operation_Done :
        for Unused_Loop_Var in 1 .. 1 loop
            exit Operation_Done when Err_Code /= OK;
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMACFG_Address,
                          Data => Cfg);
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Address,
                          Data => Cfg_Selwre);

            if Cfg.Lock = LOCK_UNLOCKED then
                Err_Code := DMA_LOCK_ERROR;
                exit Operation_Done;
            end if;

            pragma Warnings (Off, "attribute Valid is assumed to return True", Reason => "Arthur needs to investigate");
            if not Cfg.Target'Valid then
                Err_Code := DMA_TARGET_ERROR;
                exit Operation_Done;
            end if;
            pragma Warnings (On, "attribute Valid is assumed to return True");

            Rv_Brom_Dma.Set_Dma_Config (Cfg => Rv_Brom_Dma.Config'(
                                                Wprid => LwU4(Cfg_Selwre.Wprid),
                                                Gscid => Cfg_Selwre.Gscid,
                                                Target => LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_Field'Enum_Rep (Cfg.Target)
                                            ));
        end loop Operation_Done;
    end Sanitize_And_Set_Bcr_Dma_Cfg;

    procedure Load_PKC_Boot_Param_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := LOAD_PKC_BOOT_PARAM_PHASE;
        Src                     : External_Memory_Address;
        Dst                     : Dmem_Offset_Byte;
        Num_Of_Blocks           : Dmem_Size_Block;
    begin
        Phase_Entrance_Check (Pz_Code => THIS_PHASE,
                              Err_Code => Err_Code);

        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_TRUE);

        if Need_Fetch_Br_From_External = HS_TRUE then
            -- Fetch from external

            -- dkumar => Freddie to file a ticket to investigate the rigth way to do this. Note that this part
            -- goes back to my question on: What is the right design point here between linker script, generated header,
            -- named numbers and SPARK code? Right now it's mix of SPARK and linker. I don't like linker script
            -- and SPARK code is running into inability to see that variable is initialized. Need Adacore's guidance
            --
            -- Dhawal can explain to Adacore but need HW team to create a small reproducer since Dhawal doesn't have
            -- time for everything

            -- dkumar => Arthur to create a reproducer for "... are not modeled in SPARK" Need in next 6-8 hours.
            if Is_Pointer_Walking_Enabled = HS_FALSE then
                Src := Get_Bcr_Dma_Addr_Pkc_Param;
            else
                Src := Get_Bcr_Dma_Addr_Stage2_Pkc_Param
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Stage2_Pkc_Boot_Parameters_Byte_Size);
            end if;
            Dst := Rv_Boot_Plugin_Sw_Interface.Get_Pkc_Boot_Parameters_Dmem_Offset;
            Num_Of_Blocks := Rv_Boot_Plugin_Sw_Interface.Get_Pkc_Boot_Parameters_Block_Size;

            --vcast_dont_instrument_start
            -- pragma Assert(Rv_Brom_Dma.Src_Dst_Aligned_For_Read(Src, Dst, Rv_Brom_Dma.SIZE_256B));
            pragma Assert(Num_Of_Blocks > 0);
            pragma Assert(LwU32(Num_Of_Blocks) * 256 <= LwU32(Dmem_Size_Byte'Last) - LwU32(Dst));
            --vcast_dont_instrument_end

            if External_Memory_Address(Num_Of_Blocks) * 256 - 1 <= External_Memory_Address'Last - Src then
                Rv_Brom_Dma.Read_Block (Src           => Src,
                                        Dst           => Dst,
                                        Num_Of_Blocks => Num_Of_Blocks,
                                        Err_Code      => Err_Code);
            else
                Err_Code := DMA_FB_ADDRESS_ERROR;
            end if;
        elsif Need_Fetch_Br_From_External = HS_FALSE then
            null;
        else
            -- FI_COUNTERMEASURE: redundant condition check
            Rv_Brom_Riscv_Core.Halt;
        end if;
        -- else
        -- The Signature has been already bit-banged into the DMEM.
        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_FALSE);
        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Err_Code);
    end Load_PKC_Boot_Param_Phase;

    function Get_Bcr_Dma_Addr_Pkc_Param return Rv_Brom_Dma.External_Memory_Address is
        Bcr_Dmaaddr_Pkcparam_Lo : LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_LO_Register;
        Bcr_Dmaaddr_Pkcparam_Hi : LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_HI_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_LO_Address,
                      Data => Bcr_Dmaaddr_Pkcparam_Lo);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_HI_Address,
                      Data => Bcr_Dmaaddr_Pkcparam_Hi);

        return Make_Bcr_External_Address (Addr_Lo => Bcr_Dmaaddr_Pkcparam_Lo.Val,
                                          Addr_Hi => External_Memory_Addr_Hi (Bcr_Dmaaddr_Pkcparam_Hi.Val));
    end Get_Bcr_Dma_Addr_Pkc_Param;

    function Get_Bcr_Dma_Addr_Stage2_Pkc_Param return Rv_Brom_Dma.External_Memory_Address is
        Bcr_Dmaaddr_Stage2_Pkc_Param_Lo : LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_LO_Register;
        Bcr_Dmaaddr_Stage2_Pkc_Param_Hi : LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_HI_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_LO_Address,
                      Data => Bcr_Dmaaddr_Stage2_Pkc_Param_Lo);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_STAGE2_PKCPARAM_HI_Address,
                      Data => Bcr_Dmaaddr_Stage2_Pkc_Param_Hi);
        return Make_Bcr_External_Address (Addr_Lo => Bcr_Dmaaddr_Stage2_Pkc_Param_Lo.Val,
                                          Addr_Hi => External_Memory_Addr_Hi (Bcr_Dmaaddr_Stage2_Pkc_Param_Hi.Val));
    end Get_Bcr_Dma_Addr_Stage2_Pkc_Param;

    procedure Sanitize_Manifest_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := SANITIZE_MANIFEST_PHASE;
    begin
        Phase_Entrance_Check (Pz_Code => THIS_PHASE,
                              Err_Code => Err_Code);

        Rv_Boot_Plugin_Sw_Interface.Sanitize_Manifest (Err_Code => Err_Code);

        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Err_Code);
    end Sanitize_Manifest_Phase;

    procedure Load_Fmc_Data_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := LOAD_FMC_PHASE;
    begin
        Phase_Entrance_Check (Pz_Code => THIS_PHASE,
                              Err_Code => Err_Code);
        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_TRUE);
        Load_Fmc_Data (Err_Code => Err_Code);
    end Load_Fmc_Data_Phase;

    procedure Load_Fmc_Code_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := LOAD_FMC_PHASE;
    begin
        Load_Fmc_Code (Err_Code => Err_Code);

        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_FALSE);
        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Err_Code);
    end Load_Fmc_Code_Phase;

    procedure Load_Fmc_Code (Err_Code : in out Error_Codes) is
        Src                    : External_Memory_Address;
        Dst                    : Imem_Offset_Byte;
        Num_Of_Blocks          : Imem_Size_Block;
    begin
        if Need_Fetch_Br_From_External = HS_TRUE then
            -- Fetch from outside
            if Is_Pointer_Walking_Enabled = HS_FALSE then
                Src := Get_Bcr_Dma_Addr_Fmc_Code;
            else
                Src := Get_Bcr_Dma_Addr_Stage2_Pkc_Param
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Stage2_Pkc_Boot_Parameters_Byte_Size)
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Pkc_Boot_Parameters_Byte_Size)
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Sw_Pubkey_Byte_Size);
            end if;
            Dst  := Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Code_Imem_Offset;
            Num_Of_Blocks := Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Code_Block_Size;

            --vcast_dont_instrument_start
            pragma Assert(Rv_Brom_Dma.Src_Dst_Aligned_For_Read(Src, Dst, Rv_Brom_Dma.SIZE_256B));
            pragma Assert(LwU32(Num_Of_Blocks) * 256 <= LwU32(Rv_Boot_Plugin_Sw_Interface.Get_End_Of_Imem_Overwritable_Region - Dst));
            --vcast_dont_instrument_end

            if Num_Of_Blocks > 0 and then
               External_Memory_Address(Num_Of_Blocks) * 256 - 1 <= External_Memory_Address'Last - Src
            then
                Rv_Brom_Dma.Read_Block (Src           => Src,
                                        Dst           => Dst,
                                        Num_Of_Blocks => Num_Of_Blocks,
                                        Err_Code      => Err_Code);
            else
                Err_Code := DMA_FB_ADDRESS_ERROR;
            end if;
            pragma Warnings (Off, "statement has no effect", Reason => "FI_COUNTERMEASURE check all possible branches");
        elsif Need_Fetch_Br_From_External = HS_FALSE then
            pragma Warnings (On, "statement has no effect");
            -- The FMC Code has been already bit-banged into the DMEM.
            null;
        else
            -- FI_COUNTERMEASURE: redundant condition check
            Rv_Brom_Riscv_Core.Halt;
        end if;
    end Load_Fmc_Code;

    procedure Load_Fmc_Data (Err_Code : in out Error_Codes) is
        Src                    : External_Memory_Address;
        Dst                    : Dmem_Offset_Byte;
        Num_Of_Blocks          : Dmem_Size_Block;
    begin
        if Need_Fetch_Br_From_External = HS_TRUE then
            -- Fetch from outside
            if Is_Pointer_Walking_Enabled = HS_FALSE then
                Src := Get_Bcr_Dma_Addr_Fmc_Data;
            else
                Src := Get_Bcr_Dma_Addr_Stage2_Pkc_Param
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Stage2_Pkc_Boot_Parameters_Byte_Size)
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Pkc_Boot_Parameters_Byte_Size)
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Sw_Pubkey_Byte_Size)
                    + Rv_Brom_Dma.External_Memory_Address(Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Code_Block_Size * 256);
            end if;
            Dst := Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Data_Dmem_Offset;
            Num_Of_Blocks := Dmem_Size_Block(Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Data_Block_Size);

            if Num_Of_Blocks > 0 then

                --vcast_dont_instrument_start
                pragma Assert (Rv_Brom_Dma.Src_Dst_Aligned_For_Read (Src, Dst, Rv_Brom_Dma.SIZE_256B));
                pragma Assert (LwU32 (Num_Of_Blocks) * 256 <= LwU32 (Dmem_Size_Byte'Last) - LwU32 (Dst));
                --vcast_dont_instrument_end

                if External_Memory_Address(Num_Of_Blocks) * 256 - 1 <= External_Memory_Address'Last - Src
                then
                    Rv_Brom_Dma.Read_Block (Src           => Src,
                                            Dst           => Dst,
                                            Num_Of_Blocks => Num_Of_Blocks,
                                            Err_Code      => Err_Code);
                else
                    Err_Code := DMA_FB_ADDRESS_ERROR;
                end if;
            else
                null; -- if FMC data size is 0, then we don't need to do any DMA
            end if;
            pragma Warnings (Off, "statement has no effect", Reason => "FI_COUNTERMEASURE check all possible branches");
        elsif Need_Fetch_Br_From_External = HS_FALSE then
            pragma Warnings (On, "statement has no effect");
            -- The FMC data has been already bit-banged into the DMEM.
            null;
        else
            -- FI_COUNTERMEASURE: redundant condition check
            Rv_Brom_Riscv_Core.Halt;
        end if;
    end Load_Fmc_Data;

    function Get_Bcr_Dma_Addr_Fmc_Code return Rv_Brom_Dma.External_Memory_Address is
        Bcr_Dmaaddr_Fmccode_Lo : LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO_Register;
        Bcr_Dmaaddr_Fmccode_Hi : LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO_Address,
                      Data => Bcr_Dmaaddr_Fmccode_Lo);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI_Address,
                      Data => Bcr_Dmaaddr_Fmccode_Hi);
        return Make_Bcr_External_Address (Addr_Lo => Bcr_Dmaaddr_Fmccode_Lo.Val,
                                          Addr_Hi => External_Memory_Addr_Hi (Bcr_Dmaaddr_Fmccode_Hi.Val));
    end Get_Bcr_Dma_Addr_Fmc_Code;

    function Get_Bcr_Dma_Addr_Fmc_Data return Rv_Brom_Dma.External_Memory_Address is
        Bcr_Dmaaddr_Fmcdata_Lo : LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_LO_Register;
        Bcr_Dmaaddr_Fmcdata_Hi : LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_HI_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_LO_Address,
                      Data => Bcr_Dmaaddr_Fmcdata_Lo);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCDATA_HI_Address,
                      Data => Bcr_Dmaaddr_Fmcdata_Hi);
        return Make_Bcr_External_Address (Addr_Lo => Bcr_Dmaaddr_Fmcdata_Lo.Val,
                                          Addr_Hi => External_Memory_Addr_Hi (Bcr_Dmaaddr_Fmcdata_Hi.Val));
    end Get_Bcr_Dma_Addr_Fmc_Data;

    procedure Revoke_Resource_Phase (Err_Code : in out Error_Codes) is
        THIS_PHASE : constant Phase_Codes := REVOKE_RESOURCE_PHASE;
        Imem_Scrub_Start : Imem_Offset_Byte;
        Dmem_Scrub_Start : Dmem_Offset_Byte;
        Local_Err        : Error_Codes;
    begin
        Phase_Entrance_Check (Pz_Code  => THIS_PHASE,
                              Err_Code => Err_Code);
        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_TRUE,
        --                                                         Priv_Level3 => HS_FALSE);

        -- Reset each device
        Rv_Brom_Dma.Finalize (Err_Code => Err_Code);
        if Err_Code = OK then
            Local_Err := Err_Code;
        end if;

        -- Scrub IMEM and DMEM
        Imem_Scrub_Start  := Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Code_Imem_Offset + Imem_Offset_Byte (Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Code_Block_Size) * 256;
        if Imem_Scrub_Start < Imem_Offset_Byte (Rv_Boot_Plugin_Sw_Interface.Get_Boot_Plugin_Code_Imem_Offset) then
            Rv_Brom_Riscv_Core.Scrub_Imem_Block (Start => Imem_Scrub_Start,
                                                 Size  => Imem_Size_Byte (Rv_Boot_Plugin_Sw_Interface.Get_Boot_Plugin_Code_Imem_Offset) - Imem_Size_Byte (Imem_Scrub_Start));
        end if;

        Dmem_Scrub_Start := Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Data_Dmem_Offset + Dmem_Offset_Byte (Rv_Boot_Plugin_Sw_Interface.Get_Fmc_Data_Block_Size * 256);
        Rv_Brom_Riscv_Core.Scrub_Dmem_Block (Start => Dmem_Scrub_Start,
                                             Size  => Dmem_Size_Byte (Rv_Boot_Plugin_Sw_Interface.Get_Brom_Stack_Start) - Dmem_Size_Byte (Dmem_Scrub_Start));

        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_FALSE);
        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Err_Code);
    end Revoke_Resource_Phase;


    procedure Configure_Fmc_Elw_Phase (Err_Code : Error_Codes) is
        THIS_PHASE : constant Phase_Codes := CONFIGURE_FMC_ELW_PHASE;
        Local_Err  : Error_Codes;
    begin
        Phase_Entrance_Check (Pz_Code => THIS_PHASE,
                              Err_Code => Err_Code);

        -- Remove the pl0 access for cpuctrl register in case some pl0 engine reset the riscv.
        Rv_Brom_Riscv_Core.Remove_Pl0_Access_For_Cpuctl;

        -- Set up FMC environment, The following steps must be followed.
        -- Step 1,
        -- IO-PMP
        -- Debug Access
        -- Key Isolation
        Program_Manifest_Io_Pmp;
        Program_Manifest_Debug_Control;

        -- Step 2,
        -- Register Pair
        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_TRUE);

        Program_Manifest_Register_Pair (Err_Code => Local_Err);

        -- yitianc todo: make sure we can remove this
        -- Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
        --                                                         Priv_Level3 => HS_FALSE);

        -- MSPM SCP secure indicator - be at here since we don't need PL3 or BR privilege after programming register pairs
        Program_Manifest_Mspm;
        -- FI related.
        Program_Manifest_Mspm;
        -- Device Map
        Phase_Exit_Check (Pz_Code  => THIS_PHASE,
                          Err_Code => Local_Err);

        -- FI related, even if we missed the previous MSPM configuration,
        -- We can still make sure we locked the machine mask and set all other fields to 0
        Rv_Brom_Riscv_Core.Lock_Machine_Mask;
        Rv_Brom_Riscv_Core.Lock_Machine_Mask;

        Log_Pass;
        Program_Manifest_Device_Map;

        -- Step 3,
        -- FMC's Ucode ID
        Program_Manifest_Ucode_Id;
        -- Core-PMP - core-PMP must be after #2 since it is able to lock out #2; carve out a larger region since we still need stack
        Program_Manifest_Core_Pmp;

        --vcast_dont_instrument_start
        -- Scrub stack
        Rv_Brom_Riscv_Core.Scrub_Dmem_Block (Start => Rv_Boot_Plugin_Sw_Interface.Get_Brom_Stack_Start,
                                             Size  => Rv_Boot_Plugin_Sw_Interface.Get_Brom_Stack_Size);

        -- reduce the Core-PMP BROM region since we don't use stack any more
        Rv_Brom_Riscv_Core.Reset_Csrs;

        Rv_Brom_Riscv_Core.Setup_Trap_Handler (Addr => LwU64(Imem_Pa_Offset_Byte'First)); -- 16#100000# is start of IMEM

        Rv_Brom_Riscv_Core.Clear_Gpr;

        Rv_Brom_Riscv_Core.Inst.Ecall;

        -- The following will not be exelwted because we trapped into FMC (IMEM offset 0)
        Rv_Brom_Riscv_Core.Halt;
        pragma Annotate (GNATprove, False_Positive, "call to nonreturning subprogram might be exelwted", "This Halt shouldn't be exelwted in normal run");
        --vcast_dont_instrument_end
    end Configure_Fmc_Elw_Phase;

end Rv_Boot_Plugin_Selwre_Boot;
