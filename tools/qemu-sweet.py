#!/usr/bin/env python3
"""Build and run the IN2013 XNU payload under QEMU virt.

This is a bring-up smoke test only. QEMU virt does not emulate the OnePlus 8
SM8250, its UFS controller, display, touch controller, or power management.
A recorded JUMP marker means only that the shim reached the XNU entry call.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(cmd: list[str], *, cwd: Path = ROOT, env=None) -> None:
    print("+", " ".join(map(str, cmd)), flush=True)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kernel", required=True, type=Path,
                    help="Mach-O kernel.development.instantnoodle from xnu-kernel artifact")
    ap.add_argument("--out-dir", type=Path, default=ROOT / "build/qemu-smoke")
    ap.add_argument("--qemu-bin", default="qemu-system-aarch64")
    ap.add_argument("--timeout", type=int, default=45,
                    help="seconds to run after launch (default: 45)")
    args = ap.parse_args()

    qemu = shutil.which(args.qemu_bin)
    if not qemu:
        ap.error(f"{args.qemu_bin} not found; install qemu-system-arm")
    kernel = args.kernel.resolve()
    if not kernel.is_file():
        ap.error(f"kernel not found: {kernel}")

    out = args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    adt = out / "qemu.adt"
    payload = out / "qemu.payload"
    serial_log = out / "serial.log"
    qemu_log = out / "qemu.log"

    # QEMU-specific ADT: QEMU virt GIC/timer addresses, and no IN2013 UFS node.
    run([sys.executable, str(ROOT / "tools/mkadt-in2013.py"), str(adt), "--qemu"])
    run([sys.executable, str(ROOT / "tools/mkpayload.py"), str(kernel), str(adt),
         str(payload), "--cmdline", "debug=0xc serial=3 cpus=1 keepsyms=1"])

    # Build a separate shim variant. The production IN2013 shim is unchanged.
    env = os.environ.copy()
    env.update({
        "CC": "clang",
        "OBJCOPY": "llvm-objcopy",
        "READELF": "readelf",
        "NM": "nm",
        "TARGET_FLAGS": "--target=aarch64-linux-gnu",
        "LINK_FLAGS": "-fuse-ld=lld",
        "SHIM_DEFINES": "-DSWEET_QEMU",
    })
    run(["bash", "build.sh"], cwd=ROOT / "shim", env=env)
    shim = ROOT / "shim/out/Image"

    cmd = [
        qemu, "-machine", "virt,gic-version=3", "-cpu", "cortex-a76",
        "-accel", "tcg,thread=multi", "-smp", "1", "-m", "8G",
        "-display", "none", "-monitor", "none", "-serial", "stdio",
        "-kernel", str(shim), "-initrd", str(payload), "-no-reboot",
        "-d", "guest_errors,cpu_reset", "-D", str(qemu_log),
    ]
    print("+", " ".join(cmd), flush=True)
    timed_out = False
    try:
        proc = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True,
                              timeout=args.timeout, check=False)
        output = proc.stdout or ""
        print(output, end="", flush=True)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        raw = exc.stdout or ""
        output = raw.decode(errors="replace") if isinstance(raw, bytes) else raw
        print(output, end="", flush=True)

    serial_log.write_text(output, encoding="utf-8", errors="replace")
    if "JUMP:" not in output or "XNU: jumping" not in output:
        print("FAIL: shim did not report the XNU entry handoff; inspect serial.log and qemu.log",
              file=sys.stderr)
        return 1
    if not timed_out:
        print("NOTE: QEMU exited after the handoff marker; inspect qemu.log for the exit cause.",
              file=sys.stderr)
    print("PASS: shim reached the XNU entry call in QEMU.")
    print("LIMIT: this does not prove XNU completed early boot or that IN2013 hardware works.")
    print(f"Logs: {serial_log} ; {qemu_log}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
