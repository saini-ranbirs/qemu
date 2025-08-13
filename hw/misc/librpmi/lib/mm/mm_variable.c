// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm_variable.h"

#define VAR_MAX_INFOSIZE  1024	/* Max information size per MM variable */
#define VAR_MAX_NUM       50	/* Total number of MM variables */

static rpmi_uint8_t var_store[VAR_MAX_NUM][VAR_MAX_INFOSIZE];
static rpmi_uint8_t store_count;
static rpmi_uint8_t store_ptr;
static enum var_store_var_data_type store_n_data_type;

static rpmi_uint8_t *m_var_buf_payload = NULL;
static rpmi_uint32_t m_var_buf_payload_size;

#if defined(ENABLE_DEBUG) && ENABLE_DEBUG

static const char *null_string = "NULL";

#define STRING_CASE(x)  case x: return #x

static const char *get_var_fn_string(rpmi_uint32_t function_code)
{
	switch (function_code) {
		STRING_CASE(MM_VAR_FN_GET_VARIABLE);
		STRING_CASE(MM_VAR_FN_GET_NEXT_VARIABLE_NAME);
		STRING_CASE(MM_VAR_FN_SET_VARIABLE);
		STRING_CASE(MM_VAR_FN_QUERY_VARIABLE_INFO);
		STRING_CASE(MM_VAR_FN_READY_TO_BOOT);
		STRING_CASE(MM_VAR_FN_EXIT_BOOT_SERVICE);
		STRING_CASE(MM_VAR_FN_GET_STATISTICS);
		STRING_CASE(MM_VAR_FN_LOCK_VARIABLE);
		STRING_CASE(MM_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_SET);
		STRING_CASE(MM_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_GET);
		STRING_CASE(MM_VAR_FN_GET_PAYLOAD_SIZE);
		STRING_CASE(MM_VAR_FN_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT);
		STRING_CASE(MM_VAR_FN_SYNC_RUNTIME_CACHE);
		STRING_CASE(MM_VAR_FN_GET_RUNTIME_CACHE_INFO);

	default:
		return null_string;
	}
}

static void mm_memdump(const void *src, rpmi_size_t count, const char *name)
{
#define MAX_BYTES_PER_LINE  16
#define MAX_LINES_PER_DUMP  25

	size_t remaining = count, line_ctr = 0, byte_ctr, byte_ctr_max;
	unsigned char buf_data[MAX_BYTES_PER_LINE];
	const char *temp = src;

	if (!remaining)
		return;

	DPRINTF("MM %s : %06lu", name, count);

	while (remaining) {
		byte_ctr = 0;

		byte_ctr_max = (remaining >= MAX_BYTES_PER_LINE) ?
		    MAX_BYTES_PER_LINE : remaining;

		while (byte_ctr < byte_ctr_max) {
			buf_data[byte_ctr] = *(temp + byte_ctr);
			byte_ctr++;
		}

		/* For simplicity, fill rest with ZERO's if required */
		while (byte_ctr < MAX_BYTES_PER_LINE) {
			buf_data[byte_ctr] = 0x00;
			byte_ctr++;
		}

		if (line_ctr < MAX_LINES_PER_DUMP) {
			DPRINTF("%06lu "
				"%02X%02X %02X%02X %02X%02X %02X%02X "
				"%02X%02X %02X%02X %02X%02X %02X%02X",
				count - remaining,
				buf_data[1], buf_data[0], buf_data[3],
				buf_data[2], buf_data[5], buf_data[4],
				buf_data[7], buf_data[6], buf_data[9],
				buf_data[8], buf_data[11], buf_data[10],
				buf_data[13], buf_data[12], buf_data[15],
				buf_data[14]);
		}

		temp = temp + byte_ctr_max;
		remaining = remaining - byte_ctr_max;
		line_ctr++;
	}

	DPRINTF("%06lu", count - remaining);
}

#else /* !(defined(ENABLE_DEBUG) && ENABLE_DEBUG) */

static void mm_memdump(const void *src, rpmi_size_t count, const char *name)
{
	// Nothing to do
}

#endif /* !(defined(ENABLE_DEBUG) && ENABLE_DEBUG) */

enum rpmi_error mm_variable_init(void)
{
	m_var_buf_payload_size = VAR_MAX_INFOSIZE - MM_VAR_COMM_HEADER_SIZE;

	m_var_buf_payload = rpmi_env_zalloc(m_var_buf_payload_size);
	if (!m_var_buf_payload) {
		DPRINTF("failed to allocate memory of size = %d",
			m_var_buf_payload_size);
		return RPMI_ERR_FAILED;
	}

	store_n_data_type = STORE_TYPE_RAM_DATA_TYPE_RAW;
	store_count = store_ptr = 0;

	return RPMI_SUCCESS;
}

void mm_variable_term(void)
{
	rpmi_env_free(m_var_buf_payload);
}

static rpmi_uint64_t validate_input(struct mm_var_comm_header *comm_hdr,
				    rpmi_uint32_t payload_size,
				    rpmi_bool_t is_context_get_variable)
{
	struct mm_var_comm_access_variable *var;
	rpmi_uint64_t infosize;

	if (payload_size < offsetof(struct mm_var_comm_access_variable, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/*
	 * Copy the input communicate buffer payload to pre-allocated MM
	 * variable buffer payload.
	 */
	rpmi_env_memcpy(m_var_buf_payload, comm_hdr->data, payload_size);
	var = (struct mm_var_comm_access_variable *)m_var_buf_payload;

	// Prevent infosize overflow
	if ((((rpmi_uint64_t)(~0) - var->datasize) <
	     offsetof(struct mm_var_comm_access_variable, name))
	    ||
	    (((rpmi_uint64_t)(~0) - var->namesize) <
	     offsetof(struct mm_var_comm_access_variable, name) +
	     var->datasize)) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize = offsetof(struct mm_var_comm_access_variable, name) +
	    var->datasize + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	if ((var->namesize < sizeof(rpmi_uint16_t)) ||
	    (var->name[var->namesize / sizeof(rpmi_uint16_t) - 1] != L'\0')) {
		// Ensure Variable Name is a Null-terminated string
		DPRINTF("Variable Name NOT Null-terminated !!!");
		return EFI_ACCESS_DENIED;
	}

	if (is_context_get_variable && (var->name[0] == 0))
		return EFI_INVALID_PARAMETER;

	return EFI_SUCCESS;
}

static rpmi_uint64_t find_var_raw_ram(rpmi_uint16_t *varname,
				      struct efi_guid *vendor_guid)
{
	struct mm_var_comm_access_variable *var;

	while (store_ptr < store_count) {
		var =
		    (struct mm_var_comm_access_variable *)&var_store[store_ptr];
		if (!rpmi_env_memcmp(vendor_guid, &var->guid, GUID_LENGTH)) {
			if (!rpmi_env_memcmp(varname, var->name, var->namesize))
				return EFI_SUCCESS;
		}

		store_ptr++;
	}

	return (store_ptr >= store_count) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

static rpmi_uint64_t get_var_raw_ram(struct mm_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	struct mm_var_comm_access_variable *var1, *var2;
	rpmi_uint64_t status;
	void *var1_data;

	rpmi_env_memcpy(m_var_buf_payload, comm_hdr->data, payload_size);
	var1 = (struct mm_var_comm_access_variable *)m_var_buf_payload;

	store_ptr = 0;
	status = find_var_raw_ram(var1->name, &var1->guid);
	if (EFI_ERROR(status))
		return status;

	// Get data size
	var1_data = (rpmi_uint8_t *)var1->name + var1->namesize;
	var2 = (struct mm_var_comm_access_variable *)&var_store[store_ptr];
	if (var2->datasize && (var1->datasize >= var2->datasize)) {
		if (var1_data == NULL) {
			status = EFI_INVALID_PARAMETER;
			goto done;
		}

		rpmi_env_memcpy(var1_data,
				(rpmi_uint8_t *)var2->name + var2->namesize,
				var2->datasize);
		status = EFI_SUCCESS;
	} else {
		status = EFI_BUFFER_TOO_SMALL;
	}

	var1->datasize = var2->datasize;

done:
	if ((status == EFI_SUCCESS) || (status == EFI_BUFFER_TOO_SMALL)) {
		if (store_ptr < store_count)
			var1->attr = var2->attr;
	}

	rpmi_env_memcpy(comm_hdr->data, (rpmi_uint8_t *)&var_store[store_ptr],
			payload_size);

	return status;
}

static rpmi_uint64_t fn_get_variable(struct mm_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	switch (store_n_data_type) {
	case STORE_TYPE_RAM_DATA_TYPE_RAW:
		status = get_var_raw_ram(comm_hdr, payload_size);
		break;

	case STORE_TYPE_RAM_DATA_TYPE_EDK2FLASH:
	case STORE_TYPE_FLASH_DATA_TYPE_RAW:
	case STORE_TYPE_FLASH_DATA_TYPE_EDK2FLASH:
		status = EFI_UNSUPPORTED;
		break;

	default:
		status = EFI_INVALID_PARAMETER;
		break;
	}

	return status;
}

static void set_var_raw_ram(struct mm_var_comm_header *comm_hdr,
			    rpmi_uint32_t payload_size)
{
	struct mm_var_comm_access_variable *var1, *var2;
	rpmi_uint8_t count = 0;

	mm_memdump(comm_hdr->data, payload_size, "SET");
	var1 = (struct mm_var_comm_access_variable *)comm_hdr->data;

	/* Check if same Vendor GUID and Variable Name pre-exists */
	while (count < store_count) {
		/* Check Vendor GUID match first */
		if (rpmi_env_memcmp(comm_hdr->data, &var_store[count],
				    sizeof(struct efi_guid)) != 0) {
			count++;
			continue;
		}

		var2 = (struct mm_var_comm_access_variable *)&var_store[count];

		/* Now check Variable Name match */
		if ((var1->namesize == var2->namesize) &&
		    (rpmi_env_memcmp(var1->name, var2->name,
				     var1->namesize) == 0)) {
			/* Match found, update the existing data */
			DPRINTF("Update existing Variable at Pos = %d", count);
			mm_memdump(&var_store[count], payload_size, "U1");
			rpmi_env_memcpy(&var_store[count], comm_hdr->data,
					payload_size);
			mm_memdump(&var_store[count], payload_size, "U2");
			break;
		}

		count++;
	}

	if (count == store_count) {
		rpmi_env_memcpy(&var_store[count], comm_hdr->data,
				payload_size);
		mm_memdump(&var_store[count], payload_size, "NV");
		store_count++;
		DPRINTF("Added New Variable: Attribues = 0x%x, Total = %d",
			var1->attr, store_count);
	}
}

static rpmi_uint64_t fn_set_variable(struct mm_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	switch (store_n_data_type) {
	case STORE_TYPE_RAM_DATA_TYPE_RAW:
		set_var_raw_ram(comm_hdr, payload_size);
		status = EFI_SUCCESS;
		break;

	case STORE_TYPE_RAM_DATA_TYPE_EDK2FLASH:
	case STORE_TYPE_FLASH_DATA_TYPE_RAW:
	case STORE_TYPE_FLASH_DATA_TYPE_EDK2FLASH:
		status = EFI_UNSUPPORTED;
		break;

	default:
		status = EFI_INVALID_PARAMETER;
		break;
	}

	return status;
}

static inline rpmi_uint64_t fn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
						rpmi_uint32_t payload_size)
{
	struct mm_var_comm_get_payload_size *get_payload_size;

	if (payload_size < sizeof(struct mm_var_comm_get_payload_size))
		return EFI_INVALID_PARAMETER;

	get_payload_size = (struct mm_var_comm_get_payload_size *)comm_hdr_data;
	get_payload_size->var_payload_size = m_var_buf_payload_size;

	return EFI_SUCCESS;
}

enum rpmi_error mm_variable_handler(void *comm_buf, rpmi_uint64_t bufsize)
{
	rpmi_uint64_t status = EFI_SUCCESS, comm_buf_payload_size;
	struct mm_var_comm_header *var_comm_hdr;

	if (comm_buf == NULL) {
		DPRINTF("Nothing to do.");
		return RPMI_SUCCESS;
	}

	if (bufsize < MM_VAR_COMM_HEADER_SIZE) {
		DPRINTF("MM comm buffer size invalid !!!");
		return RPMI_SUCCESS;
	}

	comm_buf_payload_size = bufsize - MM_VAR_COMM_HEADER_SIZE;
	if (comm_buf_payload_size > m_var_buf_payload_size) {
		DPRINTF("MM comm buffer payload size invalid !!!");
		return RPMI_SUCCESS;
	}

	DPRINTF("comm_buf_payload_size = %ld", comm_buf_payload_size);

	var_comm_hdr = (struct mm_var_comm_header *)comm_buf;

	switch (var_comm_hdr->function) {
	case MM_VAR_FN_GET_VARIABLE:
		DPRINTF("Processing %s",
			get_var_fn_string(var_comm_hdr->function));
		status =
		    validate_input(var_comm_hdr, comm_buf_payload_size, true);
		if (status != EFI_SUCCESS)
			break;

		status = fn_get_variable(var_comm_hdr, comm_buf_payload_size);
		break;

	case MM_VAR_FN_SET_VARIABLE:
		DPRINTF("Processing %s",
			get_var_fn_string(var_comm_hdr->function));
		status =
		    validate_input(var_comm_hdr, comm_buf_payload_size, false);
		if (status != EFI_SUCCESS)
			break;

		status = fn_set_variable(var_comm_hdr, comm_buf_payload_size);
		break;

	case MM_VAR_FN_GET_PAYLOAD_SIZE:
		DPRINTF("Processing %s",
			get_var_fn_string(var_comm_hdr->function));
		status = fn_get_payload_size(var_comm_hdr->data,
					     comm_buf_payload_size);
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
