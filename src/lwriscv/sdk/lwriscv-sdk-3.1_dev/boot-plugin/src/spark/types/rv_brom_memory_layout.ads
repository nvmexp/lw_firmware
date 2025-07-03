-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
-- 
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining constants corresponding to the general Peregrine memory layout.
-- this layout coupled with the Peregrine tightly so it is not subject to change over 
-- different Bootrom variant and generations.

with Project;

package Rv_Brom_Memory_Layout is
    
    -- RISCV Physical memory layout
    -- ==============================----------
    --             | Global IO      |  Global
    --             | Global Memory  |  Apeture
    -- 0x0160_0000 |                |  
    -- ------------==================-----------
    -- 0x015F_FFFF |  Reserved      |  Local 
    -- 0x0130_0000 |    3M          |  Reserved
    -- ------------==================-----------
    -- 0x012F_FFFF |  Local IO      |
    --             |                |  Local IO
    -- 0x0128_0000 |    512K        |  Apeture
    -- ------------==================-----------
    -- 0x012F_FFFF |    EMEM        |
    -- 0x0120_0000 |    512K        |
    -- ------------==================
    -- 0x0117_FFFF |   Reserved     |  Local
    -- 0x0020_0000 |    15.5M       |  Memory
    -- ------------==================  
    -- 0x001F_FFFF |    DMEM        |  Apeture
    -- 0x0018_0000 |    512K        |
    -- ------------==================
    -- 0x0017_FFFF |    IMEM        |
    -- 0x0010_0000 |    512K        |
    -- ------------==================
    -- 0x000F_FFFF |  IROM/BROM     |
    -- 0x0008_0000 |    512K        |
    -- -------------=================-----------
    -- 0x0007_FFFF |   Reserved     |  Local 
    -- 0x0000_0000 |    512K        |  Reserved
    -- ------------==================-----------
    
    -- For BROM usage
    -- IMEM memory layout is defined as follows
    -- 
    -- RISCV_PA_IMEM_MEMEND
    -- ----------------============================
    -- FMCCODE_END     |                          |
    --                 |         FMC code         |
    --                 |   RISCV_PA_IMEM_MEMEND   |
    --                 |                          |
    --                 |                          |
    -- FMCCODE_START   |                          |
    -- ----------------============================
    
    -- DMEM memory layout is defined as follows
    --
    -- RISCV_PA_DMEM_MEMEND
    -- -----------------============================
    -- PKCPARM_END      |    Pkc Boot Parameters   |
    -- PKCPARM_START    |          2K              |
    -- -----------------============================
    -- STACK_END        |                          |
    --                  |         STACK            |
    -- STACK_START      |          3K              |
    -- -----------------============================ 
    -- PUBKEY_END       |     Public Key(Reserved) |
    -- PUBKEY_START     |          2K              |
    -- -----------------============================
    -- FMCDATA_END      |                          |
    --                  |                          |
    --                  |        FMC Data          |
    --                  |                          |
    --                  |    RISCV_PA_DMEM_MEMEND  |
    --                  | - 5K (7K if SW Key used) |
    --                  |                          |
    -- FMCDATA_START    |                          |
    -- -----------------=============================
    

    -- IO ranges, if the IO range doesn't exist its END and START will all equals to 0.
    RISCV_PA_INTIO_END         : constant := Project.LW_XXX_RISCV_PA_INTIO_END;
    RISCV_PA_INTIO_START       : constant := Project.LW_XXX_RISCV_PA_INTIO_START;
    RISCV_PA_INTIO_EXISTS      : constant Boolean := Project.LW_XXX_RISCV_PA_INTIO_EXISTS;
    
    RISCV_PA_EXTIO1_END        : constant := Project.LW_XXX_RISCV_PA_EXTIO1_END;
    RISCV_PA_EXTIO1_START      : constant := Project.LW_XXX_RISCV_PA_EXTIO1_START;
    RISCV_PA_EXTIO1_EXISTS     : constant Boolean := Project.LW_XXX_RISCV_PA_EXTIO1_EXISTS;

    
    RISCV_PA_EXTIO2_END        : constant := Project.LW_XXX_RISCV_PA_EXTIO2_END;
    RISCV_PA_EXTIO2_START      : constant := Project.LW_XXX_RISCV_PA_EXTIO2_START;
    RISCV_PA_EXTIO2_EXISTS     : constant Boolean := Project.LW_XXX_RISCV_PA_EXTIO2_EXISTS;

    RISCV_PA_EXTIO3_END        : constant := Project.LW_XXX_RISCV_PA_EXTIO3_END;
    RISCV_PA_EXTIO3_START      : constant := Project.LW_XXX_RISCV_PA_EXTIO3_START;
    RISCV_PA_EXTIO3_EXISTS     : constant Boolean := Project.LW_XXX_RISCV_PA_EXTIO3_EXISTS;

    RISCV_PA_EXTIO4_END        : constant := Project.LW_XXX_RISCV_PA_EXTIO4_END;
    RISCV_PA_EXTIO4_START      : constant := Project.LW_XXX_RISCV_PA_EXTIO4_START;
    RISCV_PA_EXTIO4_EXISTS     : constant Boolean := Project.LW_XXX_RISCV_PA_EXTIO3_EXISTS;
    RISCV_HAS_MODULE_TFBIF     : constant Boolean := Project.LW_XXX_HAS_MODULE_TFBIF;
        
    -- Imem Block size
    RISCV_IMEM_BLOCKSIZE       : constant := Project.LW_XXX_FALCON_IMEM_SIZE;
    RISCV_IMEM_BLOCKSIZE_LIMIT : constant := RISCV_IMEM_BLOCKSIZE - 1;
    -- Dmem Block size
    RISCV_DMEM_BLOCKSIZE       : constant := Project.LW_XXX_FALCON_DMEM_SIZE;
    RISCV_DMEM_BLOCKSIZE_LIMIT : constant := RISCV_DMEM_BLOCKSIZE - 1;

    -- Imem Byte size
    RISCV_IMEM_BYTESIZE       : constant := RISCV_IMEM_BLOCKSIZE * 256;
    RISCV_IMEM_BYTESIZE_LIMIT : constant := RISCV_IMEM_BYTESIZE - 1;
    -- Dmem Byte size
    RISCV_DMEM_BYTESIZE       : constant := RISCV_DMEM_BLOCKSIZE * 256;
    RISCV_DMEM_BYTESIZE_LIMIT : constant := RISCV_DMEM_BYTESIZE - 1;

    -- Imem Bit size
    RISCV_IMEM_BITSIZE       : constant := RISCV_IMEM_BYTESIZE * 8 ;
    RISCV_IMEM_BITSIZE_LIMIT : constant := RISCV_IMEM_BITSIZE - 1;
    -- Dmem Bit size
    RISCV_DMEM_BITSIZE       : constant := RISCV_DMEM_BYTESIZE * 8 ;
    RISCV_DMEM_BITSIZE_LIMIT : constant := RISCV_DMEM_BITSIZE - 1;

    -- Imem Dword size
    RISCV_IMEM_DWORDSIZE       : constant := RISCV_IMEM_BYTESIZE / 4;
    RISCV_IMEM_DWORDSIZE_LIMIT : constant := RISCV_IMEM_DWORDSIZE - 1;
    -- Dmem Dword size
    RISCV_DMEM_DWORDSIZE       : constant := RISCV_DMEM_BYTESIZE / 4;
    RISCV_DMEM_DWORDSIZE_LIMIT : constant := RISCV_DMEM_DWORDSIZE - 1;

    RISCV_TCM_MAX_BYTESIZE       : constant := RISCV_IMEM_BYTESIZE + RISCV_DMEM_BYTESIZE;
    RISCV_TCM_MAX_BYTESIZE_LIMIT : constant := RISCV_TCM_MAX_BYTESIZE - 1;

    -- Physical memory start
    RISCV_PA_BROM_START   : constant := 16#00080000#;
    RISCV_PA_IMEM_START   : constant := 16#00100000#;
    RISCV_PA_DMEM_START   : constant := 16#00180000#;

    -- Physical memory end
    RISCV_PA_IMEM_BYTESIZE_LIMIT   : constant := RISCV_PA_IMEM_START + RISCV_IMEM_BYTESIZE_LIMIT;  --16#0017_FFFF#
    RISCV_PA_DMEM_BYTESIZE_LIMIT   : constant := RISCV_PA_DMEM_START + RISCV_DMEM_BYTESIZE_LIMIT;  --16#001F_FFFF#

    FMCCODE_END           : constant := RISCV_IMEM_BYTESIZE;
    FMCCODE_START         : constant := 0;
    FMCCODE_MAX_SIZE      : constant := FMCCODE_END - FMCCODE_START;

    PKCPARAM_END           : constant := RISCV_DMEM_BYTESIZE;
    PKCPARAM_SIZE          : constant := 16#A00#;
    PKCPARAM_START         : constant := PKCPARAM_END - PKCPARAM_SIZE;

    BROMDATA_END           : constant := PKCPARAM_START;
    BROMDATA_SIZE          : constant := 16#C00#;
    BROMDATA_START         : constant := BROMDATA_END - BROMDATA_SIZE;
    
    SWPUBKEY_END           : constant := BROMDATA_START;
    SWPUBKEY_SIZE          : constant := 16#200#;
    SWPUBKEY_START         : constant := SWPUBKEY_END - SWPUBKEY_SIZE;
    
    STAGE2_PKCPARAM_END    : constant := SWPUBKEY_START;
    STAGE2_PKCPARAM_SIZE   : constant := 16#400#;
    STAGE2_PKCPARAM_START  : constant := STAGE2_PKCPARAM_END - STAGE2_PKCPARAM_SIZE;

    FMCDATA_END            : constant := STAGE2_PKCPARAM_START;
    FMCDATA_START          : constant := 0;
    FMCDATA_MAX_SIZE       : constant := FMCDATA_END - FMCDATA_START;
    
    -- If S/W public is not used, the fmc data size can reach to the start of BROMDATA_START
    FMCDATA_BLOCKEND       : constant := FMCDATA_END / 256;
    FMCDATA_MAX_END        : constant := BROMDATA_START;
    FMCDATA_MAX_BLOCKEND   : constant := FMCDATA_MAX_END / 256;
    FMCDATA_MAX_LIMIT      : constant := FMCDATA_MAX_END - 1;
    
    FMCCODE_START_PA_ADDR         : constant := RISCV_PA_IMEM_START + FMCCODE_START;
    PKCPARAM_START_PA_ADDR        : constant := RISCV_PA_DMEM_START + PKCPARAM_START;
    BROMDATA_START_PA_ADDR        : constant := RISCV_PA_DMEM_START + BROMDATA_START;
    SWPUBKEY_START_PA_ADDR        : constant := RISCV_PA_DMEM_START + SWPUBKEY_START;
    STAGE2_PKCPARAM_START_PA_ADDR : constant := RISCV_PA_DMEM_START + STAGE2_PKCPARAM_START;
    FMCDATA_START_PA_ADDR         : constant := RISCV_PA_DMEM_START + FMCDATA_START;
        
end Rv_Brom_Memory_Layout;


