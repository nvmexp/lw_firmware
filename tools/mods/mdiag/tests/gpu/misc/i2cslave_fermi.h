/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _I2CSLAVE_FERMI_H_
#define _I2CSLAVE_FERMI_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"

#define RMA_SEL_DATA32  0x6
#define RMA_SEL_DATA32  0x6
#define AR_IS_IO        1
#define AR_ISNT_IO      0
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE           0
#define TRUE            1

#define  DEVICE_ADDR_3_FERMI  0x4c //7'b1001_100
#define  DEVICE_ADDR_2_FERMI  0x4d //7'b1001_101
#define  DEVICE_ADDR_1_FERMI  0x4e //7'b1001_110
#define  DEVICE_ADDR_0_FERMI  0x4f //7'b1001_111
#define  READ_BYTE       0x1
#define  WRITE_BYTE      0x2
#define  READ_BYTE_CRC   0x3
#define  WRITE_BYTE_CRC  0x4
#define  READ_WORD       0x9
#define  WRITE_WORD      0xa
#define  READ_WORD_CRC   0xb
#define  WRITE_WORD_CRC  0xc
#define  PRE_ARP         0x1
#define  RST_ARP         0x2
#define  GET_UID         0x3
#define  SET_UID         0x4
#define  GET_UID_DIR     0x5
#define  RST_ARP_DIR     0x6

#define LW_PBUS_DEBUG_1 0x00001084

#define STRAP_OVERWRITE             31
#define STRAP_SMB_ALT_ADDR          24

class i2cslave_fermi : public LWGpuSingleChannelTest {
public:
    i2cslave_fermi(ArgReader *params);

    virtual ~i2cslave_fermi(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    int checkSanity();
//    int i2cRdByte();
    int i2cRdByte( int i2c_dev,  int i2c_adr, UINT32 *i2c_rdat,int use32);
    int i2cWrByte( int i2c_dev,  int i2c_adr, UINT32 i2c_wdat,int use32);
    int i2cSetup();
    int i2cPriEn( int i2c_dev);
    int testBasic();
    int testscratch();
    int testCheckGpio();
    int testPriAccess(int i2c_dev);
    int setupPriRead(int i2c_dev, UINT32 pri_adr);
    int setupPriWrite(int i2c_dev, UINT32 pri_adr,UINT32 pri_wdat);
    void setFuse(int adr_sel);
    void set_opt_i2cs_wide_en();
    int i2cPreArp();
    int i2cRstArp();
    int i2cGetUid(UINT32 *get_id_3, UINT32 *get_id_2, UINT32 *get_id_1,UINT32 *get_id_0);
    int i2cSetUid(UINT32 set_id_3, UINT32 set_id_2, UINT32 set_id_1,UINT32 set_id_0, UINT32 arp_adr);
    int testArp();
    int testMsgbox();
    int testMsgbox_command();
    int testMsgbox_datain();
    int testMsgbox_dataout();
    int testMsgbox_mutex();
    int testGpio_9();
    UINT32 temp2code(UINT32 temp);
    int testClkdiv();
    int testOffset();
    int test_smb_alt_addr();
    int WriteDacBgapCntl(int index, UINT32 newFieldValue);

    UINT32 m_arch;

private:
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(i2cslave_fermi, i2cslave_fermi, "GPU Thermal I2cSlave Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &i2cslave_fermi_testentry
#endif

#endif  // _i2cslave_fermi_H_
