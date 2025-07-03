--  with TEXT_IO; use TEXT_IO;
with Ada.Unchecked_Colwersion;
with System;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
-- with Utility;use Utility;

package body Falcon_Io is

   pragma Assert (System.Address'Size = LwU32'Size);
   function UC_SYSTEM_ADDRESS is new Ada.Unchecked_Colwersion (Source => System.Address,
                                                               Target => LwU32);

   type Arr_Char_Idx_LwU8_Max_Digits is array (LwU8 range 1 .. 10) of Character; -- 2**32 - 1 fits in 10 digits
   IO_BUFFER_LEN         : constant := 8192;

  procedure Get_Falcon_IO_buffer_Addr( Falcon_IO_buffer_Addr : out LwU32 )
  is
      Falcon_IO_buffer : LwU32
       with
       Import     => True,
       Convention => C,
       External_Name => "falcon_io__io_buffer";
  begin
      Falcon_IO_buffer_Addr := UC_SYSTEM_ADDRESS( Falcon_IO_buffer'Address );
   end Get_Falcon_IO_buffer_Addr;


   pragma Linker_Section(Get_Falcon_IO_buffer_Addr, LINKER_SECTION_NAME);

   procedure Write_To_Vcast_Buff( Data  : Character;
                                  Index : LwU32;
                                  Addr  : LwU32)
   with SPARK_Mode => Off
   is
     Vcast_Buff_At_Input_Idx : Character
       with Address => System'To_Address(Addr + Index), Volatile;
   begin
     Vcast_Buff_At_Input_Idx := Data;
   end Write_To_Vcast_Buff;
   pragma Linker_Section(Write_To_Vcast_Buff, LINKER_SECTION_NAME);
   Next_Entry_Idx_In_Buf : LwU32 := 0;

   IS_OPEN     : Boolean := False;
--     OUTPUT_FILE : TEXT_IO.FILE_TYPE;

   procedure Flush_Io_Buffer;

   ---------------------------------------------------------------------------
   -- The following section contains utility functions                      --
   ---------------------------------------------------------------------------

   procedure LwU32_To_Char_Array(Number        : LwU32;
                                 Arr           : out Arr_Char_Idx_LwU8_Max_Digits;
                                 Num_Of_Digits : out LwU8)
   is
      Number_Local : LwU32;
      Char_As_LwU8 : LwU8;
      function To_Char is new Ada.Unchecked_Colwersion (Source => LwU8,
                                                        Target => Character);
   begin
      if Number = 0 then
         -- 0 is special case. It has one digit
         Num_Of_Digits := 1;
         Arr(1) := '0';
         return;
      end if;

      Num_Of_Digits := 0;
      Number_Local := Number;
      while Number_Local /= 0 loop
         Num_Of_Digits := Num_Of_Digits + 1;
         Number_Local := Number_Local / 10;
      end loop;

      Number_Local := Number;
      for I in reverse 1 .. Num_Of_Digits loop
         Char_As_LwU8 := LwU8(Number_Local mod 10) + Character'Pos('0');
         Arr(I) := To_Char(Char_As_LwU8);
         Number_Local := Number_Local / 10;
      end loop;

   end LwU32_To_Char_Array;
   pragma Linker_Section(LwU32_To_Char_Array, LINKER_SECTION_NAME);
   ---------------------------------------------------------------------------
   -- The following section contains FILE IO which can not be used on bare  --
   -- metal and needs to be replaced with write to BSI RAM or similar or    --
   -- DMA to FB or system memory                                            --
   ---------------------------------------------------------------------------
   procedure OPEN_FILE is
   begin
      if not IS_OPEN then
--           TEXT_IO.CREATE (OUTPUT_FILE, TEXT_IO.OUT_FILE, "Ada.DAT");
         IS_OPEN         := True;
      end if;
   end OPEN_FILE;

   ---------------------------------------------------------------------------
   procedure CLOSE_FILE is
   begin
      if Next_Entry_Idx_In_Buf > 0 then
         Flush_Io_Buffer;
--           TEXT_IO.CLOSE ( OUTPUT_FILE );
         IS_OPEN := False;
      end if;

   end CLOSE_FILE;
   ---------------------------------------------------------------------------
   -- The following section contains generic functions used to manage the   --
   -- internal buffer in falcon that caches the data to be flushed          --
   ---------------------------------------------------------------------------
   procedure Flush_Io_Buffer is
      Max_Idx : LwU32;
   begin
      if Next_Entry_Idx_In_Buf = 0 then
         -- There is nothing in the buffer
         return;
      end if;

      -- Open the output stream
      OPEN_FILE;

      -- The last index to be flushed is one less than "next"
      Max_Idx := Next_Entry_Idx_In_Buf - 1;

      for I in 0 .. Max_Idx loop
         --           TEXT_IO.PUT(OUTPUT_FILE, Io_Buffer(I));
         null;
      end loop;

      -- Reset the next entry index
      Next_Entry_Idx_In_Buf := 0;
   end Flush_Io_Buffer;
   pragma Linker_Section(Flush_Io_Buffer, LINKER_SECTION_NAME);

   ------------------------------------------------------
   procedure Insert_Char_In_Io_Buffer (Chr : Character) is
      Falcon_io_addr : LwU32;
   begin
      if Next_Entry_Idx_In_Buf = IO_BUFFER_LEN then
         Flush_Io_Buffer;
      end if;
      Get_Falcon_IO_buffer_Addr( Falcon_IO_buffer_Addr => Falcon_io_addr );
      Write_To_Vcast_Buff( Data  => Chr,
                           Index => Next_Entry_Idx_In_Buf,
                           Addr  => Falcon_io_addr);
      Next_Entry_Idx_In_Buf := Next_Entry_Idx_In_Buf + 1;
   end Insert_Char_In_Io_Buffer;
   pragma Linker_Section(Insert_Char_In_Io_Buffer, LINKER_SECTION_NAME);
   ------------------------------------------------------
   procedure Insert_LwU32_In_Io_Buffer (Number : LwU32) is
      Arr_For_Num_As_Str : Arr_Char_Idx_LwU8_Max_Digits;
      Num_Of_Digits      : LwU8;
   begin
      LwU32_To_Char_Array(Number        => Number,
                          Arr           => Arr_For_Num_As_Str,
                          Num_Of_Digits => Num_Of_Digits);

      for I in 1 .. Num_Of_Digits loop
         Insert_Char_In_Io_Buffer(Arr_For_Num_As_Str(I));
      end loop;

   end Insert_LwU32_In_Io_Buffer;
      pragma Linker_Section(Insert_LwU32_In_Io_Buffer, LINKER_SECTION_NAME);
   ------------------------------------------------------
   procedure Insert_Space_In_Io_Buffer is
   begin
      Insert_Char_In_Io_Buffer(' ');
   end Insert_Space_In_Io_Buffer;
   pragma Linker_Section(Insert_Space_In_Io_Buffer, LINKER_SECTION_NAME);
   ------------------------------------------------------
   procedure Insert_Newline_In_Io_Buffer is
   begin
      Insert_Char_In_Io_Buffer(Character'Val(10));
   end Insert_Newline_In_Io_Buffer;
   pragma Linker_Section(Insert_Newline_In_Io_Buffer, LINKER_SECTION_NAME);
   ---------------------------------------------------------------------------
   -- The following section contains functions exported to vcast_cover_io   --
   ---------------------------------------------------------------------------
   procedure WRITE_COVERAGE_LINE (S : String) is
   begin
      for I in S'Range loop
         Insert_Char_In_Io_Buffer(S(I));
      end loop;
      Insert_Newline_In_Io_Buffer;
   end WRITE_COVERAGE_LINE;
   pragma Linker_Section(WRITE_COVERAGE_LINE, LINKER_SECTION_NAME);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer) is
   begin
      Insert_LwU32_In_Io_Buffer(LwU32(V1));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V2));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V3));
      Insert_Newline_In_Io_Buffer;
   end WRITE_COVERAGE_LINE;
   pragma Linker_Section(WRITE_COVERAGE_LINE, LINKER_SECTION_NAME);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Character) is
   begin
      Insert_LwU32_In_Io_Buffer(LwU32(V1));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V2));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V3));
      Insert_Space_In_Io_Buffer;

      Insert_Char_In_Io_Buffer(V4);
      Insert_Newline_In_Io_Buffer;
   end WRITE_COVERAGE_LINE;
   pragma Linker_Section(WRITE_COVERAGE_LINE, LINKER_SECTION_NAME);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Character;
                                  V5 : Character) is
   begin
      Insert_LwU32_In_Io_Buffer(LwU32(V1));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V2));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V3));
      Insert_Space_In_Io_Buffer;

      Insert_Char_In_Io_Buffer(V4);
      -- No space

      Insert_Char_In_Io_Buffer(V5);
      Insert_Newline_In_Io_Buffer;
   end WRITE_COVERAGE_LINE;
   pragma Linker_Section(WRITE_COVERAGE_LINE, LINKER_SECTION_NAME);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Unsigned_32;
                                  V5 : Unsigned_32) is
   begin
--        WRITE_TO_INST_FILE(INTEGER_TIC_IMAGE(V1)     & " " &
--                           INTEGER_TIC_IMAGE(V2)     & " " &
--                           INTEGER_TIC_IMAGE(V3)     & -- No extra space but 'Image will prepend one anyway
--                           UNSIGNED_32_TIC_IMAGE(V4) & -- No extra space but 'Image will prepend one anyway
--                           UNSIGNED_32_TIC_IMAGE(V5));
      Insert_LwU32_In_Io_Buffer(LwU32(V1));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V2));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(LwU32(V3));
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(V4);
      Insert_Space_In_Io_Buffer;

      Insert_LwU32_In_Io_Buffer(V5);
      Insert_Newline_In_Io_Buffer;
   end WRITE_COVERAGE_LINE;
   pragma Linker_Section(WRITE_COVERAGE_LINE, LINKER_SECTION_NAME);
end Falcon_Io;
