// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Spacemit Co., Ltd.
 *
 */

#include <cpu_func.h>
#include <display.h>
#include <dm.h>
#include <edid.h>
#include <log.h>
#include <video.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "spacemit_dpu.h"

DECLARE_GLOBAL_DATA_PTR;

static void dpu_init(struct spacemit_mode_modeinfo *spacemit_mode, ulong fbbase,
		     void __iomem *dpu_base_addr)
{
	unsigned int vsync, hsync, vbp, vfp, hbp, hfp;

	vsync = spacemit_mode->vsync_len & 0x3ff;
	hsync = spacemit_mode->hsync_len & 0x3ff;
	vbp = spacemit_mode->upper_margin & 0xfff;
	vfp = spacemit_mode->lower_margin & 0xfff;
	hbp = spacemit_mode->left_margin & 0xfff;
	hfp = spacemit_mode->right_margin & 0xfff;

	dpu_writel(dpu_base_addr, 0x30000, 0x1);

	dpu_writel(dpu_base_addr, 0x5120C, hfp << 16);
	dpu_writel(dpu_base_addr, 0x51210, (hbp << 16) | hsync);
	dpu_writel(dpu_base_addr, 0x51214, (vsync << 16) | vfp);
	dpu_writel(dpu_base_addr, 0x51218, (spacemit_mode->xres << 16) | vbp);
	dpu_writel(dpu_base_addr, 0x5121C, spacemit_mode->yres);
	dpu_writel(dpu_base_addr, 0x51200, 0x184);
	dpu_writel(dpu_base_addr, 0x51208, spacemit_mode->pix_fmt_out << 12);
	dpu_writel(dpu_base_addr, 0x5123c, 0x1);

	dpu_writel(dpu_base_addr, 0x1284, 0x840);

	dpu_writel(dpu_base_addr, 0x1000, 0xf00217c);
	dpu_writel(dpu_base_addr, 0x1024, (unsigned int)(fbbase & 0xffffffff));
	dpu_writel(dpu_base_addr, 0x1028, (unsigned int)(fbbase >> 32));
	dpu_writel(dpu_base_addr, 0x103c, spacemit_mode->xres * 4);
	dpu_writel(dpu_base_addr, 0x1040, spacemit_mode->xres | (spacemit_mode->yres << 16));
	dpu_writel(dpu_base_addr, 0x1044, 0x0);
	dpu_writel(dpu_base_addr, 0x1048, (spacemit_mode->xres - 1) | ((spacemit_mode->yres - 1) << 16));
	dpu_writel(dpu_base_addr, 0x1074, 0x8);
	/* supports up to 3840x2160 */
	dpu_writel(dpu_base_addr, 0x107c, 0x100001e0);

	dpu_writel(dpu_base_addr, 0x30004, (spacemit_mode->xres | (spacemit_mode->yres << 16)));
	dpu_writel(dpu_base_addr, 0x30020, ((spacemit_mode->xres - 1) << 16));
	dpu_writel(dpu_base_addr, 0x30024, ((spacemit_mode->yres - 1) << 16));
	dpu_writel(dpu_base_addr, 0x30038, 0xff03);
	dpu_writel(dpu_base_addr, 0x30300, 0x0);
	dpu_writel(dpu_base_addr, 0x30334, (spacemit_mode->xres | (spacemit_mode->yres << 16)));

	dpu_writel(dpu_base_addr, 0x340, 0x1040001);

	dpu_writel(dpu_base_addr, 0x34c, 0x821);

	dpu_writel(dpu_base_addr, 0x348, 0x1);
	dpu_writel(dpu_base_addr, 0x350, 0x1);
}

static int spacemit_display_init(struct udevice *dev, ulong fbbase, ofnode ep_node)
{
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct spacemit_mode_modeinfo spacemit_mode = {0};
	struct display_timing timing;
	int remote_dpu_id, dpu_id;
	struct udevice *disp = NULL;
	int ret;
	u32 remote_phandle;
	ofnode remote;
	const char *compat;
	struct display_plat *disp_uc_plat;
	void __iomem *dpu_base_addr;

	ret = ofnode_read_u32(ep_node, "remote-endpoint", &remote_phandle);
	if (ret)
		return ret;

	remote = ofnode_get_by_phandle(remote_phandle);
	if (!ofnode_valid(remote))
		return -EINVAL;
	remote_dpu_id = ofnode_read_u32_default(remote, "reg", -1);
	uc_priv->bpix = VIDEO_BPP32;

	while (ofnode_valid(remote)) {
		remote = ofnode_get_parent(remote);
		if (!ofnode_valid(remote)) {
			pr_err("%s(%s): no UCLASS_DISPLAY for remote-endpoint\n",
				 __func__, dev_read_name(dev));
			return -EINVAL;
		}

		uclass_find_device_by_ofnode(UCLASS_DISPLAY, remote, &disp);
		if (disp)
			break;
	};

	ret = ofnode_read_u32(remote, "dpu-id", &dpu_id);
	if (ret) {
		pr_warn("%s(%s): Failed to read dpu-id property, using default\n",
			__func__, dev_read_name(dev));
		dpu_id = 0;
	}

	compat = ofnode_get_property(remote, "compatible", NULL);
	if (!compat) {
		pr_err("%s(%s): Failed to find compatible property\n",
			__func__, dev_read_name(dev));
		return -EINVAL;
	}

	if (!strstr(compat, "dp") && !strstr(compat, "edp")) {
		pr_err("%s(%s): unsupported output %s (this port only drives DP)\n",
			__func__, dev_read_name(dev), compat);
		return -EINVAL;
	}

	if (dpu_id == 0) {
		dpu_base_addr = (void __iomem *)0xc0340000;
	} else if (dpu_id == 1) {
		dpu_base_addr = (void __iomem *)0xc0440000;
	} else {
		pr_err("%s(%s): Unsupported dpu_id %d\n",
		       __func__, dev_read_name(dev), dpu_id);
		return -EINVAL;
	}

	disp_uc_plat = dev_get_uclass_plat(disp);
	disp_uc_plat->source_id = remote_dpu_id;
	disp_uc_plat->src_dev = dev;

	ret = device_probe(disp);
	if (ret) {
		pr_err("%s: device '%s' display won't probe (ret=%d)\n",
			__func__, dev->name, ret);
		return ret;
	}

	ret = display_read_timing(disp, &timing);
	if (ret) {
		pr_err("%s: Failed to read timings (ret=%d)\n", __func__, ret);
		return ret;
	}

	if (timing.hactive.typ > 3840) {
		pr_err("%s: no support the resolution of %dx%d\n", __func__,
			timing.hactive.typ, timing.vactive.typ);
		return -EINVAL;
	}

	uc_priv->xsize = timing.hactive.typ;
	uc_priv->ysize = timing.vactive.typ;

	pr_info("fb=%lx, size=%dx%d\n", fbbase, uc_priv->xsize, uc_priv->ysize);

	memset((void *)fbbase, 0, uc_priv->xsize * uc_priv->ysize * VNBYTES(uc_priv->bpix));
	flush_cache(fbbase, uc_priv->xsize * uc_priv->ysize * VNBYTES(uc_priv->bpix));

	ret = display_enable(disp, 1 << VIDEO_BPP32, &timing);
	if (ret) {
		pr_err("%s: Failed to enable display\n", __func__);
		return ret;
	}

	spacemit_mode.pix_fmt_out = OUTFMT_RGB888;
	spacemit_mode.pixclock_freq = timing.pixelclock.typ;
	spacemit_mode.left_margin = timing.hback_porch.typ;
	spacemit_mode.right_margin = timing.hfront_porch.typ;
	spacemit_mode.hsync_len = timing.hsync_len.typ;
	spacemit_mode.upper_margin = timing.vback_porch.typ;
	spacemit_mode.lower_margin = timing.vfront_porch.typ;
	spacemit_mode.vsync_len = timing.vsync_len.typ;
	spacemit_mode.xres = timing.hactive.typ;
	spacemit_mode.yres = timing.vactive.typ;
	spacemit_mode.hsync_invert = 1;
	spacemit_mode.vsync_invert = 1;

	dpu_init(&spacemit_mode, fbbase, dpu_base_addr);

	return 0;
}

static int spacemit_dpu_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct spacemit_dpu_priv *priv = dev_get_priv(dev);
	ofnode port, node;
	int ret;

	priv->regs_crtc0 = dev_remap_addr_name(dev, "crtc0");
	if (!priv->regs_crtc0)
		return -EINVAL;

	priv->regs_crtc1 = dev_remap_addr_name(dev, "crtc1");
	if (!priv->regs_crtc1)
		return -EINVAL;

	port = dev_read_subnode(dev, "port");
	if (!ofnode_valid(port)) {
		pr_err("%s(%s): 'port' subnode not found\n",
			__func__, dev_read_name(dev));
		return -EINVAL;
	}

	for (node = ofnode_first_subnode(port);
	     ofnode_valid(node);
	     node = ofnode_next_subnode(node)) {
		ret = spacemit_display_init(dev, plat->base, node);
		if (ret)
			pr_err("Device failed: ret=%d\n", ret);
		if (!ret)
			break;
	}

	video_set_flush_dcache(dev, 1);

	return 0;
}

struct spacemit_dpu_driverdata dpu_driverdata = {
	.features = DPU_FEATURE_OUTPUT_10BIT,
};

static const struct udevice_id spacemit_dc_ids[] = {
	{ .compatible = "spacemit,dpu",
	  .data = (ulong)&dpu_driverdata },
	{ }
};

static const struct video_ops spacemit_dpu_ops = {
};

static int spacemit_dpu_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->size = 4 * (CONFIG_VIDEO_SPACEMIT_MAX_XRES *
			  CONFIG_VIDEO_SPACEMIT_MAX_YRES);

	return 0;
}

U_BOOT_DRIVER(spacemit_dpu) = {
	.name	= "spacemit_dpu",
	.id	= UCLASS_VIDEO,
	.of_match = spacemit_dc_ids,
	.ops	= &spacemit_dpu_ops,
	.bind	= spacemit_dpu_bind,
	.probe	= spacemit_dpu_probe,
	.priv_auto	= sizeof(struct spacemit_dpu_priv),
};
