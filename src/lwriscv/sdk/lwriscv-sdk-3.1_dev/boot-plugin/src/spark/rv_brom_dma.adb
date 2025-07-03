-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

package body Rv_Brom_Dma is

    package Fb_If is
        procedure Initialize with Inline_Always;
        procedure Finalize with Inline_Always;
        procedure Set_Config (Cfg : Config) with Inline_Always;
        procedure Set_Fb_Addr (Fb_Addr : LwU64) with Inline_Always;
    end Fb_If;

    package body Fb_If is separate;

    DMA_MAX_TIMEOUT : constant LwU32 := 16#FFFF_FFF_E#; -- Can't use LwU32'Last - 1 since '-' has a contract which will lead to elaboration
                                                        -- dkumar -> Arthur: Why are all timeouts not in one place?

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMACTL_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFMOFFS_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFCMD_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMAINFO_CTL_Register);


    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFCMD_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMAINFO_CTL_Register);

    procedure Execute (Fb_Addr    : LwU64;
                       Mem_Offset : LwU24;
                       Size       : LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field;
                       To_Imem    : LW_PRGNLCL_FALCON_DMATRFCMD_IMEM_Field;
                       Write      : LW_PRGNLCL_FALCON_DMATRFCMD_WRITE_Field) with
        Pre => Src_Dst_Aligned_For_Read (Fb_Addr, Mem_Offset, Size),
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    -- When NACK as ACK feature is enabled, the DMA error can be retrieved from Dma_Info_Ctl.Intr_Err_Completion
    procedure Check_Dma_Error_And_Clear ( Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    -- If the DMA request buffer is full, new request will be dropped siliently.
    -- So we must to ensure that there is an empty entry before issuing new DMA request.
    procedure Wait_For_Free_Entry with
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Initialize (Err_Code : in out Error_Codes) is
    begin
        -- Is this really necessary?
        Wait_For_Complete (Err_Code => Err_Code);

        if Err_Code = OK then
            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_FALCON_DMACTL_Address,
                          Data =>  LW_PRGNLCL_FALCON_DMACTL_Register'
                              (Require_Ctx         => LW_PRGNLCL_FALCON_DMACTL_REQUIRE_CTX_FALSE, -- Don't Require CTX
                               Dmem_Scrubbing      => SCRUBBING_DONE, -- DK: Awkward looking code due to inability to say R/O field
                               Imem_Scrubbing      => SCRUBBING_DONE,
                               Dmaq_Num            => 0,
                               Selwre_Stat         => 0,
                               Dcache_Ilwalidating => ILWALIDATING_DONE,
                               Icache_Ilwalidating => ILWALIDATING_DONE)
                         );
            Fb_If.Initialize;
        end if;
    end Initialize;

    procedure Finalize (Err_Code : out Error_Codes) is
    begin
        Err_Code := OK;
        Wait_For_Complete (Err_Code => Err_Code);

        -- This is intended to clear the information left in DMATRFCMD,
        -- but if we write to this register, it will trigger an DMA read to
        -- override the DMEM starts from offset 0.
        --          Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFCMD_Address,
        --                         Data => LW_PRGNLCL_FALCON_DMATRFCMD_Register'(
        --                             Full      => FULL_FALSE,
        --                             Idle      => IDLE_FALSE,
        --                             Sec       => 16#0#,
        --                             Imem      => IMEM_FALSE,
        --                             Write     => WRITE_FALSE,
        --                             Notify    => NOTIFY_FALSE,
        --                             Size      => SIZE_4B,
        --                             Ctxdma    => 16#0#,
        --                             Set_Dmtag => LW_PRGNLCL_FALCON_DMATRFCMD_SET_DMTAG_INIT,
        --                             Error     => ERROR_FALSE,
        --                             Lvl       => 16#0#,
        --                             Set_Dmlvl => LW_PRGNLCL_FALCON_DMATRFCMD_SET_DMLVL_INIT)
        --                        );

        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_FALCON_DMAINFO_CTL_Address,
                      Data => LW_PRGNLCL_FALCON_DMAINFO_CTL_Register'(
                          Clr_Fbrd            => FBRD_FALSE,
                          Clr_Fbwr            => FBWR_FALSE,
                          Intr_Err_Completion => LW_PRGNLCL_FALCON_DMAINFO_CTL_INTR_ERR_COMPLETION_CLR,
                          Intr_Err_Dmatype    => DMATYPE_NORMAL,
                          Intr_Err_Dmaread    => DMAREAD_FALSE
                         )
                     );
        Fb_If.Finalize;

    end Finalize;

    procedure Set_Dma_Config (Cfg : Config) renames Fb_If.Set_Config;

    procedure Read (Src        : External_Memory_Address;
                    Dst        : Imem_Offset_Byte;
                    Size       : Imem_Transfer_Size) is

    begin
        Execute (Fb_Addr    => LwU64 (Src),
                 Mem_Offset => LwU24 (Dst),
                 Size       => LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size),
                 To_Imem    => IMEM_TRUE,
                 Write      => WRITE_FALSE);
    end Read;

    procedure Read (Src        : External_Memory_Address;
                    Dst        : Dmem_Offset_Byte;
                    Size       : Dmem_Transfer_Size) is
    begin
        Execute (Fb_Addr    => LwU64 (Src),
                 Mem_Offset => LwU24 (Dst),
                 Size       => LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size),
                 To_Imem    => IMEM_FALSE,
                 Write      => WRITE_FALSE);
    end Read;

    procedure Read_Block (Src           : External_Memory_Address;
                          Dst           : Imem_Offset_Byte;
                          Num_Of_Blocks : Imem_Size_Block;
                          Err_Code      : in out Error_Codes) is
        Fb_Src   : External_Memory_Address  := Src;
        Imem_Dst : LwU32 := LwU32(Dst);
    begin
        if Err_Code = OK then
            for Blk in 1 .. Num_Of_Blocks loop
                --vcast_dont_instrument_start
                pragma Loop_Ilwariant (Imem_Dst = Imem_Dst'Loop_Entry + LwU32 (Blk - 1) * 256 and then
                                       Src_Dst_Aligned_For_Read (Fb_Src, Imem_Offset_Byte (Imem_Dst), Rv_Brom_Dma.SIZE_256B) and then
                                       Fb_Src = Src + External_Memory_Address(Blk - 1) * 256);
                --vcast_dont_instrument_end

                Rv_Brom_Dma.Read (Src      => Fb_Src,
                                  Dst      => Imem_Offset_Byte (Imem_Dst),
                                  Size     => Rv_Brom_Dma.SIZE_256B);

                -- Don't increment on last iteration to avoid overflow.
                if Blk < Num_Of_Blocks then
                    Fb_Src := Fb_Src + 256;
                    Imem_Dst := Imem_Dst + 256;
                end if;
            end loop;

            Wait_For_Complete (Err_Code => Err_Code);
        end if;
    end Read_Block;

    -- Dma Data in from outside to Dmem
    -- Each Block is 256-byte length
    procedure Read_Block (Src           : External_Memory_Address;
                          Dst           : Dmem_Offset_Byte;
                          Num_Of_Blocks : Dmem_Size_Block;
                          Err_Code      : in out Error_Codes) is
        Fb_Src   : External_Memory_Address  := Src;
        Dmem_Dst : LwU32 := LwU32(Dst);
    begin
        if Err_Code = OK then
            for Blk in 1 .. Num_Of_Blocks loop
                --vcast_dont_instrument_start
                pragma Loop_Ilwariant (Dmem_Dst = Dmem_Dst'Loop_Entry + LwU32 (Blk - 1) * 256 and then
                                       Src_Dst_Aligned_For_Read (Fb_Src, Dmem_Offset_Byte (Dmem_Dst), Rv_Brom_Dma.SIZE_256B) and then
                                       Fb_Src = Src + External_Memory_Address(Blk - 1) * 256) ;
                --vcast_dont_instrument_end

                Rv_Brom_Dma.Read (Src      => Fb_Src,
                                  Dst      => Dmem_Offset_Byte (Dmem_Dst),
                                  Size     => Rv_Brom_Dma.SIZE_256B);

                -- Don't increment on last iteration to avoid overflow.
                if Blk < Num_Of_Blocks then
                    Fb_Src := Fb_Src + 256;
                    Dmem_Dst := Dmem_Dst + 256;
                end if;
            end loop;

            Wait_For_Complete (Err_Code => Err_Code);
        end if;
    end Read_Block;

    procedure Write (Src      : Imem_Offset_Byte;
                     Dst      : External_Memory_Address;
                     Size     : Imem_Transfer_Size) is
    begin
        Execute (Fb_Addr    => LwU64 (Dst),
                 Mem_Offset => LwU24 (Src),
                 Size       => LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size),
                 To_Imem    => IMEM_TRUE,
                 Write      => WRITE_TRUE);
    end Write;


    procedure Write (Src      : Dmem_Offset_Byte;
                     Dst      : External_Memory_Address;
                     Size     : Dmem_Transfer_Size) is
    begin
        Execute (Fb_Addr    => LwU64 (Dst),
                 Mem_Offset => LwU24 (Src),
                 Size       => LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size),
                 To_Imem    => IMEM_FALSE,
                 Write      => WRITE_TRUE);
    end Write;

    procedure Execute (Fb_Addr    : LwU64;
                       Mem_Offset : LwU24;
                       Size       : LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field;
                       To_Imem    : LW_PRGNLCL_FALCON_DMATRFCMD_IMEM_Field;
                       Write      : LW_PRGNLCL_FALCON_DMATRFCMD_WRITE_Field) is


        Trf_Cmd  : constant LW_PRGNLCL_FALCON_DMATRFCMD_Register := (Full      => FULL_FALSE,
                                                                     Idle      => IDLE_FALSE,
                                                                     Sec       => 0,
                                                                     Imem      => To_Imem,
                                                                     Write     => Write,
                                                                     Notify    => NOTIFY_FALSE,
                                                                     Size      => Size,
                                                                     Ctxdma    => 0,
                                                                     Set_Dmtag => LW_PRGNLCL_FALCON_DMATRFCMD_SET_DMTAG_INIT,
                                                                     Error     => ERROR_FALSE,
                                                                     Lvl       => 0,
                                                                     Set_Dmlvl => LW_PRGNLCL_FALCON_DMATRFCMD_SET_DMLVL_INIT);
    begin
        -- Wait for free entry before issuing any DMA operation
        Wait_For_Free_Entry;

        Fb_If.Set_Fb_Addr (Fb_Addr => Fb_Addr);
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFMOFFS_Address,
                       Data => LW_PRGNLCL_FALCON_DMATRFMOFFS_Register'(Offs => Mem_Offset));
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFCMD_Address,
                       Data => Trf_Cmd);

    end Execute;

    procedure Wait_For_Complete (Err_Code : in out Error_Codes) is
        Trf_Cmd  : LW_PRGNLCL_FALCON_DMATRFCMD_Register;
        Timer    : LwU32 := 0;
    begin
        Operation_Done :
        for Unused_Loop_Var in 1 .. 1 loop
            exit Operation_Done when Err_Code /= OK;
            loop
                Csb_Reg_Rd32 ( Addr => LW_PRGNLCL_FALCON_DMATRFCMD_Address,
                               Data => Trf_Cmd);
                exit when Trf_Cmd.Idle = IDLE_TRUE;

                if Timer > DMA_MAX_TIMEOUT then
                    Rv_Boot_Plugin_Error_Handling.Log_Info (Info_Code => DMA_WAIT_FOR_IDLE_HANG);
                else
                    Timer := Timer + 1;
                end if;
            end loop;
            Check_Dma_Error_And_Clear (Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;
            Rv_Boot_Plugin_Error_Handling.Check_Mmio_Or_Io_Pmp_Error (Err_Code => Err_Code);
        end loop Operation_Done;
    end Wait_For_Complete;

    procedure Check_Dma_Error_And_Clear ( Err_Code : in out Error_Codes) is
        Dma_Info_Ctl : LW_PRGNLCL_FALCON_DMAINFO_CTL_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_DMAINFO_CTL_Address,
                      Data => Dma_Info_Ctl);
        if Dma_Info_Ctl.Intr_Err_Completion = LW_PRGNLCL_FALCON_DMAINFO_CTL_INTR_ERR_COMPLETION_TRUE then
            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_FALCON_DMAINFO_CTL_Address,
                          Data => LW_PRGNLCL_FALCON_DMAINFO_CTL_Register'(
                              Clr_Fbrd            => FBRD_FALSE,
                              Clr_Fbwr            => FBWR_FALSE,
                              Intr_Err_Completion => LW_PRGNLCL_FALCON_DMAINFO_CTL_INTR_ERR_COMPLETION_CLR,
                              Intr_Err_Dmatype    => DMATYPE_NORMAL,
                              Intr_Err_Dmaread    => DMAREAD_FALSE
                             ));
            Err_Code := DMA_NACK_ERROR;
        end if;
    end Check_Dma_Error_And_Clear;

    procedure Wait_For_Free_Entry is
        Trf_Cmd  : LW_PRGNLCL_FALCON_DMATRFCMD_Register;
        Timer    : LwU32 := 0;
    begin
        Operation_Done :
        loop
            Csb_Reg_Rd32 ( Addr => LW_PRGNLCL_FALCON_DMATRFCMD_Address,
                           Data => Trf_Cmd);
            exit Operation_Done when Trf_Cmd.Full = FULL_FALSE;

            if Timer > DMA_MAX_TIMEOUT then
                Rv_Boot_Plugin_Error_Handling.Log_Info (Info_Code => DMA_WAIT_FOR_IDLE_HANG);
            else
                Timer := Timer + 1;
            end if;
        end loop Operation_Done;

    end Wait_For_Free_Entry;


end Rv_Brom_Dma;
