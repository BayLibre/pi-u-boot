// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 Spacemit, Inc
 */

#include <config.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <fastboot.h>
#include <fdtdec.h>
#include <malloc.h>
#include <misc.h>
#include <watchdog.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <cros_ec.h>
#include <fb_spacemit.h>
#if CONFIG_IS_ENABLED(MD5)
#include <u-boot/md5.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#if CONFIG_IS_ENABLED(CROS_EC)
static const char *const ec_update_board_models[] = {
	"SpacemiT K3 pico-itx board",
	"spacemit k3 pico itx board",
	"spacemit k3 deb1 board",
};

#if CONFIG_IS_ENABLED(MD5)
static void fastboot_ec_print_md5(const char *label,
				  const unsigned char digest[MD5_SUM_LEN])
{
	char md5_str[MD5_SUM_LEN * 2 + 1];
	int i;
	int pos = 0;

	for (i = 0; i < MD5_SUM_LEN; i++)
		pos += snprintf(md5_str + pos, sizeof(md5_str) - pos, "%02x",
				digest[i]);

	pr_info("%s%s\n", label, md5_str);
}

static int fastboot_cros_ec_read_rw_md5(struct udevice *dev,
					u32 rw_offset, u32 image_size,
					u32 chunk, uint8_t *read_buf,
					unsigned char digest[MD5_SUM_LEN])
{
	struct MD5Context md5_ctx;
	u32 off, todo;
	int ret;

	MD5Init(&md5_ctx);
	for (off = 0; off < image_size; off += todo) {
		todo = min(image_size - off, chunk);
		ret = cros_ec_flash_read(dev, read_buf, rw_offset + off, todo);
		if (ret) {
			pr_err("EC RW readback failed (ret=%d)\n", ret);
			return ret;
		}
		MD5Update(&md5_ctx, read_buf, todo);
		schedule();
	}
	MD5Final(digest, &md5_ctx);

	return 0;
}
#endif

static void fastboot_cros_ec_erase_after_verify_failure(struct udevice *dev,
							u32 rw_offset,
							u32 rw_size)
{
	int ret;

	pr_err("Erasing RW region due to verify failure...\n");
	schedule();
	ret = cros_ec_flash_erase(dev, rw_offset, rw_size);
	if (ret)
		pr_err("EC RW erase after verify failure failed (ret=%d)\n",
		       ret);
}

static bool fastboot_should_update_ec(void)
{
	struct fdt_header *working_fdt = (struct fdt_header *)gd->fdt_blob;
	int len;
	int nodeoffset;
	const char *model;
	int i;

	if (!working_fdt || fdt_check_header(working_fdt))
		return false;

	nodeoffset = fdt_path_offset(working_fdt, "/");
	if (nodeoffset < 0)
		return false;

	model = fdt_getprop(working_fdt, nodeoffset, "model", &len);
	if (!model || len <= 0)
		return false;

	for (i = 0; i < ARRAY_SIZE(ec_update_board_models); i++) {
		if (!strcmp(model, ec_update_board_models[i]))
			return true;
	}

	return false;
}

static int fastboot_cros_ec_update_rw(struct udevice *dev,
				      const uint8_t *image, u32 image_size,
				      bool verify)
{
	u32 rw_offset, rw_size;
	u32 off, todo;
	u32 chunk = 4096;
	uint8_t *read_buf = NULL;
	int ret;

#if CONFIG_IS_ENABLED(MD5)
	unsigned char image_md5[MD5_SUM_LEN];
	unsigned char current_md5[MD5_SUM_LEN];
	unsigned char readback_md5[MD5_SUM_LEN];
#endif

	if (!image_size)
		return -EINVAL;

	ret = cros_ec_flash_offset(dev, EC_FLASH_REGION_ACTIVE, &rw_offset,
				   &rw_size);
	if (ret) {
		pr_err("Could not read RW region info (ret=%d)\n", ret);
		return ret;
	}
	if (image_size > rw_size) {
		pr_err("Image too large (0x%x > 0x%x)\n", image_size, rw_size);
		return -EFBIG;
	}

	if (verify || CONFIG_IS_ENABLED(MD5)) {
		read_buf = malloc(min(image_size, chunk));
		if (!read_buf) {
			pr_err("EC RW verify buffer alloc failed\n");
			return -ENOMEM;
		}
	}

#if CONFIG_IS_ENABLED(MD5)
	pr_info("Calculating incoming EC RW firmware MD5...\n");
	md5_wd(image, image_size, image_md5, 0x10000);

	pr_info("Calculating current EC RW firmware MD5...\n");
	ret = fastboot_cros_ec_read_rw_md5(dev, rw_offset, image_size, chunk,
					   read_buf, current_md5);
	if (ret)
		goto out;

	if (!memcmp(current_md5, image_md5, MD5_SUM_LEN)) {
		fastboot_ec_print_md5("Current EC RW MD5:  ", current_md5);
		pr_info("EC RW firmware is unchanged, skip update\n");
		ret = 0;
		goto out;
	}

	fastboot_ec_print_md5("Current EC RW MD5:  ", current_md5);
	fastboot_ec_print_md5("Incoming EC RW MD5: ", image_md5);
#endif

	ret = cros_ec_invalidate_hash(dev);
	if (ret) {
		pr_err("EC hash invalidate failed (ret=%d)\n", ret);
		goto out;
	}

	pr_info("Erasing RW region...\n");
	schedule();
	ret = cros_ec_flash_erase(dev, rw_offset, rw_size);
	if (ret) {
		pr_err("EC RW erase failed (ret=%d)\n", ret);
		goto out;
	}

	pr_info("Writing RW firmware...\n");
	for (off = 0; off < image_size; off += todo) {
		todo = min(image_size - off, chunk);
		ret = cros_ec_flash_write(dev, image + off, rw_offset + off, todo);
		if (ret) {
			pr_err("EC RW write failed (ret=%d)\n", ret);
			goto out;
		}
		schedule();
		pr_cont("\rWriting: %3u%%", (unsigned int)(((off + todo) * 100U) /
							   image_size));
	}
	pr_cont("\n");

	if (!verify)
		goto reboot_rw;

#if CONFIG_IS_ENABLED(MD5)
	pr_info("Calculating written EC RW firmware MD5...\n");
	ret = fastboot_cros_ec_read_rw_md5(dev, rw_offset, image_size, chunk,
					   read_buf, readback_md5);
	if (ret) {
		fastboot_cros_ec_erase_after_verify_failure(dev, rw_offset,
							    rw_size);
		goto out;
	}

	fastboot_ec_print_md5("Written EC RW MD5:  ", readback_md5);
	fastboot_ec_print_md5("Incoming EC RW MD5: ", image_md5);
	if (memcmp(readback_md5, image_md5, MD5_SUM_LEN)) {
		pr_err("EC RW MD5 verify mismatch\n");
		fastboot_cros_ec_erase_after_verify_failure(dev, rw_offset,
							    rw_size);
		ret = -EIO;
		goto out;
	}
#else
	pr_info("Verifying RW firmware...\n");
	for (off = 0; off < image_size; off += todo) {
		todo = min(image_size - off, chunk);
		ret = cros_ec_flash_read(dev, read_buf, rw_offset + off, todo);
		if (ret) {
			pr_err("EC RW readback failed (ret=%d)\n", ret);
			fastboot_cros_ec_erase_after_verify_failure(dev,
								    rw_offset,
								    rw_size);
			goto out;
		}
		schedule();
		if (memcmp(read_buf, image + off, todo)) {
			pr_err("EC RW verify mismatch at 0x%08x\n",
			       rw_offset + off);
			fastboot_cros_ec_erase_after_verify_failure(dev,
								    rw_offset,
								    rw_size);
			ret = -EIO;
			goto out;
		}
		pr_cont("\rVerifying: %3u%%",
			(unsigned int)(((off + todo) * 100U) / image_size));
	}
	pr_cont("\n");
#endif

reboot_rw:
	pr_info("Rebooting EC to RW...\n");
	ret = cros_ec_reboot(dev, EC_REBOOT_JUMP_RW, 0);
	if (ret)
		pr_err("EC reboot to RW failed (ret=%d)\n", ret);

out:
	free(read_buf);

	return ret;
}

void fastboot_oem_flash_ec(const char *cmd_parameter,
			   void *download_buffer, u32 download_bytes,
			   char *response)
{
	struct udevice *dev;
	int ret;

	if (!cmd_parameter || strcmp(cmd_parameter, "flash")) {
		fastboot_fail("Unsupported oem ec command", response);
		return;
	}

	pr_info("Fastboot oem ec:%s\n", cmd_parameter);
	pr_info("Fastboot EC image: addr=%p size=0x%x\n",
		download_buffer, download_bytes);

	if (!fastboot_should_update_ec()) {
		pr_info("Skip EC update on this board\n");
		fastboot_okay("EC update skipped", response);
		return;
	}

	if (!download_bytes) {
		fastboot_fail("No staged EC image", response);
		return;
	}

	dev = board_get_cros_ec_dev();
	if (!dev) {
		fastboot_fail("EC device not found", response);
		return;
	}

	ret = fastboot_cros_ec_update_rw(dev, download_buffer, download_bytes,
					 true);
	if (ret) {
		fastboot_response("FAIL", response, "EC update failed (%d)", ret);
		return;
	}

	fastboot_okay("EC update done", response);
}
#else
void fastboot_oem_flash_ec(const char *cmd_parameter,
			   void *download_buffer, u32 download_bytes,
			   char *response)
{
	(void)cmd_parameter;
	(void)download_buffer;
	(void)download_bytes;
	fastboot_fail("EC update not supported", response);
}
#endif

/*
 * K3: pass the fastboot speed selection from the SPL to U-Boot proper via a
 * CIU debug scratch register. The SPL enumerates at high-speed; U-Boot proper
 * reads this flag to pick the maximum speed for its first USB enumeration.
 */
#define SPACEMIT_FASTBOOT_SPEED_SUPER_BIT	BIT(0)

u32 spacemit_k3_fastboot_speed_flags(void)
{
	return readl((void *)BOOT_CIU_DEBUG_REG0);
}

enum usb_device_speed spacemit_k3_fastboot_requested_speed(void)
{
	return (spacemit_k3_fastboot_speed_flags() & SPACEMIT_FASTBOOT_SPEED_SUPER_BIT) ?
	       USB_SPEED_SUPER : USB_SPEED_HIGH;
}

void spacemit_k3_fastboot_set_superspeed_flag(bool enable)
{
	u32 val = readl((void *)BOOT_CIU_DEBUG_REG0);

	if (enable)
		val |= SPACEMIT_FASTBOOT_SPEED_SUPER_BIT;
	else
		val &= ~SPACEMIT_FASTBOOT_SPEED_SUPER_BIT;

	writel(val, (void *)BOOT_CIU_DEBUG_REG0);
}
