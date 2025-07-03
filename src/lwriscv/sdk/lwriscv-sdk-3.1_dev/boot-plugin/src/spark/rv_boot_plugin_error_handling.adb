-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

with Lw_Types; use Lw_Types;
with Rv_Brom_Types; use Rv_Brom_Types;

with Rv_Brom_Dma;
with Rv_Brom_Riscv_Core;
with Rv_Boot_Plugin_Sw_Interface;
with Rv_Boot_Plugin_Pmp;

package body Rv_Boot_Plugin_Error_Handling is

    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BR_RETCODE_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_IRQSTAT_Register);

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BR_RETCODE_Register);

    pragma Warnings (Off, "pragma ""No_Inline"" ignored (not yet supported)", Reason => "Tool deficiency leaves no choice but to ignore. Need to file a ticket");
    -- In trap_handler if we get exception again we will trap the trap_handler in loop.
    -- To break this loop, we will set exception handler to this procedure
    procedure Trap_Handler_For_Halt_Only with No_Inline, No_Return;
    pragma Warnings (On, "pragma ""No_Inline"" ignored (not yet supported)");

    function Get_Address_Of_Trap_Handler return LwU64 with Inline_Always;
    function Get_Address_Of_Trap_Handler_For_Halt_Only return LwU64 with Inline_Always;


    -- Each compressed Nop instruction oclwpies 2 byte,
    -- So we add 32 nops to mach the CAM_ENTRY number of ROM patch feature
    procedure Adding_32_Nops with
        Global => null,
        Inline_Always;

    -------------------------
    -- Last_Chance_Handler --
    -------------------------
    procedure Last_Chance_Handler is
        --pragma Unreferenced (Line, Source_Location);
    begin
        --vcast_dont_instrument_start
        --Rv_Brom_Log.Put_Line ("I'm in last chance handler");
        Rv_Brom_Riscv_Core.Halt;
        --vcast_dont_instrument_end
    end Last_Chance_Handler;

    procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes) is
        Retcode : LW_PRGNLCL_RISCV_BR_RETCODE_Register;
    begin
        -- Make sure we will trap to trap handler.
        -- We need make sure that the Address of trap handler must be 4 byte aligned.
        --vcast_dont_instrument_start
        pragma Assume (Get_Address_Of_Trap_Handler mod 4 = 0);
        --vcast_dont_instrument_end
        Rv_Brom_Riscv_Core.Setup_Trap_Handler(Addr => Get_Address_Of_Trap_Handler);
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);
        Retcode := (Result   => RESULT_FAIL,
                    Phase    => Phase_Codes'Enum_Rep (Pz_Code),
                    Syndrome => Error_Codes'Enum_Rep (Err_Code),
                    Info     => Retcode.Info
                   );

        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);

        Rv_Brom_Riscv_Core.Inst.Ecall;
        --vcast_dont_instrument_start
        Rv_Brom_Riscv_Core.Halt;
        pragma Annotate (GNATprove, False_Positive, "call to nonreturning subprogram might be exelwted", "This Halt shouldn't be exelwted in normal run");
        --vcast_dont_instrument_end
    end Throw_Critical_Error;

    procedure Phase_Entrance_Check (Pz_Code : Phase_Codes; Err_Code : Error_Codes) is
        Retcode : LW_PRGNLCL_RISCV_BR_RETCODE_Register;
    begin
        if Err_Code = OK then
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                          Data => Retcode);
            if Retcode.Syndrome = Error_Codes'Enum_Rep (OK) then
                Retcode := (Result   => RESULT_RUNNING,
                            Phase    => Phase_Codes'Enum_Rep (Pz_Code),
                            Syndrome => Error_Codes'Enum_Rep (Err_Code),
                            Info     => Retcode.Info
                           );
                Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                              Data => Retcode);
            else
                Throw_Critical_Error (Pz_Code  => Pz_Code,
                                      Err_Code => MMIO_ERROR);
            end if;

        else
            Throw_Critical_Error (Pz_Code  => Pz_Code,
                                  Err_Code => Err_Code);
        end if;

    end Phase_Entrance_Check;

    procedure Phase_Exit_Check (Pz_Code : Phase_Codes; Err_Code : Error_Codes) is
        Local_Err : Error_Codes := Err_Code;
    begin
        if Local_Err = OK then
            --pragma Unreferenced (Pz_Code);
            Check_Mmio_Or_Io_Pmp_Error (Err_Code => Local_Err);
        end if;

        Adding_32_Nops;
        if Local_Err /= OK then
            Throw_Critical_Error (Pz_Code  => Pz_Code,
                                  Err_Code => Local_Err);
        end if;
    end Phase_Exit_Check;

    procedure Check_Mmio_Or_Io_Pmp_Error ( Err_Code : out Error_Codes) is
        Stat : LW_PRGNLCL_FALCON_IRQSTAT_Register;
    begin
        Err_Code := OK;
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_IRQSTAT_Address,
                      Data => Stat);
        if Stat.Memerr = MEMERR_TRUE then
            Err_Code := MMIO_ERROR;
        end if;

        -- FBDMA, CPDMA(except for shortlwt DMAs), PMB, and SHA accessing IMEM and DMEM (KMEM and BMEM not inllwded)
        -- are protected by IO-PMP. FBDMA accessing global memory is protected by IO-PMP.
        if Stat.Iopmp = IOPMP_TRUE then
            Err_Code := IOPMP_ERROR;
        end if;

    end Check_Mmio_Or_Io_Pmp_Error;

    procedure Log_Info (Info_Code : Info_Codes) is
        Retcode : LW_PRGNLCL_RISCV_BR_RETCODE_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);

        --vcast_dont_do_mcdc
        Retcode := (Result   => RESULT_RUNNING,
                    Phase    => Retcode.Phase,
                    Syndrome => Retcode.Syndrome,
                    Info     => Info_Codes'Enum_Rep (Info_Code) or Retcode.Info
                   );

        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);
    end Log_Info;

    procedure Log_Pass is
        Retcode : LW_PRGNLCL_RISCV_BR_RETCODE_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);
        Retcode := (Result   => RESULT_PASS,
                    Phase    => Retcode.Phase,
                    Syndrome => Retcode.Syndrome,
                    Info     => Retcode.Info
                   );
        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);
    end Log_Pass;


    -- Trap_Handler will be called only when a problem happens. We should NOT return to FMC in this case.
    -- This is practically part of cleanup before halting.
    procedure Trap_Handler is
        Unused_Err_Code : Error_Codes;  -- We don't want to handle errors in this procedure
                                        -- We don't care about any error enountered in the middle as long as we can execute the following steps
        Retcode  : LW_PRGNLCL_RISCV_BR_RETCODE_Register;
    begin
        -- We need make sure that the Address of trap handler must be 4 byte aligned.
        -- But we set the aligned attribute in the linker script, I don't believe SPARK knows this,
        -- So we explicitly Assume the address of Get_Address_Of_Trap_Handler_For_Halt_Only is 4 byte aligned.
        --vcast_dont_instrument_start
        pragma Assume (Get_Address_Of_Trap_Handler_For_Halt_Only mod 4 = 0);
        --vcast_dont_instrument_end
        Rv_Brom_Riscv_Core.Setup_Trap_Handler (Addr => Get_Address_Of_Trap_Handler_For_Halt_Only);

        -- If there are any runtime exceptions, we will end up in this trap handler directly.
        -- So we have to make sure the RETCODE is set properly.
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                      Data => Retcode);
        if Retcode.Syndrome = Error_Codes'Enum_Rep (OK) or else
            Retcode.Result /= RESULT_FAIL
        then
            Retcode := (Result   => RESULT_FAIL,
                        Phase    => Retcode.Phase,
                        Syndrome => Error_Codes'Enum_Rep (CRITICAL_ERROR),
                        Info     => Retcode.Info
                       );

            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_BR_RETCODE_Address,
                          Data => Retcode);
        end if;

        Rv_Brom_Riscv_Core.Set_Mepc (Addr => 0);

        -- Use priv level 3
        Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_TRUE,
                                                                Priv_Level3 => HS_TRUE);

        Rv_Brom_Riscv_Core.Change_Priv_Level_And_Selwre_Status (Selwre_Mode => HS_FALSE,
                                                                Priv_Level3 => HS_FALSE);

        Rv_Brom_Dma.Finalize (Err_Code => Unused_Err_Code);

        -- Write MPLM.MMLOCK = 1
        Rv_Brom_Riscv_Core.Lock_Machine_Mask;

        --vcast_dont_instrument_start
        -- Scrub stack
        -- Maybe we don't need this, we can block all access from core-pmp and io-pmp
        Rv_Brom_Riscv_Core.Scrub_Dmem_Block (Start => Rv_Boot_Plugin_Sw_Interface.Get_Brom_Stack_Start,
                                             Size  => Rv_Boot_Plugin_Sw_Interface.Get_Brom_Stack_Size);

        Rv_Brom_Riscv_Core.Reset_Csrs;
        Rv_Brom_Riscv_Core.Clear_Gpr; -- Reset RISCV GPRs. Make sure this is as far down here as possible since we can
                                      -- only have forced inline fns from here onwards that do not lead to furthermore
                                      -- stack usage
        Rv_Brom_Riscv_Core.Halt;
        pragma Annotate (GNATprove, False_Positive, "call to nonreturning subprogram might be exelwted", "This Halt shouldn't be exelwted in normal run");
        --vcast_dont_instrument_end
    end Trap_Handler;

    function Get_Address_Of_Trap_Handler return LwU64 with
        SPARK_Mode => Off
    is
    begin
        return Address_To_LwU64 (Trap_Handler'Address);
    end Get_Address_Of_Trap_Handler;

    procedure Trap_Handler_For_Halt_Only is
    begin

        Rv_Brom_Riscv_Core.Lock_Machine_Mask;
        --vcast_dont_instrument_start
        Rv_Brom_Riscv_Core.Halt;
        pragma Annotate (GNATprove, False_Positive, "call to nonreturning subprogram might be exelwted", "This Halt shouldn't be exelwted in normal run");
        --vcast_dont_instrument_end
    end Trap_Handler_For_Halt_Only;

    function Get_Address_Of_Trap_Handler_For_Halt_Only return LwU64 with
        SPARK_Mode => Off
    is
    begin
        return Address_To_LwU64 (Trap_Handler_For_Halt_Only'Address);
    end Get_Address_Of_Trap_Handler_For_Halt_Only;


    procedure Adding_32_Nops is
    begin
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
        Rv_Brom_Riscv_Core.Inst.Nop;
    end Adding_32_Nops;


end Rv_Boot_Plugin_Error_Handling;
