--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;                use Lw_Types;
with Types;                   use Types;
with Policy_Types;            use Policy_Types;
with Mpu_Policy_Types;        use Mpu_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;             use Dev_Prgnlcl;

package Policy_External_Types
is
    -- External Partition Policy types
    type External_Policy_Type is record
        Switchable_To       : aliased Plcy_Switchable_To_Array;
        Entry_Point_Address : aliased Plcy_Entry_Point_Addr_Type;
        Ucode_Id            : aliased Plcy_Ucode_Id_Type;
        Sspm                : aliased Plcy_SSPM;
        Secret_Mask         : aliased Plcy_Secret_Mask;
        Debug_Control       : aliased Plcy_Debug_Control;
        Mpu_Control         : aliased Plcy_Mpu_Control;
        Device_Map_Group_0  : aliased Plcy_Device_Map_Group_0;
        Device_Map_Group_1  : aliased Plcy_Device_Map_Group_1;
        Device_Map_Group_2  : aliased Plcy_Device_Map_Group_2;
        Device_Map_Group_3  : aliased Plcy_Device_Map_Group_3;
        Core_Pmp            : aliased Plcy_Core_Pmp;
        Io_Pmp              : aliased Plcy_Io_Pmp;
#if POLICY_CONFIG_SBI
        SBI_Access_Config   : aliased Plcy_SBI_Access;
#end if;
#if POLICY_CONFIG_PRIV_LOCKDOWN
        Priv_Lockdown       : aliased LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register;
#end if;
    end record;

end Policy_External_Types;
