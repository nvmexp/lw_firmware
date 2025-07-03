package VCAST_Ada_Options is
   function VCAST_APPEND_TO_TESTINSS return boolean; -- Unused function

   -- Replacing function with constant to ensure that CCG can not insert malloc (in response to "new" in Ada code)
   -- based on a run time check which will always be false. Usage of constant permits optimization
--     function VCAST_USE_STATIC_MEMORY return boolean;
   VCAST_USE_STATIC_MEMORY : constant boolean := TRUE;

   -- Replacing function with constant to avoid dynamic allocation
--     function VCAST_MAX_COVERED_SUBPROGRAMS return integer;
   VCAST_MAX_COVERED_SUBPROGRAMS : constant integer := 1000;
   function VCAST_MAX_FILES return integer;  -- Unused function

   -- Replacing function with constant to avoid dynamic allocation
--     function VCAST_MAX_MCDC_STATEMENTS return integer;
   VCAST_MAX_MCDC_STATEMENTS     : constant integer := 1;

   function VCAST_MAX_STRING_LENGTH return integer;  -- Unused function
   function VCAST_MAX_RANGE return integer;  -- Unused function
end VCAST_Ada_Options;
