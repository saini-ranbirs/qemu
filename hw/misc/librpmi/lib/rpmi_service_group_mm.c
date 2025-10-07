// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm/mm_variable.h"

#define RPMI_MM_MAJOR_VER   (0x1UL)
#define RPMI_MM_MINOR_VER   0x0

#define MM_MAJOR_VER_MASK   0xEFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

#define MM_MAJOR_VER(x)     (((x) & MM_MAJOR_VER_MASK) >> MM_MAJOR_VER_SHIFT)
#define MM_MINOR_VER(x)     ((x) & MM_MINOR_VER_MASK)

struct rpmi_mm_attr {
	rpmi_uint32_t mm_version;
	rpmi_uint32_t shmem_addr_lo;
	rpmi_uint32_t shmem_addr_hi;
	rpmi_uint32_t shmem_size;
};

struct rpmi_mm_group {
	struct rpmi_mm_attr mma;
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

static enum rpmi_error rpmi_mm_get_attributes(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *xport,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *rsp = (void *)response_data;
	struct rpmi_mm_group *sgmm = group->priv;
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

extern void mm_memdump(const void *src, rpmi_size_t count, const char *name);

static inline int get_guid_index(const rpmi_uint8_t *guid,
				 rpmi_uint16_t msg_len)
{
	rpmi_uint8_t i;

	//mm_memdump(guid, GUID_LENGTH, "I/P - GUID");
	for (i = 1; i < array_size(mm_comm_hdr_guid_lut); i++) {
		if (rpmi_env_memcmp((void *)guid, &mm_comm_hdr_guid_lut[i].guid,
				    msg_len))
			continue;

		return i;
	}

	return 0;
}

rpmi_uint8_t msg_buffer[17 * 1024];

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	struct efi_mm_comm_header *mm_comm_hdr, *msg;
	rpmi_uint32_t *rsp = (void *)response_data;
	struct rpmi_mm_group *sgmm = group->priv;
	rpmi_uint64_t status = RPMI_ERR_NO_DATA;
	struct rpmi_mm_comm_req *mmc_req;
	rpmi_uint64_t msg_len, mm_addr;
	rpmi_uint8_t index;//, *buf;
	struct efi_var_policy_comm_header *policy_hdr;

	if (!request_data)
		return RPMI_ERR_NO_DATA;

	DPRINTF("ENTER");

	mmc_req = (struct rpmi_mm_comm_req *)request_data;
	mm_addr = sgmm->mma.shmem_addr_hi;
	mm_addr = (mm_addr << 32) | sgmm->mma.shmem_addr_lo;
	mm_addr = mm_addr + mmc_req->idata_off;

	//buf = rpmi_env_zalloc(sizeof(struct efi_mm_comm_header));
	//rpmi_env_readb(mm_addr, buf, sizeof(struct efi_mm_comm_header));
	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)&msg_buffer,
		       sizeof(struct efi_mm_comm_header));
	//mm_memdump(msg_buffer, sizeof(struct efi_mm_comm_header), "BUF1");

	//mm_comm_hdr = (struct efi_mm_comm_header *)buf;
	mm_comm_hdr = (struct efi_mm_comm_header *)msg_buffer;
	msg_len =
	    offsetof(struct efi_mm_comm_header, data) + mm_comm_hdr->msg_len;
	//rpmi_env_free(buf);
	DPRINTF("msg_len = %ld", msg_len);

	//msg = rpmi_env_zalloc(msg_len);
	msg = (struct efi_mm_comm_header *)msg_buffer;
	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)msg, msg_len);
	//mm_memdump(msg, msg_len, "BUF2");

	index = get_guid_index((rpmi_uint8_t *)&msg->hdr_guid,
			       sizeof(msg->hdr_guid));

	switch (mm_comm_hdr_guid_lut[index].name) {
	case EFI_MM_VAR_PROTOCOL_GUID:
		DPRINTF("Handling header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = mm_variable_handler(&msg->data, msg_len);
		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case EFI_MM_VAR_POLICY_GUID:
		policy_hdr = (struct efi_var_policy_comm_header *)&msg->data;
		policy_hdr->result = 0x00;

		msg_len = offsetof(struct efi_mm_comm_header, data) +
			  sizeof(*policy_hdr);
		msg_len = msg_len + sizeof(msg->hdr_guid) - 1;
		msg_len = msg_len / sizeof(msg->hdr_guid);
		msg_len = msg_len * sizeof(msg->hdr_guid);

		DPRINTF("Handling (dummy) header %s",
			get_hdr_guid_string(mm_comm_hdr_guid_lut[index].name));
		status = RPMI_SUCCESS;
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
		status = RPMI_SUCCESS;
		msg_len = 0;
		break;
	}

	*response_datalen = 2 * sizeof(rpmi_uint32_t);
	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);
	rsp[1] = rpmi_to_xe32(xport->is_be, msg_len);

	DPRINTF("response length = %d status = %ld", rsp[1], status);
	DPRINTF("EXIT\n");

	//rpmi_env_free(msg);
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
			      rpmi_uint32_t shmem_size)
{
	struct rpmi_service_group *group;
	struct rpmi_mm_group *sgmm;

	/* Allocate MM group */
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

	mm_variable_init();

	return group;
}

void rpmi_service_group_mm_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("invalid parameters");
		return;
	}

	mm_variable_term();

	rpmi_env_free(group->priv);
}
