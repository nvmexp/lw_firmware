with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;
package Falcon_Io is
   subtype Unsigned_32 is LwU32;

   procedure Get_Falcon_IO_buffer_Addr( Falcon_IO_buffer_Addr : out LwU32 )
     with
       Global => null,
       Depends => ( Falcon_IO_buffer_Addr => null ),
       Inline_Always;

   procedure CLOSE_FILE with
      Convention    => C,
      Export        => True,
      External_Name => "close_ada_coverage_file";

   procedure WRITE_COVERAGE_LINE (S : String);


   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Character);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Character;
                                  V5 : Character);

   procedure WRITE_COVERAGE_LINE (V1 : Integer;
                                  V2 : Integer;
                                  V3 : Integer;
                                  V4 : Unsigned_32;
                                  V5 : Unsigned_32);

end Falcon_Io;
