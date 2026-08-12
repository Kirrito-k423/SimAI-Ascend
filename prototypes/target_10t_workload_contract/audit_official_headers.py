#!/usr/bin/env python3
"""PROTOTYPE ONLY: compare every generated baseline tensor with official headers.

This intentionally reads only the safetensors JSON headers via HTTP Range. It
does not download tensor data. Standard proxy environment variables are used.
"""

from __future__ import annotations

import concurrent.futures
import json
import struct
import urllib.request
from pathlib import Path

from contract import Architecture, generate_tensor_manifest


BASE = (
    "https://huggingface.co/deepseek-ai/DeepSeek-V4-Pro/resolve/"
    "45040942eb0d1c4e29fa6b92a6195f110e9e7444/"
    "model-%05d-of-00064.safetensors"
)
MAX_HEADER_BYTES = 256 * 1024 * 1024


def read_range(url: str, start: int, end: int) -> bytes:
    request = urllib.request.Request(url, headers={"Range": f"bytes={start}-{end}"})
    with urllib.request.urlopen(request, timeout=90) as response:
        expected = end - start + 1
        # Never continue if a mirror ignores Range: an unbounded read here
        # could otherwise fetch an entire checkpoint shard.
        payload = response.read(expected + 1)
        if response.status != 206 or len(payload) != expected:
            raise RuntimeError(
                f"range request refused or malformed: status={response.status}, "
                f"expected={expected}, actual={len(payload)}"
            )
        return payload


def read_header(shard: int):
    url = BASE % shard
    header_size = struct.unpack("<Q", read_range(url, 0, 7))[0]
    if header_size <= 0 or header_size > MAX_HEADER_BYTES:
        raise RuntimeError(f"unsafe safetensors header size for shard {shard}: {header_size}")
    header = json.loads(read_range(url, 8, 7 + header_size))
    header.pop("__metadata__", None)
    return header


def main():
    fixture = json.loads(
        Path(__file__).with_name("official_v4_pro_fixture.json").read_text(encoding="utf-8")
    )
    architecture = Architecture.from_dict(fixture["architecture"])
    generated = {tensor.name: tensor for tensor in generate_tensor_manifest(architecture)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        shards = list(executor.map(read_header, range(1, 65)))
    official = {name: metadata for shard in shards for name, metadata in shard.items()}
    missing = sorted(set(official) - set(generated))
    extra = sorted(set(generated) - set(official))
    mismatched = []
    for name in sorted(set(official) & set(generated)):
        expected = generated[name]
        official_dtype = official[name]["dtype"]
        generated_dtype = (
            "I8" if expected.storage_dtype == "PACKED_FP4_I8" else expected.storage_dtype
        )
        official_shape = tuple(official[name]["shape"])
        if official_dtype != generated_dtype or official_shape != expected.storage_shape:
            mismatched.append(
                {
                    "name": name,
                    "official_dtype": official_dtype,
                    "generated_dtype": generated_dtype,
                    "official_shape": official_shape,
                    "generated_shape": expected.storage_shape,
                }
            )
    result = {
        "status": "PASS" if not missing and not extra and not mismatched else "FAIL",
        "official_tensor_count": len(official),
        "generated_tensor_count": len(generated),
        "missing": missing[:20],
        "extra": extra[:20],
        "mismatched": mismatched[:20],
        "tensor_data_downloaded": False,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    raise SystemExit(0 if result["status"] == "PASS" else 1)


if __name__ == "__main__":
    main()
