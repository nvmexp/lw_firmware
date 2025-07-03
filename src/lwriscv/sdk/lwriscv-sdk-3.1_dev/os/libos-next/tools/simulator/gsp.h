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

#if LIBOS_CONFIG_GDMA_SUPPORT
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

#endif

struct gsp : 
    riscv_core, 
    lwstom_memory, 
    gsp_mmpu_state, 
#if LIBOS_CONFIG_GDMA_SUPPORT
    gsp_gdma_state,
#endif    
    gsp_falcon_state
{
    trivial_memory                  mmpuIMEM;
    trivial_memory                  mmpuDMEM;
    trivial_memory                  fb;

    LwU64 mmioBase;

    // Used to set a0/a4/a5 for partition "start" message
    // @todo: Need to abstract this
    LwU64 heapBase, heapSize;

    // PRI memory binding
    virtual const uint32_t get_memory_kind() { return 0; }

    virtual void soft_reset()
    {
        riscv_core::reset();

        // Reset the MMPU and falcon state
        (gsp_mmpu_state &)*this   = gsp_mmpu_state();
        (gsp_falcon_state &)*this = gsp_falcon_state();

        // jump back to the boot-vector
        programCounter = pc = partition[partitionIndex].entry;
    }

    void wire_hals()
    {
        priv_write_HAL        = bind_hal(&gsp::priv_write, this);
        priv_read_HAL         = bind_hal(&gsp::priv_read, this);
        csr_write_HAL         = bind_hal(&gsp::csr_write, this);
        csr_read_HAL          = bind_hal(&gsp::csr_read, this);
        csr_validate_access_HAL = bind_hal(&gsp::csr_validate_access, this);
        
#if LIBOS_LWRISCV == LIBOS_LWRISCV_1_0
            mmioBase              = 0x2000000000000000ull;
            has_supervisor_mode   = false;
#elif LIBOS_LWRISCV == LIBOS_LWRISCV_1_1
            mmioBase              = 0x2000000000000000ull;
            has_supervisor_mode   = false;
#else
            mmioBase              = 0; // PRGNLCL register addresses are absolute addresses.
            has_supervisor_mode   = true;
#endif
    }

    gsp(image *firmware)
        : riscv_core(firmware),
          mmpuIMEM('ITCM', LW_RISCV_AMAP_IMEM_START, LIBOS_CONFIG_IMEM_SIZE),
          mmpuDMEM('DTCM', LW_RISCV_AMAP_DMEM_START, LIBOS_CONFIG_DMEM_SIZE),
          fb('FB', LW_RISCV_AMAP_FBGPA_START, 1ULL << 32)
    {
#if !LIBOS_CONFIG_RISCV_S_MODE
            privilege = privilege_machine;
#else
            privilege = privilege_supervisor;
#endif
        memory_regions.push_back(&mmpuIMEM);
        memory_regions.push_back(&mmpuDMEM);
        memory_regions.push_back(&fb);
        memory_regions.push_back(this); // PRI

        wire_hals();
    }

    //
    //  HAL functions
    //
    void dmaTransfer();
#if LIBOS_CONFIG_GDMA_SUPPORT
    void gdmaTransfer(LwU8 chanIdx);
#endif
    LwBool probe_address(LwU64 i, LwU64 va, LwU64 &pa, TranslationKind & mask);
    
    // RISCV-CORE expects these
    virtual LwBool translate_address(LwU64 va, LwU64 &pa, TranslationKind & mask);
    virtual LwBool csr_validate_access(LwU32 addr);

    void pmp_assert_partition_has_access(LwU64 physical, LwU64 size, TranslationKind kind);
    std::function<void(LwU64 addr, LwU32 value)> priv_write_HAL;
    std::function<LwU32(LwU64 addr)>             priv_read_HAL;

    void priv_write(LwU64 addr, LwU32 value);
    void csr_write(LwU32 addr, LwU64 value);
    LwU64 csr_read(LwU32 addr);
    LwU32 priv_read(LwU64 addr);

    /*
        We're registered as a memory region for providing
        functions like pri
    */
    virtual LwBool test_address(LwU64 addr) override
    {
        return (addr >= mmioBase) && ((addr - mmioBase) < 0x100000000ULL);
    }

    virtual LwU8 read8(LwU64 addr) override {
        assert(0 && "Byte reads/writes to PRI not allowed"); 
        return 0;
    }

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
