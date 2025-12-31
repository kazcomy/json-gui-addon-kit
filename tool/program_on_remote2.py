# write program on remote (for gfx_master)
try:
    Import("env")  # type: ignore
except Exception:
    env = None
import os
import sys
import getpass

REMOTE_PATH_DEFAULT = "/tmp/firmware.bin"

def _resolve_remote():
    user = os.environ.get("REMOTE_USER", "") or os.environ.get("USER", "") or getpass.getuser()
    host = os.environ.get("REMOTE_HOST_GFX_MASTER", "") or os.environ.get("REMOTE_HOST", "")
    path = os.environ.get("REMOTE_PATH", "") or REMOTE_PATH_DEFAULT
    if not host:
        print("[program_on_remote2] REMOTE_HOST_GFX_MASTER or REMOTE_HOST is required", file=sys.stderr)
        return None, None, None
    return user, host, path

def remote_upload(source, target, env):
    local_bin = str(source[0])
    user, host, path = _resolve_remote()
    if not user or not host or not path:
        return
    env.Execute(f"scp {local_bin} {user}@{host}:{path}")  # type: ignore
    env.Execute(
        f"ssh {user}@{host} "
        f"'~/.platformio/packages/tool-openocd-riscv-wch/bin/openocd "
        f"-s ~/.platformio/packages/tool-openocd-riscv-wch/scripts "
        f"-f ~/.platformio/packages/tool-openocd-riscv-wch/bin/wch-riscv.cfg "
        f"-c \"gdb_port 3333; tcl_port disabled; telnet_port disabled\" -c init -c halt "
        f"-c \"program {path} verify reset exit\"'"
    )
if env is not None:
    env.Replace(UPLOADCMD=remote_upload)
