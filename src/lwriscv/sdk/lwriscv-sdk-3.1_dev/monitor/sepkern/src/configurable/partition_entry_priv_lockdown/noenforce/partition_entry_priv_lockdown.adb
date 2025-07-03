--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Policy;             use Policy;

package body Partition_Entry_Priv_Lockdown
is
    procedure Enforce_Priv_Lockdown (Partition_Policy : Policy.Policy)
    is
    begin
        null;
    end Enforce_Priv_Lockdown;
end Partition_Entry_Priv_Lockdown;
