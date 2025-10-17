// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "rpmi_mm.h"

#ifdef DEBUG
#define PREFIX_STR  "== LIBRPMI: MM: =========> %s: %03u: "

#define DPRINTF(msg...)							\
	{								\
		rpmi_env_printf(PREFIX_STR, __func__, __LINE__);	\
		rpmi_env_printf(msg);					\
		rpmi_env_printf("\n");					\
	}
#else
#define DPRINTF(msg...)
#endif

#define RPMI_MM_MAJOR_VER   (0x1UL)
#define RPMI_MM_MINOR_VER   0x0

#define MM_MAJOR_VER_MASK   0xEFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

struct rpmi_mm_attr {
	rpmi_uint32_t mm_version;
	rpmi_uint32_t shmem_addr_lo;
	rpmi_uint32_t shmem_addr_hi;
	rpmi_uint32_t shmem_size;
};

struct rpmi_service_group_mm {
	struct rpmi_mm_attr mma;
	const struct rpmi_mm_platform_ops *ops;
	void *ops_priv;
	struct rpmi_service_group group;
};

struct rpmi_mm_comm_req {
	rpmi_uint32_t idata_off;
	rpmi_uint32_t idata_len;
	rpmi_uint32_t odata_off;
	rpmi_uint32_t odata_len;
};

struct rpmi_mm_comm_header_guid {
	enum efi_mm_header_guid name;
	struct efi_guid guid;
};

struct rpmi_mm_comm_header_guid mm_comm_hdr_guid_lut[] = {
	[0] { EFI_MM_HDR_GUID_NONE, EFI_MM_HDR_GUID_NONE_DATA },
	[1] { EFI_MM_VAR_PROTOCOL_GUID, EFI_MM_VAR_PROTOCOL_GUID_DATA },
	[2] { EFI_MM_VAR_POLICY_GUID, EFI_MM_VAR_POLICY_GUID_DATA },
	[3] { EFI_MM_END_OF_DXE_GUID, EFI_MM_END_OF_DXE_GUID_DATA },
	[4] { EFI_MM_READY_TO_BOOT_GUID, EFI_MM_READY_TO_BOOT_GUID_DATA },
	[5] { EFI_MM_EXIT_BOOT_SVC_GUID, EFI_MM_EXIT_BOOT_SVC_GUID_DATA },
};

#define MAX_TRANSFER_SIZE  (16 * 1024)	/* 16 KB */
static rpmi_uint8_t payload_buffer[MAX_PAYLOAD_SIZE];
rpmi_uint8_t msg_buffer[MAX_TRANSFER_SIZE];

#ifdef DEBUG

static rpmi_uint16_t mm_calls_counter = 0;

#define STRING_CASE(x)  case x: return #x

static const char *get_hdr_guid_string(enum efi_mm_header_guid guid)
{
	switch (guid) {
		STRING_CASE(EFI_MM_VAR_PROTOCOL_GUID);
		STRING_CASE(EFI_MM_VAR_POLICY_GUID);
		STRING_CASE(EFI_MM_END_OF_DXE_GUID);
		STRING_CASE(EFI_MM_READY_TO_BOOT_GUID);
		STRING_CASE(EFI_MM_EXIT_BOOT_SVC_GUID);

	default:
		STRING_CASE(EFI_MM_HDR_GUID_UNSUPPORTED);
	}
}

static const char *null_string = "NULL";

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

#endif /* DEBUG */

static enum rpmi_error rpmi_mm_get_attributes(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *xport,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_service_group_mm *sgmm = group->priv;
	rpmi_uint32_t *rsp = (void *)response_data;
	enum rpmi_error status;

	if (sgmm && response_datalen) {
		*response_datalen = 5 * sizeof(rpmi_uint32_t);
		rsp[1] = rpmi_to_xe32(xport->is_be, sgmm->mma.mm_version);
		rsp[2] = rpmi_to_xe32(xport->is_be, sgmm->mma.shmem_addr_lo);
		rsp[3] = rpmi_to_xe32(xport->is_be, sgmm->mma.shmem_addr_hi);
		rsp[4] = rpmi_to_xe32(xport->is_be, sgmm->mma.shmem_size);
		status = RPMI_SUCCESS;
	} else {
		status = RPMI_ERR_NO_DATA;
	}

	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);

	return RPMI_SUCCESS;
}

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

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct mm_var_comm_access_variable *)payload_buffer;

	/* Prevent infosize overflow */
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

static rpmi_uint64_t fn_get_variable(struct rpmi_service_group_mm *sgmm,
				     struct mm_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_input(comm_hdr, payload_size, true);
	if (status != EFI_SUCCESS)
		return status;

	return sgmm->ops->get_variable(sgmm->ops_priv, comm_hdr->data,
				       payload_size);
}

static rpmi_uint64_t validate_name(struct mm_var_comm_header *comm_hdr,
				   rpmi_uint32_t payload_size)
{
	struct mm_var_comm_get_next_var_name *var;
	rpmi_uint64_t infosize, max_len;

	if (payload_size < offsetof(struct mm_var_comm_get_next_var_name, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct mm_var_comm_get_next_var_name *)payload_buffer;

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
	    offsetof(struct mm_var_comm_get_next_var_name, name) +
	    var->namesize) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize =
	    offsetof(struct mm_var_comm_access_variable, name) + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	return EFI_SUCCESS;
}

static rpmi_uint64_t fn_get_next_var_name(struct rpmi_service_group_mm *sgmm,
					  struct mm_var_comm_header *comm_hdr,
					  rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_name(comm_hdr, payload_size);
	if (status != EFI_SUCCESS)
		return status;

	return sgmm->ops->get_next_variable_name(sgmm->ops_priv, comm_hdr->data,
						 payload_size);
}

static rpmi_uint64_t fn_set_variable(struct rpmi_service_group_mm *sgmm,
				     struct mm_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	status = validate_input(comm_hdr, payload_size, false);
	if (status != EFI_SUCCESS)
		return status;

	return sgmm->ops->set_variable(sgmm->ops_priv, comm_hdr->data,
				       payload_size);
}

static inline rpmi_uint64_t fn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
						rpmi_uint32_t payload_size)
{
	struct mm_var_comm_get_payload_size *get_payload_size;

	if (payload_size < sizeof(struct mm_var_comm_get_payload_size))
		return EFI_INVALID_PARAMETER;

	get_payload_size = (struct mm_var_comm_get_payload_size *)comm_hdr_data;
	get_payload_size->var_payload_size = MAX_PAYLOAD_SIZE;

	return EFI_SUCCESS;
}

static enum rpmi_error mm_var_fn_handler(struct rpmi_service_group_mm *sgmm,
					 void *comm_buf, rpmi_uint64_t bufsize)
{
	rpmi_uint64_t status = EFI_SUCCESS, payload_size;
	struct mm_var_comm_header *var_comm_hdr;

	if (comm_buf == NULL) {
		DPRINTF("Nothing to do.");
		return RPMI_SUCCESS;
	}

	if (bufsize < MM_VAR_COMM_HEADER_SIZE) {
		DPRINTF("MM comm buffer size invalid !!!");
		return RPMI_SUCCESS;
	}

	payload_size = bufsize - MM_VAR_COMM_HEADER_SIZE;
	DPRINTF("bufsize = %ld hdrsize = %ld payload_size = %ld",
		bufsize, MM_VAR_COMM_HEADER_SIZE, payload_size);

	if (payload_size > MAX_PAYLOAD_SIZE) {
		DPRINTF("MM comm buffer payload size invalid > %ld !!!",
			MAX_PAYLOAD_SIZE);
		return RPMI_SUCCESS;
	}

	rpmi_env_memset(payload_buffer, 0x00, MAX_PAYLOAD_SIZE);
	var_comm_hdr = (struct mm_var_comm_header *)comm_buf;

	switch (var_comm_hdr->function) {
	case MM_VAR_FN_GET_VARIABLE:
		DPRINTF("Processing %s mm_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++mm_calls_counter);
		status = fn_get_variable(sgmm, var_comm_hdr, payload_size);
		break;

	case MM_VAR_FN_GET_NEXT_VARIABLE_NAME:
		DPRINTF("Processing %s mm_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++mm_calls_counter);
		status = fn_get_next_var_name(sgmm, var_comm_hdr, payload_size);
		break;

	case MM_VAR_FN_SET_VARIABLE:
		DPRINTF("Processing %s mm_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++mm_calls_counter);
		status = fn_set_variable(sgmm, var_comm_hdr, payload_size);
		break;

	case MM_VAR_FN_GET_PAYLOAD_SIZE:
		DPRINTF("Processing %s mm_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++mm_calls_counter);
		status = fn_get_payload_size(var_comm_hdr->data, payload_size);
		break;

	case MM_VAR_FN_READY_TO_BOOT:
	case MM_VAR_FN_EXIT_BOOT_SERVICE:
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

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	struct rpmi_service_group_mm *sgmm = group->priv;
	struct efi_var_policy_comm_header *policy_hdr;
	struct efi_mm_comm_header *mm_comm_hdr, *msg;
	rpmi_uint32_t *rsp = (void *)response_data;
	rpmi_uint64_t status = RPMI_ERR_NO_DATA;
	struct rpmi_mm_comm_req *mmc_req;
	rpmi_uint64_t msg_len, mm_addr;
	rpmi_uint8_t index;

	if (!request_data)
		return RPMI_ERR_NO_DATA;

	mmc_req = (struct rpmi_mm_comm_req *)request_data;
	mm_addr = sgmm->mma.shmem_addr_hi;
	mm_addr = (mm_addr << 32) | sgmm->mma.shmem_addr_lo;
	mm_addr = mm_addr + mmc_req->idata_off;

	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)&msg_buffer,
		       sizeof(struct efi_mm_comm_header));

	mm_comm_hdr = (struct efi_mm_comm_header *)msg_buffer;
	msg_len =
	    offsetof(struct efi_mm_comm_header, data) + mm_comm_hdr->msg_len;

	msg = (struct efi_mm_comm_header *)msg_buffer;
	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)msg, msg_len);

	index = get_guid_index((rpmi_uint8_t *)&msg->hdr_guid,
			       sizeof(msg->hdr_guid));

	switch (mm_comm_hdr_guid_lut[index].name) {
	case EFI_MM_VAR_PROTOCOL_GUID:
		DPRINTF("Handling header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = mm_var_fn_handler(sgmm, &msg->data, msg_len);
		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case EFI_MM_VAR_POLICY_GUID:
		DPRINTF("Handling (dummy) header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = RPMI_SUCCESS;

		policy_hdr = (struct efi_var_policy_comm_header *)&msg->data;
		policy_hdr->result = 0x00;

		/* Maintain msg_len as next multiple of GUID_LENGTH/ 16 bytes */
		msg_len = offsetof(struct efi_mm_comm_header, data) +
		    sizeof(*policy_hdr);
		msg_len = msg_len + GUID_LENGTH - 1;
		msg_len = msg_len / GUID_LENGTH;
		msg_len = msg_len * GUID_LENGTH;

		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case EFI_MM_END_OF_DXE_GUID:
	case EFI_MM_READY_TO_BOOT_GUID:
	case EFI_MM_EXIT_BOOT_SVC_GUID:
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

	*response_datalen = 2 * sizeof(rpmi_uint32_t);
	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);
	rsp[1] = rpmi_to_xe32(xport->is_be, msg_len);

	DPRINTF("response length = %d status = %ld", rsp[1], status);

	return status;
}

/* Keep entry index same as service_id value */
static struct rpmi_service rpmi_mm_services[RPMI_MM_SRV_ID_MAX] = {
	[1] = {
	       .service_id = RPMI_MM_SRV_ENABLE_NOTIFICATION,
	       .min_a2p_request_datalen = 0,
	       .process_a2p_request = NULL,
	        },
	[2] = {
	       .service_id = RPMI_MM_SRV_GET_ATTRIBUTES,
	       .min_a2p_request_datalen = 0,
	       .process_a2p_request = rpmi_mm_get_attributes,
	        },
	[3] = {
	       .service_id = RPMI_MM_SRV_COMMUNICATE,
	       .min_a2p_request_datalen = 4,
	       .process_a2p_request = rpmi_mm_communicate,
	        },
};

struct rpmi_service_group
*rpmi_service_group_mm_create(rpmi_uint32_t shmem_addr_hi,
			      rpmi_uint32_t shmem_addr_lo,
			      rpmi_uint32_t shmem_size,
			      const struct rpmi_mm_platform_ops *ops,
			      void *ops_priv)
{
	struct rpmi_service_group_mm *sgmm;
	struct rpmi_service_group *group;

	/* Allocate MM service group */
	sgmm = rpmi_env_zalloc(sizeof(*sgmm));
	if (!sgmm) {
		DPRINTF("failed to allocate MM service group instance");
		return NULL;
	}

	sgmm->mma.mm_version =
	    ((RPMI_MM_MAJOR_VER << MM_MAJOR_VER_SHIFT) & MM_MAJOR_VER_MASK) |
	    ((RPMI_MM_MINOR_VER & MM_MINOR_VER_MASK));
	sgmm->mma.shmem_addr_hi = shmem_addr_hi;
	sgmm->mma.shmem_addr_lo = shmem_addr_lo;
	sgmm->mma.shmem_size = shmem_size;
	sgmm->ops = ops;
	sgmm->ops_priv = ops_priv;

	group = &sgmm->group;
	group->name = "mm";
	group->servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE;
	group->servicegroup_version =
	    RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_MM_SRV_ID_MAX;
	group->services = rpmi_mm_services;
	group->process_events = NULL;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sgmm;

	return group;
}

void rpmi_service_group_mm_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("invalid parameters");
		return;
	}

	rpmi_env_free(group->priv);
}
