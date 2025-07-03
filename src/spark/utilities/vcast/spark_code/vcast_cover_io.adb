---------------------------------------
-- Copyright 2010 Vector Software    --
-- East Greenwich, Rhode Island USA --
---------------------------------------
--target=apex with ADA.COMMAND_LINE;
with Unchecked_Colwersion;
with VCAST_Ada_Options;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

package body VCAST_COVER_IO is

   ---------- COVERAGE CONSTANTS AND TYPES ----------
   -- WARNING: these numbers might need to change!!!!!
   AVL_SUBTYPE_LIMIT  : constant := 1_000;
   BITS_SUBTYPE_LIMIT : constant := 99_999;

   subtype VCAST_AVL_TYPE is VCAST_AVL_Tree_Array (0 .. AVL_SUBTYPE_LIMIT);
   type VCAST_AVL_TYPE_PTR is access VCAST_AVL_TYPE;

   subtype VCAST_BITS_TYPE is VCAST_BIT_ARRAY_TYPE (0..BITS_SUBTYPE_LIMIT);
   type VCAST_BITS_TYPE_PTR is access VCAST_BITS_TYPE;



--target=apex   function GET_PROCESS return vcast_string is
--target=apex      RET_VAL : constant string := ADA.COMMAND_LINE.COMMAND_NAME;
--target=apex   begin
--target=apex      for INDEX in reverse RET_VAL'first .. RET_VAL'last - 1 loop
--target=apex         if RET_VAL(INDEX) = '/' or else
--target=apex            RET_VAL(INDEX) = '\' or else
--target=apex            RET_VAL(INDEX) = ']' or else
--target=apex            RET_VAL(INDEX) = ':'
--target=apex         then
--target=apex            return "-" & RET_VAL(INDEX + 1 .. RET_VAL'last);
--target=apex         end if;
--target=apex      end loop;
--target=apex      return "-" & RET_VAL;
--target=apex   exception
--target=apex      when others =>
--target=apex         return "";
--target=apex   end GET_PROCESS;
--target=apex
--target=apex   function GETPID return vcast_string is
--target=apex      function C_GETPID return Integer;
--target=apex      pragma IMPORT ( C, C_GETPID, "getpid" );
--target=apex      PID : Integer;
--target=apex   begin
--target=apex      PID := C_GETPID;
--target=apex      declare
--target=apex         RETVAL : constant string := Integer'image ( PID );
--target=apex      begin
--target=apex         return "-" & RETVAL(RETVAL'first+1..RETVAL'last);
--target=apex      end;
--target=apex   exception
--target=apex      when others =>
--target=apex         return "";
--target=apex   end GETPID;

   ------------------------------------------------------

   ---------------------------------------------------------------------------
   -- Utility functions, used only by optimized coverage functions below.
   ---------------------------------------------------------------------------
   ---------------------------------------------------------------------------
   -- This function colwerts the bit array into an unsigned_32 number. We
   -- don't use UNCHECKED_COLWERSION because it doesn't work correctly on SOLARIS!
   function BIT_ARRAY_MCDC_TO_INT ( BITS : VCAST_BIT_ARRAY_MCDC_TYPE )
                                    return Unsigned_32 is
      RV  : Unsigned_32 := 0;
      PV  : Unsigned_32 := 1;
   begin
      for I in BITS'first .. BITS'last loop
         if BITS(I) then
            RV := RV + PV;
         end if;
         if PV >= UNSIGNED_32_MAX then
            return 0;
         end if;
         PV := PV * 2;
      end loop;
      return RV;
   end BIT_ARRAY_MCDC_TO_INT;

   --  Supporting subprograms
   ---------------------------------------------------------------------------
   function address_to_avl_node_ref is new
     Unchecked_Colwersion (System.Address, AVL_Node_Ref);

   function Get_Avl_Node return AVL_Node_Ref is
   begin
         if VCAST_Ada_Options.VCAST_USE_STATIC_MEMORY then
          AVL_Node_Array_Pos := AVL_Node_Array_Pos+1;
          return address_to_avl_node_ref (
            AVL_Node_Array(AVL_Node_Array_Pos-1)'Address);
         else
            return new AVL_Node;
         end if;
   end Get_Avl_Node;

   function address_to_mcdc_statement_ptr is new
     Unchecked_Colwersion (System.Address, VCAST_MCDC_Statement_Ptr);
   function Get_MCDC_Statement return VCAST_MCDC_Statement_Ptr is
      Ptr : VCAST_MCDC_Statement_Ptr;
   begin
     if VCAST_Ada_Options.VCAST_USE_STATIC_MEMORY then
         -- If has another avl item.
         if not (AVL_Item_Array_Pos <= VCAST_NUM_MCDC_STMNTS) then
            if not VCAST_Max_MCDC_Statements_Exceeded then
               WRITE_COVERAGE_LINE ("VCAST_MAX_MCDC_STATEMENTS exceeded!");
               VCAST_Max_MCDC_Statements_Exceeded := True;
            end if;
            return null;
         end if;

         AVL_Item_Array_Pos := AVL_Item_Array_Pos+1;
         return address_to_mcdc_statement_ptr (AVL_Item_Array(AVL_Item_Array_Pos-1)'Address);
     else
       Ptr := new VCAST_MCDC_Statement;
       return Ptr;
     end if;
   end Get_MCDC_Statement;

   function VCAST_MCDC_Statement_Is_Equal (L, R : VCAST_MCDC_Statement_Ptr)
     return Integer is
   begin
     if L.MCDC_Bits = R.MCDC_Bits then
       if L.MCDC_Bits_Used = R.MCDC_Bits_Used then
         return 0;
       elsif L.MCDC_Bits_Used < R.MCDC_Bits_Used then
         return -1;
       else
         return 1;
       end if;
     elsif L.MCDC_Bits < R.MCDC_Bits then
       return -1;
     else
       return 1;
     end if;
   end VCAST_MCDC_Statement_Is_Equal;


   procedure Search_Insert (T : in out AVL_Tree;
                            Element : VCAST_MCDC_Statement_Ptr;
                            Node : in out AVL_Node_Ref;
                            Increased : in out Boolean;
                            Inserted : out Boolean);

   procedure Balance_Left (Node : in out AVL_Node_Ref;
                           Decreased : in out Boolean);

   procedure Balance_Right (Node : in out AVL_Node_Ref;
                            Decreased : in out Boolean);

   function Search (
      T : AVL_Tree;
      Element : VCAST_MCDC_Statement_Ptr;
      Node : AVL_Node_Ref) return Boolean;

   procedure Search_Insert (T : in out AVL_Tree;
                            Element : VCAST_MCDC_Statement_Ptr;
                            Node : in out AVL_Node_Ref;
                            Increased : in out Boolean;
                            Inserted : out Boolean) is
      P1, P2 : AVL_Node_Ref;
      Val : Integer;
   begin
      Inserted := True;
      if Node = null then
         Node := Get_Avl_Node;

         Node.Element := Element;
         Node.Left := null;
         Node.Right := null;
         Node.Balance := Middle;

         Increased := True;
      else
         Val := VCAST_MCDC_Statement_Is_Equal (Element, Node.Element);
         if Val = -1 then
            Search_Insert (T, Element, Node.Left, Increased, Inserted);
            if Increased then
               case Node.Balance is
                  when Right =>
                     Node.Balance := Middle;
                     Increased := False;
                  when Middle =>
                     Node.Balance := Left;
                  when Left =>
                     P1 := Node.Left;
                     if P1.Balance = Left then
                        Node.Left := P1.Right;
                        P1.Right := Node;
                        Node.Balance := Middle;
                        Node := P1;
                     else
                        P2 := P1.Right;
                        P1.Right := P2.Left;
                        P2.Left := P1;
                        Node.Left := P2.Right;
                        P2.Right := Node;
                        if P2.Balance = Left then
                           Node.Balance := Right;
                        else
                           Node.Balance := Middle;
                        end if;
                        if P2.Balance = Right then
                           P1.Balance := Left;
                        else
                           P1.Balance := Middle;
                        end if;
                        Node := P2;
                     end if;
                     Node.Balance := Middle;
                     Increased := False;
               end case;
            end if;
         elsif Val = 1 then
            Search_Insert (T, Element, Node.Right, Increased, Inserted);
            if Increased then
               case Node.Balance is
                  when Left =>
                     Node.Balance := Middle;
                     Increased := False;
                  when Middle =>
                     Node.Balance := Right;
                  when Right =>
                     P1 := Node.Right;
                     if P1.Balance = Right then
                        Node.Right := P1.Left;
                        P1.Left := Node;
                        Node.Balance := Middle;
                        Node := P1;
                     else
                        P2 := P1.Left;
                        P1.Left := P2.Right;
                        P2.Right := P1;
                        Node.Right := P2.Left;
                        P2.Left := Node;
                        if P2.Balance = Right then
                           Node.Balance := Left;
                        else
                           Node.Balance := Middle;
                        end if;
                        if P2.Balance = Left then
                           P1.Balance := Right;
                        else
                           P1.Balance := Middle;
                        end if;
                        Node := P2;
                     end if;
                     Node.Balance := Middle;
                     Increased := False;
               end case;
            end if;
         else
            --  We need to cope with the case where elements _compare_
            --  equal but their non-VCAST_MCDC_Statement_Ptr data content has changed.
            Node.Element := Element;
            Inserted := False;
         end if;
      end if;
   end Search_Insert;

   procedure Balance_Left (Node : in out AVL_Node_Ref;
                           Decreased : in out Boolean) is
      P1, P2 : AVL_Node_Ref;
      Balance1, Balance2 : Node_Balance;
   begin
      case Node.Balance is
         when Left =>
            Node.Balance := Middle;
         when Middle =>
            Node.Balance := Right;
            Decreased := False;
         when Right =>
            P1 := Node.Right;
            Balance1 := P1.Balance;
            if Balance1 >= Middle then
               Node.Right := P1.Left;
               P1.Left := Node;
               if Balance1 = Middle then
                  Node.Balance := Right;
                  P1.Balance := Left;
                  Decreased := False;
               else
                  Node.Balance := Middle;
                  P1.Balance := Middle;
               end if;
               Node := P1;
            else
               P2 := P1.Left;
               Balance2 := P2.Balance;
               P1.Left := P2.Right;
               P2.Right := P1;
               Node.Right := P2.Left;
               P2.Left := Node;
               if Balance2 = Right then
                  Node.Balance := Left;
               else
                  Node.Balance := Middle;
               end if;
               if Balance2 = Left then
                  P1.Balance := Right;
               else
                  P1.Balance := Middle;
               end if;
               Node := P2;
               P2.Balance := Middle;
            end if;
      end case;
   end Balance_Left;

   procedure Balance_Right (Node : in out AVL_Node_Ref;
                            Decreased : in out Boolean)  is
      P1, P2 : AVL_Node_Ref;
      Balance1, Balance2 : Node_Balance;
   begin
      case Node.Balance is
         when Right =>
            Node.Balance := Middle;
         when Middle =>
            Node.Balance := Left;
            Decreased := False;
         when Left =>
            P1 := Node.Left;
            Balance1 := P1.Balance;
            if Balance1 <= Middle then
               Node.Left := P1.Right;
               P1.Right := Node;
               if Balance1 = Middle then
                  Node.Balance := Left;
                  P1.Balance := Right;
                  Decreased := False;
               else
                  Node.Balance := Middle;
                  P1.Balance := Middle;
               end if;
               Node := P1;
            else
               P2 := P1.Right;
               Balance2 := P2.Balance;
               P1.Right := P2.Left;
               P2.Left := P1;
               Node.Left := P2.Right;
               P2.Right := Node;
               if Balance2 = Left then
                  Node.Balance := Right;
               else
                  Node.Balance := Middle;
               end if;
               if Balance2 = Right then
                  P1.Balance := Left;
               else
                  P1.Balance := Middle;
               end if;
               Node := P2;
               P2.Balance := Middle;
            end if;
      end case;
   end Balance_Right;

   function Search (
      T : AVL_Tree;
      Element : VCAST_MCDC_Statement_Ptr;
      Node : AVL_Node_Ref) return Boolean is
      VAL : Integer;
   begin
      if Node /= null then
         VAL := VCAST_MCDC_Statement_Is_Equal ( Node.Element, Element );
         if VAL = 0 then
            return True;
         elsif VAL = 1 then
            return Search (T, Element, Node.Left);
         else
            return Search (T, Element, Node.Right);
         end if;
      else
         return False;
      end if;
   end Search;

   --  end supporting functions

   procedure Insert (T : in out AVL_Tree;
                     Element : VCAST_MCDC_Statement_Ptr;
                     Not_Found : out Boolean) is
      Increased : Boolean := False;
      Result : Boolean;
   begin
      Search_Insert (T, Element, T.Rep, Increased, Result);
      if Result then
         T.Size := T.Size + 1;
         Not_Found := True;
      else
         Not_Found := False;
      end if;
   end Insert;

   function Extent (T : AVL_Tree) return Natural is
   begin
      return T.Size;
   end Extent;

   function Is_Null (T : AVL_Tree) return Boolean is
   begin
      return T.Rep = null;
   end Is_Null;

   function Is_Member (
      T : AVL_Tree;
      Element : VCAST_MCDC_Statement_Ptr) return Boolean is
   begin
      return Search (T, Element, T.Rep);
   end Is_Member;

   procedure Access_Actual_VCAST_MCDC_Statement_Ptr (
      Node :         AVL_Node_Ref;
      K    :         VCAST_MCDC_Statement_Ptr;
      Elem :    out  VCAST_MCDC_Statement_Ptr;
      Found: in out  Boolean) is
      VAL : Integer;
   begin
      if Found then
         return;
      end if;

      if Node /= null then
         VAL := VCAST_MCDC_Statement_Is_Equal (Node.Element, K );
         if VAL = 0 then
            Found := True;
            Elem := Node.Element;
         elsif VAL = 1 then
            Access_Actual_VCAST_MCDC_Statement_Ptr ( Node.Left, K, Elem, Found);
         else
            Access_Actual_VCAST_MCDC_Statement_Ptr ( Node.Right, K, Elem, Found);
         end if;
      end if;
   end Access_Actual_VCAST_MCDC_Statement_Ptr;

   procedure Find (
      In_The_Tree :        AVL_Tree;
      K           :        VCAST_MCDC_Statement_Ptr;
      Elem        :    out VCAST_MCDC_Statement_Ptr;
      Found       :    out Boolean) is
      Found_Result : Boolean := False;
   begin
      Access_Actual_VCAST_MCDC_Statement_Ptr (In_The_Tree.Rep, K, Elem, Found_Result);
      Found := Found_Result;
   end Find;

   function address_to_mcdc_statement is new
     Unchecked_Colwersion (System.Address, VCAST_MCDC_Statement_Ptr);
   function address_to_subprogram_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_Subprogram_Coverage_Ptr);
   function address_to_statement_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_Statement_Coverage_Ptr);
   function address_to_branch_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_Branch_Coverage_Ptr);
   function address_to_mcdc_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_MCDC_Coverage_Ptr);
   function address_to_STATEMENT_MCDC_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_STATEMENT_MCDC_Coverage_Ptr);
   function address_to_STATEMENT_BRANCH_coverage is new
     Unchecked_Colwersion (System.Address, VCAST_STATEMENT_BRANCH_Coverage_Ptr);

   function ADDRESS_TO_AVL_TREE_PTR is new
            Unchecked_Colwersion ( System.Address, VCAST_AVL_TYPE_PTR );

   function ADDRESS_TO_VCAST_BITS_TYPE is new
            Unchecked_Colwersion ( System.Address, VCAST_BITS_TYPE_PTR );

   function ADDRESS_TO_COVERAGE_PTR is new
            Unchecked_Colwersion ( System.Address, VCAST_Subprogram_Coverage_Ptr);

   procedure SAVE_STATEMENT_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer) is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     Statement_Coverage_Ptr : VCAST_Statement_Coverage_Ptr :=
       address_to_statement_coverage (Subprogram_Coverage.Coverage_Ptr);
     VCAST_Bits : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Statement_Coverage_Ptr.Coverage_Bits);
   begin
     if VCAST_Bits (Statement) = False then
         VCAST_Bits (Statement) := True;
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => (Statement+1));
     end if;
   end SAVE_STATEMENT_REALTIME;

   procedure SAVE_STATEMENT_STATEMENT_MCDC_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer) is

     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SM_Coverage : VCAST_STATEMENT_MCDC_Coverage_Ptr :=
       address_to_STATEMENT_MCDC_coverage (Subprogram_Coverage.Coverage_Ptr);
     Statement_Coverage_Ptr : VCAST_Statement_Coverage_Ptr :=
       address_to_statement_coverage (SM_Coverage.Statement_Coverage);
     VCAST_Bits : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Statement_Coverage_Ptr.Coverage_Bits);
   begin
     if VCAST_Bits (Statement) = False then
         VCAST_Bits (Statement) := True;
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => (Statement+1));
     end if;
   end SAVE_STATEMENT_STATEMENT_MCDC_REALTIME;

   procedure SAVE_STATEMENT_STATEMENT_BRANCH_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer) is

     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SB_Coverage : VCAST_STATEMENT_BRANCH_Coverage_Ptr :=
       address_to_STATEMENT_BRANCH_coverage (Subprogram_Coverage.Coverage_Ptr);
     Statement_Coverage_Ptr : VCAST_Statement_Coverage_Ptr :=
       address_to_statement_coverage (SB_Coverage.Statement_Coverage);
     VCAST_Bits : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Statement_Coverage_Ptr.Coverage_Bits);
   begin
     if VCAST_Bits (Statement) = False then
         VCAST_Bits (Statement) := True;
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => (Statement+1));
     end if;
   end SAVE_STATEMENT_STATEMENT_BRANCH_REALTIME;
   pragma Linker_Section(SAVE_STATEMENT_STATEMENT_BRANCH_REALTIME, LINKER_SECTION_NAME);

   function SAVE_BRANCH_CONDITION_REALTIME (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     Branch_Coverage_Ptr : VCAST_Branch_Coverage_Ptr :=
       address_to_branch_coverage (Subprogram_Coverage.Coverage_Ptr);
     VCAST_Bits_True : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_True);
     VCAST_Bits_False : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_False);
     VCAST_IO : String(1..1) := " ";
   begin
     if CONDITION and then VCAST_Bits_True (Statement) = False then
       VCAST_Bits_True (Statement) := True;
       VCAST_IO(1) := 'T';
     elsif not CONDITION and then VCAST_Bits_False (Statement) = False then
       VCAST_Bits_False (Statement) := True;
       VCAST_IO(1) := 'F';
     end if;

      if VCAST_IO(1) /= ' ' then
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => VCAST_IO(1));
      end if;
     return CONDITION;
   end SAVE_BRANCH_CONDITION_REALTIME;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_REALTIME (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SB_Coverage : VCAST_STATEMENT_BRANCH_Coverage_Ptr :=
       address_to_STATEMENT_BRANCH_coverage (Subprogram_Coverage.Coverage_Ptr);
     Branch_Coverage_Ptr : VCAST_Branch_Coverage_Ptr :=
       address_to_branch_coverage (SB_Coverage.Branch_Coverage);
     VCAST_Bits_True : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_True);
     VCAST_Bits_False : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_False);
     VCAST_IO : String(1..1) := " ";
   begin
     if CONDITION and then VCAST_Bits_True (Statement) = False then
       VCAST_Bits_True (Statement) := True;
       VCAST_IO(1) := 'T';
     elsif not CONDITION and then VCAST_Bits_False (Statement) = False then
       VCAST_Bits_False (Statement) := True;
       VCAST_IO(1) := 'F';
     end if;

     if VCAST_IO(1) /= ' ' then
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => VCAST_IO(1));
     end if;
     return CONDITION;
   end SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_REALTIME;
    pragma Linker_Section(SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_REALTIME, LINKER_SECTION_NAME);

   procedure SAVE_STATEMENT_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer) is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
   begin
      WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                          V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                          V3 => (Statement+1));
   end SAVE_STATEMENT_ANIMATION;

   procedure SAVE_STATEMENT_STATEMENT_MCDC_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer) is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
        address_to_subprogram_coverage (Coverage);
   begin
      WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                          V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                          V3 => (Statement+1));
   end SAVE_STATEMENT_STATEMENT_MCDC_ANIMATION;

   procedure SAVE_STATEMENT_STATEMENT_BRANCH_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer) is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
        address_to_subprogram_coverage (Coverage);
   begin
      WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                          V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                          V3 => (Statement+1));
   end SAVE_STATEMENT_STATEMENT_BRANCH_ANIMATION;

   function SAVE_BRANCH_CONDITION_ANIMATION (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean;
     ON_PATH   :  Boolean )
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
        address_to_subprogram_coverage (Coverage);
     VCAST_IO : String(1..2) := "  ";
   begin
     if CONDITION then
       VCAST_IO(1) := 'T';
     else
       VCAST_IO(1) := 'F';
     end if;

     if not ON_PATH then
       VCAST_IO(2) := 'X';
     end if;

     if VCAST_IO(1) /= ' ' then
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => VCAST_IO(1),
                             V5 => VCAST_IO(2));
     end if;
     return CONDITION;
   end SAVE_BRANCH_CONDITION_ANIMATION;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_ANIMATION (
      Coverage  :  System.Address;
      Statement :  Integer;
      CONDITION :  Boolean)
      return Boolean is
      Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
         address_to_subprogram_coverage (Coverage);
      VCAST_IO : String(1..1) := " ";
   begin
     if CONDITION then
       VCAST_IO(1) := 'T';
     else
       VCAST_IO(1) := 'F';
     end if;

     if VCAST_IO(1) /= ' ' then
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => VCAST_IO(1));
     end if;
     return CONDITION;
   end SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_ANIMATION;

   function SAVE_BRANCH_CONDITION_BUFFERED (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     Branch_Coverage_Ptr : VCAST_Branch_Coverage_Ptr :=
       address_to_branch_coverage (Subprogram_Coverage.Coverage_Ptr);
     VCAST_Bits_True : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_True);
     VCAST_Bits_False : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_False);
   begin
     if CONDITION and then VCAST_Bits_True (Statement) = False then
       VCAST_Bits_True (Statement) := True;
     elsif not CONDITION and then VCAST_Bits_False (Statement) = False then
       VCAST_Bits_False (Statement) := True;
     end if;
     return CONDITION;
   end SAVE_BRANCH_CONDITION_BUFFERED;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_BUFFERED (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SB_Coverage : VCAST_STATEMENT_BRANCH_Coverage_Ptr :=
       address_to_STATEMENT_BRANCH_coverage (Subprogram_Coverage.Coverage_Ptr);
     Branch_Coverage_Ptr : VCAST_Branch_Coverage_Ptr :=
       address_to_branch_coverage (SB_Coverage.Branch_Coverage);
     VCAST_Bits_True : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_True);
     VCAST_Bits_False : VCAST_BITS_TYPE_PTR :=
       ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_False);
   begin
     if CONDITION and then VCAST_Bits_True (Statement) = False then
       VCAST_Bits_True (Statement) := True;
     elsif not CONDITION and then VCAST_Bits_False (Statement) = False then
       VCAST_Bits_False (Statement) := True;
     end if;
     return CONDITION;
   end SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_BUFFERED;

   function SAVE_MCDC_SUBCONDITION (
     MCDC_Statement:  System.Address;
     BIT           :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
   begin
     -- If this is a sub-condition for an MCDC expression, just record the bit
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(BIT+1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(BIT+1) := True;

     return CONDITION;
   end SAVE_MCDC_SUBCONDITION;

   function SAVE_MCDC_STATEMENT_MCDC_SUBCONDITION (
     MCDC_Statement:  System.Address;
     BIT           :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
   begin
     -- If this is a sub-condition for an MCDC expression, just record the bit
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(BIT+1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(BIT+1) := True;

     return CONDITION;
   end SAVE_MCDC_STATEMENT_MCDC_SUBCONDITION;

   function SAVE_MCDC_CONDITION_REALTIME (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
     MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
       address_to_mcdc_coverage (Subprogram_Coverage.Coverage_Ptr);
     AVL_TYPE_PTR : VCAST_AVL_TYPE_PTR :=
       ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;

     if not Is_Member (AVL_TYPE_PTR(Statement), MCDC_Statement_Ptr) then
       declare
         New_MCDC_Statement : VCAST_MCDC_Statement_Ptr := Get_MCDC_Statement;
         found : Boolean;
       begin
         if New_MCDC_Statement = null then
           return CONDITION;
         end if;
         New_MCDC_Statement.all := MCDC_Statement_Ptr.all;
         Insert (AVL_TYPE_PTR(Statement), New_MCDC_Statement, found);
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits),
                             V5 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits_Used));
       end;
     end if;

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_CONDITION_REALTIME;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_REALTIME (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SM_Coverage : VCAST_STATEMENT_MCDC_Coverage_Ptr :=
       address_to_STATEMENT_MCDC_coverage (Subprogram_Coverage.Coverage_Ptr);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
     MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
       address_to_mcdc_coverage (SM_Coverage.MCDC_Coverage);
     AVL_TYPE_PTR : VCAST_AVL_TYPE_PTR :=
       ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;

     if not Is_Member (AVL_TYPE_PTR(Statement), MCDC_Statement_Ptr) then
       declare
         New_MCDC_Statement : VCAST_MCDC_Statement_Ptr := Get_MCDC_Statement;
         found : Boolean;
       begin
         if New_MCDC_Statement = null then
           return CONDITION;
         end if;
         New_MCDC_Statement.all := MCDC_Statement_Ptr.all;
         Insert (AVL_TYPE_PTR(Statement), New_MCDC_Statement, found);
         WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                             V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                             V3 => Statement,
                             V4 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits),
                             V5 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits_Used));
       end;
     end if;

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_STATEMENT_MCDC_CONDITION_REALTIME;

   function SAVE_MCDC_CONDITION_ANIMATION (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;


     WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                         V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                         V3 => Statement,
                         V4 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits),
                         V5 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits_Used));

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_CONDITION_ANIMATION;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_ANIMATION (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;
     WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                         V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                         V3 => Statement,
                         V4 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits),
                         V5 => BIT_ARRAY_MCDC_TO_INT (MCDC_Statement_Ptr.MCDC_Bits_Used));

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_STATEMENT_MCDC_CONDITION_ANIMATION;

   function SAVE_MCDC_CONDITION_BUFFERED (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
     MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
       address_to_mcdc_coverage (Subprogram_Coverage.Coverage_Ptr);
     AVL_TYPE_PTR : VCAST_AVL_TYPE_PTR :=
       ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;

     if not Is_Member (AVL_TYPE_PTR(Statement), MCDC_Statement_Ptr) then
       declare
         New_MCDC_Statement : VCAST_MCDC_Statement_Ptr := Get_MCDC_Statement;
         found : Boolean;
       begin
         if New_MCDC_Statement = null then
           return CONDITION;
         end if;
         New_MCDC_Statement.all := MCDC_Statement_Ptr.all;
         Insert (AVL_TYPE_PTR(Statement), New_MCDC_Statement, found);
       end;
     end if;

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_CONDITION_BUFFERED;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_BUFFERED (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean is
     Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr :=
       address_to_subprogram_coverage (Coverage);
     SM_Coverage : VCAST_STATEMENT_MCDC_Coverage_Ptr :=
       address_to_STATEMENT_MCDC_coverage (Subprogram_Coverage.Coverage_Ptr);
     MCDC_Statement_Ptr : VCAST_MCDC_Statement_Ptr :=
       address_to_mcdc_statement (MCDC_Statement);
     MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
       address_to_mcdc_coverage (SM_Coverage.MCDC_Coverage);
     AVL_TYPE_PTR : VCAST_AVL_TYPE_PTR :=
       ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
   begin
     -- Store the total expression
     if CONDITION then
       MCDC_Statement_Ptr.MCDC_Bits(1) := True;
     end if;
     MCDC_Statement_Ptr.MCDC_Bits_Used(1) := True;

     if not Is_Member (AVL_TYPE_PTR(Statement), MCDC_Statement_Ptr) then
       declare
         New_MCDC_Statement : VCAST_MCDC_Statement_Ptr := Get_MCDC_Statement;
         found : Boolean;
       begin
         if New_MCDC_Statement = null then
           return CONDITION;
         end if;
         New_MCDC_Statement.all := MCDC_Statement_Ptr.all;
         Insert (AVL_TYPE_PTR(Statement), New_MCDC_Statement, found);
       end;
     end if;

     MCDC_Statement_Ptr.MCDC_Bits := (others => False);
     MCDC_Statement_Ptr.MCDC_Bits_Used := (others => False);

     return CONDITION;
   end SAVE_MCDC_STATEMENT_MCDC_CONDITION_BUFFERED;

   procedure VCAST_Append_Subprog_List_Ptr (Coverage : System.Address) is
     VCAST_New : VCAST_Subprog_List_Ptr;
   begin
     if VCAST_Ada_Options.VCAST_USE_STATIC_MEMORY then
       VCAST_Subprogram_Coverage_Array_Obj(VCAST_Subprogram_Coverage_Array_Pos) :=
         ADDRESS_TO_COVERAGE_PTR (Coverage);
       VCAST_Subprogram_Coverage_Array_Pos := VCAST_Subprogram_Coverage_Array_Pos + 1;
     else
       VCAST_New := new VCAST_Subprog_List;
       VCAST_New.VCAST_Data := ADDRESS_TO_COVERAGE_PTR (Coverage);
       VCAST_New.VCAST_Next := null;
       if VCAST_Subprog_Root = null then
         VCAST_Subprog_Root := VCAST_New;
         VCAST_Subprog_Lwr := VCAST_New;
       else
         VCAST_Subprog_Lwr.VCAST_Next := VCAST_New;
         VCAST_Subprog_Lwr := VCAST_Subprog_Lwr.VCAST_Next;
       end if;
     end if;
   end VCAST_Append_Subprog_List_Ptr;

   procedure VCAST_REGISTER_SUBPROGRAM (
     Coverage  :  System.Address) is
   begin
     if VCAST_Ada_Options.VCAST_USE_STATIC_MEMORY then
      if VCAST_Subprogram_Coverage_Array_Pos > VCAST_Ada_Options.VCAST_MAX_COVERED_SUBPROGRAMS then
         if VCAST_Max_Covered_Subprograms_Exceeded = False then
            WRITE_COVERAGE_LINE ("VCAST_MAX_COVERED_SUBPROGRAMS exceeded!");
            VCAST_Max_Covered_Subprograms_Exceeded := True;
         end if;
      else
         VCAST_Subprogram_Coverage_Array_Obj(VCAST_Subprogram_Coverage_Array_Pos) :=
            ADDRESS_TO_COVERAGE_PTR (Coverage);
         VCAST_Subprogram_Coverage_Array_Pos := VCAST_Subprogram_Coverage_Array_Pos + 1;
      end if;
     else
       VCAST_Append_Subprog_List_Ptr (Coverage);
     end if;
   end VCAST_REGISTER_SUBPROGRAM;

   procedure VCAST_DUMP_STATEMENT_COVERAGE (Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr) is
     Statement_Coverage_Ptr : VCAST_Statement_Coverage_Ptr;
   begin
     if Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_STATEMENT then
       Statement_Coverage_Ptr :=
         address_to_statement_coverage (Subprogram_Coverage.Coverage_Ptr);
     elsif Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_STATEMENT_MCDC then
       declare
         SM_Coverage : VCAST_STATEMENT_MCDC_Coverage_Ptr :=
           address_to_STATEMENT_MCDC_coverage (Subprogram_Coverage.Coverage_Ptr);
       begin
          Statement_Coverage_Ptr :=
            address_to_statement_coverage (SM_Coverage.Statement_Coverage);
       end;
     elsif Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_STATEMENT_BRANCH then
       declare
         SB_Coverage : VCAST_STATEMENT_BRANCH_Coverage_Ptr :=
           address_to_STATEMENT_BRANCH_coverage (Subprogram_Coverage.Coverage_Ptr);
       begin
          Statement_Coverage_Ptr :=
            address_to_statement_coverage (SB_Coverage.Statement_Coverage);
       end;
     end if;

     declare
       VCAST_Bits : VCAST_BITS_TYPE_PTR :=
         ADDRESS_TO_VCAST_BITS_TYPE (Statement_Coverage_Ptr.Coverage_Bits);
     begin
       for I in 0 .. Subprogram_Coverage.VCAST_Num_Statement_Statements-1 loop
            if VCAST_Bits (I) = True then

               WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                                   V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                                   V3 => (I+1));
            end if;
       end loop;
     end;
   end VCAST_DUMP_STATEMENT_COVERAGE;

   procedure VCAST_DUMP_BRANCH_COVERAGE (Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr) is
     Branch_Coverage_Ptr : VCAST_Branch_Coverage_Ptr;
   begin
     if Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_BRANCH then
         Branch_Coverage_Ptr :=
           address_to_branch_coverage (Subprogram_Coverage.Coverage_Ptr);
     elsif Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_STATEMENT_BRANCH then
       declare
         SB_Coverage : VCAST_STATEMENT_BRANCH_Coverage_Ptr :=
           address_to_STATEMENT_BRANCH_coverage (Subprogram_Coverage.Coverage_Ptr);
       begin
         Branch_Coverage_Ptr :=
           address_to_branch_coverage (SB_Coverage.Branch_Coverage);
       end;
     end if;

     declare
       VCAST_Bits_True : VCAST_BITS_TYPE_PTR :=
         ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_True);
       VCAST_Bits_False : VCAST_BITS_TYPE_PTR :=
         ADDRESS_TO_VCAST_BITS_TYPE (Branch_Coverage_Ptr.Branch_Bits_False);
     begin
       for I in 0 .. Subprogram_Coverage.VCAST_Num_Statement_Statements-1 loop
         if VCAST_Bits_True (I) = True then
               WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                                   V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                                   V3 => I,
                                   V4 => 'T');
         end if;

         if VCAST_Bits_False (I) = True then
            WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                                V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                                V3 => I,
                                V4 => 'F');
         end if;
       end loop;
     end;
   end VCAST_DUMP_BRANCH_COVERAGE;

   procedure VCAST_DUMP_MCDC_COVERAGE (Subprogram_Coverage : VCAST_Subprogram_Coverage_Ptr) is
     AVL_TYPE_PTR : VCAST_AVL_TYPE_PTR;
     Statement : Integer := 0;
     procedure VCAST_DUMP_MCDC_STATEMENT (Item :     VCAST_MCDC_Statement_Ptr;
                                          OK   :    out Boolean) is
     begin
       OK := True;
       WRITE_COVERAGE_LINE(V1 => Subprogram_Coverage.VCAST_Unit_Id,
                           V2 => Subprogram_Coverage.VCAST_Subprogram_ID,
                           V3 => Statement,
                           V4 => BIT_ARRAY_MCDC_TO_INT (Item.MCDC_Bits),
                           V5 => BIT_ARRAY_MCDC_TO_INT (Item.MCDC_Bits_Used));
     end VCAST_DUMP_MCDC_STATEMENT;

     procedure VCAST_DUMP_MCDC_STATEMENTS (Over_The_Tree : AVL_Tree) is
        Continue : Boolean := True;
        procedure Visit (Node : AVL_Node_Ref);
        procedure Visit (Node : AVL_Node_Ref) is
        begin
           if Node /= null then
              Visit (Node.Left);
              if not Continue then
                 return;
              end if;
              VCAST_DUMP_MCDC_STATEMENT (Node.Element, Continue);
              if not Continue then
                 return;
              end if;
              Visit (Node.Right);
           end if;
        end Visit;
     begin
        Visit (Over_The_Tree.Rep);
     end VCAST_DUMP_MCDC_STATEMENTS;
   begin
     if Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_MCDC then
       declare
         MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
           address_to_mcdc_coverage (Subprogram_Coverage.Coverage_Ptr);
       begin
         AVL_TYPE_PTR := ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
       end;
     elsif Subprogram_Coverage.Coverage_Kind = VCAST_COVERAGE_STATEMENT_MCDC then
       declare
         SM_Coverage : VCAST_STATEMENT_MCDC_Coverage_Ptr :=
           address_to_STATEMENT_MCDC_coverage (Subprogram_Coverage.Coverage_Ptr);
         MCDC_Coverage : VCAST_MCDC_Coverage_Ptr :=
            address_to_mcdc_coverage (SM_Coverage.MCDC_Coverage);
       begin
         AVL_TYPE_PTR := ADDRESS_TO_AVL_TREE_PTR (MCDC_Coverage.Avl_Trees);
       end;
     end if;

     -- Walk the tree and output the data
     for I in 0 .. Subprogram_Coverage.VCAST_Num_Statement_Statements-1 loop
       Statement := I;
       VCAST_DUMP_MCDC_STATEMENTS (AVL_TYPE_PTR (Statement));
     end loop;
   end VCAST_DUMP_MCDC_COVERAGE;

   procedure VCAST_DUMP_COVERAGE_DATA is
     Ptr : VCAST_Subprogram_Coverage_Ptr;
     Lwr : VCAST_Subprog_List_Ptr;
   begin
     if VCAST_Ada_Options.VCAST_USE_STATIC_MEMORY then
       for I in 1..VCAST_Subprogram_Coverage_Array_Pos-1 loop
         Ptr := VCAST_Subprogram_Coverage_Array_Obj(I);
         case Ptr.Coverage_Kind is
           when VCAST_COVERAGE_STATEMENT =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
           when VCAST_COVERAGE_BRANCH =>
             VCAST_DUMP_BRANCH_COVERAGE (Ptr);
           when VCAST_COVERAGE_MCDC =>
             VCAST_DUMP_MCDC_COVERAGE (Ptr);
           when VCAST_COVERAGE_STATEMENT_MCDC =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
             VCAST_DUMP_MCDC_COVERAGE (Ptr);
           when VCAST_COVERAGE_STATEMENT_BRANCH =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
             VCAST_DUMP_BRANCH_COVERAGE (Ptr);
         end case;
       end loop;
     else
       Lwr := VCAST_Subprog_Root;
       while Lwr /= null loop
         Ptr := Lwr.VCAST_Data;
         case Ptr.Coverage_Kind is
           when VCAST_COVERAGE_STATEMENT =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
           when VCAST_COVERAGE_BRANCH =>
             VCAST_DUMP_BRANCH_COVERAGE (Ptr);
           when VCAST_COVERAGE_MCDC =>
             VCAST_DUMP_MCDC_COVERAGE (Ptr);
           when VCAST_COVERAGE_STATEMENT_MCDC =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
             VCAST_DUMP_MCDC_COVERAGE (Ptr);
           when VCAST_COVERAGE_STATEMENT_BRANCH =>
             VCAST_DUMP_STATEMENT_COVERAGE (Ptr);
             VCAST_DUMP_BRANCH_COVERAGE (Ptr);
         end case;
         Lwr := Lwr.VCAST_Next;
       end loop;
     end if;
   end VCAST_DUMP_COVERAGE_DATA;

end VCAST_COVER_IO;
