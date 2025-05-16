#include <librpmi.h>
#include <stddef.h>
#include "librpmi_internal.h"

#define DEBUG  1

#ifdef DEBUG

#include "stdio.h"
#include "stdarg.h"
#endif

#ifdef DEBUG
#define DPRINTF(msg...)         rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

// The payload for this function is SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE.
//
#define SMM_VARIABLE_FUNCTION_GET_VARIABLE  1
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME.
//
#define SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME  2
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE.
//
#define SMM_VARIABLE_FUNCTION_SET_VARIABLE  3
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_QUERY_VARIABLE_INFO.
//
#define SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO  4
//
// It is a notify event, no extra payload for this function.
//
#define SMM_VARIABLE_FUNCTION_READY_TO_BOOT  5
//
// It is a notify event, no extra payload for this function.
//
#define SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE  6
//
// The payload for this function is VARIABLE_INFO_ENTRY. The GUID in EFI_MM_COMMUNICATE_HEADER
// is gEfiSmmVariableProtocolGuid.
//
#define SMM_VARIABLE_FUNCTION_GET_STATISTICS  7
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_LOCK_VARIABLE
//
#define SMM_VARIABLE_FUNCTION_LOCK_VARIABLE  8

#define SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET  9

#define SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET  10

#define SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE  11
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_RUNTIME_VARIABLE_CACHE_CONTEXT
//
#define SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT  12

#define SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE  13
//
// The payload for this function is SMM_VARIABLE_COMMUNICATE_GET_RUNTIME_CACHE_INFO
//
#define SMM_VARIABLE_FUNCTION_GET_RUNTIME_CACHE_INFO  14


//
// This structure is used for SMM variable. the collected statistics data is saved in SMRAM. It can be got from
// SMI handler. The communication buffer should be:
// EFI_MM_COMMUNICATE_HEADER + SMM_VARIABLE_COMMUNICATE_HEADER + payload.
//
typedef struct {
  rpmi_uint64_t         function;
  rpmi_uint64_t    return_status;
  rpmi_uint8_t         data[1];
} SMM_VARIABLE_COMMUNICATE_HEADER;

typedef struct {
  rpmi_uint64_t    variable_payload_size;
} SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE;

//
// EFI Time Abstraction:
//  Year:       2000 - 20XX
//  Month:      1 - 12
//  Day:        1 - 31
//  Hour:       0 - 23
//  Minute:     0 - 59
//  Second:     0 - 59
//  Nanosecond: 0 - 999,999,999
//  TimeZone:   -1440 to 1440 or 2047
//
typedef struct {
  rpmi_uint16_t  year;
  rpmi_uint8_t   month;
  rpmi_uint8_t   day;
  rpmi_uint8_t   hour;
  rpmi_uint8_t   minute;
  rpmi_uint8_t   second;
  rpmi_uint8_t   pad1;
  rpmi_uint32_t  nanosecond;
  rpmi_int16_t   time_zone;
  rpmi_uint8_t   daylight;
  rpmi_uint8_t   pad2;
} EFI_TIME;

///
/// Single Authenticated Variable Data Header Structure.
///
typedef struct {
  ///
  /// Variable Data Start Flag.
  ///
  rpmi_uint16_t      start_id;
  ///
  /// Variable State defined above.
  ///
  rpmi_uint8_t       state;
  rpmi_uint8_t       reserved;
  ///
  /// Attributes of variable defined in UEFI specification.
  ///
  rpmi_uint32_t      attributes;
  ///
  /// Associated monotonic count value against replay attack.
  ///
  rpmi_uint64_t      monotonic_count;
  ///
  /// Associated TimeStamp value against replay attack.
  ///
  EFI_TIME    time_stamp;
  ///
  /// Index of associated public key in database.
  ///
  rpmi_uint32_t      pub_key_index;
  ///
  /// Size of variable null-terminated Unicode string name.
  ///
  rpmi_uint32_t      name_size;
  ///
  /// Size of the variable data without this header.
  ///
  rpmi_uint32_t      data_size;
  ///
  /// A unique identifier for the vendor that produces and consumes this varaible.
  ///
  EFI_GUID    vendor_guid;
} AUTHENTICATED_VARIABLE_HEADER;

typedef struct {
  rpmi_uint16_t    revision;
  rpmi_uint16_t    property;
  rpmi_uint32_t    attributes;
  rpmi_uint64_t     min_size;
  rpmi_uint64_t     max_size;
} VAR_CHECK_VARIABLE_PROPERTY;

typedef struct {
  EFI_GUID                       guid;
  rpmi_uint64_t                          name_size;
  VAR_CHECK_VARIABLE_PROPERTY    variable_property;
  rpmi_int16_t                         name[1];
} SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY;

static rpmi_uint32_t max_variable_size = 0x2800;

enum rpmi_error rpmi_mm_variable_handler(rpmi_uint8_t *comm_buffer, rpmi_uint32_t *comm_buffer_size)
{
	SMM_VARIABLE_COMMUNICATE_HEADER *smm_variable_function_header;
	SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *get_payload_size;
	rpmi_uint32_t tmp_comm_buffer_size, m_variable_buffer_payload_size;
	rpmi_uint32_t comm_buffer_payload_size;
	rpmi_uint32_t status;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);
	if (!comm_buffer || !comm_buffer_size)
		return RPMI_SUCCESS;

	tmp_comm_buffer_size = *comm_buffer_size;

	comm_buffer_payload_size = tmp_comm_buffer_size - sizeof (SMM_VARIABLE_COMMUNICATE_HEADER);
	smm_variable_function_header = (SMM_VARIABLE_COMMUNICATE_HEADER *)comm_buffer;
	DPRINTF("====================================================> "
		"%s: smm_variable_function_header->function=0x%lx \n", __func__, smm_variable_function_header->function);
	switch(smm_variable_function_header->function)
	{
		case SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE:
			m_variable_buffer_payload_size = max_variable_size +
					offsetof(SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, name) -
					sizeof(AUTHENTICATED_VARIABLE_HEADER);
			if (comm_buffer_payload_size < sizeof(SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE)) {
				return RPMI_SUCCESS;
			}

			get_payload_size = (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *)smm_variable_function_header->data;
			get_payload_size->variable_payload_size = m_variable_buffer_payload_size;
			status = EFI_SUCCESS;
			break;
		case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
		case SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME:
		case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
		case SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO:
		case SMM_VARIABLE_FUNCTION_READY_TO_BOOT:
		case SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE:
		case SMM_VARIABLE_FUNCTION_GET_STATISTICS:
		case SMM_VARIABLE_FUNCTION_LOCK_VARIABLE:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET:
		case SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT:
		case SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE:
		case SMM_VARIABLE_FUNCTION_GET_RUNTIME_CACHE_INFO:
		default:
			status = EFI_SUCCESS;
			break;
	}

	smm_variable_function_header->return_status = status;
	return RPMI_SUCCESS;
}
