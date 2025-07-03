/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    soe_eraseledger.h
 * @brief   Contains Erase ledger specific defines.
 */

#ifndef SOE_ERASELEDGER_H
#define SOE_ERASELEDGER_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define LW_ERASE_LEDGER_HEADER_SIZE                (16)
#define LW_ERASE_LEDGER_SECTOR_HEADER_SIZE         (4)
#define LW_ERASE_LEDGER_DATA_SECTOR_HEADER_SIZE    (4)
#define LW_ERASE_LEDGER_SESSION_HEADER_SIZE        (4)
#define LW_ERASE_LEDGER_IMAGE_SIG_SIZE             (8)

/*!
 * Colwenience macro to iterate over all EEPROM Sector data structures.
 *
 * @param[in]   _pC     Container object pointer.
 * @param[out]  _pS     Iterating pointer (must be L-value).
 *
 * @note    Do not alter list (add/remove elements)  while iterating.
 */
#define FOR_EACH_SECTOR(_pC,_pS) \
    for(_pS = (_pC)->pSectorHead; _pS != NULL; _pS = _pS->pNext)

#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_COUNT                   23:0
#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_STATUS                 27:24
#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_STATUS_ACTIVE            0x7
#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_STATUS_RETIRED           0x6
#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_RSVD                   31:28
#define LW_ERASE_LEDGER_SESSION_HEADER_ERASE_LIMIT_RSVD_VALUE               0xF

#define LW_ERASE_LEDGER_DATA_SECTOR_HEADER_INFO0_LIFETIME_ERASE_COUNT      23:0
#define LW_ERASE_LEDGER_DATA_SECTOR_HEADER_INFO0_RSVD                     31:24
#define LW_ERASE_LEDGER_DATA_SECTOR_HEADER_INFO0_RSVD_VALUE                0xFF

#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_FIRST              11:0
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_LAST              23:12
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_RSVD              30:24
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_RSVD_VALUE         0x7F
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_VALID             31:31
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_VALID_YES             1
#define LW_ERASE_LEDGER_SECTOR_HEADER_DATA_SECTOR_RANGE_VALID_NO              0

/* ------------------------- Types Definitions ----------------------------- */
typedef struct ERASE_LEDGER_REGION ERASE_LEDGER_REGION;
typedef struct ERASE_LEDGER_REGION *PERASE_LEDGER_REGION;
typedef struct ERASE_LEDGER_SESSION_HEADER ERASE_LEDGER_SESSION_HEADER;
typedef struct ERASE_LEDGER_SESSION_HEADER *PERASE_LEDGER_SESSION_HEADER;
typedef struct ERASE_LEDGER_SESSION ERASE_LEDGER_SESSION;
typedef struct ERASE_LEDGER_SESSION *PERASE_LEDGER_SESSION;
typedef struct ERASE_LEDGER_DATA_SECTOR ERASE_LEDGER_DATA_SECTOR;
typedef struct ERASE_LEDGER_DATA_SECTOR *PERASE_LEDGER_DATA_SECTOR;
typedef struct ERASE_LEDGER_DATA_SECTOR_HEADER ERASE_LEDGER_DATA_SECTOR_HEADER;
typedef struct ERASE_LEDGER_DATA_SECTOR_HEADER *PERASE_LEDGER_DATA_SECTOR_HEADER;
typedef struct ERASE_LEDGER_SECTOR_HEADER ERASE_LEDGER_SECTOR_HEADER;
typedef struct ERASE_LEDGER_SECTOR_HEADER *PERASE_LEDGER_SECTOR_HEADER;
typedef struct ERASE_LEDGER_SECTOR ERASE_LEDGER_SECTOR;
typedef struct ERASE_LEDGER_SECTOR *PERASE_LEDGER_SECTOR;
typedef struct ERASE_LEDGER_HEADER ERASE_LEDGER_HEADER;
typedef struct ERASE_LEDGER_HEADER *PERASE_LEDGER_HEADER;
typedef struct ERASE_LEDGER ERASE_LEDGER;
typedef struct ERASE_LEDGER *PERASE_LEDGER;

/*!
 * Erase Ledger is a writeable region in EEPROM like InfoROM region.
 */
struct ERASE_LEDGER_REGION
{
    LwU32    startAddress;
    LwU32    endAddress;
};

/*!
 * Header to @ref ERASE_LEDGER_SESSION.
 */
struct ERASE_LEDGER_SESSION_HEADER
{
    /*!
     * Erase ledger Session Erase Limit.
     */
    LwU32    eraseLimit;
};


/*!
 * An Erase Ledger Session corresponds to a VM Session in passthrough or
 * a baremetal instance. CSP must create such a session using flash tools
 * while launching VM.  An erase limit is set for a session when it is created.
 * A ledger session is created for all tracked data sectors in EEPROM simultaneously.
 */
struct ERASE_LEDGER_SESSION
{
    /*!
     * Data Sector Header contains lifetime erase count
     * of the tracked EEPROM data sector and reserved bits.
     */
    ERASE_LEDGER_SESSION_HEADER    header;

    /*!
     * Active session start offset within EEPROM.
     */
    LwU32    startOffset;

    /*!
     * Active session end offset within EEPROM.
     */
    LwU32    endOffset;

    /*!
     * Tally bit offset within active session.
     */
    LwU32    tallyBitOffset;
};

/*!
 * Header to @ref ERASE_LEDGER_DATA_SECTOR.
 */
struct ERASE_LEDGER_DATA_SECTOR_HEADER
{
    LwU32   lifetimeEraseCount;
};

/*!
 * This structure refers to the EEPROM data sectors tracked by Erase Ledger.
 * Erase ledger limits the erases on data sectors.
 */
struct ERASE_LEDGER_DATA_SECTOR
{
    /*!
     * Data Sector Header contains lifetime erase count
     * of the tracked EEPROM data sector and reserved bits.
     */
    ERASE_LEDGER_DATA_SECTOR_HEADER    header;

    /*!
     * Erase Ledger Data Sector start offset within EEPROM.
     */
    LwU32    dataSectorOffset;

    /*!
     * Lwrrently active session in this data sector.
     */
    ERASE_LEDGER_SESSION    session;

    /*!
     * EEPROM data Sector ledger structures are linked into a singly linked list
     * so that they can be iterated over using @ref FOR_EACH_SECTOR() macro.
     */
    PERASE_LEDGER_DATA_SECTOR    pNext;
};

/*!
 * The header to sectors of Erase ledger Image tracks the state of sector.
 */
struct ERASE_LEDGER_SECTOR_HEADER
{
    /*!
     * First EEPROM Data Sector tracked by this Ledger Sector.
     */
    LwU16     dataSectorRangeFirst;

    /*!
     * Last EEPROM data Sector tracked by this Ledger Sector.
     */
    LwU16     dataSectorRangeLast;

    /*!
     * Whether the ledger sector is valid or not.
     */
    LwBool    bValid;

    /*!
     * Whether the ledger sector is in use or not.
     */
    LwBool    bInUse;
};

/*!
 * This structure refers to the sectors of Erase ledger Image itself.
 * It holds all data and state related to Erase Ledger Sector.
 */
struct ERASE_LEDGER_SECTOR
{
    /*!
     * Erase Ledger Sector start offset within Erase ledger Image.
     */
    LwU32    ledgerSectorOffset;

    /*!
     * @ref ERASE_LEDGER_SECTOR_HEADER
     */
    ERASE_LEDGER_SECTOR_HEADER    header;

    /*!
     * Head pointer to the singly linked list of EEPROM data sector structures.
     * Used by @ref FOR_EACH_SECTOR() iterating macro.
     */
    PERASE_LEDGER_DATA_SECTOR    pSectorHead;

    /*!
     * Erase Ledger Sector data structures are linked into a singly linked list so t
     * they can be iterated over using @ref FOR_EACH_SECTOR() macro.
     */
    PERASE_LEDGER_SECTOR    pNext;
};

/*!
 * Erase ledger image header. Erase Ledger image resides in its own
 * NBSI glob in EEPROM and it must precede with this header.
 */
struct ERASE_LEDGER_HEADER
{
    /*!
     * Unique Erase Ledger Image signature.
     */
    LwU8    signature[LW_ERASE_LEDGER_IMAGE_SIG_SIZE];

    /*!
     * Erase Ledger Image Version.
     */
    LwU32    version;

    /*!
     * Size of Erase Ledger Image in 4KB sectors.
     */
    LwU8     ledgerImageSize;

    /*!
     * Number of EEPROM Data Sectors tracked per Erase Ledger Sector.
     */
    LwU8     ledgersPerSector;
};

/*!
 * Erase ledger tracks and limits sector erases for each EEPROM data sector.
 * This structure contains all of the Erase Ledger state.
 */
struct ERASE_LEDGER
{
    /*!
     * @ref ERASE_LEDGER_HEADER
     */
    ERASE_LEDGER_HEADER header;

    /*!
     * Head pointer to the singly linked list of sectors of erase ledger image.
     * Used by @ref FOR_EACH_SECTOR() iterating macro.
     */
    PERASE_LEDGER_SECTOR  pSectorHead;
};

/* ------------------------ External Definitions --------------------------- */
extern ERASE_LEDGER eraseLedger;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
#endif // SOE_ERASELEDGER_H

