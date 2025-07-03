/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XHCIDEV_H
#define INCLUDED_XHCIDEV_H

#ifndef INCLUDED_XHCICOMN_H
    #include "xhcicomn.h"
#endif

#ifndef INCLUDED_XHCITRB_H
    #include "xhcitrb.h"
#endif

#ifndef INCLUDED_XHCIREG_H
    #include "xhcireg.h"
#endif

#ifndef INCLUDED_XHCIRING_H
    #include "xhciring.h"
#endif

#ifndef INCLUDED_XHCICNTX_H
    #include "xhcicntx.h"
#endif

#ifndef INCLUDED_USBTYPE_H
    #include "usbtype.h"
#endif

#ifndef INCLUDED_USBDEXT_H
    #include "usbdext.h"
#endif

#ifndef INCLUDED_STL_VECTOR
    #include <vector>
    #define INCLUDED_STL_VECTOR
#endif

#define XHCI_ENUM_MODE_INSERT_SENSE             (1<<0)

#define XUSB_MEMPOOL_SIZE                       65536

// in PCI cfg space
#define MCP_USB3_CFG_ARU_C11_CSBRANGE           0x41C
#define CFG_ARU_FW_SCRATCH                      0x440
#define LW_PROJ__XUSB_CFG_ARU_RST                       0x0000042C
#define LW_PROJ__XUSB_CFG_ARU_RST_CFGRST                0:0
#define LW_PROJ__XUSB_CFG_ARU_RST_CFGRST_NOT_PENDING    0x00000000
#define LW_PROJ__XUSB_CFG_ARU_RST_CFGRST_PENDING        0x00000001
#define MCP_USB3_CFG_CSB_ADDR_0                 0x800
#define USB3_PCI_CAP_PTR_OFFSET                 0x34

// following are defined in USB3_Frontend_IAS.doc,
// the base address for them is m_OffsetPciMailbox
#define USB3_PCI_OFFSET_MAILBOX_CMD             0x4
#define USB3_PCI_OFFSET_DATA_IN                 0x8
#define USB3_PCI_OFFSET_DATA_OUT                0xc
#define USB3_PCI_OFFSET_OWNER                   0x10
#define USB3_MAILBOX_OWNER_NONE                 0
#define USB3_MAILBOX_OWNER_ID_FW                1
#define USB3_MAILBOX_OWNER_ID                   2
// on BAR2, see also https://lwtegra/home/tegra_manuals/html/manuals/t234/dev_t_xusb_bar2.html
#define XHCI_BAR2_MMIO_ADDRESS_RANGE            0x10000

#define XUSB_BAR2_ARU_SMI_0                     0x14
#define XUSB_BAR2_ARU_IFRDMA_CFG0               0xE0
#define XUSB_BAR2_ARU_IFRDMA_CFG1               0xE4
#define XUSB_BAR2_ARU_C11_CSBRANGE_0            0x9c
#define XUSB_BAR2_ARU_FW_SCRATCH_0              0x1000
#define XUSB_BAR2_CSB_ADDR_0                    0x2000

#define FW_IOCTL_LOG_BUFFER_LEN                 2
#define FW_IOCTL_LOG_DEQUEUE_LOW                4
#define FW_IOCTL_LOG_DEQUEUE_HIGH               5
#define FW_IOCTL_TYPE_SHIFT                     24

// on CSB
#define LW_USB3_CSB_FALCON_CGCTL                0x000000a0
// see also falcon_arch.doc or lw_ref_dev_falcon_pri.h for definitions
#define LW_PFALCON_FALCON_IMFILLCTL             0x00000158
#define LW_PFALCON_FALCON_IMFILLCTL_NBLOCKS     7:0

#define LW_PFALCON_FALCON_IMFILLRNG1            0x00000154
#define LW_PFALCON_FALCON_IMFILLRNG1_TAG_LO     15:0
#define LW_PFALCON_FALCON_IMFILLRNG1_TAG_HI     31:16

#define LW_USB3_CSB_FALCON_IMEMC0               0x00000180
#define LW_USB3_CSB_FALCON_IMEMC_OFFSET         7:2
#define LW_USB3_CSB_FALCON_IMEMC_BLK            15:8
#define LW_USB3_CSB_FALCON_IMEMC_INCW           24:24
#define LW_USB3_CSB_FALCON_IMEMC_INCR           25:25
#define LW_USB3_CSB_FALCON_IMEMD0               0x00000184
#define LW_USB3_CSB_FALCON_IMEMT0               0x00000188

#define LW_USB3_CSB_FALCON_DMEMC0               0x000001C0
#define LW_USB3_CSB_FALCON_DMEMD0               0x000001C4

#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR          0x00101A00
#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR_RSVD     7:0
#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR_SIZE     19:8
#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR_RO       29:29
#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR_NS       30:30
#define LW_USB3_CSB_MEMPOOL_ILOAD_ATTR_TC       31:31

#define LW_PROJ_USB3_CSB_MEMPOOL_ILOAD_BASE_LO  0x00101A04
#define LW_PROJ_USB3_CSB_MEMPOOL_ILOAD_BASE_HI  0x00101A08

#define LW_PROJ_USB3_CSB_MEMPOOL_APMAP          0x010181C
#define LW_PROJ_USB3_CSB_MEMPOOL_APMAP_BOOTPATH 31:31

#define XUSB_CSB_ARU_SCRATCH0                   0x00100100

#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG  0x0101A14
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION   31:24
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION_L2IMEM_LOAD_LOCKED_RESULT 0x0000011
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_TRIG_ACTION_L2IMEM_ILWALIDATE_ALL     0x0000040

#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE              0x0101A10
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_RSVD         7:0
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_RSVD_DEFAULT 0x0000000
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_OFFSET   19:8
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_OFFSET_DEFAULT 0x0000000
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_COUNT    31:24
#define LW_PROJ_USB3_CSB_MEMPOOL_L2IMEMOP_SIZE_SRC_COUNT_DEFAULT 0x0000000

#define LW_USB3_FALCON_DMACTL                   0x0000010c
#define LW_USB3_FALCON_CPUCTL                   0x00000100
#define LW_USB3_FALCON_BOOTVEC                  0x00000104

#define LW_PROJ__XUSB_CSB_MEMPOOL_RAM                      0x0101800    /* R-I4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_RAM_SIZE                 3:0          /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_RAM_SIZE_DEFAULT         0x0000002    /* R-I-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT                  0x0101804    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_BASE             13:0         /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_BASE_DEFAULT     0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_SIZE             29:16        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DDIRECT_SIZE_DEFAULT     0x0000000    /* RWI-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC           0x0101A50    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_ADDR      17:2         /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_ADDR_INIT 0            /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCW     30:30        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCW_INIT 0            /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCR     31:31        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMC_AINCR_INIT 0            /* RWI-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_REGACCESS_MEMD           0x0101A54    /* RWI4R */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS          0x0101A44    /* R-I4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_FREE_COUNT 2:0          /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_FREE_COUNT_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_PENDING  16:16        /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_STATUS_PENDING_DEFAULT 0x0000000    /* R-I-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR            0x0101A30    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_SIZE       17:4         /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_SIZE_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_NS         29:29        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_NS_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_RO         30:30        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_RO_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_TC         31:31        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_ATTR_TC_DEFAULT 0x0000000    /* RWI-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_LO         0x0101A34    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_LO_ADDR    31:4         /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_LO_ADDR_DEFAULT 0x0000000    /* RWI-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI         0x0101A38    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI_ADDR    7:0          /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_BASE_HI_ADDR_DEFAULT 0x0000000    /* RWI-V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG            0x0101A3C    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_LOCAL_OFFSET 17:4         /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_LOCAL_OFFSET_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_ACTION     31:24        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_ACTION_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_TRIG_ACTION_WAITFENCE_RESULT 0x0000014    /* RW--V */

#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT          0x0101A40    /* RWI4R */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_INST_DATA 0:0          /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_INST_DATA_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_INST_DATA_IDIRECT 0x0000001    /* R---V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_INST_DATA_DDIRECT 0x0000000    /* R---V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_FENCE    1:1          /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_FENCE_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_WRITE    2:2          /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_WRITE_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_RETURN_OFFSET 17:4         /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_RETURN_OFFSET_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_ERR      24:24        /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_ERR_DEFAULT 0x0000000    /* R-I-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_POP      30:30        /* RWIVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_POP_DEFAULT 0x0000000    /* RWI-V */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_VLD      31:31        /* R-IVF */
#define LW_PROJ__XUSB_CSB_MEMPOOL_DIRECTOP_RESULT_VLD_DEFAULT 0x0000000    /* R-I-V */

#define XHCI_DEFAULT_MSILINES                   1
#define XHCI_DEFAULT_MSIXLINES                  1
#define XHCI_MAX_SLOT_NUM                       255
#define XHCI_MAX_DCI_NUM                        32
#define XHCI_MAX_INTR_NUM                       256

#define XHCI_ALIGNMENT_STREAM_CONTEXT           16
#define XHCI_ALIGNMENT_SCRATCHED_BUF_ARRAY      64

#define XHCI_XCAP_ID_LEG                        1
#define XHCI_XCAP_ID_PROTOCOL                   2
#define XHCI_XCAP_ID_PM                         3
#define XHCI_XCAP_ID_IO_VIRT                    4
#define XHCI_XCAP_ID_MSI                        5
#define XHCI_XCAP_ID_DEBUG                      10
#define XHCI_XCAP_ID_EXT_MSI                    17

#define XHCI_RATIO_64BIT                        1
#define XHCI_RATIO_IDT                          2
#define XHCI_RATIO_EVENT_DATA                   3

#define XHCI_DEL_MODE_PRECISE                   0
#define XHCI_DEL_MODE_SLOT                      1
#define XHCI_DEL_MODE_SLOT_EXPECT_EP0           2

#define XHCI_AO_PHYS_BASE                       0x3540000

// Note that software must ensure that no interface data structure reachable by the xHC spans a Page
// (PAGESIZE) or 64KB boundary. This includes all Context data structures and TRB Ring segments. Data
// buffers referenced by TRBs may cross page or 64KB boundaries.

// Root Hub Port Number field is between 1 and MaxPorts, 0 doesn't exist

#define FW_LOG_ENTRY_SIZE                       32
#define FW_LOG_PAYLOAD_SIZE                     27
#define NUM_OF_LOG_ENTRIES                      4096
#define SW_OWNED_LOG_ENTRY                      0x01
#define XHCI_GET_EXTND_PROP_CONTEXT_SIZE_GET_TIME_STAMP 16
#define EXTND_CAP_ID_ILWALID                    0x0FFE // 11:1 are RsvdZ

typedef enum _LOWPOWER_MODE {
    LOWPOWER_MODE_U0_Idle = 0,
    LOWPOWER_MODE_U1 ,
    LOWPOWER_MODE_U2 ,
    LOWPOWER_MODE_U3 ,
    LOWPOWER_MODE_L1,
    LOWPOWER_MODE_L2
} _LOWPOWER_MODE_E;

class XhciController:public XhciComnDev
{
public:
    // see also sw/dev/mcp_drv/usb3/xhcifw/mailbox.h
    typedef enum _MBOX_CMD {
        SW_FW_MBOX_CMD_MSG_ENABLED = 1,
        SW_FW_MBOX_CMD_INC_FALC_CLOCK,
        SW_FW_MBOX_CMD_DEC_FALC_CLOCK,
        SW_FW_MBOX_CMD_INC_SSPI_CLOCK,
        SW_FW_MBOX_CMD_DEC_SSPI_CLOCK,
        SW_FW_MBOX_CMD_SET_BW,
        SW_FW_MBOX_CMD_SET_SS_PWR_GATING,
        SW_FW_MBOX_CMD_SET_SS_PWR_UNGATING,
        SW_FW_MBOX_CMD_SAVE_DFE_CNTLE,
        SW_FW_MBOX_CMD_AIRPLANE_MODE_ENABLED,
        SW_FW_MBOX_CMD_AIRPLANE_MODE_DISABLED,
        SW_FW_MBOX_CMD_ENABLE_PUPD,
        SW_FW_MBOX_CMD_DISABLE_PUPD,
        SW_FW_MBOX_CMD_DBC_WAKE_STACK,   // Added new Mailbox message from FW to SW
        SW_FW_MBOX_CMD_TOGGLE_HSIC_PWR_RAIL,
        //needs to be the last cmd
        SW_FW_MBOX_CMD_MAX,
        // msg type to ack above commands
        SW_FW_MBOX_CMD_ACK = 128,
        SW_FW_MBOX_CMD_NACK
    } MBOX_CMD_E;

    typedef struct _TimeStamp {
        UINT64 systime; // 31:0 31:0 64bit
        UINT16 busInterCount; // 29:16 14bit
        UINT16 delta; // 12:0 13bit
    } TimeStamp;

    typedef enum _ExtendedPropSubtype {
        EXTND_PROP_SUBTYPE_GETTIMESTAMP = 1
    } ExtendedPropSubtype;

    typedef enum _ExtendedPropEci {
        EXTND_PROP_ECI_TIMESTAMP = (1<<12)
    } ExtendedPropEci;

    typedef enum _L1_STATUS {
        L1S_ILWALID,
        L1S_SUCCESS,
        L1S_NOT_YET,
        L1S_NOT_SUPPORTED,
        L1S_TIMEOUT_ERROR,
        L1S_RESERVED
    } L1Status;

    //==========================================================================
    // Natives
    //==========================================================================
    XhciController();
    virtual ~ XhciController();
    const char* GetName();

    //! \brief for recent CheetAh chips (T194) the PCI CFG space is ahead of aperture space.
    //! Call this function to set the leading space size beore mapping MMIO registers
    RC SetPciReservedSize();
    RC SetU1U2Timeout(UINT32 Port, UINT32 U1Timeout, UINT32 U2Timeout = 0);
    RC SetL1DevSlot(UINT32 Port, UINT32 SlotId = 0);
    RC EnableL1Status(UINT32 Port, UINT32 TimeoutMs = 1000);
    RC Usb2ResumeDev(UINT32 Port);
    RC PortPLS(UINT32 Port, UINT32 *pData, UINT32 Pls = 0, bool IsClearPlc = true);
    RC WaitPLS(UINT32 Port, UINT32 ExpPls, UINT32 TimeoutMs = 100);
    RC PortPP(UINT32 Port, bool IsClearPP = true, UINT32 TimeoutMs = 100);

    //! \brief Get Page size defined in Cap registers
    //! \param [o] pPageSize : Page size in Byte
    RC GetPageSize(UINT32 * pPageSize);

    //! \brief Deal with Run/Stop bit in USBCMD
    //! \param [i] IsStart : Is to Start or Stop (true -> start)
    RC StartStop(bool IsStart);

    //! \brief Stop all running endpoint and save registers' states
    RC StateSave();

    //! \brief Restore register and start all endpoints that have been turned off
    RC StateRestore();

    //! \brief if to read register from local buffer when user call Xhci.RegRd()
    //! \param [i] IsEnable : Is to enable the read from local cache
    //! \param [i] IsSilent : Is to print promtp when enable/disable
    RC SetLocalReg(bool IsEnable,  bool IsSilent = false);

    //! \brief Set the appraoch of device enumeration
    //! \param [i] ModeMask : bit mask for mods used in dev enum
    RC SetEnumMode(UINT32 ModeMask);
    UINT32 GetEnumMode();

    //! \brief Reset Host Controller, no SW data structure touched
    RC ResetHc();

    //! \brief Reset a specified USB port
    //! \param [i] Port : port number
    //! \param [i] RouteSting : RouteString if the port is behind hub(s)
    //! \param [o] pSpeed : negociated speed of the port / device
    //! \param [i] IsForce : if to do a fource reset no matter if the port is in a good state
    RC ResetPort(UINT32 Port, UINT32 RouteSting = 0, UINT08 * pSpeed = NULL, bool IsForce = true);

    //! \brief download the firmware binary to Falcon micro controller
    //! \param [i] FileName : the filename of the firmware binary
    RC LoadFW(const string FileName, bool isEnMailbox = false);

    //! \brief Check if firmware send msg thru mailbox to SW, if yet, handle it
    //! \param [i] TimeoutMs : the timeout to wait for subsequent message.
    //!                        if no msg exists when function is called, this param takes no effect
    RC HandleFwMsg(UINT32 TimeoutMs = 1);

    //! \brief Save binary to a local file
    //! \param [i] InitTag : initial value of the Tag number ( get it by studying original FW binary)
    //! \param [i] ByteSize : the size of binary
    RC SaveFW(UINT32 InitTag = 0, UINT32 ByteSize = 1024);

    //! \brief Read a 32-bit Mailbox register
    //! \param [in] Offset : Register offset to read
    //! \param [out] pData : Data read back
    RC MailboxRegRd32(const UINT32 Offset, UINT32 *pData) const;

    //! \brief Write a 32-bit Mailbox register
    //! \param [in] Offset : Offset to write to
    //! \param [in] Data : Data to write
    RC MailboxRegWr32(const UINT32 Offset, UINT32 Data) const;

    //! \brief Read a 32-bit BAR2 register
    //! \param [in] Offset : Register offset to read
    //! \param [out] pData : Data read back
    RC Bar2Rd32(const UINT32 Offset, UINT32 *pData) const;

    //! \brief Write a 32-bit BAR2 register
    //! \param [in] Offset : Offset to write to
    //! \param [in] Data : Data to write
    RC Bar2Wr32(const UINT32 Offset, UINT32 Data) const;

    //! \brief helper to write to CSB space
    //! \param [i] Addr : offset
    //! \param [i] Data : data
    //! \sa P4:\\sw\dev\mcp_drv\usb3main\xhcifw\include\lwcsb_T234.h
    RC CsbWr(UINT32 Addr, UINT32 Data);
    //! \brief  helper to read from CSB space
    //! \param [i] Addr : offset
    //! \param [o] pData : pointer to data
    RC CsbRd(UINT32 Addr, UINT32 *pData);

    //! \brief Send message to Mailbox
    //! \param [o] Data : data to be send to falcon
    //! \param [o] IsId : if to write the Owner register with SW's ID
    RC MailboxTx(UINT32 Data, bool IsSetId = false);
    //! \brief Check FW's ack by reading mailbox
    RC MailboxAckCheck();
    //! \brief Find the offset of capability in PCI CFG space
    RC FindMailBoxCap();

    //! \brief Get the usable MEM pool size
    UINT32 CsbGetBufferSize();
    //! \brief Setup the DDirect partition
    RC CsbDmaPartition();
    //! \brief Fill MEM pool with pattern32
    RC CsbDmaFillPattern(vector<UINT32> * pPattern32);
    //! \brief Read back the pattern to check (optional)
    //! \param [o] pData32 : vector to hold the data
    RC CsbDmaReadback(vector<UINT32> * pData32);
    //! \brief Trigger DMA
    //! \param [i] pTargerAddr : Physical address of target system memory
    //! \param [i] IsWrite : to write or to read
    RC CsbDmaTrigger(PHYSADDR pTargerAddr, bool IsWrite);
    //! \brief Check if the DMA is finished or error happens
    //! \param [o] pIsComplete : is finished
    //! \param [o] pDmaCount : total count of DMA operation
    RC CsbDmaCheckStatus(bool *pIsComplete, UINT64 *pDmaCount = NULL, UINT32 *pFreeCount = NULL);

    //! \brief Save user created Xfer Ring to internal vector
    //! \param [i] pXferRing : address of the Xfer Ring
    RC SaveXferRing(TransferRing *pXferRing);

    //! \brief Allocate / De-alloc Scratch Buffer
    //! \param [i] IsAlloc : if to allocate or dealloc
    RC HandleScratchBuf(bool IsAlloc);

    //! \brief Clear PORTSC status bit(s), by default cear all status
    //! \param [i] Port : Port number
    //! \param [i] PortSc : which status bit(s) to clear
    RC ClearStatusChangeBits(UINT08 Port, UINT32 PortSc = 0);

    //! \brief Retrieve Tasker::EventId associated with specified Event Ring
    //! \param [i] RingIndex : Index of Event Ring
    //! \param [o] pEventId : pointer of Event ID
    RC GetEventId(UINT32 RingIndex, Tasker::EventID * pEventId);

    //! \brief Write the appropriate doorbell register
    //! \param [i] SlotId :
    //! \param [i] Endpoint :
    //! \param [i] IsOut :
    //! \param [i] StreamId :
    RC RingDoorBell(UINT32 SlotId = 0, UINT08 Endpoint = 0,
                    bool IsOut = false, UINT16 StreamId = 0);

    //! \brief Wait for all Command(s) in CMD Ring to be finished
    //! \param [o] pCmpCode : Completion Code of command completion event
    //! \param [o] pSlotId : Slot ID of command completion event
    //! \param [i] TimeoutMs : Timeout in Ms
    RC WaitCmdCompletion(UINT08* pCmpCode = NULL,
                         UINT08* pSlotId = NULL,
                         UINT32 TimeoutMs = XHCI_DEFAULT_TIMEOUT_MS);

    //! \brief Relocate Cmd Ring to another location
    RC CmdRingReloc();

    //! \brief Initialize Xfer Ring for specified endpoint
    // if there is already a Ring exists, remove it then create new one
    // CmdSetTRDeqPtr will be ilwolved to set the Physical address
    //! \param [i] SlotId : Slot ID
    //! \param [i] EpNum : Endpoint Number
    //! \param [i] IsDataOut : Data Direction
    //! \param [i] StreamId : Stream ID
    RC InitXferRing(UINT08 SlotId, UINT08 EpNum, bool IsDataOut, UINT16 StreamId = 0);

    //! \brief Create Stream for specified Endpoint
    //! \param [i] SlotId : Slot ID
    //! \param [i] Endpoint : Endpoint Number
    //! \param [i] IsDataOut : Data Direction
    //! \param [o] pPhyBase : Physical address of generated stream context
    //! \param [i] MaxStream : MaxStream parameter
    RC InitStream(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, PHYSADDR *pPhyBase, UINT32 MaxStream = 0);

    //! \brief Toggle Event Data flag
    // SA SetupXfer()
    //! \param [i] IsEnable : is turn on Event Data insertion
    RC SetEventData(bool IsEnable);

    //! \brief Send Configre Endpoint command to HC
    // based on the Endpoint infomation passed in pvEpInfo, it will
    // setup corresponding Input Context
    //! \param [i] SlotId : Slot ID
    //! \param [i] pvEpInfo : vector of EndpointInfo
    //! \param [i] IsHub : isHub
    //! \param [i] Ttt : Ttt is IsHub = true
    //! \param [i] Mtt : Mtt if IsHub = true
    //! \param [i] NumPorts : NumPorts if IsHub = true
    RC CfgEndpoint(UINT08 SlotId, vector<EndpointInfo>* pvEpInfo,
                   bool IsHub = false, UINT32 Ttt = 0, bool Mtt = false, UINT32 NumPorts = 0);

    //! \brief Update Device Context with given Endpoint 0 Mps
    //! \param [i] SlotId : Slot ID
    //! \param [i] Mps : Mps get from Device Descriptor
    RC SetEp0Mps(UINT08 SlotId, UINT32 Mps);

    //! \brief Update Device Context with given misc bits
    //! \param [i] SlotId : Slot ID
    //! \param [i] IntrTarget : Interrupt Target
    //! \param [i] MaxLatMs : Max Latency Ms
    RC SetDevContextMiscBit(UINT08 SlotId, UINT16 IntrTarget, UINT16 MaxLatMs);

    //! \brief Set the default CERR in all Ep Context
    //! \param [i] Cerr : Cerr
    RC SetCErr(UINT08 Cerr);

    //! \brief Retrieve Mps for specific endpoint
    //! \param [i] SlotId : Slot ID
    //! \param [i] Endpoint : Endpoint Number
    //! \param [i] IsDataOut : Data Direction
    //! \param [o] pMps : Mps data
    RC GetEpMps(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT32 * pMps);
    //! \brief Retrieve EpState for specific endpoint
    //! \param [i] SlotId : Slot ID
    //! \param [i] Endpoint : Endpoint Number
    //! \param [i] IsDataOut : Data Direction
    //! \param [o] pEpState : Endpoint State data
    RC GetEpState(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT08 * pEpState);

    //! \brief Issue a No Op Xfer on specified Ring
    //! \param [i] RingId : Transfer Ring Index
    //! \param [o] ppDeqPointer : pointer to new Dequeue Pointer
    RC XferNoOp(UINT32 RingId, XhciTrb ** ppDeqPointer = NULL);

    //! \brief Get optimised transfer length supported by one TRB
    // called by class driver to help determine fraglist orgnize
    UINT32 GetOptimisedFragLength();

    //! \brief Update Deq Pointer of Cmd Ring
    //! \param [i] pTrb : pointer to complished Command TRB
    //! \param [i] IsAdvancePtr : if to advance the pTrb
    RC UpdateCmdRingDeq(XhciTrb* pTrb, bool IsAdvancePtr = true);

    //! \brief Update Deq Pointer of EventRing Ring
    //! \param [i] EventIndex : Event Index
    //! \param [i] pDeqPtr : pointer to new Deq Pointer
    //! \param [i] SegmengIndex : affected Event Ring segment index
    RC UpdateEventRingDeq(UINT32 EventIndex, XhciTrb * pDeqPtr, UINT32 SegmengIndex);

    //! \brief Handle the number of Event Ring semgments update
    //--------------------------------------------------------------------------
    virtual RC EventRingSegmengSizeHandler(UINT32 EventRingIndex, UINT32 NumSegments);

    //! \brief Handle the newly arrived Event TRB, update the Command Ring / Xfer Ring DeqPtr
    //--------------------------------------------------------------------------
    virtual RC NewEventTrbHandler(XhciTrb* pTrb);

    //! \brief Get protocol sopprted by given root hub port
    //--------------------------------------------------------------------------
    RC GetPortProtocol(UINT32 Port, UINT16 *pProtocol);
    //! \brief Read in protocol information from extended cap
    //--------------------------------------------------------------------------
    RC ReadPortProtocol();

    //! \brief Transactions on control endpoints
    //--------------------------------------------------------------------------
    RC Setup2Stage(UINT08 SlotId, UINT08 ReqType, UINT08 Req, UINT16 Value,
                   UINT16 Index, UINT16 Length);
    RC Setup3Stage(UINT08 SlotId, UINT08 ReqType, UINT08 Req, UINT16 Value,
                   UINT16 Index, UINT16 Length, void * pData);

    //! \brief reserve enough memory buffer for the TDs
    // this is a better to have for EHCI/OHCI
    // a must have for XHCI since XHCI use Ring instead of linked list
    //--------------------------------------------------------------------------
    RC ReserveTd(UINT08 SlotId,
                 UINT08 Endpoint,
                 bool IsDataOut,
                 UINT32 NumTD);

    //! \brief Transactions on specified xfer endpoints
    // this is tha main function called by class drivers
    // each fragment of pFrags will be translted to a TRB on the Xfer Ring
    //--------------------------------------------------------------------------
    RC SetupTd(UINT08 SlotId,
               UINT08 Endpoint,
               bool IsDataOut,
               MemoryFragment::FRAGLIST* pFrags,
               bool IsIsoch = false,    // if this is a Isoch TD
               bool IsDbRing = true,    // to ring the Doorbell or not
               UINT16 StreamId = 0,     // StreamID
               void * pDataBuffer = NULL);  // caller indicates me to release it

    //! \brief Return fragment list of data buffers build by SetupTd()
    // data buffers are built by calling SetupTd() with pDataBuffer provided
    //--------------------------------------------------------------------------
    RC DataRetrieve(UINT08 SlotId,
                    UINT08 Endpoint,
                    bool IsDataOut,
                    MemoryFragment::FRAGLIST * pFragList,
                    UINT32 StreamId = 0,
                    UINT32 NumTd = 0);

    //! \brief Release all the data buffers associated with specified Transfer Ring
    //--------------------------------------------------------------------------
    RC DataRelease(UINT08 SlotId,
                   UINT08 Endpoint,
                   bool IsDataOut,
                   UINT32 StreamId = 0,
                   UINT32 NumTd = 0);

    //! \brief Wait for Xfer TRB to be finished
    //--------------------------------------------------------------------------
    RC WaitXfer(UINT08 SlotId,
                UINT08 Endpoint,
                bool IsDataOut,
                UINT32 TimeoutMs = XHCI_DEFAULT_TIMEOUT_MS,
                UINT32 * pCompCode = NULL,     // return the Complation Code to caller
                bool isWaitAll = true,         // wait for last TRBs in Xfer Ring to be finished
                UINT32 * pTransferLength = NULL,
                bool IsSilent = false);

    //! \brief Validate the completion code
    //! \param [i] CompletionCode : completion code from event TRB
    //! \param [o] pIsStall : if STALL oclwrs
    RC ValidateCompCode(UINT32 CompletionCode, bool *pIsStall = NULL);

    //! \brief handle the STALL condition
    //! \param [i] SlotId : SlotID
    //! \param [i] Endpoint : endpoint
    //! \param [i] IsDataOut : Direction of endpoint
    RC ResetEndpoint(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut);

    //--------------------------------------------------------------------------
    // Misc / debug
    //--------------------------------------------------------------------------

    //! \brief enable Test Mode for specific port
    //! \param [i] PortIndex : the index of port being put into test mode
    //! \param [i] TestMode : the target test mode
    //! \param [i] RouteSting : rounter string for hub (default to 0x0)
    RC EnablePortTestMode(UINT32 PortIndex, UINT08 TestMode, UINT32 RouteSting = 0x0);
    //! \brief disable Test Mode (only one port could be in test mode at any time)
    RC DisablePortTestMode();
    //! \brief enable Test Mode on specified HUB downstream port
    //! \param [i] PortIndex : the index of root hub port to which HUB is attached
    //! \param [i] TestMode : the target test mode
    //! \param [i] RouteSting : the target route string, see also UsbCfg()
    RC EnableHubPortTestMode(UINT32 PortIndex, UINT08 TestMode, UINT32 RouteSting);
    //! \brief disable Test Mode by reset the HUB
    RC DisableHubPortTestMode(UINT32 PortIndex, UINT32 RouteString);

    static RC SetFullCopy(bool IsEnable);
    RC SetRnd(UINT32 Index, UINT32 Ratio);
    RC InitDummyBuf(UINT32 BufferSize);
    RC GetDummyBuf(void ** ppBuf, UINT32 BufferSize, UINT32 Alignment = 64);
    RC FindTrb(PHYSADDR pPhyTrb);
    RC TrTd(UINT32 SlotId, UINT32 Ep, bool IsDataOut, UINT32 Num);

    //==========================================================================
    //FW LOG Debug
    //==========================================================================
    void * m_pLogBuffer;
    void * m_pFwLogDeqPtr;

    //! \brief Log firmware logs into a user specified file
    //! \param fileName a " .txt" fileName to save the log entries
    RC SaveFwLog(string FileName = "fw_log.txt");
    RC PrintFwStatus();

    //==========================================================================
    // info
    //==========================================================================
    RC PrintReg( Tee::Priority PriRaw = Tee::PriNormal ) const;
    RC PrintRegGlobal(Tee::Priority PriRaw = Tee::PriNormal,
                      Tee::Priority PriInfo = Tee::PriLow) const;
    RC PrintRegPort(UINT32 Port, Tee::Priority PriRaw = Tee::PriNormal,
                    Tee::Priority PriInfo = Tee::PriLow);
    RC PrintRegRuntime(Tee::Priority PriRaw = Tee::PriNormal,
                       Tee::Priority PriInfo = Tee::PriLow) const;
    //! \brief Print misc data structures
    //--------------------------------------------------------------------------
    RC PrintData(UINT08 Type, UINT08 Index = 0);
    //! \brief Print list of all existing Rings
    //--------------------------------------------------------------------------
    RC PrintRingList(Tee::Priority Pri = Tee::PriNormal) const;
    //! \brief Print content of specified ring
    //--------------------------------------------------------------------------
    //! \brief Print content of specified ring
    //--------------------------------------------------------------------------
    RC PrintRing(UINT32 Uuid, Tee::Priority Pri = Tee::PriNormal);
    //! \brief Clear content of specified ring for debug purpose
    //--------------------------------------------------------------------------
    RC ClearRing(UINT32 Uuid);

    //! \brief Dump all the registers & data structures in a read only mode
    // could work with Linux driver
    //--------------------------------------------------------------------------
    RC DumpAll();
    RC DumpRing(PHYSADDR pHead, UINT32 NumTrb = 0, bool IsEventRing = false);

    //==========================================================================
    // wrappers of data structures
    //==========================================================================
    //! \brief Get/Set default Interrupt Target of specified Ring
    //--------------------------------------------------------------------------
    RC GetIntrTarget(UINT08 SlotId, UINT08 EpNum, bool IsDataOut,
                     UINT32* pIntrTarget);
    RC GetIntrTarget(UINT32 RingUuid, UINT32* pIntrTarget);
    RC SetIntrTarget(UINT08 SlotId, UINT08 EpNum, bool IsDataOut,
                     UINT32 IntrTarget);
    RC SetIntrTarget(UINT32 RingUuid, UINT32 IntrTarget);

    //! \brief Remove not used TDs for specified Xfer Ring
    //--------------------------------------------------------------------------
    RC CleanupTd(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, bool IsIssueStop = true);

    //! \brief Flush the cached TRBs in Event Ring and local event queue
    //--------------------------------------------------------------------------
    RC FlushEvent(UINT32 EventRingIndex, UINT32 * pNumEvent = NULL);
    //! \brief Check for Transfer Event comes from specified Endpoint
    // if there's not any, returns immidiately
    //--------------------------------------------------------------------------
    bool CheckXferEvent(UINT32 EventRingIndex, UINT08 SlotId, UINT08 Endpoint,
                        bool IsDataOut, XhciTrb * pTrb);
    //! \brief Wait for exact CMD/Xfer TRB to be finished
    //--------------------------------------------------------------------------
    RC WaitForEvent(UINT32 EventRingIndex,
                    XhciTrb * pTrb,
                    UINT32 WaitMs = XHCI_DEFAULT_TIMEOUT_MS,
                    XhciTrb *pEventTrb = NULL,
                    bool IsFlushEvents = true);
    //! \brief Wait for TRB of specified type to be finished
    //--------------------------------------------------------------------------
    RC WaitForEvent(UINT32 EventRingIndex,
                    UINT08 EventType,
                    UINT32 WaitMs = XHCI_DEFAULT_TIMEOUT_MS,
                    XhciTrb *pEventTrb = NULL,
                    bool IsFlushEvents = true,
                    UINT08 SlotId = 0,
                    UINT08 Dci = 0);

    //! \brief Set default size of each type of Ring
    //--------------------------------------------------------------------------
    RC SetCmdRingSize(UINT32 Size);
    RC SetEventRingSize(UINT32 Size);
    RC SetXferRingSize(UINT32 Size);

    //! \brief Back door to modify TRBs manually
    //--------------------------------------------------------------------------
    RC GetTrb(UINT32 Uuid, UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb);
    RC SetTrb(UINT32 Uuid, UINT32 SegIndex, UINT32 TrbIndex, XhciTrb * pTrb);

    //! \brief Methods to locate a specified Ring
    //--------------------------------------------------------------------------
    RC FindRing(UINT32 Uuid, XhciTrbRing ** ppRing, UINT32 * pMaxUuid = NULL) const;
    RC FindCmdRing(UINT32 *pUuid);
    RC FindEventRing(UINT32 EventIndex, UINT32 *pUuid);
    RC FindXferRing(UINT08 SlotId, UINT08 EpNum, bool IsDataOut,
                    UINT32 * pUuid,
                    UINT16 StreamId = 0);

    //! \brief Append / delete segment to / form Ring
    //--------------------------------------------------------------------------
    RC RingAppend(UINT32 RingUuid, UINT32 SegSize);
    RC RingRemove(UINT32 RingUuid, UINT32 SegIndex);

    //! \brief Get type of Ring (CMD/Xfer/Event)
    //--------------------------------------------------------------------------
    RC GetRingType(UINT32 RingUuid, UINT32 * pType);

    //! \brief Get array of segment sizes of specified Ring
    //--------------------------------------------------------------------------
    RC GetRingSize(UINT32 RingUuid, vector<UINT32> *pvSizes = NULL);

    //! \brief Get / Set current Enq(CMD/Xfer)/Deq(Event) location of Ring
    //--------------------------------------------------------------------------
    RC GetRingPtr(UINT32 RingUuid, UINT32 * pSeg, UINT32 * pIndex,
                  PHYSADDR * pEnqPtr = NULL, UINT32 BackLevel = 0);
    RC SetRingPtr(UINT32 Uuid, UINT32 SegIndex, UINT32 TrbIndex, INT32 CycleBit = 0);

    //! \brief Back door to modify Device Context
    //--------------------------------------------------------------------------
    RC GetDevContext(UINT08 SlotId, vector<UINT32>* pvData = NULL);
    RC SetDevContext(UINT08 SlotId, vector<UINT32>* pvData);

    //! \brief Back door to specify user defined Input Context
    //--------------------------------------------------------------------------
    RC SetUserContext(vector<UINT32>* pvData, UINT32 Alignment = 0);

    //! \brief to specify user defined Stream Layout
    // i.e. Primary Stream Array has pvData->size() entries
    // each element of pvData specify the size of corresponding Secondary Stream, 0 means Xfer Ring
    //--------------------------------------------------------------------------
    RC SetStreamLayout(vector<UINT32>* pvData);

    //! \brief Create Secondary Stream Array in specified entry of Primary Array with specified size
    //--------------------------------------------------------------------------
    RC SetupStreamSecArray(UINT08 SlotId, UINT08 Endpoint, bool IsDataOut, UINT32 PriIndex, UINT32 SecSize);

    //! \brief Data structure setup helper
    //--------------------------------------------------------------------------
    RC SetupDeviceContext(UINT32 SlotId, UINT32 NumEntries = 32);

    //! \brief Search Device Context Array for device belongs to given Port and return its slot id if found
    //--------------------------------------------------------------------------
    RC GetSlotIdByPort(UINT32 Port, UINT32 * pSlotId, UINT32 RouteSting = 0x0);

    //! \brief Find Xfer Ring object for specified Slot/Ep/Direction
    //--------------------------------------------------------------------------
    RC FindXferRing(UINT08 SlotId, UINT08 EndPoint, bool IsDataOut,
                    TransferRing ** ppRing,
                    UINT16 StreamId = 0);

    //! \brief Find StreamContextArrayWrapper object for specified Slot/Ep/Direction
    //--------------------------------------------------------------------------
    RC FindStream(UINT08 SlotId, UINT08 EndPoint, bool IsDataOut,
                  StreamContextArrayWrapper ** ppStreamCntxt);

    //==========================================================================
    // ext USB device related
    //==========================================================================
    RC UsbCfg(UINT32 Port,
              bool IsLoadClassDriver = true,
              UINT32 RouteString = 0,
              UINT32 UsbSpeed = 0);       // if UsbSpeed is specified, Skip PortReset()
    RC DeCfg(UINT08 SlotId);

    //! \brief Retrieve the newly assigned Slot ID
    //--------------------------------------------------------------------------
    RC GetNewSlot(UINT32 * pSlotId);

    //! \brief Get the array of existing Slot IDs
    //--------------------------------------------------------------------------
    RC GetDevSlots(vector<UINT08> *pvSizes = NULL);

    //! \brief Standard USB requests
    //--------------------------------------------------------------------------
    RC ReqGetDescriptor(UINT08 SlotId, UsbDescriptorType DescType, UINT08 Index, void * pDataIn, UINT16 Len);
    RC ReqClearFeature(UINT08 SlotId, UINT16 Feature, UsbRequestRecipient Recipient, UINT16 Index);
    RC ReqGetConfiguration(UINT08 SlotId, UINT08 *pData08);
    RC ReqGetInterface(UINT08 SlotId, UINT16 Interface, UINT08 *pData08);
    RC ReqGetStatus(UINT08 SlotId, UsbRequestRecipient Recipient, UINT16 Index, UINT16 *pData16);
    RC ReqSetConfiguration(UINT08 SlotId, UINT16 CfgValue);
    RC ReqSetFeature(UINT08 SlotId, UINT16 Feature, UsbRequestRecipient Recipient, UINT08 Index, UINT08 Suspend = 0);
    RC ReqSetInterface(UINT08 SlotId, UINT16 Alternate, UINT16 Interface);
    RC ReqSetSEL(UINT08 SlotId, UINT08 U1Sel, UINT08 U1Pel, UINT16 U2Sel, UINT16 U2Pel);
    // class-specific request
    RC ReqMassStorageReset(UINT08 SlotId, UINT16 Interface = 0);

    //! \brief Standard USB HUB requests
    //--------------------------------------------------------------------------
    RC HubReqClearHubFeature(UINT08 SlotId, UINT16 Feature);
    RC HubReqClearPortFeature(UINT08 SlotId, UINT16 Feature, UINT08 Port, UINT08 IndicatorSelector=0);
    RC HubReqGetHubDescriptor(UINT08 SlotId, void * pDataIn, UINT16 Len, bool IsSuperSpeed = false);
    RC HubReqGetHubStatus(UINT08 SlotId, UINT32 * pData);
    RC HubReqGetPortStatus(UINT08 SlotId, UINT16 Port, UINT32 * pData);
    RC HubReqPortErrorCount(UINT08 SlotId, UINT16 Port, UINT16 * pData);
    //RC HubReqSetHubDescriptor();
    RC HubReqSetHubFeature(UINT08 SlotId, UINT16 Feature, UINT16 Index=0, UINT16 Length=0);
    RC HubReqSetHubDepth(UINT08 SlotId, UINT16 HubDepth);
    RC HubReqSetPortFeature(UINT08 SlotId, UINT16 Feature, UINT08 Port, UINT08 Index=0);

    //! \brief Commands defined by Xhci
    //--------------------------------------------------------------------------
    RC CommandStopAbort(bool IsStop, bool IsWaitForEvent = true);
    RC CmdSetTRDeqPtr(UINT08 SlotId, UINT08 Dci, PHYSADDR pPa, bool Dcs, UINT16 StreamId = 0, UINT08 Sct = 0);
    RC CmdResetDevice(UINT08 SlotId);
    RC CmdCfgEndpoint(UINT08 SlotId, InputContext * pInpCntxt, bool IsDeCfg = false);
    RC CmdEnableSlot(UINT08 * pSlotId);
    RC CmdDisableSlot(UINT08 SlotId);
    RC CmdAddressDev(UINT08 Port, UINT32 RouteString, UINT08 Speed, UINT08 SlotId, UINT08 * pUsbAddr, bool IsDummy = false);
    RC CmdNoOp(XhciTrb ** ppDeqPointer = NULL);
    RC CmdEvalContext(UINT08 SlotId);
    RC CmdResetEndpoint(UINT08 SlotId, UINT08 EpDci, bool IsTsp);
    RC CmdStopEndpoint(UINT08 SlotId, UINT08 EpDci, bool IsSp);
    RC CmdGetPortBw(UINT08 DevSpeed, UINT08 HubSlotId, vector<UINT08>* pvData = NULL);
    RC CmdForceHeader(UINT32 Lo, UINT32 Mid, UINT32 Hi, UINT08 PacketType, UINT08 PortNum);
    RC CmdGetExtendedProperty(UINT08 SlotId,
                              UINT08 EpId,
                              UINT16 Eci,
                              UINT08 SubType = 0,
                              UINT08* pDataBuffer = nullptr);

    // vendor defined commands
    RC SendIsoCmd(UINT08 SlotId, UINT32 Cmd, vector<UINT32> * pCmdData);

    //==========================================================================
    //  class driver
    //==========================================================================
    //! \brief Get type of external USB device
    //--------------------------------------------------------------------------
    RC GetDevClass(UINT08 SlotId, UINT32 *pType);
    RC SetConfiguration(UINT32 SlotId, UINT32 CfgIndex);

    // for HUB
    RC FindHub(UINT08 Port, UINT32 RouteString, UINT08 * pSlotId);
    RC ParseRouteSting(UINT32 RouteSting, UINT32 * pParentRouteString, UINT08 * pHubPort);

    // for BULK
    RC GetSectorSize(UINT08 SlotId, UINT32 * pSectorSize);
    RC GetMaxLba(UINT08 SlotId, UINT32 * pMaxLba);
    RC CmdRead10(UINT08 SlotId,
                 UINT32 Lba,
                 UINT32 NumSectors,
                 MemoryFragment::FRAGLIST * pFragList,
                 vector<UINT08> *pvCbw = NULL);
    RC CmdRead16(UINT08 SlotId,
                 UINT32 Lba,
                 UINT32 NumSectors,
                 MemoryFragment::FRAGLIST * pFragList,
                 vector<UINT08> *pvCbw = NULL);

    RC CmdWrite10(UINT08 SlotId,
                  UINT32 Lba,
                  UINT32 NumSectors,
                  MemoryFragment::FRAGLIST * pFragList,
                  vector<UINT08> *pvCbw = NULL);
    RC CmdWrite16(UINT08 SlotId,
                  UINT32 Lba,
                  UINT32 NumSectors,
                  MemoryFragment::FRAGLIST * pFragList,
                  vector<UINT08> *pvCbw = NULL);

    RC BulkSendCbwWrite10(UINT32 SlotId, UINT32 Lba, UINT32 NumSectors, EP_INFO * pEpInfo);
    RC BulkSendCbwRead10(UINT32 SlotId, UINT32 Lba, UINT32 NumSectors, EP_INFO * pEpInfo);
    RC BulkGetInEpInfo(UINT08 SlotID,  EP_INFO * pEpInfo);
    RC BulkSendCsw(UINT32 SlotId, EP_INFO * pEpInfo);

    RC CmdSense(UINT08 SlotId, void * pBuf = NULL, UINT08 Len = 0);
    RC CmdModeSense(UINT08 SlotId, void * pBuf = NULL, UINT08 Len = 0);
    RC CmdTestUnitReady(UINT08 SlotId);
    RC CmdInquiry(UINT08 SlotId, void * pBuf = NULL, UINT32 Len = 36);
    RC CmdReadCapacity(UINT08 SlotId, UINT32 * pMaxLBA, UINT32 * pSectorSize);
    RC CmdReadFormatCapacity(UINT08 SlotId, UINT32 * pNumBlocks, UINT32 * pBlockSize);

    // for performance tests
    //RC PerfRead(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors, MemoryFragment::FRAGLIST * pFragList);
    //RC PerfWrite(UINT08 SlotId, UINT32 Lba, UINT32 NumSectors, MemoryFragment::FRAGLIST * pFragList);

    // for Interrupt
    RC ReadMouse(UINT08 SlotId, UINT32 RunSeconds);

    // Iso
    RC ReadWebCam(UINT08 SlotId, UINT32 MaxSecs, UINT32 MaxRunTds, UINT32 MaxAddTds);
    RC SaveFrame(UINT08 SlotId, const string fileName);

    // For Hub
    RC ReadHub(UINT08 SlotId, UINT32 MaxSeconds);
    RC HubSearch(UINT08 SlotId);
    RC HubReset(UINT08 SlotId, UINT08 Port);

    // For Configurable Device
    RC DevRegRd(UINT08 SlotId, UINT32 Offset, UINT64 * pData);
    RC DevRegWr(UINT08 UsbAddr, UINT32 Offset, UINT64 Data);
    RC DataIn(UINT08 UsbAddr,
              UINT08 Endpoint,
              MemoryFragment::FRAGLIST * pFragList,
              bool IsIsoch = false);
    RC DataOut(UINT08 SlotId,
               UINT08 Endpoint,
               MemoryFragment::FRAGLIST * pFragList,
               bool IsIsoch = false);

    RC Search(UINT32 PortNo = 0, vector<UINT32> * pvPorts = NULL);

    long Isr();
    //long IsrMsiCommon(UINT32 EventRingIndex);

    //==========================================================================
    //  framework
    //==========================================================================
    RC Reset();
    RC PrintState(Tee::Priority = Tee::PriNormal) const;
    RC PrintExtDevInfo(Tee::Priority = Tee::PriNormal) const;
    RC InitExtDev() { return OK; }
    RC UninitExtDev() { return OK; }

    virtual RC RegRd(const UINT32 offset, UINT32 * pData, const bool IsDebug=false) const;
    virtual RC RegWr(const UINT32 offset, const UINT32 data, const bool IsDebug=false) const;

    //! \brief Set mapping between SSPorts and HSPorts.
    RC SetSSPortMap(UINT32 PortMap);

    //! \brief Set the physical address of BAR2, should be called before .Init()
    //! \param [i] PhysBar2 : BAR2 physical address
    RC SetBar2(UINT32 PhysBar2);

    //! \brief Get the time stamp from xHC
    //! \param pData: buffer receive time stamp result
    RC GetTimeStamp(TimeStamp* pData = NULL);
    //! \brief Enable BTO (Bus Time Out) test.
    RC EnableBto(bool IsEnable = true);

    //! \brief Get the current bus error number
    UINT32 GetBusErrCount(void);

    //! \brief disable port
    RC DisablePorts(vector<UINT32> Ports);

    //! \brief set lowpower mode
    RC SetLowPowerMode(UINT32 Port, UINT08 LowPwrMode);
protected:
    //! \brief Check if device is connected on port
    //-------------------------------------------
    RC IsDevConn(UINT32 Port, bool *pIsDevConn);

    //! \brief configuration USB device with address 0 (just reset)
    //--------------------------------------------------------------------------
    RC CfgDev(UINT32 Port, bool IsLoadClassDriver, UINT32 Speed, UINT32 RouteString, bool IsLegacy = false);

    //! \brief Get the handle of external USB device's object
    // this function searches Device Context Array for a matching
    //--------------------------------------------------------------------------
    RC GetExtDev(UINT08 RootHubPort, UINT32 RouteString, UsbDevExt ** pExtDev);

    //! \brief remove the specified Ring from m_vpRings
    //--------------------------------------------------------------------------
    //! Three deferent mode supported : precise mode, SlotId mode and SlotId except EP0 mode.
    //! if DelMode == XHCI_DEL_MODE_PRECISE, all parameters take account, when everthing matches, the tranfer ring will be del
    //! if DelMode == XHCI_DEL_MODE_SLOT, if SlotID match, the transferting will be deleted
    //! if DelMode == XHCI_DEL_MODE_SLOT_EXPECT_EP0, same as above expect that EP0 will not be deleted
    RC RemoveRingObj(UINT08 SlotId, UINT08 DelMode, UINT08 EpNum = 0, bool IsDataOut = true, UINT16 StreamId = 0);

    //! \brief remove the specified Stream Context Array from m_vpStreamArray
    //--------------------------------------------------------------------------
    RC RemoveStreamContextArray(UINT08 SlotId, bool IsSlotOnly, UINT08 EpNum = 0, bool IsDataOut = true);

    //! \brief Handle to access external USB device objects
    //--------------------------------------------------------------------------
    RC GetExtStorageDev(UINT08 UsbAddr, UsbStorage** ppStor);
    RC GetExtHidDev(UINT08 UsbAddr, UsbHidDev** ppHid);
    RC GetExtVideoDev(UINT08 UsbAddr, UsbVideoDev** ppVideo);
    RC GetExtHubDev(UINT08 UsbAddr, UsbHubDev** ppHub);
    RC GetExtConfigurableDev(UINT08 UsbAddr, UsbConfigurable** ppDev);

    //! \brief Initialize CMD / Event Ring
    //--------------------------------------------------------------------------
    RC InitCmdRing();
    RC InitEventRing(UINT32 Index);

    //==========================================================================
    //  framework
    //==========================================================================
    RC InitBar();
    RC UninitBar();
    RC InitRegisters();
    RC InitDataStructures();
    RC UninitRegisters();
    RC UninitDataStructures();
    RC ToggleControllerInterrupt(bool IsEnable);
    RC GetIsr(ISR* Func) const;

    RC DeviceEnableInterrupts();
    RC DeviceDisableInterrupts();

private:

    //! \brief Check if HC support 64bit address, not used by CheetAh
    //! \param [o] pIsSupport64 : if HW supports 64bit address
    RC IsSupport64bit(bool *pIsSupport64);

    //! \brief Launch the Falcon micro controller
    //! \param [i] pData32 : address of raw firmware binary data
    //! \param [i] Size : size of binary data
    RC LaunchFalcon(void *pData, UINT32 Size);

    //! \brief Do ARU_RST for the Falcon micro controller
    RC ResetFalcon();

    // release data structures that will be stale when user call Xhci.Uninit()
    RC ReleaseDataStructures();

    // counter for interrupt
    UINT32 m_EventCount[XHCI_MAX_INTR_NUM];     // n: IRQ from Evnet Ring n

    //! \brief Wait for bit cleared in specified register
    //--------------------------------------------------------------------------
    RC WaitRegBitClearAny(UINT32 Offset, UINT32 UnExp);

    //! \brief slot ID validation
    //--------------------------------------------------------------------------
    RC CheckDevValid(UINT08 SlotId);

    //! \brief Apply m_Ratio64Bit to get the random adress
    //--------------------------------------------------------------------------
    RC GetRndAddrBits(UINT32 *pDmaBits);

    //! \brief Set default size of each type of Ring
    //--------------------------------------------------------------------------
    RC SetRingSize(UINT08 Type, UINT32 Size);

    //! \brief Get supported Eci (Extended Capability Identifier)
    //--------------------------------------------------------------------------
    RC GetEci(void);

    //! \var  use with SetUserContext(), we should enforce this in the future
    static bool m_IsFullCopy;

    //! \var version of HC, could be 0.96 or 1.00 etc.
    UINT16      m_HcVer;

    //! \var Offsets for register sets
    UINT08      m_CapLength;
    UINT32      m_DoorBellOffset;
    UINT32      m_RuntimeOffset;

    //! \var Params from HCSPARAMS1
    UINT32      m_MaxSlots;
    UINT32      m_MaxIntrs;
    UINT32      m_MaxPorts;

    //! \var params from HCSPARAMS2
    UINT32      m_MaxEventRingSize;
    UINT32      m_MaxSratchpadBufs;

    //! \var params from HCCPARAMS
    UINT32      m_ExtCapPtr;    // offset in DWORD
    bool        m_Is64ByteContext;
    bool        m_IsNss;
    UINT08      m_MaxPSASize;

    //! \var params for HCCPARAMS2
    bool        m_IsGetSetCap;

    typedef struct _protocol
    {
        UINT16  UsbVer;   //0x300, 0x200
        UINT08  CompPortOffset;
        UINT08  CompPortCount;
        UINT08  ProtocolSlotType;
        UINT08  Psic;
        UINT32  Psi[16];
    }Protocol;
    vector<Protocol> m_vPortProtocol;

    //! \var arames from XHCI_REG_OPR_PAGESIZE
    UINT32      m_PageSize;

    //! \var Scratchpad Buffer Array
    void *      m_pScratchpadBufferArray;

    // Ring label scheme:
    // Base -- m_UuidOffset
    //   offset 0 : Command Ring
    //   offset 1 - maxIntrs : Event Rings
    //   offset maxIntrs+1 - : Xfer Rings
    // in the future we might have a couple of reverved Rings, m_UuidOffset is the number of them
    //! \var the offset for computing Command Ring ID
    UINT32      m_UuidOffset;
    //! \var latest Slot ID newly assigned by HC
    UINT32      m_NewSlotId;
    //! \var to hold input context specified by user for test / debug purpose
    vector<UINT32>  m_vUserContext;
    //! \var address alignment of input context specified by user for test / debug purpose
    UINT32 m_UserContextAlign;
    //! \var to hold lwstomized Stream Layout specified by user for test / debug purpose
    vector<UINT32>  m_vUserStreamLayout;

   //! \var Event for interrupts
    Tasker::EventID m_Event[XHCI_MAX_INTR_NUM];

    //! \var pointer to Device Context Array object
    DeviceContextArray *    m_pDevContextArray;
    //! \var pointer to Command Ring Object
    CmdRing *   m_pCmdRing;
    //! \var pointers to Event Ring objects
    EventRing * m_pEventRings[XHCI_MAX_INTR_NUM];

    //! \var transfer rings created in run time for endpoints
    vector<TransferRing *>  m_vpRings;
    //! \var Stream Arrays created in run time for endpoints
    vector<StreamContextArrayWrapper *>  m_vpStreamArray;

    //! \var pointers for xternal USB device objects
    // indexed by slot Id, entry 0 is dummy
    UsbDevExt * m_pUsbDevExt[XHCI_MAX_SLOT_NUM+1];

    //! \var default Command Ring segment size
    UINT32      m_CmdRingSize;
    //! \var default Event Ring segment size
    UINT32      m_EventRingSize;
    //! \var default Transfer Ring segment size
    UINT32      m_XferRingSize;

    //! \var If to insert Event Data TRB in Xfer Ring. TODO: apply this in control endpoint
    bool        m_IsInsertEventData;
    //! \var pointer of MEM used by firmware
    void *      m_pDfiMem;
    //! \var size of the firmware buffer
    UINT32      m_DfiLength;
    void *      m_pDfiScratchMem;

    //! \var for Power saving and restore
    void *      m_pRegBuffer;
    bool        m_IsFromLocal;
    vector<UINT32> m_vRunningEp;

    //! \var for randomization
    UINT32      m_Ratio64Bit;       // ratio to use 64bit address over 32bit address
    UINT32      m_RatioIdt;         // ratio of IDT bit get generated for out transfers less than 8 Bytes
    UINT32      m_EventData;        // ratio of insert EventDataTrb

    //! \var buffer for used by Xhci.InsetTd() if user doesn't care about the content
    void *      m_pDummyBuffer;
    UINT32      m_DummyBufferSize;

    //! \var offset of mailbox register set in PCI cfg space
    UINT32      m_OffsetPciMailbox;

    //! \var override for default Cerr setting in EP Context
    UINT08      m_CErr;

    //! \var record the pending CMD (no status collected yet)
    UINT32      m_PendingDmaCmd;
    //! \var record the total count of DMA operatieon for statistic
    UINT64      m_DmaCount;

    //! \var bit mask to specify appraoches used in USB device enumeration
    UINT32      m_EnumMode;

    //! \var bit mask to specify appraoches used in USB device enumeration
    UINT32      m_TestModePortIndex;

    //! \var mapping for defining which SS port maps to which HS Port on the board
    UINT32      m_SSPortMap;

    //!\var under certain situation we need to map more memory for the MMIO reigsters
    //! howevder the additional chunk is before the base address
    UINT32      m_LeadingSpace;

    //!var If to skip all RegWr in InitReg() for debug prupose
    bool        m_IsSkipRegWrInInit;

    //! BAR2 mapped virtual address
    void *      m_pBar2;
    PHYSADDR    m_PhysBAR2;

    //!var save the enumeration of supported entended capabilities
    UINT16      m_Eci;
    //!var for BTO (Bus Time Out) test
    bool        m_IsBto; // switch for enable/disable bus error counter while transfer
    UINT32      m_BusErrCount; // record bus error number while transfer
};

#endif // __XHCIDEV_H
