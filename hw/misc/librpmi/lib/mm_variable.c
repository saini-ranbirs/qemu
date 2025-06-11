// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stddef.h>
#include "librpmi_internal.h"
#include "mm_variable.h"
#include "EDK2RT_VariableParsing.h"

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

EFI_GUID gEfiVariableGuid = EFI_VARIABLE_GUID;
EFI_GUID gEfiAuthenticatedVariableGuid = EFI_AUTHENTICATED_VARIABLE_GUID;

void mm_memdump(const void *src, rpmi_size_t count)
{
#define BFR_DATA_LIMIT  16

	const char *temp = src;
	unsigned char bfr_data[BFR_DATA_LIMIT];
	size_t bfr_counter, bfr_counter_limit;
	size_t remaining = count, loop_count = 0;

	if (!remaining)
		return;

	while (remaining) {
		bfr_counter = 0;

		(remaining >= BFR_DATA_LIMIT) ?
		    (bfr_counter_limit = BFR_DATA_LIMIT) :
		    (bfr_counter_limit = remaining);

		while (bfr_counter < bfr_counter_limit) {
			bfr_data[bfr_counter] = *(temp + bfr_counter);
			bfr_counter++;
		}

		/* For simplicity, fill rest with ZERO's if required */
		while (bfr_counter < BFR_DATA_LIMIT) {
			bfr_data[bfr_counter] = 0x00;
			bfr_counter++;
		}

		if (loop_count < 25) {
		DPRINTF("\n%06lu "
			"%02X%02X %02X%02X %02X%02X %02X%02X "
			"%02X%02X %02X%02X %02X%02X %02X%02X",
			count - remaining,
			bfr_data[1], bfr_data[0], bfr_data[3], bfr_data[2],
			bfr_data[5], bfr_data[4], bfr_data[7], bfr_data[6],
			bfr_data[9], bfr_data[8], bfr_data[11], bfr_data[10],
			bfr_data[13], bfr_data[12], bfr_data[15], bfr_data[14]);
		}

		temp = temp + bfr_counter_limit;
		remaining = remaining - bfr_counter_limit;
		loop_count++;
	}

	DPRINTF("\n%06lu\n", count - remaining);
}

enum rpmi_error mm_variable_init(void)
{
	m_var_bfr_payload_size =
	    max_variable_size +
	    offsetof(SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)
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
	get_payload_size->VariablePayloadSize = m_var_bfr_payload_size;

	DPRINTF("====================================================> "
		"%s: get_payload_size->variable_payload_size = 0x%x \n",
		__func__, m_var_bfr_payload_size);

	return EFI_SUCCESS;
}

static int get_variable_from_variable_store(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr)
{
	rpmi_uint8_t svar_data[786432];

	get_data_from_secure_variable_fd(svar_data);

	// Vendor Guid
	DPRINTF("====================================================> "
		"%s: Vendor Guid \n", __func__);
	mm_memdump((void *)&smm_var_hdr->Guid, sizeof(smm_var_hdr->Guid));

	DPRINTF("====================================================> "
		"%s: Var Name Size = %ld \n", __func__, smm_var_hdr->NameSize);

	DPRINTF("====================================================> "
		"%s: Var Name \n", __func__);
	mm_memdump(smm_var_hdr->Name, smm_var_hdr->NameSize);

	DPRINTF("====================================================> "
		"%s: Flash dump follows \n", __func__);
	mm_memdump((void *)svar_data, 0x170);

	/*{
		EFI_FIRMWARE_VOLUME_HEADER *fvh;
		VARIABLE_STORE_HEADER *vsh;

		fvh = (EFI_FIRMWARE_VOLUME_HEADER *)svar_data;
		vsh = (VARIABLE_STORE_HEADER *)&svar_data[72];

		DPRINTF("secure-var: FVH = %p VSH = %p\n",
			(void *)fvh, (void *)vsh);
		DPRINTF("secure-var: Store Status = %d\n",
			GetVariableStoreStatus(vsh));
	}*/

	DPRINTF("====================================================> "
		"%s: Start Pointer = %p \n", __func__,
		GetStartPointer((VARIABLE_STORE_HEADER *)&svar_data[72]));
	mm_memdump((void *)&svar_data[72], 0x170 - 72);
	DPRINTF("====================================================> "
		"%s: End   Pointer = %p, \n", __func__,
		GetEndPointer((VARIABLE_STORE_HEADER *)&svar_data[72]));

	//FindVariableEx();

	return 0;
}

static inline int fxn_get_variable(rpmi_uint8_t *comm_hdr_data,
				   rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr;
	rpmi_uint64_t info_size;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);

	DPRINTF("====================================================> "
		"smm_var_comm_hdr->function = "
		"SMM_VARIABLE_FUNCTION_GET_VARIABLE %lu \n",
		sizeof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE));

	if (bfr_payload_size <
	    offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name))
		return EFI_INVALID_PARAMETER;

	smm_var_hdr =
	    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)m_var_bfr_payload;

	//
	// Prevent info_size overflow
	//
	if (((rpmi_uint64_t)(~0) -
	     smm_var_hdr->DataSize <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) ||
	    ((rpmi_uint64_t)(~0) -
	     smm_var_hdr->NameSize <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
	     smm_var_hdr->DataSize))
		return EFI_INVALID_PARAMETER;

	info_size = offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		    smm_var_hdr->DataSize + smm_var_hdr->NameSize;

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
	if ((smm_var_hdr->NameSize < sizeof(rpmi_int16_t)) ||
	    (smm_var_hdr->Name[smm_var_hdr->NameSize / sizeof(rpmi_int16_t) - 1] != '\0')) {
		//
		// Make sure VariableName is A Null-terminated string.
		//
		return EFI_INVALID_PARAMETER;
	}

	get_variable_from_variable_store(smm_var_hdr);

	DPRINTF("====================================================> "
		"%s: EXIT %d \n", __func__, __LINE__);

	//CopyMem (SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);

	return EFI_NOT_FOUND;
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
	comm_bfr_payload_size =
	    *comm_bfr_size - sizeof(SMM_VARIABLE_COMMUNICATE_HEADER);

	if (comm_bfr_payload_size > m_var_bfr_payload_size) {
		DPRINTF("====================================================> "
			"%s: MM comm buffer payload size invalid!\n", __func__);
		return RPMI_SUCCESS;
	}

	DPRINTF("====================================================> "
		"%s: smm_var_comm_hdr->function = 0x%lx \n",
		__func__, smm_var_comm_hdr->Function);

	mm_memdump(smm_var_comm_hdr, *comm_bfr_size);

	DPRINTF("====================================================> "
		"%s: comm_bfr_payload_size = %d \n",
		__func__, comm_bfr_payload_size);

	mm_memdump(smm_var_comm_hdr->Data, comm_bfr_payload_size);

	switch (smm_var_comm_hdr->Function) {
	case SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE:
		status = fxn_get_payload_size(smm_var_comm_hdr->Data,
					      comm_bfr_payload_size);
		break;

	case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
		memcpy(m_var_bfr_payload, smm_var_comm_hdr->Data,
		       comm_bfr_payload_size);
		status = fxn_get_variable(smm_var_comm_hdr->Data,
					  comm_bfr_payload_size);
		if (status == EFI_SUCCESS)
			memcpy(smm_var_comm_hdr->Data, m_var_bfr_payload,
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

	smm_var_comm_hdr->ReturnStatus = status;

	DPRINTF("====================================================> "
		"%s: EXIT \n", __func__);

	return RPMI_SUCCESS;
}
