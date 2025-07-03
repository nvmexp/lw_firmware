/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ Application Includes --------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objspi.h"
#include "soe_objsaw.h"
#include "config/g_spi_hal.h"
#include "dev_lwlsaw_ip.h"
#include "dev_lwlsaw_ip_addendum.h"

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA ERASE_LEDGER eraseLedger;

/*!
 * Find tally bit offset within an active ledger session.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 * @param[in]   sessionStartOffset     Session start byte offset within EEPROM.
 * @param[in]   sessionEndOffset       Session end byte offset within EEPROM.
 * @param[out]  pTallyBitOffset        Tally Bit Offset within current session.
 *
 * @return FLCN_OK                     If tally bit offset was found.
 * @return other                       Propagates return values from EEPROM Read calls.
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomFindTallyBitOffset
(
    PSPI_DEVICE_ROM              pSpiRom,
    LwU32                        sessionStartOffset,
    LwU32                        sessionEndOffset,
    LwU32                       *pTallyBitOffset
)
{
    LwU32          dword;
    LwU32          tallyOffset = 0x0;
    FLCN_STATUS    status      = FLCN_ERR_ILWALID_STATE;

    // Read Dwords from lwrrently active session to find tally offset.
    while (sessionStartOffset <= sessionEndOffset)
    {
        status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, sessionStartOffset,
                     sizeof(LwU32), (LwU8 *)&dword);
        if (status != FLCN_OK)
        {
            goto s_spiRomFindTallyBitOffset_exit;
        }

        if (dword == 0)
        {
            tallyOffset += 32;
        }
        else
        {
            LOWESTBITIDX_32(dword);
            tallyOffset += dword;
            break;
        }

        sessionStartOffset += sizeof(dword);
    }

    *pTallyBitOffset = tallyOffset;

s_spiRomFindTallyBitOffset_exit:
    return status;
}

/*!
 * Allocate and initialize lwrrently active ledger session within Data Sector.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 * @param[in]   pLedgerSector          ERASE_LEDGER_SECTOR pointer.
 * @param[in]   pSector                ERASE_LEDGER_DATA_SECTOR pointer.
 * @param[in]   dataSectorSize         Size of data sector in bytes.
 *
 * @return FLCN_OK                     If Data Sectors were initialized
 *                                     successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT   If argument is an invalid pointer
 * @return FLCN_ERR_NO_FREE_MEM        If there is no free memory.
 * @return other                       Propagates return values from various calls
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomInitLedgerSession
(
    PSPI_DEVICE_ROM              pSpiRom,
    PERASE_LEDGER_SECTOR         pLedgerSector,
    PERASE_LEDGER_DATA_SECTOR    pSector,
    LwU32                        dataSectorSize
)
{
    LwU32          dword;
    LwU32          startOffset;
    LwU32          endOffset;
    LwU32          tallyBitOffset;
    FLCN_STATUS    status           = FLCN_ERR_ILWALID_STATE;

    // Sanity checks
    if ((pSector == NULL) || (pSector->dataSectorOffset == 0))
    {
        SOE_HALT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomInitLedgerSession_exit;
    }

    startOffset = pSector->dataSectorOffset +
                  LW_ERASE_LEDGER_DATA_SECTOR_HEADER_SIZE;
    endOffset   = pSector->dataSectorOffset + dataSectorSize - 1;

    //
    // Initialize Session parameters to default values if
    // the ledger sector is not valid or not in use.
    //
    if (!(pLedgerSector->header.bValid) || !(pLedgerSector->header.bInUse))
    {
        pSector->session.startOffset    = startOffset +
                                          LW_ERASE_LEDGER_SESSION_HEADER_SIZE;
        pSector->session.endOffset      = endOffset;
        pSector->session.tallyBitOffset = 0x0;
        status                          = FLCN_OK;
        goto s_spiRomInitLedgerSession_exit;
    }

    // Read EEPROM and find lwrrently active session.
    while (startOffset <= endOffset)
    {
        status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, startOffset,
                     sizeof(LwU32), (LwU8 *)&dword);
        if (status != FLCN_OK)
        {
            goto s_spiRomInitLedgerSession_exit;
        }

        if (FLD_TEST_DRF(_ERASE_LEDGER, _SESSION_HEADER_ERASE_LIMIT, _STATUS, _RETIRED, dword))
        {
            status = s_spiRomFindTallyBitOffset (pSpiRom,
                          (startOffset + LW_ERASE_LEDGER_SESSION_HEADER_SIZE),
                          endOffset, &tallyBitOffset);
            if (status != FLCN_OK)
            {
                goto s_spiRomInitLedgerSession_exit;
            }

            // Update lifetime erase count by the erases tallied in retired session.
            pSector->header.lifetimeEraseCount += tallyBitOffset;
        }
        else if (FLD_TEST_DRF(_ERASE_LEDGER, _SESSION_HEADER_ERASE_LIMIT, _STATUS, _ACTIVE, dword))
        {
            pSector->session.header.eraseLimit = DRF_VAL(_ERASE_LEDGER,
                                                         _SESSION_HEADER_ERASE_LIMIT,
                                                         _COUNT, dword);
            pSector->session.startOffset = startOffset +
                                           LW_ERASE_LEDGER_SESSION_HEADER_SIZE;
            pSector->session.endOffset   = endOffset;
            status = s_spiRomFindTallyBitOffset (pSpiRom, pSector->session.startOffset,
                          pSector->session.endOffset, &(pSector->session.tallyBitOffset));
            if (status != FLCN_OK)
            {
                goto s_spiRomInitLedgerSession_exit;
            }
            break;
        }

        startOffset += sizeof(dword);
    }

s_spiRomInitLedgerSession_exit:
    return status;
}

/*!
 * Allocate and initialize all tracked EEPROM Data Sector by a Ledger Sector.
 *
 * This function creates a linked list for all the data sectors tracked by
 * a Ledger sector and initializes it from EEPROM.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 * @param[in]   pLedgerSector          ERASE_LEDGER_SECTOR pointer.
 *
 * @return FLCN_OK                     If Data Sectors were initialized
 *                                     successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT   If argument is an invalid pointer
 * @return FLCN_ERR_NO_FREE_MEM        If there is no free memory.
 * @return other                       Propagates return values from various calls
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomInitLedgerDataSectors
(
    PSPI_DEVICE_ROM         pSpiRom,
    PERASE_LEDGER_SECTOR    pLedgerSector
)
{
    PERASE_LEDGER                pEraseLedger = &eraseLedger;
    PERASE_LEDGER_DATA_SECTOR    pSector = NULL;
    PERASE_LEDGER_DATA_SECTOR    pPrev   = NULL;
    LwU32                        ledgerSectorOffset;
    LwU32                        dataSectorSize;
    LwU32                        header;
    LwU8                         i;
    FLCN_STATUS                  status = FLCN_OK;

    // Sanity checks
    if ((pLedgerSector == NULL) || (pLedgerSector->ledgerSectorOffset == 0))
    {
        SOE_HALT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomInitLedgerDataSectors_exit;
    }

    ledgerSectorOffset = pLedgerSector->ledgerSectorOffset;
    dataSectorSize = ((LW_ROM_SECTOR_SIZE - LW_ERASE_LEDGER_SECTOR_HEADER_SIZE) /
                     (pEraseLedger->header.ledgersPerSector));

    dataSectorSize = LW_ALIGN_DOWN(dataSectorSize, sizeof(LwU32));

    // Create a linked list of all the Erase ledger image sectors based on size.
    for (i = 0; i < (pEraseLedger->header.ledgersPerSector); i++)
    {
        // Allocate node for Data Sector, ...
        pSector = lwosCallocType(0, 1, ERASE_LEDGER_DATA_SECTOR);
        if (pSector == NULL)
        {
            SOE_HALT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_spiRomInitLedgerDataSectors_exit;
        }

        //
        // ... initialize it, ...
        //
        pSector->dataSectorOffset = ledgerSectorOffset +
                                    LW_ERASE_LEDGER_SECTOR_HEADER_SIZE +
                                    (i * dataSectorSize);
        pSector->pNext = NULL;

        // Read EEPROM and initialize @ref ERASE_LEDGER_DATA_SECTOR_HEADER
        status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, pSector->dataSectorOffset,
                     LW_ERASE_LEDGER_DATA_SECTOR_HEADER_SIZE, (LwU8 *)&header);
        if (status != FLCN_OK)
        {
            goto s_spiRomInitLedgerDataSectors_exit;
        }

        pSector->header.lifetimeEraseCount = DRF_VAL(_ERASE_LEDGER,
                                                     _DATA_SECTOR_HEADER_INFO0,
                                                     _LIFETIME_ERASE_COUNT,
                                                     header);

        // If uninitialized then lifetimeEraseCount will be all 0xff in EEPROM.
        if ((pSector->header.lifetimeEraseCount) == 0xffffff)
        {
            pSector->header.lifetimeEraseCount = 0;
        }

        // Initialize lwrrently active ledger session for this data sector.
        status = s_spiRomInitLedgerSession(pSpiRom, pLedgerSector, pSector, dataSectorSize);
        if (status != FLCN_OK)
        {
            goto s_spiRomInitLedgerDataSectors_exit;
        }

        // Insert node into the linked list.
        if (NULL == pLedgerSector->pSectorHead)
        {
            pLedgerSector->pSectorHead = pSector;
        }
        else
        {
            pPrev->pNext = pSector;
        }

        pPrev = pSector;
        pSector = pSector->pNext;
    }

s_spiRomInitLedgerDataSectors_exit:
    return status;
}

/*!
 * Allocate and initialize Erase Ledger Image Sectors.
 *
 * This function creates a linked list for all the sectors present in Erase
 * Ledger Image and initializes it by reading ledger image in EEPROM.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 *
 * @return FLCN_OK                     If Erase Ledger Sectors were initialized
 *                                     successfully.
 * @return FLCN_ERR_NO_FREE_MEM        If there is no free memory.
 * @return FLCN_ERR_ILWALID_STATE      If the Erase Ledger sector are not 4KB
 *                                     aligned in EEPROM.
 * @return other                       Propagates return values from various calls
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomInitLedgerSectors
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    PERASE_LEDGER           pEraseLedger = &eraseLedger;
    PERASE_LEDGER_SECTOR    pSector = NULL;
    PERASE_LEDGER_SECTOR    pPrev   = NULL;
    LwU32                   ledgerImageOffset = pSpiRom->pLedgerRegion->startAddress;
    LwU32                   sectorHeader = 0x0;
    LwU8                    i;
    FLCN_STATUS             status = FLCN_OK;

    // Create a linked list of all the Erase ledger image sectors based on size.
    for (i = 0; i < (pEraseLedger->header.ledgerImageSize); i++)
    {
        // Allocate node for Erase Ledger Sector, ...
        pSector = lwosCallocType(0, 1, ERASE_LEDGER_SECTOR);
        if (pSector == NULL)
        {
            SOE_HALT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_spiRomInitLedgerSectors_exit;
        }

        //
        // ... initialize it, ...
        //
        pSector->ledgerSectorOffset = ledgerImageOffset + (i * LW_ROM_SECTOR_SIZE);
        pSector->pNext = NULL;

        // Check for Ledger sector alignment
        if (!LW_IS_ALIGNED(pSector->ledgerSectorOffset, LW_ROM_IMAGE_ALIGN))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_spiRomInitLedgerSectors_exit;
        }

        // Read EEPROM and initialize @ref ERASE_LEDGER_SECTOR_HEADER.
        status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, pSector->ledgerSectorOffset,
                     sizeof(LwU32), (LwU8 *)&(sectorHeader));
        if (status != FLCN_OK)
        {
            goto s_spiRomInitLedgerSectors_exit;
        }
        else
        {
            pSector->header.dataSectorRangeFirst = DRF_VAL(_ERASE_LEDGER,
                                                           _SECTOR_HEADER_DATA_SECTOR_RANGE,
                                                           _FIRST, sectorHeader);
            pSector->header.dataSectorRangeLast = DRF_VAL(_ERASE_LEDGER,
                                                          _SECTOR_HEADER_DATA_SECTOR_RANGE,
                                                          _LAST, sectorHeader);
            pSector->header.bValid = FLD_TEST_DRF(_ERASE_LEDGER,
                                                  _SECTOR_HEADER_DATA_SECTOR_RANGE,
                                                  _VALID, _YES, sectorHeader);

            // Determine whether this sector is in use or not.
            if (!(FLD_TEST_DRF_NUM(_ERASE_LEDGER, _SECTOR_HEADER_DATA_SECTOR_RANGE,
                                   _FIRST, 0xfff, sectorHeader)) &&
                 !(FLD_TEST_DRF_NUM(_ERASE_LEDGER, _SECTOR_HEADER_DATA_SECTOR_RANGE,
                                    _LAST, 0xfff, sectorHeader)))
            {
                pSector->header.bInUse = LW_TRUE;
            }
        }

        // Allocate and initialize tracked data sectors in this ledger sector.
        status = s_spiRomInitLedgerDataSectors(pSpiRom, pSector);
        if (status != FLCN_OK)
        {
            goto s_spiRomInitLedgerSectors_exit;
        }

        // Insert node into the linked list.
        if (NULL == pEraseLedger->pSectorHead)
        {
            pEraseLedger->pSectorHead = pSector;
        }
        else
        {
            pPrev->pNext = pSector;
        }

        pPrev   = pSector;
        pSector = pSector->pNext;
    }

s_spiRomInitLedgerSectors_exit:
    return status;
}

/*!
 * Get Ledger sector which tracks given EEPROM data sector address.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 * @param[in]   eraseAddress           Tracked Data Sector Address in EEPROM.
 * @param[out]  ppLedgerSector         PERASE_LEDGER_SECTOR pointer.
 *
 * @return FLCN_OK                     If data sector was found.
 * @return other                       Propagates return values from other calls.
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomGetLedgerSector
(
    PSPI_DEVICE_ROM         pSpiRom,
    LwU32                   eraseAddress,
    PERASE_LEDGER_SECTOR   *ppLedgerSector
)
{
    PERASE_LEDGER_SECTOR    pLedgerSector;
    PERASE_LEDGER           pEL = &eraseLedger;
    LwU32                   dataSectorFirstOffset;
    LwU32                   dataSectorLastOffset;
    FLCN_STATUS             status = FLCN_ERR_ILWALID_STATE;

    // Sanity Checks.
    if ((pSpiRom == NULL) ||
        (!LW_ROM_REGION_INITIALIZED(pSpiRom->pLedgerRegion)))
    {
        SOE_HALT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomGetLedgerSector_exit;
    }

    // Check for Ledger sector alignment
    if (!LW_IS_ALIGNED(eraseAddress, LW_ROM_IMAGE_ALIGN))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomGetLedgerSector_exit;
    }

    // Find the ledger sector tracking this data sector.
    FOR_EACH_SECTOR(pEL, pLedgerSector)
    {
        if (!(pLedgerSector->header.bValid) || !(pLedgerSector->header.bInUse))
        {
            continue;
        }

        dataSectorFirstOffset = pLedgerSector->header.dataSectorRangeFirst << 12;
        dataSectorLastOffset  = pLedgerSector->header.dataSectorRangeLast << 12;

        // Check whether given sector is tracked by this ledger sector or not.
        if ((eraseAddress >= dataSectorFirstOffset) &&
            (eraseAddress <= dataSectorLastOffset))
        {
            break;
        }
    }

    *ppLedgerSector = pLedgerSector;

s_spiRomGetLedgerSector_exit:
    return status;
}

/*!
 * Copy all tracked data sectors from one ledger sector to other.
 *
 * @param[in]   pSpiRom                SPI_DEVICE_ROM pointer.
 * @param[in]   ppSrc                  ERASE_LEDGER_SECTOR pointer.
 * @param[in]   ppDest                 ERASE_LEDGER_SECTOR pointer.
 * @param[in]   eraseAddress           Tracked Data Sector Address in EEPROM.
 *
 * @return FLCN_OK                     If source ledger sector contents were copied
 *                                     and written successfully to other sector.
 * @return other                       Propagates return values from other calls.
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomCopyLedgerSector
(
    PSPI_DEVICE_ROM         pSpiRom,
    PERASE_LEDGER_SECTOR    pSrc,
    PERASE_LEDGER_SECTOR    pDest,
    LwU32                   eraseAddress
)
{
    LwU32                     sectorHeader;
    LwU32                     dataSectorHeader;
    LwU32                     sessionHeader;
    FLCN_STATUS               status = FLCN_ERR_ILWALID_STATE;
    LwU32                     lifetimeEraseCount;
    LwU32                     dataSectorAddr;
    PERASE_LEDGER_DATA_SECTOR pDestDataSector;
    PERASE_LEDGER_DATA_SECTOR pDataSector;

    // Copy Ledger Sector headers.
    memcpy (&(pSrc->header), &(pDest->header), sizeof(ERASE_LEDGER_SECTOR_HEADER));

    // Read @ref ERASE_LEDGER_SECTOR_HEADER.
    status = spiRomReadDirect_HAL(&Spi, pSpiRom, pSrc->ledgerSectorOffset,
                 LW_ERASE_LEDGER_SECTOR_HEADER_SIZE, (LwU8 *)&(sectorHeader));
    if (status != FLCN_OK)
    {
        goto s_spiRomCopyLedgerSector_exit;
    }

    // Write back packed header info to EEPROM
    status = spiRomWritePage(pSpiRom, &sectorHeader, pDest->ledgerSectorOffset, sizeof(LwU32));
    if (status != FLCN_OK)
    {
        goto s_spiRomCopyLedgerSector_exit;
    }

    // Mark source ledger sector as invalid.
    sectorHeader = FLD_SET_DRF(_ERASE_LEDGER, _SECTOR_HEADER, _DATA_SECTOR_RANGE_VALID,
                               _NO, sectorHeader);
    status = spiRomWritePage(pSpiRom, &sectorHeader, pSrc->ledgerSectorOffset, sizeof(LwU32));
    if (status != FLCN_OK)
    {
        goto s_spiRomCopyLedgerSector_exit;
    }

    // Parse through all tracked data sectors in source sector and update pDestination sector.
    dataSectorAddr = pSrc->header.dataSectorRangeFirst << 12;
    pDestDataSector = pDest->pSectorHead;

    FOR_EACH_SECTOR(pSrc, pDataSector)
    {
        if (dataSectorAddr == eraseAddress)
        {
            pDataSector->header.lifetimeEraseCount++;
        }
        dataSectorAddr += LW_ROM_SECTOR_SIZE;

        lifetimeEraseCount = pDataSector->header.lifetimeEraseCount +
                             pDataSector->session.tallyBitOffset;

        // Read EEPROM and update @ref ERASE_LEDGER_DATA_SECTOR_HEADER
        status = spiRomReadDirect_HAL(&Spi, pSpiRom, pDataSector->dataSectorOffset,
                     LW_ERASE_LEDGER_DATA_SECTOR_HEADER_SIZE, (LwU8 *)&dataSectorHeader);
        if (status != FLCN_OK)
        {
            goto s_spiRomCopyLedgerSector_exit;
        }
        dataSectorHeader = FLD_SET_DRF_NUM(_ERASE_LEDGER, _DATA_SECTOR_HEADER_INFO0,
                               _LIFETIME_ERASE_COUNT,
                               lifetimeEraseCount,
                               dataSectorHeader);

        // Write back packed header info to EEPROM
        status = spiRomWritePage(pSpiRom, &dataSectorHeader,
                     pDestDataSector->dataSectorOffset,
                     LW_ERASE_LEDGER_DATA_SECTOR_HEADER_SIZE);
        if (status != FLCN_OK)
        {
            goto s_spiRomCopyLedgerSector_exit;
        }

        pDestDataSector->header.lifetimeEraseCount = lifetimeEraseCount;

        // Copy and update Session details
        status = spiRomReadDirect_HAL(&Spi, pSpiRom,
                     pDataSector->session.startOffset - LW_ERASE_LEDGER_SESSION_HEADER_SIZE,
                     sizeof(LwU32), (LwU8 *)&sessionHeader);
        if (status != FLCN_OK)
        {
            goto s_spiRomCopyLedgerSector_exit;
        }

        // Write back packed session header info to EEPROM
        status = spiRomWritePage(pSpiRom, &sessionHeader,
                     pDestDataSector->session.startOffset - LW_ERASE_LEDGER_SESSION_HEADER_SIZE,
                     LW_ERASE_LEDGER_SESSION_HEADER_SIZE);
        if (status != FLCN_OK)
        {
            goto s_spiRomCopyLedgerSector_exit;
        }

        pDestDataSector->session.header.eraseLimit = pDataSector->session.header.eraseLimit;
        pDestDataSector->session.tallyBitOffset    = 0;

        pDestDataSector = pDestDataSector->pNext;
    }

s_spiRomCopyLedgerSector_exit:
    return status;
}

sysSYSLIB_CODE FLCN_STATUS
spiRomInitEraseLedger_LS10
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    LwU32                   headerOffset;
    FLCN_STATUS             status        = FLCN_ERR_ILWALID_STATE;
    PERASE_LEDGER_REGION    pLedgerRegion = NULL;

    if ((pSpiRom == NULL) ||
        (!LW_ROM_REGION_INITIALIZED(pSpiRom->pLedgerRegion)))
    {
        SOE_HALT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomInitEraseLedger_LS10_exit;
    }

    pLedgerRegion = pSpiRom->pLedgerRegion;

    // Determine starting offset of the ERASE_LEDGER_HEADER in EEPROM.
    headerOffset = pLedgerRegion->startAddress - LW_ERASE_LEDGER_HEADER_SIZE;

    // Read ERASE_LEDGER_HEADER from EEPROM.
    status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, headerOffset,
                                  sizeof(ERASE_LEDGER_HEADER),
                                  (LwU8 *)&(eraseLedger.header));
    if (status != FLCN_OK)
    {
        goto spiRomInitEraseLedger_LS10_exit;
    }

    // Validate the Erase Ledger image signature in the header.
    if (memcmp(eraseLedger.header.signature, "NBSI EL",
                    LW_ERASE_LEDGER_IMAGE_SIG_SIZE))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomInitEraseLedger_LS10_exit;
    }

    // Sanity check the start and end addresses
    if (pLedgerRegion->startAddress >= pLedgerRegion->endAddress)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomInitEraseLedger_LS10_exit;
    }

    // Check for Ledger sector alignment
    if (!LW_IS_ALIGNED(pLedgerRegion->startAddress, LW_ROM_IMAGE_ALIGN))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomInitEraseLedger_LS10_exit;
    }

    status = s_spiRomInitLedgerSectors(pSpiRom);
spiRomInitEraseLedger_LS10_exit:
    if (status != FLCN_OK)
    {
        // Deinitialize the InfoROM bounds so ROM operations fail
        if (pSpiRom)
        {
            pSpiRom->pLedgerRegion->startAddress = ~((LwU32)0);
            pSpiRom->pLedgerRegion->endAddress   = ~((LwU32)0);
        }
    }

    return status;
}

/*!
 * @brief Initialize Erase Ledger accessible range.
 *
 * Initialize Erase Ledger region from secure scratch registers which must be
 * populated by firmware.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 *
 * @return FLCN_ERR_ILWALID_STATE        If any pointer argument is NULL.
 * @return FLCN_ERR_FEATURE_NOT_ENABLED  If scratch register fields are not
 *                                       populated by firmware.
 * @return FLCN_OK                       If SPI EEPROM Accessible range was
 *                                       correctly initialized.
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomInitLedgerRegionFromScratch_LS10
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    LwU32    reg;
    LwU32    offset4K;
    LwU32    sizeIn4K;

    if ((pSpiRom == NULL) || ((pSpiRom->pLedgerRegion) == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Initialize EL region from secure scratch register.
    reg = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_0);

    // Sanity checks for valid EL bounds;
    if (FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_OFFSET,
                            0xfff, reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_OFFSET, 0x0,
                                    reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_SIZE, 0xfff,
                                    reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_SIZE, 0x0,
                                    reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    offset4K = DRF_VAL(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_OFFSET,
                            reg);
    sizeIn4K = DRF_VAL(_LWLSAW_SW, _SCRATCH_0, _ERASE_LEDGER_CARVEOUT_SIZE,
                            reg);

    pSpiRom->pLedgerRegion->startAddress = (offset4K << 12);
    pSpiRom->pLedgerRegion->endAddress   = pSpiRom->pLedgerRegion->startAddress +
                                               (sizeIn4K << 12);

    return FLCN_OK;
}

/*!
 * Check whether sector erase limit is reached or not.
 *
 * This function checks whether Erase limit is exhausted or not for
 * the tracked data Sector.
 *
 * @param[in]   pSpiRom                 SPI_DEVICE_ROM pointer.
 * @param[in]   eraseAddress            Starting address of tracked data Sector.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT    If argument is an invalid pointer or
 *                                      accessible range is uninitialized.
 * @return FLCN_ERR_ILWALID_STATE       If the ROM contents of the accessible
 *                                      range are not valid for the accessible
 *                                      range (e.g., not a valid Ledger image)
 * @return FLCN_OK                      The accessible range of the ROM device
 *                                      is valid.
 */
sysSYSLIB_CODE LwBool
spiRomIsEraseLimitReached_LS10
(
    PSPI_DEVICE_ROM    pSpiRom,
    LwU32              eraseAddress
)
{
    PERASE_LEDGER_DATA_SECTOR   pDataSector;
    PERASE_LEDGER_SECTOR        pLedgerSector;
    LwU32                       dataSectorAddr;
    LwU32                       lifetimeEraseCount;

    (void)s_spiRomGetLedgerSector(pSpiRom, eraseAddress, &pLedgerSector);

    dataSectorAddr = pLedgerSector->header.dataSectorRangeFirst << 12;

    // Find the data sector pointer to determine sector lifetime erase count.
    FOR_EACH_SECTOR(pLedgerSector, pDataSector)
    {
        if (dataSectorAddr == eraseAddress)
        {
            break;
        }
        dataSectorAddr += LW_ROM_SECTOR_SIZE;
    }

    if (pDataSector == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    lifetimeEraseCount = pDataSector->header.lifetimeEraseCount +
                         pDataSector->session.tallyBitOffset;

    return (lifetimeEraseCount >= pDataSector->session.header.eraseLimit);
}

/*!
 * Tally erase for tracked data sector.
 *
 * @param[in]   pSpiRom                 SPI_DEVICE_ROM pointer.
 * @param[in]   eraseAddress            Starting address of tracked data Sector.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT    If argument is an invalid pointer or
 *                                      accessible range is uninitialized.
 * @return FLCN_ERR_ILWALID_STATE       If the ROM contents of the accessible
 *                                      range are not valid for the accessible
 *                                      range (e.g., not a valid Ledger image)
 * @return FLCN_OK                      The accessible range of the ROM device
 *                                      is valid.
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomEraseTally_LS10
(
    PSPI_DEVICE_ROM    pSpiRom,
    LwU32              eraseAddress
)
{
    PERASE_LEDGER_DATA_SECTOR   pDataSector;
    PERASE_LEDGER_SECTOR        pLedgerSector;
    PERASE_LEDGER_SECTOR        pSrcSector;
    PERASE_LEDGER_SECTOR        pDestSector;
    PERASE_LEDGER               pEL = &eraseLedger;
    LwU32                       tallyByteOffset;
    LwU32                       dataSectorAddr;
    LwU32                       tallyByte;
    LwU32                       tallyDword;
    FLCN_STATUS                 status = FLCN_ERR_ILWALID_STATE;
    LwBool                      bFound = LW_FALSE;

    (void)s_spiRomGetLedgerSector(pSpiRom, eraseAddress, &pLedgerSector);

    dataSectorAddr = pLedgerSector->header.dataSectorRangeFirst << 12;

    // Find the data sector pointer corresponding to eraseAddress.
    FOR_EACH_SECTOR(pLedgerSector, pDataSector)
    {
        if (dataSectorAddr == eraseAddress)
        {
            break;
        }
        dataSectorAddr += LW_ROM_SECTOR_SIZE;
    }

    if (pDataSector == NULL)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    tallyByteOffset =  pDataSector->session.startOffset +
                       ((pDataSector->session.tallyBitOffset) / 8);

    //
    // Tally erase for tracked data sector if there is place to tally in
    // lwrrently active session.
    //
    if (tallyByteOffset <= (pDataSector->session.endOffset))
    {
        // Read tallyByte from EEPROM;
        status = spiRomReadDirect_HAL(&Spi, pSpiRom,
                     tallyByteOffset, sizeof(LwU8), (LwU8 *)&tallyByte);
        if (status != FLCN_OK)
        {
            return status;
        }

        //
        // Lwrrently HW-SPI writes are 4 bytes aligned (size of DATA_ARRAY
        // registers). So, writing back a dword here instead of single byte.
        // TODO-ANUJS : HW-SPI write calls (spiWriteData_HAL()) need not enforce
        // 4-byte alignment. Instead, irrelevant bytes can be set to all 1's.
        //
        tallyDword = ((LwU32)(tallyByte << 1)) | 0xffffff00;

        // Write back tallyDword to EEPROM.
        status = spiRomWritePage(pSpiRom, &(tallyDword), tallyByteOffset, sizeof(LwU32));
        if (status != FLCN_OK)
        {
            return status;
        }

        // Update tally count in Erase Ledger structures.
        pDataSector->session.tallyBitOffset++;

        return FLCN_OK;
    }

    //
    // If there is no place to tally in the ledger sector then
    // look for an unused ledger sector. If found then mark current ledger
    // sector as invalid and copy headers to unused sector with updated
    // lifetimeEraseCount. Otherwise reclaim an invalid ledger by erasing.
    //
    pSrcSector = pLedgerSector;
    FOR_EACH_SECTOR(pEL, pLedgerSector)
    {
        if (!(pLedgerSector->header.bInUse))
        {
            bFound = LW_TRUE;
            break;
        }
    }

    if (bFound)
    {
        pDestSector = pLedgerSector;
        pSrcSector->header.bValid = LW_FALSE;
    }
    else
    {
        pLedgerSector = pSrcSector;
        FOR_EACH_SECTOR(pEL, pLedgerSector)
        {
            if (!(pLedgerSector->header.bValid))
            {
                break;
            }
        }

        if (pLedgerSector == NULL)
        {
            return FLCN_ERR_ILWALID_STATE;
        }

        //
        // TODO-ANUJS : Erase all invalid sectors instead of only one
        // when erase calls use timer callbacks (Bug 200244811).
        //
        status = spiRomEraseSector(pSpiRom, pLedgerSector->ledgerSectorOffset);
        if (status != FLCN_OK)
        {
            return status;
        }

        pDestSector = pLedgerSector;
    }

    //
    // Copy source ledger sector into pDestination ledger sector and mark
    // current ledger sector as invalid.
    //
    status = s_spiRomCopyLedgerSector(pSpiRom, pSrcSector, pDestSector, eraseAddress);
    if (status != FLCN_OK)
    {
        return status;
    }

    return FLCN_OK;
}
