#!/usr/bin/env python3
"""
Verify that converter memcalc usage matches runtime usage via cmd_json().
"""
import argparse
import ctypes
import json
import sys
from pathlib import Path


def _load_converter():
    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / "tool"))
    import nested_to_flat as conv  # pylint: disable=import-error
    return conv


def _encode_json(obj):
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def _apply_cmd_json(lib, payload, flags):
    frame = bytes([flags]) + payload
    if len(frame) > 255:
        raise SystemExit(f"payload too large: {len(frame)} bytes")
    buf = (ctypes.c_uint8 * len(frame))(*frame)
    rc = lib.cmd_json(buf, ctypes.c_uint8(len(frame)))
    if rc != 0:
        raise SystemExit(f"cmd_json returned {rc}")


def _get_usage(lib):
    head_used = ctypes.c_uint16(0)
    tail_used = ctypes.c_uint16(0)
    element_count = ctypes.c_uint8(0)
    element_capacity = ctypes.c_uint8(0)
    lib.ui_memcalc_get_usage(ctypes.byref(head_used),
                             ctypes.byref(tail_used),
                             ctypes.byref(element_count),
                             ctypes.byref(element_capacity))
    return {
        "head_used": head_used.value,
        "tail_used": tail_used.value,
        "total_used": head_used.value + tail_used.value,
        "element_count": element_count.value,
        "element_capacity": element_capacity.value,
        "arena_cap": lib.ui_memcalc_get_arena_cap(),
    }


def verify_one(conv, input_path, height):
    with open(input_path, "r", encoding="utf-8") as f:
        doc = json.load(f)
    conv.validate_nested_input(doc)
    elements = conv.flatten(doc)
    elements = conv.shorten(elements)
    conv.validate_and_sanitize(elements, height=height)
    out_elements = [{"t": "h", "n": len(elements)}] + elements

    predicted = conv.check_memory_budget(out_elements)

    lib = conv._load_memcalc_lib()
    lib.cmd_json.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
    lib.cmd_json.restype = ctypes.c_int

    lib.ui_memcalc_reset()
    for idx, obj in enumerate(out_elements):
        flags = 0
        if idx == 0:
            flags |= conv.JSON_FLAG_HEAD
        if idx == (len(out_elements) - 1):
            flags |= conv.JSON_FLAG_COMMIT
        payload = _encode_json(obj)
        _apply_cmd_json(lib, payload, flags)

    actual = _get_usage(lib)

    if predicted["head_used"] != actual["head_used"] or predicted["tail_used"] != actual["tail_used"]:
        raise SystemExit(
            "usage mismatch: predicted(head={0}, tail={1}) actual(head={2}, tail={3})".format(
                predicted["head_used"],
                predicted["tail_used"],
                actual["head_used"],
                actual["tail_used"],
            )
        )
    if predicted["element_count"] != actual["element_count"]:
        raise SystemExit(
            "element count mismatch: predicted={0} actual={1}".format(
                predicted["element_count"],
                actual["element_count"],
            )
        )


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("inputs", nargs="+", help="nested (long-key) JSON files")
    ap.add_argument("--height", type=int, default=32, choices=[32, 64],
                    help="display height for clamping (32 or 64)")
    args = ap.parse_args()

    conv = _load_converter()
    for path in args.inputs:
        verify_one(conv, path, args.height)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
