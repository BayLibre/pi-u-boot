// SPDX-License-Identifier: GPL-2.0+

#include <env.h>
#include <i2c.h>
#include <dm.h>
#include <dm/uclass.h>
#include <log.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <stdlib.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <tlv_eeprom.h>
#include <dt-bindings/pinctrl/k3-pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

#define I2C_PIN_CONFIG(x) ((x) | EDGE_NONE | PULL_UP | PAD_DS0)
#define READ_I2C_LINE_LEN (16)

#if CONFIG_IS_ENABLED(DM_I2C)
int _read_from_i2c(struct udevice *dev, u32 addr, u32 size, uchar *buf);
#else
int _read_from_i2c(int chip, u32 addr, u32 size, uchar *buf);
#endif
bool _is_valid_tlvinfo_header(struct tlvinfo_header *hdr);

struct tlv_eeprom {
	uint8_t type;
	uint8_t length;
};

struct eeprom_config {
	uint8_t bus;
	uint16_t addr;
	uint8_t pin_function;
	uint32_t scl_pin_reg;
	uint32_t sda_pin_reg;
};

const struct eeprom_config eeprom_info[] = {
	// eeprom @deb and com260: I2C2, pin group(GPIO_46, GPIO_47)
	{ 2, 0x50, MUX_MODE5, 0xd401e0b8, 0xd401e0bc },
	// eeprom @evb: I2C6, pin group(PWR_SSP_TXD, PWR_SSP_RXD)
	{ 6, 0x50, MUX_MODE2, 0xd401e264, 0xd401e268 },
};

#if CONFIG_IS_ENABLED(DM_I2C)
static void init_tlv_data(struct udevice *dev, uint8_t *buffer, uint32_t size)
{
	uint32_t offset;
	struct tlvinfo_header *hdr = (struct tlvinfo_header *) buffer;

	offset = sizeof(struct tlvinfo_header);
	_read_from_i2c(dev, 0, offset, buffer);
	if (!_is_valid_tlvinfo_header(hdr) || ((be16_to_cpu(hdr->totallen) + offset) > size)) {
		memset(buffer, 0, size);
		return;
	}

	_read_from_i2c(dev, offset, be16_to_cpu(hdr->totallen), buffer + offset);
}
#else
static void init_tlv_data(uint8_t chip, uint8_t *buffer, uint32_t size)
{
	uint32_t offset;
	struct tlvinfo_header *hdr = (struct tlvinfo_header *) buffer;

	offset = sizeof(struct tlvinfo_header);
	_read_from_i2c(chip, 0, offset, buffer);
	if (!_is_valid_tlvinfo_header(hdr) || ((be16_to_cpu(hdr->totallen) + offset) > size)) {
		memset(buffer, 0, size);
		return;
	}

	_read_from_i2c(chip, offset, be16_to_cpu(hdr->totallen), buffer + offset);
}
#endif

static void i2c_set_pinctrl(uint32_t value, uint32_t reg_addr)
{
	writel(value, (void __iomem *) (size_t) reg_addr);
}

static uint32_t i2c_get_pinctrl(uint32_t reg_addr)
{
	return readl((void __iomem *) (size_t) reg_addr);
}

int init_tlv_from_eeprom(uint8_t *tlv_data, uint32_t tlv_size)
{
	int saddr, i;
	uint8_t bus;
	uint32_t scl_pin_backup, sda_pin_backup;
#if CONFIG_IS_ENABLED(DM_I2C)
	struct udevice *i2c_bus, *i2c_dev;
	int ret;
#endif

	for (i = 0; i < ARRAY_SIZE(eeprom_info); i++) {
		bus = eeprom_info[i].bus;
		saddr = eeprom_info[i].addr;

		scl_pin_backup = i2c_get_pinctrl(eeprom_info[i].scl_pin_reg);
		sda_pin_backup = i2c_get_pinctrl(eeprom_info[i].sda_pin_reg);
		i2c_set_pinctrl(I2C_PIN_CONFIG(eeprom_info[i].pin_function),
				eeprom_info[i].scl_pin_reg);
		i2c_set_pinctrl(I2C_PIN_CONFIG(eeprom_info[i].pin_function),
				eeprom_info[i].sda_pin_reg);

#if CONFIG_IS_ENABLED(DM_I2C)
		/* Get I2C bus device by seq */
		ret = uclass_get_device_by_seq(UCLASS_I2C, bus, &i2c_bus);
		if (ret) {
			pr_err("%s: get i2c bus %d failed, ret=%d\n", __func__, bus, ret);
			i2c_set_pinctrl(scl_pin_backup, eeprom_info[i].scl_pin_reg);
			i2c_set_pinctrl(sda_pin_backup, eeprom_info[i].sda_pin_reg);
			continue;
		}

		/* Probe I2C device */
		ret = dm_i2c_probe(i2c_bus, saddr, 0, &i2c_dev);
		if (ret) {
			pr_err("%s: probe i2c(%d) @eeprom 0x%x failed, ret=%d\n", __func__, bus, saddr, ret);
			i2c_set_pinctrl(scl_pin_backup, eeprom_info[i].scl_pin_reg);
			i2c_set_pinctrl(sda_pin_backup, eeprom_info[i].sda_pin_reg);
		} else {
			pr_info("find eeprom in bus %d, address 0x%x\n", bus, saddr);
			init_tlv_data(i2c_dev, tlv_data, tlv_size);
			return 0;
		}
#else
		if ((i2c_set_bus_num(bus) < 0) || (i2c_probe(saddr) < 0)) {
			pr_err("%s: probe i2c(%d) @eeprom 0x%x failed\n", __func__, bus, saddr);
			i2c_set_pinctrl(scl_pin_backup, eeprom_info[i].scl_pin_reg);
			i2c_set_pinctrl(sda_pin_backup, eeprom_info[i].sda_pin_reg);
		} else {
			pr_info("find eeprom in bus %d, address 0x%x\n", bus, saddr);
			init_tlv_data(saddr, tlv_data, tlv_size);
			return 0;
		}
#endif
	}

	return -EINVAL;
}

#if CONFIG_IS_ENABLED(DM_I2C)
int _read_from_i2c(struct udevice *dev, u32 addr, u32 size, uchar *buf)
{
	u32 nbytes = size;
	u32 linebytes = 0;
	int ret;

	do {
		linebytes = (nbytes > READ_I2C_LINE_LEN) ? READ_I2C_LINE_LEN : nbytes;
		ret = dm_i2c_read(dev, addr, buf, linebytes);
		if (ret) {
			pr_err("read from i2c error:%d\n", ret);
			return -1;
		}

		buf += linebytes;
		nbytes -= linebytes;
		addr += linebytes;
	} while (nbytes > 0);

	return 0;
}
#else
int _read_from_i2c(int chip, u32 addr, u32 size, uchar *buf)
{
	u32 nbytes = size;
	u32 linebytes = 0;
	int ret;

	do {
		linebytes = (nbytes > READ_I2C_LINE_LEN) ? READ_I2C_LINE_LEN : nbytes;
		ret = i2c_read(chip, addr, 1, buf, linebytes);
		if (ret) {
			pr_err("read from i2c error:%d\n", ret);
			return -1;
		}

		buf += linebytes;
		nbytes -= linebytes;
		addr += linebytes;
	} while (nbytes > 0);

	return 0;
}
#endif