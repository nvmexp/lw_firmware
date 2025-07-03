pragma Warnings (Off);
pragma Ada_95;
pragma Source_File_Name (ada_main, Spec_File_Name => "b__fub_main.ads");
pragma Source_File_Name (ada_main, Body_File_Name => "b__fub_main.adb");
pragma Suppress (Overflow_Check);

package body ada_main is



   procedure adainit is
   begin
      null;

   end adainit;

   procedure Ada_Main_Program;
   pragma Import (Ada, Ada_Main_Program, "_ada_fub_main");

   function main return Integer is
   begin
      adainit;
      Ada_Main_Program;
      return (gnat_exit_status);
   end;

--  BEGIN Object file/option list
   --   -LE:\P4\sw\dev\gpu_drv\chips_a\uproc\Spark\ucode\fub\src\c_src\
   --   -LE:\P4\sw\dev\gpu_drv\chips_a\uproc\Spark\ucode\fub\src\c_src\
   --   -LG:/adacore_app/ccg/libexec/gnat_ccg/lib/gcc/i686-pc-mingw32/8.3.1/rts-ccg\adalib\
   --   -static
   --   -lgnat
--  END Object file/option list   

end ada_main;
