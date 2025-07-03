--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Policy;             use Policy;

package Partition_Entry_Priv_Lockdown
is
    procedure Enforce_Priv_Lockdown (Partition_Policy : Policy.Policy)
    with
        Inline_Always;
end Partition_Entry_Priv_Lockdown;
