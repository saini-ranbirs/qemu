
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "target/riscv/cpu.h"
#include "librpmi.h"
#include "mm/rpmi_mm_efi.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)    (sizeof(x) / sizeof((x)[0]))
#endif

#define MAX_VAR_NUM  50    /* Total number of MM variables */

static rpmi_uint8_t var_store[MAX_VAR_NUM][MAX_VARINFO_SIZE];
static rpmi_uint8_t var_count;
static rpmi_uint8_t var_ptr;

struct asciihex2char_lut {
    rpmi_uint16_t hexval;
    char cval;
};

static struct asciihex2char_lut ascii_charmap[] = {
    { 0x00, ' ' },
    { 0x30, '0' }, { 0x31, '1' }, { 0x32, '2' }, { 0x33, '3' }, { 0x34, '4' },
    { 0x35, '5' }, { 0x36, '6' }, { 0x37, '7' }, { 0x38, '8' }, { 0x39, '9' },
    { 0x20, ' ' },
    { 0x41, 'A' }, { 0x42, 'B' }, { 0x43, 'C' }, { 0x44, 'D' }, { 0x45, 'E' },
    { 0x46, 'F' }, { 0x47, 'G' }, { 0x48, 'H' }, { 0x49, 'I' }, { 0x4A, 'J' },
    { 0x4B, 'K' }, { 0x4C, 'L' }, { 0x4D, 'M' }, { 0x4E, 'N' }, { 0x4F, 'O' },
    { 0x50, 'P' }, { 0x51, 'Q' }, { 0x52, 'R' }, { 0x53, 'S' }, { 0x54, 'T' },
    { 0x55, 'U' }, { 0x56, 'V' }, { 0x57, 'W' }, { 0x58, 'X' }, { 0x59, 'Y' },
    { 0x5A, 'Z' },
    { 0x61, 'a' }, { 0x62, 'b' }, { 0x63, 'c' }, { 0x64, 'd' }, { 0x65, 'e' },
    { 0x66, 'f' }, { 0x67, 'g' }, { 0x68, 'h' }, { 0x69, 'i' }, { 0x6A, 'j' },
    { 0x6B, 'k' }, { 0x6C, 'l' }, { 0x6D, 'm' }, { 0x6E, 'n' }, { 0x6F, 'o' },
    { 0x70, 'p' }, { 0x71, 'q' }, { 0x72, 'r' }, { 0x73, 's' }, { 0x74, 't' },
    { 0x75, 'u' }, { 0x76, 'v' }, { 0x77, 'w' }, { 0x78, 'x' }, { 0x79, 'y' },
    { 0x7A, 'z' },
};

static void mm_memdump(const void *src, rpmi_size_t count, const char *name)
{
#define MAX_BYTES_PER_LINE  16
#define MAX_LINES_PER_DUMP  64

    size_t remaining = count, line_ctr = 0, byte_ctr, byte_ctr_max;
    unsigned char buf_data[MAX_BYTES_PER_LINE];
    const char *temp = src;

    if (!remaining) {
        return;
    }

    info_report("MM: %s: %06lu", name, count);

    while (remaining) {
        byte_ctr = 0;

        byte_ctr_max = (remaining >= MAX_BYTES_PER_LINE) ?
            MAX_BYTES_PER_LINE : remaining;

        while (byte_ctr < byte_ctr_max) {
            buf_data[byte_ctr] = *(temp + byte_ctr);
            byte_ctr++;
        }

        /* For simplicity, fill rest with ZERO's if required */
        while (byte_ctr < MAX_BYTES_PER_LINE) {
            buf_data[byte_ctr] = 0x00;
            byte_ctr++;
        }

        if (line_ctr < MAX_LINES_PER_DUMP) {
            info_report("%06lu: "
                        "%02X%02X %02X%02X %02X%02X %02X%02X "
                        "%02X%02X %02X%02X %02X%02X %02X%02X",
                        count - remaining,
                        buf_data[1], buf_data[0], buf_data[3], buf_data[2],
                        buf_data[5], buf_data[4], buf_data[7], buf_data[6],
                        buf_data[9], buf_data[8], buf_data[11], buf_data[10],
                        buf_data[13], buf_data[12], buf_data[15], buf_data[14]);
        }

        temp = temp + byte_ctr_max;
        remaining = remaining - byte_ctr_max;
        line_ctr++;
    }

    info_report("%06lu", count - remaining);
}


static void print_variable_name(rpmi_uint16_t *ascii_varname,
                                rpmi_uint16_t ascii_namesize)
{
    char text_varname[512];
    rpmi_uint16_t namesize;
    rpmi_uint16_t i, j;

    namesize = (ascii_namesize / 2) + (ascii_namesize % 2) - 1;

    if (namesize >= ARRAY_SIZE(text_varname)) {
        info_report("Namesize assumption breached as %d > %ld",
                    namesize, ARRAY_SIZE(text_varname));
        namesize = ARRAY_SIZE(text_varname) - 1;
    }

    for (i = 0; i < namesize; i++) {
        for (j = 0; j < ARRAY_SIZE(ascii_charmap); j++) {
            if (ascii_charmap[j].hexval == ascii_varname[i]) {
                text_varname[i] = ascii_charmap[j].cval;
                break;
            }
        }

        if (j == ARRAY_SIZE(ascii_charmap)) {
            info_report("Unmapped hexval = 0x%x", ascii_varname[i]);
            text_varname[i] = '-';
        }
    }

    text_varname[i] = '\0';

    info_report("Var Size = %d Name = %s", ascii_namesize, text_varname);
}

static rpmi_uint64_t find_var_raw_ram(rpmi_uint16_t *varname,
                                      struct rpmi_guid_t *vndr_guid)
{
    struct efi_var_access_variable *var;

    if (varname[0] == 0) {
        info_report("EFI_SUCCESS 0 - Var Ptr = %d", var_ptr);
        return EFI_SUCCESS;
    }

    while (var_ptr < var_count) {
        var = (struct efi_var_access_variable *)&var_store[var_ptr];
        if (!rpmi_env_memcmp(vndr_guid, &var->guid, GUID_LENGTH)) {
            if (!rpmi_env_memcmp(varname, var->name, var->namesize)) {
                info_report("EFI_SUCCESS 1 - Var Ptr = %d", var_ptr);
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
        info_report("EFI_SUCCESS 2 - Var Ptr = %d", var_ptr);
    }

    return (var_ptr >= var_count) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

static rpmi_uint64_t get_var_raw_ram(void *priv, const rpmi_uint8_t *data,
                                     rpmi_uint32_t datasize)
{
    struct efi_var_access_variable *var1, *var2;
    rpmi_uint64_t status;
    void *var1_data;

    info_report("get_var_raw_ram");
    var_ptr = 0;
    var1 = (struct efi_var_access_variable *)data;

    print_variable_name(var1->name, var1->namesize);
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

    info_report("get_next_var_name_raw_ram");
    var_ptr = 0;
    var = (struct efi_var_get_next_var_name *)data;

    print_variable_name(var->name, var->namesize);
    status = find_var_raw_ram(var->name, &var->guid);
    info_report("Status = 0x%lx Var Ptr = %d Var Count = %d",
                status, var_ptr, var_count);
    if (EFI_ERROR(status)) {
        /*
         * For VariableName is an empty string, find_var_raw_ram() will try to
         * find & return the first qualified variable, and if find_var_raw_ram()
         * returns error (EFI_NOT_FOUND) as no any variable is found, still
         * return the error (EFI_NOT_FOUND).
         */
        if (var->name[0] != 0) {
            /*
             * For VariableName is not an empty string, and find_var_raw_ram()
             * returns error as VariableName and VendorGuid are not a name and
             * GUID of an existing variable, there is no way to get next
             * variable, follow spec to return EFI_INVALID_PARAMETER.
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
            rpmi_env_memcpy(&var->guid, &avar->guid, sizeof(var->guid));
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

    info_report("set_var_raw_ram");
    var1 = (struct efi_var_access_variable *)data;
    print_variable_name(var1->name, var1->namesize);

    /* Check if same Vendor GUID and Variable Name pre-exists */
    while (count < var_count) {
        /* Check if Vendor GUID matches first */
        if (rpmi_env_memcmp((void *)data, &var_store[count],
                            sizeof(struct rpmi_guid_t)) != 0) {
            count++;
            continue;
        }

        var2 = (struct efi_var_access_variable *)&var_store[count];

        /* Now check Variable Name match */
        if ((var1->namesize == var2->namesize) &&
            (rpmi_env_memcmp(var1->name, var2->name, var1->namesize) == 0)) {
            /* Match found, update the existing data */
            if (datasize) {
                rpmi_env_memcpy(&var_store[count], data, datasize);
                info_report("Updated Variable: Pos = %d", count);
            }

            break;
        }

        count++;
    }

    if (count == var_count) {
        if (var_count == MAX_VAR_NUM) {
            return EFI_ACCESS_DENIED;
        }

        rpmi_env_memcpy(&var_store[count], data, datasize);
        var_count++;
        mm_memdump(&var_store[count], datasize, "VS");
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

int add_mm_group(struct rpmi_context *rctx, struct rpmi_shmem *mm_shmem);

int add_mm_group(struct rpmi_context *rctx, struct rpmi_shmem *mm_shmem)
{
    struct rpmi_service_group *grp;
    struct rpmi_mm_efi mmefi;
    enum rpmi_error status;

    /* Create and add MM EFI service group */
    mmefi.ops = &efi_ops;
    mmefi.ops_priv = rctx;

    grp = rpmi_service_group_mm_create(mm_shmem);
    if (!grp) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: mm grp create failed\n ", __func__);
        return -1;
    }

    status = register_rpmi_mm_efi_service(grp, &mmefi);
    if (status) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: mm efi service registration failed\n ", __func__);
        return -1;
    }

    rpmi_context_add_group(rctx, grp);

    return 0;
}
