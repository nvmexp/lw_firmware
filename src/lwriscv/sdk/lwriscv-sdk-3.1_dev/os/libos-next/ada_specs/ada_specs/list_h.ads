pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;

package list_h is

   --  arg-macro: procedure LIST_INSERT_SORTED (list, element, type, headerName, valueName)
   --    ListInsertSorted(list, element, (int)offsetof(type, valueName)-(int)offsetof(type,headerName))
   --  arg-macro: procedure LIST_POP_BACK (list, type, headerName)
   --    CONTAINER_OF(ListPopBack(andlist), type, headerName);
   --  arg-macro: procedure LIST_POP_FRONT (list, type, headerName)
   --    CONTAINER_OF(ListPopFront(andlist), type, headerName);
   type ListHeader is record
      next : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:6
      prev : aliased lwtypes_h.LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:6
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:7

   procedure ListLink (id : access ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:9
   with Import => True, 
        Convention => C, 
        External_Name => "ListLink";

   procedure ListUnlink (id : access ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:10
   with Import => True, 
        Convention => C, 
        External_Name => "ListUnlink";

   procedure ListPushFront (list : access ListHeader; element : access ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:11
   with Import => True, 
        Convention => C, 
        External_Name => "ListPushFront";

   procedure ListPushBack (list : access ListHeader; element : access ListHeader)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:12
   with Import => True, 
        Convention => C, 
        External_Name => "ListPushBack";

  -- Internal API
   procedure ListInsertSorted
     (header : access ListHeader;
      element : access ListHeader;
      fieldFromHeader : lwtypes_h.LwS64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:24
   with Import => True, 
        Convention => C, 
        External_Name => "ListInsertSorted";

   function ListPopBack (arg1 : access ListHeader) return access ListHeader  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:25
   with Import => True, 
        Convention => C, 
        External_Name => "ListPopBack";

   function ListPopFront (arg1 : access ListHeader) return access ListHeader  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/list.h:26
   with Import => True, 
        Convention => C, 
        External_Name => "ListPopFront";

end list_h;
