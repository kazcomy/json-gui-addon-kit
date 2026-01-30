# write program on remote (for gfx_slave and gfx_demo_on_slave)
# Supports both legacy PlatformIO and WCH toolchain
import os
import sys
import getpass

# WCH Toolchain paths
WCH_TOOLCHAIN_DEFAULT = "/opt/wch-toolchain"
OPENOCD_BIN_DEFAULT = "{}/OpenOCD/bin/openocd"
OPENOCD_SCRIPTS_DEFAULT = "{}/OpenOCD/scripts"
OPENOCD_CFG_DEFAULT = "wch-riscv.cfg"

REMOTE_PATH_DEFAULT = "/tmp/firmware.bin"

# Try to import PlatformIO env if available
try:
    Import("env")  # type: ignore # Provided by PlatformIO at runtime
except Exception:
    env = None  # fallback for standalone usage

def _resolve_remote():
    user = os.environ.get("REMOTE_USER", "") or os.environ.get("USER", "") or getpass.getuser()
    host = os.environ.get("REMOTE_HOST_GFX_SLAVE", "") or os.environ.get("REMOTE_HOST", "")
    path = os.environ.get("REMOTE_PATH", "") or REMOTE_PATH_DEFAULT
    if not host:
        print("[program_on_remote] REMOTE_HOST_GFX_SLAVE or REMOTE_HOST is required", file=sys.stderr)
        return None, None, None
    return user, host, path

def _get_toolchain_paths():
    """Get OpenOCD paths from environment or use WCH toolchain defaults"""
    wch_toolchain = os.environ.get("WCH_TOOLCHAIN", WCH_TOOLCHAIN_DEFAULT)
    openocd_bin = os.environ.get("OPENOCD_BIN", OPENOCD_BIN_DEFAULT.format(wch_toolchain))
    openocd_scripts = os.environ.get("OPENOCD_SCRIPTS", OPENOCD_SCRIPTS_DEFAULT.format(wch_toolchain))
    openocd_cfg = os.environ.get("OPENOCD_CFG", OPENOCD_CFG_DEFAULT)
    return openocd_bin, openocd_scripts, openocd_cfg

def remote_upload(source, target, env):
    local_bin = str(source[0])
    user, host, path = _resolve_remote()
    if not user or not host or not path:
        return

    openocd_bin, openocd_scripts, openocd_cfg = _get_toolchain_paths()

    env.Execute(f"scp {local_bin} {user}@{host}:{path}")  # type: ignore
    env.Execute(
        f"ssh {user}@{host} "
        f"'{openocd_bin} "
        f"-s {openocd_scripts} "
        f"-f {openocd_scripts}/{openocd_cfg} "
        f"-c \"gdb_port 3333; tcl_port disabled; telnet_port disabled\" -c init -c halt "
        f"-c \"program {path} verify reset exit\"'"
    )

if env is not None:
    env.Replace(UPLOADCMD=remote_upload)
