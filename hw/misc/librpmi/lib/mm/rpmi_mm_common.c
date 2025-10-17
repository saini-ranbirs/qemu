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

	fp = mm_service_handlers[mm->inst_type];
	if (!fp)
		return RPMI_ERR_NO_DATA;

	return fp(mm->shmem_addr, mm->shmem_size, &mm->u, service, xport,
		  req_datalen, req_data, rsp_datalen, rsp_data);
}
