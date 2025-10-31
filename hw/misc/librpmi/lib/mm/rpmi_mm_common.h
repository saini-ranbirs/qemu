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

struct rpmi_mm_comm_req {
	rpmi_uint32_t idata_off;
	rpmi_uint32_t idata_len;
	rpmi_uint32_t odata_off;
	rpmi_uint32_t odata_len;
};

typedef enum rpmi_error
(*communicate_fp)(rpmi_uint64_t shmem_addr, rpmi_uint32_t shmem_size,
		  union rpmi_mm_instance_platform_ops *mmipops,
		  struct rpmi_service *service, struct rpmi_transport *xport,
		  rpmi_uint16_t req_datalen, const rpmi_uint8_t *req_data,
		  rpmi_uint16_t *rsp_datalen, rpmi_uint8_t *rsp_data);

enum rpmi_error rpmi_mm_instance_communicate(struct rpmi_mm *mm,
					     struct rpmi_service *service,
					     struct rpmi_transport *xport,
					     rpmi_uint16_t req_datalen,
					     const rpmi_uint8_t *req_data,
					     rpmi_uint16_t *rsp_datalen,
					     rpmi_uint8_t *rsp_data);

enum rpmi_error rpmi_mm_instance_meminfo(struct rpmi_mm *mm,
					 rpmi_uint64_t *mem_addr,
					 rpmi_uint32_t *mem_size);

#endif /* __RPMI_MM_COMMON_H__ */
