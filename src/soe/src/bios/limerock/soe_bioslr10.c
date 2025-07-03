
/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objsoe.h"
#include "soe_device.h"
#include "spidev.h"
#include "soe_objcore.h"
#include "dmemovl.h"
#include "soe_objdiscovery.h"
#include "dev_lwlipt_lnk_ip.h"

/* ------------------------ Application Includes --------------------------- */
#include "soe_device.h"
#include "soe_bioslr10.h"

#include "config/g_spi_private.h"
#include "config/g_soe_hal.h"
//#include "config/g_bios_private.h"

// MAX_PACKED_STRUCT_READ_SIZE
// should match the largest packed structure size,
// that we are going to read from the VBIOS
#define MAX_PACKED_STRUCT_READ_SIZE    32

/* ------------------------ Global --------------------------- */

static LwU32
_biosGetBiosSize
(
    PSPI_DEVICE_ROM     pSpiRom
)
{
    PROM_REGION pBiosRegion;
    LwU32 biosSize;

    // collect BIOS size
    pBiosRegion = pSpiRom->pBiosRegion;
    biosSize = pBiosRegion->endAddress - pBiosRegion->startAddress;

    return biosSize;
}

/*!
 * @brief Parse packed little endian data and unpack into padded structure
 *
 * @param[in]   format          Data format
 * @param[in]   packedData      Packed little endian data
 * @param[out]  unpackedData    Unpacked padded structure
 * @param[out]  unpackedSize    Unpacked data size
 * @param[out]  fieldsCount     Number of fields
 *
 * @return 'FLCN_OK'
 */
static FLCN_STATUS
_soe_devinit_unpack_structure
(
    const char *format,
    const LwU8 *packedData,
    LwU32      *unpackedData,
    LwU32      *unpackedSize,
    LwU32      *fieldsCount
)
{
    LwU32 unpkdSize = 0;
    LwU32 fields = 0;
    LwU32 count;
    LwU32 data;
    char fmt;

    while ((fmt = *format++))
    {
        count = 0;
        while ((fmt >= '0') && (fmt <= '9'))
        {
            count *= 10;
            count += fmt - '0';
            fmt = *format++;
        }
        if (count == 0)
            count = 1;

        while (count--)
        {
            switch (fmt)
            {
                case 'b':
                    data = *packedData++;
                    unpkdSize += 1;
                    break;

                case 's':    // signed byte
                    data = *packedData++;
                    if (data & 0x80)
                        data |= ~0xff;
                    unpkdSize += 1;
                    break;

                case 'w':
                    data  = *packedData++;
                    data |= *packedData++ << 8;
                    unpkdSize += 2;
                    break;

                case 'd':
                    data  = *packedData++;
                    data |= *packedData++ << 8;
                    data |= *packedData++ << 16;
                    data |= *packedData++ << 24;
                    unpkdSize += 4;
                    break;

                default:
                    return FLCN_ERROR;
            }
            *unpackedData++ = data;
            fields++;
        }
    }

    if (unpackedSize != NULL)
        *unpackedSize = unpkdSize;

    if (fieldsCount != NULL)
        *fieldsCount = fields;

    return FLCN_OK;
}

/*!
 * @brief Callwlate packed and unpacked data size based on given data format
 *
 * @param[in]   format          Data format
 * @param[out]  packedSize      Packed data size
 * @param[out]  unpackedSize    Unpacked data size
 *
 */
static void
_soe_devinit_callwlate_sizes
(
    const char *format,
    LwU32      *packedSize,
    LwU32      *unpackedSize
)
{
    LwU32 unpkdSize = 0;
    LwU32 pkdSize = 0;
    LwU32 count;
    char fmt;

    while ((fmt = *format++))
    {
        count = 0;
        while ((fmt >= '0') && (fmt <= '9'))
        {
            count *= 10;
            count += fmt - '0';
            fmt = *format++;
        }
        if (count == 0)
            count = 1;

        switch (fmt)
        {
            case 'b':
                pkdSize += count * 1;
                unpkdSize += count * sizeof(bios_U008);
                break;

            case 's':    // signed byte
                pkdSize += count * 1;
                unpkdSize += count * sizeof(bios_S008);
                break;

            case 'w':
                pkdSize += count * 2;
                unpkdSize += count * sizeof(bios_U016);
                break;

            case 'd':
                pkdSize += count * 4;
                unpkdSize += count * sizeof(bios_U032);
                break;
        }
    }

    if (packedSize != NULL)
        *packedSize = pkdSize;

    if (unpackedSize != NULL)
        *unpackedSize = unpkdSize;
}

/*!
 * @brief Read VBIOS and
 * @Callwlate packed and unpacked data size based on given data format
 *
 * @param[in]   format          Data format
 * @param[out]  packedSize      Packed data size
 * @param[out]  unpackedSize    Unpacked data size
 *
 */

static FLCN_STATUS
_soe_vbios_read_structure
(
    lwswitch_device *device,
    void            *structure,
    LwU32           offset,
    LwU32           *ppacked_size,
    const char      *format
)
{
    LwU32  packed_size;
    LwU8  *packed_data;
    LwU32  unpacked_bytes;
    LwU32 readAddress;
    FLCN_STATUS status = FLCN_ERROR;

    // callwlate the size of the data as indicated by its packed format.
    _soe_devinit_callwlate_sizes(format, &packed_size, &unpacked_bytes);

    // 
    // is 'packed_size' bigger than packed_data buffer size ?
    // 
    if (packed_size > MAX_PACKED_STRUCT_READ_SIZE)
    {
        SOE_HALT();
        return FLCN_ERROR;  
    }

    if (ppacked_size)
        *ppacked_size = packed_size;

    //
    // is 'offset' too big?
    // happens when we read bad ptrs from fixed addrs in image frequently
    //
    if ((offset + packed_size) > device->biosImage.size)
    {
        return FLCN_ERROR;
    }

    packed_data = &device->biosImage.pImage[0];

    readAddress = (spiRom.pBiosRegion->startAddress + offset);
    // read VBIOS content
    status = spiRomReadDirect_HAL(&SpiHal, &spiRom, readAddress, packed_size, packed_data);
    if (status != FLCN_OK)
    {
        return status;
    }
    
    return _soe_devinit_unpack_structure(format, packed_data, structure, &unpacked_bytes, NULL);
}

static LwU8
_soe_vbios_read8
(
    lwswitch_device *device,
    LwU32           offset
)
{
    bios_U008 data;     // BiosReadStructure expects 'bios' types

    _soe_vbios_read_structure(device, &data, offset, (LwU32 *) 0, "b");

    return (LwU8) data;
}

static LwU16
_soe_vbios_read16
(
    lwswitch_device *device,
    LwU32           offset
)
{
    bios_U016 data;     // BiosReadStructure expects 'bios' types

    _soe_vbios_read_structure(device, &data, offset, (LwU32 *) 0, "w");

    return (LwU16) data;
}


static LwU32
_soe_vbios_read32
(
    lwswitch_device *device,
    LwU32           offset
)
{
    bios_U032 data;     // BiosReadStructure expects 'bios' types

    _soe_vbios_read_structure(device, &data, offset, (LwU32 *) 0, "d");

    return (LwU32) data;
}
/*!
 * @brief Parsing VBIOS to get Lwlink Config Table address
 */
static FLCN_STATUS
_soe_verify_BIT_Version
(
    lwswitch_device *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    BIT_HEADER_V1_00         bitHeader;
    BIT_TOKEN_V1_00          bitToken;
    FLCN_STATUS              rmStatus;
    LwU32                    dataPointerOffset;
    LwU8                     i;

    rmStatus = _soe_vbios_read_structure(device,
                                              (LwU8*) &bitHeader,
                                              bios_config->bit_address,
                                              (LwU32 *) 0,
                                              BIT_HEADER_V1_00_FMT);

    if(rmStatus != FLCN_OK)
    {
        return rmStatus;
    }

    //
    // parse through the bit tokens quickly and isolate the biosdata and intdata tokens
    // once we have both, compare the versions and either return success or failure
    //
    for(i=0; i < bitHeader.TokenEntries; i++)
    {
        LwU32 BitTokenLocation = bios_config->bit_address + bitHeader.HeaderSize + (i * bitHeader.TokenSize);
        rmStatus = _soe_vbios_read_structure(device,
                                          (LwU8*) &bitToken,
                                          BitTokenLocation,
                                          (LwU32 *) 0,
                                          BIT_TOKEN_V1_00_FMT);
        if(rmStatus != FLCN_OK)
        {
            return FLCN_ERROR;
        }

        dataPointerOffset = (bios_config->pci_image_address + bitToken.DataPtr);
        switch(bitToken.TokenId)
        {
            case BIT_TOKEN_LWINIT_PTRS:
            {
                BIT_DATA_LWINIT_PTRS_V1 lwInitTablePtrs;
                rmStatus = _soe_vbios_read_structure(device,
                                                  (LwU8*) &lwInitTablePtrs,
                                                  dataPointerOffset,
                                                  (LwU32 *) 0,
                                                  BIT_DATA_LWINIT_PTRS_V1_30_FMT);
                if (rmStatus != FLCN_OK)
                {
                    return FLCN_ERROR;
                }
                // Update the retrived info with device info
                bios_config->lwlink_config_table_address = (lwInitTablePtrs.LwlinkConfigDataPtr + bios_config->pci_image_address);
            }
        }
    }

    if (rmStatus == FLCN_OK)
    {
        return FLCN_OK;
    }

    return FLCN_ERROR;
}


static FLCN_STATUS
_soe_validate_BIT_header
(
    lwswitch_device *device,
    LwU32            bit_address
)
{
    LwU32    headerSize = 0;
    LwU32    chkSum = 0;
    LwU32    i;

    //
    // For now let's assume the Header Size is always at the same place.
    // We can create something more complex if needed later.
    //
    headerSize = (LwU32)_soe_vbios_read8(device, bit_address + BIT_HEADER_SIZE_OFFSET);

    // Now perform checksum
    for (i = 0; i < headerSize; i++)
        chkSum += (LwU32)_soe_vbios_read8(device, bit_address + i);

    //Byte checksum removes upper bytes
    chkSum = chkSum & 0xFF;

    if (chkSum)
        return FLCN_ERROR;

    return FLCN_OK;
}


static FLCN_STATUS
_soe_verify_header
(
    lwswitch_device *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    LwU32       i;
    FLCN_STATUS   status = FLCN_ERROR;

    if ((bios_config == NULL) || (!bios_config->pci_image_address))
    {
        return status;
    }

    // attempt to find the init info in the BIOS
    for (i = bios_config->pci_image_address; i < device->biosImage.size - 3; i++)
    {
        LwU16 bitheaderID = _soe_vbios_read16(device, i);
        if (bitheaderID == BIT_HEADER_ID)
        {
            LwU32 signature = _soe_vbios_read32(device, i + 2);
            if (signature == BIT_HEADER_SIGNATURE)
            {
                bios_config->bit_address = i;

                // Checksum BIT to prove accuracy
                if (FLCN_OK != _soe_validate_BIT_header(device, bios_config->bit_address))
                {
                    device->biosImage.pImage = 0;
                    device->biosImage.size = 0;
                }
            }
        }
        // only if we find the bit address do we break
        if (bios_config->bit_address)
            break;
    }
    if (bios_config->bit_address)
    {
        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief Parse VBIOS for BIT OFFSET based on PCI image location 
 */ 
static FLCN_STATUS
_soe_vbios_update_bit_offset
(
    lwswitch_device *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    FLCN_STATUS   status = FLCN_OK;

    if (bios_config->bit_address)
    {
        goto soe_vbios_update_bit_Offset_done;
    }

    status = _soe_verify_header(device, bios_config);
    if (status != FLCN_OK)
    {
        goto soe_vbios_update_bit_Offset_done;
    }

    if (bios_config->bit_address)
    {
        status = _soe_verify_BIT_Version(device, bios_config);
        if (status != FLCN_OK)
        {
            goto soe_vbios_update_bit_Offset_done;
        }
    }

soe_vbios_update_bit_Offset_done:
    return status;
}

/*!
 * @brief Parse VBIOS for PCI image location
 */
static FLCN_STATUS
_soe_vbios_identify_pci_image_loc
(
    lwswitch_device         *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    FLCN_STATUS   status = FLCN_OK;
    LwU32       i;

    if (bios_config->pci_image_address)
    {
        goto soe_vbios_identify_pci_image_loc_done;
    }
for (i = 0; i < (device->biosImage.size - PCI_ROM_HEADER_PCI_DATA_SIZE); i++)
    // Match the PCI_EXP_ROM_SIGNATURE and followed by the PCI Data structure
    // with PCIR and matching vendor ID

    // attempt to find the init info in the BIOS
    
    {
        LwU16 pci_rom_sigature = _soe_vbios_read16(device, i);

        if (pci_rom_sigature == PCI_EXP_ROM_SIGNATURE)
        {
            LwU32 pcir_data_dffSet  = _soe_vbios_read16(device, i + PCI_ROM_HEADER_SIZE);  // 0x16 -> 0x18 i.e, including the ROM Signature bytes

            if (((i + pcir_data_dffSet) + PCI_DATA_STRUCT_SIZE) < device->biosImage.size)
            {
                LwU32 pcirSigature = _soe_vbios_read32(device, (i + pcir_data_dffSet));

                if (pcirSigature == PCI_DATA_STRUCT_SIGNATURE)
                {
                    PCI_DATA_STRUCT pciData;
                    status = _soe_vbios_read_structure(device,
                                                   (LwU8*) &pciData,
                                                   i + pcir_data_dffSet,
                                                   (LwU32 *) 0,
                                                   PCI_DATA_STRUCT_FMT);
                    if (status != FLCN_OK)
                    {
                        goto soe_vbios_identify_pci_image_loc_done;
                    }

                    // Validate the vendor details as well
                    if (pciData.vendorID == PCI_VENDOR_ID_LWIDIA)
                    {
                        // mailbox validation
                        bios_config->pci_image_address = i;
                        break;
                    }
                }
            }
        }
    }

soe_vbios_identify_pci_image_loc_done:
    return status;
}

/*!
 * @brief Parse Lwlink configuration table
 */ 
static FLCN_STATUS
_soe_read_vbios_link_entries
(
    lwswitch_device *device,
    LwU32            tblPtr,
    LwU32            expected_link_entriesCount,
    LWLINK_CONFIG_DATA_LINKENTRY  *link_entries,
    LwU32            *identified_link_entriesCount
)
{
    FLCN_STATUS status = FLCN_ERROR;
    LwU32 i;
    LWLINK_VBIOS_CONFIG_DATA_LINKENTRY vbios_link_entry;
    *identified_link_entriesCount = 0;

    for (i = 0; i < expected_link_entriesCount; i++)
    {
        status = _soe_vbios_read_structure(device,
                                        &vbios_link_entry,
                                        tblPtr, (LwU32 *)0,
                                        LWLINK_CONFIG_DATA_LINKENTRY_FMT);
        if (status != FLCN_OK)
        {
            return status;
        }
        link_entries[i].lwLinkparam0 = (LwU8)vbios_link_entry.lwLinkparam0;
        link_entries[i].lwLinkparam1 = (LwU8)vbios_link_entry.lwLinkparam1;
        link_entries[i].lwLinkparam2 = (LwU8)vbios_link_entry.lwLinkparam2;
        link_entries[i].lwLinkparam3 = (LwU8)vbios_link_entry.lwLinkparam3;
        link_entries[i].lwLinkparam4 = (LwU8)vbios_link_entry.lwLinkparam4;
        link_entries[i].lwLinkparam5 = (LwU8)vbios_link_entry.lwLinkparam5;
        link_entries[i].lwLinkparam6 = (LwU8)vbios_link_entry.lwLinkparam6;
        tblPtr += sizeof(LWLINK_CONFIG_DATA_LINKENTRY);


    }

    *identified_link_entriesCount = i;
    return status;
}

/*!
 * @brief Access Lwlink config table from VBIOS based on BIT OFFSET
 * for the current switch
 */ 
static FLCN_STATUS
_soe_vbios_fetch_lwlink_entries
(
    lwswitch_device         *device,
    LWSWITCH_BIOS_LWLINK_CONFIG    *bios_config
)
{
    LwU32                       tblPtr;
    LwU8                        version;
    LwU8                        size;
    FLCN_STATUS                 status = FLCN_ERROR;
    LWLINK_CONFIG_DATA_HEADER   header;
    LwU32                       base_entry_index;
    LwU32                       expected_base_entry_count;
    //LwU32                       switch_physical_id = -1;

    tblPtr = bios_config->lwlink_config_table_address;
    if (!tblPtr)
    {
        goto soe_vbios_fetch_lwlink_entries_done;
    }

    // Read the table version number
    version = _soe_vbios_read8(device, tblPtr);

    switch (version)
    {
        case LWLINK_CONFIG_DATA_HEADER_VER_20:
            size = _soe_vbios_read8(device, tblPtr + 1);
            if (size == LWLINK_CONFIG_DATA_HEADER_20_SIZE)
            {
                // Grab Lwlink Config Data Header
                status = _soe_vbios_read_structure(device, &header.ver_20, tblPtr, (LwU32 *) 0, LWLINK_CONFIG_DATA_HEADER_20_FMT);
            }
            break;
        default:
            ;
    }
    if (status != FLCN_OK)
    {
        goto soe_vbios_fetch_lwlink_entries_done;
    }

    // collect base_entry_index from GPIO to read lwlink configuration
    //switch_physical_id = soeReadGpuPositionId_HAL();

    expected_base_entry_count = header.ver_20.BaseEntryCount;
    if (expected_base_entry_count > LWSWITCH_NUM_BIOS_LWLINK_CONFIG_BASE_ENTRY)
    {
        expected_base_entry_count = LWSWITCH_NUM_BIOS_LWLINK_CONFIG_BASE_ENTRY;
    }

    // Todo :
    // fetch the base_entry_index from GPIO to get link entries for that switch
    // Current implememntation :
    // LR10 all switch configuration are same. So using id-0's config for all
    expected_base_entry_count = 1;

    for (base_entry_index = 0; base_entry_index < expected_base_entry_count; base_entry_index++)
    {
        LwU32 expected_link_entriesCount = header.ver_20.LinkEntryCount;
        if (expected_link_entriesCount > LWSWITCH_NUM_LINKS_LR10)
        {
            expected_link_entriesCount = LWSWITCH_NUM_LINKS_LR10;
        }

        tblPtr += (header.ver_20.HeaderSize + header.ver_20.BaseEntrySize);
        // Todo :
        // introduce condition to read link entries based on switch_physical_id
        _soe_read_vbios_link_entries(device,
                                 tblPtr,
                                 expected_link_entriesCount,
                                 bios_config->link_vbios_entry[base_entry_index],
                                 &bios_config->identified_Link_entries[base_entry_index]);
    }
soe_vbios_fetch_lwlink_entries_done:
    return status;
}

/*!
 * @brief Access PCI image location from VBIOS
 */
static FLCN_STATUS
_biosFetchVbiosPciImageAddress_LR10
(
    struct lwswitch_device      *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    FLCN_STATUS status  = FLCN_ERROR;

    status = spiRomInitInforomRegionFromScratch_HAL(&SpiHal, &spiRom);
    if ((status != FLCN_OK) && (status != FLCN_ERR_FEATURE_NOT_ENABLED))
    {
        goto biosFetchVbiosPciImageAddress_LR10_exit;
    }

    device->biosImage.size = _biosGetBiosSize(&spiRom);
    if (device->biosImage.size == 0)
    {
        goto biosFetchVbiosPciImageAddress_LR10_exit;
    }

    if (_soe_vbios_identify_pci_image_loc(device, bios_config)  != FLCN_OK)
    {
        goto biosFetchVbiosPciImageAddress_LR10_exit;
    }

    if(bios_config->pci_image_address == 0)
    {
        status = FLCN_ERROR;
        goto biosFetchVbiosPciImageAddress_LR10_exit;
    }

biosFetchVbiosPciImageAddress_LR10_exit:
    return status;
}

/*!
 * @brief Generated mask for enbled Link with Active Repeater
 * from Lwlink Configuration table, parsed from VBIOS
 */
static LwU64
_biosFetchActiveRepeaterMask_LR10
(
    struct lwswitch_device      *device,
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config
)
{
    LwU8                         linkID;
    LwU32                        maxLinks;
    LWLINK_CONFIG_DATA_LINKENTRY *vbios_link_entry = NULL;
    LwU64                        lmask             = 0x0;
    
    maxLinks = bios_config->identified_Link_entries[0];

    if (maxLinks > NUM_LWLIPT_LNK_ENGINE)
    {
        REG_WR32_STALL(CSB, LW_CSOE_FALCON_MAILBOX0,
                       maxLinks);
        REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER,
                       SOE_ERROR_UNKNOWN);
        SOE_HALT();
    }
    
    for(linkID = 0; linkID < maxLinks; linkID++)
    {
        vbios_link_entry = &bios_config->link_vbios_entry[bios_config->link_base_entry_assigned][linkID];
        if ((vbios_link_entry != NULL) && 
             FLD_TEST_DRF(_LWLINK_VBIOS,_PARAM0, _ACTIVE_REPEATER, _PRESENT,
                          vbios_link_entry->lwLinkparam0))
        {
            lmask = lmask | LWBIT64(linkID);
        }
    }

    return lmask;
}

/*!
 * @brief Driver function to generate the Active Repeater Mask from VBIOS
 */
static FLCN_STATUS
_biosGenerateActiveRepeaterMask_LR10
(
    struct lwswitch_device      *device,
    LwU64                       *activeRepeaterMask
)
{
    LwU8        pReadBuffer[MAX_PACKED_STRUCT_READ_SIZE];
    FLCN_STATUS status                                     = FLCN_ERROR;

    // initialize BIOS Config ..
    LWSWITCH_BIOS_LWLINK_CONFIG *bios_config;
    bios_config = lwosMallocAligned(0, sizeof(LWSWITCH_BIOS_LWLINK_CONFIG), lwrtosBYTE_ALIGNMENT);
    if (bios_config == NULL)
    {
        goto biosGenerateActiveRepeaterMask_LR10_exit;
    }
    bios_config->link_base_entry_assigned = 0;
    bios_config->vbios_disabled_link_mask = 0;
    bios_config->bit_address                 = 0;
    bios_config->pci_image_address           = 0;
    bios_config->lwlink_config_table_address = 0;

    device->biosImage.pImage = pReadBuffer;

    //
    // Locate the PCI ROM Image
    //
    status = _biosFetchVbiosPciImageAddress_LR10(device, bios_config);
    if(status != FLCN_OK)
    {
        goto biosGenerateActiveRepeaterMask_LR10_exit;
    }

    //
    // Locate and fetch BIT offset
    //
    
    status = _soe_vbios_update_bit_offset(device, bios_config);
    if(status != FLCN_OK)
    {
        goto biosGenerateActiveRepeaterMask_LR10_exit;
    }

    //
    // Fetch LwLink Entries
    //
    status = _soe_vbios_fetch_lwlink_entries(device, bios_config);
    if(status != FLCN_OK)
    {
        goto biosGenerateActiveRepeaterMask_LR10_exit;
    }

    *activeRepeaterMask = _biosFetchActiveRepeaterMask_LR10(device, bios_config);

biosGenerateActiveRepeaterMask_LR10_exit:

    return status;
}

/*
 * @brief Setup the PROD values related to Optical Links for LR10 registers
 * that requires LS privileges.
 */
static void
_biosSetupProdValuesForActiveRepeaters_LR10
(
    LwU64 mask
)
{
    LwU32 baseAddress;
    LwU32 data32;
    LwU32 i;
                       
    data32 = REG_RD32_STALL(CSB, LW_CSOE_FALCON_SCTL);
    if (FLD_TEST_DRF(_CSOE, _FALCON_SCTL, _LSMODE, _TRUE, data32))
    {
        FOR_EACH_INDEX_IN_MASK(64, i, mask)
        {
            baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, i, ADDRESS_UNICAST, 0);
            
            //clear locks
            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_CLEAR);
            data32 = FLD_SET_DRF_NUM(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_CLEAR, _RESERVED_15, 1, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_CLEAR, data32);

            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR);
            data32 = FLD_SET_DRF_NUM(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR, _RECEIVER_DETECT_ENABLE, 1, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR, data32);

            //update registers
            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL);
            data32 = FLD_SET_DRF_NUM(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_MODE_CTRL, _RESERVED_15, 1, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL, data32);

            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL);
            data32 = FLD_SET_DRF(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_CHANNEL_CTRL, _RECEIVER_DETECT_ENABLE, _DISABLE, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL, data32);

            //enable locks
            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK);
            data32 = FLD_SET_DRF(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_MODE_CTRL_LOCK, _RESERVED_15, _LOCKED, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK, data32);

            data32 = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK);
            data32 = FLD_SET_DRF(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK, _RECEIVER_DETECT_ENABLE , _LOCKED, data32);
            REG_WR32(BAR0, baseAddress + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK, data32);

        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

}

/*!
 * Pre-STATE_INIT for LWLINK
 */
void
biosPreInit_LR10(void)
{
    LwU64 mask = 0x0;
    LwBool bIsFmodel;
    LwBool bIsEmulation;

    bIsFmodel = FLD_TEST_DRF(_PSMC, _BOOT_2, _FMODEL, _YES, 
                                REG_RD32(BAR0, LW_PSMC_BOOT_2));

    bIsEmulation = FLD_TEST_DRF(_PSMC, _BOOT_2, _EMULATION, _YES, 
                                REG_RD32(BAR0, LW_PSMC_BOOT_2));

    if (bIsFmodel || bIsEmulation)
    {
        return;
    }

    _biosGenerateActiveRepeaterMask_LR10(&device, &mask);

    _biosSetupProdValuesForActiveRepeaters_LR10(mask);
}
