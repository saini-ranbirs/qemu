/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

#define DEBUG  1

#ifdef DEBUG

#include "stdio.h"
#include "stdarg.h"

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

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
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

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	DPRINTF("====================================================> "
		"%s: received MM_COMUNICATE call \n", __func__);

	return RPMI_ERR_NO_DATA;
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
