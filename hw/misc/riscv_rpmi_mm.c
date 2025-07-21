
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "target/riscv/cpu.h"
#include "librpmi.h"

int add_mm_group(struct rpmi_context *rctx, hwaddr mm_shm_addr, int mm_shm_sz);

int add_mm_group(struct rpmi_context *rctx, hwaddr mm_shm_addr, int mm_shm_sz)
{
    struct rpmi_service_group *grp;

    /* Create and add MM service group*/
    grp = rpmi_service_group_mm_create(mm_shm_addr >> 32,
                                       mm_shm_addr & 0xFFFFFFFF, mm_shm_sz);
    if (!grp) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: mm grp create failed\n ", __func__);
        return -1;
    }

    rpmi_context_add_group(rctx, grp);

    return 0;
}
