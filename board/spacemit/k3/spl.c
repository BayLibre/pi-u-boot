// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025, Spacemit
 */

#include <dm.h>
#include <init.h>
#include <spl.h>
#include <misc.h>
#include <log.h>
#include <linux/delay.h>
#include <remoteproc.h>
#include <image.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <mapmem.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#include <i2c.h>
#if CONFIG_IS_ENABLED(ESPI)
#include <espi.h>
#endif
#include <tlv_eeprom.h>
#include <linux/lzo.h>
#include <linux/string.h>
#include <vsprintf.h>
#include <dt-bindings/pinctrl/k3-pinctrl.h>
#include <asm/sections.h>
#include <u-boot/crc.h>
#include <clk.h>

#if defined(CONFIG_K3_BOARD_FPGA)
#define GDB_DOWNLOAD_DEBUG
#endif

#define LZOP_HEADER_HAS_FILTER	0x00000800UL

/* MFPR (Multi-Function Pin Register) definitions */
#define MFPR_BASE          0xD401E000
#define MMC1_DAT3_MFPR     (MFPR_BASE + 0x218)
#define MMC1_DAT2_MFPR     (MFPR_BASE + 0x21C)
#define MMC1_DAT1_MFPR     (MFPR_BASE + 0x220)
#define MMC1_DAT0_MFPR     (MFPR_BASE + 0x224)
#define MMC1_CMD_MFPR      (MFPR_BASE + 0x228)
#define MMC1_CLK_MFPR      (MFPR_BASE + 0x22C)

extern void spl_fixup_fdt(void *fdt_blob);
#if CONFIG_IS_ENABLED(SPACEMIT_POWER)
extern int board_pmic_init(void);
#endif
extern enum board_boot_mode get_boot_mode(void);
extern void update_usb_serial_number(void);
extern int get_tlvinfo(uint8_t id, uint8_t *buffer, int max_size);
extern ulong read_ddr_training_info(struct ddr_info_t *info);

static void spl_load_env(void);

/**
 * mfpr_set_pin_mux - Set pin mux mode via MFPR register
 * @reg_addr: MFPR register physical address
 * @mux_mode: Mux mode value (MUX_MODE0~MUX_MODE7)
 *
 * Note: This function directly writes MFPR registers without using
 * pinctrl framework, which is intended for early SPL stage before
 * pinctrl driver initialization.
 */
static void mfpr_set_pin_mux(uintptr_t reg_addr, u32 mux_mode)
{
	u32 reg_val;

	reg_val = readl((void __iomem *)reg_addr);
	reg_val &= ~MUX_MODE7;  /* Clear mux mode bits [2:0] */
	reg_val |= mux_mode;
	writel(reg_val, (void __iomem *)reg_addr);
}

/**
 * setup_debug_jtag_on_mmc1_pins - Configure MMC1 pins as JTAG interface
 *
 * This function reconfigures MMC1 data/cmd/clk pins to JTAG mode (MUX_MODE5)
 * for hardware debugging purposes. Only call this when explicitly enabled
 * via device tree property "spacemit,enable-debug-jtag".
 *
 * Pin mapping (MUX_MODE5):
 *   MMC1_DAT3 -> JTAG_TDI
 *   MMC1_DAT2 -> JTAG_TDO
 *   MMC1_DAT1 -> JTAG_TMS
 *   MMC1_DAT0 -> JTAG_TCK
 *   MMC1_CMD  -> (Reserved for JTAG)
 *   MMC1_CLK  -> (Reserved for JTAG)
 */
static void setup_debug_jtag_on_mmc1_pins(void)
{
	mfpr_set_pin_mux(MMC1_DAT3_MFPR, MUX_MODE5);
	mfpr_set_pin_mux(MMC1_DAT2_MFPR, MUX_MODE5);
	mfpr_set_pin_mux(MMC1_DAT1_MFPR, MUX_MODE5);
	mfpr_set_pin_mux(MMC1_DAT0_MFPR, MUX_MODE5);
	mfpr_set_pin_mux(MMC1_CMD_MFPR, MUX_MODE5);
	mfpr_set_pin_mux(MMC1_CLK_MFPR, MUX_MODE5);

	printf("Debug JTAG enabled on MMC1 pins\n");
}

void restore_ddr_pma_attribute(void)
{
	csr_write(CSR_PMAADDR8, SYS_SDRAM_UPPER_LIMIT_ADDR >> 2);
	asm("sfence.vma zero, zero");
}

void set_audio_buffer_cacheable(void)
{
	/* Audio buffer was set to IO attribute in boot rom,
	Need to restore it to cachable attribute, so it can
	be used as data buffer or instruction buffer.
	*/
	csr_clear(CSR_PMACFG0, 0xFFUL << 48);
	csr_set(CSR_PMACFG0, 0x20UL << 48);
	asm("sfence.vma zero, zero");
}

int get_product_name(char *name, int max_size)
{
	if (NULL == name)
		return EINVAL;

	memset(name, 0, max_size);
	if (get_tlvinfo(TLV_CODE_PRODUCT_NAME, name, max_size) > 0) {
		pr_info("Get product name from eeprom %s\n", name);
		return 0;
	}

	// Use default product name
	strlcpy(name, DEFAULT_PRODUCT_NAME, max_size);
	return 0;
}

bool restore_ddr_training_info(void)
{
	struct ddr_info_t* info;

	info = (struct ddr_info_t*)DDR_TRAINING_INFO_BUFF;
	/* Force to do DDR fully training as any condition is met:
	  USB download mode
	  SD card boot mode
	  training info is invalid
	*/
	if ((BOOT_MODE_USB == get_boot_mode())
		|| (BOOT_MODE_SD == get_boot_mode())
		|| (sizeof(*info) != read_ddr_training_info(info))
		|| (DDR_TRAINING_INFO_MAGIC != info->magic)
		|| (info->crc32 != crc32(0, (const uint8_t*)&info->chipid, sizeof(*info) - 8))) {
		// clear magic, set invalid
		memset(info, 0, 128);
		return false;
	}

	return true;
}

#define TURBO0_FREQUENCY		(1000000000)

static int cpu_frequency_set(void)
{
	int ret;
	unsigned int cluster0_frequency;
	unsigned int cluster1_frequency;
	unsigned int cluster2_frequency;
	unsigned int cluster3_frequency;
	unsigned int topd_frequency;
	unsigned int axi_frequency;
	unsigned int cci_frequency;
	struct clk top_dclk, axi_clk, cci_clk, cluster0_clk, cluster1_clk, cluster2_clk, cluster3_clk, clk_pll3, clk_pll4, clk_pll5, clk_pll8, pll_src3, clt1_pll_src, pll_src5, clt3_pll_src;
	ofnode cpu_node;

	cpu_node = ofnode_path("/cpus");
	if (!ofnode_valid(cpu_node)) {
		debug("No cpus node found\n");
		return -1;
	}

	ret = clk_get_by_name_nodev(cpu_node, "cluster0", &cluster0_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "cluster1", &cluster1_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "cluster2", &cluster2_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "cluster3", &cluster3_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "topd", &top_dclk);
	ret |= clk_get_by_name_nodev(cpu_node, "axi", &axi_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "cci", &cci_clk);
	ret |= clk_get_by_name_nodev(cpu_node, "pll3", &clk_pll3);
	ret |= clk_get_by_name_nodev(cpu_node, "pll4", &clk_pll4);
	ret |= clk_get_by_name_nodev(cpu_node, "pll5", &clk_pll5);
	ret |= clk_get_by_name_nodev(cpu_node, "pll8", &clk_pll8);
	ret |= clk_get_by_name_nodev(cpu_node, "pll_src3", &pll_src3);
	ret |= clk_get_by_name_nodev(cpu_node, "clt1_pll_src", &clt1_pll_src);
	ret |= clk_get_by_name_nodev(cpu_node, "pll_src5", &pll_src5);
	ret |= clk_get_by_name_nodev(cpu_node, "clt3_pll_src", &clt3_pll_src);
	if (ret) {
		pr_err("Get cluster clk error\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "cluster0_frequency",
			      (u32 *)&cluster0_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "cluster1_frequency",
			      (u32 *)&cluster1_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "cluster2_frequency",
			      (u32 *)&cluster2_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "cluster3_frequency",
			      (u32 *)&cluster3_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "cci_frequency",
			      (u32 *)&cci_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "topd_frequency",
			      (u32 *)&topd_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	ret = ofnode_read_u32(cpu_node, "axi_frequency",
			      (u32 *)&axi_frequency);
	if (ret) {
		debug("Can't get cpufrequency\n");
		return -1;
	}

	if ((cluster0_frequency != cluster1_frequency) ||
			(cluster2_frequency != cluster3_frequency)) {
		printk("Cluster0/2 should be same as Cluster1/3")	;
		return -1;
	}

	clk_enable(&cluster0_clk);
	clk_enable(&cluster1_clk);
	clk_enable(&cluster2_clk);
	clk_enable(&cluster3_clk);
	clk_enable(&top_dclk);
	clk_enable(&axi_clk);
	clk_enable(&cci_clk);
	clk_enable(&clk_pll3);
	clk_enable(&clk_pll4);
	clk_enable(&clk_pll5);
	clk_enable(&clk_pll8);

	clk_set_rate(&top_dclk, topd_frequency);
	clk_set_rate(&axi_clk, axi_frequency);

	if (cluster0_frequency > TURBO0_FREQUENCY) {
		clk_set_rate(&clk_pll3, cluster0_frequency);
		clk_set_rate(&clk_pll4, cluster0_frequency);
	}

	clk_set_rate(&cluster0_clk, cluster0_frequency);

	clk_set_parent(&clt1_pll_src, &pll_src3);

	clk_set_rate(&cluster1_clk, cluster1_frequency);

	if (cluster2_frequency > TURBO0_FREQUENCY) {
		clk_set_rate(&clk_pll5, cluster2_frequency);
		clk_set_rate(&clk_pll8, cluster2_frequency);
	}

	clk_set_rate(&cluster2_clk, cluster2_frequency);
	clk_set_parent(&clt3_pll_src, &pll_src5);
	clk_set_rate(&cluster3_clk, cluster3_frequency);
	clk_set_rate(&cci_clk, cci_frequency);

	return 0;
}

int spl_board_init_f(void)
{
	int ret;
	struct udevice *dev;

	/*
	 * Do NOT clear the BSS here: the RISC-V SPL crt0 (start.S
	 * spl_clear_bss) already zeroes it before board_init_f. The vendor
	 * cleared it here because its start.S had spl_clear_bss commented out;
	 * upstream runs both, and this second clear lands after
	 * preloader_console_init() — wiping the live console/DM state and
	 * hanging the first post-banner printf.
	 */

#if CONFIG_IS_ENABLED(SYS_I2C_LEGACY)
	/* init i2c */
	i2c_init_board();
#endif

	/* use audio buffer as temp data buffer during DDR initialization */
	set_audio_buffer_cacheable();
#ifdef CONFIG_DDR_TRAINING_SAVE_RESTORE
	restore_ddr_training_info();
#endif
	/* DDR init */
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		return ret;
	}

	/*
	 * Extend PMA entry 8 to cover the full DRAM range, only now that the
	 * controller is trained -- matching the vendor order. Doing it before
	 * the probe leaves the untrained DRAM region mapped during PHY
	 * training (lpddr_silicon_init) and hangs it.
	 */
	restore_ddr_pma_attribute();

	/*
	 * Check if debug JTAG is enabled via device tree
	 * Add "spacemit,enable-debug-jtag;" to root node in DTS to enable
	 */
	if (ofnode_read_bool(ofnode_root(), "spacemit,enable-debug-jtag")) {
		setup_debug_jtag_on_mmc1_pins();
	} else {
		debug("SPL: Debug JTAG disabled (not configured in DTS)\n");
	}

#ifdef GDB_DOWNLOAD_DEBUG
	u32 read_data;

	/* Wait for boot image was downloaded by gdb */
	printf("wait image\n");
	read_data = readl((void*)0xc0800000);
	while(read_data != 0xa55a)
		read_data = readl((void*)0xc0800000);
	printf("get new image\n");
#endif

#if CONFIG_IS_ENABLED(SPACEMIT_POWER)
	/*
	 * Bring up the PMIC rails before raising clocks. board_pmic_init()
	 * programs the boot-on spm8821 buck / LDO rails (the core rail) over
	 * legacy I2C, so the cluster PLLs can lock at their target frequency.
	 * Without it the core rail stays at the low BootROM default and
	 * ccu_pll_enable() times out (and the CPU is unstable).
	 */
	board_pmic_init();
#endif
	cpu_frequency_set();

#ifdef CONFIG_SPL_REMOTEPROC_K3_PROC
	rproc_init();
#endif

#ifdef CONFIG_SPL_ESPI
	/* Probe eSPI device */
	ret = uclass_first_device(UCLASS_ESPI, &dev);
	if (ret) {
		pr_debug("eSPI: Init failed (ret=%d)\n", ret);
		return 0;
	}
	if (!dev) {
		pr_debug("eSPI: No device found\n");
		return 0;
	}
#endif

	return 0;
}

void spl_board_init(void)
{
	spl_load_env();
}

#if CONFIG_IS_ENABLED(FIT_IMAGE_POST_PROCESS)
extern void flush_cache(unsigned long addr, unsigned long size);

static int lzop_get_uncompressed_size(const unsigned char *src, size_t src_len,
				      size_t *out_len)
{
	const unsigned char *p = src;
	const unsigned char *end = src + src_len;
	size_t total = 0;
	u16 version;
	u32 flags;
	u32 dlen;
	u32 slen;
	u8 name_len;

	if (!src || !out_len || !lzop_is_valid_header(src))
		return -EINVAL;

	p += 9;
	if ((size_t)(end - p) < 7)
		return -EINVAL;

	version = get_unaligned_be16(p);
	p += 7;
	if (version >= 0x0940) {
		if (p >= end)
			return -EINVAL;
		p++;
	}

	if ((size_t)(end - p) < 12)
		return -EINVAL;

	flags = get_unaligned_be32(p);
	if (flags & LZOP_HEADER_HAS_FILTER) {
		if ((size_t)(end - p) < 16)
			return -EINVAL;
		p += 4;
	}

	p += 12;

	if (version >= 0x0940) {
		if ((size_t)(end - p) < 4)
			return -EINVAL;
		p += 4;
	}

	if (p >= end)
		return -EINVAL;

	name_len = *p++;
	if ((size_t)(end - p) < (size_t)name_len + 4)
		return -EINVAL;
	p += name_len + 4;

	while (p < end) {
		if ((size_t)(end - p) < 4)
			return -EINVAL;
		dlen = get_unaligned_be32(p);
		p += 4;

		if (!dlen) {
			*out_len = total;
			return 0;
		}

		if ((size_t)(end - p) < 8)
			return -EINVAL;
		slen = get_unaligned_be32(p);
		p += 8;

		if (!slen || slen > dlen || (size_t)(end - p) < slen)
			return -EINVAL;

		total += dlen;
		p += slen;
	}

	return -EINVAL;
}

static int decompress_lzo_fit_image_to_malloc(const void *fit, int node,
					      void **p_image, size_t *p_size)
{
	uint8_t comp = IH_COMP_NONE;
	size_t out_len;
	size_t expected_len;
	void *dst;
	int ret;

	if (fit_image_get_comp(fit, node, &comp) || comp != IH_COMP_LZO)
		return 0;

	ret = lzop_get_uncompressed_size(*p_image, *p_size, &expected_len);
	if (ret || !expected_len)
		return -EINVAL;

	dst = malloc(expected_len);
	if (!dst)
		return -ENOMEM;

	out_len = expected_len;
	ret = lzop_decompress(*p_image, *p_size, dst, &out_len);
	if (ret) {
		free(dst);
		return ret;
	}

	*p_image = dst;
	*p_size = out_len;

	return 1;
}

void board_fit_image_post_process(const void *fit, int node, void **p_image,
				  size_t *p_size)
{
	const char *name = fit_get_name(fit, node, NULL);

	/*
	 * Keep the standard pre-load FIT post-process hook intact for
	 * compatibility. Spacemit still needs this early hook for rcpu firmware
	 * images, since the generic SPL load path would otherwise copy or
	 * decompress the ELF payload into the remote processor reserved memory
	 * before rproc_load() parses it.
	 */
#ifdef CONFIG_SPL_REMOTEPROC_K3_PROC
	bool fw_image = name && (!strncmp(name, "rcpu0-fw", 8) ||
				 !strncmp(name, "rcpu1-fw", 8));
	bool free_image = false;
	int ret;

	if (!fw_image)
		return;

	ret = decompress_lzo_fit_image_to_malloc(fit, node, p_image, p_size);
	if (ret < 0) {
		*p_size = 0;
		return;
	}
	free_image = ret > 0;

	if (!strncmp(name, "rcpu0-fw", 8)) {
		ret = rproc_load(0, (ulong)*p_image, *p_size);
		if (!ret)
			ret = rproc_start(0);
		if (ret)
			pr_err("failed to start rcpu0 firmware: %d\n", ret);
	} else {
		ret = rproc_load(1, (ulong)*p_image, *p_size);
		if (!ret)
			ret = rproc_start(1);
		if (ret)
			pr_err("failed to start rcpu1 firmware: %d\n", ret);
	}

	if (free_image)
		free(*p_image);

	*p_size = 0;
#endif
}

int board_spl_fit_image_post_load(const void *fit, int node,
				  struct spl_image_info *image_info)
{
#ifdef CONFIG_SPL_REMOTEPROC_K3_PROC
	char product_name[64] = { 0 };
	const char *name = fit_get_name(fit, node, NULL);
	bool data_null_image = name && !strcmp(name, "rcpu-data-null");
	void *image;

	if (!image_info || !image_info->size)
		return 0;

	if (data_null_image) {
		image = map_sysmem(image_info->load_addr, image_info->size);
		/* copy the product name to this space */
		get_product_name(product_name, 64);
		strcpy(image, product_name);
		flush_cache(image_info->load_addr, 64);
		return 0;
	}
#endif

	return 0;
}
#endif

#ifdef CONFIG_SPL_LOAD_FIT
/*
 * Override default FIT buffer address to avoid conflict with uboot load address.
 * Use CONFIG_SPL_LOAD_FIT_ADDRESS (0x110000000) instead of CONFIG_SYS_TEXT_BASE
 * (0x102000000) to prevent FIT data from being overwritten when uboot is loaded.
 */
void *board_spl_fit_buffer_addr(ulong fit_size, int sectors, int bl_len)
{
	return (void *)CONFIG_SPL_LOAD_FIT_ADDRESS;
}

int board_fit_config_name_match(const char* name)
{
	char product_name[64];

	if ((0 == get_product_name(product_name, sizeof(product_name))) &&
		(0 == strcmp(product_name, name))) {
		pr_info("Boot from fit configuration %s\n", name);
		return 0;
	}
	else {
		/* boot using default FIT config */
		return -1;
	}
}
#endif

/**********************************************************
 * load env from storage
 *********************************************************/
static struct env_driver *_spl_env_driver_lookup(enum env_location loc)
{
	struct env_driver *drv;
	const int n_ents = ll_entry_count(struct env_driver, env_driver);
	struct env_driver *entry;

	drv = ll_entry_start(struct env_driver, env_driver);
	for (entry = drv; entry != drv + n_ents; entry++) {
		if (loc == entry->location)
			return entry;
	}

	/* Not found */
	return NULL;
}

static struct env_driver *spl_env_driver_lookup(enum env_operation op, enum env_location loc)
{
	struct env_driver *drv;

	if (loc == ENVL_UNKNOWN)
		return NULL;

	drv = _spl_env_driver_lookup(loc);
	if (!drv) {
		pr_debug("%s: No environment driver for location %d\n", __func__, loc);
		return NULL;
	}

	return drv;
}

static void spl_load_env(void)
{
	struct env_driver *drv;
	int ret = -1;
	u32 boot_mode = get_boot_mode();

#ifdef CONFIG_SPL_RSA_VERIFY
	/*
	 * In secure boot mode, do not load environment from external flash
	 * to prevent unauthorized environment modification
	 */
	pr_info("Secure boot enabled, skip loading environment from flash\n");
	return;
#endif

	/*if boot from usb, spl should not find env*/
	if (boot_mode == BOOT_MODE_USB){
		return;
	}

	/*
	only load env from mtd dev, because only mtd dev need
	env mtdparts info to load image.
	*/
	enum env_location loc = ENVL_UNKNOWN;
	switch (boot_mode) {
#ifdef CONFIG_ENV_IS_IN_NAND
	case BOOT_MODE_NAND:
		loc = ENVL_NAND;
		break;
#endif
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
	case BOOT_MODE_NOR:
		loc = ENVL_SPI_FLASH;
		break;
#endif
#ifdef CONFIG_ENV_IS_IN_MTD
	case BOOT_MODE_NAND:
	case BOOT_MODE_NOR:
		loc = ENVL_MTD;
		break;
#endif
	default:
		return;
	}

	drv = spl_env_driver_lookup(ENVOP_INIT, loc);
	if (!drv){
		pr_err("%s, can not load env from storage\n", __func__);
		return;
	}

	ret = drv->load();
	if (!ret){
		pr_info("has init env successful\n");
	}else{
		pr_err("load env from storage fail, would use default env\n");
		/*if load env from storage fail, it should not write bootmode to reg*/
		boot_mode = BOOT_MODE_NONE;
	}
}

void board_boot_order(u32* spl_boot_list)
{
#ifdef GDB_DOWNLOAD_DEBUG
	spl_boot_list[0] = BOOT_DEVICE_RAM;
#else
	u32 boot_mode = get_boot_mode();
	pr_debug("boot_mode:0x%x\n", boot_mode);
	if (boot_mode == BOOT_MODE_USB) {
		update_usb_serial_number();
		spl_boot_list[0] = BOOT_DEVICE_BOARD;
	} else {
		switch (boot_mode) {
		case BOOT_MODE_SD:
			spl_boot_list[0] = BOOT_DEVICE_MMC1;
			break;
		case BOOT_MODE_EMMC:
			spl_boot_list[0] = BOOT_DEVICE_MMC2;
			break;
		case BOOT_MODE_NAND:
			spl_boot_list[0] = BOOT_DEVICE_NAND;
			break;
		case BOOT_MODE_NOR:
			spl_boot_list[0] = BOOT_DEVICE_SPI;
			break;
#ifdef CONFIG_SPL_UFS_SUPPORT
		case BOOT_MODE_UFS:
			spl_boot_list[0] = BOOT_DEVICE_UFS;
			break;
#endif
		default:
			spl_boot_list[0] = BOOT_DEVICE_RAM;
			break;
		}

		// reserve for debug/test to load/run uboot from ram.
		spl_boot_list[1] = BOOT_DEVICE_RAM;
	}
#endif
}

static void spl_fixup_model(void *fdt)
{
	char product_name[64];
	char model[80];
	int root;

	root = fdt_path_offset(fdt, "/");
	if (root < 0)
		return;

	if (fdt_getprop(fdt, root, "model", NULL))
		return;

	get_product_name(product_name, sizeof(product_name));
	snprintf(model, sizeof(model), "spacemit %s board", product_name);
	fdt_setprop_string(fdt, root, "model", model);
}

void spl_perform_board_fixups(struct spl_image_info *spl_image)
{
	void *fdt_addr = spl_image ? spl_image_fdt_addr(spl_image) : NULL;

	if (fdt_addr) {
		dram_init_banksize();
		spl_fixup_model(fdt_addr);
		spl_fixup_fdt(fdt_addr);
	}
}
