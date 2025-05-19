// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stddef.h>
#include "librpmi_internal.h"
#include "mm_variable.h"

#define DEBUG  1

#ifdef DEBUG

#include "stdio.h"
#include "stdarg.h"
#include "string.h"

#define DPRINTF(msg...)         rpmi_env_printf(msg)

#else

#define DPRINTF(msg...)

#endif

static rpmi_uint32_t max_variable_size = 0x2800;
static rpmi_uint8_t* m_var_bfr_payload = NULL;
static rpmi_uint32_t m_var_bfr_payload_size = 0;

extern char *riscv_var_fd;
static int check_vard_fd_access = true;

enum rpmi_error mm_variable_init(void)
{
	m_var_bfr_payload_size =
	    max_variable_size +
	    offsetof(SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, name)
	    - sizeof(AUTHENTICATED_VARIABLE_HEADER);

	m_var_bfr_payload = rpmi_env_zalloc(m_var_bfr_payload_size);
	if (!m_var_bfr_payload) {
		DPRINTF("====================================================> "
			"%s() failed to allocate memory \n", __func__ );
		return RPMI_ERR_FAILED;
	}

	DPRINTF("====================================================> "
		"%s() memory allocated for buffer \n", __func__ );

	return RPMI_SUCCESS;
}

static inline int fxn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
				       rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *get_payload_size;

	DPRINTF("====================================================> "
		"smm_var_comm_hdr->function = "
		"SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE.\n" );

	if (bfr_payload_size <
	    sizeof(SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE))
		return EFI_INVALID_PARAMETER;

	get_payload_size =
	    (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *)comm_hdr_data;
	get_payload_size->variable_payload_size = m_var_bfr_payload_size;

	DPRINTF("====================================================> "
		"%s: get_payload_size->variable_payload_size = 0x%x \n",
		__func__, m_var_bfr_payload_size);

	return EFI_SUCCESS;
}

static inline int fxn_get_variable(rpmi_uint8_t *comm_hdr_data,
				   rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr;
	rpmi_uint64_t info_size;

	DPRINTF("====================================================> "
		"smm_var_comm_hdr->function = "
		"SMM_VARIABLE_FUNCTION_GET_VARIABLE %lu \n",
		sizeof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE));

	if (bfr_payload_size <
	    offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, name))
		return EFI_INVALID_PARAMETER;

	smm_var_hdr =
	    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)m_var_bfr_payload;

	//
	// Prevent info_size overflow
	//
	if (((rpmi_uint64_t)(~0) -
	     smm_var_hdr->data_size <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, name)) ||
	    ((rpmi_uint64_t)(~0) -
	     smm_var_hdr->name_size <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, name) +
	     smm_var_hdr->data_size))
		return EFI_INVALID_PARAMETER;

	info_size = offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, name) +
		    smm_var_hdr->data_size + smm_var_hdr->name_size;

	//
	// range check already covered before
	//
	if (info_size > bfr_payload_size)
		return EFI_INVALID_PARAMETER;

	//
	// The VariableSpeculationBarrier() call here is to ensure the previous
	// range/content checks for the CommBuffer have been completed before the
	// subsequent consumption of the CommBuffer content.
	//
	//VariableSpeculationBarrier ();
	if ((smm_var_hdr->name_size < sizeof(rpmi_int16_t)) ||
	    (smm_var_hdr->name[smm_var_hdr->name_size / sizeof(rpmi_int16_t) - 1] != '\0')) {
		//
		// Make sure VariableName is A Null-terminated string.
		//
		return EFI_INVALID_PARAMETER;
	}

	DPRINTF("====================================================> "
		"%s: Success %d \n", __func__, __LINE__);

	//CopyMem (SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);

	return EFI_NOT_FOUND;
}

rpmi_bool_t
IsValidVariableHeader (
  VARIABLE_HEADER  *Variable,
  VARIABLE_HEADER  *VariableStoreEnd
  )
{
  if ((Variable == NULL) || (Variable >= VariableStoreEnd) || (Variable->StartId != VARIABLE_DATA)) {
    //
    // Variable is NULL or has reached the end of variable store,
    // or the StartId is not correct.
    //
    return false;
  }

  return true;
}

#define EFI_SMM_VARIABLE_VARIABLE_GUID_DATA	\
	{ 0xddcf3616, 0x3275, 0x4164,		\
	  { 0x98, 0xb6, 0xfe, 0x85, 0x70, 0x7f, 0xfe, 0x7d } }

#define EFI_SMM_VARIABLE_AUTH_VARIABLE_GUID_DATA	\
	{ 0xaaf32c78, 0x947b, 0x439a,			\
	  { 0xa1, 0x80, 0x2e, 0x14, 0x4e, 0xc3, 0x77, 0x92 } }

EFI_GUID gEfiVariableGuid = EFI_SMM_VARIABLE_VARIABLE_GUID_DATA;
EFI_GUID gEfiAuthVariableGuid = EFI_SMM_VARIABLE_AUTH_VARIABLE_GUID_DATA;

VARIABLE_STORE_STATUS
GetVariableStoreStatus (
  VARIABLE_STORE_HEADER  *VarStoreHeader
  )
{
  if ((memcmp (&VarStoreHeader->Signature, &gEfiAuthVariableGuid, sizeof(EFI_GUID)) ||
       memcmp (&VarStoreHeader->Signature, &gEfiVariableGuid, sizeof(EFI_GUID))) &&
      (VarStoreHeader->Format == VARIABLE_STORE_FORMATTED) &&
      (VarStoreHeader->State == VARIABLE_STORE_HEALTHY)
      )
  {
    return EfiValid;
  } else if ((((rpmi_uint32_t *)(&VarStoreHeader->Signature))[0] == 0xffffffff) &&
             (((rpmi_uint32_t *)(&VarStoreHeader->Signature))[1] == 0xffffffff) &&
             (((rpmi_uint32_t *)(&VarStoreHeader->Signature))[2] == 0xffffffff) &&
             (((rpmi_uint32_t *)(&VarStoreHeader->Signature))[3] == 0xffffffff) &&
             (VarStoreHeader->Size == 0xffffffff) &&
             (VarStoreHeader->Format == 0xff) &&
             (VarStoreHeader->State == 0xff)
             )
  {
    return EfiRaw;
  } else {
    return EfiInvalid;
  }
}

VARIABLE_HEADER *
GetStartPointer (
  VARIABLE_STORE_HEADER  *VarStoreHeader
  )
{
  //
  // The start of variable store.
  //
  return (VARIABLE_HEADER *)HEADER_ALIGN (VarStoreHeader + 1);
}

static inline int fxn_set_variable(void)
{
	DPRINTF("====================================================> "
		"smm_var_comm_hdr->function = "
		"SMM_VARIABLE_FUNCTION_SET_VARIABLE \n");

	return EFI_SUCCESS;
}

enum rpmi_error mm_variable_handler(rpmi_uint8_t *comm_bfr,
				    rpmi_uint32_t *comm_bfr_size)
{
	SMM_VARIABLE_COMMUNICATE_HEADER *smm_var_comm_hdr;
	rpmi_uint32_t comm_bfr_payload_size;
	rpmi_uint32_t status = EFI_SUCCESS;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);

	if (!comm_bfr || !comm_bfr_size)
		return EFI_INVALID_PARAMETER;

	smm_var_comm_hdr = (SMM_VARIABLE_COMMUNICATE_HEADER *)comm_bfr;
	DPRINTF("====================================================> "
		"%s: smm_var_comm_hdr->function = 0x%lx \n",
		__func__, smm_var_comm_hdr->function);

	comm_bfr_payload_size =
	    *comm_bfr_size - sizeof(SMM_VARIABLE_COMMUNICATE_HEADER);

	if (comm_bfr_payload_size > m_var_bfr_payload_size) {
		DPRINTF("====================================================> "
			"%s: MM comm buffer payload size invalid!\n", __func__);
		return RPMI_SUCCESS;
	}

	switch (smm_var_comm_hdr->function) {
	case SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE:
		status = fxn_get_payload_size(smm_var_comm_hdr->data,
					      comm_bfr_payload_size);
		break;

	case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
		if (!check_vard_fd_access) {
			dump_data_from_secure_variable_fd(riscv_var_fd);
			check_vard_fd_access = false;
		}

		memcpy(m_var_bfr_payload, smm_var_comm_hdr->data,
		       comm_bfr_payload_size);
		status = fxn_get_variable(smm_var_comm_hdr->data,
					  comm_bfr_payload_size);
		if (status == EFI_SUCCESS)
			memcpy(smm_var_comm_hdr->data, m_var_bfr_payload,
			       comm_bfr_payload_size);

		break;

	case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
		status = fxn_set_variable();
		break;

		case SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME:
		case SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO:
		case SMM_VARIABLE_FUNCTION_READY_TO_BOOT:
		case SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE:
		case SMM_VARIABLE_FUNCTION_GET_STATISTICS:
		case SMM_VARIABLE_FUNCTION_LOCK_VARIABLE:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET:
		case SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT:
		case SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE:
		case SMM_VARIABLE_FUNCTION_GET_RUNTIME_CACHE_INFO:
		default:
			break;
	}

	smm_var_comm_hdr->return_status = status;

	return RPMI_SUCCESS;
}
