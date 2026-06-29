// SPDX-License-Identifier: GPL-2.0+
#include <dm.h>
#include <i2c.h>
#include <errno.h>

/* Target register and value */
#define CTF2301_PWM_VALUE           0x4c
#define CTF2301_PWM_MAX_VALUE       0xff

static int ctf2301_probe(struct udevice *dev)
{
    u8 write_val = CTF2301_PWM_MAX_VALUE;
    int ret;

    debug("CTF2301: Start probing %s\n", dev->name);

    ret = dm_i2c_write(dev, CTF2301_PWM_VALUE, &write_val, 1);
    if (ret) {
        debug("CTF2301: Write failed, err=%d\n", ret);
        return ret;
    }

    return 0;
}

static const struct udevice_id ctf2301_ids[] = {
    { .compatible = "sensylink,ctf2301" },
    { }
};

U_BOOT_DRIVER(ctf2301) = {
    .name       = "ctf2301",
    .id         = UCLASS_MISC,
    .of_match   = ctf2301_ids,
    .probe      = ctf2301_probe,
    .flags      = DM_FLAG_PROBE_AFTER_BIND, 
};
