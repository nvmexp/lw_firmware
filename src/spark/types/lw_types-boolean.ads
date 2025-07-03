-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Defining types
--
--  @description
--  Lw_Types.Boolean is a child package of Lw_Types which declares boolean type.

package Lw_Types.Boolean is

   type LwBool is (LW_FALSE, LW_TRUE);
   for LwBool'Size use 1;
   for LwBool use
      (
       LW_FALSE => 0,
       LW_TRUE => 1
      );

end Lw_Types.Boolean;
