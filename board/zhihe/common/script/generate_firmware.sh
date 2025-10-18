#!/bin/bash

# Generate RVBL firmware
# $1 - bin filename
# $2 - payload filename
# $3 - output firmware filename
function generate_rvbl() {
    BIN_FILE=$1
    PLD_FILE=$2
    OUT_FILE=$3

    if [ ! -e ${BIN_FILE} ]; then
        echo "  File ${BIN_FILE} does not exist."
        exit 1;
    fi

    BIN_SIZE=$(stat -c%s ${BIN_FILE})
    PAD_SIZE=$(( ( 16 - (BIN_SIZE % 16)) % 16 ))
    PAD_BIN_SIZE=$(( BIN_SIZE + PAD_SIZE ))

    echo "6F0010005256424C" | xxd -r -p > ${OUT_FILE}
    echo ${PAD_BIN_SIZE} | awk '{printf("%08x\n", $0+2048)}' | xxd -r -p| od -An -tx4 -w4 | xxd -r -p >> ${OUT_FILE}
    if [ -e ${PLD_FILE} ]; then
        stat -c%s ${PLD_FILE} | awk '{printf("%08x\n", $0)}' | xxd -r -p| od -An -tx4 -w4 | xxd -r -p >> ${OUT_FILE}
    else
        echo "00000000" | xxd -r -p| od -An -tx4 -w4 | xxd -r -p >> ${OUT_FILE}
    fi
    dd if=/dev/zero  bs=1 count=2032 >> ${OUT_FILE} 2>/dev/null
    cat ${BIN_FILE} >> ${OUT_FILE}

    if [ ${PAD_SIZE} -ne 0 ]; then
        echo "  rvbl ${BIN_FILE} padding ${PAD_SIZE} bytes"
        dd if=/dev/zero  bs=1 count=${PAD_SIZE} >> ${OUT_FILE} 2>/dev/null
    fi

    if [ -e ${PLD_FILE} ]; then
        cat ${PLD_FILE} >> ${OUT_FILE}
    fi
}

# Generate bare chip programming
# Integrate the spl and boot itb files
# $1 - SPL bin filename
# $2 - Boot itb filename
# $3 - Output firmware filename
function generate_programming_firmware() {
    echo "Generate programming firmware"

    BOOT_ITB_FILE=riscv-boot.itb
    SPL_FIT_FILE=spl-with-fit-rvbl.bin

    if [ -n "$2" ]; then
        BOOT_ITB_FILE=$2
    fi

    if [ -n "$3" ]; then
        SPL_FIT_FILE=$3
    fi

    generate_rvbl $1 ${BOOT_ITB_FILE} ${SPL_FIT_FILE}
}


# Generate Boot firmware
# Integrate the Bootzero and SPL bin files
# $1 - Bootzero bin filename
# $2 - SPL bin filename
# $3 - boot itb
function generate_boot_firmware() {
    echo "Generate boot firmware"

    BTZ_SPL_FILE=btz-with-spl-rvbl.bin
    BTZ_UBOOT_FILE=btz-with-uboot-rvbl.bin

    generate_rvbl $2 none u-boot-spl-rvbl.bin

    if [ "$1" == "none" ]; then
         cp u-boot-spl-rvbl.bin ${BTZ_SPL_FILE}
    else
        # bootzero2.bin
        cp $1 ${BTZ_SPL_FILE}
        cat u-boot-spl-rvbl.bin >> ${BTZ_SPL_FILE}
    fi
    cp ${BTZ_SPL_FILE} ${BTZ_UBOOT_FILE}
    fallocate -l 0xD0000 ${BTZ_UBOOT_FILE}
    cat $3 >> ${BTZ_UBOOT_FILE}
}

if [ "$1" == "rvbl" ]; then
    generate_rvbl $2 $3 $4
    exit 0
fi

if [ "$1" != "none" ]; then
    if [ ! -e $1 ]; then
        echo "  Bootzero: $1 does not exist."
        exit 1;
    fi
fi

if [ ! -e $2 ]; then
    echo "  SPL: $2 does not exist."
    exit 1;
fi

generate_boot_firmware $1 $2 $3

if [ -z $3 ]; then
    echo "  The third parameter is null, ignore generate programming firmware"
    exit 0;
fi

if [ ! -e $3 ]; then
    echo "  ITB: $3 does not exist."
    exit 1;
fi

generate_programming_firmware $2 $3

# Example1: Generate all firmware
# generate_firmware bootzero/bootzero.bin u-boot/spl/u-boot-spl.bin riscv-boot.itb
#
# Output files
# bootzero-rvbl.bin u-boot-spl-rvbl.bin btz-with-spl-rvbl.bin
#
# Need to prepare the riscv-boot.itb file in advance
# Before calling mkimage, please confirm that the correct file path is configured in the riscv-boot.its file
# ----
# cp u-boot/board/zhihe/a210-evb/riscv-boot.its .
# gzip -kf opensbi/build/platform/generic/firmware/fw_dynamic.bin
# gzip -kf u-boot/u-boot.bin
# mkimage -f riscv-boot.its riscv-boot.itb
# ----
#
# Example2: Generate boot firmware
# u-boot/board/zhihe/a210-evb/script/generate_firmware.sh bootzero/bootzero.bin u-boot/spl/u-boot-spl.bin
#
# Output files
# bootzero-rvbl.bin u-boot-spl-rvbl.bin spl-with-fit-rvbl.bin btz-with-spl-rvbl.bin
#
# Example3: Generate rvbl firmware
# u-boot/board/zhihe/a210-evb/script/generate_firmware.sh rvbl spl/u-boot-spl.bin spl/u-boot-spl-rvbl.bin
#
# Output files
# spl/u-boot-spl-rvbl.bin
#
