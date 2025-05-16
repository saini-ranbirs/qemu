/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_MM_VARIABLE_H__
#define __RPMI_MM_VARIABLE_H__

#include "mm_comm.h"

#define ENABLE_DEBUG  1

#if defined(ENABLE_DEBUG) && ENABLE_DEBUG

#include "stdio.h"
#include "stdarg.h"
#include "string.h"

#define PREFIX_STR  "\n== LIBRPMI: MM ===============================> %s: %u: "

#define DPRINTF(msg...)							\
	{								\
		rpmi_env_printf(PREFIX_STR, __func__, __LINE__);	\
		rpmi_env_printf(msg);					\
	}

#else /* !(defined(ENABLE_DEBUG) && ENABLE_DEBUG) */

#define DPRINTF(msg...)

#endif /* !(defined(ENABLE_DEBUG) && ENABLE_DEBUG) */

enum rpmi_error mm_variable_init(void);
void mm_variable_term(void);

enum rpmi_error mm_variable_handler(void *comm_buf, rpmi_uint64_t bufsize);

#endif /* __RPMI_MM_VARIABLE_H__ */
