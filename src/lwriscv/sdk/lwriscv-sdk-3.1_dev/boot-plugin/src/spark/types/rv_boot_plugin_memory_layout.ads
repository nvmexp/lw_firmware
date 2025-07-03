-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining constants corresponding to the general Peregrine memory layout.
-- this layout coupled with the Peregrine tightly so it is not subject to change over
-- different Bootrom variant and generations.

with Project;
with Rv_Brom_Memory_Layout; use Rv_Brom_Memory_Layout;

package Rv_Boot_Plugin_Memory_Layout is
    BOOT_PLUGIN_CODE_END         : constant := RISCV_IMEM_BYTESIZE;
    BOOT_PLUGIN_CODE_SIZE        : constant := 16#2000#;
    BOOT_PLUGIN_CODE_START       : constant := BOOT_PLUGIN_CODE_END - BOOT_PLUGIN_CODE_SIZE;
end Rv_Boot_Plugin_Memory_Layout;
