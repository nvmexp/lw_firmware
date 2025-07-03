-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Hs_Wrapper; use Hs_Wrapper;

--  @summary
--  HS Wrapper Main
--
--  @description
--  This package contains dummy procedure to please SPARK for having a main . This
-- will be changed once we have NS in SPARK.

procedure Hs_Wrapper_Main
is
begin
   Hs_Wrapper_Entry;
end Hs_Wrapper_Main;

