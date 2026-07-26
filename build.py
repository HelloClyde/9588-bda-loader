# SPDX-License-Identifier: Apache-2.0
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE_DIR = ROOT / "src"
ASSET_DIR = ROOT / "assets"
DIST_DIR = ROOT / "dist"


def run(command: list[str]) -> None:
    print("+", subprocess.list2cmdline(command))
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="构建并校验 9588 BDA Loader。",
    )
    parser.add_argument(
        "--diagnostic",
        action="store_true",
        help="构建会写入 A:\\BDALOAD.LOG 的诊断版",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="输出路径；默认写入 dist/BdaLoader[Debug].bda",
    )
    parser.add_argument(
        "--prefix",
        default=os.environ.get("BDA_TOOLCHAIN_PREFIX"),
        help="MIPS 工具链前缀，例如 mipsel-none-elf-",
    )
    args = parser.parse_args()

    source = SOURCE_DIR / (
        "bda_loader_debug.c" if args.diagnostic else "bda_loader.c"
    )
    output = args.output or DIST_DIR / (
        "BdaLoaderDebug.bda" if args.diagnostic else "BdaLoader.bda"
    )
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)

    command = [
        sys.executable,
        "-m",
        "bda_packer",
        str(source),
        "--title",
        "BDA Loader",
        "--category",
        "9",
        "--icon-png",
        str(ASSET_DIR / "bda-loader-icon.png"),
        "--icon-alpha-threshold",
        "96",
        "-o",
        str(output),
    ]
    if args.prefix:
        command.extend(["--prefix", args.prefix])
    run(command)
    run([sys.executable, "-m", "bda_packer.validate", str(output)])


if __name__ == "__main__":
    main()
