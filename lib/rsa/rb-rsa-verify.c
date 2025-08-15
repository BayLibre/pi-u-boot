#ifndef USE_HOSTCC
#include <asm/byteorder.h>
#include <asm/types.h>
#include <asm/unaligned.h>
#include <dm.h>
#include <fdtdec.h>
#include <linux/errno.h>
#include <log.h>
#include <malloc.h>
#else
#include "fdt_host.h"
#include "mkimage.h"
#include <fdt_support.h>
#include <linux/kconfig.h>
#endif
#include <rambus/rambus_rsa.h>
#include <rambus/rambus.h>
#include <u-boot/rsa-mod-exp.h>
#include <u-boot/rsa.h>

/**
 * rsa_verify_with_pkey() - Verify a signature against some data using
 * only modulus and exponent as RSA key properties.
 * @info:	Specifies key information
 * @hash:	Pointer to the expected hash
 * @sig:	Signature
 * @sig_len:	Number of bytes in signature
 *
 * Parse a RSA public key blob in DER format pointed to in @info and fill
 * a key_prop structure with properties of the key. Then verify a RSA PKCS1.5
 * signature against an expected hash using the calculated properties.
 *
 * Return	0 if verified, -ve on error
 */

int padding_pkcs_15_verify(struct image_sign_info *info, const uint8_t *msg, int msg_len, const uint8_t *hash,
                           int hash_len)
{
    return -EACCES;
}

int padding_pss_verify(struct image_sign_info *info, const uint8_t *msg, int msg_len, const uint8_t *hash, int hash_len)
{
    return -EACCES;
}

int rsa_verify_with_pkey(struct image_sign_info *info, const void *hash, uint8_t *sig, uint sig_len)
{
    return -EACCES;
}

int rsa_verify_hash(struct image_sign_info *info, const uint8_t *hash, uint8_t *sig, uint sig_len)
{
    return -EACCES;
}

static int rsa_verify_using_rb_csi(struct image_sign_info *info, const uint8_t *hash, uint8_t hash_len, uint8_t *sig,
                                   uint sig_len, int node)
{
    const void *blob = info->fdt_blob;
    int length;
    const char *algo;
    csi_rsa_t rsa;
    csi_rsa_context_t context;
    csi_error_t csi_ret;
    int ret;
    static uint8_t test_rsa_2048_e[256] __attribute__((aligned(64))) = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x01
    };

    ret = rambus_crypto_open();
    if(ret != 0) {
        debug("%s: rambus_crypto_open failed, error:%d\n", __func__, ret);
        return -EBUSY;
    }

    /* Init rb rsa */
    csi_ret = csi_rsa_init(&rsa, 0);
    if (csi_ret != CSI_OK) {
        debug("%s: Error in csi_rsa_init\n", __func__);
        ret = -EACCES;
        goto output;
    }
    context.key_bits = RSA_KEY_BITS_2048;
    context.is_hash = RSA_HASH_DISABLE;
    context.is_crt = RSA_CRT_DISABLE;
    context.padding_type = RSA_PADDING_MODE_PKCS1;
    context.hash_type = RSA_HASH_TYPE_SHA256;

    if (node < 0) {
        debug("%s: Skipping invalid node\n", __func__);
        ret = -EBADF;
        goto output;
    }

    algo = fdt_getprop(blob, node, "algo", NULL);
    if (strcmp(info->name, algo)) {
        debug("%s: Wrong algo: have %s, expected %s\n", __func__, info->name, algo);
        ret = -EFAULT;
        goto output;
    }

    /* Set public key param*/
    // context.e = fdt_getprop(blob, node, "rsa,exponent", &length);
    context.n = (void *)fdt_getprop(blob, node, "rsa,modulus", NULL);
    context.e = test_rsa_2048_e;
    length = 64;
    if (!context.e || !context.n || length < sizeof(uint64_t)) {
        ret = -EFAULT;
        goto output;
    }

    bool bool_ret = csi_rsa_verify(&rsa, &context, hash, hash_len, sig, sig_len, RSA_HASH_TYPE_SHA256);

    if (bool_ret == true) {
        debug("%s: success\n", "csi_rsa_verify");
        ret = 0;
    } else {
        ret = -EFAULT;
    }

output:
    rambus_crypto_close();
    return ret;
}

static int rb_rsa_verify_hash(struct image_sign_info *info, const uint8_t *hash, uint8_t hash_len, uint8_t *sig,
                              uint sig_len)
{
    int ret = 0;

    if (CONFIG_IS_ENABLED(FIT_SIGNATURE)) {
        const void *blob = info->fdt_blob;
        int ndepth, noffset;
        int sig_node, node;
        char name[100];

        sig_node = fdt_subnode_offset(blob, 0, FIT_SIG_NODENAME);
        if (sig_node < 0) {
            debug("%s: No signature node found\n", __func__);
            return -ENOENT;
        }

        /* See if we must use a particular key */
        if (info->required_keynode != -1) {
            ret = rsa_verify_using_rb_csi(info, hash, hash_len, sig, sig_len, info->required_keynode);
            if (ret)
                debug("%s: Failed to verify required_keynode\n", __func__);
            return ret;
        }

        /* Look for a key that matches our hint */
        snprintf(name, sizeof(name), "key-%s", info->keyname);
        node = fdt_subnode_offset(blob, sig_node, name);
        ret = rsa_verify_using_rb_csi(info, hash, hash_len, sig, sig_len, node);
        if (!ret)
            return ret;
        debug("%s: Could not verify key '%s', trying all\n", __func__, name);

        /* No luck, so try each of the keys in turn */
        for (ndepth = 0, noffset = fdt_next_node(blob, sig_node, &ndepth); (noffset >= 0) && (ndepth > 0);
             noffset = fdt_next_node(blob, noffset, &ndepth)) {
            if (ndepth == 1 && noffset != node) {
                ret = rsa_verify_using_rb_csi(info, hash, hash_len, sig, sig_len, noffset);
                if (!ret)
                    break;
            }
        }
    }
    debug("%s: Failed to verify by any means\n", __func__);

    return ret;
}

int rsa_verify(struct image_sign_info *info, const struct image_region region[], int region_count, uint8_t *sig,
               uint sig_len)
{
    /* Reserve memory for maximum checksum-length */
    uint8_t hash[info->crypto->key_len];
    int ret;

    /*
   * Verify that the checksum-length does not exceed the
   * rsa-signature-length
   */
    if (info->checksum->checksum_len > info->crypto->key_len) {
        debug("%s: invalid checksum-algorithm %s for %s\n", __func__, info->checksum->name, info->crypto->name);
        return -EINVAL;
    }

    /* Calculate checksum with checksum-algorithm */
    ret = info->checksum->calculate(info->checksum->name, region, region_count, hash);
    if (ret < 0) {
        debug("%s: Error in checksum calculation\n", __func__);
        return -EINVAL;
    }

    return rb_rsa_verify_hash(info, hash, RSA_256_BYTE_LEN, sig, sig_len);
}

U_BOOT_CRYPTO_ALGO(rsa2048) = {
    .name = "rsa2048",
    .key_len = RSA2048_BYTES,
    .verify = rsa_verify,
};

#ifndef USE_HOSTCC
U_BOOT_PADDING_ALGO(pkcs_15) = {
    .name = "pkcs-1.5",
    .verify = padding_pkcs_15_verify,
};
#endif
