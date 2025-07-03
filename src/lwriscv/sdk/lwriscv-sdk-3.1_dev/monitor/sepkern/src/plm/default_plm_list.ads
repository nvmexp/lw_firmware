-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with PLM_Types; use PLM_Types;

package PLM_List
is

    -- default empty list, this list is for dummy build and builds that do not
    -- require programming plm registers.
    -- for all applications that do need to setup plm registers, configure
    -- correct plm list path first.

    PLM_AD_Pair : constant PLM_Array_Type (1 .. 0) := (
        others => Null_PLM_Pair
    );

end PLM_List;
