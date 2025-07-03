-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Definition of type system.
--
--  @description
--  Lw_Types.Falcon_Types is a child package which defines all types that are dependent on Falcon HW.
package Lw_Types.Falcon_Types
with SPARK_Mode => On
is

   --  FALCON IMEM size is 2^16 bytes.
   FALCON_IMEM_OFFSET_BYTES_LEN_IN_BITS              : constant := 16;

   MAX_FALCON_IMEM_SIZE_BYTES                        : constant LwU32 := 2 ** FALCON_IMEM_OFFSET_BYTES_LEN_IN_BITS;
   MAX_FALCON_IMEM_OFFSET_BYTES_FOR_LAST_DWORD_DATUM : constant  LwU32 := ( MAX_FALCON_IMEM_SIZE_BYTES - 4 );

   MAX_NUM_OF_IMEM_TAGS                              : constant  LwU16 := LwU16( MAX_FALCON_IMEM_SIZE_BYTES/256 );
   MAX_FALCON_IMEM_TAG_INDEX                         : constant  LwU16 := ( MAX_NUM_OF_IMEM_TAGS - 1 );

   -- FALCON DMEM size is 2^16 bytes.
   FALCON_DMEM_OFFSET_BYTES_LEN_IN_BITS              : constant := 16;

   MAX_FALCON_DMEM_SIZE_BYTES                        : constant LwU32 := 2 ** FALCON_DMEM_OFFSET_BYTES_LEN_IN_BITS;
   MAX_FALCON_DMEM_OFFSET_BYTES                      : constant LwU32 := ( MAX_FALCON_DMEM_SIZE_BYTES - 4 );

   FALCON_CSW_EXCEPTION_BIT                          : constant := 24;
   FALCON_CSW_INTERRUPT0_EN_BIT                      : constant := 16;
   FALCON_CSW_INTERRUPT1_EN_BIT                      : constant := 17;
   FALCON_CSW_INTERRUPT2_EN_BIT                      : constant := 18;

   --  UCODE_IMEM_TAG: Used for variables to denote Tag value in Falcon IMEM.
   type UCODE_IMEM_TAG is new LwU16 range 0 .. MAX_FALCON_IMEM_TAG_INDEX;

   --  UCODE_IMEM_OFFSET_IN_FALCON_BYTES: Used for variables to denote Falcon IMEM offset in bytes.
   type UCODE_IMEM_OFFSET_IN_FALCON_BYTES  is new LwU32 range 0 .. MAX_FALCON_IMEM_OFFSET_BYTES_FOR_LAST_DWORD_DATUM
     with Size => 32;

   --  UCODE_IMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES: Used for variables to denote dword aligned Falcon IMEM offset in
   --  bytes.
   subtype UCODE_IMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES is UCODE_IMEM_OFFSET_IN_FALCON_BYTES
     with Dynamic_Predicate => UCODE_IMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES mod 4 = 0;

   --  UCODE_DMEM_OFFSET_IN_FALCON_BYTES: Used for variables to denote Falcon DMEM offset in bytes.
   type UCODE_DMEM_OFFSET_IN_FALCON_BYTES  is new LwU32 range 0 .. MAX_FALCON_DMEM_OFFSET_BYTES  with Size => 32;

   --  UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES: Used for variables to denote dword aligned Falcon DMEM offset in
   --  bytes.
   subtype UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES is UCODE_DMEM_OFFSET_IN_FALCON_BYTES
     with Dynamic_Predicate => UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES mod 4 = 0;


   --  UCODE_IMEM_SIZE_IN_FALCON_BYTES: Used for variables to denote size of data blob in Falcon IMEM offset in bytes.
   type UCODE_IMEM_SIZE_IN_FALCON_BYTES is new LwU32 range 0 .. MAX_FALCON_IMEM_SIZE_BYTES  with Size => 32;

   --  UCODE_IMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES: Used for variables to denote dword aligned size of code in
   --  Falcon IMEM in bytes.
   subtype UCODE_IMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES is UCODE_IMEM_SIZE_IN_FALCON_BYTES
     with Dynamic_Predicate => UCODE_IMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES mod 4 = 0;

   --  UCODE_DMEM_SIZE_IN_FALCON_BYTES: Used for variables to denote size of data blob in Falcon DMEM offset in bytes.
   type UCODE_DMEM_SIZE_IN_FALCON_BYTES is new LwU32 range 0 .. MAX_FALCON_DMEM_SIZE_BYTES  with Size => 32;

   --  UCODE_DMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES: Used for variables to denote dword aligned size of data blob in
   --  Falcon IMEM offset in bytes.
   subtype UCODE_DMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES is UCODE_DMEM_SIZE_IN_FALCON_BYTES
     with Dynamic_Predicate => UCODE_DMEM_SIZE_IN_FALCON_4B_ALIGNED_BYTES mod 4 = 0;

   --  UCODE_DESC_DMEM_OFFSET: Dedicated data type used to store UCODE_DMEM_OFFSET relative to UCODE IMEM start.
   -- Since DMEM data immediately coincides with IMEM data, its range cannot exceed IMEM size.
   type UCODE_DESC_DMEM_OFFSET is new LwU32 range 0 .. MAX_FALCON_IMEM_SIZE_BYTES  with Size => 32;

   --  UCODE_DESC_DMEM_4B_ALIGNED_OFFSET: Dedicated type used to store a DWORD representing offset of Ucode DMEM data
   --  start relative to Ucode IMEM data start. Ucode DMEM data immediately follows IMEM data so MAX value of this
   --  type would be IMEM Size.
   subtype UCODE_DESC_DMEM_4B_ALIGNED_OFFSET is UCODE_DESC_DMEM_OFFSET
     with Dynamic_Predicate => UCODE_DESC_DMEM_4B_ALIGNED_OFFSET mod 4 = 0;

   type LW_FLCN_IMTAG_BLK_REGISTER is record
      F_Blk_Valid   : LwU1;
      F_Blk_Pending : LwU1;
      F_Blk_Selwre  : LwU1;
   end record
     with Size => 32, Object_Size => 32;

   for LW_FLCN_IMTAG_BLK_REGISTER use record
      F_Blk_Valid   at 0 range 24 .. 24;
      F_Blk_Pending at 0 range 25 .. 25;
      F_Blk_Selwre  at 0 range 26 .. 26;
   end record;
end Lw_Types.Falcon_Types;
