-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Falcon_V4; use Dev_Falcon_V4;
with Dev_Riscv_Pri; use Dev_Riscv_Pri;
with Dev_Pwr_Pri; use Dev_Pwr_Pri;

package Constants is

   ENGINE_BASE_FALCON : constant := LW_FALCON_PWR_BASE;
   ENGINE_BASE_RISCV  : constant := LW_FALCON2_PWR_BASE;
   ENGINE_BASE_FBIF   : constant := Lw_Ppwr_Fbif_Transcfg;

end Constants;
