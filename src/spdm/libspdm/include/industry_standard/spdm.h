/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//
// LWE (tandrewprinz): Including LWPU copyright, as file contains LWPU modifications.
// A few LWPU-custom extensions to the SPDM protocol are defined here.
//

/** @file
  Definitions of DSP0274 Security Protocol & data Model Specification (SPDM)
  version 1.1.0 in Distributed Management Task Force (DMTF).
**/

#ifndef __SPDM_H__
#define __SPDM_H__

#pragma pack(1)

///
/// SPDM response code (1.0)
///
#define SPDM_DIGESTS 0x01
#define SPDM_CERTIFICATE 0x02
#define SPDM_CHALLENGE_AUTH 0x03
#define SPDM_VERSION 0x04
#define SPDM_MEASUREMENTS 0x60
#define SPDM_CAPABILITIES 0x61
#define SPDM_ALGORITHMS 0x63
#define SPDM_VENDOR_DEFINED_RESPONSE 0x7E
#define SPDM_ERROR 0x7F
///
/// SPDM response code (1.1)
///
#define SPDM_KEY_EXCHANGE_RSP 0x64
#define SPDM_FINISH_RSP 0x65
#define SPDM_PSK_EXCHANGE_RSP 0x66
#define SPDM_PSK_FINISH_RSP 0x67
#define SPDM_HEARTBEAT_ACK 0x68
#define SPDM_KEY_UPDATE_ACK 0x69
#define SPDM_ENCAPSULATED_REQUEST 0x6A
#define SPDM_ENCAPSULATED_RESPONSE_ACK 0x6B
#define SPDM_END_SESSION_ACK 0x6C
// LWE(eddichang): Adding vendor defined response message code
#define SPDM_VENDOR_DEFINED_RESPONSE 0x7E
///
/// SPDM request code (1.0)
///
#define SPDM_GET_DIGESTS 0x81
#define SPDM_GET_CERTIFICATE 0x82
#define SPDM_CHALLENGE 0x83
#define SPDM_GET_VERSION 0x84
#define SPDM_GET_MEASUREMENTS 0xE0
#define SPDM_GET_CAPABILITIES 0xE1
#define SPDM_NEGOTIATE_ALGORITHMS 0xE3
#define SPDM_VENDOR_DEFINED_REQUEST 0xFE
#define SPDM_RESPOND_IF_READY 0xFF
///
/// SPDM request code (1.1)
///
#define SPDM_KEY_EXCHANGE 0xE4
#define SPDM_FINISH 0xE5
#define SPDM_PSK_EXCHANGE 0xE6
#define SPDM_PSK_FINISH 0xE7
#define SPDM_HEARTBEAT 0xE8
#define SPDM_KEY_UPDATE 0xE9
#define SPDM_GET_ENCAPSULATED_REQUEST 0xEA
#define SPDM_DELIVER_ENCAPSULATED_RESPONSE 0xEB
#define SPDM_END_SESSION 0xEC
// LWE(eddichang): Adding vendor defined request message code
#define SPDM_VENDOR_DEFINED_REQUEST 0xFE

///
/// SPDM message header
///
typedef struct {
	uint8 spdm_version;
	uint8 request_response_code;
	uint8 param1;
	uint8 param2;
} spdm_message_header_t;

#define SPDM_MESSAGE_VERSION_10 0x10
#define SPDM_MESSAGE_VERSION_11 0x11
#define SPDM_MESSAGE_VERSION SPDM_MESSAGE_VERSION_10

///
/// SPDM GET_VERSION request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_get_version_request_t;

///
/// SPDM GET_VERSION response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	uint8 reserved;
	uint8 version_number_entry_count;
	//spdm_version_number_t  version_number_entry[version_number_entry_count];
} spdm_version_response;

///
/// SPDM VERSION structure
///
typedef struct {
	uint16 alpha : 4;
	uint16 update_version_number : 4;
	uint16 minor_version : 4;
	uint16 major_version : 4;
} spdm_version_number_t;

///
/// SPDM GET_CAPABILITIES request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	// Below field is added in 1.1.
	uint8 reserved;
	uint8 ct_exponent;
	uint16 reserved2;
	uint32 flags;
} spdm_get_capabilities_request;

///
/// SPDM GET_CAPABILITIES response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	uint8 reserved;
	uint8 ct_exponent;
	uint16 reserved2;
	uint32 flags;
} spdm_capabilities_response;

///
/// SPDM GET_CAPABILITIES request flags (1.1)
///
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP BIT1
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHAL_CAP BIT2
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCRYPT_CAP BIT6
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP BIT7
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP BIT8
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_EX_CAP BIT9
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PSK_CAP (BIT10 | BIT11)
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PSK_CAP_REQUESTER BIT10
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCAP_CAP BIT12
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HBEAT_CAP BIT13
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_UPD_CAP BIT14
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP BIT15
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PUB_KEY_ID_CAP BIT16

///
/// SPDM GET_CAPABILITIES response flags (1.0)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CACHE_CAP BIT0
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP BIT1
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHAL_CAP BIT2
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP (BIT3 | BIT4)
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_NO_SIG BIT3
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_SIG BIT4
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP BIT5
///
/// SPDM GET_CAPABILITIES response flags (1.1)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCRYPT_CAP BIT6
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MAC_CAP BIT7
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MUT_AUTH_CAP BIT8
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP BIT9
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP (BIT10 | BIT11)
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP_RESPONDER BIT10
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP_RESPONDER_WITH_CONTEXT    \
	BIT11
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCAP_CAP BIT12
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HBEAT_CAP BIT13
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_UPD_CAP BIT14
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP BIT15
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PUB_KEY_ID_CAP BIT16

///
/// SPDM NEGOTIATE_ALGORITHMS request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == Number of Algorithms Structure Tables
	// param2 == RSVD
	uint16 length;
	uint8 measurement_specification;
	uint8 reserved;
	uint32 base_asym_algo;
	uint32 bash_hash_algo;
	uint8 reserved2[12];
	uint8 ext_asym_count;
	uint8 ext_hash_count;
	uint16 reserved3;
	//spdm_extended_algorithm_t                 ext_asym[ext_asym_count];
	//spdm_extended_algorithm_t                 ext_hash[ext_hash_count];
	// Below field is added in 1.1.
	//spdm_negotiate_algorithms_struct_table_t  alg_struct[param1];
} spdm_negotiate_algorithms_request_t;

typedef struct {
	uint8 alg_type;
	uint8 alg_count; // BIT[0:3]=ext_alg_count, BIT[4:7]=fixed_alg_byte_count
	//uint8                alg_supported[fixed_alg_byte_count];
	//uint32               alg_external[ext_alg_count];
} spdm_negotiate_algorithms_struct_table_t;

typedef struct {
	uint8 ext_alg_count : 4;
	uint8 fixed_alg_byte_count : 4;
} spdm_negotiate_algorithms_struct_table_alg_count_t;

#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_DHE 2
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_AEAD 3
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_REQ_BASE_ASYM_ALG 4
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_KEY_SCHEDULE 5

typedef struct {
	uint8 alg_type;
	uint8 alg_count;
	uint16 alg_supported;
} spdm_negotiate_algorithms_common_struct_table_t;

///
/// SPDM NEGOTIATE_ALGORITHMS request base_asym_algo/REQ_BASE_ASYM_ALG
///
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048 BIT0
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048 BIT1
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072 BIT2
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072 BIT3
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256 BIT4
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096 BIT5
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096 BIT6
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384 BIT7
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521 BIT8

///
/// SPDM NEGOTIATE_ALGORITHMS request bash_hash_algo
///
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256 BIT0
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384 BIT1
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512 BIT2
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256 BIT3
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384 BIT4
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512 BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS request DHE
///
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_2048 BIT0
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_3072 BIT1
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_4096 BIT2
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1 BIT3
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1 BIT4
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_521_R1 BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS request AEAD
///
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM BIT0
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM BIT1
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305 BIT2
// LWE (tandrewprinz) LW_LWSTOM_ALG: Adding ID for LW-defined AEAD.
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_CTR_HMAC_SHA_256 BIT15

///
/// SPDM NEGOTIATE_ALGORITHMS request KEY_SCHEDULE
///
#define SPDM_ALGORITHMS_KEY_SCHEDULE_HMAC_HASH BIT0

///
/// SPDM NEGOTIATE_ALGORITHMS response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == Number of Algorithms Structure Tables
	// param2 == RSVD
	uint16 length;
	uint8 measurement_specification_sel;
	uint8 reserved;
	uint32 measurement_hash_algo;
	uint32 base_asym_sel;
	uint32 base_hash_sel;
	uint8 reserved2[12];
	uint8 ext_asym_sel_count;
	uint8 ext_hash_sel_count;
	uint16 reserved3;
	//spdm_extended_algorithm_t                 ext_asym_sel[ext_asym_sel_count];
	//spdm_extended_algorithm_t                 ext_hash_sel[ext_hash_sel_count];
	// Below field is added in 1.1.
	//spdm_negotiate_algorithms_struct_table_t  alg_struct[param1];
} spdm_algorithms_response_t;

///
/// SPDM NEGOTIATE_ALGORITHMS response measurement_hash_algo
///
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_RAW_BIT_STREAM_ONLY BIT0
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256 BIT1
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384 BIT2
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_512 BIT3
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_256 BIT4
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_384 BIT5
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_512 BIT6

///
/// SPDM extended algorithm
///
typedef struct {
	uint8 registry_id;
	uint8 reserved;
	uint16 algorithm_id;
} spdm_extended_algorithm_t;

///
/// SPDM registry_id
///
#define SPDM_REGISTRY_ID_DMTF 0
#define SPDM_REGISTRY_ID_TCG 1
#define SPDM_REGISTRY_ID_USB 2
#define SPDM_REGISTRY_ID_PCISIG 3
#define SPDM_REGISTRY_ID_IANA 4
#define SPDM_REGISTRY_ID_HDBASET 5
#define SPDM_REGISTRY_ID_MIPI 6
#define SPDM_REGISTRY_ID_CXL 7
#define SPDM_REGISTRY_ID_JEDEC 8

///
/// SPDM GET_DIGESTS request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_get_digest_request_t;

///
/// SPDM GET_DIGESTS response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == slot_mask
	//uint8                digest[digest_size][slot_count];
} spdm_digest_response_t;

///
/// SPDM GET_CERTIFICATE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == slot_id
	// param2 == RSVD
	uint16 Offset;
	uint16 length;
} spdm_get_certificate_request_t;

///
/// SPDM GET_CERTIFICATE response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == slot_id
	// param2 == RSVD
	uint16 portion_length;
	uint16 remainder_length;
	//uint8                cert_chain[portion_length];
} spdm_certificate_response_t;

typedef struct {
	//
	// Total length of the certificate chain, in bytes,
	// including all fields in this table.
	//
	uint16 length;
	uint16 reserved;
	//
	// digest of the Root Certificate.
	// Note that Root Certificate is ASN.1 DER-encoded for this digest.
	// The hash size is determined by the SPDM device.
	//
	//uint8    root_hash[hash_size];
	//
	// One or more ASN.1 DER-encoded X509v3 certificates where the first certificate is signed by the Root
	// Certificate or is the Root Certificate itself and each subsequent certificate is signed by the preceding
	// certificate. The last certificate is the Leaf Certificate.
	//
	//uint8    certificates[length - 4 - hash_size];
} spdm_cert_chain_t;

///
/// SPDM CHALLENGE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == slot_id
	// param2 == HashType
	uint8 nonce[32];
} spdm_challenge_request_t;

///
/// SPDM CHALLENGE request HashType
///
#define SPDM_CHALLENGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH 0
#define SPDM_CHALLENGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH 1
#define SPDM_CHALLENGE_REQUEST_ALL_MEASUREMENTS_HASH 0xFF

///
/// SPDM CHALLENGE response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == ResponseAttribute
	// param2 == slot_mask
	//uint8                cert_chain_hash[digest_size];
	//uint8                nonce[32];
	//uint8                measurement_summary_hash[digest_size];
	//uint16               opaque_length;
	//uint8                opaque_data[opaque_length];
	//uint8                signature[key_size];
} spdm_challenge_auth_response_t;

typedef struct {
	uint8 slot_id : 4;
	uint8 reserved : 3;
	uint8 basic_mut_auth_req : 1;
} spdm_challenge_auth_response_attribute_t;

#define SPDM_CHALLENGE_AUTH_RESPONSE_ATTRIBUTE_BASIC_MUT_AUTH_REQ BIT7

///
/// SPDM GET_MEASUREMENTS request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == Attributes
	// param2 == measurement_operation
	uint8 nonce[32];
	// Below field is added in 1.1.
	uint8 SlotIDParam; // BIT[0:3]=slot_id, BIT[4:7]=reserved
} spdm_get_measurements_request_t;

typedef struct {
	uint8 slot_id : 4;
	uint8 reserved : 4;
} spdm_get_measurements_request_slot_id_parameter_t;

///
/// SPDM GET_MEASUREMENTS request Attributes
///
#define SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_GENERATE_SIGNATURE BIT0

///
/// SPDM GET_MEASUREMENTS request measurement_operation
///
#define SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_TOTAL_NUMBER_OF_MEASUREMENTS \
	0
//SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_INDEX
#define SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_ALL_MEASUREMENTS   \
	0xFF

///
/// SPDM MEASUREMENTS block common header
///
typedef struct {
	uint8 index;
	uint8 measurement_specification;
	uint16 measurement_size;
	//uint8                measurement[measurement_size];
} spdm_measurement_block_common_header_t;

#define SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF BIT0

///
/// SPDM MEASUREMENTS block DMTF header
///
typedef struct {
	uint8 dmtf_spec_measurement_value_type;
	uint16 dmtf_spec_measurement_value_size;
	//uint8                Dmtf_spec_measurement_value[dmtf_spec_measurement_value_size];
} spdm_measurement_block_dmtf_header_t;

typedef struct {
	spdm_measurement_block_common_header_t Measurement_block_common_header;
	spdm_measurement_block_dmtf_header_t Measurement_block_dmtf_header;
	//uint8                                 hash_value[hash_size];
} spdm_measurement_block_dmtf_t;

typedef struct {
	uint8 Content : 7;
	uint8 presentation : 1;
} SPDM_MEASUREMENTS_BLOCK_MEASUREMENT_TYPE;

///
/// SPDM MEASUREMENTS block MeasurementValueType
///
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_IMMUTABLE_ROM 0
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE 1
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION 2
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_FIRMWARE_CONFIGURATION 3
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MEASUREMENT_MANIFEST 4
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MASK 0x7
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM BIT7

///
/// SPDM GET_MEASUREMENTS response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == TotalNumberOfMeasurement/RSVD
	// param2 == slot_id
	uint8 number_of_blocks;
	uint8 measurement_record_length[3];
	//uint8                measurement_record[measurement_record_length];
	//uint8                nonce[32];
	//uint16               opaque_length;
	//uint8                opaque_data[opaque_length];
	//uint8                signature[key_size];
} spdm_measurements_response_t;

///
/// SPDM ERROR response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == Error Code
	// param2 == Error data
	//uint8                extended_error_data[];
} spdm_error_response_t;

///
/// SPDM error code
///
#define SPDM_ERROR_CODE_ILWALID_REQUEST 0x01
#define SPDM_ERROR_CODE_BUSY 0x03
#define SPDM_ERROR_CODE_UNEXPECTED_REQUEST 0x04
#define SPDM_ERROR_CODE_UNSPECIFIED 0x05
#define SPDM_ERROR_CODE_UNSUPPORTED_REQUEST 0x07
#define SPDM_ERROR_CODE_MAJOR_VERSION_MISMATCH 0x41
#define SPDM_ERROR_CODE_RESPONSE_NOT_READY 0x42
#define SPDM_ERROR_CODE_REQUEST_RESYNCH 0x43
#define SPDM_ERROR_CODE_VENDOR_DEFINED 0xFF
///
/// SPDM error code (1.1)
///
#define SPDM_ERROR_CODE_ILWALID_SESSION 0x02
#define SPDM_ERROR_CODE_DECRYPT_ERROR 0x06
#define SPDM_ERROR_CODE_REQUEST_IN_FLIGHT 0x08
#define SPDM_ERROR_CODE_ILWALID_RESPONSE_CODE 0x09
#define SPDM_ERROR_CODE_SESSION_LIMIT_EXCEEDED 0x0A

///
/// SPDM ResponseNotReady extended data
///
typedef struct {
	uint8 rd_exponent;
	uint8 request_code;
	uint8 token;
	uint8 rd_tm;
} spdm_error_data_response_not_ready_t;

typedef struct {
	spdm_message_header_t header;
	// param1 == Error Code
	// param2 == Error data
	spdm_error_data_response_not_ready_t extend_error_data;
} spdm_error_response_data_response_not_ready_t;

///
/// SPDM RESPONSE_IF_READY request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == request_code
	// param2 == token
} spdm_response_if_ready_request_t;

///
/// SPDM VENDOR_DEFINED request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	uint16 standard_id;
	uint8 len;
	//uint8                vendor_id[len];
	//uint16               payload_length;
	//uint8                vendor_defined_payload[payload_length];
} spdm_vendor_defined_request_msg_t;

///
/// SPDM VENDOR_DEFINED response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	uint16 standard_id;
	uint8 len;
	//uint8                vendor_id[len];
	//uint16               payload_length;
	//uint8                vendor_defined_payload[payload_length];
} spdm_vendor_defined_response_msg_t;

//
// Below command is defined in SPDM 1.1
//

///
/// SPDM KEY_EXCHANGE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == HashType
	// param2 == slot_id
	uint16 req_session_id;
	uint16 reserved;
	uint8 random_data[32];
	//uint8                exchange_data[D];
	//uint16               opaque_length;
	//uint8                opaque_data[opaque_length];
} spdm_key_exchange_request_t;

///
/// SPDM KEY_EXCHANGE request HashType
///
#define SPDM_KEY_EXCHANGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH 0
#define SPDM_KEY_EXCHANGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH 1
#define SPDM_KEY_EXCHANGE_REQUEST_ALL_MEASUREMENTS_HASH 0xFF

///
/// SPDM KEY_EXCHANGE response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == heartbeat_period
	// param2 == RSVD
	uint16 rsp_session_id;
	uint8 mut_auth_requested;
	uint8 req_slot_id_param;
	uint8 random_data[32];
	//uint8                exchange_data[D];
	//uint8                measurement_summary_hash[digest_size];
	//uint16               opaque_length;
	//uint8                opaque_data[opaque_length];
	//uint8                signature[S];
	//uint8                verify_data[H];
} spdm_key_exchange_response_t;

///
/// SPDM KEY_EXCHANGE response mut_auth_requested
///
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED BIT0
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_ENCAP_REQUEST BIT1
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_GET_DIGESTS BIT2

///
/// SPDM FINISH request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == signature_included
	// param2 == req_slot_id
	//uint8                signature[S];
	//uint8                verify_data[H];
} spdm_finish_request_t;

///
/// SPDM FINISH request signature_included
///
#define SPDM_FINISH_REQUEST_ATTRIBUTES_SIGNATURE_INCLUDED BIT0

///
/// SPDM FINISH response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	//uint8                verify_data[H];
} spdm_finish_response_t;

///
/// SPDM PSK_EXCHANGE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == HashType
	// param2 == RSVD
	uint16 req_session_id;
	uint16 psk_hint_length;
	uint16 context_length;
	uint16 opaque_length;
	//uint8                psk_hint[psk_hint_length];
	//uint8                context[context_length];
	//uint8                opaque_data[opaque_length];
} spdm_psk_exchange_request_t;

///
/// SPDM PSK_EXCHANGE response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == heartbeat_period
	// param2 == RSVD
	uint16 rsp_session_id;
	uint16 reserved;
	uint16 context_length;
	uint16 opaque_length;
	//uint8                measurement_summary_hash[digest_size];
	//uint8                context[context_length];
	//uint8                opaque_data[opaque_length];
	//uint8                verify_data[H];
} spdm_psk_exchange_response_t;

///
/// SPDM PSK_FINISH request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
	//uint8                verify_data[H];
} spdm_psk_finish_request_t;

///
/// SPDM PSK_FINISH response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_psk_finish_response_t;

///
/// SPDM HEARTBEAT request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_heartbeat_request_t;

///
/// SPDM HEARTBEAT response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_heartbeat_response_t;

///
/// SPDM KEY_UPDATE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == key_operation
	// param2 == tag
} spdm_key_update_request_t;

///
/// SPDM KEY_UPDATE Operations table
///
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_UPDATE_KEY 1
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_UPDATE_ALL_KEYS 2
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_VERIFY_NEW_KEY 3

///
/// SPDM KEY_UPDATE response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == key_operation
	// param2 == tag
} spdm_key_update_response_t;

///
/// SPDM GET_ENCAPSULATED_REQUEST request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_get_encapsulated_request_request_t;

///
/// SPDM ENCAPSULATED_REQUEST response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == request_id
	// param2 == RSVD
	//uint8                encapsulated_request[];
} spdm_encapsulated_request_response_t;

///
/// SPDM DELIVER_ENCAPSULATED_RESPONSE request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == request_id
	// param2 == RSVD
	//uint8                encapsulated_response[];
} spdm_deliver_encapsulated_response_request_t;

///
/// SPDM ENCAPSULATED_RESPONSE_ACK response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == request_id
	// param2 == payload_type
	//uint8                encapsulated_request[];
} spdm_encapsulated_response_ack_response_t;

///
/// SPDM ENCAPSULATED_RESPONSE_ACK_RESPONSE payload Type
///
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_ABSENT 0
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_PRESENT 1
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_REQ_SLOT_NUMBER 2

///
/// SPDM END_SESSION request
///
typedef struct {
	spdm_message_header_t header;
	// param1 == end_session_request_attributes
	// param2 == RSVD
} spdm_end_session_request_t;

///
/// SPDM END_SESSION request Attributes
///
#define SPDM_END_SESSION_REQUEST_ATTRIBUTES_PRESERVE_NEGOTIATED_STATE_CLEAR BIT0

///
/// SPDM END_SESSION response
///
typedef struct {
	spdm_message_header_t header;
	// param1 == RSVD
	// param2 == RSVD
} spdm_end_session_response_t;

#pragma pack()

#endif
