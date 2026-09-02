// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#include <asm/gpio.h>
#include <asm/io.h>
#include <clk.h>
#include <display.h>
#include <dm.h>
#include <edid.h>
#include <log.h>
#include <regmap.h>
#include <syscon.h>

#include <power-domain-uclass.h>
#include <power-domain.h>
#include <power/regulator.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <backlight.h>

#include "spacemit_inno_dp.h"

#define DP_CEA_1080P60_VIC	16

static void dp_set_entry(struct timing_entry *entry, u32 value)
{
	entry->min = value;
	entry->typ = value;
	entry->max = value;
}

static void dp_set_1080p60_timing(struct display_timing *timing)
{
	memset(timing, 0, sizeof(*timing));

	dp_set_entry(&timing->pixelclock, 148500000);
	dp_set_entry(&timing->hactive, 1920);
	dp_set_entry(&timing->hfront_porch, 88);
	dp_set_entry(&timing->hback_porch, 148);
	dp_set_entry(&timing->hsync_len, 44);
	dp_set_entry(&timing->vactive, 1080);
	dp_set_entry(&timing->vfront_porch, 4);
	dp_set_entry(&timing->vback_porch, 36);
	dp_set_entry(&timing->vsync_len, 5);
	timing->flags = DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH;
}

static void dp_decode_detailed_timing(const struct edid_detailed_timing *t,
				      struct display_timing *timing)
{
	u32 hactive, hblank, hsync_offset, hsync_width;
	u32 vactive, vblank, vsync_offset, vsync_width;

	memset(timing, 0, sizeof(*timing));

	hactive = EDID_DETAILED_TIMING_HORIZONTAL_ACTIVE(*t);
	hblank = EDID_DETAILED_TIMING_HORIZONTAL_BLANKING(*t);
	hsync_offset = EDID_DETAILED_TIMING_HSYNC_OFFSET(*t);
	hsync_width = EDID_DETAILED_TIMING_HSYNC_PULSE_WIDTH(*t);
	vactive = EDID_DETAILED_TIMING_VERTICAL_ACTIVE(*t);
	vblank = EDID_DETAILED_TIMING_VERTICAL_BLANKING(*t);
	vsync_offset = EDID_DETAILED_TIMING_VSYNC_OFFSET(*t);
	vsync_width = EDID_DETAILED_TIMING_VSYNC_PULSE_WIDTH(*t);

	dp_set_entry(&timing->pixelclock,
		     EDID_DETAILED_TIMING_PIXEL_CLOCK(*t));
	dp_set_entry(&timing->hactive, hactive);
	dp_set_entry(&timing->hfront_porch, hsync_offset);
	dp_set_entry(&timing->hback_porch, hblank - hsync_offset - hsync_width);
	dp_set_entry(&timing->hsync_len, hsync_width);
	dp_set_entry(&timing->vactive, vactive);
	dp_set_entry(&timing->vfront_porch, vsync_offset);
	dp_set_entry(&timing->vback_porch, vblank - vsync_offset - vsync_width);
	dp_set_entry(&timing->vsync_len, vsync_width);

	timing->flags = 0;
	if (EDID_DETAILED_TIMING_FLAG_HSYNC_POLARITY(*t))
		timing->flags |= DISPLAY_FLAGS_HSYNC_HIGH;
	else
		timing->flags |= DISPLAY_FLAGS_HSYNC_LOW;

	if (EDID_DETAILED_TIMING_FLAG_VSYNC_POLARITY(*t))
		timing->flags |= DISPLAY_FLAGS_VSYNC_HIGH;
	else
		timing->flags |= DISPLAY_FLAGS_VSYNC_LOW;

	if (EDID_DETAILED_TIMING_FLAG_INTERLACED(*t))
		timing->flags |= DISPLAY_FLAGS_INTERLACED;
}

static u32 dp_calc_vrefresh(const struct display_timing *timing)
{
	u32 htotal, vtotal;
	u64 refresh;

	htotal = timing->hactive.typ + timing->hfront_porch.typ +
		 timing->hback_porch.typ + timing->hsync_len.typ;
	vtotal = timing->vactive.typ + timing->vfront_porch.typ +
		 timing->vback_porch.typ + timing->vsync_len.typ;
	if (!htotal || !vtotal)
		return 0;

	refresh = (u64)timing->pixelclock.typ;
	refresh += (u64)htotal * vtotal / 2;
	do_div(refresh, (u64)htotal * vtotal);

	return refresh;
}

static bool dp_is_1080p60_timing(const struct display_timing *timing)
{
	u32 refresh = dp_calc_vrefresh(timing);

	return timing->hactive.typ == 1920 &&
	       timing->vactive.typ == 1080 &&
	       !(timing->flags & DISPLAY_FLAGS_INTERLACED) &&
	       refresh >= 59 && refresh <= 61;
}

static bool dp_find_1080p60_dtd(const struct edid_detailed_timing *dtd,
				int count, struct display_timing *timing)
{
	int i;
	struct display_timing tmp;

	for (i = 0; i < count; i++) {
		if (EDID_DETAILED_TIMING_PIXEL_CLOCK(dtd[i]) == 0)
			continue;

		dp_decode_detailed_timing(&dtd[i], &tmp);
		if (!dp_is_1080p60_timing(&tmp))
			continue;

		*timing = tmp;
		return true;
	}

	return false;
}

static bool dp_find_1080p60_cea_vdb(const struct edid_cea861_info *cea,
				    struct display_timing *timing)
{
	int offset = cea->dtd_offset;
	int data_len;
	int i;

	if (offset < 4 || offset > EDID_SIZE)
		return false;

	data_len = offset - 4;
	for (i = 0; i < data_len; ) {
		int len = EDID_CEA861_DB_LEN(*cea, i);
		int type = EDID_CEA861_DB_TYPE(*cea, i);
		int j;

		if (i + len >= data_len)
			break;

		if (type == EDID_CEA861_DB_VIDEO) {
			for (j = 0; j < len; j++) {
				u8 svd = cea->data[i + 1 + j];

				if ((svd & 0x7f) != DP_CEA_1080P60_VIC)
					continue;

				dp_set_1080p60_timing(timing);
				return true;
			}
		}

		i += len + 1;
	}

	return false;
}

static bool dp_find_1080p60_from_edid(const u8 *buf, int buf_size,
				      struct display_timing *timing)
{
	const struct edid1_info *edid = (const struct edid1_info *)buf;

	if (buf_size < sizeof(*edid) || edid_check_info((struct edid1_info *)edid))
		return false;

	if (dp_find_1080p60_dtd((const struct edid_detailed_timing *)
				edid->monitor_details.descriptor,
				ARRAY_SIZE(edid->monitor_details.descriptor),
				timing))
		return true;

	if (edid->extension_flag && buf_size >= EDID_EXT_SIZE) {
		const struct edid_cea861_info *cea =
			(const struct edid_cea861_info *)(buf + sizeof(*edid));

		if (cea->extension_tag == EDID_CEA861_EXTENSION_TAG) {
			int count = EDID_CEA861_DTD_COUNT(*cea);
			int offset = cea->dtd_offset;

			if (dp_find_1080p60_cea_vdb(cea, timing))
				return true;

			if (offset >= 4 &&
			    offset + count * sizeof(struct edid_detailed_timing) < EDID_SIZE &&
			    dp_find_1080p60_dtd((const struct edid_detailed_timing *)((const u8 *)cea + offset),
						count, timing))
				return true;
		}
	}

	return false;
}

static int dp_enable(struct udevice *dev, int panel_bpp,
		     const struct display_timing *edid)
{
	struct spacemit_inno_dp_priv *priv = dev_get_priv(dev);
	struct soc_dp_video_mode *mode = &priv->dp_dev.video_mode;
	unsigned long get_rate, set_rate;
	int ret;
	void __iomem *pmu_addr;
	u32 value;

	pr_debug("%s \n", __func__);

	if (soc_dp_hw_read_sink_caps(&priv->dp_dev)) {
		pr_info("Failed to read sink caps\n");
		priv->dp_dev.link.revision = 0x14;
		priv->dp_dev.link.max_rate = SOC_DP_LINK_RATE_5_40;
		priv->dp_dev.link.max_num_lanes = SOC_DP_LANE_2;
		priv->dp_dev.link.enhanced_framing = 1;
	} else {
		// DVI, HDMI HUB or DP++
		if (priv->dp_dev.dpcd[0x05] & 0x1)
			return -1;
	}

	set_rate = clk_round_rate(&priv->pxclk, edid->pixelclock.typ);
	ret = clk_set_rate(&priv->pxclk, set_rate);
	if (ret < 0) {
		pr_err("clk_set_rate pxclk %ld failed: %d\n", set_rate, ret);
		return ret;
	}

	get_rate = clk_get_rate(&priv->pxclk);
	pr_debug("%s pxclk = %ld\n", __func__, get_rate);

	set_rate = clk_round_rate(&priv->dppxclk, edid->pixelclock.typ);
	ret = clk_set_rate(&priv->dppxclk, set_rate);
	if (ret < 0) {
		pr_err("clk_set_rate dppxclk %ld failed: %d\n", set_rate, ret);
		return ret;
	}

	get_rate = clk_get_rate(&priv->dppxclk);
	pr_debug("%s dppxclk rate = %ld\n", __func__, get_rate);

	/* use DP pixel clock */
	pmu_addr = (void __iomem *)0xd4282800;
	if (priv->dp_id == 0 || priv->edp_id == 0) {
		value = readl(pmu_addr + 0x23c);
		value |= BIT(2);
		writel(value, (pmu_addr + 0x23c));
	} else if (priv->dp_id == 1 || priv->edp_id == 1) {
		value = readl(pmu_addr + 0x23c);
		value |= BIT(18);
		writel(value, (pmu_addr + 0x23c));
	}

	mode->clock = edid->pixelclock.typ / 1000;

	mode->hdisplay = (uint16_t)edid->hactive.typ;
	mode->hsync_start = (uint16_t)(edid->hactive.typ + edid->hfront_porch.typ);
	mode->hsync_end = (uint16_t)(mode->hsync_start + edid->hsync_len.typ);
	mode->htotal = (uint16_t)(mode->hsync_end + edid->hback_porch.typ);

	mode->vdisplay = (uint16_t)edid->vactive.typ;
	mode->vsync_start = (uint16_t)(edid->vactive.typ + edid->vfront_porch.typ);
	mode->vsync_end = (uint16_t)(mode->vsync_start + edid->vsync_len.typ);
	mode->vtotal = (uint16_t)(mode->vsync_end + edid->vback_porch.typ);

	mode->flags = 0;
	if (edid->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		mode->flags |= SOC_DP_MODE_FLAG_PHSYNC;
	if (edid->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		mode->flags |= SOC_DP_MODE_FLAG_PVSYNC;

	mode->flags |= SOC_DP_MODE_FLAG_PHSYNC;
	mode->flags |= SOC_DP_MODE_FLAG_PVSYNC;

	if (soc_dp_mode_set(&priv->dp_dev, mode) == 0)
		soc_dp_hw_enable(&priv->dp_dev);

	return 0;
}

static int dp_read_edid(struct udevice *dev, uint8_t *buf, int buf_size)
{
	struct spacemit_inno_dp_priv *priv = dev_get_priv(dev);
	u32 edid_size = EDID_LENGTH;
	int ret;
	int i;

	pr_debug("%s \n", __func__);

	for (i = 0; i < 3; i++) {
		ret = soc_dp_conn_get_edid_block(&priv->dp_dev, buf, 0, EDID_LENGTH);
		if (ret) {
			pr_info("EDID read failed\n");
			continue;
		}

		/*
		 * check if the EDID has an extension flag, and read additional
		 * EDID data if needed
		 */
		if (buf[EDID_EXTENSION_FLAG]) {
			edid_size += EDID_LENGTH;
			ret = soc_dp_conn_get_edid_block(&priv->dp_dev, buf + EDID_LENGTH, 1, EDID_LENGTH);
			if (ret) {
				pr_info("additional EDID Read failed!\n");
				continue;
			}
		}

		return edid_size;
	}

	return ret;
}

static int dp_read_timing(struct udevice *dev, struct display_timing *timing)
{
	struct spacemit_inno_dp_priv *priv = dev_get_priv(dev);
	u8 edid[EDID_EXT_LENGTH];
	struct display_timing fallback_timing;
	int edid_len;
	int panel_bpp;
	int ret;

	pr_debug("%s \n", __func__);

	edid_len = dp_read_edid(dev, edid, sizeof(edid));
	if (edid_len < 0)
		return edid_len;

	ret = edid_get_timing(edid, edid_len, timing, &panel_bpp);
	if (ret)
		return ret;

	if (!priv->dp_dev.edp_mode && timing->hactive.typ > 1920) {
		if (dp_find_1080p60_from_edid(edid, edid_len, &fallback_timing))
			*timing = fallback_timing;
	}

	return 0;
}

static int spacemit_dp_of_to_plat(struct udevice *dev)
{
	return 0;
}

static int spacemit_dp_probe(struct udevice *dev)
{
	struct spacemit_inno_dp_priv *priv = dev_get_priv(dev);
	struct power_domain pm_domain;
	void __iomem *ciu_addr, *pmu_addr;
	u32 value;
	unsigned long rate;
	unsigned long base;
	u32 id;
	u32 pix_clk;
	int ret;

	memset(priv, 0, sizeof(*priv));
	priv->base = dev_remap_addr_name(dev, "base");
	if (!priv->base)
		return -EINVAL;

	ret = dev_read_u32(dev, "dp-id", &id);
	if (ret) {
		priv->dp_id = -1;
	} else
		priv->dp_id = id;

	ret = dev_read_u32(dev, "edp-id", &id);
	if (ret) {
		priv->edp_id = -1;
	} else
		priv->edp_id = id;

	if (priv->dp_id == -1 && priv->edp_id == -1) {
		pr_err("dp-id and edp-id are not found\n");
		return -EINVAL;
	}

	ret = power_domain_get(dev, &pm_domain);
	if (ret) {
		pr_err("power_domain_get dp failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "pxclk", &priv->pxclk);
	if (ret) {
		pr_err("clk_get_by_name pxclk failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "mclk", &priv->mclk);
	if (ret) {
		pr_err("clk_get_by_name mclk failed: %d", ret);
		return ret;
	}

	if ((priv->dp_id == 0) || (priv->edp_id == 0)) {
		ret = clk_get_by_name(dev, "hclk", &priv->hclk);
		if (ret) {
			pr_err("clk_get_by_name hclk failed: %d", ret);
			return ret;
		}
	}

	ret = clk_get_by_name(dev, "escclk", &priv->escclk);
	if (ret) {
		pr_err("clk_get_by_name escclk failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "dscclk", &priv->dscclk);
	if (ret) {
		pr_err("clk_get_by_name dscclk failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "aclk", &priv->aclk);
	if (ret) {
		pr_err("clk_get_by_name aclk failed: %d", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "dppxclk", &priv->dppxclk);
	if (ret) {
		pr_err("clk_get_by_name dppxclk failed: %d", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "aclk_reset", &priv->aclk_reset);
	if (ret) {
		pr_err("reset_get_by_name aclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "mclk_reset", &priv->mclk_reset);
	if (ret) {
		pr_err("reset_get_by_name mclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "esc_reset", &priv->esc_reset);
	if (ret) {
		pr_err("reset_get_by_name esc_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "dscclk_reset", &priv->dscclk_reset);
	if (ret) {
		pr_err("reset_get_by_name dscclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "lcd_reset", &priv->lcd_reset);
	if (ret) {
		pr_err("reset_get_by_name lcd_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "dp_reset", &priv->dp_reset);
	if (ret) {
		pr_err("reset_get_by_name dp_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->mclk_reset);
	if (ret) {
		pr_err("reset_assert mclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->aclk_reset);
	if (ret) {
		pr_err("reset_assert aclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->esc_reset);
	if (ret) {
		pr_err("reset_assert esc_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->dscclk_reset);
	if (ret) {
		pr_err("reset_assert dscclk_reset failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->lcd_reset);
	if (ret) {
		pr_err("reset_assert lcd_reset failed: %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->mclk);
	if (ret < 0) {
		pr_err("clk_enable mclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->escclk);
	if (ret < 0) {
		pr_err("clk_enable escclk failed: %d\n", ret);
		return ret;
	}

	if ((priv->dp_id == 0) || (priv->edp_id == 0)) {
		ret = clk_enable(&priv->hclk);
		if (ret < 0) {
			pr_err("clk_enable hclk failed: %d\n", ret);
			return ret;
		}
	}

	ret = clk_enable(&priv->dscclk);
	if (ret < 0) {
		pr_err("clk_enable dscclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->aclk);
	if (ret < 0) {
		pr_err("clk_enable aclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->pxclk);
	if (ret < 0) {
		pr_err("clk_enable pxclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->mclk, 307200000);
	if (ret < 0) {
		pr_err("clk_set_rate mclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->aclk, 409600000);
	if (ret < 0) {
		pr_err("clk_set_rate aclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->escclk, 51200000);
	if (ret < 0) {
		pr_err("clk_set_rate escclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->dscclk, 614400000);
	if (ret < 0) {
		pr_err("clk_set_rate dscclk failed: %d\n", ret);
		return ret;
	}

	pix_clk = dev_read_u32_default(dev, "pix-clk", 150000000);
	pr_debug("%s() set pixel clock %d \n", __func__, pix_clk);

	ret = clk_set_rate(&priv->pxclk, pix_clk);
	if (ret < 0) {
		pr_err("clk_set_rate pxclk failed: %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&priv->dp_reset);
	if (ret) {
		pr_err("reset_assert dp_reset failed: %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->dppxclk);
	if (ret < 0) {
		pr_err("clk_enable dppxclk failed: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&priv->dppxclk, pix_clk);
	if (ret < 0) {
		pr_err("clk_set_rate dppxclk failed: %d\n", ret);
		return ret;
	}

	rate = clk_get_rate(&priv->mclk);
	pr_debug("%s clk_get_rate mclk rate = %ld\n", __func__, rate);

	rate = clk_get_rate(&priv->aclk);
	pr_debug("%s clk_get_rate aclk rate = %ld\n", __func__, rate);

	if ((priv->dp_id == 0) || (priv->edp_id == 0)) {
		rate = clk_get_rate(&priv->hclk);
		pr_debug("%s clk_get_rate hclk rate = %ld\n", __func__, rate);
	}

	rate = clk_get_rate(&priv->escclk);
	pr_debug("%s clk_get_rate escclk rate = %ld\n", __func__, rate);

	rate = clk_get_rate(&priv->dscclk);
	pr_debug("%s clk_get_rate dscclk rate = %ld\n", __func__, rate);

	rate = clk_get_rate(&priv->pxclk);
	pr_debug("%s clk_get_rate pxclk rate = %ld\n", __func__, rate);

	rate = clk_get_rate(&priv->dppxclk);
	pr_debug("%s clk_get_rate dppxclk rate = %ld\n", __func__, rate);

	ret = gpio_request_by_name(dev, "power-gpios", 0, &priv->power,
				   GPIOD_IS_OUT);
	if (ret) {
		pr_debug("%s: Warning: cannot get power GPIO: ret=%d\n",
		      __func__, ret);
		priv->power_valid = false;
	} else {
		priv->power_valid = true;
	}

	ret = gpio_request_by_name(dev, "enable-gpios", 0, &priv->enable,
				   GPIOD_IS_OUT);
	if (ret) {
		pr_debug("%s: Warning: cannot get enable GPIO: ret=%d\n",
		      __func__, ret);
		priv->enable_valid = false;
	} else {
		priv->enable_valid = true;
	}


	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &priv->backlight);
	if (ret) {
		pr_debug("%s: Warning: cannot get backlight pwm: ret = %d\n",
			__func__, ret);
		priv->bl_valid = false;
	} else {
		priv->bl_valid = true;
	}

	if (priv->power_valid) {
		dm_gpio_set_value(&priv->power, 1);
		mdelay(2);
	}

	if (priv->enable_valid) {
		dm_gpio_set_value(&priv->enable, 1);
		mdelay(2);
	}

	if (priv->bl_valid) {
			ret = backlight_set_brightness(priv->backlight, BACKLIGHT_DEFAULT);
			pr_debug("%s: set brightness, ret = %d\n", __func__, ret);
			if (ret)
				return ret;
			ret = backlight_enable(priv->backlight);
			pr_debug("%s: enable backlight, ret = %d\n", __func__, ret);
			if (ret)
				return ret;
	}

	if ((priv->edp_id == 0) || (priv->edp_id == 1)) {
		priv->dp_type = INNO_EDP;
		priv->dp_dev.edp_mode = true;
	} else {
		priv->dp_type = INNO_DP;
		priv->dp_dev.edp_mode = false;
	}

	/* mux dp0 */
	ciu_addr = (void __iomem *)0xd4282c00;
	if ((priv->dp_id == 0) || (priv->edp_id == 0)) {
		value = readl(ciu_addr + 0x12c);
		value |= BIT(8);
		writel(value, (ciu_addr + 0x12c));
	}

	/* use DP pixel clock */
	pmu_addr = (void __iomem *)0xd4282800;
	if (priv->dp_id == 0 || priv->edp_id == 0) {
		value = readl(pmu_addr + 0x23c);
		value |= BIT(2);
		writel(value, (pmu_addr + 0x23c));
		base = DP0_REGISTER_BASE_ADDRESS;
	} else if (priv->dp_id == 1 || priv->edp_id == 1) {
		value = readl(pmu_addr + 0x23c);
		value |= BIT(18);
		writel(value, (pmu_addr + 0x23c));
		base = DP1_REGISTER_BASE_ADDRESS;
	}

	soc_dp_init(&priv->dp_dev, base, SOC_DP_REF_CLK_24M, SOC_VIDEO_RGB_8BIT);
	mdelay(5);

	if (soc_dp_hw_detect_hpd(&priv->dp_dev) != connector_status_connected) {
		pr_info("dp cannot get HPD signal\n");
		return -1;
	}

	return 0;
}

static const struct dm_display_ops spacemit_dp_ops = {
	.read_timing = dp_read_timing,
	.read_edid = dp_read_edid,
	.enable = dp_enable,
};

static const struct udevice_id spacemit_dp_ids[] = {
	{ .compatible = "spacemit,inno-dp" },
	{ .compatible = "spacemit,inno-edp" },
	{ }
};

U_BOOT_DRIVER(spacemit_dp) = {
	.name = "spacemit_dp",
	.id = UCLASS_DISPLAY,
	.of_match = spacemit_dp_ids,
	.ops = &spacemit_dp_ops,
	.of_to_plat = spacemit_dp_of_to_plat,
	.probe = spacemit_dp_probe,
	.priv_auto	= sizeof(struct spacemit_inno_dp_priv),
};
