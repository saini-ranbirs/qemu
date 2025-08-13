/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_MM_COMMUNICATION_H__
#define __RPMI_MM_COMMUNICATION_H__

#define MAX_BIT  0x8000000000000000ULL
#define ENCODE_ERROR(code)  ((rpmi_uint64_t)(MAX_BIT | (code)))
#define RETURN_ERROR(code)  (((rpmi_int64_t)(rpmi_uint64_t)(code)) < 0)

#define EFI_SUCCESS            (rpmi_uint64_t)0
#define EFI_INVALID_PARAMETER  ENCODE_ERROR(2)
#define EFI_UNSUPPORTED        ENCODE_ERROR(3)
#define EFI_BUFFER_TOO_SMALL   ENCODE_ERROR(5)
#define EFI_NOT_FOUND          ENCODE_ERROR(14)
#define EFI_ACCESS_DENIED      ENCODE_ERROR(15)

#define EFI_ERROR(n)           RETURN_ERROR(n)

// Basic data type definitions introduced in UEFI.
struct efi_guid {
	rpmi_uint32_t data1;
	rpmi_uint16_t data2;
	rpmi_uint16_t data3;
	rpmi_uint8_t data4[8];
};

#define GUID_LENGTH  16

enum efi_mm_header_guid {
	EFI_MM_HDR_GUID_UNSUPPORTED,
	EFI_MM_HDR_GUID_NONE = EFI_MM_HDR_GUID_UNSUPPORTED,
	EFI_MM_VAR_PROTOCOL_GUID,
};

#define EFI_MM_HDR_GUID_NONE_DATA	\
	{ 0x00000000, 0x0000, 0x0000,	\
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

#define EFI_MM_VAR_PROTOCOL_GUID_DATA	\
	{ 0xed32d533, 0x99e6, 0x4209,	\
	  { 0x9c, 0xc0, 0x2d, 0x72, 0xcd, 0xd9, 0x98, 0xa7 } }

#pragma pack(1)

/**
 * struct efi_mm_comm_header - Header used for MM communication
 *
 * @hdr_guid:  guid used for disambiguation of message format
 * @msg_len:   size of data (in bytes) - does not include the size of the header
 * @data:      an array of bytes that is msg_len in size
 *
 * To avoid confusion in interpreting frames, the communication buffer should
 * always begin with struct efi_mm_comm_header.
 */
struct efi_mm_comm_header {
	struct efi_guid hdr_guid;
	rpmi_uint64_t msg_len;
	rpmi_uint8_t data[1];
};

#pragma pack()

// The payload for this function is struct mm_var_comm_access_variable
#define MM_VAR_FN_GET_VARIABLE  1

#define MM_VAR_FN_GET_NEXT_VARIABLE_NAME  2

// The payload for this function is struct mm_var_comm_access_variable
#define MM_VAR_FN_SET_VARIABLE  3

#define MM_VAR_FN_QUERY_VARIABLE_INFO  4

#define MM_VAR_FN_READY_TO_BOOT  5

#define MM_VAR_FN_EXIT_BOOT_SERVICE  6

#define MM_VAR_FN_GET_STATISTICS  7

#define MM_VAR_FN_LOCK_VARIABLE  8

#define MM_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_SET  9

#define MM_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_GET  10

// The payload for this function is struct mm_var_comm_get_payload_size
#define MM_VAR_FN_GET_PAYLOAD_SIZE  11

#define MM_VAR_FN_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT  12

#define MM_VAR_FN_SYNC_RUNTIME_CACHE  13

#define MM_VAR_FN_GET_RUNTIME_CACHE_INFO  14

// Size of MM communicate header, without including the payload
#define MM_COMM_HEADER_SIZE  (offsetof(struct efi_mm_comm_header, data))

// Size of MM variable communicate header, without including the payload
#define MM_VAR_COMM_HEADER_SIZE  (offsetof(struct mm_var_comm_header, data))

/*
 * This structure is used for MM variable. The communication buffer should be:
 *      struct efi_mm_comm_header + struct mm_var_comm_header + payload
 */
struct mm_var_comm_header {
	rpmi_uint64_t function;
	rpmi_uint64_t return_status;
	rpmi_uint8_t data[1];
};

// This structure is used to communicate with MM via SetVariable & GetVariable.
struct mm_var_comm_access_variable {
	struct efi_guid guid;
	rpmi_uint64_t datasize;
	rpmi_uint64_t namesize;
	rpmi_uint32_t attr;
	rpmi_uint16_t name[1];
};

enum var_store_var_data_type {
	STORE_TYPE_RAM_DATA_TYPE_RAW,
	STORE_TYPE_RAM_DATA_TYPE_EDK2FLASH,
	STORE_TYPE_FLASH_DATA_TYPE_RAW,
	STORE_TYPE_FLASH_DATA_TYPE_EDK2FLASH,
};

struct mm_var_comm_get_payload_size {
	rpmi_uint64_t var_payload_size;
};

#endif /* __RPMI_MM_COMMUNICATION_H__ */
