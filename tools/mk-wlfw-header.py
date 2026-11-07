#!/usr/bin/env python3
"""Generate the small XNU-facing WLFW/QMI header from Linux ath11k sources.

The upstream QMI message layouts and element tables are kept in Linux's qmi.h/qmi.c.
This script extracts only the WLAN firmware definitions needed by sweet_wlfw.c and
emits them as a final patch in the XNU patch series.
"""
from __future__ import annotations

import argparse
import difflib
import re
from pathlib import Path


def extract_braced(text: str, start: int) -> str:
    opening = text.find("{", start)
    if opening < 0:
        raise ValueError("declaration has no opening brace")
    depth = 0
    for pos in range(opening, len(text)):
        char = text[pos]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                end = text.find(";", pos)
                if end < 0:
                    raise ValueError("declaration has no terminating semicolon")
                return text[start:end + 1]
    raise ValueError("unterminated declaration")


def declarations(text: str, pattern: str) -> list[str]:
    found = []
    seen = set()
    for match in re.finditer(pattern, text, re.MULTILINE):
        block = extract_braced(text, match.start())
        if block not in seen:
            found.append(block)
            seen.add(block)
    return found


def macros(text: str) -> list[str]:
    result = []
    lines = text.splitlines()
    i = 0
    wanted = re.compile(r"^(?:ATH11K_QMI_|QMI_WLANFW_|QMI_WLFW_|WLANFW_)")
    while i < len(lines):
        line = lines[i]
        match = re.match(r"\s*#define\s+([A-Za-z0-9_]+)", line)
        if match and wanted.match(match.group(1)):
            block = [line.strip()]
            while block[-1].endswith("\\") and i + 1 < len(lines):
                i += 1
                block.append(lines[i].strip())
            result.append("\n".join(block))
        i += 1
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ath11k_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--patch", type=Path, required=True)
    args = parser.parse_args()

    qmi_h = (args.ath11k_dir / "qmi.h").read_text()
    qmi_c = (args.ath11k_dir / "qmi.c").read_text()

    type_pattern = (
        r"\b(?:struct|enum)\s+"
        r"(?:qmi_wlanfw_[A-Za-z0-9_]+|qmi_wlfw_[A-Za-z0-9_]+)"
        r"\s*\{"
    )
    types = declarations(qmi_h, type_pattern)
    bdf_enum = declarations(qmi_h, r"\benum\s+ath11k_qmi_bdf_type\s*\{")
    tables = declarations(
        qmi_c,
        r"(?:static\s+)?const\s+struct\s+qmi_elem_info\s+"
        r"qmi_(?:wlanfw|wlfw)_[A-Za-z0-9_]+_ei\s*\[\s*\]\s*=\s*\{",
    )
    defines = macros(qmi_h)

    required_types = (
        "struct qmi_wlanfw_host_cap_req_msg_v01",
        "struct qmi_wlanfw_ind_register_req_msg_v01",
        "struct qmi_wlanfw_request_mem_ind_msg_v01",
        "struct qmi_wlanfw_respond_mem_req_msg_v01",
        "struct qmi_wlanfw_cap_resp_msg_v01",
        "struct qmi_wlanfw_bdf_download_req_msg_v01",
        "struct qmi_wlanfw_wlan_mode_req_msg_v01",
    )
    joined_types = "\n".join(types)
    missing = [name for name in required_types if name not in joined_types]
    if missing:
        raise SystemExit("qmi.h is missing required WLFW definitions: " + ", ".join(missing))
    if not bdf_enum:
        raise SystemExit("qmi.h is missing enum ath11k_qmi_bdf_type")
    if len(tables) < 10:
        raise SystemExit(f"qmi.c yielded only {len(tables)} WLFW element tables; refusing an incomplete header")

    header = [
        "/* Generated from Linux ath11k qmi.h/qmi.c (Linux 7.0.14). Do not edit. */",
        "#ifndef SWEET_WLFW_ATH11K_H",
        "#define SWEET_WLFW_ATH11K_H",
        "",
        "#include <limits.h>",
        "#include <stddef.h>",
        "#include <stdint.h>",
        '#include "hakios.qmi.h"',
        "",
        '#define WLFW_CHIP "QCA6390"',
        "#define WLFW_SERVICE_INSTANCE ATH11K_QMI_WLFW_SERVICE_INS_ID_V01_QCA6390",
        "",
        "/* ath11k QMI constants used by the XNU WLFW client. */",
        *defines,
        "",
        "/* ath11k message layouts. */",
        *types,
        *bdf_enum,
        "",
        "/* The matching upstream QMI TLV element descriptions. */",
        *tables,
        "",
        "#endif /* SWEET_WLFW_ATH11K_H */",
        "",
    ]
    content = "\n\n".join(header)
    args.output.write_text(content)

    diff = list(difflib.unified_diff(
        [], content.splitlines(keepends=True),
        fromfile="/dev/null", tofile="bsd/dev/sweet_wlfw_ath11k.h",
        n=3,
    ))
    if len(diff) < 3:
        raise SystemExit("could not create generated XNU patch")
    diff[1] = "+++ b/bsd/dev/sweet_wlfw_ath11k.h\n"
    args.patch.parent.mkdir(parents=True, exist_ok=True)
    args.patch.write_text("--- /dev/null\n" + "".join(diff[1:]))
    print(f"Generated {args.output}: {len(types)} message types, {len(tables)} element tables, {len(defines)} constants")
    print(f"Generated patch {args.patch}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
