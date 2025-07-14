// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm_variable.h"

EFI_STATUS
EFIAPI
VarCheckPolicyLibCommonConstructor (
  VOID
  );

EFI_STATUS
EFIAPI
VarCheckPolicyLibMmiHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *RegisterContext,
  IN OUT VOID        *CommBuffer,
  IN OUT UINTN       *CommBufferSize
  );

#if ENABLE_DEBUG
int rpmi_env_printf(const char *format, ...)
{
	va_list args;
	char bfr[512];
	int bytes_written;

	va_start(args, format);

	bytes_written = vsnprintf(bfr, sizeof(bfr), format, args);
	fwrite(bfr, sizeof(char), bytes_written, stdout);

	va_end(args);

	return bytes_written;
}
#endif

#define RPMI_MM_MAJOR_VER  0x1UL
#define RPMI_MM_MINOR_VER  0x0

#define MM_MAJOR_VER_MASK   0xEFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

#define MM_MAJOR_VER(x)  (((x) & MM_MAJOR_VER_MASK) >> MM_MAJOR_VER_SHIFT)
#define MM_MINOR_VER(x)  ((x) & MM_MINOR_VER_MASK)

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

enum mm_header_guid {
	EFI_SMM_HEADER_GUID_NONE,
	EFI_SMM_VARIABLE_PROTOCOL_GUID,
	EFI_SMM_VARIABLE_CHECK_POLICY_GUID,
};

#define EFI_SMM_HEADER_GUID_NONE_DATA	\
	{ 0x00000000, 0x0000, 0x0000,	\
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

#define EFI_SMM_VARIABLE_PROTOCOL_GUID_DATA	\
	{ 0xed32d533, 0x99e6, 0x4209,	\
	  { 0x9c, 0xc0, 0x2d, 0x72, 0xcd, 0xd9, 0x98, 0xa7 } }

#define EFI_SMM_VARIABLE_CHECK_POLICY_GUID_DATA	\
	{ 0xda1b0d11, 0xd1a7, 0x46c4,	\
	  { 0x9d, 0xc9, 0xf3, 0x71, 0x48, 0x75, 0xc6, 0xeb } }

struct rpmi_mm_hdr_guid {
	enum mm_header_guid name;
	EFI_GUID guid;
};

struct rpmi_mm_hdr_guid hguid_lut[] = {
	[0] { EFI_SMM_HEADER_GUID_NONE, EFI_SMM_HEADER_GUID_NONE_DATA },
	[1] { EFI_SMM_VARIABLE_PROTOCOL_GUID,
	      EFI_SMM_VARIABLE_PROTOCOL_GUID_DATA },
	[2] { EFI_SMM_VARIABLE_CHECK_POLICY_GUID,
	      EFI_SMM_VARIABLE_CHECK_POLICY_GUID_DATA },
};

/* Defined in EDK2 MdePkg/Include/Protocol/MmCommunication.h */

/**
 * struct efi_mm_communicate_header - Header used for SMM variable communication

 * @hdr_guid:  header use for disambiguation of content
 * @msg_len:   length of the message. Does not include the size of the header
 * @data:      payload of the message
 *
 * Defined in EDK2 as EFI_MM_COMMUNICATE_HEADER.
 * To avoid confusion in interpreting frames, the communication buffer should
 * always begin with efi_mm_communicate_header.
 */
struct efi_mm_communicate_header {
	EFI_GUID hdr_guid;
	rpmi_uint64_t msg_len;
	rpmi_uint8_t data[];
};

#define MM_COMMUNICATE_HEADER_SIZE (sizeof(struct efi_mm_communicate_header))

static enum rpmi_error rpmi_mm_get_attributes(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *xport,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *rsp = (void *)response_data;
	enum rpmi_error status;
	struct rpmi_mm_group *sgmm = group->priv;

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

	DPRINTF("====================================================> "
		"%s: received MM_GET_ATTRIBUTES call \n", __func__);

	return RPMI_SUCCESS;
}

static int get_comm_header_guid(const rpmi_uint8_t *guid, rpmi_uint16_t msg_len)
{
	rpmi_uint8_t i;

	for (i = 1; i < array_size(hguid_lut); i++) {
		if (memcmp(guid, &hguid_lut[i].guid, msg_len))
			continue;

		return i;
	}

	return 0;
}

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	struct rpmi_mm_comm_req *mmc_req;
	struct rpmi_mm_group *sgmm = group->priv;
	rpmi_uint8_t *buf;
	rpmi_uint64_t msg_len;
	rpmi_uint64_t status = RPMI_ERR_NO_DATA;
	struct efi_mm_communicate_header *mm_comm_hdr, *msg;
	rpmi_uint64_t mm_addr;
	rpmi_uint8_t index;
	rpmi_uint32_t *rsp = (void *)response_data;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);

	if (!request_data)
		return RPMI_ERR_NO_DATA;

	mmc_req = (struct rpmi_mm_comm_req *)request_data;
	mm_addr = sgmm->mma.shmem_addr_hi;
	mm_addr = (mm_addr << 32) | sgmm->mma.shmem_addr_lo;
	mm_addr = mm_addr + mmc_req->idata_off;

	DPRINTF("idata_off = 0x%x idata_len = 0x%x \n", mmc_req->idata_off, mmc_req->idata_len);
	DPRINTF("odata_off = 0x%x odata_len = 0x%x \n", mmc_req->odata_off, mmc_req->odata_len);

	buf = rpmi_env_zalloc(sizeof(struct efi_mm_communicate_header));
	rpmi_env_readb(mm_addr, buf, sizeof(struct efi_mm_communicate_header));

	mm_comm_hdr = (struct efi_mm_communicate_header *)buf;
	msg_len = offsetof(struct efi_mm_communicate_header, data) +
		  mm_comm_hdr->msg_len;
	rpmi_env_free(buf);

	msg = rpmi_env_zalloc(msg_len);
	rpmi_env_readb(mm_addr, (rpmi_uint8_t *)msg, msg_len);
	DPRINTF("====================================================> "
		"%s: memdump with msg_len = %ld", __func__, msg_len);

	mm_memdump(msg, msg_len, "MSG");//mm_memdump(msg, HEADER_GUID_SIZE, "G");

	index = get_comm_header_guid((rpmi_uint8_t *)msg, sizeof(EFI_GUID));

	switch (hguid_lut[index].name) {
	case EFI_SMM_VARIABLE_PROTOCOL_GUID:
		status =
		    mm_variable_handler(&msg->data, (UINTN *)&msg_len);
		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	case EFI_SMM_VARIABLE_CHECK_POLICY_GUID:
		DPRINTF("====================================================> "
			"%s: header guid EFI_SMM_VARIABLE_CHECK_POLICY_GUID \n", __func__);
		status = VarCheckPolicyLibMmiHandler(NULL, NULL, &msg->data,
						     (UINTN *)&msg_len);
		rpmi_env_writeb(mm_addr + mmc_req->odata_off,
				(rpmi_uint8_t *)msg, msg_len);
		break;

	default:
		DPRINTF("====================================================> "
			"%s: header guid !EFI_SMM_VARIABLE_PROTOCOL_GUID "
			"!EFI_SMM_VARIABLE_CHECK_POLICY_GUID\n", __func__);
		status = RPMI_ERR_NO_DATA;
		msg_len = 0;
		break;
	}

	*response_datalen = 2 * sizeof(rpmi_uint32_t);
	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);
	rsp[1] = rpmi_to_xe32(xport->is_be, msg_len);

	DPRINTF("====================================================> "
		"%s: EXIT rsp_len = %d status = %ld\n", __func__, rsp[1], status);

	rpmi_env_free(msg);
	return status;
}

static struct rpmi_service rpmi_mm_services[RPMI_MM_SRV_ID_MAX] = {
	[RPMI_MM_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_MM_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = NULL,
	},
	[RPMI_MM_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_MM_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_mm_get_attributes,
	},
	[RPMI_MM_SRV_COMMUNICATE] = {
		.service_id = RPMI_MM_SRV_COMMUNICATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_mm_communicate,
	},
};

struct rpmi_service_group *
rpmi_service_group_mm_create(rpmi_uint32_t shmem_addr_hi,
			     rpmi_uint32_t shmem_addr_lo,
			     rpmi_uint32_t shmem_size)
{
	struct rpmi_mm_group *sgmm;
	struct rpmi_service_group *group;

	/* Allocate MM group */
	sgmm = rpmi_env_zalloc(sizeof(*sgmm));
	if (!sgmm) {
		DPRINTF("%s: failed to allocate MM service group instance\n",
			__func__);
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
	VarCheckPolicyLibCommonConstructor();

	DPRINTF("====================================================> "
		"%s: received call 0x%x \n", __func__, shmem_size);

	return group;
}

void rpmi_service_group_mm_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free(group->priv);
}
