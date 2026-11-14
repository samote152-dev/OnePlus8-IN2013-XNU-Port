#!/usr/bin/env python3
"""Verify the final patched SWEET UFS init path is safe for IN2013."""
from __future__ import annotations

import re
import sys
from pathlib import Path

REQUIRED_GUARDED = (
    "sweet_cpufreq_init",
    "sweet_gpu_init",
    "sweet_keys_init",
    "sweet_touch_init",
    "sweet_usb_probe",
    "sweet_rpmh_ddr_top",
    "sweet_wifi_init",
    "sweet_bt_init",
    "apollo_audio_init",
    "apollo_adsp_init",
    "apollo_haptic_init",
    "apollo_backlight_init",
)
REQUIRED_UNGUARDED = ("sweet_log_start", "sweet_sample_start")
MARKER = (
    "INSTANTNOODLE: inherited SWEET/APOLLO CPUFREQ, GPU, input, USB, RPMh, "
    "WLAN/BT, audio, haptic and backlight helpers disabled pending board validation"
)


def extract_function(source: str) -> str:
    match = re.search(
        r"(?m)^static\s+int\s*\n\s*sweet_ufs_init\s*\(\s*void\s*\)\s*\n\s*\{",
        source,
    )
    if not match:
        raise ValueError("could not locate static int sweet_ufs_init(void)")

    start = match.end()
    depth = 1
    i = start
    state = "code"
    while i < len(source):
        c = source[i]
        n = source[i + 1] if i + 1 < len(source) else ""
        if state == "code":
            if c == "/" and n == "*":
                state = "block_comment"
                i += 2
                continue
            if c == "/" and n == "/":
                state = "line_comment"
                i += 2
                continue
            if c == '"':
                state = "string"
            elif c == "'":
                state = "char"
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return source[start:i]
        elif state == "block_comment":
            if c == "*" and n == "/":
                state = "code"
                i += 2
                continue
        elif state == "line_comment":
            if c == "\n":
                state = "code"
        elif state in ("string", "char"):
            if c == "\\":
                i += 2
                continue
            if (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "code"
        i += 1
    raise ValueError("unterminated sweet_ufs_init function")


def verify(body: str) -> list[str]:
    errors: list[str] = []
    if MARKER not in body:
        errors.append("INSTANTNOODLE disabled-helper marker is missing from sweet_ufs_init")

    guard_stack: list[tuple[bool, bool]] = []
    target_guards = 0
    seen_guarded: dict[str, int] = {name: 0 for name in REQUIRED_GUARDED}
    seen_unguarded: dict[str, int] = {name: 0 for name in REQUIRED_UNGUARDED}

    for lineno, line in enumerate(body.splitlines(), 1):
        stripped = line.strip()
        if re.match(r"#\s*if\s+!defined\s*\(\s*INSTANTNOODLE\s*\)", stripped):
            guard_stack.append((True, True))
            target_guards += 1
            continue
        if re.match(r"#\s*if(n?def)?\b", stripped):
            guard_stack.append((False, True))
            continue
        if re.match(r"#\s*else\b", stripped):
            if guard_stack and guard_stack[-1][0]:
                guard_stack[-1] = (True, False)
            continue
        if re.match(r"#\s*elif\b", stripped):
            if guard_stack and guard_stack[-1][0]:
                guard_stack[-1] = (True, False)
            continue
        if re.match(r"#\s*endif\b", stripped):
            if guard_stack:
                guard_stack.pop()
            continue

        code = re.sub(r"/\*.*?\*/|//.*", "", line)
        in_guard = any(is_target and active for is_target, active in guard_stack)
        for name in REQUIRED_GUARDED:
            if re.search(rf"\b{re.escape(name)}\s*\(", code):
                seen_guarded[name] += 1
                if not in_guard:
                    errors.append(f"{name} call at sweet_ufs_init line {lineno} is outside #if !defined(INSTANTNOODLE)")
        for name in REQUIRED_UNGUARDED:
            if re.search(rf"\b{re.escape(name)}\s*\(", code):
                seen_unguarded[name] += 1
                if in_guard:
                    errors.append(f"{name} must remain outside INSTANTNOODLE helper guards")

    if target_guards != 2:
        errors.append(f"expected 2 explicit #if !defined(INSTANTNOODLE) blocks, found {target_guards}")
    for name, count in seen_guarded.items():
        if count != 1:
            errors.append(f"expected exactly one {name} call in sweet_ufs_init, found {count}")
    for name, count in seen_unguarded.items():
        if count != 1:
            errors.append(f"expected exactly one {name} call in sweet_ufs_init, found {count}")
    if '"ufsroot0"' not in body:
        errors.append("IN2013 UFS root node creation is missing")
    return errors


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} PATH_TO_PATCHED_SWEET_UFS_C", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    try:
        body = extract_function(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    errors = verify(body)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("INSTANTNOODLE guard verified: 12 SWEET/APOLLO helpers excluded; UFS root, console log and sampler retained")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
