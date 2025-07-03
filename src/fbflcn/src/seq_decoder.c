/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "seq_decoder.h"
#include "dev_fbpa.h"
#include "dev_fbfalcon_pri.h"
#include "fbflcn_helpers.h"   // for FBFLCN_HALT
#include "dev_fbfalcon_csb.h"


#define LW_CFBFALCON_SYNC_CTRL_START        0:0
#define LW_CFBFALCON_SYNC_STATUS_RUNNING    1:1
#define LW_CFBFALCON_SYNC_STATUS_DONE       0:0

extern LwU32 gbl_dmemdata;
extern LwU32 gbl_dmemopcaddr;
extern LwU32 gbl_seq_cnt;
extern LwU16 gbl_prev_fbio_idx;
extern LwU32 _sequencerDataStart;

LwU32 crcTable[256];

void
seq_add_stall
(
    LwU32 cmd_addr
)
{
    LwU32 addr = cmd_addr & 0x00FFFFFF;
    cmd_addr &= 0xFF000000;

//    if (((addr >= 0x9a0b04) && (addr < 0x9a0df4)) ||
//        (addr == 0x9a060c) || (addr == 0x9a065c) ||
//        (addr == 0x9a0800) || (addr == 0x9a0830)  || (addr == 0x9a0890))
    if (((addr >= LW_PFB_FBPA_FBIOTRNG_SUBP0_WCK) && (addr < LW_PFB_FBPA_FBIO_SUBP1BYTE3_RX_DFE2)) ||
        (addr == LW_PFB_FBPA_FBIO_SPARE) || (addr == LW_PFB_FBPA_FBIO_DELAY) ||
        (addr == LW_PFB_FBPA_FBIO_CFG8) || (addr == LW_PFB_FBPA_FBIO_CFG1) || (addr == LW_PFB_FBPA_FBIO_PWR_CTRL))
    {
        // check on RMW completes previous writes first?
        // possibly reverse this logic and use SEQ_CMD_OPCODE_STORE
        if ((cmd_addr == SEQ_CMD_OPCODE_WR_STALL) || (cmd_addr == SEQ_CMD_OPCODE_RMW))
        {
            gbl_prev_fbio_idx = 0;
        }
        else
        {
            gbl_prev_fbio_idx = gbl_pa_instr_cnt;
        }
    }
    else if (gbl_prev_fbio_idx != 0) 
    {
        // set address in index to be stall
        gbl_pa_mclk_sw_seq[gbl_prev_fbio_idx] = SEQ_CMD_OPCODE_WR_STALL | (gbl_pa_mclk_sw_seq[gbl_prev_fbio_idx] & 0x00FFFFFF);
        // clear prev_fbio_idx
        gbl_prev_fbio_idx = 0;
    }
        
    return;
}

void
seq_pa_write1
(
    LwU32 data
)
{
    if ((gbl_pa_instr_cnt+1) > PA_MAX_SEQ_SIZE)
    {
        FW_MBOX_WR32(13,gbl_pa_instr_cnt);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_SIZE_ERROR);
    }
    // not used for writing registers (either wait or end seq)
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = data;
}

void
seq_pa_write
(
    LwU32 addr,
    LwU32 data
)
{
    if ((gbl_pa_instr_cnt+2) > PA_MAX_SEQ_SIZE)
    {
        FW_MBOX_WR32(13,gbl_pa_instr_cnt);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_SIZE_ERROR);
    }

    seq_add_stall(addr);
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = addr;
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = data;
}

void
seq_pa_write_mask
(
    LwU32 addr,
    LwU32 data,
    LwU32 mask
)
{
    if ((gbl_pa_instr_cnt+3) > PA_MAX_SEQ_SIZE)
    {
        FW_MBOX_WR32(13,gbl_pa_instr_cnt);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_SIZE_ERROR);
    }
    seq_add_stall(addr);
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = addr;
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = data;
    gbl_pa_mclk_sw_seq[gbl_pa_instr_cnt++] = mask;
}


LwU32
gen_seq_crc32
//crc32_halfbyte (const void* loc, size_t size, LwU32 prevCrc32)
(
    const LwU32* loc,
    LwU32 size          // in dw
)
{
    LwU32 idx;
    LwU32 byte;
    LwU32 crc;

    // gen CRC table
    // using x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
    // check if we need to generate table (2nd term should not be 0 if generated)
    //if (crcTable[1] == 0)
    if (crcTable[1] != 0x77073096)
    {
        for (byte=0; byte < 256; byte++)
        {
            crc = byte;
            for (idx = 0; idx < 8; idx++)
            {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320L : 0);
            }
            crcTable[byte] = crc;
        }
    }


    // start CRC callwlation
    //
    crc = 0xFFFFFFFF;
    size *= 4;      // # of bytes

    unsigned char* current = (unsigned char*) loc;
    while (size--)
    {
        crc = (crc >> 8) ^ crcTable[(crc & 0xFF) ^ *current++];
    }

/*
// halfbyte algo
    LwU32 crc = ~prevCrc32;
    LwU8* lwrByte = (unsigned char*) loc;

    static LwU32 crcTable[16] =
    {
        0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC,
        0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
        0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,
        0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C
    };

    while (size--)
    {
        crc = crcTable[(crc ^  *lwrByte      ) & 0x0F] ^ (crc >> 4);
        crc = crcTable[(crc ^ (*lwrByte >> 4)) & 0x0F] ^ (crc >> 4);
        lwrByte++;
    }
*/

    return ~crc;
}

LwU32
seqHalt
(
    LwU32 pc,
    LwU32 errData0,
    LwU32 errData1,
    LwU32 errCode
)
{
    LwU32 status = REG_RD32(CSB, LW_CFBFALCON_SYNC_STATUS);

    // clear running bit, set errCode & DONE bit
    FW_MBOX_WR32(11, status);
    status = (status & 0x0000FFFD) | errCode | SEQ_DONE;
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS, status);
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS2, status);   // let FB know
    FW_MBOX_WR32(12, errData0);
    FW_MBOX_WR32(13, errData1);
    FW_MBOX_WR32(14, pc);
    FW_MBOX_WR32(15, errCode);
    falc_halt();
    return status;
}

LW_STATUS
paDecode
(
    LwU32 *DecodeDmemStartAdr,
    LwU32 size
)
{
    LwU32 dmemdata;

    LwU32 opCode;
    LwU32 addr;
    LwU32 val;
    LwU32 mask;
    LwU32 ptimer0_start;
    LwBool timer_ok;
    LW_STATUS status = LW_OK;
    LwBool running = LW_TRUE;
    LwU32 subp0_status;
    LwU32 subp1_status;
    LwU32 i = 0;
    LwU32 wait_timer=0;
    LwU32 rd_val;

    extern PAFLCN_SEQUENCER_INFO_FIELD seqInfoField;

    do {
        dmemdata = *DecodeDmemStartAdr;
        opCode = dmemdata & 0xFF000000;
        addr = dmemdata & 0xFFFFFF;
        gbl_dmemopcaddr = dmemdata;
        gbl_seq_cnt = i;

        // Note: SEQ_CMD_OPCODE_START done in paStartSeq
        
        switch (opCode) {
            case SEQ_CMD_OPCODE_STORE:
                DecodeDmemStartAdr += 0x1;
                val = *(DecodeDmemStartAdr);
                gbl_dmemdata = val;
                i++;
                REG_WR32(CSB,addr,val);
                break;

            case SEQ_CMD_OPCODE_RMW_L:
                //FW_MBOX_WR32(7, seqInfoField.half_subp);
                if (seqInfoField.half_subp != 0) {
                    DecodeDmemStartAdr += 0x2;  // skip next 2dw
                    i = i + 2;
                    break;
                }
            case SEQ_CMD_OPCODE_RMW:
                DecodeDmemStartAdr += 0x1;
                val = *(DecodeDmemStartAdr);
                gbl_dmemdata = val;
                DecodeDmemStartAdr += 0x1;
                mask = *(DecodeDmemStartAdr);
                i = i + 2;

                val = val & mask;   // needed? also clear below needed?
                val |= (REG_RD32(CSB,addr) & (~mask));  // rd & clear
                REG_WR32_STALL(CSB,addr,val);
                break;

            case SEQ_CMD_OPCODE_WAIT:
                wait_timer = addr << 5;
                ptimer0_start = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);          // set new timer
                do {
                    val = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
                    gbl_dmemdata = val;
                    if (REF_VAL(LW_CFBFALCON_SYNC_CTRL_START, REG_RD32(CSB, LW_CFBFALCON_SYNC_CTRL)) != 0x1)
                    {
                        // debug interval count
                        FW_MBOX_WR32(12, ptimer0_start);
                        FW_MBOX_WR32(13, wait_timer);       
                        FW_MBOX_WR32(14, val);       
                        status = PA_FB_REQ_HALT;
                        return seqHalt(i, opCode, wait_timer, status);
                    }
                } while ((val - ptimer0_start) < wait_timer);
                break;

            case SEQ_CMD_OPCODE_WR_STALL:
                DecodeDmemStartAdr += 0x1;
                val = *(DecodeDmemStartAdr);
                gbl_dmemdata = val;
                REG_WR32_STALL(CSB,addr,val);
                i++;
                break;
                
            case SEQ_CMD_OPCODE_POLL:
                DecodeDmemStartAdr += 0x1;
                val = *(DecodeDmemStartAdr);
                gbl_dmemdata = val;
                DecodeDmemStartAdr += 0x1;
                mask = *(DecodeDmemStartAdr);
                val = val & mask;       // needed?
                timer_ok = LW_TRUE;
                FW_MBOX_WR32(6, val);
                FW_MBOX_WR32(7, addr);       
                FW_MBOX_WR32(8, 0xba5eba11);       
                FW_MBOX_WR32(9, i);       
                ptimer0_start = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);          // set new timer
                rd_val = (mask & REG_RD32(CSB,addr));
                FW_MBOX_WR32(8, rd_val);       
                wait_timer = 0;
                LwBool run_timer_check = LW_TRUE;
                // disable timer check if fb stop poll
                if ((addr == LW_CFBFALCON_SYNC_CTRL) && ((val & 0xFF00) == 0x1000))
                {
                    run_timer_check = LW_FALSE;
                }
                while (timer_ok && (rd_val != val)) {
                    
                    if (run_timer_check) {
                        wait_timer = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
                        if ((wait_timer - ptimer0_start) >= POLL_TIMER_VAL) {
                            //timer_ok = LW_FALSE;
                            //status = PA_ERR_POLL_TIMER_EXPIRED;
                            FW_MBOX_WR32(3, ptimer0_start);
                            FW_MBOX_WR32(4, POLL_TIMER_VAL);       
                            FW_MBOX_WR32(5, wait_timer);       
                            return seqHalt(GET_PC(), addr, val, PA_ERR_POLL_TIMER_EXPIRED);
                        }
                    }

                    if (REF_VAL(LW_CFBFALCON_SYNC_CTRL_START, REG_RD32(CSB, LW_CFBFALCON_SYNC_CTRL)) != 0x1) {
                        status = PA_FB_REQ_HALT;
                        return seqHalt(GET_PC(), opCode, addr, status);
                    }
                    else {
                        rd_val = (mask & REG_RD32(CSB,addr));
                    }
                    FW_MBOX_WR32(8, rd_val);       
                }
                i = i + 2;
                break;

// polls until not running then if error, sets error bits in status register
            case SEQ_CMD_OPCODE_TRNCHK:
                running = LW_TRUE;
                timer_ok = LW_TRUE;
                wait_timer = 0;
                ptimer0_start = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);          // set new timer

                while (timer_ok && running)
                {
                    val = REG_RD32(CSB, addr);  // expected LW_PFB_FBPA_TRAINING_STATUS
                    subp0_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, val);
                    subp1_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, val);
                    if ((subp0_status != LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_RUNNING) &&
                        (subp1_status != LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_RUNNING)) {
                        running = LW_FALSE;
                    }
                    else if (REF_VAL(LW_CFBFALCON_SYNC_CTRL_START, REG_RD32(CSB, LW_CFBFALCON_SYNC_CTRL)) != 0x1)
                    {
                        status = PA_FB_REQ_HALT;
                        return seqHalt(i, opCode, addr, status);
                    }
                    else {
                        wait_timer = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0) - ptimer0_start;
                        if (wait_timer >= POLL_TIMER_VAL) {
                            //timer_ok = LW_FALSE;
                            status = PA_ERR_TRNCHK_TIMER_EXPIRED;
                            return seqHalt(i, addr, val, status);
                        }
                    }
                }

                if ((subp0_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_ERROR) ||
                    (subp1_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_ERROR)) {
                    status = PA_ERR_TRAINING;
                    return seqHalt(i, addr, val, status);
                }
                break;

            case SEQ_CMD_OPCODE_END:
                // set run=0, done=1
                val = (REG_RD32(CSB, LW_CFBFALCON_SYNC_STATUS) & 0xFFFFFFFC) | 0x1;
                REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS, val);
                break;

            default:
            // invalid command...
                status = PA_ERR_ILWALID_CMD;
                return seqHalt(i, (LwU32)&DecodeDmemStartAdr, dmemdata, status);
                break;
        }

        DecodeDmemStartAdr += 0x1;
        i++;
    } while ((status == LW_OK) && (i < size));

    return status;
}

LW_STATUS
paStartSeq
(
    void
)
{
    // get timer & subp
    //   set  status segid=0, running=1(bit1)
    //     do pa_decode

    LW_STATUS status = LW_OK;

    // pa falcon build code get location of sequence buffer from linker script: gt_ga1x_paflcn_link_script.ld

    LwU32 gen_crc;
    LwU32 check_crc;
    LwU32 size;
    LwU32 data;
    LwU32 startOpCode;
    LwU32 sync_status;

    extern LwU32 gbl_pa_seq[PA_MAX_SEQ_SIZE];

    gbl_dmemopcaddr=0;
    gbl_dmemdata=0;
    gbl_seq_cnt=0;

    // poll ctrl segid=0, start=1(bit0)
    do
    {
        data = REG_RD32(CSB, LW_CFBFALCON_SYNC_CTRL);

        // for unload
        if ((data & 0xFFFF0000) != 0)
        {
            seqHalt(GET_PC(), 0, 0, PA_FB_REQ_HALT);
        }
    }
    while (data != 0x1);
    
    // verify start seq opcode, get crc, size
    data = gbl_pa_seq[0];
    startOpCode = data & 0xFF000000;
    size = data & 0x00FFFFFF;

    if (size >= PA_MAX_SEQ_SIZE)
    {
        return seqHalt(GET_PC(), (LwU32) &_sequencerDataStart, size, PA_ERR_SIZE_FAULT);
    }
    if (startOpCode != SEQ_CMD_OPCODE_START)
    {
        return seqHalt(GET_PC(), (LwU32) &_sequencerDataStart, data, PA_ERR_NO_START);
    }

    check_crc = gbl_pa_seq[1];

    // verify crc
    gen_crc = gen_seq_crc32(&gbl_pa_seq[2], (size-2));
    if (check_crc != gen_crc)
    {
        return seqHalt(GET_PC(), check_crc, gen_crc, PA_ERR_BAD_CRC);
    }

    sync_status = REG_RD32(CSB, LW_CFBFALCON_SYNC_STATUS);
    sync_status = FLD_SET_DRF_NUM(_CFBFALCON, _SYNC_STATUS, _RUNNING, 0x1, sync_status);
    sync_status = FLD_SET_DRF_NUM(_CFBFALCON, _SYNC_STATUS, _DONE, 0x0, sync_status);
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS, sync_status);
    status = paDecode(&gbl_pa_seq[2], (size-2));
// TODO: possibly clear the array?

    return status;
}
