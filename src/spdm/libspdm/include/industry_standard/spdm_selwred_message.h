/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/** @file
  Definitions of DSP0277 Selwred Messages using SPDM Specification
  version 1.0.0 in Distributed Management Task Force (DMTF).
**/

#ifndef __SPDM_SELWRED_MESSAGE_H__
#define __SPDM_SELWRED_MESSAGE_H__

#pragma pack(1)

//
// ENC+AUTH session:
//
// +-----------------+
// | ApplicationData |-----------------------------------------------------
// +-----------------+                                                     |
//                                                                         V
// +---------------------------------+--------------------------=-------+-------+------+---+
// |SPDM_SELWRED_MESSAGE_ADATA_HEADER|spdm_selwred_message_cipher_header_t|AppData|Random|MAC|
// | session_id | SeqNum (O) | length |       application_data_length      |       |  (O) |   |
// +---------------------------------+----------------------------------+-------+------+---+
// |                                 |                                                 |   |
//  --------------------------------- ------------------------------------------------- ---
//                  |                                         |                          |
//                  V                                         V                          V
//            AssociatedData                            encrypted_data                 AeadTag
//
// (O) means Optional or Transport Layer Specific.
//
// AUTH session:
//
// +-----------------+
// | ApplicationData |------------------
// +-----------------+                  |
//                                      V
// +---------------------------------+-------+---+
// |SPDM_SELWRED_MESSAGE_ADATA_HEADER|AppData|MAC|
// | session_id | SeqNum (T) | length |       |   |
// +---------------------------------+-------+---+
// |                                         |   |
//  ----------------------------------------- ---
//                      |                     |
//                      V                     V
//                AssociatedData           AeadTag
//

typedef struct {
	uint32 session_id;
} spdm_selwred_message_a_data_header1_t;

// The length of sequence_number between HEADER_1 and HEADER_2 is transport specific.

typedef struct {
	uint16 length; // The length of the remaining data, including application_data_length(O), payload, Random(O) and MAC.
} spdm_selwred_message_a_data_header2_t;

typedef struct {
	uint16 application_data_length; // The length of the payload
} spdm_selwred_message_cipher_header_t;

//
// Selwred Messages opaque data format
//
#define SELWRED_MESSAGE_OPAQUE_DATA_SPEC_ID 0x444D5446
#define SELWRED_MESSAGE_OPAQUE_VERSION 0x1

typedef struct {
	uint32 spec_id; // SELWRED_MESSAGE_OPAQUE_DATA_SPEC_ID
	uint8 opaque_version; // SELWRED_MESSAGE_OPAQUE_VERSION
	uint8 total_elements;
	uint16 reserved;
	//opaque_element_table_t  opaque_list[];
} selwred_message_general_opaque_data_table_header_t;

typedef struct {
	uint8 id;
	uint8 vendor_len;
	//uint8    vendor_id[vendor_len];
	//uint16   opaque_element_data_len;
	//uint8    opaque_element_data[opaque_element_data_len];
	//uint8    align_padding[];
} opaque_element_table_header_t;

#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION 0x1

typedef struct {
	uint8 id; // SPDM_REGISTRY_ID_DMTF
	uint8 vendor_len;
	uint16 opaque_element_data_len;
	//uint8    sm_data_version;
	//uint8    sm_data_id;
	//uint8    sm_data[];
} selwred_message_opaque_element_table_header_t;

typedef struct {
	uint8 sm_data_version; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION
	uint8 sm_data_id; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_VERSION_SELECTION
} selwred_message_opaque_element_header_t;

#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_VERSION_SELECTION 0x0

typedef struct {
	uint8 sm_data_version; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION
	uint8 sm_data_id; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_VERSION_SELECTION
	spdm_version_number_t selected_version;
} selwred_message_opaque_element_version_selection_t;

#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_SUPPORTED_VERSION 0x1

typedef struct {
	uint8 sm_data_version; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION
	uint8 sm_data_id; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_SUPPORTED_VERSION
	uint8 version_count;
	//spdm_version_number_t   versions_list[version_count];
} selwred_message_opaque_element_supported_version_t;

#pragma pack()

#endif
