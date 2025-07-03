-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;                use Lw_Types;
with Policy_Types;            use Policy_Types;
with Iopmp_Policy_Types;      use Iopmp_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Policy;                  use Policy;
with Policy_External;         use Policy_External;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;             use Dev_Prgnlcl;

with Ada.Unchecked_Colwersion;

package Policy_External_To_Internal_Colwersions
is

    -- Colwersions for Core PMP Address
    function PMP_Address_To_LW_RISCV_CSR_PMPADDR_Register is new Ada.Unchecked_Colwersion
        (Source => PMP_Address,
         Target => LW_RISCV_CSR_PMPADDR_Register);

    function PMP_Address_To_LW_RISCV_CSR_MEXTPMPADDR_Register is new Ada.Unchecked_Colwersion
        (Source => PMP_Address,
         Target => LW_RISCV_CSR_MEXTPMPADDR_Register);

    -- Colwersions for Core PMP Entry - 8
    function PMP_Address_Mode_To_PMPCFG2_PMP8A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP8A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP8X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP8X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP8W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP8W_Field);
    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP8R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP8R_Field);

    -- Colwersions for Core PMP Entry - 9
    function PMP_Address_Mode_To_PMPCFG2_PMP9A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP9A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP9X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP9X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP9W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP9W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP9R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP9R_Field);

    -- Colwersions for Core PMP Entry - 10
    function PMP_Address_Mode_To_PMPCFG2_PMP10A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP10A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP10X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP10X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP10W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP10W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP10R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP10R_Field);

    -- Colwersions for Core PMP Entry - 11
    function PMP_Address_Mode_To_PMPCFG2_PMP11A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP11A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP11X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP11X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP11W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP11W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP11R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP11R_Field);

    -- Colwersions for Core PMP Entry - 12
    function PMP_Address_Mode_To_PMPCFG2_PMP12A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP12A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP12X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP12X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP12W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP12W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP12R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP12R_Field);

    -- Colwersions for Core PMP Entry - 13
    function PMP_Address_Mode_To_PMPCFG2_PMP13A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
        Target => LW_RISCV_CSR_PMPCFG2_PMP13A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP13X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP13X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP13W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP13W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP13R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP13R_Field);

    -- Colwersions for Core PMP Entry - 14
    function PMP_Address_Mode_To_PMPCFG2_PMP14A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP14A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP14X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP14X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP14W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP14W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP14R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP14R_Field);

    -- Colwersions for Core PMP Entry - 15
    function PMP_Address_Mode_To_PMPCFG2_PMP15A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_PMPCFG2_PMP15A_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP15X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP15X_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP15W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP15W_Field);

    function PMP_Access_To_LW_RISCV_CSR_PMPCFG2_PMP15R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_PMPCFG2_PMP15R_Field);

    -- Colwersions for Core PMP Entry - 16
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP16A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP16A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP16X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP16X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP16W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP16W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP16R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP16R_Field);

    -- Colwersions for Core PMP Entry - 17
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP17A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP17A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP17X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP17X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP17W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP17W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP17R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP17R_Field);

    -- Colwersions for Core PMP Entry - 18
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP18A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP18A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP18X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP18X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP18W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP18W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP18R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP18R_Field);

    -- Colwersions for Core PMP Entry - 19
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP19A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP19A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP19X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP19X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP19W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP19W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP19R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP19R_Field);

    -- Colwersions for Core PMP Entry - 20
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP20A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP20A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP20X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP20X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP20W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP20W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP20R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP20R_Field);

    -- Colwersions for Core PMP Entry - 21
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP21A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP21A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP21X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP21X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP21W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP21W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP21R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP21R_Field);

    -- Colwersions for Core PMP Entry - 22
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP22A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP22A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP22X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP22X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP22W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP22W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP22R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP22R_Field);

    -- Colwersions for Core PMP Entry - 23
    function PMP_Address_Mode_To_MEXTPMPCFG0_PMP23A_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Address_Mode,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP23A_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP23X_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP23X_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP23W_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP23W_Field);

    function PMP_Access_To_LW_RISCV_CSR_MEXTPMPCFG0_PMP23R_Field is new Ada.Unchecked_Colwersion
        (Source => PMP_Access,
         Target => LW_RISCV_CSR_MEXTPMPCFG0_PMP23R_Field);

    -- Colwersions for IOPMP
    function Io_Pmp_Address_Mode_To_LwU2 is new Ada.Unchecked_Colwersion
        (Source => Io_Pmp_Address_Mode,
         Target => LwU2);

    function Plcy_Io_Pmp_Cfg_To_LW_PRGNLCL_RISCV_IOPMP_CFG_Register is new Ada.Unchecked_Colwersion
        (Source => Plcy_Io_Pmp_Cfg,
         Target => LW_PRGNLCL_RISCV_IOPMP_CFG_Register);

    -- Colwersions for Device Map
    function Plcy_Device_Map_Group_0_To_LwU32 is new Ada.Unchecked_Colwersion
        (Source => Plcy_Device_Map_Group_0,
         Target => LwU32);

    function Plcy_Device_Map_Group_1_To_LwU32 is new Ada.Unchecked_Colwersion
        (Source => Plcy_Device_Map_Group_1,
         Target => LwU32);

    function Plcy_Device_Map_Group_2_To_LwU32 is new Ada.Unchecked_Colwersion
        (Source => Plcy_Device_Map_Group_2,
         Target => LwU32);

    function LW_PRGNLCL_RISCV_DEVICEMAP_SUBMMODE_Register_To_Plcy_Device_Map_Group_2 is new Ada.Unchecked_Colwersion
        (Source => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_Register,
         Target => Plcy_Device_Map_Group_2);

    function LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_0 is new Ada.Unchecked_Colwersion
        (Source => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register,
         Target => Plcy_Device_Map_Group_0);

    function LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_1 is new Ada.Unchecked_Colwersion
        (Source => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register,
         Target => Plcy_Device_Map_Group_1);

    function LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_2 is new Ada.Unchecked_Colwersion
        (Source => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register,
         Target => Plcy_Device_Map_Group_2);

    function Plcy_Device_Map_Group_3_To_LwU32 is new Ada.Unchecked_Colwersion
        (Source => Plcy_Device_Map_Group_3,
         Target => LwU32);

end Policy_External_To_Internal_Colwersions;
