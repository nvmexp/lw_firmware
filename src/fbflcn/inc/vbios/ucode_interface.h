#if !defined(__UCODE_INTERFACE_H__INCLUDED_)
#define __UCODE_INTERFACE_H__INCLUDED_

#include "cert20.h"
#include "ucode_postcodes.h"

/*
    Command interface design summary:
        LW_PPWR_FALCON_MAILBOX0  is used to accept command and reflect command lifecycle: CMD_STS_NEW,   CMD_STS_PENDING, CMD_STS_COMPLETE
        LW_PPWR_FALCON_MAILBOX1  is used to return command error code, and indicate availability of log infomation

        cmd_in buffer is used to hold command parameters when command is issued.  It should be filled in by external agent, Like Lwflash, and use by command routine in uCode.
        cmd_out buffer is used to hold output data by the command. It is filled in by uCode command and read by external agent, like Lwflash
        lwf_img_data_buffer in DMEM is used to hold data for commands.
        printfBufferHdr in DMEM is used to hold printf log infomaiton when available.

        Command fetcher runs in unselwre mode.  It fetches command from _MAILBOX0 register.
        Dispacter is runs in secure mode. it dispatches uCode command routine.
*/


/*
 * DMEM Mapper Structure -
 * Holds pointers to various data & structures stored in the DMEM
 *
 */
#define DMEMMAPPER_SIG      0x50414d44      // DMEMMAPPER signature "DMAP"
#define PLAYREADY_PDUB_SIG  0x59445250      // PlayReady PDUB signature "PRDY" (ASCII)

typedef enum
{
    APPLICATION_ID_INTERFACE_NONE                    = 0x00,
    APPLICATION_ID_INTERFACE_DEVINIT_ENGINE,        // 0x01
    APPLICATION_ID_INTERFACE_DEVINIT_COMPACTION,    // 0x02
    APPLICATION_ID_INTERFACE_DEVINIT_WAR_PATCHER,   // 0x03
    APPLICATION_ID_INTERFACE_DMEM_MAPPER,           // 0x04
    APPLICATION_ID_INTERFACE_MULTIPLE_TARGET,       // 0x05
    APPLICATION_ID_INTERFACE_SMBPBI_SERVER,         // 0x06
    APPLICATION_ID_INTERFACE_INFOROM_FEATURES,      // 0x07
    APPLICATION_ID_INTERFACE_PLAYREADY_PDUB,        // 0x08
    APPLICATION_ID_INTERFACE_RESERVED
} APPLICATION_ID_INTERFACE;

struct DMEMMAPPER {
        LwU32 signature;
        LwU16 version;
        LwU16 size;
        LwU32 cmd_in_buffer_offset;
        LwU32 cmd_in_buffer_size;
        LwU32 cmd_out_buffer_offset;
        LwU32 cmd_out_buffer_size;
        LwU32 lwf_img_data_buffer_offset;
        LwU32 lwf_img_data_buffer_size;
        LwU32 printfBufferHdr;
        LwU32 rsvd;
};

/*
  DMEMMAPPER_V2 should be used when version field with value 0x2
     This version added
        1. uCode build time stamp.
        2. uCode signature pointers.
*/
struct DMEMMAPPER_V2 {
        LwU32 signature;
        LwU16 version;
        LwU16 size;
        LwU32 cmd_in_buffer_offset;
        LwU32 cmd_in_buffer_size;
        LwU32 cmd_out_buffer_offset;
        LwU32 cmd_out_buffer_size;
        LwU32 lwf_img_data_buffer_offset;
        LwU32 lwf_img_data_buffer_size;
        LwU32 printfBufferHdr;                 // Use LwU32 for 64bit structure pointer compatibility -- struct PRINTFBUFFERHDR* printfBufferHdr;
        LwU32 ucode_build_time_stamp;          // Use LwU32 for 64bit structure pointer compatibility -- struct BUILDTIMESTAMP* ucode_build_time_stamp;
        LwU32 ucode_signature;                 // Use LwU32 for 64bit structure pointer compatibility -- struct UCODESIGNATURE* ucode_signature;
};

#define LW_DMEMMAPPER_INIT_CMD_INIT_CMD                 7:0
#define LW_DMEMMAPPER_INIT_CMD_RSVD                    31:8

#define LW_DMEMMAPPER_UCODE_FEATURE_REG_PRINTF          0:0
#define LW_DMEMMAPPER_UCODE_FEATURE_REG_PRINTF_YES        1
#define LW_DMEMMAPPER_UCODE_FEATURE_REG_PRINTF_NO         0
#define LW_DMEMMAPPER_UCODE_FEATURE_SEC_PRINTF          1:1
#define LW_DMEMMAPPER_UCODE_FEATURE_SEC_PRINTF_YES        1
#define LW_DMEMMAPPER_UCODE_FEATURE_SEC_PRINTF_NO         0
#define LW_DMEMMAPPER_UCODE_FEATURE_REG_ACCESS          2:2    // feature that use HS falcon to access register for caller.  It is for debug only
#define LW_DMEMMAPPER_UCODE_FEATURE_REG_ACCESS_YES        1
#define LW_DMEMMAPPER_UCODE_FEATURE_REG_ACCESS_NO         0
#define LW_DMEMMAPPER_UCODE_FEATURE_CMD_INTERFACE       3:3    // Command Interface is on/off
#define LW_DMEMMAPPER_UCODE_FEATURE_CMD_INTERFACE_YES     1
#define LW_DMEMMAPPER_UCODE_FEATURE_CMD_INTERFACE_NO      0
#define LW_DMEMMAPPER_UCODE_FEATURE_ALT_LOAD            4:4    // Alternate uCode load type.  Alternate load could be a smaller uCode.
#define LW_DMEMMAPPER_UCODE_FEATURE_ALT_LOAD_YES          1    //   Alternate uCode load supported
#define LW_DMEMMAPPER_UCODE_FEATURE_ALT_LOAD_NO           0    //   Alternate uCode load not supported

struct LWFLASH_SELWRITY_DATA_SIGNATURES {
    LwU32   vProductionSignatures[4];
    LwU32   vDebugSignatures[4];
};

struct APPLICATION_INTERFACE_HEADER {
    LwU8    byVersion;
    LwU8    byHeaderSize;
    LwU8    byEntrySize;
    LwU8    byEntryCount;
};

struct APPLICATION_INTERFACE_ENTRY {
    LwU32   dwID;
    LwU32   dwPtr;
};

struct PLAYREADY_PDUB_INTERFACE_V1 {
    LwU32   dwSignature;
    LwU16   wVersion;
    LwU16   wStructSize;
    LwU16   wAppVersion;
    LwU16   wAppFeatures;
    LwU32   dwReserved;
};

/*
  DMEMMAPPER_V3 should be used when version field with value 0x3
     This version added
         falcon_type
         ucode_feature
*/
struct DMEMMAPPER_V3 {
        LwU32 signature;
        LwU16 version;
        LwU16 size;
        LwU32 cmd_in_buffer_offset;
        LwU32 cmd_in_buffer_size;
        LwU32 cmd_out_buffer_offset;
        LwU32 cmd_out_buffer_size;
        LwU32 lwf_img_data_buffer_offset;
        LwU32 lwf_img_data_buffer_size;
        LwU32 printfBufferHdr;                 // Use LwU32 for 64bit structure pointer compatibility -- struct PRINTFBUFFERHDR* printfBufferHdr;
        LwU32 ucode_build_time_stamp;          // Use LwU32 for 64bit structure pointer compatibility -- struct BUILDTIMESTAMP* ucode_build_time_stamp;
        LwU32 ucode_signature;                 // Use LwU32 for 64bit structure pointer compatibility -- struct UCODESIGNATURE* ucode_signature;
        LwU32 init_cmd;                        // Specify initial falcon command
        LwU32 ucode_feature;                   // Field used to record uCode capabilities,  like printf service,  PMU uCode level register access
        LwU32 ucode_cmd_mask0;                 // Supported command.  bit set signal command available.  bit position corrosponding command number from 0.
        LwU32 ucode_cmd_mask1;                 // Supported command.  bit set signal command available.  bit position corrosponding command number from 32.
        LwU32 multiTgtTbl;                     // Point to multi target interface table.  used to specify Falcon type ( PMU, LWDEC or SEC),  load type and stack top.
};


//  same as MULTI_TGT_INTERFACE in VBIOS PMU interface.h,  put here for lwflash to pick up,  change a struct name to avoid conflict
struct MULTI_FALCON_INTERFACE
{
    LwU16   version;
    LwU16   structSize;
    LwU16   appVersion;
    LwU16   appFeatures;
    LwU8    targetId;
#define LW_MULTI_TGT_IFACE_TGT_ID_NONE          0
#define LW_MULTI_TGT_IFACE_TGT_ID_PMU           1
#define LW_MULTI_TGT_IFACE_TGT_ID_DPU           2
#define LW_MULTI_TGT_IFACE_TGT_ID_FECS          3
#define LW_MULTI_TGT_IFACE_TGT_ID_LWDEC         4
#define LW_MULTI_TGT_IFACE_TGT_ID_SEC           5

    LwU8    loadType;
#define LW_MULTI_TGT_IFACE_LOAD_TYPE_ENTIRE     0
#define LW_MULTI_TGT_IFACE_LOAD_TYPE_ALT        1

    LwU16   reserved10;
    LwU32   initStack;
};


//
// These is the list of PMU registers that also used in SEC2 and LWDEC
//
// LW_PPWR_FALCON_MAILBOX0
// LW_PPWR_FALCON_MAILBOX1
// LW_PPWR_FALCON_HWCFG
// LW_PPWR_FALCON_IMEMC
// LW_PPWR_FALCON_IMEMD
// LW_PPWR_FALCON_IMEMT
// LW_PPWR_FALCON_DMEMC
// LW_PPWR_FALCON_DMEMD
//
// LW_PPWR_FALCON_CPUCTL
// LW_PPWR_FALCON_DMATRFCMD
// LW_PPWR_FALCON_DMACTL
// LW_PPWR_FALCON_BOOTVEC
//
// LW_PPWR_FALCON_ICD_CMD
// LW_PPWR_FALCON_ICD_RDATA
//
// LW_PPWR_PMU_RESET_THERM    -- PMU only
// LW_PPWR_FALCON_SFTRESET    -- PMU only

/*
 * Unified internal REGISTER_MAP structure used in Core84 and earlier
 */
#define LW_FALCREG_MAP_VERSION                     7:0
#define LW_FALCREG_MAP_VERSION_VER1               0x01    // version of the struct
#define LW_FALCREG_MAP_SIZE                       23:8    // size of this struct
#define LW_FALCREG_MAP_RSVD                      31:24

struct FALCON_REGISTER_MAP_INT        // Registers used in uCode
{
        LwU32 structctrl;             // version, size of the struct.  use bit fields for smaller size for uCode
        LwU32 mailbox0;
        LwU32 mailbox1;
        LwU32 bar0Addr;
        LwU32 bar0Data;
        LwU32 bar0Csr;
        LwU32 bar0Timeout;
        LwU32 scpCtlCfg;
        LwU32 scpCtlStat;
        LwU32 gptmrint;
        LwU32 irqdest;
        LwU32 irqmset;
        LwU32 irqmclr;
        LwU32 irqsclr;
        LwU32 gptmrctl;
        LwU32 debuginfo;
        LwU32 sctl;
        LwU32 ptimer0;
        LwU32 ptimer1;
};

// This struct is used by core87+ only.
struct FALCON_REGISTER_MAP_INT_V2        // Registers used in uCode
{
        LwU32 structctrl;             // version, size of the struct.  use bit fields for smaller size for uCode
        LwU32 mailbox0;
        LwU32 mailbox1;
        LwU32 bar0Addr;
        LwU32 bar0Data;
        LwU32 bar0Csr;
        LwU32 bar0Timeout;
        LwU32 scpCtlCfg;
        LwU32 scpCtlStat;
        LwU32 gptmrint;
        LwU32 irqdest;
        LwU32 irqmset;
        LwU32 irqmclr;
        LwU32 irqsclr;
        LwU32 gptmrctl;
        LwU32 debuginfo;
        LwU32 sctl;
        LwU32 ptimer0;
        LwU32 ptimer1;
        LwU32 falconExterrAddr;
        LwU32 falconExterrStat;
        LwU32 sctl1;
        LwU32 tracepc;
        LwU32 traceidx;
        LwU32 ibrkpt1;
        LwU32 dmemCOffsetZero;
        LwU32 dmemDOffsetZero;
        LwU32 falconDOCControl;
        LwU32 falconDOCD0;
        LwU32 falconDOCD1;
        LwU32 falconDICControl;
        LwU32 falconDICD0;
        LwU32 fbifTransCfg;
        LwU32 fbifRegionCfg;
        LwU32 irqPLMAddr;
        LwU32 irqSCMaskAddr;
        LwU32 resetPLMAddr;
        LwU32 fbifTransCfgAddr;
        LwU32 fbifTransCfgStride;
        LwU32 fbifRegionCfgAddr;
        LwU32 fbifCtl;
};

/*
 * Secure/Inselwre internal REGISTER_MAP structures used in Core86 and later
 */
#define LW_FALCREG_MAP_SELWRE_VERSION                         7:0
#define LW_FALCREG_MAP_SELWRE_VERSION_VER1                   0x01    // version of the struct
#define LW_FALCREG_MAP_SELWRE_SIZE                           23:8    // size of this struct
#define LW_FALCREG_MAP_SELWRE_RSVD                          31:24

#define LW_FALCREG_MAP_UNSELWRE_VERSION                       7:0
#define LW_FALCREG_MAP_UNSELWRE_VERSION_VER1                 0x01    // version of the struct
#define LW_FALCREG_MAP_UNSELWRE_SIZE                         23:8    // size of this struct
#define LW_FALCREG_MAP_UNSELWRE_RSVD                        31:24

struct FALCON_REGISTER_MAP_SELWRE_INT        // Registers used in uCode
{
        LwU32 structctrl;             // version, size of the struct.  use bit fields for smaller size for uCode
        LwU32 mailbox0;
        LwU32 mailbox1;
        LwU32 bar0Addr;
        LwU32 bar0Data;
        LwU32 bar0Csr;
        LwU32 bar0Timeout;
        LwU32 scpCtlCfg;
        LwU32 sctl;
        LwU32 ptimer1;
        LwU32 ptimer0;
        LwU32 falconIrqDest;
        LwU32 falconIrqMset;
        LwU32 falconExterrAddr;
        LwU32 falconExterrStat;
        LwU32 sctl1;
        LwU32 falconDOCControl;
        LwU32 falconDOCD0;
        LwU32 falconDOCD1;
        LwU32 falconDICControl;
        LwU32 falconDICD0;
        LwU32 tracepc;
        LwU32 traceidx;
        LwU32 ibrkpt1;
};

struct FALCON_REGISTER_MAP_UNSELWRE_INT        // Registers used in uCode
{
        LwU32 structctrl;             // version, size of the struct.  use bit fields for smaller size for uCode
        LwU32 gptmrint;
        LwU32 irqdest;
        LwU32 irqmset;
        LwU32 irqmclr;
        LwU32 irqsclr;
        LwU32 gptmrctl;
        LwU32 scpCtlStat;
        LwU32 debuginfo;
        LwU32 timer0;
        LwU32 timer1;
        LwU32 tracepc;
        LwU32 traceidx;
        LwU32 ibrkpt1;
};


struct FALCON_REGISTER_MAP_EXT        // Registers used in loader
{
        LwU32 structctrl;             // version, size of the struct.  use bit fields for smaller size for uCode
        LwU32 mailbox0;
        LwU32 mailbox1;
        LwU32 hwcfg;
        LwU32 hwcfg1;
        LwU32 imemc;
        LwU32 imemd;
        LwU32 imemt;
        LwU32 dmemc;
        LwU32 dmemd;
        LwU32 cpuctl;
        LwU32 dmatrfcmd;
        LwU32 dmactl;
        LwU32 bootvec;
        LwU32 icd_cmd;
        LwU32 icd_rdata;
};


 /**
 * HULK Overclocking License Requst
 * This structure holds all information needed to process
 * a overclocking license request from a user
 *
 */

 #define LW_HOLR_TOOL_LWFLASH           0x1
 #define LW_HOLR_TOOL_LWLICENSEFLASH    0x2

 // Pack the HOLR structure
 #if defined(LW_FALCON) || defined(GCC_FALCON)
 #pragma pack(1)
 #else
 #pragma pack(push, 1)
 #endif

 typedef struct {
     LwU8        identifier[4];       // "HOLR"
     LwU8        version;             // version of this structure
     LwU8        size;                // size of this structure
     LwU8        toolID;              // Lwflash, Lwlicenseflash or others
     LwU32       toolVersion[4];      // Version of the tool used.
     LwU8        fuseType;            // Debug or Production fuse
     LwU32       hulkType;            // Indicate types of HULK this request is for
     LwU32       gpuid[8];            // sha256 hash of ECID
     LwU16       devid;               // Device ID
     LwU32       nonce[4];            // 128 bit random number fed to generate a derived key
     LwU32       hmac_aes[4];         // 128 bit signature = AES128 (key, hash)
 } HULK_OC_LICENSE_REQ, *PHULK_OC_LICENSE_REQ;

/**
 * HULK Overclocking License Requst Hash
 * This structure holds the hash of the request struct (HOLR)
 *
 *  @author Varun Kumar
 */
 struct HULK_OC_LICENSE_REQ_SHA256
 {
     LwU8  holr_sha256[32];
 };

 #if defined(LW_FALCON) || defined(GCC_FALCON)
 #pragma pack()
 #else
 #pragma pack(pop)
 #endif

 // End of packing for HOLR structure

 /*
 * Debug Buffer Header
 * Header Structure for the debug buffer
 *
 */

struct DBGBUFFERHDR {
        LwU16 start;
        LwU16 end;
        LwU16 entryCount;
        LwU8 flag;
};

/*
 * PRINTFBUFFERHDR
 * Header Structure for the basic
 * style debug buffer.
 *
 */


#define LW_PRINTF_BUFFER_FLAG                  16:0
#define LW_PRINTF_BUFFER_FLAG_OVERFLOW         0x01       // _FLAG_OVERFLOW indicates a printf buffer overflow oclwrred.
#define LW_PRINTF_BUFFER_FLAG_USED             0x02       // _FLAG_USED indicates at least one printf statement is present.
                                                          // this flag will be part of command return status for upper levle software,
                                                          // like Lwflash, so that Lwflash would know, a printf log is avaialable.
#if defined(LW_UNIX) || defined(LW_DOS)
struct PRINTFBUFFERHDR {
        LwU32 offset;
        LwU32 pBuffer_offset;
        LwU32 pBuffer_size;
        LwU32 Flag;
} __attribute__((__may_alias__));
#else
struct PRINTFBUFFERHDR {
        LwU32 offset;
        LwU32 pBuffer_offset;
        LwU32 pBuffer_size;
        LwU32 Flag;
};
#endif

/*
 * Debug Buffer States
 */

#define DEBUG_BUFFER_WRITE_START        0x01
#define DEBUG_BUFFER_WRITE_COMPLETE     0x02
#define DEBUG_BUFFER_READ_START         0x03
#define DEBUG_BUFFER_READ_COMPLETE      0x04
#define DEBUG_BUFFER_EMPTY              0x05

/*
    uCode build time
*/
struct BUILDTIMESTAMP {
       LwU32 bldDate;
       LwU32 bldTime;
};

/*
    uCode Signature
*/
struct UCODESIGNATURE {
      LwU32 prodSig;
      LwU32 debugSig;
};

/* ---------------------- List of lwflash-pmu commands ---------------------*/

#define LW_UCODE_CMD_COMMAND               27:0       /* R-XVF */
#define LW_UCODE_CMD_COMMAND_NONE          0x00000000 /* R---V */
#define LW_UCODE_CMD_COMMAND_INIT          0x00000001 /* uCode initialize */
#define LW_UCODE_CMD_COMMAND_EID           0x00000002 /* Get EEPROM ID */
#define LW_UCODE_CMD_COMMAND_ESI           0x00000003 /* EEPROM_RECORD Struct Init*/
#define LW_UCODE_CMD_COMMAND_ERD           0x00000004 /* Read EEPROM */
#define LW_UCODE_CMD_COMMAND_EWR           0x00000005 /* Write to EEPROM*/
#define LW_UCODE_CMD_COMMAND_ESE           0x00000006 /* EEPROM Erase Sector */
#define LW_UCODE_CMD_COMMAND_ECE           0x00000007 /* EEPROM Chip Erase */
#define LW_UCODE_CMD_COMMAND_RRD           0x00000008 /* Priv Register Read using PMU */
#define LW_UCODE_CMD_COMMAND_RWR           0x00000009 /* Priv Register Write using PMU */
#define LW_UCODE_CMD_COMMAND_PREP          0x0000000a /* Preperation stage for flashing */
#define LW_UCODE_CMD_COMMAND_CLOSE         0x0000000b /* Close and restore stage for flashing */
#define LW_UCODE_CMD_COMMAND_EPROT         0x0000000c /* Set EEPROM software protection */
#define LW_UCODE_CMD_COMMAND_ERDSR         0x0000000d /* EEPROM Read Status Register */
#define LW_UCODE_CMD_COMMAND_VV            0x0000000e /* Start VBIOS Verify process*/
#define LW_UCODE_CMD_COMMAND_ECID          0x0000000f /* Read ECID from GPU */
#define LW_UCODE_CMD_COMMAND_LICVERIFY     0x00000010 /* Verify License against GPU */
#define LW_UCODE_CMD_COMMAND_BSI_INFO      0x00000011 /* Display BSI infomation */
#define LW_UCODE_CMD_COMMAND_HULKPROC      0x00000012 /* Process HULK license   */
#define LW_UCODE_CMD_COMMAND_ARB           0x00000013 /* Command for arbitrary purpose */
#define LW_UCODE_CMD_COMMAND_HULKREQ       0x00000014 /* Request HULK license */
#define LW_UCODE_CMD_COMMAND_FRTS          0x00000015 /* For fw runtime security */
#define LW_UCODE_CMD_COMMAND_PRDY          0x00000016 /* Start PlayReady main procedure*/
#define LW_UCODE_CMD_COMMAND_OTP_READ      0x00000017 /* Read EEPROM OTP data */
#define LW_UCODE_CMD_COMMAND_OTP_READLOCK  0x00000018 /* Read EEPROM OTP Lock state from Status Reg */
#define LW_UCODE_CMD_COMMAND_SELWRE_BOOT   0x00000019 /* Start firmware secure boot process */


/* --------------- Possible states of lwflash-pmu commands -----------------*/

#define LW_UCODE_CMD_STS                   31:28      /* R-XVF */
#define LW_UCODE_CMD_STS_NONE              0x00000000 /* R---V */
#define LW_UCODE_CMD_STS_NEW               0x00000001 /* R---V */
#define LW_UCODE_CMD_STS_PENDING           0x00000002 /* R---V */
#define LW_UCODE_CMD_STS_COMPLETE          0x00000003 /* R---V */

/* ---------------- Return code from pmu after exelwting a command -----------------------
   This set of defines is used when error code is reported through PMU mailbox register
------------------------------------------------------------------------------------------*/

#define LW_UCODE_ERR_CODE                  30:0       /* R-XVF */
#define LW_UCODE_ERR_LOG                   31:31      /* R-XVF */
#define LW_UCODE_ERR_LOG_NO                0x00000000 /* R---V */
#define LW_UCODE_ERR_LOG_YES               0x00000001 /* R---V */

#define LW_UCODE_EXCEPTION_STATUS_BASE                             LW_PBUS_VBIOS_SCRATCH
#define LW_UCODE_EXCEPTION_STATUS_REG                                       7      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(7)
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID                                  27:24    //  Stores UCode ID of the ucode which has created the exception.
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID_NONE                             0x00000000
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID_PREOS_APP                        0x00000001
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID_GC6                              0x00000002
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID_COMPACTION                       0x00000003
#define LW_UCODE_EXCEPTION_STATUS_UCODE_ID_UDE                              0x00000004

/*---------------- Structures used by command handlers  ------------------*/

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_EID:   NONE  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_EID:   OUT_CMD_EID */
/*
 * struct OUT_CMD_EID
 *
 * Return data from cmd_eeprom_id()
 * cmd_eeprom_id() returns the
 * manufacturer's id and device id
 * of the EEPROM on the GPU board.
 *
 */

typedef struct {
        LwU16 mfgID;
        LwU16 devID;
} OUT_CMD_EID, *POUT_CMD_EID;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ESI:   EEPROM_RECORD */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ESI:   NONE  */

/*
 * EEPROM_RECORD struct
 * This struct contains all the details of a given EEPROM.
 * Functions that read/write/erase an EEPROM will need the following data.
 *
 * The original definition of this struct in LwFlash code is as follows
 *
 * struct EEPROMRecord
 * {
 *     BYTE byManufacturerCode;
 *     WORD wDeviceCode;
 *     WORD wBytesPerPage;
 *     WORD wKilobytesPerBlock; // Might actually be sector erase for some parts
 *     bool bEraseRequired;
 *     bool bTVUpdateSupported;
 *     bool bSerial;
 *     char* szManufacturer;
 *     char* szVoltage;
 *     char* szName;
 *     DWORD dwSizeDepthKbits;
 *     WORD wSizeWidthBits;
 *     BYTE byChipEraseCmd;
 *     BYTE byBlockEraseCmd;   // Might actually be sector erase for some parts
 *     BYTE byWriteProtectMask;
 *     bool bWriteProtectOff;
 * }
 *
 */

typedef struct {
        LwU16 mfgID;
        LwU16 devID;
        LwU16 bytesPerPage;
        LwU16 kiloBytesPerPage;
        LwU16 eraseRequired;
        LwU16 serial;
        LwU32 sizeDepthKBits;
        LwU16 sizeWidthBits;
        LwU16 chipEraseCommand;
        LwU16 blockEraseCommand;
        LwU16 writeProtectMask;
        LwU16 writeProtectOff;
        LwU16 featureFlag;
} EEPROM_RECORD, *PEEPROM_RECORD;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ERD:   IN_CMD_ERD  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ERD:   NONE        */
/*
 * struct IN_CMD_ERD
 *
 * Input data for cmd_eeprom_read()
 * The function needs a starting
 * address to start reading from
 * and the number of bytes to read.
 *
 */
typedef struct {
        LwU32 startAddress;
        LwU32 bytes;
} IN_CMD_ERD, *PIN_CMD_ERD;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ERSFDP:   IN_CMD_ERSFDP  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ERSFDP:   OUT_CMD_ERSFDP  */
/*
 * struct IN_CMD_ERSFDP
 *
 * Input data for cmd_eeprom_rsfdp()
 * The function reads the  Serial Flash Discoverable Parameters
 *     startAddress  start address
 *     size          Number of bytes to read back
 */
typedef struct {
        LwU32 startAddress;
        LwU32 size;
} IN_CMD_ERSFDP, *PIN_CMD_ERSFDP;

typedef struct {
        LwU32 regVal;
} OUT_CMD_ERSFDP, *POUT_CMD_ERSFDP;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ESE:   IN_CMD_ESE  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ESE:   NONE        */
/*
 * struct IN_CMD_ESE
 *
 * Input data for cmd_eeprom_se()
 * The function needs a starting
 * address to start erasing the
 * sector from.
 *
 */
typedef struct {
        LwU32 startAddress;
} IN_CMD_ESE, *PIN_CMD_ESE;



/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ECE:   NONE        */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ECE:   NONE        */




/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_EWR:   IN_CMD_EWR  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_EWR:   NONE        */
/*
 * struct IN_CMD_EWR
 *
 * Input data for cmd_eeprom_wr()
 * The function programms the EEPOROM
 *
 */
#define LW_IN_CMD_EWR_CTRL_FLAG_NO_ERASE              0:0   // Indicate No Erase prior to programming
#define LW_IN_CMD_EWR_CTRL_FLAG_NO_ERASE_NO             0
#define LW_IN_CMD_EWR_CTRL_FLAG_NO_ERASE_YES            1   // does not erase prior to programming - requires license
typedef struct {
        LwU32 startAddress;
        LwU32 bytes;
        LwU32 ctrl_flag;    // Control information
} IN_CMD_EWR, *PIN_CMD_EWR;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_RRD:   IN_CMD_RRD  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_RRD:   OUT_CMD_RDD */
/*
 * struct IN_CMD_RRD
 *
 * Input data for cmd_reg_read()
 * The function read private registers.
 *
 */
typedef struct {
        LwU32 regAddress;
        LwU32 num;
} IN_CMD_RRD, *PIN_CMD_RRD;

typedef struct {
        LwU32 regVal;
} OUT_CMD_RRD, *POUT_CMD_RRD;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_RWR:   IN_CMD_RWR  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_RWR:   None  */
/*
 * struct IN_CMD_RWR
 *
 * Input data for cmd_reg_write()
 * The function write private registers.
 *
 */
typedef struct {
        LwU32 regAddress;
        LwU32 regVal;
} IN_CMD_RWR, *PIN_CMD_RWR;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_RWR:   IN_CMD_RWR  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_RWR:   None  */
/*
 * struct IN_CMD_RWR
 *
 * Input data for cmd_reg_write()
 * The function write private registers.
 *
 */

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_PREP:   IN_CMD_PREP  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_PREP:   OUT_CMD_PREP  */
/*
 * struct IN_CMD_PREP
 *
 * Input data for cmd_lwf_prep()
 * The function write private registers and
 * verify License passed in lwf_img_data_buffer
 * with parameters of license size and control
 * flag.
 *
 */
#define LW_IN_CMD_PREP_CTRL_FLAG_LICENSE_PRESENT          0:0
#define LW_IN_CMD_PREP_CTRL_FLAG_LICENSE_PRESENT_NO         0
#define LW_IN_CMD_PREP_CTRL_FLAG_LICENSE_PRESENT_YES        1
#define LW_IN_CMD_PREP_LICENSE_BUFFER_SIZE               1024   // DMEM buffer size for holding LWF-ENG/HULK license
#define LW_IN_CMD_PREP_CTRL_FLAG_USE_SWSPI                1:1
#define LW_IN_CMD_PREP_CTRL_FLAG_USE_SWSPI_NO               0
#define LW_IN_CMD_PREP_CTRL_FLAG_USE_SWSPI_YES              1

typedef struct {
        LwU32 ctrl_flag;    // Control information
        LwU32 lic_size;     // Size of lwf license in lwf_img_data_buf
        LwU32 spi_clk_freq; // Target frequency for SPI clock (Hz)
} IN_CMD_PREP, *PIN_CMD_PREP;

typedef struct {
        LwU32 spi_clk_freq; // Actual frequency for SPI clock (Hz)
} OUT_CMD_PREP, *POUT_CMD_PREP;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_CLOSE:   None  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_CLOSE:   None  */
/*
 * struct IN_CMD_RWR
 *
 * Input data for cmd_reg_write()
 * The function write private registers.
 *
 */

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_EPROT:   IN_CMD_EPROT  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_EPROT:   None  */
/*
 * struct IN_CMD_EPROT
 *
 * Input data for cmd_eeprom_protect()
 * The function enables or disables SW write protection.
 *     swProtectState
 *       0 = disable
 *       1 = enable
 */
typedef struct {
        LwU32 swProtectState;
} IN_CMD_EPROT, *PIN_CMD_EPROT;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ERDSR:   IN_CMD_ERDSR  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ERDSR:   OUT_CMD_ERDSR  */
/*
 * struct IN_CMD_ERDSR
 *
 * Input data for cmd_eeprom_rdsr()
 * The function reads the EEPROM status register(s).
 *     regIdx   Status register index (0,1)
 *     regVal   Value read from status register
 */
typedef struct {
        LwU32 regIdx;
} IN_CMD_ERDSR, *PIN_CMD_ERDSR;

typedef struct {
        LwU32 regVal;
} OUT_CMD_ERDSR, *POUT_CMD_ERDSR;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_VV:   IN_CMD_VV  ( Input Structure used by Lwflash  */
/*                                           IN_CMD_FWSECLIC_VV ( Input Structure used by fwseclic */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_VV:   NONE        */
/*
 * struct IN_CMD_VV
 *
 * Input data for cmd_eeprom_vv()
 * The function Verifies VBIOS signature
 *
 */
#define LW_IN_CMD_VV_CTRL_FLAG_HULK_PRESENT                               0:0
#define LW_IN_CMD_VV_CTRL_FLAG_HULK_PRESENT_YES                           1
#define LW_IN_CMD_VV_CTRL_FLAG_HULK_PRESENT_NO                            0
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_OVERRIDE_TBL_PRESENT                 1:1
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_OVERRIDE_TBL_PRESENT_YES             1
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_OVERRIDE_TBL_PRESENT_NO              0
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_REBRAND                              2:2
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_REBRAND_YES                          1
#define LW_IN_CMD_VV_CTRL_FLAG_DEVID_REBRAND_NO                           0
#define LW_IN_CMD_VV_CTRL_FLAG_MAGIC_NUMBER_PRESENT                       3:3
#define LW_IN_CMD_VV_CTRL_FLAG_MAGIC_NUMBER_PRESENT_YES                   1
#define LW_IN_CMD_VV_CTRL_FLAG_MAGIC_NUMBER_PRESENT_NO                    0


typedef struct {
        LwU32 ctrl_flag;                     // Control infomation
        LwU32 hulk_size;                     // size of HULK certificate in lwf_img_data_buf buffer
        LwU32 devid_override_tbl_offset;     // VBIOS ROM offset of DevID Override Table
        LwU32 magic_number_offset;           // VBIOS ROM offset of magic number
} IN_CMD_VV, *PIN_CMD_VV;

#define LW_IN_CMD_FWSECLIC_VV_VERSION_VER                                7:0
#define LW_IN_CMD_FWSECLIC_VV_VERSION_VER_1                               1
#define LW_IN_CMD_FWSECLIC_VV_VERSION_SIG                               31:16
#define LW_IN_CMD_FWSECLIC_VV_VERSION_SIG_FWSECLIC                     0x4346    // "FC" to indicate this input struct is for fwseclic

#define LW_IN_CMD_FWSECLIC_VV_FLAG_MEDIA                                 3:0
#define LW_IN_CMD_FWSECLIC_VV_FLAG_MEDIA_EEPROM                           0
#define LW_IN_CMD_FWSECLIC_VV_FLAG_MEDIA_SYSMEM_NONCOHERENT               1
#define LW_IN_CMD_FWSECLIC_VV_FLAG_MEDIA_SYSMEM_COHERENT                  2
#define LW_IN_CMD_FWSECLIC_VV_FLAG_MEDIA_FB                               3


typedef struct {
        LwU32 version;                       // Version and etc.
        LwU64 fw_media_offset;               // Media offset of the copy of VBIOS FW Image,  Media could be System memory, FB and etc.
        LwU32 fw_media_size;                 // VBIOS FW image size in media,  Media could be System memory, FB and etc.
        LwU32 flag;                          // Control flag, specify media type and etc.
} IN_CMD_FWSECLIC_VV, *PIN_CMD_FWSECLIC_VV;

#define LW_OUT_CMD_VV_VERSION                               1

#define LW_OUT_CMD_VV_CTRL_FLAG_VDPA_FINALIZED             0:0
#define LW_OUT_CMD_VV_CTRL_FLAG_VDPA_FINALIZED_NO           0
#define LW_OUT_CMD_VV_CTRL_FLAG_VDPA_FINALIZED_YES          1
#define LW_OUT_CMD_VV_CTRL_FLAG_DEVID_TABLE_VALIDATED      1:1       // Indicate DevID match list has been finalized by Lwflash
#define LW_OUT_CMD_VV_CTRL_FLAG_DEVID_TABLE_VALIDATED_NO    0
#define LW_OUT_CMD_VV_CTRL_FLAG_DEVID_TABLE_VALIDATED_YES   1

// this status is for quick status access. same status can be found in cert2.0 related global structure too.
#define LW_OUT_CMD_VV_INTBLK_STATUS_INTBLK_PRESENT                0:0     // Indicate Internal cert block present in VBIOS image
#define LW_OUT_CMD_VV_INTBLK_STATUS_INTBLK_PRESENT_NO              0
#define LW_OUT_CMD_VV_INTBLK_STATUS_INTBLK_PRESENT_YES             1
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_TABLE_PRESENT            1:1    // Indicate VDPA Table is present in VBIOS image
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_TABLE_PRESENT_NO          0
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_TABLE_PRESENT_YES         1
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_INTSIG_PRESENT           2:2    // Indicate Internal Signature is present
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_INTSIG_PRESENT_NO         0
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_INTSIG_PRESENT_YES        1
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_VALIDATED                3:3    // Indicate VDPA table has been finalized and validated by Lwflash
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_VALIDATED_NO              0
#define LW_OUT_CMD_VV_INTBLK_STATUS_VDPA_VALIDATED_YES             1
typedef struct {
        LwU32 version;             // Version of this struct   -- Note,  VV command becomes a complex command,  add struct version control
        LwU32 size;                // size of this struct
        LwU32 ctrl_flag;           // Control infomation
        LwU32 intblk_sts;          // Quick status of internal block processing
        LwU32 intblk_offset;       // BIOS Cert Internal block offset
        LwU32 intblk_size;         // BIOS Cert Internal block size

} OUT_CMD_VV, *POUT_CMD_VV;

// vv command output structure.  It is populated when fwseclic is ilwoked by vv command

#define LW_OUT_CMD_FWSECLIC_VV_VERSION                                        7:0
#define LW_OUT_CMD_FWSECLIC_VV_VERSION_1                                        1

#define LW_OUT_CMD_FWSECLIC_VV_INFO_CERTBLK_VERSION                          15:0
#define LW_OUT_CMD_FWSECLIC_VV_INFO_OUTPUT_VALID                             16:16
#define LW_OUT_CMD_FWSECLIC_VV_INFO_OUTPUT_VALID_NO                             0
#define LW_OUT_CMD_FWSECLIC_VV_INFO_OUTPUT_VALID_YES                            1
#define LW_OUT_CMD_FWSECLIC_VV_INFO_METADATA_PRESENT                         17:17
#define LW_OUT_CMD_FWSECLIC_VV_INFO_METADATA_PRESENT_NO                         0
#define LW_OUT_CMD_FWSECLIC_VV_INFO_METADATA_PRESENT_YES                        1
#define LW_OUT_CMD_FWSECLIC_VV_INFO_CMD                                     31:24     // store the command that generated this output structure
                                                                                      // both vv and sb commands have similar output structures. 
                                                                                      // RM could call any one these two commands to retrieve BIOS Cert and Metadata information

// struct and defines used by RM
#define LW_OUT_CMD_FWSECLIC_OEM_VENDOR_NAME_LENGTH                           (0x40)
typedef struct {
    LwU32   vbiosType;      //  LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_ values
    LwU32   expirationDate; // The expiration date of the VBIOS, seconds elapsed since 1970 Jan 1.
                            // Zero means no expiration date
    LwU32   creationDate;   // The Creation date of the VBIOS, seconds elapsed since 1970 Jan 1.
    LwU8    pVendorName[ LW_OUT_CMD_FWSECLIC_OEM_VENDOR_NAME_LENGTH ]; // The vendor name
} VBIOS_SELWRITY_INFO;


typedef struct {
        LwU32 version;                   // Version and size of this struct   -- Note,  VV command becomes a complex command,  add struct version control
        LwU32 size;                      // size of this struct
        LwU32 info;                      // BIOS Cert basic infomation, like Cert Block Version, if this structure is valid and etc.
        LwU32 status;                    // status of vbios security verification that are not covered by POST Code
        LwU32 bioscertOffset;            // VBIOS image offset of BIOS Cert Block
        VBIOS_SELWRITY_INFO  vbiosSelwrityInfo;    // BIOS Cert Information used by RM for LWAPI, by MBSet or other methods
        BCRT2X_PROJ_ID_S projId;         // metadata
} OUT_CMD_FWSECLIC_VV, *POUT_CMD_FWSECLIC_VV;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_ECID: IN_CMD_ECID */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_ECID: OUT_CMD_ECID   */
/*
 * struct IN_CMD_ECID
 *
 * Input data for cmd_ecid()
 *
 * struct OUT_CMD_ECID
 *
 * Output data for cmd_ecid()
 * The function returns ECID value of processed chip
 * for requested Algo type
 *
 */
#define    LW_IN_CMD_ECID_DESC_ALGO                        15:0
#define    LW_IN_CMD_ECID_DESC_ALGO_SHA2                      1                              // ECID encoded by SHA2
#define    LW_IN_CMD_ECID_DESC_ALGO_DMHASH                    2                              // ECID encoded by dmhash(davies meyer hash)
#define    LW_IN_CMD_ECID_DESC_ALGO_RAW                       3                              // ECID raw data
typedef struct {
        LwU32 ecid_desc_algo;           // ECID algo type,
} IN_CMD_ECID, *PIN_CMD_ECID;

typedef struct {
        char * ecid;           // ECID,  length depends on ecid_desc_algo
} OUT_CMD_ECID, *POUT_CMD_ECID;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_LICVERIFY:   IN_CMD_LICVERIFY  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_LICVERIFY:   NONE        */

#define  LIC_VERIFY_MODE_HULK_VERIFY_LWF_SKIP  0    // used by cert20HULKCheckSkipLwfSigVerify for skipping LWF VBIOS Signature check
#define  LIC_VERIFY_MODE_HULK_VERIFY_ID_ONLY   1    // used by cert20HULKCheckSkipLwfSigVerify for HULK ID check

typedef struct {
        LwU32 lic_size;            // size of certificate/license in lwf_img_data_buf buffer
        LwU32 verify_mode;         // used to specify how the license get verified.
} IN_CMD_LICVERIFY, *PIN_CMD_LICVERIFY;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_BSI_INFO:   None  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_BSI_INFO:   None  */

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_HULKPROC:   None  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_HULKPROC:   OUT_CMD_HULKPROC  */
#define    LW_OUT_CMD_HULKPROC_STRUCT_DESC_VERSION                 7:0                                // Version of this Struc
#define    LW_OUT_CMD_HULKPROC_STRUCT_DESC_VERSION_VER1              1                                // Version 1
#define    LW_OUT_CMD_HULKPROC_STRUCT_DESC_SIZE                   15:8                                // Size of this struct,  in number of bytes
#define    LW_OUT_CMD_HULKPROC_STRUCT_DESC_MAX_REG_NUM           24:16                                // Max number of Register Entries allowed in buffer


#define    LW_OUT_CMD_HULKPROC_ENTRY_DESC_NUM                     7:0                                 // Number of Register entries
#define    LW_OUT_CMD_HULKPROC_ENTRY_DESC_SIZE                   24:16                                // Registers entry struct size
typedef struct {
        LwU32 struct_desc;              // Strlwture Version, size.
        LwU32 entry_desc;               // Entry number, entry struct size
} OUT_CMD_HULKPROC, *POUT_CMD_HULKPROC;

typedef struct {
        LwU32 reg_address;              // Register address
} OUT_CMD_HULKPROC_ENTRY, *POUT_CMD_HULKPROC_ENTRY;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_HULKREQ:     IN_CMD_HULKREQ  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_HULKREQ:     OUT_CMD_HULKREQ */
typedef struct {
    LwU32   hulkType;
    LwU32   toolID;
    LwU32   toolVersion[4];
} IN_CMD_HULKREQ, *PIN_CMD_HULKREQ;

typedef struct {
    PHULK_OC_LICENSE_REQ phulk_oc_license_req;
} OUT_CMD_HULKREQ, *POUT_CMD_HULKREQ;


/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_FRTS:   IN_CMD_FRTS,  IN_CMD_FRTS_SEC  */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_FRTS:   OUT_CMD_FRTS, OUT_CMD_FRTS_SEC  */

/*
    Note:  We have a set of normal Input/Output structures, this is similiar to all other commands.
           In addition,  we have a set of secure Input/Output structure for this command. Both of them are passed through
           BSI secure scratch registers.

           The secure input structure doesn't need to permanently occupy any secure BSI secure scratch registers.
           The calling agant can save/restore these scratch registers before and after calling the command,
           to preserve the content of these registers.

           The secure output structure will permanently occupy a BSI secure scratch register location.
           It will be used as a quick indicator for the status of LW_UCODE_CMD_COMMAND_FRTS command.

           The scratch register location is TBD.
*/

/*  note:  the content of this structure need to stay in secure scratch space
         The secure scratch space could be scratch registers,  BSI area,  or any other media with secure level 2 or above.
         We can ask RM's preference before deciding it.
         TODO:  Once the secure space is decided,  need to update this note with specific information
*/

#define  LW_IN_CMD_FRTS_VER_SIZE_VERSION                    7:0   // Version field of IN_CMD_FRTS
#define  LW_IN_CMD_FRTS_VER_SIZE_VERSION_1                  1     // Version 1
#define  LW_IN_CMD_FRTS_VER_SIZE_SIZE                       15:8  // size of IN_CMD_FRTS
#define  LW_IN_CMD_FRTS_VER_RSVD                            31:16

#define  LW_IN_CMD_FRTS_FLAG_SELWRE_INPUT_ABSENT            0:0   // secure input struct absent, Could be used for command testing
#define  LW_IN_CMD_FRTS_FLAG_SELWRE_INPUT_ABSENT_YES        1
#define  LW_IN_CMD_FRTS_FLAG_SELWRE_INPUT_ABSENT_NO         0
#define  LW_IN_CMD_FRTS_FLAG_KEEP_UEFI_IN_WPR               1:1   // trim out UEFI image or not for the WPR VBIOS copy
#define  LW_IN_CMD_FRTS_FLAG_KEEP_UEFI_IN_WPR_YES           1
#define  LW_IN_CMD_FRTS_FLAG_KEEP_UEFI_IN_WPR_NO            0
// It could be neccessary to use a HULK license for testing cases. This command could copy data to FB,  or sysmem ( in testing case ),
// which having potential to destroy data in these media.
#define  LW_IN_CMD_FRTS_FLAG_HULK_PRESENCE                  1:1   // HULK provided or not, it is relax security for testing
#define  LW_IN_CMD_FRTS_FLAG_HULK_PRESENCE_YES              1
#define  LW_IN_CMD_FRTS_FLAG_HULK_PRESENCE_NO               0

typedef struct {
    LwU32   ver_size;                   // Version and size of this input structure
    LwU32   flag;
    LwU64   sysmem_offset;              // System memory offset of VBIOS image. Used by fwseclic to fetch VBIOS image from sysmem
    LwU32   sysmem_size;                // VBIOS image size in sysmem
    LwU64   sysmem_offset_copy_back;    // System memory offset for VBIOS image copy back.
                                        //   Note:  This VIBOS copy back area in sysmem is for debug.  WPR area is not readable from CPU.
                                        //          VBIOS fwseclic could copy back ~10K of data with VBIOS image descriptor and VDPA tables
                                        //          generated when copy VBIOS data to WPR
    LwU32   sysmem_size_copy_back;      // VBIOS image size in sysmem for copy back
} IN_CMD_FRTS, *PIN_CMD_FRTS;

typedef struct
{
    /* Version of the structure  */
    LwU32 version;

    /* Size of the structure */
    LwU32 size;

    /* VBIOS image address in the media (SYSMEM/FB/EPROM) */
    LwU64 gfwImageOffset;

    /* VBIOS image size */
    LwU32 gfwImageSize;

    /* Storage location of the VBIOS image */
    LwU32 mediaType;
} FWSECLIC_READ_VBIOS_DESC, *PFWSECLIC_READ_VBIOS_DESC;

#define FWSECLIC_READ_VBIOS_STRUCT_VERSION_1                  0x1
#define FWSECLIC_READ_VBIOS_STRUCT_MEDIA_SYSMEM               0x1
#define FWSECLIC_READ_VBIOS_STRUCT_MEDIA_EEPROM               0x2

typedef struct
{
    /* Version of the structure  */
    LwU32 version;

    /* Size of the structure */
    LwU32 size;

    /* FRTS region address in the media (SYSMEM/FB) */
    LwU32 frtsRegionOffset;

    /* FRTS region size */
    LwU32 frtsRegionSize;

    /* Storage location of the FRTS image */
    LwU32 frtsRegionMediaType;
} FWSECLIC_FRTS_REGION_DESC, *PFWSECLIC_FRTS_REGION_DESC;

#define FWSECLIC_FRTS_REGION_DESC_STRUCT_VERSION_1            0x1
#define FWSECLIC_FRTS_REGION_MEDIA_SYSMEM                     0x1
#define FWSECLIC_FRTS_REGION_MEDIA_FB                         0x2
#define FWSECLIC_FRTS_REGION_SIZE_1MB_IN_4K                   0x100

typedef struct
{
    FWSECLIC_READ_VBIOS_DESC readVbiosDesc;
    FWSECLIC_FRTS_REGION_DESC frtsRegionDesc;
} FWSECLIC_FRTS_CMD, *PFWSECLIC_FRTS_CMD;

//
// Note:  This secure input structure for FRTS uses secure scratch registers.
//        The secure scratch registers defines for FRTS are in //sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/volta/gv100/dev_gc6_island_addendum.h
//
#define  LW_IN_CMD_FRTS_SEC_CTRL_VERSION                                3:0   // Version field of IN_CMD_FRTS_SEC
#define  LW_IN_CMD_FRTS_SEC_CTRL_VERSION_1                              1     // Version 1
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE                             7:4   // Media Type field of IN_CMD_FRTS_SEC
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_FB                          0     // FRTS copy VBIOS data back to FB
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_NONCOHERENT          1     // FRTS copy VBIOS data back to _NONCOHERENT_SYSMEM
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_COHERENT             2     // FRTS copy VBIOS data back to _COHERENT_SYSMEM,

#define  LW_IN_CMD_FRTS_SEC_CTRL_WPR_ID                     15:8  // WPR area ID that VBIOS section is located
#define  LW_IN_CMD_FRTS_SEC_CTRL_RSVD                       31:16

#define  LW_IN_CMD_FRTS_SEC_WPR_OFFSET                      31:0  // WPR OFFSET in 4K Bytes
#define  LW_IN_CMD_FRTS_SEC_WPR_SIZE                        16:0  // WPR size in 4K Bytes
typedef struct {
    LwU16   ctrl;           // version,  wpr ID and etc.
    LwU32   wpr_offset;     // Start offset of VBIOS section in 4K bytes
    LwU32   wpr_size;       // WPR size of VBIOS section
} IN_CMD_FRTS_SEC, *PIN_CMD_FRTS_SEC;


#define  LW_OUT_CMD_FRTS_VER_SIZE_VERSION                   7:0   // Version field of OUT_CMD_FRTS
#define  LW_OUT_CMD_FRTS_VER_SIZE_VERSION_1                 1     // Version 1
#define  LW_OUT_CMD_FRTS_VER_SIZE_SIZE                      15:8  // Size of OUT_CMD_FRTS
#define  LW_OUT_CMD_FRTS_VER_RSVD                           31:16

#define  LW_OUT_CMD_FRTS_FLAG_SELWRE_OUTPUT_ABSENT          0:0   // secure output struct absent.
#define  LW_OUT_CMD_FRTS_FLAG_SELWRE_OUTPUT_ABSENT_YES      1
#define  LW_OUT_CMD_FRTS_FLAG_SELWRE_OUTPUT_ABSENT_NO       0
typedef struct {
    LwU32   ver_size;   // version and structure size of the secure input
    LwU32   flag;       // used to indicate if a secure output is present and etc.
} OUT_CMD_FRTS, *POUT_CMD_FRTS;

//
// Note:  This secure output structure for FRTS uses secure scratch registers.
//        The secure scratch registers defines for FRTS are in  //sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/volta/gv100/dev_gc6_island_addendum.h
//
#define  LW_OUT_CMD_FRTS_SEC_FLAG_CMD_SUCCESSFUL            0:0
#define  LW_OUT_CMD_FRTS_SEC_FLAG_CMD_SUCCESSFUL_YES        1
#define  LW_OUT_CMD_FRTS_SEC_FLAG_CMD_SUCCESSFUL_NO         0
#define  LW_OUT_CMD_FRTS_SEC_FLAG_ACR_READY                 1:1
#define  LW_OUT_CMD_FRTS_SEC_FLAG_ACR_READY_YES             1
#define  LW_OUT_CMD_FRTS_SEC_FLAG_ACR_READY_NO              0
typedef struct {
    LwU16   flag;       // fwseclic command, ACR status and etc.
} OUT_CMD_FRTS_SEC, *POUT_CMD_FRTS_SEC;

#define LW_FW_IMAGE_DESC_IDENTIFIER_SIG             0x49574647    // "GFWI"
#define LW_FW_IMAGE_DESC_VERSION_VER                       7:0
#define LW_FW_IMAGE_DESC_VERSION_VER_1                     1
#define LW_FW_IMAGE_DESC_FLAG_COPY_COMPLETE                0:0
#define LW_FW_IMAGE_DESC_FLAG_COPY_COMPLETE_NO             0
#define LW_FW_IMAGE_DESC_FLAG_COPY_COMPLETE_YES            1

// VBIOS_IMAGE_DESCRIPTOR is genrated by LW_UCODE_CMD_COMMAND_FRTS command
// It is placed in the WPR area at the the start of Firmware runtime security section,
// in another word,  its location is at IN_CMD_FRTS.wpr_offset
typedef struct
{
    LwU32   identifier;         // ID string,  with a value "GFWI"
    LwU32   version;            // version for this struct
    LwU32   size;               // size of this struct
    LwU32   flag;               // With indicators to indicate Firmware image copy to WPR has been completed and etc.
    LwU32   vdpa_header_offset; // offset of vdpa table header in WPR Firmware section.
                                // It shouldn't be needed by RM LS uCode. at least not immediately. It is placed here for future proof.
    LwU32   vdpa_entry_offset;  // offset to VDPA table entries in WPR Firmware section that RM LS code can use to find DIRT table.
    LwU32   vdpa_entry_count;   // Total VDPA entry number
    LwU32   vdpa_entry_size;    // VDPA Entry structure size.
                                // See note2
    LwU32   fw_Image_offset;    // offset of Firmware image start in WPR Firmware section
    LwU32   fw_Image_size;      // Firmware image size in WPR Firmware section
} FW_IMAGE_DESCRIPTOR;

/*
    Note1:  VDPA table is a pre-sorted table. VDPA entries are placed back to back in pre-sorted order.
    Note2:  VDPA entry size should be determined by VDPA table entry type. however, for simplicity and verification purpose,
            We also have a entry size field in Firmware Image Descriptor.  Current RM uCode should only need one type of
            VDPA entry to find Firmware tables. It should be safe to use this field to stride through VDPA entries.
*/

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_OTP_READ:    IN_CMD_OTPRD */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_OTP_READ:    NONE         */
/*
 * struct IN_CMD_OTPRD
 *
 * Input data for cmd_otp_read()
 * The function read data from a specified EEPOROM OTP Region
 *
 */
typedef struct {
        LwU32 startAddress;
        LwU32 bytes;
        LwU32 otp_region;   // OTP region number
} IN_CMD_OTPRD, *PIN_CMD_OTPRD;

/* INPUT STRUCT:  LW_UCODE_CMD_COMMAND_SELWRE_BOOT: NONE                        */
/* OUTPUT STRUCT: LW_UCODE_CMD_COMMAND_SELWRE_BOOT: OUT_CMD_FWSECLIC_SB         */

/*
 * struct OUT_CMD_FWSECLIC_SB
 *
 * Output data for secure boot command. 
 *    Note:  At least the upper part of the structure need to be the same as struct OUT_CMD_FWSECLIC_VV
 *           RM could use SB or VV command to retrieve VBIOS Cert and Metadata information.
 * The function read data from a specified EEPOROM OTP Region
 *
 */
 
#define LW_OUT_CMD_FWSECLIC_SB_VERSION                                        7:0
#define LW_OUT_CMD_FWSECLIC_SB_VERSION_1                                        1

#define LW_OUT_CMD_FWSECLIC_SB_INFO_CERTBLK_VERSION                          15:0
#define LW_OUT_CMD_FWSECLIC_SB_INFO_OUTPUT_VALID                             16:16
#define LW_OUT_CMD_FWSECLIC_SB_INFO_OUTPUT_VALID_NO                             0
#define LW_OUT_CMD_FWSECLIC_SB_INFO_OUTPUT_VALID_YES                            1
#define LW_OUT_CMD_FWSECLIC_SB_INFO_METADATA_PRESENT                         17:17
#define LW_OUT_CMD_FWSECLIC_SB_INFO_METADATA_PRESENT_NO                         0
#define LW_OUT_CMD_FWSECLIC_SB_INFO_METADATA_PRESENT_YES                        1
#define LW_OUT_CMD_FWSECLIC_SB_INFO_CMD                                     31:24     // store the command that generated this output structure
                                                                                      // both vv and sb commands have similar output structures. 
                                                                                      // RM could use any one these two commands to retrieve BIOS Cert and Metadata information

typedef struct {
        LwU32 version;                   // Version and size of this struct
        LwU32 size;                      // size of this struct
        LwU32 info;                      // command and BIOS Cert basic infomation, like calling command,  Cert Block Version, if this structure is valid and etc.
        LwU32 status;                    // status of vbios security verification that are not covered by POST Code
        LwU32 bioscertOffset;            // VBIOS image offset of BIOS Cert Block
        VBIOS_SELWRITY_INFO  vbiosSelwrityInfo;    // BIOS Cert Information used by RM for LWAPI, by MBSet or other methods
        BCRT2X_PROJ_ID_S projId;         // metadata
} OUT_CMD_FWSECLIC_SB, *POUT_CMD_FWSECLIC_SB;

// Use PMU_MAILBOX(i) (i=0~11) for image block data feeding
#define LW_UCODE_PUSHPULL_DATA_SCRATCH                      LW_PPWR_PMU_MAILBOX
#define LW_UCODE_PUSHPULL_DATA_SCRATCH_COUNT                10

#define LW_UCODE_PUSHPULL_CTL0                              LW_PPWR_PMU_MAILBOX(11)
#define LW_UCODE_PUSHPULL_CTL0_OFFSET                       19:0
#define LW_UCODE_PUSHPULL_CTL0_OFFSET_INIT                  0x00000000
#define LW_UCODE_PUSHPULL_CTL0_SIZE                         27:20
#define LW_UCODE_PUSHPULL_CTL0_SIZE_INIT                    0x00000000
#define LW_UCODE_PUSHPULL_CTL0_IS_IFR_OFFSET                28:28
#define LW_UCODE_PUSHPULL_CTL0_IS_IFR_OFFSET_INIT           0x00000000
#define LW_UCODE_PUSHPULL_CTL0_IS_IFR_OFFSET_TRUE           0x00000001  // Data offset into IFR image
#define LW_UCODE_PUSHPULL_CTL0_IS_IFR_OFFSET_FALSE          0x00000000  // Data offset into normal rom image
#define LW_UCODE_PUSHPULL_CTL0_STATE                        31:29       // Push Pull cycle status: 0x00 ~ 0x07
#define LW_UCODE_PUSHPULL_CTL0_STATE_INIT                   0x00000000  // Initial state
#define LW_UCODE_PUSHPULL_CTL0_STATE_UCODE_REQ              0x00000001  // uCode initialize a push pUll cycle  -- uCode set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_LWF_DLV_PENDING        0x00000002  // LWF Data Delivery pending           -- Lwflash set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_LWF_DLV_COMPLETE       0x00000003  // LWF Complete Copy Data to Scratches -- Lwflash set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_UCODE_RCV_PENDING      0x00000004  // UCODE Service pending               -- uCode set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_UCODE_RCV_COMPLETE     0x00000005  // UCODE Service Complete              -- uCode set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_COMPLETE               0x00000006  // All Push Pull Cycles completed. LWF can complete verify VBIOS task     -- uCode set this state
#define LW_UCODE_PUSHPULL_CTL0_STATE_DEVINIT_INFO_REQ       0x00000007  // UCode Requests for Devinit offsets and size

#define SELWRE_FLASH_FALCON_DMEM_LOAD_SIZE                  8192        // LWF_IMG_DATA_BUF_LEN = 8192

// UCODE IDs
#define  FALC_UCODE_APP_ID_NONE                  0x00
#define  FALC_UCODE_APP_ID_PREOS_APP             0x01
#define  FALC_UCODE_APP_ID_DEVINIT_ENGINE_GC6    0x02
#define  FALC_UCODE_APP_ID_DEVINIT_COMPACT       0x03
#define  FALC_UCODE_APP_ID_DEVINIT_ENGINE_PRI    0x04
#define  FALC_UCODE_APP_ID_FW_SEC_LIC            0x05
#define  FALC_UCODE_APP_ID_LWFLASH               0x06
#define  FALC_UCODE_APP_ID_FB_INIT               0x07
#define  FALC_UCODE_APP_ID_LS_UDE                0x08
#define  FALC_UCODE_APP_ID_HULK                  0x09
#define  FALC_UCODE_APP_ID_UNITTEST              0xF0

// Lwflash data buffer and VDPA data block size
#define  LW_LWF_IMAGE_DATA_BUFFER_SIZE           SELWRE_FLASH_FALCON_DMEM_LOAD_SIZE    // also used for VDPA data block size.
#define  LW_LWF_VDPA_DATA_BUFFER_SIZE           4452    // used for VDPA table,  include header, and signature

#define  LW_UCODE_GP_BUFFER_SIZE_16             16      // 16   bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_32             32      // 32   bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_64             64      // 64   bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_128            128     // 128  bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_256            256     // 256  bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_512            512     // 512  bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_1024           1024    // 1024 bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_1536           1536    // 1536 bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_2048           2048    // 2048 bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_3072           3072    // 3072 bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_4096           4096    // 4096 bytes general purpose data buffer
#define  LW_UCODE_GP_BUFFER_SIZE_8192           8192    // 8192 bytes general purpose data buffer


// MAX DevIDs in DevID Override Table
#define  LW_LWF_DEVID_MATCH_TABLE_MAX_NUM        64      // Max number of DevIDs in DevID Override Table

#define  LW_LWF_DEVID_MATCH_TABLE_LIST_COUNT     7:0     // bits used in devidOverrideListCount in VBIOS devid override table.

#define ASSERT_OK_OR_EXIT( function ) \
    do \
    { \
        errCode = function; \
        if ( errCode != LW_UCODE_ERR_CODE_NOERROR ) \
        { \
            goto exitPoint; \
        } \
    } while (LW_FALSE)

#define ASSERT_OK_OR_EXIT_FWSECLIC_FN( function, footnote ) \
    do \
    { \
        errCode = function; \
        if ( errCode != LW_UCODE_ERR_CODE_NOERROR ) \
        { \
            fwseclic_footnote_code = footnote; \
            goto exitPoint; \
        } \
    } while (LW_FALSE)

#define EXIT_POINT_ASSERT_OK_OR_REPORT_ERROR( function ) \
    do \
    { \
        errCodeExitPoint = function; \
        if ( ( errCodeExitPoint != LW_UCODE_ERR_CODE_NOERROR ) && ( errCode == LW_UCODE_ERR_CODE_NOERROR ) ) \
        { \
            errCode = errCodeExitPoint; \
        } \
    } while (LW_FALSE)
#endif
