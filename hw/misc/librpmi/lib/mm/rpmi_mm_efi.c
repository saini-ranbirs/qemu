// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "rpmi_mm_common.h"
#include "rpmi_mm_efi.h"

struct mm_efi_comm_header_guid {
	enum mm_efi_header_guid name;
	struct efi_guid guid;
};

struct mm_efi_comm_header_guid mm_comm_hdr_guid_lut[] = {
	[0] { MM_EFI_HDR_GUID_NONE, MM_EFI_HDR_GUID_NONE_DATA },
	[1] { MM_EFI_VAR_PROTOCOL_GUID, MM_EFI_VAR_PROTOCOL_GUID_DATA },
	[2] { MM_EFI_VAR_POLICY_GUID, MM_EFI_VAR_POLICY_GUID_DATA },
	[3] { MM_EFI_END_OF_DXE_GUID, MM_EFI_END_OF_DXE_GUID_DATA },
	[4] { MM_EFI_READY_TO_BOOT_GUID, MM_EFI_READY_TO_BOOT_GUID_DATA },
	[5] { MM_EFI_EXIT_BOOT_SVC_GUID, MM_EFI_EXIT_BOOT_SVC_GUID_DATA },
};

static rpmi_uint8_t payload_buffer[MAX_PAYLOAD_SIZE];

#define MAX_TRANSFER_SIZE  (16 * 1024)	/* 16 KB */
rpmi_uint8_t msg_buffer[MAX_TRANSFER_SIZE];

#ifdef DEBUG

static rpmi_uint16_t efi_calls_counter = 0;

#define STRING_CASE(x)  case x: return #x

static const char *get_hdr_guid_string(enum mm_efi_header_guid guid)
{
	switch (guid) {
		STRING_CASE(MM_EFI_VAR_PROTOCOL_GUID);
		STRING_CASE(MM_EFI_VAR_POLICY_GUID);
		STRING_CASE(MM_EFI_END_OF_DXE_GUID);
		STRING_CASE(MM_EFI_READY_TO_BOOT_GUID);
		STRING_CASE(MM_EFI_EXIT_BOOT_SVC_GUID);

	default:
		STRING_CASE(MM_EFI_HDR_GUID_UNSUPPORTED);
	}
}

static const char *null_string = "NULL";

static const char *get_var_fn_string(rpmi_uint32_t function_code)
{
	switch (function_code) {
		STRING_CASE(EFI_VAR_FN_GET_VARIABLE);
		STRING_CASE(EFI_VAR_FN_GET_NEXT_VARIABLE_NAME);
		STRING_CASE(EFI_VAR_FN_SET_VARIABLE);
		STRING_CASE(EFI_VAR_FN_QUERY_VARIABLE_INFO);
		STRING_CASE(EFI_VAR_FN_READY_TO_BOOT);
		STRING_CASE(EFI_VAR_FN_EXIT_BOOT_SERVICE);
		STRING_CASE(EFI_VAR_FN_GET_STATISTICS);
		STRING_CASE(EFI_VAR_FN_LOCK_VARIABLE);
		STRING_CASE(EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_SET);
		STRING_CASE(EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_GET);
		STRING_CASE(EFI_VAR_FN_GET_PAYLOAD_SIZE);
		STRING_CASE(EFI_VAR_FN_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT);
		STRING_CASE(EFI_VAR_FN_SYNC_RUNTIME_CACHE);
		STRING_CASE(EFI_VAR_FN_GET_RUNTIME_CACHE_INFO);

	default:
		return null_string;
	}
}

#endif /* DEBUG */

static inline int get_guid_index(const rpmi_uint8_t *guid,
				 rpmi_uint16_t msg_len)
{
	rpmi_uint8_t i;

	for (i = 1; i < array_size(mm_comm_hdr_guid_lut); i++) {
		if (rpmi_env_memcmp((void *)guid, &mm_comm_hdr_guid_lut[i].guid,
				    msg_len))
			continue;

		return i;
	}

	return 0;
}

static rpmi_uint64_t validate_input(struct efi_var_comm_header *comm_hdr,
				    rpmi_uint32_t payload_size,
				    rpmi_bool_t is_context_get_variable)
{
	struct efi_var_access_variable *var;
	rpmi_uint64_t infosize;

	if (payload_size < offsetof(struct efi_var_access_variable, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct efi_var_access_variable *)payload_buffer;

	/* Prevent infosize overflow */
	if ((((rpmi_uint64_t)(~0) - var->datasize) <
	     offsetof(struct efi_var_access_variable, name))
	    ||
	    (((rpmi_uint64_t)(~0) - var->namesize) <
	     offsetof(struct efi_var_access_variable, name) +
	     var->datasize)) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize = offsetof(struct efi_var_access_variable, name) +
	    var->datasize + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	/* Ensure Variable Name is a Null-terminated string */
	if ((var->namesize < sizeof(rpmi_uint16_t)) ||
	    (var->name[var->namesize / sizeof(rpmi_uint16_t) - 1] != L'\0')) {
		DPRINTF("Variable Name NOT Null-terminated !!!");
		return EFI_ACCESS_DENIED;
	}

	if (is_context_get_variable && (var->name[0] == 0))
		return EFI_INVALID_PARAMETER;

	return EFI_SUCCESS;
}

static rpmi_uint64_t fn_get_variable(struct rpmi_mm_efi *mmefi,
				     struct efi_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_input(comm_hdr, payload_size, true);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->get_variable(mmefi->ops_priv, comm_hdr->data,
					payload_size);
}

static rpmi_uint64_t validate_name(struct efi_var_comm_header *comm_hdr,
				   rpmi_uint32_t payload_size)
{
	struct efi_var_get_next_var_name *var;
	rpmi_uint64_t infosize, max_len;

	if (payload_size < offsetof(struct efi_var_get_next_var_name, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct efi_var_get_next_var_name *)payload_buffer;

	/*
	 * Calculate the possible maximum length of name string, including the
	 * Null-terminator.
	 */
	max_len = var->namesize / sizeof(rpmi_uint16_t);
	if (!max_len ||
	    (rpmi_env_strnlen((const char *)var->name, max_len) == max_len)) {
		/*
		 * Null-terminator is not found in the first namesize bytes of
		 * the name buffer, follow spec to return EFI_INVALID_PARAMETER.
		 */
		return EFI_INVALID_PARAMETER;
	}

	/* Prevent infosize overflow */
	if (((rpmi_uint64_t)(~0) - var->namesize) <
	    offsetof(struct efi_var_get_next_var_name, name) +
	    var->namesize) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize =
	    offsetof(struct efi_var_access_variable, name) + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	return EFI_SUCCESS;
}

static rpmi_uint64_t fn_get_next_var_name(struct rpmi_mm_efi *mmefi,
					  struct efi_var_comm_header *comm_hdr,
					  rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_name(comm_hdr, payload_size);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->get_next_variable_name(mmefi->ops_priv,
						  comm_hdr->data, payload_size);
}

static rpmi_uint64_t fn_set_variable(struct rpmi_mm_efi *mmefi,
				     struct efi_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_input(comm_hdr, payload_size, false);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->set_variable(mmefi->ops_priv, comm_hdr->data,
					payload_size);
}

static inline rpmi_uint64_t fn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
						rpmi_uint32_t payload_size)
{
	struct efi_var_get_payload_size *get_payload_size;

	if (payload_size < sizeof(struct efi_var_get_payload_size))
		return EFI_INVALID_PARAMETER;

	get_payload_size = (struct efi_var_get_payload_size *)comm_hdr_data;
	get_payload_size->var_payload_size = MAX_PAYLOAD_SIZE;

	return EFI_SUCCESS;
}

static enum rpmi_error efi_var_fn_handler(struct rpmi_mm *mm,
					  void *comm_buf, rpmi_uint64_t bufsize)
{
	rpmi_uint64_t status = EFI_SUCCESS, payload_size;
	struct efi_var_comm_header *var_comm_hdr;

	if (comm_buf == NULL) {
		DPRINTF("Nothing to do.");
		return RPMI_SUCCESS;
	}

	if (bufsize < EFI_VAR_COMM_HEADER_SIZE) {
		DPRINTF("MM comm buffer size invalid !!!");
		return RPMI_SUCCESS;
	}

	payload_size = bufsize - EFI_VAR_COMM_HEADER_SIZE;
	DPRINTF("bufsize = %ld hdrsize = %ld payload_size = %ld",
		bufsize, EFI_VAR_COMM_HEADER_SIZE, payload_size);

	if (payload_size > MAX_PAYLOAD_SIZE) {
		DPRINTF("MM comm buffer payload size invalid > %ld !!!",
			MAX_PAYLOAD_SIZE);
		return RPMI_SUCCESS;
	}

	rpmi_env_memset(payload_buffer, 0x00, MAX_PAYLOAD_SIZE);
	var_comm_hdr = (struct efi_var_comm_header *)comm_buf;

	switch (var_comm_hdr->function) {
	case EFI_VAR_FN_GET_VARIABLE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_get_variable(&mm->u.svc_efi, var_comm_hdr,
					 payload_size);
		break;

	case EFI_VAR_FN_GET_NEXT_VARIABLE_NAME:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_get_next_var_name(&mm->u.svc_efi, var_comm_hdr,
					      payload_size);
		break;

	case EFI_VAR_FN_SET_VARIABLE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_set_variable(&mm->u.svc_efi, var_comm_hdr,
					 payload_size);
		break;

	case EFI_VAR_FN_GET_PAYLOAD_SIZE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_get_payload_size(var_comm_hdr->data, payload_size);
		break;

	case EFI_VAR_FN_READY_TO_BOOT:
	case EFI_VAR_FN_EXIT_BOOT_SERVICE:
		DPRINTF("Processing (dummy) %s",
			get_var_fn_string(var_comm_hdr->function));
		status = EFI_SUCCESS;
		break;

	default:
		status = EFI_UNSUPPORTED;
		DPRINTF("%s not supported",
			get_var_fn_string(var_comm_hdr->function));
		break;
	}

	var_comm_hdr->return_status = status;

	return RPMI_SUCCESS;
}

enum rpmi_error mm_efi_communicate(struct rpmi_mm *mm,
				   struct rpmi_service *service,
				   struct rpmi_transport *xport,
				   rpmi_uint16_t request_datalen,
				   const rpmi_uint8_t *request_data,
				   rpmi_uint16_t *response_datalen,
				   rpmi_uint8_t *response_data)
{
	struct efi_var_policy_comm_header *policy_hdr;
	struct mm_efi_comm_header *mm_comm_hdr, *msg;
	rpmi_uint32_t *rsp = (void *)response_data;
	struct rpmi_mm_comm_req *mmc_req;
	rpmi_uint64_t msg_len, mm_addr;
	rpmi_uint64_t status;
	rpmi_uint8_t index;

	if (!request_data)
		return RPMI_ERR_NO_DATA;

	if (mm->svc_type != RPMI_MM_SERVICE_EFI)
		return RPMI_ERR_NO_DATA;

	mmc_req = (struct rpmi_mm_comm_req *)request_data;
	mm_addr = mm->shmem_addr + mmc_req->idata_off;

	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)&msg_buffer,
		       sizeof(struct mm_efi_comm_header));

	mm_comm_hdr = (struct mm_efi_comm_header *)msg_buffer;
	msg_len =
	    offsetof(struct mm_efi_comm_header, data) + mm_comm_hdr->msg_len;

	msg = (struct mm_efi_comm_header *)msg_buffer;
	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)msg, msg_len);

	index = get_guid_index((rpmi_uint8_t *)&msg->hdr_guid,
			       sizeof(msg->hdr_guid));

	switch (mm_comm_hdr_guid_lut[index].name) {
	case MM_EFI_VAR_PROTOCOL_GUID:
		DPRINTF("Handling header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = efi_var_fn_handler(mm, &msg->data, msg_len);
		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case MM_EFI_VAR_POLICY_GUID:
		DPRINTF("Handling (dummy) header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = RPMI_SUCCESS;

		policy_hdr = (struct efi_var_policy_comm_header *)&msg->data;
		policy_hdr->result = 0x00;

		/* Maintain msg_len as next multiple of GUID_LENGTH/ 16 bytes */
		msg_len = offsetof(struct mm_efi_comm_header, data) +
		    sizeof(*policy_hdr);
		msg_len = msg_len + GUID_LENGTH - 1;
		msg_len = msg_len / GUID_LENGTH;
		msg_len = msg_len * GUID_LENGTH;

		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case MM_EFI_END_OF_DXE_GUID:
	case MM_EFI_READY_TO_BOOT_GUID:
	case MM_EFI_EXIT_BOOT_SVC_GUID:
		DPRINTF("Handling (dummy) header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = RPMI_SUCCESS;
		msg_len = 0;
		break;

	default:
		DPRINTF("Header guid %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = RPMI_ERR_NO_DATA;
		msg_len = 0;
		break;
	}

	rsp[1] = rpmi_to_xe32(xport->is_be, msg_len);

	return status;
}
