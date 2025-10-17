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

enum rpmi_error rpmi_mm_instance_communicate(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *xport,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	struct rpmi_service_group_mm *sgmm = group->priv;
	communicate_fp fp;

	if ((sgmm->mm.svc_type < RPMI_MM_SERVICE_EFI) ||
	    (sgmm->mm.svc_type >= RPMI_MM_SERVICE_MAX))
		return RPMI_ERR_NO_DATA;

	fp = mm_service_handlers[sgmm->mm.svc_type];
	if (!fp)
		return RPMI_ERR_NO_DATA;

	return fp(group, service, xport, request_datalen,
		  request_data, response_datalen, response_data);
}
