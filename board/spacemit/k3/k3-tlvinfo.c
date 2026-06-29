// SPDX-License-Identifier: GPL-2.0+

#include <log.h>
#include <asm/io.h>
#include <stdlib.h>
#include <tlv_eeprom.h>
#include <u-boot/crc.h>
#include <net.h>
#include <linux/kernel.h>
#include <linux/string.h>

#define TLVINFO_MAX_SIZE (256)
#define TLVINFO_SIZE_MAX_TLV_LEN (TLVINFO_MAX_SIZE - sizeof(struct tlvinfo_header))

extern int tlv_device_init(void);
extern int init_tlv_from_eeprom(uint8_t *tlv_data, uint32_t tlv_size);

#if CONFIG_IS_ENABLED(CMD_TLV_EEPROM)
extern bool tlvinfo_add_tlv(u8 *eeprom, int tcode, char *strval);
#else
/*
 * TLV write-back lives in the tlv_eeprom command. Without it the board can
 * still read identity from the EEPROM; persisting a generated value is a no-op.
 */
static inline bool tlvinfo_add_tlv(u8 *eeprom, int tcode, char *strval)
{
	return false;
}
#endif

/* File scope function prototypes */
static bool is_checksum_valid(u8 *tlv_data);
static __section(".data") uint8_t tlvinfo_buffer[TLVINFO_MAX_SIZE];
static __section(".data") bool had_read_tlvinfo = false;

/**
 *  _is_valid_tlvinfo_header
 *
 *  Perform sanity checks on the first 11 bytes of the TlvInfo
 *  data pointed to by the parameter:
 *      1. First 8 bytes contain null-terminated ASCII string "TlvInfo"
 *      2. Version byte is 1
 *      3. Total length bytes contain value which is less than or equal
 *         to the allowed maximum (2048-11)
 *
 */
bool _is_valid_tlvinfo_header(struct tlvinfo_header *hdr)
{
	return ((strcmp(hdr->signature, TLV_INFO_ID_STRING) == 0) &&
		(hdr->version == TLV_INFO_VERSION) &&
		(be16_to_cpu(hdr->totallen) <= TLV_TOTAL_LEN_MAX));
}

static inline bool is_valid_tlv(struct tlvinfo_tlv *tlv)
{
	return ((tlv->type != 0x00) && (tlv->type != 0xFF));
}

/**
 *  is_checksum_valid
 *
 *  Validate the checksum in the provided TlvInfo data. First,
 *  verify that the TlvInfo header is valid, then make sure the last
 *  TLV is a CRC-32 TLV. Then calculate the CRC over the data
 *  and compare it to the value stored in the CRC-32 TLV.
 */
static bool is_checksum_valid(u8 *tlv_data)
{
	struct tlvinfo_header *tlv_hdr = (struct tlvinfo_header *) tlv_data;
	struct tlvinfo_tlv *tlv_crc;
	unsigned int calc_crc;
	unsigned int stored_crc;

	// Is the tlv_data header valid?
	if (!_is_valid_tlvinfo_header(tlv_hdr)) {
		pr_err("%s, not valid tlv info header\n", __func__);
		return false;
	}

	// Is the last TLV a CRC?
	tlv_crc = (struct tlvinfo_tlv *) (&tlv_data[sizeof(struct tlvinfo_header) +
						    be16_to_cpu(tlv_hdr->totallen) -
						    (sizeof(struct tlvinfo_tlv) + 4)]);
	if (tlv_crc->type != TLV_CODE_CRC_32 || tlv_crc->length != 4)
		return false;

	// Calculate the checksum
	calc_crc = crc32(0, (void *) tlv_data,
			 sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen) - 4);
	stored_crc = (tlv_crc->value[0] << 24) | (tlv_crc->value[1] << 16) |
		     (tlv_crc->value[2] << 8) | tlv_crc->value[3];

	return calc_crc == stored_crc;
}

/**
 *  tlvinfo_find_tlv
 *
 *  This function finds the TLV with the supplied code in the EERPOM.
 *  An offset from the beginning of the TLV info is returned in the
 *  index parameter if the TLV is found.
 */
bool tlvinfo_find_tlv(u8 *tlv_data, u8 tcode, int *index)
{
	struct tlvinfo_header *tlv_hdr = (struct tlvinfo_header *) tlv_data;
	struct tlvinfo_tlv *tlv;
	int tlv_data_end;

	// Search through the TLVs, looking for the first one which matches the
	// supplied type code.
	*index = sizeof(struct tlvinfo_header);
	tlv_data_end = sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen);
	while (*index < tlv_data_end) {
		tlv = (struct tlvinfo_tlv *) (&tlv_data[*index]);
		if (!is_valid_tlv(tlv))
			return false;
		if (tlv->type == tcode)
			return true;
		*index += sizeof(struct tlvinfo_tlv) + tlv->length;
	}
	return (false);
}

/**
 *  update_crc
 *
 *  This function updates the CRC-32 TLV. If there is no CRC-32 TLV, then
 *  one is added. This function should be called after each update to the
 *  TLV info structure, to make sure the CRC is always correct.
 */
static void update_crc(u8 *tlv_data)
{
	struct tlvinfo_header *tlv_hdr = (struct tlvinfo_header *) tlv_data;
	struct tlvinfo_tlv *tlv_crc;
	unsigned int calc_crc;
	int index;

	// Discover the CRC TLV
	if (!tlvinfo_find_tlv(tlv_data, TLV_CODE_CRC_32, &index)) {
		unsigned int totallen = be16_to_cpu(tlv_hdr->totallen);

		if ((totallen + sizeof(struct tlvinfo_tlv) + 4) > TLVINFO_SIZE_MAX_TLV_LEN)
			return;
		index = sizeof(struct tlvinfo_header) + totallen;
		tlv_hdr->totallen = cpu_to_be16(totallen + sizeof(struct tlvinfo_tlv) + 4);
	}
	tlv_crc = (struct tlvinfo_tlv *) (&tlv_data[index]);
	tlv_crc->type = TLV_CODE_CRC_32;
	tlv_crc->length = 4;

	// Calculate the checksum
	calc_crc = crc32(0, (const void *) tlv_data,
			 sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen) - 4);
	tlv_crc->value[0] = (calc_crc >> 24) & 0xFF;
	tlv_crc->value[1] = (calc_crc >> 16) & 0xFF;
	tlv_crc->value[2] = (calc_crc >> 8) & 0xFF;
	tlv_crc->value[3] = (calc_crc >> 0) & 0xFF;
}

static int init_tlv_data(uint8_t *buffer, u32 tlv_size)
{
	return init_tlv_from_eeprom(buffer, tlv_size);
}

static int read_tlvinfo(u8 *tlv_data, u32 tlv_size)
{
	struct tlvinfo_header *tlv_hdr = (struct tlvinfo_header *) tlv_data;
	int ret;

	if (had_read_tlvinfo)
		return 0;

	ret = init_tlv_data(tlv_data, tlv_size);
	if (0 != ret) {
		pr_err("fail to read tlv data\n");
		return ret;
	}

	// If the contents are invalid, start over with default contents
	if (!is_checksum_valid(tlv_data)) {
		strcpy(tlv_hdr->signature, TLV_INFO_ID_STRING);
		tlv_hdr->version = TLV_INFO_VERSION;
		tlv_hdr->totallen = cpu_to_be16(0);
		pr_info("reset tlv data\n");
		update_crc(tlv_data);
	}

	had_read_tlvinfo = true;
	return ret;
}

int get_tlvinfo(int tcode, char *buf, int max_size)
{
	int tlv_end;
	int offset, size;

	if (read_tlvinfo(tlvinfo_buffer, sizeof(tlvinfo_buffer))) {
		pr_err("read tlv info fail\n");
		return -1;
	}

	struct tlvinfo_header *tlv_hdr = (struct tlvinfo_header *) tlvinfo_buffer;
	struct tlvinfo_tlv *tlv;

	offset = sizeof(struct tlvinfo_header);
	tlv_end = sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen);
	while (offset < tlv_end) {
		tlv = (struct tlvinfo_tlv *) (&tlvinfo_buffer[offset]);
		if (!is_valid_tlv(tlv)) {
			pr_err("Invalid TLV field starting at TLV info offset %d\n", offset);
			return -1;
		}

		if (tlv->type == tcode) {
			size = min((uint32_t)tlv->length, (uint32_t)max_size);
			memcpy(buf, tlv->value, size);
			pr_debug("get tlvinfo value:%x,%s\n", tcode, buf);
			return size;
		}
		offset += sizeof(struct tlvinfo_tlv) + tlv->length;
	}
	pr_debug("can not get tlvinfo index:%x\n", tcode);
	return -1;
}

#if !defined(CONFIG_XPL_BUILD)
bool tlvinfo_delete_tlv(u8* tlv_data, u8 code)
{
	int index;
	int tlength;
	struct tlvinfo_header* tlv_hdr = (struct tlvinfo_header*)tlv_data;
	struct tlvinfo_tlv* tlv;

	// Find the TLV and then move all following TLVs "forward"
	if (tlvinfo_find_tlv(tlv_data, code, &index)) {
		tlv = (struct tlvinfo_tlv*)(&tlv_data[index]);
		tlength = sizeof(struct tlvinfo_tlv) + tlv->length;
		memcpy(&tlv_data[index], &tlv_data[index + tlength],
			sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen) - index - tlength);
		tlv_hdr->totallen = cpu_to_be16(be16_to_cpu(tlv_hdr->totallen) - tlength);
		update_crc(tlv_data);
		return true;
	}
	return false;
}

int set_tlvinfo(int tcode, char* val)
{
	/*init tlvinfo at first*/
	if (read_tlvinfo(tlvinfo_buffer, sizeof(tlvinfo_buffer))) {
		pr_err("init tlv info fail\n");
		return -1;
	}

	tlvinfo_delete_tlv(tlvinfo_buffer, tcode);
	if ((val != NULL) && tlvinfo_add_tlv(tlvinfo_buffer, tcode, val))
		return 0;

	return -1;
}

int flush_tlvinfo(void)
{
	struct tlvinfo_header* tlv_hdr;
	int tlv_data_len;

	/*init tlvinfo at first*/
	if (read_tlvinfo(tlvinfo_buffer, sizeof(tlvinfo_buffer))) {
		pr_err("init tlv info fail\n");
		return -1;
	}

	tlv_hdr = (struct tlvinfo_header*)tlvinfo_buffer;
	update_crc(tlvinfo_buffer);

	tlv_data_len = sizeof(struct tlvinfo_header) + be16_to_cpu(tlv_hdr->totallen);
	if (write_tlv_eeprom(tlvinfo_buffer, tlv_data_len)) {
		pr_err("write to tlv_data fail\n");
		return -1;
	}

	return 0;
}
#endif
