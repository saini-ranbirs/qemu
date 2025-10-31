// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "rpmi_mm_common.h"
#include "rpmi_mm_efi.h"

static communicate_fp mm_service_handlers[RPMI_MM_INSTANCE_MAX] = {
	[RPMI_MM_INSTANCE_EFI] = mm_efi_communicate,
};

/** RPMI Management Mode (MM) structure to create an MM group instance */
struct rpmi_mm {
	/** MM shared memory address */
	rpmi_uint64_t shmem_addr;

	/** MM shared memory size */
	rpmi_uint32_t shmem_size;

	/** MM instance type */
	enum rpmi_mm_instance_type inst_type;

	union rpmi_mm_instance_platform_ops u;
};

enum rpmi_error rpmi_mm_instance_meminfo(struct rpmi_mm *mm,
					 rpmi_uint64_t *mem_addr,
					 rpmi_uint32_t *mem_size)
{
	if (!mem_addr || !mem_size || !mm) {
		DPRINTF("invalid parameter: null pointer");
		return RPMI_ERR_INVALID_PARAM;
	}

	*mem_addr = mm->shmem_addr;
	*mem_size = mm->shmem_size;

	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_mm_instance_communicate(struct rpmi_mm *mm,
					     struct rpmi_service *service,
					     struct rpmi_transport *xport,
					     rpmi_uint16_t req_datalen,
					     const rpmi_uint8_t *req_data,
					     rpmi_uint16_t *rsp_datalen,
					     rpmi_uint8_t *rsp_data)
{
	communicate_fp fp;

	if (!mm)
		return RPMI_ERR_INVALID_PARAM;

	if ((mm->inst_type < RPMI_MM_INSTANCE_EFI) ||
	    (mm->inst_type >= RPMI_MM_INSTANCE_MAX))
		return RPMI_ERR_NO_DATA;

	/*if (mm->service_handler) {
		fp = (communicate_fp)(mmi->service_handler);
		return fp(mmi->mm, service, xport, req_datalen, req_data,
			  rsp_datalen, rsp_data);
	}*/

	fp = mm_service_handlers[mm->inst_type];
	if (!fp)
		return RPMI_ERR_NO_DATA;

	return fp(mm->shmem_addr, mm->shmem_size, &mm->u, service, xport,
		  req_datalen, req_data, rsp_datalen, rsp_data);
}

struct rpmi_mm *rpmi_mm_create(rpmi_uint64_t shmem_addr,
			       rpmi_uint32_t shmem_size,
			       enum rpmi_mm_instance_type inst_type,
			       union rpmi_mm_instance_platform_ops *mmp_ops)
{
	struct rpmi_mm *mm;

	if ((inst_type < RPMI_MM_INSTANCE_EFI) ||
	    (inst_type >= RPMI_MM_INSTANCE_MAX)) {
		DPRINTF("invalid parameter: instance type");
		return NULL;
	}

	/* Allocate MM service instance group */
	mm = rpmi_env_zalloc(sizeof(*mm));
	if (!mm) {
		DPRINTF("failed to allocate MM instance type");
		return NULL;
	}

	mm->shmem_addr = shmem_addr;
	mm->shmem_size = shmem_size;
	mm->inst_type = inst_type;

	rpmi_env_memcpy(&mm->u, mmp_ops, sizeof(mm->u));

	return mm;
}
