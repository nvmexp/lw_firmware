with Sec_Entry; use Sec_Entry;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Fub_Post_Codes; use Fub_Post_Codes;
with Tu10x.Hal; use Tu10x.Hal;
--with Plm_Lowering; use Plm_Lowering;

package body Fub_Entry
with SPARK_Mode => On
is

   procedure Fub_Entry_Wrapper
   is
      Status : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
   begin
      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         Hs_Entry_Inline(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Hs_Entry_NonInline(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         --  Lower_Plms;
         Fub_Report_Status(Status => Fub_Ok);

      end loop Main_Block;

      Hs_Entry_Cleanup(Status);
   end Fub_Entry_Wrapper;


end Fub_Entry;
