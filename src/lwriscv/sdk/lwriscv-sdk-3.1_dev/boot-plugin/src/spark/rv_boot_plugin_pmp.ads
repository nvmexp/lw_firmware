with Lw_Types; use Lw_Types;
with Rv_Brom_Memory_Layout; use Rv_Brom_Memory_Layout;
with Rv_Boot_Plugin_Error_Handling;use Rv_Boot_Plugin_Error_Handling;

package Rv_Boot_Plugin_Pmp is

    -- IOPMP constants
    IOPMP_INDEX_BACKGROUND : constant := 31;
    IOPMP_INDEX_DMEM       : constant := 30;
    IOPMP_INDEX_IMEM       : constant := 29;
    IOPMP_INDEX_GLBLMEM    : constant := 28;

    IOPMP_MODE_BACKGROUND_NAPOT : constant := 16#c000_0000_0000_0000#; -- #31 NAPOT
    IOPMP_MODE_DMEM_NAPOT       : constant := 16#3000_0000_0000_0000#; -- #30 NAPOT
    IOPMP_MODE_IMEM_NAPOT       : constant := 16#0c00_0000_0000_0000#; -- #29 NAPOT
    IOPMP_MODE_GLBLMEM_NAPOT    : constant := 16#0300_0000_0000_0000#; -- #28 NAPOT

    -- address callwlation for NAPOT: addr = (base >> 2) + 2 ** (log2_size - 3) - 1
    IOPMP_ADDR_BACKGROUND : constant := 2 ** (64 - 3) - 1; -- base = 0; size = 2^64
    IOPMP_ADDR_DMEM       : constant := RISCV_PA_DMEM_START / 4 + 2 ** (19 - 3) - 1; -- base = RISCV_PA_DMEM_START; size = 2^19
    IOPMP_ADDR_IMEM       : constant := RISCV_PA_IMEM_START / 4 + 2 ** (19 - 3) - 1; -- base = RISCV_PA_IMEM_START; size = 2^19
    IOPMP_ADDR_GLBLMEM    : constant := 2 ** (52 - 2) + 2 ** (47 - 3) - 1;           -- base = 0; size = 2^47; bit 52 is set to
                                                                                     -- indicate it's global memory; ctxdma = 0



    -- COREPMP constants
    COREPMP_INDEX_BACKGROUND    : constant := 31;
    COREPMP_INDEX_BROM          : constant := 30;
    COREPMP_INDEX_DMEM_BOUND    : constant := 29;
    COREPMP_INDEX_DMEM_BASE     : constant := 28;
    COREPMP_INDEX_LCLIO         : constant := 27;
    COREPMP_INDEX_GLBLIO        : constant := 26;
    COREPMP_INDEX_26            : constant := 26;
    COREPMP_INDEX_25            : constant := 25;
    COREPMP_INDEX_24            : constant := 24;

    COREPMP_ADDR_BACKGROUND          : constant := 2 ** (64 - 3) - 1;                               -- base = 0; size = 2^64
    COREPMP_ADDR_BROM_INIT           : constant := RISCV_PA_BROM_START / 4 + 2 ** (17 - 3) - 1;     -- base = RISCV_PA_BROM_START; size = 2^17
    COREPMP_ADDR_BROM_AUTHED         : constant := RISCV_PA_BROM_START / 4 + 2 ** (18 - 3) - 1;     -- base = RISCV_PA_BROM_START; size = 2^18
    COREPMP_ADDR_BROM_OVERSIZED      : constant := 2 ** (21 - 3) - 1;                               -- base = 0; size = 2^21, oversized entry to
                                                                                                    -- temporarily enable DMEM to prevent DoS
    COREPMP_ADDR_DMEM_BOUND          : constant := (RISCV_PA_DMEM_START + RISCV_DMEM_BYTESIZE) / 4; -- bound = RISCV_PA_DMEM_START +
                                                                                                    -- RISCV_DMEM_BYTESIZE
    COREPMP_ADDR_DMEM_BASE_INIT      : constant := RISCV_PA_DMEM_START / 4;                         -- base = RISCV_PA_DMEM_START
    COREPMP_ADDR_DMEM_BASE_OVERSIZED : constant := RISCV_PA_IMEM_START / 4;                         -- base = RISCV_PA_IMEM_START, oversized entry
                                                                                                    -- to temporarily enable IMEM so that used
                                                                                                    -- portion can be scrubbed
    COREPMP_ADDR_LCLIO               : constant := RISCV_PA_INTIO_START / 4 + 2 ** (21 - 3) - 1;    -- base = RISCV_PA_INTIO_START; size = 2^21
    COREPMP_ADDR_GLGLIO              : constant := RISCV_PA_EXTIO1_START / 4 + 2 ** (26 - 3) - 1;   -- base = RISCV_PA_EXTIO1_START; size = 2^26

    procedure Initialize with Inline_Always;

end Rv_Boot_Plugin_Pmp;
