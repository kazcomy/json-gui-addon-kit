#!/usr/bin/env python3
"""Super simple remote OpenOCD launcher for PlatformIO.

Do only one thing:
    ssh -L <local>:localhost:<remote> user@host <openocd ...>

No retries, no pkill, no init/halt (GDB will handle reset/halt via monitor).

Environment variables (all optional):
    REMOTE_USER (default: current user)
    REMOTE_HOST (required if --host is not provided)
    REMOTE_GDB_PORT / LOCAL_GDB_PORT (default both 3333)
    OPENOCD_PATH (~/.platformio/.../openocd)
    OPENOCD_SCRIPTS (.../scripts)
    OPENOCD_CFG (.../wch-riscv.cfg)
    SSH_VERBOSE=1  (adds -vvv)
    USE_SUDO=1     (prefix openocd with sudo)

CLI overrides (optional):
    --host <hostname>    (overrides REMOTE_HOST)
    Positional <hostname> is also accepted for convenience

Keep this file minimal to rule out script complexity as a cause of premature exit.
"""
import os, sys, shutil, getpass

def main():
    user = os.environ.get("REMOTE_USER", "") or os.environ.get("USER", "") or getpass.getuser()
    # Allow caller to override host via CLI (--host or positional)
    cli_host = None
    argv_tokens = sys.argv[1:]
    for i, tok in enumerate(argv_tokens):
        if tok == "--host" and i + 1 < len(argv_tokens):
            cli_host = argv_tokens[i + 1]
            break
        if tok.startswith("--host="):
            cli_host = tok.split("=", 1)[1]
            break
    if cli_host is None:
        for tok in argv_tokens:
            if not tok.startswith("-"):
                cli_host = tok
                break
    host = cli_host or os.environ.get("REMOTE_HOST", "")
    if not host:
        print("[remote_debug_server] REMOTE_HOST or --host is required", file=sys.stderr)
        return 1
    remote = f"{user}@{host}"
    rport = int(os.environ.get("REMOTE_GDB_PORT", "3333"))
    lport = int(os.environ.get("LOCAL_GDB_PORT", str(rport)))

    openocd = os.environ.get("OPENOCD_PATH", "~/.platformio/packages/tool-openocd-riscv-wch/bin/openocd")
    scripts = os.environ.get("OPENOCD_SCRIPTS", "~/.platformio/packages/tool-openocd-riscv-wch/scripts")
    cfg     = os.environ.get("OPENOCD_CFG", "~/.platformio/packages/tool-openocd-riscv-wch/bin/wch-riscv.cfg")
    extra   = os.environ.get("OPENOCD_EXTRA_CMDS", "")  # e.g. "; adapter speed 2000"

    sudo_prefix = "sudo " if os.environ.get("USE_SUDO") else ""

    # Minimal command: just set gdb_port and disable unused ports.
    # (No init/halt so OpenOCD should remain idle until GDB connects.)
    openocd_cmd = (
        f"{sudo_prefix}{openocd} -s {scripts} -f {cfg} "
        f"-c \"gdb_port {rport}; tcl_port disabled; telnet_port disabled{extra}\""
    )

    # Optional remote cleanup (opt-in now). Enable with FORCE_KILL=1.
    parts = []
    if os.environ.get("FORCE_KILL"):
        killer = "pkill -f wch-riscv.cfg || true; sleep 0.25;"
        if sudo_prefix:
            killer = "sudo " + killer
        parts.append(killer)
    parts.append(openocd_cmd)
    base_sequence = " ".join(parts)

    # Extract pre-kill (everything except final openocd invocation)
    prekill = base_sequence[:-len(openocd_cmd)].strip() if base_sequence.endswith(openocd_cmd) else ""

    # Trap wrapper: ensure remote openocd is terminated if ssh session ends (default ON).
    if os.environ.get("TRAP_WRAP", "1") == "1":
        # Use bash for trap; run openocd in background then wait.
        wrapped = (
            "bash -lc 'set -e; "
            f"{prekill} {openocd_cmd} & pid=$!; echo [remote_debug_server] remote pid=$pid; "
            "trap \"kill $pid 2>/dev/null||true\" EXIT INT TERM HUP; "
            "wait $pid'"
        )
        remote_cmd = wrapped
    else:
        remote_cmd = base_sequence

    ssh = shutil.which("ssh") or "ssh"
    args = [ssh, "-L", f"{lport}:localhost:{rport}", remote, remote_cmd]
    if os.environ.get("SSH_VERBOSE"):
        args.insert(1, "-vvv")

    print(f"[remote_debug_server] remote={remote} lport={lport} rport={rport}")
    print(f"[remote_debug_server] exec: {' '.join(args)}")
    if os.environ.get("FORCE_KILL"):
        print("[remote_debug_server] remote pre-kill enabled (pattern: wch-riscv.cfg)")
    else:
        print("[remote_debug_server] remote pre-kill disabled (default)")
    print(f"[remote_debug_server] remote openocd: {openocd_cmd}")
    print(f"[remote_debug_server] trap wrapper: {os.environ.get('TRAP_WRAP','1')=='1'}")
    print(f"[remote_debug_server] sent remote cmd: {remote_cmd}")
    try:
        os.execvp(args[0], args)
    except Exception as e:  # pragma: no cover
        print(f"ERROR execvp: {e}", file=sys.stderr)
        return 1
    return 0  # not reached
if __name__ == "__main__":
    raise SystemExit(main())
