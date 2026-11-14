#!/usr/bin/env python3
"""Patch the pinned darwin-xnu-build KDK downloader with integrity checks.

The upstream script downloads a DMG and immediately runs hdiutil attach. A
transient or HTTP-error body can therefore masquerade as a corrupt image.
Resolve the selected KDK entry from Dortania's manifest and validate size and
SHA-256 before mounting. Fail closed if the pinned upstream block changes.
"""
from pathlib import Path
import sys

OLD = """        KDK_URL=$(curl -s "https://raw.githubusercontent.com/dortania/KdkSupportPkg/gh-pages/manifest.json" | jq -r --arg KDK_NAME "$KDK_NAME" '.[] | select(.name==$KDK_NAME) | .url')
        running "Downloading '$KDK_NAME' to /tmp"
        curl --progress-bar --max-time 900 --connect-timeout 60 -L -o /tmp/KDK.dmg "${KDK_URL}"
        running "Installing KDK"
        hdiutil attach /tmp/KDK.dmg
"""

NEW = r'''        KDK_MANIFEST_URL="https://raw.githubusercontent.com/dortania/KdkSupportPkg/gh-pages/manifest.json"
        KDK_MANIFEST=$(curl --fail --silent --show-error --location --retry 4 --retry-all-errors --max-time 120 "$KDK_MANIFEST_URL")
        KDK_ENTRY=$(printf '%s' "$KDK_MANIFEST" | jq -ce --arg KDK_NAME "$KDK_NAME" '.[] | select(.name==$KDK_NAME)')
        KDK_URL=$(printf '%s' "$KDK_ENTRY" | jq -er '.url')
        KDK_SIZE=$(printf '%s' "$KDK_ENTRY" | jq -er '.fileSize')
        KDK_SHA256=$(printf '%s' "$KDK_ENTRY" | jq -er '.sha256sum' | tr '[:upper:]' '[:lower:]')
        if ! [[ "$KDK_SIZE" =~ ^[0-9]+$ && "$KDK_SHA256" =~ ^[0-9a-f]{64}$ ]]; then
            error "Invalid KDK manifest metadata for $KDK_NAME"
            exit 1
        fi
        running "Downloading and verifying '$KDK_NAME' to /tmp"
        KDK_VALID=0
        for KDK_ATTEMPT in 1 2 3; do
            rm -f /tmp/KDK.dmg
            if curl --fail --silent --show-error --location --retry 4 --retry-all-errors --max-time 900 --connect-timeout 60 --output /tmp/KDK.dmg "$KDK_URL"; then
                KDK_ACTUAL_SIZE=$(stat -f%z /tmp/KDK.dmg)
                if [ "$KDK_ACTUAL_SIZE" = "$KDK_SIZE" ] && printf '%s  %s\n' "$KDK_SHA256" /tmp/KDK.dmg | shasum -a 256 -c -; then
                    KDK_VALID=1
                    break
                fi
                echo "KDK integrity mismatch on attempt $KDK_ATTEMPT: size $KDK_ACTUAL_SIZE, expected $KDK_SIZE"
            else
                echo "KDK download failed on attempt $KDK_ATTEMPT"
            fi
            sleep 3
        done
        if [ "$KDK_VALID" -ne 1 ]; then
            error "KDK download failed size/SHA-256 validation after 3 attempts"
            exit 1
        fi
        hdiutil verify /tmp/KDK.dmg
        running "Installing KDK"
        hdiutil attach /tmp/KDK.dmg
'''
if len(sys.argv) != 2:
    raise SystemExit("usage: patch-dxb-kdk-download.py PATH/TO/dxb/build.sh")
path = Path(sys.argv[1])
text = path.read_text()
count = text.count(OLD)
if count != 1:
    raise SystemExit(f"expected exactly one pinned KDK download block, found {count}; refusing to patch")
path.write_text(text.replace(OLD, NEW, 1))
print(f"patched and integrity-guarded KDK download in {path}")
