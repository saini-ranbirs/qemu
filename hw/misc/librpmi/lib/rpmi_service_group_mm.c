// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

#define DEBUG  1

#if defined(DEBUG) && DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

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

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	return RPMI_ERR_NO_DATA;
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
