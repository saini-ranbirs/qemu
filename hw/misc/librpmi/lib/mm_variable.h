/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_MM_VARIABLE_H__
#define __RPMI_MM_VARIABLE_H__

#include "librpmi_internal.h"
#undef NULL

#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Guid/VariableFormat.h>

#define ENABLE_RS_SVA   1
#define ENABLE_DEBUG    1

#ifdef ENABLE_DEBUG

#include "stdio.h"
#include "stdarg.h"
#include "string.h"

#define DPRINTF(msg...)         rpmi_env_printf(msg)

#else

#define DPRINTF(msg...)

#endif

#define ASSERT(x)

#define AtRuntime()     FALSE
#define CopyMem         rpmi_env_memcpy
#define memdump         mm_memdump

#define StrnCpyS(d, s, n)  (EFI_STATUS)rpmi_env_memcpy((void *)d, (const void *)s, (rpmi_size_t)n)

#define GUID_LENGTH     16


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
enum rpmi_error mm_variable_handler(IN OUT VOID   *CommBuffer,
				    IN OUT UINTN  *CommBufferSize);

void mm_memdump(const void *src, rpmi_size_t count, const char *name);

int get_data_from_secure_variable_fd(rpmi_uint8_t *data);

EFI_STATUS
FindVariableRS (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid
  );

EFI_STATUS
EFIAPI
VariableServiceGetVariable (
  IN      CHAR16    *VariableName,
  IN      EFI_GUID  *VendorGuid,
  OUT     UINT32    *Attributes OPTIONAL,
  IN OUT  UINTN     *DataSize,
  OUT     VOID      *Data OPTIONAL
  );

EFI_STATUS
EFIAPI
VariableServiceGetNextVariableInternal (
  IN  CHAR16                 *VariableName,
  IN  EFI_GUID               *VendorGuid,
  IN  VARIABLE_STORE_HEADER  **VariableStoreList,
  OUT VARIABLE_HEADER        **VariablePtr,
  IN  BOOLEAN                AuthFormat
  );

EFI_STATUS
EFIAPI
VariableServiceGetNextVariableName (
  IN OUT  UINTN     *VariableNameSize,
  IN OUT  CHAR16    *VariableName,
  IN OUT  EFI_GUID  *VendorGuid
  );

#endif /* __RPMI_MM_VARIABLE_H__ */
