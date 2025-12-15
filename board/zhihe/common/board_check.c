#include <log.h>
#include <fdt_support.h>
#include <env.h>

#include "include/board_boot.h"
#include "include/board_check.h"

/*****************
 * SPL Build
 *****************/
#ifdef CONFIG_SPL_BUILD
#include <image.h>
#include "include/board_porting.h"

/*
 * board type check
 */
/*
Attention:
The following variable must not be initialized to zero.
This global variable is assigned in the 'f' stage and
must persist into the 'r' stage of the SPL.
If it is initialized to zero and becomes a BSS variable,
it will be re-zeroed upon entering the 'r' stage, causing data loss.
*/
static enum board_type _board_type = BOARD_UNKNOWN;
static enum ddr_type _ddr_type = DDR_TYPE_UNKNOWN;

int spl_set_board_info(enum board_type typeboard, enum ddr_type typeddr)
{
    _board_type = typeboard;
    _ddr_type = typeddr;

    return 0;
}

enum board_type spl_get_board_type(void)
{
	return _board_type;
}

enum ddr_type spl_get_ddr_type(void)
{
	return _ddr_type;
}

/*
 * This function is called in 'spl_perform_fixups'
 *     to pass the hardware type to U-Boot via FDT.
 * Depends on the board_get_fit_config function
 *     to convert the type ID into a type string.
 */
int spl_set_binfo_to_uboot_fdt(void *fdt_uboot)
{
    int chosen;
    const char *board_name;

    chosen = fdt_find_or_add_subnode(fdt_uboot, 0, "chosen");
    if (chosen >= 0) {
        /* baord name */
        board_name = spl_get_fit_dtb_name(0);
        if (board_name) {
            fdt_setprop_string(fdt_uboot, chosen, "board", board_name);
        }

        /* real dtb name */
        board_name = spl_get_fit_dtb_name(1);
        if (board_name) {
            fdt_setprop_string(fdt_uboot, chosen, "board-dtb", board_name);
        }
    } else {
        printf("spl: could not find '/chosen'\n");
        return -1;
    }
    return 0;
}

const char *spl_multi_fit_check(const char *suffix)
{
    int noffset;
    const char *defconf;
    int suffix_len;
    int defconf_len;

    if (suffix == NULL) {
        return NULL;
    }

    /* Get default cfg from fit */
    noffset = fit_conf_get_node((void*)CONFIG_SYS_LOAD_ADDR, NULL);
    defconf = fdt_get_name((void*)CONFIG_SYS_LOAD_ADDR, noffset, NULL);
    if (defconf == NULL) {
        return NULL;
    }

    suffix_len = strlen(suffix);
    defconf_len = strlen(defconf);
    if (defconf_len < suffix_len) {
        return NULL;
    }

    const char *str = &defconf[defconf_len - suffix_len];
    if (strcmp(str, suffix) != 0) {
        return NULL;
    }

    return suffix;
}

#else

/*****************
 * U-Boot Build
 *****************/
/*
 * This function is called in 'board_init'
 *     to obtain the current hardware type and
 *     address differences in hardware initialization.
 */
const char *uboot_get_binfo_from_fdt(void *fdt_uboot)
{
    const char *name = NULL;
    int chosen_node = 0;

    if (fdt_uboot) {
        chosen_node = fdt_path_offset(fdt_uboot, "/chosen");
        name = fdt_getprop(fdt_uboot, chosen_node, "board", NULL);
    } else {
        printf("%s(%d) get uboot fdt blob failed.\n", __func__, __LINE__);
    }
    return name;
}

const char *uboot_sync_fdt_binfo_to_env(void *fdt_uboot)
{
    const char *name = NULL;
    int chosen_node = 0;
    char *value;
    char dtb_file[MAX_DTB_FILENAME_LEN];

    if (fdt_uboot) {
        chosen_node = fdt_path_offset(fdt_uboot, "/chosen");
        name = fdt_getprop(fdt_uboot, chosen_node, "board-dtb", NULL);
        value = env_get("dtb_file");
        if (value && (value[0] == '\0')) {
            value = NULL;
        }
        if (name && (value == NULL)) {
            sprintf(dtb_file, "%s.dtb", name);
            env_set("dtb_file", dtb_file);
        }
    }
    return name;
}
#endif
