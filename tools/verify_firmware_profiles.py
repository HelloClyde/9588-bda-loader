#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""离线检查 BBK 9588/9688 固件的 BDA path-loader 布局。

脚本只读取用户提供的固件，不会修改、复制或打包固件内容。它同时接受底层升级目录
中的原始内核，以及系统数据目录中带 64-byte 包装头的 knl.bin。
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct


LOAD_BASE = 0x80004000


@dataclass(frozen=True)
class KnownProfile:
    name: str
    version: str
    path_entry: int
    return_address: int
    cache_barrier: int
    launch_a2: str


KNOWN_PROFILES = {
    (0x8002E1C0, 0x8002E3EC, 0x80004264): KnownProfile(
        "9588-JZ4720", "V3.30", 0x8002E1C0, 0x8002E3EC, 0x80004264, "s4"
    ),
    (0x80021098, 0x8002128C, 0x80004150): KnownProfile(
        "9588-JZ4730", "V3.30", 0x80021098, 0x8002128C, 0x80004150, "s6"
    ),
    (0x8002C5B0, 0x8002C7A4, 0x80004264): KnownProfile(
        "9588-JZ4740", "V3.30", 0x8002C5B0, 0x8002C7A4, 0x80004264, "s6"
    ),
    (0x80021678, 0x8002186C, 0x80004150): KnownProfile(
        "9688-JZ4730", "V2.32", 0x80021678, 0x8002186C, 0x80004150, "s6"
    ),
    (0x8002E6B8, 0x8002E8AC, 0x80004264): KnownProfile(
        "9688-JZ4740", "V2.32", 0x8002E6B8, 0x8002E8AC, 0x80004264, "s6"
    ),
}


@dataclass(frozen=True)
class Match:
    path_entry: int
    return_address: int
    cache_barrier: int
    launch_a2: str
    native_caller_returns: tuple[int, ...]


def word(data: bytes, offset: int) -> int | None:
    if offset < 0 or offset + 4 > len(data):
        return None
    return struct.unpack_from("<I", data, offset)[0]


def word_at_va(data: bytes, address: int) -> int | None:
    return word(data, address - LOAD_BASE)


def decode_direct_jump_target(call_site: int, instruction: int) -> int:
    return ((call_site + 4) & 0xF0000000) | ((instruction & 0x03FFFFFF) << 2)


def decode_lui_addiu_address(lui: int, addiu: int) -> int:
    immediate = addiu & 0xFFFF
    if immediate & 0x8000:
        immediate -= 0x10000
    return (((lui & 0xFFFF) << 16) + immediate) & 0xFFFFFFFF


def validate_cache_barrier(data: bytes, address: int) -> bool:
    expected = (
        0x3C048000,
        0x3C038000,
        0x34843FFF,
        0xBC610000,
        0x24630020,
        0x0083102B,
        0x1040FFFC,
        0x00000000,
    )
    return all(
        word_at_va(data, address + index * 4) == instruction
        for index, instruction in enumerate(expected)
    )


def validate_tail(data: bytes, return_address: int) -> int | None:
    checks = {
        -0x40: 0x3C0481C0,
        -0x28: 0x34840020,
        -0x24: 0x3C1281C0,
        -0x14: 0x36520020,
        -0x08: 0x0240F809,
        0x00: 0x1660001F,
    }
    if any(
        word_at_va(data, return_address + delta) != expected
        for delta, expected in checks.items()
    ):
        return None
    call_site = return_address - 0x10
    call = word_at_va(data, call_site)
    if call is None or call >> 26 != 3:
        return None
    target = decode_direct_jump_target(call_site, call)
    return target if validate_cache_barrier(data, target) else None


def matches_common_prologue(data: bytes, entry: int) -> bool:
    checks = {
        0x00: 0x27BDFF10,
        0x04: 0xAFB000D0,
        0x08: 0x00808021,
        0x10: 0xAFBF00EC,
        0x1C: 0x00C0B021,
        0x2C: 0x00A09821,
        0x44: 0x02002021,
    }
    return all(
        word_at_va(data, entry + delta) == expected
        for delta, expected in checks.items()
    )


def matches_4720_prologue(data: bytes, entry: int) -> bool:
    checks = {
        0x00: 0x27BDFF10,
        0x04: 0xAFB200D8,
        0x08: 0x00809021,
        0x10: 0xAFBF00EC,
        0x24: 0x00C0A021,
        0x28: 0x00A09821,
        0x44: 0x02402021,
    }
    return all(
        word_at_va(data, entry + delta) == expected
        for delta, expected in checks.items()
    )


def decode_native_prelaunch(
    data: bytes, caller_return: int
) -> tuple[int, int, int] | None:
    pre_path_call = word_at_va(data, caller_return - 0x24)
    trace_lui = word_at_va(data, caller_return - 0x1C)
    pre_trace_call = word_at_va(data, caller_return - 0x18)
    trace_addiu = word_at_va(data, caller_return - 0x14)
    if (
        pre_path_call is None
        or pre_path_call >> 26 != 3
        or trace_lui is None
        or trace_lui & 0xFFFF0000 != 0x3C040000
        or pre_trace_call is None
        or pre_trace_call >> 26 != 3
        or trace_addiu is None
        or trace_addiu & 0xFFFF0000 != 0x24840000
    ):
        return None
    pre_path_helper = decode_direct_jump_target(
        caller_return - 0x24, pre_path_call
    )
    pre_trace_helper = decode_direct_jump_target(
        caller_return - 0x18, pre_trace_call
    )
    pre_trace_text = decode_lui_addiu_address(trace_lui, trace_addiu)
    if (
        not LOAD_BASE <= pre_path_helper < 0x80500000
        or not LOAD_BASE <= pre_trace_helper < 0x80500000
        or not LOAD_BASE <= pre_trace_text < 0x80500000
    ):
        return None
    return pre_path_helper, pre_trace_helper, pre_trace_text


def find_native_caller_returns(data: bytes, path_entry: int) -> tuple[int, ...]:
    expected_jal = 0x0C000000 | ((path_entry >> 2) & 0x03FFFFFF)
    returns: list[int] = []
    for offset in range(0, max(0, len(data) - 4), 4):
        call_site = LOAD_BASE + offset
        if word(data, offset) != expected_jal:
            continue
        caller_return = call_site + 8
        checks = {
            -0x20: 0x27A40020,  # first helper receives path at sp+0x20
            -0x10: 0x02602821,  # move a1,s3
            -0x0C: 0x02803021,  # move a2,s4
            -0x04: 0x27A40020,  # addiu a0,sp,0x20
            0x10: 0x27A40020,   # post-BDA helper uses the same path
            -0x220: 0x27BDFEA0, # caller owns a 0x160-byte frame
        }
        if not all(
            word_at_va(data, caller_return + delta) == expected
            for delta, expected in checks.items()
        ):
            continue
        if decode_native_prelaunch(data, caller_return) is None:
            continue
        returns.append(caller_return)
    return tuple(returns)


def scan_image(data: bytes) -> list[Match]:
    matches: list[Match] = []
    for offset in range(0, max(0, len(data) - 0x230), 4):
        entry = LOAD_BASE + offset
        first = word(data, offset)
        if first != 0x27BDFF10:
            continue
        if matches_common_prologue(data, entry):
            return_address = entry + 0x1F4
            cache_barrier = validate_tail(data, return_address)
            if cache_barrier is not None:
                matches.append(
                    Match(
                        entry,
                        return_address,
                        cache_barrier,
                        "s6",
                        find_native_caller_returns(data, entry),
                    )
                )
        if matches_4720_prologue(data, entry):
            return_address = entry + 0x22C
            cache_barrier = validate_tail(data, return_address)
            if cache_barrier is not None:
                matches.append(
                    Match(
                        entry,
                        return_address,
                        cache_barrier,
                        "s4",
                        find_native_caller_returns(data, entry),
                    )
                )
    return matches


def inspect_firmware(path: Path) -> bool:
    payload = path.read_bytes()
    candidates = [(0, payload)]
    if len(payload) > 64:
        candidates.append((64, payload[64:]))

    found: list[tuple[int, bytes, Match]] = []
    for header_size, image in candidates:
        for match in scan_image(image):
            found.append((header_size, image, match))

    known_found = [
        item
        for item in found
        if (
            item[2].path_entry,
            item[2].return_address,
            item[2].cache_barrier,
        )
        in KNOWN_PROFILES
    ]
    selected = known_found or found
    if not selected:
        print(f"[不支持] {path}")
        print("  未找到通过完整 prologue、load/jalr tail 和 cache barrier 校验的 profile")
        return False

    print(f"[通过] {path}")
    print(f"  文件 SHA256: {hashlib.sha256(payload).hexdigest()}")
    for header_size, image, match in selected:
        known = KNOWN_PROFILES.get(
            (match.path_entry, match.return_address, match.cache_barrier)
        )
        print(f"  包装头: {header_size} bytes")
        print(f"  内核 SHA256: {hashlib.sha256(image).hexdigest()}")
        print(f"  profile: {known.name if known else '兼容布局（未知固件）'}")
        if known:
            print(f"  固件版本: {known.version}")
        print(f"  path entry: 0x{match.path_entry:08x}")
        print(f"  BDA return: 0x{match.return_address:08x}")
        print(f"  cache barrier: 0x{match.cache_barrier:08x}")
        print(f"  第三参数: {match.launch_a2}")
        print(
            "  原生 tail caller: "
            + (
                ", ".join(
                    f"0x{return_address:08x}"
                    for return_address in match.native_caller_returns
                )
                if match.native_caller_returns
                else "未找到"
            )
        )
        for caller_return in match.native_caller_returns:
            prelaunch = decode_native_prelaunch(image, caller_return)
            if prelaunch is not None:
                print(
                    "  原生 prelaunch: "
                    f"path=0x{prelaunch[0]:08x}, "
                    f"trace=0x{prelaunch[1]:08x}, "
                    f"text=0x{prelaunch[2]:08x}"
                )
        if not match.native_caller_returns:
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="验证 BBK 9588/9688 固件的 BDA path-loader profile"
    )
    parser.add_argument("firmware", type=Path, nargs="+", help="待检查的内核文件")
    args = parser.parse_args()
    return 0 if all(inspect_firmware(path) for path in args.firmware) else 1


if __name__ == "__main__":
    raise SystemExit(main())
