
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "target/riscv/cpu.h"
#include "librpmi.h"

int add_mm_group(struct rpmi_context *rctx, hwaddr shm_addr, int shm_sz);

int add_mm_group(struct rpmi_context *rctx, hwaddr shm_addr, int shm_sz)
{
    struct rpmi_service_group *grp;

    /* Create and add MM service group*/
    grp = rpmi_service_group_mm_create(shm_addr & 0xFFFF, shm_addr >> 32,
                                       shm_sz);
    if (!grp) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: mm grp create failed\n ", __func__);
        return -1;
    }

    rpmi_context_add_group(rctx, grp);

    return 0;
}
