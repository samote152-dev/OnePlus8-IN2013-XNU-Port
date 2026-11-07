#!/usr/bin/env python3
"""Replace kernel + ramdisk in an Android boot image v2, preserving its DTB.

This is for a temporary `fastboot boot` test image. It does not flash anything.
AVB footer/signature data is intentionally not copied: a modified image is not
AVB-signed. Use only with an unlocked bootloader and do not flash to a partition.
"""
import argparse
import hashlib
import struct
from pathlib import Path

MAGIC = b"ANDROID!"
HEADER_V2 = 2

def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]

def u64(buf, off):
    return struct.unpack_from("<Q", buf, off)[0]

def align(n, page):
    return (n + page - 1) // page * page

def layout(buf):
    if buf[:8] != MAGIC:
        raise ValueError("not an Android boot image")
    page = u32(buf, 36)
    version = u32(buf, 40)
    if version != HEADER_V2:
        raise ValueError(f"expected boot header v2, got v{version}")
    if not page or page & (page - 1):
        raise ValueError(f"invalid page size {page}")
    ksz, rsz, ssz = u32(buf, 8), u32(buf, 16), u32(buf, 24)
    rdtbo_sz = u32(buf, 1632)
    header_sz = u32(buf, 1644)
    dtb_sz = u32(buf, 1648)
    ko = page
    ro = ko + align(ksz, page)
    so = ro + align(rsz, page)
    rdo = so + align(ssz, page)
    dtbo = rdo + align(rdtbo_sz, page)
    end = dtbo + align(dtb_sz, page)
    if end > len(buf):
        raise ValueError(f"boot image sections exceed file ({end:#x} > {len(buf):#x})")
    if dtb_sz and buf[dtbo:dtbo + 4] != b"\xd0\x0d\xfe\xed":
        raise ValueError(f"DTB magic not found at calculated offset {dtbo:#x}")
    return dict(page=page, version=version, ksz=ksz, rsz=rsz, ssz=ssz,
                rdtbo_sz=rdtbo_sz, header_sz=header_sz, dtb_sz=dtb_sz,
                ko=ko, ro=ro, so=so, rdo=rdo, dtbo=dtbo, end=end)

def build(original, kernel, ramdisk):
    info = layout(original)
    page = info["page"]
    second = original[info["so"]:info["so"] + info["ssz"]]
    recovery = original[info["rdo"]:info["rdo"] + info["rdtbo_sz"]]
    dtb = original[info["dtbo"]:info["dtbo"] + info["dtb_sz"]]
    header = bytearray(original[:page])
    struct.pack_into("<I", header, 8, len(kernel))
    struct.pack_into("<I", header, 16, len(ramdisk))
    ko = page
    ro = ko + align(len(kernel), page)
    so = ro + align(len(ramdisk), page)
    rdo = so + align(len(second), page)
    dtbo = rdo + align(len(recovery), page)
    if info["rdtbo_sz"]:
        struct.pack_into("<Q", header, 1636, rdo)
    # The boot header ID is SHA-1 over each component and its 32-bit length.
    digest = hashlib.sha1()
    for part in (kernel, ramdisk, second, recovery, dtb):
        digest.update(part)
        digest.update(struct.pack("<I", len(part)))
    header[576:608] = digest.digest() + b"\0" * 12
    out = bytearray(header)
    for part in (kernel, ramdisk, second, recovery, dtb):
        out += part
        out += b"\0" * (align(len(part), page) - len(part))
    return bytes(out), dict(info, new_kernel_size=len(kernel), new_ramdisk_size=len(ramdisk),
                             new_dtb_offset=dtbo, output_size=len(out))

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("original_boot_img")
    ap.add_argument("shim_image")
    ap.add_argument("xnu_payload", help="mkpayload.py output; becomes the boot ramdisk/initrd")
    ap.add_argument("output_boot_img")
    a = ap.parse_args()
    original = Path(a.original_boot_img).read_bytes()
    kernel = Path(a.shim_image).read_bytes()
    ramdisk = Path(a.xnu_payload).read_bytes()
    image, info = build(original, kernel, ramdisk)
    Path(a.output_boot_img).write_bytes(image)
    print(f"header: v{info['version']}, page={info['page']}")
    print(f"kernel: {info['new_kernel_size']:,} bytes; ramdisk/payload: {info['new_ramdisk_size']:,} bytes")
    print(f"DTB: {info['dtb_sz']:,} bytes preserved at file offset {info['new_dtb_offset']:#x}")
    print(f"output: {info['output_size']:,} bytes (unsigned; temporary fastboot boot only)")
