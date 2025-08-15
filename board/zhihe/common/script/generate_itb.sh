#!/bin/bash

# Generate FIT itb files
# $1 - input its path
# $2 - input images path
# $3 - output path
# $4 - config fdt filename

ITS_PATH=$1
IMG_PATH=$2
OUT_BOOT_ITB=$3/riscv-boot.itb
OUT_LINUX_ITB=$3/riscv-linux.itb

# Copy its file
ITS_FILE_LINUX=$(mktemp --suffix=.its)
ITS_FILE_BOOT=$(mktemp --suffix=.its)
cp ${ITS_PATH}/riscv-linux.its ${ITS_FILE_LINUX}
cp ${ITS_PATH}/riscv-boot.its ${ITS_FILE_BOOT}

# Replace image path
sed -i "s#replace-path#${IMG_PATH}#g" ${ITS_FILE_LINUX}
sed -i "s#replace-path#${IMG_PATH}#g" ${ITS_FILE_BOOT}

# Replace fdt filename
FDT_NAME=p100-haps.dtb
if [ -n "$4" ]; then
    FDT_NAME=$4
fi
sed -i "s#replace-dtb#${FDT_NAME}#g" ${ITS_FILE_LINUX}
sed -i "s#replace-dtb#${FDT_NAME}#g" ${ITS_FILE_BOOT}

# Generate riscv-linux.itb
mkimage -f ${ITS_FILE_LINUX} ${OUT_LINUX_ITB}

# Generate riscv-boot.itb
gzip -kf ${IMG_PATH}/fw_dynamic.bin
gzip -kf ${IMG_PATH}/u-boot.bin
mkimage -f ${ITS_FILE_BOOT} ${OUT_BOOT_ITB}

# Cleanup tmp files
rm -f ${IMG_PATH}/fw_dynamic.bin.gz ${IMG_PATH}/u-boot.bin.gz
rm -f ${ITS_FILE_LINUX} ${ITS_FILE_BOOT}
