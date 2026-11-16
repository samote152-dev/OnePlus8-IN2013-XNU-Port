#!/bin/bash
# Build the AArch64 boot shim. Toolchain commands can be overridden via env.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-aarch64-linux-gnu-gcc}
OBJCOPY=${OBJCOPY:-aarch64-linux-gnu-objcopy}
READELF=${READELF:-aarch64-linux-gnu-readelf}
NM=${NM:-aarch64-linux-gnu-nm}
TARGET_FLAGS=${TARGET_FLAGS:-}
LINK_FLAGS=${LINK_FLAGS:-}

# Convenient LLVM/Clang path for Linux hosts without a GNU cross toolchain:
# CC=clang OBJCOPY=llvm-objcopy READELF=readelf NM=nm \
# TARGET_FLAGS=--target=aarch64-linux-gnu LINK_FLAGS=-fuse-ld=lld ./build.sh
if [[ "${CC##*/}" == clang* ]]; then
    TARGET_FLAGS=${TARGET_FLAGS:---target=aarch64-linux-gnu}
    LINK_FLAGS=${LINK_FLAGS:--fuse-ld=lld}
    READELF=${READELF:-readelf}
    NM=${NM:-nm}
    OBJCOPY=${OBJCOPY:-llvm-objcopy}
fi
SHIM_DEFINES=${SHIM_DEFINES:-}
CFLAGS="${TARGET_FLAGS} ${SHIM_DEFINES} -O2 -ffreestanding -nostdlib -fno-pic -fno-jump-tables -mgeneral-regs-only -mstrict-align -fno-stack-protector -Wall -Wextra"
mkdir -p out
for f in start.S shim.c console.c mmu.c fdt.c loader.c ufs.c; do
    $CC $CFLAGS -c "$f" -o "out/${f%.*}.o"
done
abs=$($READELF -r out/shim.o out/console.o out/mmu.o out/fdt.o out/loader.o out/ufs.o | grep -c ABS64 || true)
[ "$abs" = 0 ] || { echo "error: $abs absolute relocations in C objects" >&2; exit 1; }
$CC $LINK_FLAGS $CFLAGS -static -T shim.ld -Wl,--build-id=none -Wl,-N \
    out/start.o out/shim.o out/console.o out/mmu.o out/fdt.o out/loader.o out/ufs.o -o out/shim.elf
$OBJCOPY -O binary out/shim.elf out/Image
end=$($NM out/shim.elf | awk '$3=="_end" {print $1}')
[ -n "$end" ] || { echo 'error: _end symbol not found' >&2; exit 1; }
python3 - "$end" <<'PY'
import sys
size = int(sys.argv[1], 16)
with open('out/Image', 'ab') as f:
    f.truncate(size)
print(f'Image padded to {size} bytes')
PY
# Host render of the console layout.
gcc -DSWEET_FB_VERIFIED -O1 -Wall -o out/host_test console.c host_test.c
./out/host_test
if [[ "${BUILD_BOOTIMG:-0}" == 1 ]]; then
    : "${DTB_PATH:=../dtb/boot_unpacked/dtb}"
    : "${RAMDISK_PATH:=out/ramdisk.bin}"
    : "${MKBOOTIMG:=mkbootimg}"
    if [[ ! -f "$DTB_PATH" ]]; then echo "missing DTB: $DTB_PATH" >&2; exit 1; fi
    if [[ ! -f "$RAMDISK_PATH" ]]; then
        (cd out && rm -rf rd && mkdir rd && cd rd && find . | cpio -o -H newc --quiet | gzip -9 > ../ramdisk.bin)
    fi
    "$MKBOOTIMG" --header_version 2 --kernel out/Image --ramdisk "$RAMDISK_PATH" \
        --dtb "$DTB_PATH" --base 0 --kernel_offset 0x8000 --ramdisk_offset 0x1000000 \
        --tags_offset 0x100 --dtb_offset 0x1f00000 --pagesize 4096 \
        --os_version 17.0.0 --os_patch_level 2035-06 --cmdline "console=null" -o out/shim-boot.img
fi
file out/shim.elf out/Image
ls -l out/Image ${BUILD_BOOTIMG:+out/shim-boot.img}
