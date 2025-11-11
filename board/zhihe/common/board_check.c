#include <log.h>
#include <fdt_support.h>

/*****************
 * SPL Build
 *****************/
#ifdef CONFIG_SPL_BUILD
#include <image.h>
#include "include/boot.h"
#include "include/board_porting.h"

/*
 * This function is called in 'spl_perform_fixups'
 *     to pass the hardware type to U-Boot via FDT.
 * Depends on the board_get_fit_config function
 *     to convert the type ID into a type string.
 */
int board_set_binfo_to_fdt(void *fdt_uboot)
{
    int chosen;
    const char *board_name;

    chosen = fdt_find_or_add_subnode(fdt_uboot, 0, "chosen");
    if (chosen >= 0) {
        board_name = board_get_fit_dtb_name(0);
        fdt_setprop_string(fdt_uboot, chosen, "board", board_name);
    } else {
        printf("spl: could not find '/chosen'\n");
        return -1;
    }
    return 0;
}

const char *board_multi_fit_check(const char *suffix)
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
const char *board_get_binfo_from_fdt(void *fdt_uboot)
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
#endif
