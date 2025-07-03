-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Defines Types constants specific to a ucode
--
--  @description
--  Ucode_Specific_Constants package defines linker section name for PUB

package Ucode_Specific_Constants is

   LINKER_SECTION_NAME : constant String := ".imem_pub";

end Ucode_Specific_Constants;
