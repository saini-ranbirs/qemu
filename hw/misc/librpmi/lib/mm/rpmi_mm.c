// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "rpmi_mm_common.h"
#include "rpmi_mm_efi.h"

static communicate_fp mm_service_handlers[RPMI_MM_SERVICE_MAX] = {
	[RPMI_MM_SERVICE_EFI] = mm_efi_communicate,
};

/** RPMI Management Mode (MM) structure to create an MM group instance */
struct rpmi_mmi {
	struct rpmi_mm mm;

	void *service_handler;
};

enum rpmi_error rpmi_mm_instance_mem_info(struct rpmi_mmi *mmi,
					  rpmi_uint64_t *mem_addr,
					  rpmi_uint32_t *mem_size)
{
	if (!mem_addr || !mem_size) {
		DPRINTF("invalid parameter: null pointer");
		return RPMI_ERR_INVALID_PARAM;
	}

	*mem_addr = mmi->mm.shmem_addr;
	*mem_size = mmi->mm.shmem_size;

	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_mm_instance_communicate(struct rpmi_mmi *mmi,
					     struct rpmi_service *service,
					     struct rpmi_transport *xport,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	communicate_fp fp;

	if ((mmi->mm.svc_type < RPMI_MM_SERVICE_EFI) ||
	    (mmi->mm.svc_type >= RPMI_MM_SERVICE_MAX))
		return RPMI_ERR_NO_DATA;

	if (mmi->service_handler) {
		fp = (communicate_fp)(mmi->service_handler);
		return fp(&mmi->mm, service, xport, request_datalen,
			  request_data, response_datalen, response_data);
	}

	fp = mm_service_handlers[mmi->mm.svc_type];
	if (!fp)
		return RPMI_ERR_NO_DATA;

	return fp(&mmi->mm, service, xport, request_datalen,
		  request_data, response_datalen, response_data);
}

struct rpmi_mmi *rpmi_mm_create(struct rpmi_mm *mm)
{
	struct rpmi_mmi *mmi;

	if (!mm) {
		DPRINTF("invalid parameter: null pointer");
		return NULL;
	}

	if ((mm->svc_type < RPMI_MM_SERVICE_EFI) ||
	    (mm->svc_type >= RPMI_MM_SERVICE_MAX)) {
		DPRINTF("invalid parameter: service type");
		return NULL;
	}

	/* Allocate MM service instance group */
	mmi = rpmi_env_zalloc(sizeof(*mmi));
	if (!mmi) {
		DPRINTF("failed to allocate MM service type instance");
		return NULL;
	}

	rpmi_env_memcpy(&mmi->mm, mm, sizeof(mmi->mm));

	switch (mmi->mm.svc_type) {
	case RPMI_MM_SERVICE_EFI:
		//mmi->service_handler = mm_efi_communicate;
		mmi->service_handler = NULL;
		break;

	default:
		mmi->service_handler = NULL;
		break;
	}

	return mmi;
}
