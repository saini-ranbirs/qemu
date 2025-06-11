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

#define IN
#define OUT
#define EFIAPI
#define ASSERT(x)

#define VOID     void
#define CONST    const
#define BOOLEAN  rpmi_bool_t
#define TRUE     true
#define FALSE    false

#define INT8     rpmi_int8_t
#define INT16    rpmi_int16_t
#define INT32    rpmi_int32_t
#define INT64    rpmi_int64_t

#define UINT8    rpmi_uint8_t
#define UINT16   rpmi_uint16_t
#define UINT32   rpmi_uint32_t
#define UINT64   rpmi_uint64_t
#define UINTN    rpmi_uint64_t
#define CHAR16   rpmi_uint16_t

///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef INT64 INTN;

#define AtRuntime()    FALSE
#define ZeroMem(a, b)  rpmi_env_memset(a, 0, b)

//
// Basical data type definitions introduced in UEFI.
//
typedef struct {
  UINT32  Data1;
  UINT16  Data2;
  UINT16  Data3;
  UINT8   Data4[8];
} EFI_GUID;

#define GUID         EFI_GUID
#define GUID_LENGTH  16

//
// Status codes common to all execution phases
//
typedef UINTN RETURN_STATUS;

typedef RETURN_STATUS  EFI_STATUS;

#define RETURN_ERROR(StatusCode)  (((INTN)(RETURN_STATUS)(StatusCode)) < 0)

#define EFI_ERROR(A)  RETURN_ERROR(A)

///
/// EFI Time Abstraction:
///  Year:       1900 - 9999
///  Month:      1 - 12
///  Day:        1 - 31
///  Hour:       0 - 23
///  Minute:     0 - 59
///  Second:     0 - 59
///  Nanosecond: 0 - 999,999,999
///  TimeZone:   -1440 to 1440 or 2047
///
typedef struct {
  UINT16    Year;
  UINT8     Month;
  UINT8     Day;
  UINT8     Hour;
  UINT8     Minute;
  UINT8     Second;
  UINT8     Pad1;
  UINT32    Nanosecond;
  INT16     TimeZone;
  UINT8     Daylight;
  UINT8     Pad2;
} EFI_TIME;

//
// This structure is used for SMM variable. the collected statistics data is saved in SMRAM. It can be got from
// SMI handler. The communication buffer should be:
// EFI_MM_COMMUNICATE_HEADER + SMM_VARIABLE_COMMUNICATE_HEADER + payload.
//
typedef struct {
  UINTN         Function;
  EFI_STATUS    ReturnStatus;
  UINT8         Data[1];
} SMM_VARIABLE_COMMUNICATE_HEADER;

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

///
/// Size of SMM communicate header, without including the payload.
///
#define SMM_COMMUNICATE_HEADER_SIZE  (OFFSET_OF (EFI_MM_COMMUNICATE_HEADER, Data))

///
/// Size of SMM variable communicate header, without including the payload.
///
#define SMM_VARIABLE_COMMUNICATE_HEADER_SIZE  (OFFSET_OF (SMM_VARIABLE_COMMUNICATE_HEADER, Data))

///
/// This structure is used to communicate with SMI handler by SetVariable and GetVariable.
///
typedef struct {
  EFI_GUID    Guid;
  UINTN       DataSize;
  UINTN       NameSize;
  UINT32      Attributes;
  CHAR16      Name[1];
} SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE;

typedef struct {
  UINT16    Revision;
  UINT16    Property;
  UINT32    Attributes;
  UINTN     MinSize;
  UINTN     MaxSize;
} VAR_CHECK_VARIABLE_PROPERTY;

typedef struct {
  EFI_GUID                       Guid;
  UINTN                          NameSize;
  VAR_CHECK_VARIABLE_PROPERTY    VariableProperty;
  CHAR16                         Name[1];
} SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY;

typedef struct {
  UINTN    VariablePayloadSize;
} SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE;

#include "EDK2RT_VariableFormat.h"

//
// Attributes of variable.
//
#define EFI_VARIABLE_NON_VOLATILE                 0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS           0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS               0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD        0x00000008

typedef enum {
  VariableStoreTypeVolatile,
  VariableStoreTypeHob,
  VariableStoreTypeNv,
  VariableStoreTypeMax
} VARIABLE_STORE_TYPE;

typedef struct {
  VARIABLE_HEADER    *CurrPtr;
  //
  // If both ADDED and IN_DELETED_TRANSITION variable are present,
  // InDeletedTransitionPtr will point to the IN_DELETED_TRANSITION one.
  // Otherwise, CurrPtr will point to the ADDED or IN_DELETED_TRANSITION one,
  // and InDeletedTransitionPtr will be NULL at the same time.
  //
  VARIABLE_HEADER    *InDeletedTransitionPtr;
  VARIABLE_HEADER    *EndPtr;
  VARIABLE_HEADER    *StartPtr;
  BOOLEAN            Volatile;
} VARIABLE_POINTER_TRACK;

enum rpmi_error mm_variable_init(void);
enum rpmi_error mm_variable_handler(rpmi_uint8_t *comm_buffer,
				    rpmi_uint32_t *comm_buffer_size);

void mm_memdump(const void *src, rpmi_size_t count);

int get_data_from_secure_variable_fd(rpmi_uint8_t *data);

#endif /* __RPMI_MM_VARIABLE_H__ */
