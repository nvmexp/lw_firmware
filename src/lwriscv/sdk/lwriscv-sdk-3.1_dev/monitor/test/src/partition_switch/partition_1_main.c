#include <dev_top.h>

#include <lw_riscv_address_map.h>
#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <test.h>
#include <switch_to_param.h>

#include "policy_check.h"
#include "lwriscv/manifest_gh100.h"

#define PARTITION_0_ID 0


policy_struct expected = {
    .s_uid = 4,
    .secret_mask0 = 0,
    .secret_mask1 = 0,
    .secret_mask_lock0 = 0xffffffff,
    .secret_mask_lock1 = 0xffffffff,
    .debug_ctl = (unsigned)(
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STOP,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RUN,    _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STEP,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_J,      _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_EMASK,  _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RREG,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WREG,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RDM,    _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WDM,    _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_IBRKPT, _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RCSR,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WCSR,   _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RPC,    _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RFREG,  _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WFREG,  _ENABLE ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _START_IN_ICD,     _FALSE  ) |
        DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _SINGLE_STEP_MODE, _ENABLE)),
    .debug_ctl_lock = 0x0,
    .hash_mpu = 1,
    .mpu_start_index = 0,
    .mpu_entry_count = 128,
    .device_map_group_0 =
        DEVICEMAP(_MMODE,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_RISCV_CTL,      _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_PIC,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_TIMER,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_HOSTIF,         _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DMA,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_PMB,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DIO,            _DISABLE, _DISABLE, _UNLOCKED  ),
    .device_map_group_1 =
        DEVICEMAP(_KEY,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DEBUG,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SHA,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_KMEM,           _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_BROM,           _DISABLE, _DISABLE, _UNLOCKED  ) |
        DEVICEMAP(_ROM_PATCH,      _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_IOPMP,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_NOACCESS,       _DISABLE, _DISABLE, _UNLOCKED  ),
    .device_map_group_2 =
        DEVICEMAP(_SCP,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_FBIF,           _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_FALCON_ONLY,    _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_PRGN_CTL,       _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP0, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP1, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP2, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP3, _ENABLE,  _ENABLE,  _UNLOCKED  ),
    .device_map_group_3 =
        DEVICEMAP(_PLM,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_HUB_DIO,        _ENABLE,  _ENABLE,  _UNLOCKED  ),
    .corepmp_cfgs =
        { {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0x103000},  // 0
          {.read = 1, .write = 0, .execute = 1, .mode = MODE_TOR, .address = 0x105000},  // 1
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0x105000},  // 2
          {.read = 1, .write = 1, .execute = 1, .mode = MODE_TOR, .address = 0x108000},  // 3
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 4
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0x181000},  // 5
          {.read = 1, .write = 1, .execute = 0, .mode = MODE_TOR, .address = 0x183000},  // 6
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0x184000},  // 7
          {.read = 1, .write = 1, .execute = 0, .mode = MODE_TOR, .address = 0x200000},  // 8
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0x1400000}, // 9
          {.read = 1, .write = 1, .execute = 0, .mode = MODE_TOR, .address = 0x1600000}, // 10
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 11
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 12
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 13
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 14
          {.read = 0, .write = 0, .execute = 0, .mode = MODE_OFF, .address = 0},         // 15
        },
    .iopmp_cfgs =
       {
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 0
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 1
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 2
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 3
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 4
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 5
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 6
        {.mode = MODE_NAPOT, .address_lo = 0x7FFFFF, .address_hi = 0,
         .cfg = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,  _ENABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE) |
                DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE) |
                DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0, _ENABLE)},               // 7
       },
};
/*
 * arg0-arg4 are parameters passed by partition 0
 */
int partition_1_main(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{

    SWITCHED(1);

    EXPECT_TRUE(arg0 == PARAM_0, "Parameter 0 passing failed! expect: 0x%llx, actual: 0x%lx\n", PARAM_0, arg0);
    EXPECT_TRUE(arg1 == PARAM_1, "Parameter 1 passing failed! expect: 0x%llx, actual: 0x%lx\n", PARAM_1, arg1);
    EXPECT_TRUE(arg2 == PARAM_2, "Parameter 2 passing failed! expect: 0x%llx, actual: 0x%lx\n", PARAM_2, arg2);
    EXPECT_TRUE(arg3 == PARAM_3, "Parameter 3 passing failed! expect: 0x%llx, actual: 0x%lx\n", PARAM_3, arg3);
    EXPECT_TRUE(arg4 == PARAM_4, "Parameter 4 passing failed! expect: 0x%llx, actual: 0x%lx\n", PARAM_4, arg4);

    uint64_t reg64;
    uint32_t reg32;

    // check uid
    reg64 = csr_read(LW_RISCV_CSR_SLWRRUID);
    reg32 = DRF_VAL(_RISCV, _CSR_SLWRRUID, _SLWRRUID, reg64);
    EXPECT_TRUE(reg32 == expected.s_uid, "UCODE_ID Mismatch! expected: %d, actual: %d\n", expected.s_uid, reg32);

    // check secret mask and secret mask lock
    reg32 = localRead(LW_PRGNLCL_RISCV_SCP_SECRET_MASK(0));
    EXPECT_TRUE(reg32 == expected.secret_mask0, "Secret_Mask0 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.secret_mask0, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_SCP_SECRET_MASK(1));
    EXPECT_TRUE(reg32 == expected.secret_mask1, "Secret_Mask1 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.secret_mask1, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK(0));
    EXPECT_TRUE(reg32 == expected.secret_mask_lock0, "Secret_Mask_Lock0 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.secret_mask_lock0, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK(1));
    EXPECT_TRUE(reg32 == expected.secret_mask_lock1, "Secret_Mask_Lock1 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.secret_mask_lock1, reg32);

    // check debug control and debug control lock
    reg32 = localRead(LW_PRGNLCL_RISCV_DBGCTL);
    EXPECT_TRUE(reg32 == expected.debug_ctl, "Debug Control Mismatch! expected: 0x%x, actual: 0x%x\n", expected.debug_ctl, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_DBGCTL_LOCK);
    EXPECT_TRUE(reg32 == expected.debug_ctl_lock, "Debug Control Mismatch! expected: 0x%x, actual: 0x%x\n", expected.debug_ctl_lock, reg32);

    // check mpu
    test_get_mcsr(LW_RISCV_CSR_MMPUCTL, &reg64);
    uint32_t mpu_start_index = DRF_VAL(_RISCV, _CSR_MMPUCTL, _START_INDEX, reg64);
    uint32_t mpu_entry_count = DRF_VAL(_RISCV, _CSR_MMPUCTL, _ENTRY_COUNT, reg64);
    uint32_t hash_mpu = DRF_VAL(_RISCV, _CSR_MMPUCTL, _HASH_LWMPU_EN, reg64);
    EXPECT_TRUE(mpu_entry_count == expected.mpu_entry_count, "MPU Entry Count Mismatch! expected: 0x%x, actual: 0x%x\n", expected.mpu_entry_count, mpu_entry_count);
    EXPECT_TRUE(mpu_start_index == expected.mpu_start_index, "MPU Start Index Mismatch! expected: 0x%x, actual: 0x%x\n", expected.mpu_start_index, mpu_start_index);
    EXPECT_TRUE(hash_mpu == expected.hash_mpu, "MPU Hash Mpu Mismatch! expected: 0x%x, actual: 0x%x\n", expected.hash_mpu, hash_mpu);

    // check device_map
    reg32 = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE(0));
    EXPECT_TRUE(reg32 == expected.device_map_group_0, "Device Map Group 0 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.device_map_group_0, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE(1));
    EXPECT_TRUE(reg32 == expected.device_map_group_1, "Device Map Group 1 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.device_map_group_1, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE(2));
    EXPECT_TRUE(reg32 == expected.device_map_group_2, "Device Map Group 2 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.device_map_group_2, reg32);
    reg32 = localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE(3));
    EXPECT_TRUE(reg32 == expected.device_map_group_3, "Device Map Group 3 Mismatch! expected: 0x%x, actual: 0x%x\n", expected.device_map_group_3, reg32);

    // check Core pmp
    for (uint8_t corepmp_index = COREPMP_START_INDEX; corepmp_index < (COREPMP_START_INDEX + COREPMP_ENTRY_COUNT); corepmp_index++)
    {
        uint32_t read, write, execute, mode;

        // lwrrently we only support fixed index from 8 to 23
        // which is configured through pmpcfg2 and mextpmpcfg0

        // TODO address checking is omited for now, since ada/spark made it
        // very painful to case match the address instead of generic passing
        // of address, which increase the code size dramatically.
        // Fortunately, addresses are set in dedicated registers one per each entry
        // so it's less likely to have bugs.
        if ((corepmp_index >= 8) && (corepmp_index < 16))
        {
            test_get_mcsr(LW_RISCV_CSR_PMPCFG2, &reg64);
            read = (reg64 & (0x1UL << ((corepmp_index-8) * 8))) != 0 ? 1 : 0;
            write = (reg64 & (0x2UL << ((corepmp_index-8) * 8))) != 0 ? 1 : 0;
            execute = (reg64 & (0x4UL << ((corepmp_index-8) * 8))) != 0 ? 1 : 0;
            mode = (uint32_t)((reg64 >>((corepmp_index-8) * 8 + 3)) & 0x3);
        }
        else
        {
            test_get_mcsr(LW_RISCV_CSR_MEXTPMPCFG0, &reg64);
            read = (reg64 & (0x1UL << ((corepmp_index-16) * 8))) != 0 ? 1 : 0;
            write = (reg64 & (0x2UL << ((corepmp_index-16) * 8))) != 0 ? 1 : 0;
            execute = (reg64 & (0x4UL << ((corepmp_index-16) * 8))) != 0 ? 1 : 0;
            mode = (uint32_t)((reg64 >>((corepmp_index-16) * 8 + 3)) & 0x3);
        }

        uint8_t array_index = (uint8_t)(corepmp_index - COREPMP_START_INDEX);
        EXPECT_TRUE(read == expected.corepmp_cfgs[array_index].read, "Core Pmp Index %d read Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.corepmp_cfgs[array_index].read, read);
        EXPECT_TRUE(write == expected.corepmp_cfgs[array_index].write, "Core Pmp Index %d write Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.corepmp_cfgs[array_index].write, write);
        EXPECT_TRUE(execute == expected.corepmp_cfgs[array_index].execute, "Core Pmp Index %d execute Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.corepmp_cfgs[array_index].execute, execute);
        EXPECT_TRUE(mode == expected.corepmp_cfgs[array_index].mode, "Core Pmp Index %d mode Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.corepmp_cfgs[array_index].mode, mode);
    }

    // check io pmp
    for (uint8_t iopmp_index = IOPMP_START_INDEX; iopmp_index < (IOPMP_START_INDEX + IOPMP_ENTRY_COUNT); iopmp_index++)
    {
        uint32_t address_lo, address_hi, cfg, mode;

        if (iopmp_index < 16)
        {
            reg32 = localRead(LW_PRGNLCL_RISCV_IOPMP_MODE(0));
            mode = (uint32_t)((reg32 >> (iopmp_index * 2)) & 0x3);
        }
        else
        {
            reg32 = localRead(LW_PRGNLCL_RISCV_IOPMP_MODE(1));
            mode = (uint32_t)((reg32 >> ((iopmp_index - 16) * 2)) & 0x3);
        }

        localWrite(LW_PRGNLCL_RISCV_IOPMP_INDEX, iopmp_index);
        cfg = localRead(LW_PRGNLCL_RISCV_IOPMP_CFG);
        address_lo = localRead(LW_PRGNLCL_RISCV_IOPMP_ADDR_LO);
        address_hi = localRead(LW_PRGNLCL_RISCV_IOPMP_ADDR_HI);

        uint8_t array_index = (uint8_t)(iopmp_index - IOPMP_START_INDEX);
        EXPECT_TRUE(cfg == expected.iopmp_cfgs[array_index].cfg, "IO Pmp Index %d cfg Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.iopmp_cfgs[array_index].cfg, cfg);
        EXPECT_TRUE(address_lo == expected.iopmp_cfgs[array_index].address_lo, "IO Pmp Index %d address_lo Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.iopmp_cfgs[array_index].address_lo, address_lo);
        EXPECT_TRUE(address_hi == expected.iopmp_cfgs[array_index].address_hi, "IO Pmp Index %d address_hi Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.iopmp_cfgs[array_index].address_hi, address_hi);
        EXPECT_TRUE(mode == expected.iopmp_cfgs[array_index].mode, "IO Pmp Index %d mode Mismatch! expected: 0x%x, actual: 0x%x\n", array_index, expected.iopmp_cfgs[array_index].mode, mode);
    }

    // Test switch to valid partition 0
    TEST(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FIRST, NUM_SBI_PARAMS,
            PARAM_0, PARAM_1, PARAM_2, PARAM_3, PARAM_4, PARTITION_0_ID);

    return 0;
}

void partition_1_trap_handler(void)
{
    uint64_t scause = csr_read(LW_RISCV_CSR_SCAUSE);
    uint64_t svalue = csr_read(LW_RISCV_CSR_STVAL);
    TEST_FAILED("Partition 1 Trap handler. cause: 0x%lx, value: 0x%lx\n", scause, svalue);
    return;
}
