-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package Types
is

    -- Partition count is the compile-time input parameter
    PARTITION_COUNT  : constant := $PARTITION_COUNT;
    PARTITION_MAX_ID : constant := PARTITION_COUNT - 1;

    -- Making Partition_ID a subtype of LwU6 ensures the absolute maximum is 64 partitions
    subtype Partition_ID is LwU6 range 0 .. PARTITION_MAX_ID;

    type Csr_Addr is new LwU12;

end Types;