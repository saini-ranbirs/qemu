/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_MM_VARIABLE_H__
#define __RPMI_MM_VARIABLE_H__

#define EFI_SUCCESS            0
#define EFI_INVALID_PARAMETER  2
#define EFI_NOT_FOUND          14
#define EFI_UNSUPPORTED        0x8000000000000001ull

// The payload for this function is SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE.
//
#define SMM_VARIABLE_FUNCTION_GET_VARIABLE  1
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME.
//
#define SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME  2
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE.
//
#define SMM_VARIABLE_FUNCTION_SET_VARIABLE  3
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO.
//
#define SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO  4
//
// It is a notify event, no extra payload for this function.
//
#define SMM_VARIABLE_FUNCTION_READY_TO_BOOT  5
//
// It is a notify event, no extra payload for this function.
//
#define SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE  6
//
// The payload for this function is VARIABLE_INFO_ENTRY.
// The GUID in EFI_MM_COMMUNICATE_HEADER is gEfiSmmVariableProtocolGuid.
//
#define SMM_VARIABLE_FUNCTION_GET_STATISTICS  7
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE
//
#define SMM_VARIABLE_FUNCTION_LOCK_VARIABLE  8

#define SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET  9

#define SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET  10

#define SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE  11
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT
//
#define SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT  12

#define SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE  13
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_GET_RUNTIME_CACHE_INFO
//
#define SMM_VARIABLE_FUNCTION_GET_RUNTIME_CACHE_INFO  14

//
// Basic data type definitions introduced in UEFI.
//
typedef struct efi_guid_t {
	rpmi_uint32_t  data1;
	rpmi_uint16_t  data2;
	rpmi_uint16_t  data3;
	rpmi_uint8_t   data4[8];
} EFI_GUID;

//
// This structure is used for SMM variable. It can be got from SMI handler.
// The communication buffer should be:
//     EFI_MM_COMMUNICATE_HEADER + SMM_VARIABLE_COMMUNICATE_HEADER + payload.
//
typedef struct {
	rpmi_uint64_t  function;
	rpmi_uint64_t  return_status;
	rpmi_uint8_t   data[1];
} SMM_VARIABLE_COMMUNICATE_HEADER;

typedef struct {
	rpmi_uint64_t  variable_payload_size;
} SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE;

///
/// This structure is used to communicate with SMI handler by SetVariable and GetVariable.
///
typedef struct {
	EFI_GUID       guid;
	rpmi_uint64_t  data_size;
	rpmi_uint64_t  name_size;
	rpmi_uint32_t  attrs;
	rpmi_int16_t   name[1];
} SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE;

//
// EFI Time Abstraction:
//  Year:       2000 - 20XX
//  Month:      1 - 12
//  Day:        1 - 31
//  Hour:       0 - 23
//  Minute:     0 - 59
//  Second:     0 - 59
//  Nanosecond: 0 - 999,999,999
//  TimeZone:   -1440 to 1440 or 2047
//
typedef struct {
  rpmi_uint16_t  year;
  rpmi_uint8_t   month;
  rpmi_uint8_t   day;
  rpmi_uint8_t   hour;
  rpmi_uint8_t   minute;
  rpmi_uint8_t   second;
  rpmi_uint8_t   pad1;
  rpmi_uint32_t  nanosecond;
  rpmi_int16_t   time_zone;
  rpmi_uint8_t   daylight;
  rpmi_uint8_t   pad2;
} EFI_TIME;

///
/// Single Authenticated Variable Data Header Structure.
///
typedef struct {
	///
	/// Variable Data Start Flag.
	///
	rpmi_uint16_t      start_id;
	///
	/// Variable State defined above.
	///
	rpmi_uint8_t       state;
	rpmi_uint8_t       reserved;
	///
	/// Attributes of variable defined in UEFI specification.
	///
	rpmi_uint32_t      attributes;
	///
	/// Associated monotonic count value against replay attack.
	///
	rpmi_uint64_t      monotonic_count;
	///
	/// Associated TimeStamp value against replay attack.
	///
	EFI_TIME           time_stamp;
	///
	/// Index of associated public key in database.
	///
	rpmi_uint32_t      pub_key_index;
	///
	/// Size of variable null-terminated Unicode string name.
	///
	rpmi_uint32_t      name_size;
	///
	/// Size of the variable data without this header.
	///
	rpmi_uint32_t      data_size;
	///
	/// A unique id for the vendor that produces and consumes this variable.
	///
	EFI_GUID           vendor_guid;
} AUTHENTICATED_VARIABLE_HEADER;

typedef struct {
	rpmi_uint16_t  revision;
	rpmi_uint16_t  property;
	rpmi_uint32_t  attributes;
	rpmi_uint64_t  min_size;
	rpmi_uint64_t  max_size;
} VAR_CHECK_VARIABLE_PROPERTY;

typedef struct {
	EFI_GUID                     guid;
	rpmi_uint64_t                name_size;
	VAR_CHECK_VARIABLE_PROPERTY  variable_property;
	rpmi_int16_t                 name[1];
} SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY;

#define VARIABLE_DATA                     0x55AA

//
// Variable Store Header flags
//
#define VARIABLE_STORE_FORMATTED          0x5a
#define VARIABLE_STORE_HEALTHY            0xfe

///
/// Alignment of Variable Data Header in Variable Store region.
///
#define HEADER_ALIGNMENT  4
#define HEADER_ALIGN(Header)  (((rpmi_uint64_t) (Header) + HEADER_ALIGNMENT - 1) & (~(HEADER_ALIGNMENT - 1)))

///
/// Status of Variable Store Region.
///
typedef enum {
	EfiRaw,
	EfiValid,
	EfiInvalid,
	EfiUnknown
} VARIABLE_STORE_STATUS;

#pragma pack(1)

typedef struct {
	EFI_GUID       Signature;
	rpmi_uint32_t  Size;
	rpmi_uint8_t   Format;
	rpmi_uint8_t   State;
	rpmi_uint16_t  Reserved;
	rpmi_uint32_t  Reserved1;
} VARIABLE_STORE_HEADER;

typedef struct {
	rpmi_uint16_t  StartId;
	rpmi_uint8_t   State;
	rpmi_uint8_t   Reserved;
	rpmi_uint32_t  Attributes;
	rpmi_uint32_t  NameSize;
	rpmi_uint32_t  DataSize;
	EFI_GUID       VendorGuid;
} VARIABLE_HEADER;

#pragma pack()

enum rpmi_error mm_variable_init(void);
enum rpmi_error mm_variable_handler(rpmi_uint8_t *comm_buffer,
				    rpmi_uint32_t *comm_buffer_size);

int dump_data_from_secure_variable_fd(const char *svar_fd);

rpmi_bool_t
IsValidVariableHeader (
  VARIABLE_HEADER  *Variable,
  VARIABLE_HEADER  *VariableStoreEnd
  );

VARIABLE_STORE_STATUS
GetVariableStoreStatus (
  VARIABLE_STORE_HEADER  *VarStoreHeader
  );

VARIABLE_HEADER *
GetStartPointer (
  VARIABLE_STORE_HEADER  *VarStoreHeader
  );

#endif /* __RPMI_MM_VARIABLE_H__ */
