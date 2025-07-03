#include "libriscv.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include <assert.h>
#include "peregrine-headers.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "gsp.h"


void gsp::csr_write_GA102(LwU32 addr, LwU64 value)
{
    switch (addr)
    {
    case LW_RISCV_CSR_SMPUIDX2:
        assert(value == 0 || value == 1);
        xmpuidx2 = value;
        break;

    case LW_RISCV_CSR_SMPUVLD:
        for (int i = 0; i < 64; i++)
          mmpuVa[xmpuidx2*64 + i] = (mmpuVa[xmpuidx2*64 + i] & ~1ULL) | ((value >> i) & 1);
        break;

    case LW_RISCV_CSR_SSCRATCH:
        assert(privilege >= privilege_supervisor);
        xscratch = value;
        break;

    case LW_RISCV_CSR_SIE:
        assert(privilege >= privilege_supervisor);
        xie = value;
        return;

    case LW_RISCV_CSR_SIP:
        assert(privilege >= privilege_supervisor);
        xip = value;
        return;

    case LW_RISCV_CSR_SCOUNTEREN:
        assert(privilege >= privilege_supervisor);
        mucounteren = value;
        return;

    case LW_RISCV_CSR_SSTATUS:
        assert(privilege >= privilege_supervisor);
        mstatus = value;
        break;

    case LW_RISCV_CSR_STVEC:
        assert(privilege >= privilege_supervisor);
        xtvec = value;
        break;

    case LW_RISCV_CSR_SEPC:
        assert(privilege >= privilege_supervisor);
        xepc = value;
        break;

    case LW_RISCV_CSR_STVAL:
        assert(privilege >= privilege_supervisor);
        assert(value == xtval);
        break;

    case LW_RISCV_CSR_SCAUSE:
        assert(privilege >= privilege_supervisor);
        assert(value == xcause);
        break;

    case LW_RISCV_CSR_SATP:
        assert(privilege >= privilege_supervisor);
        satp = value;
        mpu_translation_enabled = DRF_VAL64(_RISCV_CSR, _SATP, _MODE, satp) == LW_RISCV_CSR_SATP_MODE_LWMPU;
        break;

    case LW_RISCV_CSR_SMPUIDX:
        assert(privilege >= privilege_supervisor);
        mmpuIndex = value;
        break;

    case LW_RISCV_CSR_SMPUPA:
        assert(privilege >= privilege_supervisor);
        mmpuPa[mmpuIndex] = value;
        break;

    case LW_RISCV_CSR_SMPURNG:
        assert(!(value & (LIBOS_CONFIG_MPU_PAGESIZE-1)) && "Invalid SMPU range");
        mmpuRange[mmpuIndex] = value;
        break;

    case LW_RISCV_CSR_SMPUATTR:
        assert(privilege >= privilege_supervisor);
        mmpuAttr[mmpuIndex] = value;
        break;

    case LW_RISCV_CSR_SMPUVA:
        assert(privilege >= privilege_supervisor);
        mmpuVa[mmpuIndex] = value;
        break;

    default:
        fprintf(stderr,"Unknown CSR: %08X\n", addr);
        exit(1);
        break;
    }
}

LwU64 gsp::csr_read_GA102(LwU32 addr)
{
    switch (addr)
    {
    case LW_RISCV_CSR_SMPUIDX2:
        return xmpuidx2;
        break;

    case LW_RISCV_CSR_SMPUVLD:
    {
        LwU64 value = 0;
        for (int i = 0; i < 64; i++)
          value |= (mmpuVa[xmpuidx2*64 + i] & 1ULL) << i;
        return value;
    }

    case LW_RISCV_CSR_SSCRATCH:
        assert(privilege >= privilege_supervisor);
        return xscratch;

    case LW_RISCV_CSR_SCOUNTEREN:
        assert(privilege >= privilege_supervisor);
        return mucounteren;

    case LW_RISCV_CSR_HPMCOUNTER_CYCLE:
        return cycle;

    case LW_RISCV_CSR_SSTATUS:
        assert(privilege >= privilege_supervisor);
        return mstatus;

    case LW_RISCV_CSR_STVEC:
        assert(privilege >= privilege_supervisor);
        return xtvec;

    case LW_RISCV_CSR_SEPC:
        assert(privilege >= privilege_supervisor);
        return xepc;

    case LW_RISCV_CSR_SCAUSE:
        assert(privilege >= privilege_supervisor);
        return xcause;

    case LW_RISCV_CSR_STVAL:
        assert(privilege >= privilege_supervisor);
        return xtval;

    case LW_RISCV_CSR_HPMCOUNTER_TIME:
        return mtime();

    case LW_RISCV_CSR_SIP:
        assert(privilege >= privilege_supervisor);
        return xip;

    case LW_RISCV_CSR_SIE:
        assert(privilege >= privilege_supervisor);
        return xie;

    case LW_RISCV_CSR_SATP:
        assert(privilege >= privilege_supervisor);
        return satp;

    case LW_RISCV_CSR_SMPUIDX:
        assert(privilege >= privilege_supervisor);
        return mmpuIndex;

    case LW_RISCV_CSR_SMPUPA:
        assert(privilege >= privilege_supervisor);
        return mmpuPa[mmpuIndex];

    case LW_RISCV_CSR_SMPURNG:
        assert(privilege >= privilege_supervisor);
        return mmpuRange[mmpuIndex];

    case LW_RISCV_CSR_SMPUATTR:
        assert(privilege >= privilege_supervisor);
        return mmpuAttr[mmpuIndex];

    case LW_RISCV_CSR_SMPUVA:
        assert(privilege >= privilege_supervisor);
        return mmpuVa[mmpuIndex];

    default:
        fprintf(stderr,"Unknown CSR: %08X\n", addr);
        exit(1);
    }
}

LwU32 gsp::priv_read_GA102(LwU64 addr)
{
    switch (addr)
    {
    case LW_PRGNLCL_FALCON_IRQSTAT:
        return irqSet;
        
    case LW_PRGNLCL_FALCON_DMACTL:
        return falconDmactl;

    case LW_PRGNLCL_FBIF_CTL:
        return fbifCtl;

    case LW_PRGNLCL_FBIF_TRANSCFG(0):
    case LW_PRGNLCL_FBIF_TRANSCFG(1):
    case LW_PRGNLCL_FBIF_TRANSCFG(2):
    case LW_PRGNLCL_FBIF_TRANSCFG(3):
    case LW_PRGNLCL_FBIF_TRANSCFG(4):
    case LW_PRGNLCL_FBIF_TRANSCFG(5):
    case LW_PRGNLCL_FBIF_TRANSCFG(6):
    case LW_PRGNLCL_FBIF_TRANSCFG(7):
        return fbifTranscfg[(addr - LW_PRGNLCL_FBIF_TRANSCFG(0)) / 4];

    case LW_PRGNLCL_FALCON_MAILBOX0:
        return mailbox0;

    case LW_PRGNLCL_FALCON_MAILBOX1:
        return mailbox1;

    case LW_PRGNLCL_FALCON_DMATRFCMD:
        return trfCmd;

    case LW_PRGNLCL_GDMA_CFG:
        return DRF_DEF(_PRGNLCL_GDMA, _CFG, _CHANS, _VAL);
    
    case LW_PRGNLCL_GDMA_IRQ_CTRL:
        return gdmaIrqCtrl;

    case LW_PRGNLCL_GDMA_CHAN_STATUS(0):
    case LW_PRGNLCL_GDMA_CHAN_STATUS(1):
        return gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_STATUS(0)) / 4096].status;

    case LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(0):
    case LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(1):
        return gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(0)) / 4096].reqProduce;

    case LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(0):
    case LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(1):
        return gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(0)) / 4096].reqComplete;

    default:
        printf("Invalid pri register: %016llx\n", addr);
        assert(0);
    }
}

void gsp::priv_write_GA102(LwU64 addr, LwU32 value)
{
    switch (addr)
    {
    case LW_PRGNLCL_RISCV_IRQMCLR:
        irqMask &= ~value;
        break;

    case LW_PRGNLCL_RISCV_IRQMSET:
        irqMask |= value;
        break;
        
    case LW_PRGNLCL_RISCV_PRIV_ERR_STAT:
        privErrStat = value;
        break;

    case LW_PRGNLCL_RISCV_HUB_ERR_STAT:
        hubErrStat = value;
        break;

    case LW_PRGNLCL_FALCON_IRQSCLR:
        irqSet &= ~value;
        break;

    case LW_PRGNLCL_FALCON_MAILBOX0:
        mailbox0 = value;
        break;

    case LW_PRGNLCL_FALCON_MAILBOX1:
        mailbox1 = value;
        break;

    case LW_PRGNLCL_FALCON_DMACTL:
        falconDmactl = value;
        break;

    case LW_PRGNLCL_FBIF_CTL:
        fbifCtl = value;
        break;

    case LW_PRGNLCL_FBIF_TRANSCFG(0):
    case LW_PRGNLCL_FBIF_TRANSCFG(1):
    case LW_PRGNLCL_FBIF_TRANSCFG(2):
    case LW_PRGNLCL_FBIF_TRANSCFG(3):
    case LW_PRGNLCL_FBIF_TRANSCFG(4):
    case LW_PRGNLCL_FBIF_TRANSCFG(5):
    case LW_PRGNLCL_FBIF_TRANSCFG(6):
    case LW_PRGNLCL_FBIF_TRANSCFG(7):
        fbifTranscfg[(addr - LW_PRGNLCL_FBIF_TRANSCFG(0)) / 4] = value;
        break;

    case LW_PRGNLCL_FALCON_DMATRFBASE:
        trfBase = value;
        break;

    case LW_PRGNLCL_FALCON_DMATRFMOFFS:
        trfMoffs = value;
        break;

    case LW_PRGNLCL_FALCON_DMATRFFBOFFS:
        trfFboffs = value;
        break;

    case LW_PRGNLCL_FALCON_DMATRFBASE1:
        trfBase1 = value;
        break;

    case LW_PRGNLCL_FALCON_DMATRFCMD:
        trfCmd = value;
        dmaTransfer_HAL();
        break;

    case LW_PRGNLCL_GDMA_IRQ_CTRL:
        gdmaIrqCtrl = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(0):
    case LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(0)) / 4096].commonConfig = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(0):
    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(0)) / 4096].transConfig0 = value;
        gdmaTransfer_HAL((addr - LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(0)) / 4096);
        break;

    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(0):
    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(0)) / 4096].transConfig1 = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(0):
    case LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(0)) / 4096].transConfig2 = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(0):
    case LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(0)) / 4096].srcAddr = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(0):
    case LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(0)) / 4096].srcAddrHi = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(0):
    case LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(0)) / 4096].destAddr = value;
        break;

    case LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(0):
    case LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(1):
        gdmaChan[(addr - LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(0)) / 4096].destAddrHi = value;
        break;

    default:
        printf("Invalid pri register: %016llx\n", addr);
        assert(0);
    }
}

LwBool
gsp::translate_address_GA102(LwU64 va, LwU64 &pa, LwBool write, LwBool execute, LwBool override_selwrity)
{
    if (DRF_VAL64(_RISCV_CSR, _SATP, _MODE, satp) == LW_RISCV_CSR_SATP_MODE_LWMPU)
    {
        for (int i = 0; i < LIBOS_CONFIG_MPU_INDEX_COUNT; i++)
        {
            if (!(mmpuVa[i] & 1))
                continue;

            LwU64 entry_va = mmpuVa[i] & ~ (LIBOS_CONFIG_MPU_PAGESIZE-1);
            if (va >= entry_va && (va - entry_va) < mmpuRange[i])
            {
                LwBool allowed;
                if (execute)
                    if (privilege == privilege_supervisor)
                        allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _SX, mmpuAttr[i]);
                    else
                        allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _UX, mmpuAttr[i]);
                else if (write)
                    if (privilege == privilege_supervisor)
                        allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _SW, mmpuAttr[i]);
                    else
                        allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _UW, mmpuAttr[i]);
                else if (privilege == privilege_supervisor)
                    allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _SR, mmpuAttr[i]);
                else
                    allowed = DRF_VAL64(_RISCV, _CSR_SMPUATTR, _UR, mmpuAttr[i]);

                if (!allowed && !override_selwrity)
                {
                    return LW_FALSE;
                }

                pa = va - entry_va + mmpuPa[i];
                return LW_TRUE;
            }
        }
        return LW_FALSE;
    }
    else
    {
        // MPU is disabled
        pa = va;
        return LW_TRUE;
    }
}

void gsp::dmaTransfer_GA102(void)
{
    LwU64 src, dest;
    LwU64 tcmAperture;
    LwU32 size;
    LwU32 i;

    // colwert size register value to bytes
    size = DRF_VAL(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, trfCmd);
    size = 1 << (size + 2);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _TRUE, trfCmd))
    {
        tcmAperture = LW_RISCV_AMAP_IMEM_START;
    }
    else
    {
        tcmAperture = LW_RISCV_AMAP_DMEM_START;
    }

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, trfCmd))
    {
        src  = tcmAperture + trfMoffs;
        dest = ((((LwU64)trfBase1) << 32) | trfBase) << 8;
        dest += trfFboffs;
    }
    else
    {
        src = ((((LwU64)trfBase1) << 32) | trfBase) << 8;
        src += trfFboffs;
        dest = tcmAperture + trfMoffs;
    }

    lwstom_memory *src_memory = memory_for_address(src);
    lwstom_memory *dst_memory = memory_for_address(dest);

    if (!src_memory || !dst_memory) {
      fprintf(stderr, "Invalid DMA source or destination\n");
      exit(1);
    }

    lwstom_memory *remote;

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, trfCmd))
        remote = dst_memory;
    else
        remote = src_memory;

    // Verify the programming of FBIF
    LwU32 dmaIndex = DRF_VAL(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, trfCmd);
    assert(dmaIndex < 8 && "FBIF CTXDMA index out of range");

    // @todo Add more memory types besides IMEM/DMEM and VIDMEM
    // @todo Query remote object for memory type
    if (remote->get_memory_kind() == 'FB')
    {
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, fbifTranscfg[dmaIndex]));
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, fbifTranscfg[dmaIndex]));
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0, fbifTranscfg[dmaIndex]));
    }
    else if (remote->get_memory_kind() == 'SYS')
    {
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM, fbifTranscfg[dmaIndex]));
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, fbifTranscfg[dmaIndex]));
        assert(FLD_TEST_DRF(_PRGNLCL, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, _BAR2_FN0, fbifTranscfg[dmaIndex]));
    }
    else
    {
        assert(0 && "DMA to unsupported memory type");
    }

    for (i = 0; i < size; i++)
    {
        dst_memory->write8(dest + i, src_memory->read8(src + i));
    }

    trfCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _TRUE, trfCmd);
}

void gsp::gdmaTransfer_GA102(LwU8 chanIdx)
{
    gsp_gdma_chan_state &state = gdmaChan[chanIdx];
    LwU64 dstPa, srcPa;
    LwU32 size, i;
    lwstom_memory *dstMemory, *srcMemory;

    if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LAUNCH, _FALSE,
        state.transConfig0))
    {
        return;
    }

    dstPa = (state.destAddrHi << 32) | state.destAddr;
    srcPa = (state.srcAddrHi  << 32) | state.srcAddr;
    size = DRF_VAL(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LENGTH, state.transConfig0) * 4;

    if (size == 0)
    {
        state.status |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_CAUSE, _ILWALID_COMB);
        goto returnError;
    }

    dstMemory = memory_for_address(dstPa);
    srcMemory = memory_for_address(srcPa);

    if (!dstMemory || !srcMemory)
    {
        state.status |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_CAUSE, _PA_FAULT);
        goto returnError;
    }

    state.reqProduce++;

    for (i = 0; i < size; i++)
    {
        LwU64 dst = dstPa + i;
        LwU64 src = srcPa + i;
        if (!dstMemory->test_address(dst) || !srcMemory->test_address(src))
        {
            state.status |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_CAUSE, _PA_FAULT);
            goto returnError;
        }

        dstMemory->write8(dst, srcMemory->read8(src));
    }

    state.reqComplete = state.reqProduce;

    return;

returnError:
    state.status |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE);
}

LwBool gsp::csr_validate_access_GA102(LwU32 addr)
{
    if (privilege == privilege_supervisor)
        return LW_TRUE;

    switch (addr)
    {
    case LW_RISCV_CSR_HPMCOUNTER_CYCLE:
        return REF_VAL64(LW_RISCV_CSR_SCOUNTEREN_CY, mucounteren);

    case LW_RISCV_CSR_HPMCOUNTER_TIME:
        return REF_VAL64(LW_RISCV_CSR_SCOUNTEREN_TM, mucounteren);
    }

    return LW_FALSE;
}

void gsp::update_interrupts()
{
    if (irqSet & irqMask) 
    {
        if (has_supervisor_mode) 
            xip |= DRF_NUM(_RISCV, _CSR_SIP, _SEIP, 1);
        else
            xip |= DRF_NUM(_RISCV, _CSR_MIP, _MEIP, 1);
    }
    else
    {
        if (has_supervisor_mode) 
            xip &= ~DRF_NUM(_RISCV, _CSR_SIP, _SEIP, 1);
        else
            xip &= ~DRF_NUM(_RISCV, _CSR_MIP, _MEIP, 1);        
}
}

