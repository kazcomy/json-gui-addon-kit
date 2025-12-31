#!/usr/bin/env python3
"""
Nested -> Flat UI JSON converter (outputs short keys/tokens).

Input expectation (nested, long-key format):
{
    "elements": [
        { "type": "screen", "elements": [
                { "type": "text", "x":0, "y":0, "text":"Hello" },
                { "type": "list", "x":0, "y":8, "rows":3, "elements":[ ... ] }
        ]},
        { "type": "screen", ... }
    ]
}

Conversion:
- Produces flat object list using the short spec enforced by the slave:
    { "elements": [ {"t":"h","n":5}, {"t":"s"}, {"t":"t","x":0,"y":0,"tx":"Hello","p":0}, ... ] }
- Parent index (p) is the zero-based index of the parent element in the output list.
- Key/token shortening is unconditional:
        Keys:  type->t, parent->p, text->tx, capacity->c, rows->r, value->v, overlay->ov
    Types: screen->s, list->l, text->t, barrel->b, trigger->i
- Short keys/tokens are not accepted in input.

Notes:
- TEXT capacity `c` is 0..20. Device allocates `c+1` bytes for the text payload including NUL.
- Input must be nested (elements arrays); flat input is rejected.
- A header element `{"t":"h","n":<count>}` is always emitted to reserve per-element storage.
- Memory usage is validated by executing the real slave parser via a host-built memcalc library.

Usage:
    python nested_to_flat.py input.json > output.json

Exit codes: 0 success, 1 error.
"""
import sys, json, argparse, os, subprocess, ctypes
from pathlib import Path

SHORT_MAP = {
    # long -> short (output)
    'type':'t',
    'parent':'p',
    'text':'tx',
    'rows':'r',
    'capacity':'c',        # TEXT capacity
    'value':'v',
    'overlay':'ov',
}

ALLOWED_COPY_KEYS = (
    # type
    'type',
    # position
    'x','y',
    # element-specific
    'rows','text','capacity','value','overlay',
)

TYPE_SHORT = {
    # long type name -> short token (single-char except 'po')
    'screen':'s',
    'text':'t',
    'list':'l',
    'trigger':'i',     # remapped
    'barrel':'b',
}

ALLOWED_LONG_KEYS = {
    'type','elements',
    'x','y','rows','text','capacity','value','overlay',
}

DISALLOWED_SHORT_KEYS = {
    't','p','par','v','val','tx','r','c','cap','ov','e',
}

SHORT_TYPE_TOKENS = set(TYPE_SHORT.values())
LEGACY_TYPE_TOKENS = {'te','li','ba','tr'}

MAX_ELEMENT_ID = 255
JSON_FLAG_HEAD = 0x01
JSON_FLAG_COMMIT = 0x02

_MEMCALC_LIB = None

def _project_root():
    return Path(__file__).resolve().parents[1]

def _memcalc_lib_path():
    if sys.platform == "win32":
        ext = ".dll"
    elif sys.platform == "darwin":
        ext = ".dylib"
    else:
        ext = ".so"
    return _project_root() / "tool" / f"ui_memcalc{ext}"

def _memcalc_sources(root):
    return [
        root / "tool" / "ui_memcalc.c",
        root / "tool" / "memcalc_stubs.c",
        root / "src" / "slave" / "ui_protocol.c",
        root / "src" / "slave" / "ui_runtime.c",
        root / "src" / "slave" / "ui_focus.c",
        root / "src" / "slave" / "ui_input.c",
        root / "src" / "slave" / "ui_numeric.c",
        root / "src" / "slave" / "ui_tree.c",
        root / "src" / "common" / "cobs.c",
    ]

def _memcalc_headers(root):
    headers = []
    headers.extend((root / "include" / "slave").glob("*.h"))
    headers.extend((root / "include" / "common").glob("*.h"))
    headers.extend((root / "tool" / "hal_stub").glob("*.h"))
    return headers

def _memcalc_needs_rebuild(lib_path, sources, headers):
    if not lib_path.exists():
        return True
    lib_mtime = lib_path.stat().st_mtime
    for dep in sources + headers:
        if not dep.exists():
            raise SystemExit(f"[converter] missing dependency for memcalc: {dep}")
        if dep.stat().st_mtime > lib_mtime:
            return True
    return False

def _build_memcalc_lib():
    root = _project_root()
    lib_path = _memcalc_lib_path()
    sources = _memcalc_sources(root)
    headers = _memcalc_headers(root)
    if not _memcalc_needs_rebuild(lib_path, sources, headers):
        return lib_path
    cc = os.environ.get("CC", "cc")
    if sys.platform == "darwin":
        shared_flags = ["-dynamiclib"]
    else:
        shared_flags = ["-shared"]
    cmd = [
        cc,
        "-std=c99",
        "-O2",
        "-fPIC",
        "-DUNIT_TEST=1",
        "-DUI_MEMCALC=1",
        "-I", str(root / "include" / "common"),
        "-I", str(root / "include" / "slave"),
        "-I", str(root / "tool" / "hal_stub"),
        *shared_flags,
        "-o", str(lib_path),
        *[str(s) for s in sources],
    ]
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except subprocess.CalledProcessError as e:
        msg = e.stderr.strip() if e.stderr else str(e)
        raise SystemExit(f"[converter] memcalc build failed: {msg}")
    return lib_path

def _load_memcalc_lib():
    global _MEMCALC_LIB
    if _MEMCALC_LIB is not None:
        return _MEMCALC_LIB
    lib_path = _build_memcalc_lib()
    lib = ctypes.CDLL(str(lib_path))
    lib.ui_memcalc_reset.argtypes = []
    lib.ui_memcalc_reset.restype = None
    lib.ui_memcalc_apply_object.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_uint8]
    lib.ui_memcalc_apply_object.restype = ctypes.c_int
    lib.ui_memcalc_get_usage.argtypes = [
        ctypes.POINTER(ctypes.c_uint16),
        ctypes.POINTER(ctypes.c_uint16),
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.ui_memcalc_get_usage.restype = None
    lib.ui_memcalc_get_arena_cap.argtypes = []
    lib.ui_memcalc_get_arena_cap.restype = ctypes.c_uint16
    _MEMCALC_LIB = lib
    return lib

def validate_nested_input(doc):
    errs = []
    if not isinstance(doc, dict):
        errs.append('root: must be a JSON object with "elements"')
    elements = doc.get('elements') if isinstance(doc, dict) else None
    if not isinstance(elements, list):
        errs.append('root.elements: must be an array')
        elements = []
    has_nested = False

    def visit_list(lst, path, is_root_list):
        nonlocal has_nested
        if not isinstance(lst, list):
            errs.append(f'{path}: must be an array')
            return
        for i, obj in enumerate(lst):
            p = f'{path}[{i}]'
            if not isinstance(obj, dict):
                errs.append(f'{p}: must be an object')
                continue
            for k in obj.keys():
                if k in DISALLOWED_SHORT_KEYS:
                    errs.append(f'{p}: short key "{k}" is not allowed')
                elif k not in ALLOWED_LONG_KEYS:
                    errs.append(f'{p}: unknown key "{k}"')
            t = obj.get('type')
            if not isinstance(t, str):
                errs.append(f'{p}: missing "type"')
            else:
                if t in SHORT_TYPE_TOKENS or t in LEGACY_TYPE_TOKENS:
                    errs.append(f'{p}: short type token "{t}" is not allowed')
                elif t not in TYPE_SHORT:
                    errs.append(f'{p}: unsupported type "{t}"')
                elif is_root_list and t != 'screen':
                    errs.append(f'{p}: root elements must be "screen"')
            if 'overlay' in obj and t != 'screen':
                errs.append(f'{p}: overlay is only valid on screens')
            if 'overlay' in obj and not is_root_list:
                errs.append(f'{p}: overlay is only valid on root screens')
            if 'elements' in obj:
                if not isinstance(obj.get('elements'), list):
                    errs.append(f'{p}.elements: must be an array')
                else:
                    has_nested = True
                    visit_list(obj.get('elements', []), f'{p}.elements', False)
    visit_list(elements, 'elements', True)
    if not has_nested:
        errs.append('input is flat; nested "elements" arrays are required')
    if errs:
        for msg in errs:
            print(f'[converter] {msg}', file=sys.stderr)
        raise SystemExit(1)

def flatten(doc):
    out = []
    def add(obj):
        out.append(obj); return len(out)-1
    def visit(obj, parent_idx=None):
        if not isinstance(obj, dict): return
        otype = obj.get('type')
        if not otype: return
        flat = {}
        for k in ALLOWED_COPY_KEYS:
            if k in obj:
                flat[k] = obj[k]
        if parent_idx is not None:
            flat['parent'] = parent_idx
        idx = add(flat)
        children = obj.get('elements') or []
        if isinstance(children, list):
            for ch in children:
                visit(ch, idx)
    root_elements = doc.get('elements', []) if isinstance(doc, dict) else []
    for item in root_elements:
        visit(item, None)
    return out

def shorten(elements):
    res = []
    for e in elements:
        ne = {}
        for k, v in e.items():
            if k in SHORT_MAP:
                ne[SHORT_MAP[k]] = v
            else:
                # pass-through for unknowns and already-short keys (x,y, etc.)
                ne[k] = v
        # Normalize any still-long keys that weren't in SHORT_MAP pass
        for lk, sk in SHORT_MAP.items():
            if lk in ne:
                ne[sk] = ne.pop(lk)
        # Map long type names to short tokens, and ensure key is 't'
        if 'type' in ne and 't' not in ne:
            ne['t'] = ne.pop('type')
        if 't' in ne and isinstance(ne['t'], str):
            tval = ne['t']
            ne['t'] = TYPE_SHORT.get(tval, tval)  # leave as-is if already short
        # Ensure TEXT capacity 'c' exists and is clamped (type-specific meaning)
        t = ne.get('t')
        if t == 't':
            tx = ne.get('tx', '')
            if not isinstance(tx, str):
                tx = ''
            # If input provided long-form capacity/cap, it already mapped to 'c'
            if 'c' not in ne:
                cap = len(tx)
            else:
                # trust provided value but clamp
                try:
                    cap = int(ne.get('c', 0))
                except Exception:
                    cap = 0
            if cap < 0:
                cap = 0
            if cap > 20:
                cap = 20
            ne['c'] = cap
        res.append(ne)
    return res

# --------------------------- Validation helpers ---------------------------

VALID_TYPES = { 's','t','l','b','i' }

def _as_int(v, default=0):
    try:
        if isinstance(v, bool):
            return int(v)
        if isinstance(v, (int,)):
            return int(v)
        if isinstance(v, str):
            return int(v.strip())
    except Exception:
        pass
    return default

def _clamp(val, lo, hi):
    if val < lo: return lo
    if val > hi: return hi
    return val

def validate_and_sanitize(elements, height=32):
    """Validate element objects and coerce minor issues; raise on fatal."""
    errs = []
    # Track parent existence online (by index); later passes assume valid order
    seen_overlay_root = False
    for idx, e in enumerate(elements):
        if not isinstance(e, dict):
            errs.append(f'e[{idx}]: not an object')
            continue
        t = e.get('t')
        if not isinstance(t, str):
            errs.append(f'e[{idx}]: missing type token')
            continue
        # map long type just in case
        t2 = TYPE_SHORT.get(t, t)
        if t2 not in VALID_TYPES:
            errs.append(f'e[{idx}]: invalid type token {t2!r}')
            continue
        e['t'] = t2

        # Common coordinates: x,y default to 0..127/0..(height-1)
        if 'x' in e:
            e['x'] = _clamp(_as_int(e['x'], 0), 0, 127)
        if 'y' in e:
            e['y'] = _clamp(_as_int(e['y'], 0), 0, max(0, int(height) - 1))

        # Parent index p must be valid index < idx
        if 'p' in e:
            p = _as_int(e['p'], -1)
            if p < 0 or p >= idx:
                errs.append(f'e[{idx}]: invalid parent index p={e["p"]}')

        # Type-specific
        if t2 == 't':
            tx = e.get('tx', '')
            if not isinstance(tx, str):
                errs.append(f'e[{idx}]: text.tx must be string')
                tx = ''
            if len(tx) > 20:
                tx = tx[:20]
            # capacity 'c' (0..20; 0 means auto=tx length)
            cap = _as_int(e.get('c', len(tx)), len(tx))
            cap = _clamp(cap, 0, 20)
            e['c'] = cap
            eff_cap = cap if cap > 0 else len(tx)
            if len(tx) > eff_cap:
                tx = tx[:eff_cap]
            e['tx'] = tx
        elif t2 == 'l':
            if 'r' in e:
                e['r'] = _clamp(_as_int(e['r'], 4), 1, 6)
        elif t2 == 'b':
            e['v'] = _clamp(_as_int(e.get('v', 0), 0), 0, 32767)
        elif t2 == 's':
            is_root = 'p' not in e
            ov_present = 'ov' in e
            ov = _clamp(_as_int(e.get('ov', 0), 0), 0, 1) if ov_present else 0
            if ov_present and not is_root:
                errs.append(f'e[{idx}]: ov is only valid on root screens')
            if is_root:
                if seen_overlay_root and ov == 0:
                    errs.append(f'e[{idx}]: base screens must appear before overlay screens')
                if ov != 0:
                    seen_overlay_root = True
            if ov_present:
                e['ov'] = ov
        # 'i' has no extra constraints here
        if 'p' not in e and t2 != 's':
            errs.append(f'e[{idx}]: root elements must be screens')
    if errs:
        for msg in errs:
            print(f'[converter] {msg}', file=sys.stderr)
        raise SystemExit(1)

def memcalc_usage(out_elements):
    errs = []
    if len(out_elements) <= 1:
        errs.append('no elements to evaluate')
    header = out_elements[0] if out_elements else None
    if not isinstance(header, dict) or header.get('t') != 'h':
        errs.append('first element must be header (t=h)')
        header = {}
    element_count = len(out_elements) - 1
    header_n = header.get('n')
    if not isinstance(header_n, int) or header_n != element_count:
        errs.append(f'header n={header_n} does not match element count {element_count}')
    if element_count > MAX_ELEMENT_ID:
        errs.append(f'element count {element_count} exceeds {MAX_ELEMENT_ID}')
    if errs:
        for msg in errs:
            print(f'[converter] {msg}', file=sys.stderr)
        raise SystemExit(1)

    lib = _load_memcalc_lib()
    lib.ui_memcalc_reset()
    for idx, e in enumerate(out_elements):
        payload = json.dumps(e, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        if len(payload) > 255:
            print(f'[converter] e[{idx}]: JSON object exceeds 255 bytes', file=sys.stderr)
            raise SystemExit(1)
        flags = 0
        if idx == 0:
            flags |= JSON_FLAG_HEAD
        if idx == (len(out_elements) - 1):
            flags |= JSON_FLAG_COMMIT
        rc = lib.ui_memcalc_apply_object(payload, len(payload), flags)
        if rc != 0:
            print(f'[converter] e[{idx}]: parser returned {rc}', file=sys.stderr)
            raise SystemExit(1)

    head_used = ctypes.c_uint16(0)
    tail_used = ctypes.c_uint16(0)
    element_count = ctypes.c_uint8(0)
    element_capacity = ctypes.c_uint8(0)
    lib.ui_memcalc_get_usage(ctypes.byref(head_used),
                             ctypes.byref(tail_used),
                             ctypes.byref(element_count),
                             ctypes.byref(element_capacity))
    arena_cap = lib.ui_memcalc_get_arena_cap()
    total_used = head_used.value + tail_used.value
    return {
        'head_used': head_used.value,
        'tail_used': tail_used.value,
        'total_used': total_used,
        'element_count': element_count.value,
        'element_capacity': element_capacity.value,
        'arena_cap': arena_cap,
    }

def check_memory_budget(out_elements):
    usage = memcalc_usage(out_elements)
    if usage['total_used'] > usage['arena_cap']:
        print('[converter] arena overflow: head={0} tail={1} cap={2}'.format(
            usage['head_used'], usage['tail_used'], usage['arena_cap']), file=sys.stderr)
        raise SystemExit(1)
    return usage

def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument('input', help='nested (long-key) JSON file')
    ap.add_argument('--height', type=int, default=32, choices=[32,64], help='display height for clamping (32 or 64)')
    # Header is required by the slave; no legacy mode.
    args = ap.parse_args()
    try:
        with open(args.input,'r',encoding='utf-8') as f:
            doc = json.load(f)
    except Exception as e:
        print(f'Error reading/parsing input: {e}', file=sys.stderr); sys.exit(1)
    validate_nested_input(doc)
    elements = flatten(doc)
    elements = shorten(elements)
    # Validate and sanitize based on device constraints and pruned checks
    validate_and_sanitize(elements, height=args.height)
    out_elements = [{'t': 'h', 'n': len(elements)}] + elements
    check_memory_budget(out_elements)
    json.dump({ 'elements': out_elements }, sys.stdout, ensure_ascii=False, separators=(',',':'))
    print()

if __name__ == '__main__':
    main()
