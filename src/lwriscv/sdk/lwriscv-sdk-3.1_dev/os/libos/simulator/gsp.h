#pragma once
#include "hal.h"
#include "riscv-core.h"
#include <functional>
#include <type_traits>
#include <utility>
#include "libos-config.h"
#include "peregrine-headers.h"
// Basic driver

struct gsp_mmpu_state
{
    // MPU emulation
    LwU32 mmpuIndex     = 0;
    LwU64 mmpuPa[LIBOS_CONFIG_MPU_INDEX_COUNT]    = {0};
    LwU64 mmpuRange[LIBOS_CONFIG_MPU_INDEX_COUNT] = {0};
    LwU64 mmpuAttr[LIBOS_CONFIG_MPU_INDEX_COUNT]  = {0};
    LwU64 mmpuVa[LIBOS_CONFIG_MPU_INDEX_COUNT]    = {0};
    LwU64 xmpuidx2      = 0;
};

struct gsp_falcon_state
{
    // Falcon mailbox
    LwU32 mailbox0 = 0;
    LwU32 mailbox1 = 0;

    // Interrupt controller
    LwU32 irqSet = 0;
    LwU32 irqMask = 0;
    LwU32 privErrStat = 0;
    LwU32 hubErrStat = 0;

    // DMA engine
    LwU32 falconDmactl    = 0;
    LwU32 fbifCtl         = 0;
    LwU32 fbifTranscfg[8] = {0};
    LwU32 trfBase         = 0;
    LwU32 trfBase1        = 0;
    LwU32 trfMoffs        = 0;
    LwU32 trfFboffs       = 0;
    LwU32 trfCmd          = 0;
};

struct gsp_gdma_chan_state
{
    LwU32 status       = DRF_DEF(_PRGNLCL_GDMA, _CHAN_STATUS, _REQ_QUEUE_SPACE, _INIT);
    LwU32 reqProduce   = 0;
    LwU32 reqComplete  = 0;
    LwU32 commonConfig = 0;
    LwU32 transConfig0 = 0;
    LwU32 transConfig1 = 0;
    LwU32 transConfig2 = 0;
    LwU32 srcAddr      = 0;
    LwU32 srcAddrHi    = 0;
    LwU32 destAddr     = 0;
    LwU32 destAddrHi   = 0;
};

struct gsp_gdma_state
{
    LwU32 gdmaIrqCtrl = 0;
    gsp_gdma_chan_state gdmaChan[4];
};

struct gsp : riscv_core, lwstom_memory, gsp_mmpu_state, gsp_falcon_state, gsp_gdma_state
{
    trivial_memory                  mmpuIMEM;
    trivial_memory                  mmpuDMEM;

    LwU64 resetVector;
    LwU64 mmioBase;

    // PRI memory binding
    virtual const uint32_t get_memory_kind() { return 0; }

    void reset(bool clearTcm, bool clearMpu)
    {
        riscv_core::reset();

        // Reset the MMPU and falcon state
        if (clearMpu)
        {
            (gsp_mmpu_state &)*this   = gsp_mmpu_state();
        }
        (gsp_falcon_state &)*this = gsp_falcon_state();

        // Clear IMEM and DMEM
        if (clearTcm)
        {
            std::fill(mmpuDMEM.block.begin(), mmpuDMEM.block.end(), 0);
            std::fill(mmpuIMEM.block.begin(), mmpuIMEM.block.end(), 0);
        }

        // jump back to the boot-vector
        pc = resetVector;
    }

    void wire_hals()
    {
#if LIBOS_LWRISCV == LIBOS_LWRISCV_1_0
            mmioBase              = 0x2000000000000000ull;
            has_supervisor_mode   = false;
            dmaTransfer_HAL       = bind_hal(&gsp::dmaTransfer_TU11X, this);
            translate_address_HAL = bind_hal(&gsp::translate_address_TU11X, this);
            priv_write_HAL        = bind_hal(&gsp::priv_write_TU11X, this);
            priv_read_HAL         = bind_hal(&gsp::priv_read_TU11X, this);
            csr_write_HAL         = bind_hal(&gsp::csr_write_TU11X, this);
            csr_read_HAL          = bind_hal(&gsp::csr_read_TU11X, this);
            csr_validate_access_HAL = bind_hal(&gsp::csr_validate_access_TU102, this);
#elif LIBOS_LWRISCV == LIBOS_LWRISCV_1_1
            mmioBase              = 0x2000000000000000ull;
            has_supervisor_mode   = false;
            dmaTransfer_HAL       = bind_hal(&gsp::dmaTransfer_TU11X, this);
            translate_address_HAL = bind_hal(&gsp::translate_address_TU11X, this);
            priv_write_HAL        = bind_hal(&gsp::priv_write_TU11X, this);
            priv_read_HAL         = bind_hal(&gsp::priv_read_GA100, this);
            csr_write_HAL         = bind_hal(&gsp::csr_write_TU11X, this);
            csr_read_HAL          = bind_hal(&gsp::csr_read_TU11X, this);
            csr_validate_access_HAL = bind_hal(&gsp::csr_validate_access_TU102, this);
#else
            mmioBase              = 0; // PRGNLCL register addresses are absolute addresses.
            has_supervisor_mode   = true;
            dmaTransfer_HAL       = bind_hal(&gsp::dmaTransfer_GA102, this);
            gdmaTransfer_HAL      = bind_hal(&gsp::gdmaTransfer_GA102, this);
            translate_address_HAL = bind_hal(&gsp::translate_address_GA102, this);
            priv_write_HAL        = bind_hal(&gsp::priv_write_GA102, this);
            priv_read_HAL         = bind_hal(&gsp::priv_read_GA102, this);
            csr_write_HAL         = bind_hal(&gsp::csr_write_GA102, this);
            csr_read_HAL          = bind_hal(&gsp::csr_read_GA102, this);
            csr_validate_access_HAL = bind_hal(&gsp::csr_validate_access_GA102, this);
#endif
    }

    gsp(image *firmware)
        : riscv_core(firmware),
          mmpuIMEM('ITCM', LW_RISCV_AMAP_IMEM_START, std::vector<LwU8>(LIBOS_CONFIG_IMEM_SIZE, 0)),
          mmpuDMEM('DTCM', LW_RISCV_AMAP_DMEM_START, std::vector<LwU8>(LIBOS_CONFIG_DMEM_SIZE, 0))
    {
#if !LIBOS_CONFIG_RISCV_S_MODE
            privilege = privilege_machine;
#else
            privilege = privilege_supervisor;
#endif
        memory_regions.push_back(&mmpuIMEM);
        memory_regions.push_back(&mmpuDMEM);
        memory_regions.push_back(this); // PRI

        // Supress WFI support
        event_wfi.connect(&gsp::handler_wfi, this);

        wire_hals();
    }

    void handler_wfi()
    {
    }

    //
    //  HAL functions
    //
    std::function<void(void)> dmaTransfer_HAL;
    std::function<void(LwU8 chanIdx)> gdmaTransfer_HAL;
    std::function<void(LwU64 addr, LwU32 value)> priv_write_HAL;
    std::function<LwU32(LwU64 addr)> priv_read_HAL;

    void dmaTransfer_TU11X();
    LwBool
    translate_address_TU11X(LwU64 va, LwU64 &pa, LwBool write, LwBool execute, LwBool override_selwrity);
    void priv_write_TU11X(LwU64 addr, LwU32 value);
    void csr_write_TU11X(LwU32 addr, LwU64 value);
    LwU64 csr_read_TU11X(LwU32 addr);
    LwU32 priv_read_TU11X(LwU64 addr);

    void dmaTransfer_GA100();
    LwBool
    translate_address_GA100(LwU64 va, LwU64 &pa, LwBool write, LwBool execute, LwBool override_selwrity);
    void priv_write_GA100(LwU64 addr, LwU32 value);
    void csr_write_GA100(LwU32 addr, LwU64 value);
    LwU64 csr_read_GA100(LwU32 addr);
    LwU32 priv_read_GA100(LwU64 addr);

    void dmaTransfer_GA102();
    void gdmaTransfer_GA102(LwU8 chanIdx);
    LwBool
          translate_address_GA102(LwU64 va, LwU64 &pa, LwBool write, LwBool execute, LwBool override_selwrity);
    void  priv_write_GA102(LwU64 addr, LwU32 value);
    void  csr_write_GA102(LwU32 addr, LwU64 value);
    LwU64 csr_read_GA102(LwU32 addr);
    LwU32 priv_read_GA102(LwU64 addr);

    LwBool csr_validate_access_TU102(LwU32 addr);
    LwBool csr_validate_access_GA102(LwU32 addr);

    /*
        We're registered as a memory region for providing
        functions like pri
    */
    virtual LwBool test_address(LwU64 addr) override
    {
        return (addr >= mmioBase) && ((addr - mmioBase) < 0x100000000ULL);
    }

    virtual LwU8 read8(LwU64 addr) override { assert(0 && "Byte reads/writes to PRI not allowed"); return 0;}

    virtual void write8(LwU64 addr, LwU8 value) override
    {
        assert(0 && "Byte reads/writes to PRI not allowed");
    }

    virtual LwU32 read32(LwU64 addr) override { return priv_read_HAL(addr - mmioBase); }

    virtual void write32(LwU64 addr, LwU32 value) override
    {
        priv_write_HAL(addr - mmioBase, value);
    }

    void update_interrupts();
};
