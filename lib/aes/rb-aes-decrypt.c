#ifndef USE_HOSTCC
#include <malloc.h>
#endif
#include <image.h>
#include <uboot_aes.h>
#include <rambus/rambus_aes.h>
#include <rambus/kdf.h>

int image_aes_decrypt(struct image_cipher_info *info, const void *cipher, size_t cipher_len, void **data, size_t *size)
{
    csi_aes_t aes;
    csi_error_t ret = CSI_OK;
    csi_kdf_key_handle_t handle;

    puts("rb aes decrypt ");
    *data = malloc(cipher_len);
    if (!*data) {
        debug("Can't allocate memory to decrypt\n");
        return -ENOMEM;
    }
    *size = info->size_unciphered;

    ret = csi_aes_init(&aes, 0);
    if (ret != CSI_OK) {
        debug("%s: Error in csi_aes_init\n", __func__);
        return -EACCES;
    }

    aes.context.is_kdf = 1;
    aes.context.is_dma = 1;
    handle.type = KDF_KEY_TYPE_AES_256;
    handle.aes = &aes;

    /* set key uing keyram KDF_PRO_USER_IMAGE_EK key */
    ret = csi_kdf_set_key(&handle, KDF_PRO_USER_IMAGE_EK);
    if (ret != CSI_OK) {
        debug("%s: Error in csi_kdf_set_key\n", __func__);
        goto out;
    }

    /* aes decrypt */
    ret = csi_aes_cbc_decrypt(&aes, cipher, (void *)(*data), (uint32_t)cipher_len, (void *)info->iv);
    if (ret != CSI_OK) {
        debug("%s: Error in csi_aes_cbc_decrypt\n", __func__);
        goto out;
    }

out:
    csi_aes_uninit(&aes);
    if (ret != CSI_OK)
        return -EACCES;
    return 0;
}
