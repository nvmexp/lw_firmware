with Ada.Unchecked_Colwersion;

separate (Rv_Brom_Dma)

package body Fb_If is

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_ACHK_CTL_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_CTL_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_REGIONCFG_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_THROTTLE_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_TRANSCFG_Register);

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFBASE_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFBASE1_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFFBOFFS_Register);

    procedure Set_Transcfg (Target : LwU2; Mem_Type : LwU1) with Inline_Always;

    procedure Set_Nack_As_Ack with Inline_Always;

    procedure Initialize is
    begin
        -- The CPUCTL.SRESET won't reset FB which could leave it in dirty state, so we have to re-init them to default values.
         Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FBIF_ACHK_CTL_0_Address,
                        Data => LW_PRGNLCL_FBIF_ACHK_CTL_Register'(Size     => 0,
                                                                   Ctl_Type => TYPE_INSIDE,
                                                                   State    => STATE_DISABLE,
                                                                   Count    => 0));

        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FBIF_ACHK_CTL_1_Address,
                       Data => LW_PRGNLCL_FBIF_ACHK_CTL_Register'(Size     => 0,
                                                                  Ctl_Type => TYPE_INSIDE,
                                                                  State    => STATE_DISABLE,
                                                                  Count    => 0));

        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FBIF_THROTTLE_Address,
                       Data => LW_PRGNLCL_FBIF_THROTTLE_Register'(Bucket_Size => LW_PRGNLCL_FBIF_THROTTLE_BUCKET_SIZE_INIT,
                                                                  Leak_Count  => LW_PRGNLCL_FBIF_THROTTLE_LEAK_COUNT_DISABLE,
                                                                  Leak_Size   => SIZE_16B));

        -- BROM needs to configure the following registers in order to use the FB correctly.
        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_FBIF_CTL_Address,
                      Data => LW_PRGNLCL_FBIF_CTL_Register'(
                          Flush             => FLUSH_CLEAR,
                          Ilwal_Context     => CONTEXT_CLEAR,
                          Clr_Bwcount       => BWCOUNT_CLEAR,
                          Enable            => LW_PRGNLCL_FBIF_CTL_ENABLE_TRUE, -- Enable FBIF to accept requests from client
                          Clr_Idlewderr     => IDLEWDERR_CLEAR,
                          Reset             => RESET_CLEAR,
                          Allow_Phys_No_Ctx => LW_PRGNLCL_FBIF_CTL_ALLOW_PHYS_NO_CTX_ALLOW, -- Allow issue of physical requests if no instance.
                          Idle              => IDLE_FALSE,
                          Idlewderr         => IDLEWDERR_FALSE,
                          Srtout            => SRTOUT_FALSE,
                          Clr_Srtout        => SRTOUT_CLEAR,
                          Cc_Mode           => MODE_FALSE,
                          Srtoval           => LW_PRGNLCL_FBIF_CTL_SRTOVAL_INIT)
                     );

        Set_Nack_As_Ack;
    end Initialize;

    procedure Finalize is
    begin
        -- SW requires explicitly we leave the state as
        -- NO_CTX = true, REQUIRE_CTX = false, NACK_AS_ACK = true
        -- This is exactly what we have configured in Initialize procedure.

        -- Restore REGIONCFG and TRANSCFG
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FBIF_REGIONCFG_Address,
                       Data => LW_PRGNLCL_FBIF_REGIONCFG_Register'(T0 => 0,
                                                                   T1 => 0,
                                                                   T2 => 0,
                                                                   T3 => 0,
                                                                   T4 => 0,
                                                                   T5 => 0,
                                                                   T6 => 0,
                                                                   T7 => 0));
        Set_Transcfg (Target   => LW_PRGNLCL_FBIF_TRANSCFG_TARGET_INIT,
                      Mem_Type => LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_INIT);
        Set_Fb_Addr (Fb_Addr => 0);
    end Finalize;

    -- FIXME ghlit1_lwdec might enable the nack_as_ack feature in the future.
    procedure Set_Nack_As_Ack is
    begin
        null;
    end Set_Nack_As_Ack;

    procedure Set_Config (Cfg : Config) is
    begin
        -- REGIONCFG is designed for configuring the region that will be selected by given CTXDMAx
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FBIF_REGIONCFG_Address,
                       Data => LW_PRGNLCL_FBIF_REGIONCFG_Register'(T0 => Cfg.Wprid,
                                                                   T1 => 0,
                                                                   T2 => 0,
                                                                   T3 => 0,
                                                                   T4 => 0,
                                                                   T5 => 0,
                                                                   T6 => 0,
                                                                   T7 => 0));
        Set_Transcfg (Target   => Cfg.Target,
                      Mem_Type => LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_PHYSICAL);
    end Set_Config;

    procedure Set_Transcfg (Target : LwU2; Mem_Type : LwU1) is
        Fbif_Transcfg0 : constant LW_PRGNLCL_FBIF_TRANSCFG_Register :=
                           (Target         => Target,   -- Set target as specified
                            Mem_Type       => Mem_Type, -- Set memory type
                            L2c_Wr         => LW_PRGNLCL_FBIF_TRANSCFG_L2C_WR_INIT,
                            L2c_Rd         => LW_PRGNLCL_FBIF_TRANSCFG_L2C_RD_INIT,
                            Wachk0         => LW_PRGNLCL_FBIF_TRANSCFG_WACHK0_INIT,
                            Wachk1         => LW_PRGNLCL_FBIF_TRANSCFG_WACHK1_INIT,
                            Rachk0         => LW_PRGNLCL_FBIF_TRANSCFG_RACHK0_INIT,
                            Rachk1         => LW_PRGNLCL_FBIF_TRANSCFG_RACHK1_INIT,
                            Engine_Id_Flag => FLAG_BAR2_FN0);

    begin
        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_FBIF_TRANSCFG_0_Address,
                      Data => Fbif_Transcfg0);
    end Set_Transcfg;

    procedure Set_Fb_Addr (Fb_Addr : LwU64) is
        -- Global memory offset = FALCON_DMATRFBASE1[8:0]<<40 +  FALCON_DMATRFBASE[31:0]<<8 + FALCON_DMATRFFBOFFS[31:0]
        -- FB Address will be stored separately in {DMATRFBASE1,DMATRFBASE0,DMATRFFBOFFs}
        -- 63       57|56       40|39             8|7       0
        --  =================================================
        -- | Reserved |   Base1   |     Base0      | Offset |
        -- |------------------------------------------------|
        -- |   7bit   |   17bit   |     32bits     |  8bit  |
        --  =================================================
        type Trf_Fb_Addr is record
            Offset : LwU8;
            Base0  : LwU32;
            Base1  : LwU17;
            Rsvd   : LwU7;
        end record with Size => 64, Object_Size => 64;

        for Trf_Fb_Addr use record
            Offset at 0 range 0 .. 7;
            Base0  at 0 range 8 .. 39;
            Base1  at 0 range 40 .. 56;
            Rsvd   at 0 range 57 .. 63;
        end record;

        function LwU64_To_Trf_Fb_Addr is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                                       Target => Trf_Fb_Addr);
        Addr : constant Trf_Fb_Addr := LwU64_To_Trf_Fb_Addr (Fb_Addr);
    begin
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFBASE1_Address,
                       Data => LW_PRGNLCL_FALCON_DMATRFBASE1_Register'( Base => Addr.Base1));

        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFBASE_Address,
                       Data => LW_PRGNLCL_FALCON_DMATRFBASE_Register'(Base => Addr.Base0));

        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_DMATRFFBOFFS_Address,
                       Data => LW_PRGNLCL_FALCON_DMATRFFBOFFS_Register'(Offs => LwU32(Addr.Offset)));

    end Set_Fb_Addr;
end Fb_If;
