--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Hw_State_Check.DMA;   use Hw_State_Check.DMA;
with Hw_State_Check.IOPMP; use Hw_State_Check.IOPMP;
with Hw_State_Check.SHA;   use Hw_State_Check.SHA;
with Hw_State_Check.GDMA;  use Hw_State_Check.GDMA;
with Hw_State_Check.SCP;   use Hw_State_Check.SCP;

package body Hw_State_Check
is
    procedure Check_Hw_Engine_State (Err_Code : in out Error_Codes)
    is
    begin
        Check_DMA (Err_Code);
        if Err_Code /= OK then
            return;
        end if;

        Check_IOPMP (Err_Code);
        if Err_Code /= OK then
            return;
        end if;

        Check_SHA (Err_Code);
        if Err_Code /= OK then
            return;
        end if;

        Check_GDMA (Err_Code);
        if Err_Code /= OK then
            return;
        end if;

        Check_SCP (Err_Code);

    end Check_Hw_Engine_State;

end Hw_State_Check;
