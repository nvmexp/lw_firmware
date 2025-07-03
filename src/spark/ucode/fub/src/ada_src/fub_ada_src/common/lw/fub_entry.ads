package Fub_Entry
with SPARK_Mode => On
is

   procedure Fub_Entry_Wrapper
     with
       Export => True,
       Convention => C,
       External_Name => "fub_entry_wrapper";

end Fub_Entry;
