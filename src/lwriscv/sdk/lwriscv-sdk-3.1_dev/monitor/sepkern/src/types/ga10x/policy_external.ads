-- _LWRM_COPYRIGHT_BEGIN_
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
with Policy_External_Types;   use Policy_External_Types;
with Mpu_Policy_Types;        use Mpu_Policy_Types;
with Device_Map_Policy_Types; use Device_Map_Policy_Types;
with Dev_Riscv_Csr_64;        use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;             use Dev_Prgnlcl;

package Policy_External
is
    type External_Policy is new External_Policy_Type;
    type External_Policy_Array is array (Partition_ID) of External_Policy;

end Policy_External;
