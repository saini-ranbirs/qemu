// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm/rpmi_mm_common.h"

/* RPMI Spec MM Version : Not explicitly defined in Spec as such */
#define RPMI_MM_MAJOR_VER   (0x1UL)
#define RPMI_MM_MINOR_VER   0x0

/**
 * Note: A change is required in RISC-V EDK2 MMcommunicate.h as it defines the
 * MM_MAJOR_VER_MASK as 0xEFFF0000 => The top nibble is E and not F: why?
 */
#define MM_MAJOR_VER_MASK   0xFFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

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
		rsp[1] = rpmi_to_xe32(xport->is_be, sgmm->mm_version);
		rsp[2] = rpmi_to_xe32(xport->is_be,
				      sgmm->mm.shmem_addr & 0xFFFFFFFF);
		rsp[3] = rpmi_to_xe32(xport->is_be, sgmm->mm.shmem_addr >> 32);
		rsp[4] = rpmi_to_xe32(xport->is_be, sgmm->mm.shmem_size);
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
	struct rpmi_service_group_mm *sgmm = group->priv;
	rpmi_uint32_t *rsp = (void *)response_data;
	rpmi_uint64_t status;

	if (!request_data || !sgmm)
		return RPMI_ERR_NO_DATA;

	DPRINTF("Service group = %s", group->name);

	status = rpmi_mm_instance_communicate(group, service, xport,
					      request_datalen, request_data,
					      response_datalen, response_data);

	*response_datalen = 2 * sizeof(rpmi_uint32_t);
	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);

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

struct rpmi_service_group *rpmi_service_group_mm_create(struct rpmi_mm *mm)
{
	struct rpmi_service_group_mm *sgmm;
	struct rpmi_service_group *group;

	/* Critical parameter should be non-NULL */
	if (!mm) {
		DPRINTF("invalid parameter: instance pointer is NULL");
		return NULL;
	}

	/* svc_type should be in desired range */
	if ((mm->svc_type < RPMI_MM_SERVICE_EFI) ||
	    (mm->svc_type >= RPMI_MM_SERVICE_MAX)) {
		DPRINTF("invalid parameter: service type");
		return NULL;
	}

	/* Allocate MM service group */
	sgmm = rpmi_env_zalloc(sizeof(*sgmm));
	if (!sgmm) {
		DPRINTF("failed to allocate MM service group instance");
		return NULL;
	}

	sgmm->mm_version =
	    ((RPMI_MM_MAJOR_VER << MM_MAJOR_VER_SHIFT) & MM_MAJOR_VER_MASK) |
	    ((RPMI_MM_MINOR_VER & MM_MINOR_VER_MASK));
	rpmi_env_memcpy(&sgmm->mm, mm, sizeof(sgmm->mm));

	group = &sgmm->group;

	switch (sgmm->mm.svc_type) {
	case RPMI_MM_SERVICE_EFI:
		group->name = "mm_efi";
		break;

	default:
		break;
	}

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
