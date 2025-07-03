with Ucode_Specific_Constants; use Ucode_Specific_Constants;
package VCAST_Dump_Coverage
is
   procedure Dump_Coverage_Data
     with
       Export => True,
       Convention => C,
       Linker_Section => LINKER_SECTION_NAME;

   procedure pubDumpCoverageData
     with
       Import => True,
       Convention => C,
       Global => null,
       External_Name => "pubDumpCoverageData";
end VCAST_Dump_Coverage;
