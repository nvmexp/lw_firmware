-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Hs_Wrapper; use Hs_Wrapper;
with Ns_Section; use Ns_Section;

function Ns_Main return Integer
is
   pragma Suppress(All_Checks);

begin

   -- Setup BROM for HS exelwtion
   Ns_Entry;

   -- HS entry. This call does not return.
   Hs_Wrapper_Entry;

   return 0;
end Ns_Main;
