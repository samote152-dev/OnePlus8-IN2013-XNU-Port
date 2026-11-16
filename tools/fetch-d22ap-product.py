#!/usr/bin/env python3
"""Fetch only the D22AP DeviceTree member from Apple's iOS 16.5 IPSW via HTTP Range.

The 5.7 GiB IPSW is never downloaded in full. RemoteZip reads the ZIP directory
and the single DeviceTree member from Apple's CDN. The caller then decodes IM4P
with blacktop/ipsw and mkadt-in2013.py imports only /product properties.
"""
from pathlib import Path
from hashlib import sha256
from remotezip import RemoteZip

IPSW_URL = (
    "https://updates.cdn-apple.com/2023SpringFCS/fullrestores/032-85130/"
    "6AB0D10A-8AF9-4F6C-8CF5-A1B315CABE88/"
    "iPhone10%2C3%2CiPhone10%2C6_16.5_20F66_Restore.ipsw"
)
MEMBER_SUFFIX = "DeviceTree.d22ap.im4p"
OUTPUT = Path("research/ipsw-x/DeviceTree.d22ap.im4p")
MAX_MEMBER_SIZE = 16 * 1024 * 1024


def main() -> None:
    with RemoteZip(IPSW_URL, timeout=90, support_suffix_range=False) as archive:
        matches = [info for info in archive.infolist()
                   if info.filename.endswith(MEMBER_SUFFIX)]
        if len(matches) != 1:
            raise SystemExit(
                f"expected one {MEMBER_SUFFIX} member, found "
                f"{[info.filename for info in matches]}"
            )
        info = matches[0]
        if not 0 < info.file_size <= MAX_MEMBER_SIZE:
            raise SystemExit(f"unexpected DeviceTree member size: {info.file_size}")
        data = archive.read(info.filename)  # ZIP CRC is checked by zipfile.
        if len(data) != info.file_size:
            raise SystemExit(
                f"short DeviceTree read: {len(data)} != {info.file_size}"
            )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(data)
    print(f"member: {info.filename}")
    print(f"uncompressed bytes: {len(data)}")
    print(f"sha256: {sha256(data).hexdigest()}")
    print("transfer mode: HTTP Range; full IPSW not downloaded")


if __name__ == "__main__":
    main()
