---------------------------------------
-- Copyright 2010 Vector Software    --
-- East Greenwich, Rhode Island USA --
---------------------------------------
with System;
with VCAST_Ada_Options;
with Unchecked_Colwersion;
with Lw_Types; use Lw_Types;
with Falcon_Io; use Falcon_Io;

package VCAST_COVER_IO is
   type AVL_Tree is private;
   Null_Avl_Tree : constant AVL_Tree;
   VCAST_NUM_MCDC_STMNTS : constant Integer := VCAST_Ada_Options.VCAST_MAX_MCDC_STATEMENTS;

   -- Single bit Boolean type
   type VCAST_BIT_TYPE is new Boolean;
   for VCAST_BIT_TYPE'Size use 1;

   -- Array of bits, used to track coverage history
   type VCAST_BIT_ARRAY_TYPE is array ( Natural range <> ) of VCAST_BIT_TYPE;
   -- TODO: Uncomment the below once CCG ticket S107-003 is resolved
   --pragma Pack(VCAST_BIT_ARRAY_TYPE);

   -- Unconstrained array of integer, used during MCDC coverage to store
   -- the number of sub-conditions in a given conditional statement
   type VCAST_INT_ARRAY_TYPE is array ( Natural range <> ) of Integer;

   -- Pointer to VCAST_BIT_ARRAY_TYPE
   type VCAST_BIT_ARRAY_PTR is access VCAST_BIT_ARRAY_TYPE;

   -- Global bit array used for MCDC temporary work
   subtype VCAST_BIT_ARRAY_MCDC_TYPE is VCAST_BIT_ARRAY_TYPE(1..32);
   MCDC_BITS : VCAST_BIT_ARRAY_TYPE(1..40) := (others => False);

   type VCAST_MCDC_Statement is record
     MCDC_Bits       : VCAST_BIT_ARRAY_MCDC_TYPE := (others => False);
     MCDC_Bits_Used  : VCAST_BIT_ARRAY_MCDC_TYPE := (others => False);
   end record;
   type VCAST_MCDC_Statement_Ptr is access VCAST_MCDC_Statement;

   function VCAST_MCDC_Statement_Is_Equal (L, R : VCAST_MCDC_Statement_Ptr)
     return Integer;

   type VCAST_AVL_Tree_Array is array (Integer range <>)
     of AVL_Tree;

   type VCAST_Statement_Coverage is record
     -- Allocate N/8 bytes where N is the number of statements
     Coverage_Bits                  : System.Address;
     -- The number of statement statements
     VCAST_Num_Statement_Statements : Integer;
   end record;
   type VCAST_Statement_Coverage_Ptr is access VCAST_Statement_Coverage;

   type VCAST_Branch_Coverage is record
     -- Allocate N/4 bytes where N is the number of statements
     Branch_Bits_True            : System.Address;
     Branch_Bits_False           : System.Address;
     VCAST_Num_Branch_Statements : Integer;
   end record;
   type VCAST_Branch_Coverage_Ptr is access VCAST_Branch_Coverage;

   type VCAST_MCDC_Coverage is record
     -- For each statement hit, a 'vcast_mcdc_statement record'
     -- will be inserted into this structure to represent the
     -- statements that have been hit.
     -- Allocate N tree's where N is the number of statements.
     Avl_Trees                 : System.Address;

     -- The number of mcdc statements
     VCAST_Num_MCDC_Statements : Integer;
   end record;

   type VCAST_MCDC_Coverage_Ptr is access VCAST_MCDC_Coverage;

   type VCAST_STATEMENT_MCDC_Coverage is record
     Statement_Coverage : System.Address; -- VCAST_Statement_Coverage_Ptr;
     MCDC_Coverage      : System.Address; -- VCAST_MCDC_Coverage_Ptr;
   end record;
   type VCAST_STATEMENT_MCDC_Coverage_Ptr is access VCAST_STATEMENT_MCDC_Coverage;

   type VCAST_STATEMENT_BRANCH_Coverage is record
     Statement_Coverage : System.Address; -- VCAST_Statement_Coverage_Ptr;
     Branch_Coverage    : System.Address; -- VCAST_Branch_Coverage_Ptr;
   end record;
   type VCAST_STATEMENT_BRANCH_Coverage_Ptr is access VCAST_STATEMENT_BRANCH_Coverage;

   type VCAST_Coverage_Kind is (
      VCAST_COVERAGE_STATEMENT,
      VCAST_COVERAGE_BRANCH,
      VCAST_COVERAGE_MCDC,
      VCAST_COVERAGE_STATEMENT_MCDC,
      VCAST_COVERAGE_STATEMENT_BRANCH);

   type VCAST_Subprogram_Coverage is record
     VCAST_Unit_Id                  : Integer;
     VCAST_Subprogram_ID            : Integer;
     VCAST_Num_Statement_Statements : Integer;
     -- if coverage_kind == VCAST_COVERAGE_STATEMENT
     --   coverage_ptr is of type 'struct vcast_subprogram_statement_coverage *
     -- if coverage_kind == VCAST_COVERAGE_BRANCH
     --   coverage_ptr is of type 'struct vcast_subprogram_branch_coverage *'
     -- if coverage_kind == VCAST_COVERAGE_MCDC
     --   coverage_ptr is of type 'struct vcast_subprogram_mcdc_coverage *'
     -- if coverage_kind == VCAST_COVERAGE_STATEMENT_MCDC
     --   coverage_ptr is of type 'struct vcast_subprogram_STATEMENT_MCDC_coverage *'
     -- if coverage_kind == VCAST_COVERAGE_STATEMENT_BRANCH
     --   coverage_ptr is of type 'struct vcast_subprogram_STATEMENT_BRANCH_coverage *'
     Coverage_Kind         : VCAST_Coverage_Kind;
     Coverage_Ptr          : System.Address;
   end record;
   type VCAST_Subprogram_Coverage_Ptr is access VCAST_Subprogram_Coverage;

 UNSIGNED_32_MAX : constant Unsigned_32 := Unsigned_32'last;
--target=ada83   -- need an extra bit to handle math operations
--target=ada83   type Unsigned_32 is range 0 .. (2**33) - 1;
--target=ada83   UNSIGNED_32_MAX : constant UNSIGNED_32 := 2**32;

   -- For static data
   type VCAST_Subprogram_Coverage_Array is array (1..VCAST_Ada_Options.VCAST_MAX_COVERED_SUBPROGRAMS)
      of VCAST_Subprogram_Coverage_Ptr;
   VCAST_Subprogram_Coverage_Array_Obj : VCAST_Subprogram_Coverage_Array;
   VCAST_Subprogram_Coverage_Array_Pos : Integer := 1;
   -- For dynamic data
   type VCAST_Subprog_List;
   type VCAST_Subprog_List_Ptr is access VCAST_Subprog_List;
   type VCAST_Subprog_List is record
     VCAST_Data : VCAST_Subprogram_Coverage_Ptr;
     VCAST_Next : VCAST_Subprog_List_Ptr;
   end record;
   VCAST_Subprog_Root : VCAST_Subprog_List_Ptr := null;
   VCAST_Subprog_Lwr  : VCAST_Subprog_List_Ptr := null;
   VCAST_Max_MCDC_Statements_Exceeded : Boolean := False;
   VCAST_Max_Covered_Subprograms_Exceeded : Boolean := False;

   -- For static storage of the item in node
   type AVL_Item_Array_Type is array (1..VCAST_NUM_MCDC_STMNTS) of VCAST_MCDC_Statement;
   AVL_Item_Array : AVL_Item_Array_Type;
   AVL_Item_Array_Pos : Integer := 1;

   procedure SAVE_STATEMENT_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer);

   procedure SAVE_STATEMENT_STATEMENT_MCDC_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer);

   procedure SAVE_STATEMENT_STATEMENT_BRANCH_REALTIME(
     Coverage  :  System.Address;
     Statement :  Integer);

   function SAVE_BRANCH_CONDITION_REALTIME (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_REALTIME (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean;

   procedure SAVE_STATEMENT_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer);

   procedure SAVE_STATEMENT_STATEMENT_MCDC_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer);

   procedure SAVE_STATEMENT_STATEMENT_BRANCH_ANIMATION(
     Coverage  :  System.Address;
     Statement :  Integer);

   function SAVE_BRANCH_CONDITION_ANIMATION (
     Coverage      :  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean;
     ON_PATH       :  Boolean )
     return Boolean;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_ANIMATION (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean;

   function SAVE_BRANCH_CONDITION_BUFFERED (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean;

   function SAVE_BRANCH_STATEMENT_BRANCH_CONDITION_BUFFERED (
     Coverage  :  System.Address;
     Statement :  Integer;
     CONDITION :  Boolean)
     return Boolean;

   function SAVE_MCDC_SUBCONDITION (
     MCDC_Statement:  System.Address;
     BIT           :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_STATEMENT_MCDC_SUBCONDITION (
     MCDC_Statement:  System.Address;
     BIT           :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_CONDITION_REALTIME (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_REALTIME (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_CONDITION_BUFFERED (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_BUFFERED (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_CONDITION_ANIMATION (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   function SAVE_MCDC_STATEMENT_MCDC_CONDITION_ANIMATION (
     Coverage      :  System.Address;
     MCDC_Statement:  System.Address;
     Statement     :  Integer;
     CONDITION     :  Boolean)
     return Boolean;

   procedure VCAST_REGISTER_SUBPROGRAM (Coverage  :  System.Address);

   procedure VCAST_DUMP_COVERAGE_DATA;

   -- AVL Tree
   procedure Insert (
      T : in out AVL_Tree;
      Element : VCAST_MCDC_Statement_Ptr;
      Not_Found : out Boolean);
   --  Add the VCAST_MCDC_Statement_Ptr to the tree, preserving the tree's
   --  balance. Not_Found is set to True if the VCAST_MCDC_Statement_Ptr had not
   --  previously existed in the tree, and to False otherwise.

   function Extent (T :  AVL_Tree) return Natural;
   --  Return the number of VCAST_MCDC_Statement_Ptrs in the tree.

   function Is_Null (T :  AVL_Tree) return Boolean;
   --  Return True if and only if the tree has no VCAST_MCDC_Statement_Ptrs.

   function Is_Member (
      T :  AVL_Tree;
      Element : VCAST_MCDC_Statement_Ptr) return Boolean;
   --  Return True if and only if the VCAST_MCDC_Statement_Ptr exists in the tree.

   procedure Find (
      In_The_Tree :      AVL_Tree;
      K           :      VCAST_MCDC_Statement_Ptr;
      Elem        :    out VCAST_MCDC_Statement_Ptr;
      Found       :    out Boolean);
   --  If an VCAST_MCDC_Statement_Ptr "=" to Elem is present in the Tree, Set Elem to value
   --  and set Found to True; otherwise, set Found to False.
   --  Apply MUST NOT alter the result of the ordering operation "<".

   subtype VCAST_STRING is String;
   NULL_STRING : constant VCAST_STRING := "";

private
   type AVL_Node;
   type AVL_Node_Ref is access AVL_Node;

   type Node_Balance is (Left, Middle, Right);

   type AVL_Node is record
      Element : VCAST_MCDC_Statement_Ptr;
      Left, Right : AVL_Node_Ref;
      Balance : Node_Balance := Middle;
   end record;

   type AVL_Tree is record
      Rep : AVL_Node_Ref;
      Size : Natural := 0;
   end record;

   Null_Avl_Tree : constant AVL_Tree := ( Rep => null, Size => 0 );

   -- For static storage of the node
   type AVL_Node_Array_Type is array (1..VCAST_NUM_MCDC_STMNTS) of AVL_Node;
   AVL_Node_Array : AVL_Node_Array_Type;
   AVL_Node_Array_Pos : Integer := 1;

   function address_to_node_ref is new
     Unchecked_Colwersion (System.Address, AVL_Node_Ref);
end VCAST_COVER_IO;
