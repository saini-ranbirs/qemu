/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_MM_COMMON_H__
#define __RPMI_MM_COMMON_H__

#ifdef DEBUG
#define PREFIX_STR  "== LIBRPMI: MM: =========> %19s: %03u: "

#define DPRINTF(msg...)							\
	{								\
		rpmi_env_printf(PREFIX_STR, __func__, __LINE__);	\
		rpmi_env_printf(msg);					\
		rpmi_env_printf("\n");					\
	}
#else
#define DPRINTF(msg...)
#endif

struct rpmi_service_group_mm {
	rpmi_uint32_t mm_version;
	struct rpmi_mm mm;
	struct rpmi_service_group group;
};

struct rpmi_mm_comm_req {
	rpmi_uint32_t idata_off;
	rpmi_uint32_t idata_len;
	rpmi_uint32_t odata_off;
	rpmi_uint32_t odata_len;
};

typedef enum rpmi_error (*communicate_fp)(struct rpmi_service_group *group,
					  struct rpmi_service *service,
					  struct rpmi_transport *xport,
					  rpmi_uint16_t request_datalen,
					  const rpmi_uint8_t *request_data,
					  rpmi_uint16_t *response_datalen,
					  rpmi_uint8_t *response_data);

enum rpmi_error rpmi_mm_instance_communicate(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *xport,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data);

#endif /* __RPMI_MM_COMMON_H__ */
