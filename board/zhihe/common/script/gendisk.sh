#!/bin/sh

# 全局定义
RVBL_HEAD_SZIE=2048

# 定义参数变量
do_help=0
do_fit=0
do_image=0
do_sec_image=0

TEMP=$(getopt -o h --long help,fit,image,sec-image -n "$0" -- "$@")
if [ $? != 0 ]; then
    echo "Error: parameter parsing failed" >&2
    exit 1
fi

eval set -- "$TEMP"
while true; do
    case "$1" in
        -h|--help)
            do_help=1
            shift
            ;;
        --fit)
            do_fit=1
            shift
            ;;
        --image)
            do_image=1
            shift
            ;;
        --sec-image)
            do_sec_image=1
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "unknown error"
            exit 1
            ;;
    esac
done

#################
# Common Function
#################
# Generate RVBL firmware
# $1 - bin filename
# $2 - payload filename
# $3 - output firmware filename
generate_rvbl() {
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
    HEAD_PAD_BIN_SIZE=$(( PAD_BIN_SIZE + RVBL_HEAD_SZIE ))

    echo "6F0010005256424C" | xxd -r -p > ${OUT_FILE}
    echo ${PAD_BIN_SIZE} | awk '{printf("%08x\n", $0+2048)}' | xxd -r -p| od -An -tx4 -w4 | xxd -r -p >> ${OUT_FILE}
    if [ -e ${PLD_FILE} ]; then
        stat -c%s ${PLD_FILE} | awk '{printf("%08x\n", $0)}' | xxd -r -p| od -An -tx4 -w4 | xxd -r -p >> ${OUT_FILE}
    fi

    fallocate -l ${RVBL_HEAD_SZIE} ${OUT_FILE}
    cat ${BIN_FILE} >> ${OUT_FILE}
    fallocate -l ${HEAD_PAD_BIN_SIZE} ${OUT_FILE}

    if [ -e ${PLD_FILE} ]; then
        cat ${PLD_FILE} >> ${OUT_FILE}
    fi
}

#################
# Main Command
#################

# Help
do_help() {
    echo help
}

# Generate FIT itb
# $1 its file
# $2 work dir, input images path
# $3 output itb file
do_fit() {
    ITS_FILE_ORG=$1
    ITS_FILE=$(mktemp --suffix=.its)
    cp ${ITS_FILE_ORG} ${ITS_FILE}
    WORK_PATH=$2
    OUT_BOOT_ITB=$3

    # Save hash
    cat $1 | grep "replace-path.*" | awk -v wp="${WORK_PATH}" -F'.gz|/|"' '{print wp"/"$5}' | xargs sha256sum > ${WORK_PATH}/riscv-boot.hash
    sed -i "s|${WORK_PATH}/||g" ${WORK_PATH}/riscv-boot.hash

    # GZ file
    cat $1 | grep "replace-path.*gz" | awk -F'.gz|/' '{print $4}' | xargs -i gzip -kf ${WORK_PATH}/{}

    # Replace image path
    sed -i "s#replace-path#${WORK_PATH}#g" ${ITS_FILE}

    cat ${ITS_FILE}

    # Generate itb
    mkimage -f ${ITS_FILE} ${OUT_BOOT_ITB} || exit 1

    # Cleanup tmp files
    cat ${ITS_FILE_ORG} | grep "replace-path.*gz" | awk -F'.gz|/' '{print $4}' | xargs -i rm ${WORK_PATH}/{}.gz
    rm -f ${ITS_FILE}
}

# Gen Images
# $1 - Bootzero bin filename
# $2 - SPL bin filename
# $3 - boot itb
# $4 - output path
do_image() {
    FILE_BTZ=$1
    FILE_SPL=$2
    FILE_ITB=$3
    OUT_PATH=$4

    echo "Generate boot firmware: btz-with-uboot-rvbl.bin"
    BTZ_SPL_FILE=${OUT_PATH}/btz-with-spl-rvbl.bin
    BTZ_UBOOT_FILE=${OUT_PATH}/btz-with-uboot-rvbl.bin
    SPL_RVBL=${OUT_PATH}/u-boot-spl-rvbl.bin
    generate_rvbl ${FILE_SPL} none ${SPL_RVBL}

    if [ "$1" = "none" ]; then
         cp ${SPL_RVBL} ${BTZ_SPL_FILE}
    else
        # bootzero2.bin
        cp $1 ${BTZ_SPL_FILE}
        cat ${SPL_RVBL} >> ${BTZ_SPL_FILE}
    fi
    cp ${BTZ_SPL_FILE} ${BTZ_UBOOT_FILE}
    fallocate -l 0xD0000 ${BTZ_UBOOT_FILE}
    cat $3 >> ${BTZ_UBOOT_FILE}

    # Cleanup tmp file
    rm -f ${SPL_RVBL}

    echo "Generate programming firmware: spl-with-fit-rvbl.bin"
    SPL_FIT_FILE=${OUT_PATH}/spl-with-fit-rvbl.bin
    generate_rvbl ${FILE_SPL} ${FILE_ITB} ${SPL_FIT_FILE}
}

# Gen sec Images
# $1 - Bootzero sec bin filename
# $2 - SPL sec bin filename
# $3 - boot sec itb
# $4 - output path
do_sec_image() {
    FILE_BTZ=$1
    FILE_SPL=$2
    FILE_ITB=$3
    OUT_PATH=$4

    echo "Generate boot firmware: btz-with-uboot-rvbl.bin"
    BTZ_SPL_FILE=${OUT_PATH}/btz-with-spl-rvbl.bin
    BTZ_UBOOT_FILE=${OUT_PATH}/btz-with-uboot-rvbl.bin

    if [ "$1" = "none" ]; then
         cp ${FILE_SPL} ${BTZ_SPL_FILE}
    else
        # bootzero2-sec.bin
        cp $1 ${BTZ_SPL_FILE}
        cat ${FILE_SPL} >> ${BTZ_SPL_FILE}
    fi
    cp ${BTZ_SPL_FILE} ${BTZ_UBOOT_FILE}
    fallocate -l 0xD0000 ${BTZ_UBOOT_FILE}
    cat $3 >> ${BTZ_UBOOT_FILE}

    echo "Generate programming firmware: spl-with-fit-rvbl.bin"
    SPL_FIT_FILE=${OUT_PATH}/spl-with-fit-rvbl.bin
    rm -f ${SPL_FIT_FILE}
    cat ${FILE_SPL} >> ${SPL_FIT_FILE}
    cat ${FILE_ITB} >> ${SPL_FIT_FILE}
}

#################
# main
#################
if [ $do_help -eq 1 ]; then
    do_help
elif [ $do_fit -eq 1 ]; then
    do_fit $@
elif [ $do_image -eq 1 ]; then
    do_image $@
elif [ $do_sec_image -eq 1 ]; then
    do_sec_image $@
else
    echo "v20251022"
    exit 1
fi
