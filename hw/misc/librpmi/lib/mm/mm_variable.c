// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm_variable.h"

#define VAR_MAX_INFOSIZE  1024	/* Max information size per MM variable */

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

#endif /* defined(ENABLE_DEBUG) && ENABLE_DEBUG */

enum rpmi_error mm_variable_init(void)
{
	m_var_buf_payload_size = VAR_MAX_INFOSIZE - MM_VAR_COMM_HEADER_SIZE;

	m_var_buf_payload = rpmi_env_zalloc(m_var_buf_payload_size);
	if (!m_var_buf_payload) {
		DPRINTF("failed to allocate memory of size = %d",
			m_var_buf_payload_size);
		return RPMI_ERR_FAILED;
	}

	return RPMI_SUCCESS;
}

void mm_variable_term(void)
{
	rpmi_env_free(m_var_buf_payload);
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
