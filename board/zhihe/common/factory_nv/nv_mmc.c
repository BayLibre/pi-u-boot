// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <log.h>
#include <command.h>
#include <env.h>
#include <cli.h>
#include <search.h>
#include <env_internal.h>
#include <u-boot/crc.h>

#define FNV_TOTAL_LEN CONFIG_ENV_SIZE
#define FNV_CMD_LEN 2048

static struct _s_fnv_priv {
    char runcmd_buf[FNV_CMD_LEN];
    char export_names[FNV_CMD_LEN];
    char export_buf[FNV_TOTAL_LEN];
    char read_buf[FNV_TOTAL_LEN];
} fnv_priv = { "", "", "", "" };

/* 
 * Get fnv item count
 * binary format ('\0' separated, "\0\0" terminated)
 */
static int get_fnv_count(char *nv_buf, int size)
{
    int nv_count = 0;

    if (nv_buf == NULL) {
        return 0;
    }

    env_t *env_out = (env_t *)nv_buf;

    if (env_out->data[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < size; i++) {
        if (env_out->data[i] == '\0') {
            nv_count++;
            // check end
            if (env_out->data[i + 1] == '\0') {
                break;
            }
        }
    }

    return nv_count;
}

/* 
 * Get factory partiton offset to var $factoryoff
 */
static int get_factory_offset(void)
{
    snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "part start mmc $devnum factory factoryoff");
    debug("[DBG] run %s\n", fnv_priv.runcmd_buf);
    return run_command(fnv_priv.runcmd_buf, 0);
}

/* 
 * Load fnv data to read_buf
 */
static int load_fnv_to_mem(void)
{
    int ret;

    ret = get_factory_offset();

    if (ret != CMD_RET_SUCCESS) {
        return 0;
    }

    /* load data */
    snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "mmc read 0x%lx $factoryoff 0x%x",
             (unsigned long)fnv_priv.read_buf, (FNV_TOTAL_LEN / 512));
    debug("[DBG] run %s\n", fnv_priv.runcmd_buf);
    ret = run_command(fnv_priv.runcmd_buf, 0);

    if (ret != CMD_RET_SUCCESS) {
        return 0;
    }

    /* crc check */
    uint32_t crc;
    env_t *ep = (env_t *)fnv_priv.read_buf;
    memcpy(&crc, &ep->crc, sizeof(crc));
    if (crc32(0, ep->data, ENV_SIZE) != crc) {
        return 0;
    }

    /* Get nv name to export_names */
    int fnv_len = 0;
    char *data_buf = &fnv_priv.read_buf[ENV_HEADER_SIZE];
    char *item_ptr = data_buf;
    char name[64];

    fnv_priv.export_names[0] = '\0';
    for (int i = 0; i < FNV_TOTAL_LEN; i++) {
        char *ch1 = &data_buf[i];
        char *ch2 = &data_buf[i + 1];
        if (*ch1 == '\0') {
            // get name from item
            strcpy(name, item_ptr);
            item_ptr = strchr(name, '=');
            if (item_ptr) {
                *item_ptr = '\0';
                // cat to export_names
                strncat(fnv_priv.export_names, " ", FNV_CMD_LEN);
                strncat(fnv_priv.export_names, name, FNV_CMD_LEN);
            }
            item_ptr = ch2;

            // check end
            if (*ch2 == '\0') {
                fnv_len = (ch2 - fnv_priv.read_buf) + 1;
                break;
            }
        }
    }

    return fnv_len;
}

/* 
 * Print all fnv in list
 */
static int do_fnv_print(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    int ret = CMD_RET_SUCCESS;

    /* load export_names from factory */
    load_fnv_to_mem();

    if (strlen(fnv_priv.export_names) > 0) {
        snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "env print %s", fnv_priv.export_names);
        ret = run_command(fnv_priv.runcmd_buf, 0);
    } else {
        printf("[NV] The list is empty\n");
    }

    return ret;
}

/* 
 * Save fnv to emmc
 */
static int do_fnv_save(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    int ret = CMD_RET_SUCCESS;
    int item_count;
    ssize_t len;
    char *res;

    argc--;
    argv++;

    if (argc <= 0) {
        printf("[NV] The fnv variable list is empty.\n");
        return CMD_RET_FAILURE;
    }

    /* Export fnv@export_names to export_buf */
    env_t *env_out = (env_t *)fnv_priv.export_buf;
    res = (char *)env_out->data;
    len = hexport_r(&env_htab, '\0', H_MATCH_KEY | H_MATCH_IDENT, &res, ENV_SIZE, argc, argv);
    env_out->crc = crc32(0, env_out->data, ENV_SIZE);
#ifdef CONFIG_ENV_ADDR_REDUND
	env_out->flags = ENV_REDUND_ACTIVE;
#endif
    debug("[DBG] export buffer 0x%p(%ld)\n", fnv_priv.export_buf, len);

    if (len <= 0) {
        printf("[NV] Save failed");
        return CMD_RET_FAILURE;
    }

    item_count = get_fnv_count(fnv_priv.export_buf, len);
    if (item_count != argc) {
        printf("[NV] Checked %d items. Does not match input count\n", item_count);
        return CMD_RET_FAILURE;
    }

    ret = get_factory_offset();

    if (ret != CMD_RET_SUCCESS) {
        return ret;
    }

    /* Write export_buf to emmc@FNV_OFFSET */
    printf("[NV] %d item will be saved\n", argc);

    snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "mmc write 0x%lx $factoryoff 0x%x",
                (unsigned long)fnv_priv.export_buf, DIV_ROUND_UP(FNV_TOTAL_LEN, 512));
    debug("[DBG] run %s\n", fnv_priv.runcmd_buf);
    ret = run_command(fnv_priv.runcmd_buf, 0);

    if (ret == CMD_RET_SUCCESS) {
        printf("[NV] Save fnv to uboot env\n");
        run_command("env set first_boot_done yes", 0);
        env_save();
    } else {
        printf("[NV] Save write factory fail\n");
    }

    return ret;
}

/* 
 * Load fnv to uboot env
 */
static int do_fnv_load(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    int ret;

    /* Get fnv from emmc to read_buf & get name to export_names */
    int fnv_len = load_fnv_to_mem();
    if (fnv_len >= 10) {
        printf("[NV] Load fnv(%d): %s\n", fnv_len, fnv_priv.export_names);
    } else {
        printf("[NV] No valid data in factory env.\n");
        return 0;
    }

    /* Force import fnv to uboot env */
    snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "env import -c 0x%lx 0x%x",
             (unsigned long)fnv_priv.read_buf, FNV_TOTAL_LEN);
    debug("[DBG] run %s\n", fnv_priv.runcmd_buf);
    ret = run_command(fnv_priv.runcmd_buf, 0);
    if (ret == CMD_RET_SUCCESS) {
        printf("[NV] Sync fnv to uboot env\n");
        env_save();
    }

    return ret;
}

/* 
 * Erase fnv
 */
static int do_fnv_erase(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    int ret;

    ret = get_factory_offset();

    if (ret != CMD_RET_SUCCESS) {
        return ret;
    }

    snprintf(fnv_priv.runcmd_buf, FNV_CMD_LEN, "mmc erase $factoryoff 0x%x", DIV_ROUND_UP(FNV_TOTAL_LEN, 512));
    debug("[DBG] run %s\n", fnv_priv.runcmd_buf);
    ret = run_command(fnv_priv.runcmd_buf, 0);

    return ret;
}

/* 
 * U-Boot CMD Process
 */
static struct cmd_tbl cmd_env_sub[] = {
    U_BOOT_CMD_MKENT(print, 1, 0, do_fnv_print, "", ""),
    U_BOOT_CMD_MKENT(save, 1, 0, do_fnv_save, "", ""),
    U_BOOT_CMD_MKENT(load, 1, 0, do_fnv_load, "", ""),
    U_BOOT_CMD_MKENT(erase, 1, 0, do_fnv_erase, "", ""),
};

static int do_fnv(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    struct cmd_tbl *cp;

    if (argc < 2)
        return CMD_RET_USAGE;

    /* drop initial "fnv" arg */
    argc--;
    argv++;

    cp = find_cmd_tbl(argv[0], cmd_env_sub, ARRAY_SIZE(cmd_env_sub));

    if (cp)
        return cp->cmd(cmdtp, flag, argc, argv);

    return CMD_RET_USAGE;
}

U_BOOT_CMD(fnv, CONFIG_SYS_MAXARGS, 0, do_fnv, "Factory nv handling commands",
           "save var ...\n"
           "fnv load\n"
           "fnv erase\n"
           "fnv print\n");
