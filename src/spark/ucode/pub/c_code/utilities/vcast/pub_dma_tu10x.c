/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: pub_dma_tu10x.c 
 */
// Includes
//
#include "dev_fb.h"
#include "dev_fbif_v4.h"
#include "dev_falcon_v4.h"
#include "dev_sec_csb.h"
#include "dev_bus.h"
#include "falcon_io.h"
#include <falcon-intrinsics.h>

#define HS_OVERLAY                                  ".imem_pub"

#define PUB_CSB_REG_RD32(addr)                      falc_ldxb_i_with_halt(addr)
#define PUB_CSB_REG_WR32(addr, data)                falc_stxb_i_with_halt(addr, data)

#define PUB_REG_RD32_STALL(bus,addr)                PUB_CSB_REG_RD32(addr)
#define PUB_REG_WR32_STALL(bus,addr,data)           PUB_CSB_REG_WR32(addr,data)

#define CSB_INTERFACE_MASK_VALUE                    0xffff0000U
#define CSB_INTERFACE_BAD_VALUE                     0xbadf0000U

LwU8 gp_buf_256_00        [256]  ATTR_OVLY(".data")          ATTR_ALIGNED(256);
LwU8 falcon_io__io_buffer [8192] ATTR_OVLY(".data")          ATTR_ALIGNED(256);

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
__attribute__((section(HS_OVERLAY)))
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_halt();
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        //

        falc_halt();
    }
    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
__attribute__((section(HS_OVERLAY)))
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        falc_halt();
    }
}


void *memcpy_s(void *dest, const void *src, LwU32 n)__attribute__ ((section(HS_OVERLAY)));
void *memcpy_s(void *dest, const void *src, LwU32 n)
{
    LwU32 i;
    for (i = 0; i < n; i++)
        ((char *) dest)[i] = ((char *) src)[i];

    return dest;
}

void *memset_s(void *s, char c, LwU32 n)__attribute__ ((section (HS_OVERLAY)));
void *memset_s(void *s, char c, LwU32 n)
{
    LwU32 i;

    for (i = 0; i < n; i++)
        ((char *) s)[i] = c;
    return s;
}
/*!
 * @brief Wait for BAR0 to become idle
 */
void
__attribute__((section(HS_OVERLAY)))
publibBar0WaitIdle_TU10X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using bewlo loop elsewhere.
    do
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    PUB_CSB_REG_RD32( LW_CSEC_BAR0_CSR)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            default:
                falc_halt();
        }
    }
    while (!bDone);

}

LwU32
__attribute__((section(HS_OVERLAY)))
publibBar0RegRead_TU10X
(
    LwU32 addr
)
{
    LwU32  val;

    PUB_CSB_REG_WR32(LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD));
    publibBar0WaitIdle_TU10X();
    PUB_CSB_REG_WR32(LW_CSEC_BAR0_ADDR, addr);

    PUB_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)PUB_CSB_REG_RD32( LW_CSEC_BAR0_CSR);

    publibBar0WaitIdle_TU10X();
    val = PUB_CSB_REG_RD32( LW_CSEC_BAR0_DATA);
    return val;
}

/**
 * publibBar0RegWrite_TU10X : Bar0 reg write 
 */
void
__attribute__((section(HS_OVERLAY)))
publibBar0RegWrite_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    PUB_CSB_REG_WR32(LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD));
    publibBar0WaitIdle_TU10X();
    PUB_CSB_REG_WR32(LW_CSEC_BAR0_ADDR, addr);
    PUB_CSB_REG_WR32(LW_CSEC_BAR0_DATA, data);

    PUB_CSB_REG_WR32(LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)PUB_CSB_REG_RD32( LW_CSEC_BAR0_CSR);
    publibBar0WaitIdle_TU10X();

}


/**
    pubIssueDmaSysmem_TU10x - DMA data from Falcon DMEM to specified destination in 256 byte chunks.  This function will pad
                              zeros onto the end of the copied data if the size is not a multiple of 256 bytes.

    @param destAddr         Destination address (must be 256 byte aligned)
    @param dmem_offset      Source address in DMEM where the data is located.  If this address is not 256 byte aligned
                               then this function will DMA the data by first copying it to a 256 byte aligned buffer.
    @param size             Size of the data to copy, in bytes
    @param destMediaType    Type of Destination Media.  0 = FB, 1 = System Memory.  Mirrors LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE
    @param wprId            WPR Region ID of the WPR region corresponding to destAddr.  0 indicates destAddr is not in a WPR region.

*/
void 
__attribute__((section(HS_OVERLAY)))
pubIssueDmaSysmem_TU10X
(
    LwU64 destAddr,
    LwU32 dmem_offset, 
    LwU32 size, 
    LwU8 destMediaType, 
    LwU8 wprId
)
{
 // Need a buffer to copy last chunk of data from DMEM to SYSMEM


    LwU32 data            = 0;
    LwU32 context         = 0;
    LwU16 falcCTX         = 0;
    LwU32 regionCfgMask;
    LwU64 destoff;
#define FLD_SET_REF_DEF(drf,d,v)  (((v) & ~DRF_SHIFTMASK(drf)) | (((drf ## d)&DRF_MASK(drf))<<DRF_SHIFT(drf)) )
    // DMA Setup - Step 1.  Set FBIF_TRANSCFG(0)
    data = FLD_SET_REF_DEF( LW_CSEC_FBIF_TRANSCFG_TARGET, _NONCOHERENT_SYSMEM, data);
    data = FLD_SET_REF_DEF( LW_CSEC_FBIF_TRANSCFG_MEM_TYPE, _PHYSICAL, data);

    // Writing into _FBIF_TRANSCFG(context)
    PUB_CSB_REG_WR32( ( LW_CSEC_FBIF_TRANSCFG(0) + ( context*( LW_CSEC_FBIF_TRANSCFG(1) - LW_CSEC_FBIF_TRANSCFG(0) ) ) ) , data );

    // DMA Setup - Step 2.  Set FBIF_REGIONCFG for WPR
    regionCfgMask = ~ ( 0xF << (context*4) );

    data = PUB_CSB_REG_RD32( LW_CSEC_FBIF_REGIONCFG );
    data = (data & regionCfgMask) | ((wprId & 0xF) << (context * 4));
    PUB_CSB_REG_WR32( LW_CSEC_FBIF_REGIONCFG,  data);

    // DMA Setup - Step 3.  Set up DMB and DMB1 (FB addr)
    destAddr = destAddr >> 8;
    falc_wspr(DMB, LwU64_LO32(destAddr) );  // DMB takes lower 32 bits of address
    falc_wspr(DMB1, LwU64_HI32(destAddr) );  //DMB1 takes upper 32 bits of address

    // DMA Setup - Step 4.  Set up CTX
    falc_rspr(falcCTX, CTX);  // Read existing value
    falcCTX = (((falcCTX) & ~(0x7 << 12)) | ((context) << 12));
    falc_wspr(CTX, falcCTX); // Write new value

    destoff = 0;  //current destination offset
    // Transfer as many 256 byte chunks as possible
    while ( size >= 256 ) 
    {
        // DMA from DMEM offset
        falc_dmwrite(destoff, dmem_offset | 0x00060000);  // bits [18:16] = DMA enc_size .  enc_size = 6 -> lower 8 bits of fb offset are masked (256 byte blocks)

        dmem_offset += 256;
        destoff     += 256;
        size        -= 256;
        publibBar0RegWrite_TU10X( LW_PBUS_SW_SCRATCH(60), dmem_offset );
    }

    if (size > 0) 
    {
        // For leftover data < 256 bytes, copy what's left into temp buffer then
        // DMA the full temp buffer (256 bytes) anyway
        memset_s((void*)gp_buf_256_00, '\0', sizeof(gp_buf_256_00) );  //clear temp buffer

        // Copy DMEM data into aligned temp buffer
        memcpy_s((void*)gp_buf_256_00, (void*)dmem_offset, size );
        // DMA from temp buffer                                        
        falc_dmwrite(destoff, ((LwU32)gp_buf_256_00) | 0x00060000);  // bits [18:16] = DMA enc_size .  enc_size = 6 -> lower 8 bits of fb offset are masked (256 byte blocks)
    }

    falc_dmwait();  //stall until all DMA transfers are complete
}

/*
 * pubDumpCoverageData: Dumps the coverage data at the address fectched from LW_PBUS_SW_SCRATCH registers.
 *
 */
void 
__attribute__((section(HS_OVERLAY)))
pubDumpCoverageData(void)
{
  LwU32	covDumpAddress_LSB = 0;
  LwU32	covDumpAddress_MSB = 0;
  LwU64	covDumpAddress_64 = 0;
  LwU32	covDataLength = 0;
  LwU8       *dmem_offset = (LwU8*)falcon_io__io_buffer;
  covDumpAddress_LSB = publibBar0RegRead_TU10X(LW_PBUS_SW_SCRATCH(25));
  covDumpAddress_MSB = publibBar0RegRead_TU10X(LW_PBUS_SW_SCRATCH(26));
  covDumpAddress_64 = covDumpAddress_MSB;
  covDumpAddress_64 = covDumpAddress_64<<32 | covDumpAddress_LSB;
  covDataLength = 8192;                             // This is max size falcon_io_buffer ...!!!
  publibBar0RegWrite_TU10X(LW_PBUS_SW_SCRATCH(27),covDataLength);
  publibBar0RegWrite_TU10X(LW_PBUS_SW_SCRATCH(61), (LwU32)dmem_offset);
  pubIssueDmaSysmem_TU10X(covDumpAddress_64, (LwU32)dmem_offset, covDataLength, LW_TRUE,  LW_FALSE);
}
