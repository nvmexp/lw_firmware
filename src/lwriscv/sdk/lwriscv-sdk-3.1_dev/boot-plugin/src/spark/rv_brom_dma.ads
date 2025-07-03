with Lw_Types; use Lw_Types;
with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;

with Rv_Brom_Types; use Rv_Brom_Types;
with Rv_Boot_Plugin_Error_Handling; use Rv_Boot_Plugin_Error_Handling;
with Rv_Brom_Riscv_Core; use Rv_Brom_Riscv_Core;

package Rv_Brom_Dma is
    type External_Memory_Address      is new LwU47;
    type External_Memory_Addr_Hi      is new LwU7;

    type Dmem_Transfer_Size is new LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field range SIZE_128B .. SIZE_256B;
    type Imem_Transfer_Size is new LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field range SIZE_256B .. SIZE_256B;

    type Config is record
        Wprid  : LwU4;
        Gscid  : LwU5;
        Target : LwU2;
    end record;

    procedure Initialize (Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Finalize (Err_Code : out Error_Codes) with
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Set_Dma_Config (Cfg : Config) with
        Global => (Output => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    function Src_Dst_Aligned_For_Read(Src      : LwU64;
                                      Dst      : LwU24;
                                      Size     : LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field) return Boolean is
       (Src mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (Size) + 2)) = 0 and then
       Dst mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (Size) + 2)) = 0);

    function Src_Dst_Aligned_For_Read(Src      : External_Memory_Address;
                                      Dst      : Imem_Offset_Byte;
                                      Size     : Imem_Transfer_Size) return Boolean is
       (Src mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size)) + 2)) = 0 and then
       Dst mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size)) + 2)) = 0);

    function Src_Dst_Aligned_For_Read(Src      : External_Memory_Address;
                                      Dst      : Dmem_Offset_Byte;
                                      Size     : Dmem_Transfer_Size) return Boolean is
       (Src mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size)) + 2)) = 0 and then
       Dst mod (2 ** (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field'Enum_Rep (LW_PRGNLCL_FALCON_DMATRFCMD_SIZE_Field(Size)) + 2)) = 0);

    -- Dma Data in from outside to Imem
    procedure Read (Src      : External_Memory_Address;
                    Dst      : Imem_Offset_Byte;
                    Size     : Imem_Transfer_Size) with
        Pre => Src_Dst_Aligned_For_Read (Src, Dst, Size),
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    -- Dma Data in from outside to Dmem
    procedure Read (Src      : External_Memory_Address;
                    Dst      : Dmem_Offset_Byte;
                    Size     : Dmem_Transfer_Size) with
        Pre => Src_Dst_Aligned_For_Read (Src, Dst, Size),
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    -- Dma Data in from outside to Imem
    -- Each Block is 256-byte length
    procedure Read_Block (Src           : External_Memory_Address;
                          Dst           : Imem_Offset_Byte;
                          Num_Of_Blocks : Imem_Size_Block;
                          Err_Code      : in out Error_Codes) with
        Pre => Err_Code = OK and then
               Src_Dst_Aligned_For_Read (Src, Dst, Rv_Brom_Dma.SIZE_256B) and then
               Num_Of_Blocks > 0 and then
               LwU32 (Num_Of_Blocks) * 256 <= LwU32 (Imem_Size_Byte'Last) - LwU32 (Dst) and then
               External_Memory_Address (Num_Of_Blocks) * 256 - 1 <= External_Memory_Address'Last - Src,
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    -- Dma Data in from outside to Dmem
    -- Each Block is 256-byte length
    procedure Read_Block (Src           : External_Memory_Address;
                          Dst           : Dmem_Offset_Byte;
                          Num_Of_Blocks : Dmem_Size_Block;
                          Err_Code      : in out Error_Codes) with
        Pre => Err_Code = OK and then
              Src_Dst_Aligned_For_Read(Src, Dst, Rv_Brom_Dma.SIZE_256B) and then
              Num_Of_Blocks > 0 and then
              LwU32(Num_Of_Blocks) * 256 <= LwU32(Dmem_Size_Byte'Last) - LwU32(Dst) and then
              External_Memory_Address (Num_Of_Blocks) * 256 - 1 <= External_Memory_Address'Last - Src,
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;


    procedure Write (Src      : Imem_Offset_Byte;
                     Dst      : External_Memory_Address;
                     Size     : Imem_Transfer_Size) with
        Pre => Src_Dst_Aligned_For_Read (Dst, Src, Size),
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Write (Src      : Dmem_Offset_Byte;
                     Dst      : External_Memory_Address;
                     Size     : Dmem_Transfer_Size) with
        Pre => Src_Dst_Aligned_For_Read (Dst, Src, Size),
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Wait_For_Complete (Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Global => (In_Out => Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

end Rv_Brom_Dma;
