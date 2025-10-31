
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "target/riscv/cpu.h"
#include "librpmi.h"

#define VAR_MAX_INFOSIZE  1024  /* Max information size per MM variable */
#define VAR_MAX_NUM       50    /* Total number of MM variables */

static rpmi_uint8_t var_store[VAR_MAX_NUM][VAR_MAX_INFOSIZE];
static rpmi_uint8_t var_count;
static rpmi_uint8_t var_ptr;

static rpmi_uint64_t find_var_raw_ram(rpmi_uint16_t *varname,
                                      struct efi_guid *vendor_guid)
{
    struct efi_var_access_variable *var;

    if (varname[0] == 0) {
        info_report("EFI_SUCCESS Var Ptr = %d", var_ptr);
        return EFI_SUCCESS;
    }

    while (var_ptr < var_count) {
        var = (struct efi_var_access_variable *)&var_store[var_ptr];
        if (!rpmi_env_memcmp(vendor_guid, &var->guid, GUID_LENGTH)) {
            // ASSERT ((var->namesize) != 0);
            if (!rpmi_env_memcmp(varname, var->name, var->namesize)) {
                info_report("EFI_SUCCESS Var Ptr = %d", var_ptr);
                return EFI_SUCCESS;
            } else {
                //info_report("Variable Name mismatch");
            }
        } else {
            //info_report("Vendor GUID mismatch");
        }

        var_ptr++;
    }

    if (var_ptr >= var_count) {
        info_report("EFI_NOT_FOUND Var Ptr = %d", var_ptr);
    } else {
        info_report("EFI_SUCCESS Var Ptr = %d", var_ptr);
    }

    return (var_ptr >= var_count) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

static rpmi_uint64_t get_var_raw_ram(void *priv, const rpmi_uint8_t *data,
                                     rpmi_uint32_t datasize)
{
    struct efi_var_access_variable *var1, *var2;
    rpmi_uint64_t status;
    void *var1_data;

    var_ptr = 0;
    var1 = (struct efi_var_access_variable *)data;

    status = find_var_raw_ram(var1->name, &var1->guid);
    if (EFI_ERROR(status)) {
        return status;
    }

    // Get data size
    var1_data = (rpmi_uint8_t *)var1->name + var1->namesize;
    var2 = (struct efi_var_access_variable *)&var_store[var_ptr];
    if (var2->datasize && (var1->datasize >= var2->datasize)) {
        if (var1_data == NULL) {
            status = EFI_INVALID_PARAMETER;
            goto done;
        }

        rpmi_env_memcpy(var1_data, (rpmi_uint8_t *)var2->name + var2->namesize,
                        var2->datasize);
        status = EFI_SUCCESS;
    } else {
        status = EFI_BUFFER_TOO_SMALL;
    }

    info_report("var1->datasize = %ld var2->datasize = %ld",
                var1->datasize, var2->datasize);
    var1->datasize = var2->datasize;

done:
    if ((status == EFI_SUCCESS) || (status == EFI_BUFFER_TOO_SMALL)) {
        if (var_ptr < var_count) {
            var1->attr = var2->attr;
        }

        rpmi_env_memcpy((void *)data, &var_store[var_ptr], datasize);
    }

    return status;
}

static rpmi_uint64_t get_next_var_name_raw_ram(void *priv,
                                               const rpmi_uint8_t *data,
                                               rpmi_uint32_t datasize)
{
    struct efi_var_get_next_var_name *var;
    rpmi_uint64_t status;

    var_ptr = 0;
    var = (struct efi_var_get_next_var_name *)data;

    status = find_var_raw_ram(var->name, &var->guid);
    info_report("Status = 0x%lx Var Ptr = %d Var Count = %d",
                status, var_ptr, var_count);
    if (EFI_ERROR(status)) {
        /*
         * For VariableName is an empty string, find_var_raw_ram() will
         * try to find and return the first qualified variable, and if
         * find_var_raw_ram() returns error (EFI_NOT_FOUND) as no any
         * variable is found, still return the error (EFI_NOT_FOUND).
         */
        if (var->name[0] != 0) {
            /*
             * For VariableName is not an empty string, and
             * find_var_raw_ram() returns error as VariableName and
             * VendorGuid are not a name and GUID of an existing
             * variable, there is no way to get next variable,
             * follow spec to return EFI_INVALID_PARAMETER.
             */
            status = EFI_INVALID_PARAMETER;
            info_report("Status = 0x%lx Var Ptr = %d", status, var_ptr);
        }

        info_report("Status = 0x%lx Var Ptr = %d", status, var_ptr);
        goto done;
    }

    if (var->name[0] != 0) {
        // If variable name is not empty, get next variable.
        var_ptr++;
        if (var_ptr >= var_count) {
            status = EFI_NOT_FOUND;
        }

        info_report("Status = 0x%lx Var Ptr = %d", status, var_ptr);
        goto done;
    }

done:
    if (!EFI_ERROR(status) && (var_ptr < var_count)) {
        struct efi_var_access_variable *avar;

        avar = (struct efi_var_access_variable *)&var_store[var_ptr];
        if (avar->namesize && (avar->namesize <= var->namesize)) {
            rpmi_env_memcpy(var->name, avar->name, avar->namesize);
            rpmi_env_memcpy(&var->guid, &avar->guid, sizeof(struct efi_guid));
            status = EFI_SUCCESS;
            info_report("EFI_SUCCESS");
        } else {
            status = EFI_BUFFER_TOO_SMALL;
            info_report("EFI_BUFFER_TOO_SMALL i/p size = %ld o/p size = %ld",
                        var->namesize, avar->namesize);
        }

        var->namesize = avar->namesize;
    }

    return status;
}

static rpmi_uint64_t set_var_raw_ram(void *priv, const rpmi_uint8_t *data,
                                     rpmi_uint32_t datasize)
{
    struct efi_var_access_variable *var1, *var2;
    rpmi_uint8_t count = 0;

    var1 = (struct efi_var_access_variable *)data;

    /* Check if same Vendor GUID and Variable Name pre-exists */
    while (count < var_count) {
        /* Check if Vendor GUID matches first */
        if (rpmi_env_memcmp((void *)data, &var_store[count],
                            sizeof(struct efi_guid)) != 0) {
            count++;
            continue;
        }

        var2 = (struct efi_var_access_variable *)&var_store[count];

        /* Now check Variable Name match */
        if ((var1->namesize == var2->namesize) &&
            (rpmi_env_memcmp(var1->name, var2->name, var1->namesize) == 0)) {
            /* Match found, update the existing data */
            rpmi_env_memcpy(&var_store[count], data, datasize);
            info_report("Updated Variable: Pos = %d", count);
            break;
        }

        count++;
    }

    if (count == var_count) {
        rpmi_env_memcpy(&var_store[count], data, datasize);
        var_count++;
        info_report("Added Variable: Attributes = 0x%x Var Count = %d",
                    var1->attr, var_count);
    }

    return EFI_SUCCESS;
}

struct rpmi_mm_efi_platform_ops efi_ops = {
    .get_variable = get_var_raw_ram,
    .get_next_variable_name = get_next_var_name_raw_ram,
    .set_variable = set_var_raw_ram,
};

int add_mm_group(struct rpmi_context *rctx, hwaddr mmshm_addr, int mmshm_size);

int add_mm_group(struct rpmi_context *rctx, hwaddr mmshm_addr, int mmshm_size)
{
    union rpmi_mm_instance_platform_ops mmipops;
    struct rpmi_service_group *grp;
    struct rpmi_mm *mm;

    /* Create and add MM EFI service group */
    mmipops.inst_efi.ops = &efi_ops;
    mmipops.inst_efi.ops_priv = rctx;

    mm = rpmi_mm_create(mmshm_addr, mmshm_size, RPMI_MM_INSTANCE_EFI, &mmipops);
    grp = rpmi_service_group_mm_create(mm);
    if (!grp) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: mm grp create failed\n ", __func__);
        return -1;
    }

    rpmi_context_add_group(rctx, grp);

    return 0;
}
