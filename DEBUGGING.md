# Debugging Guide

This document describes how to debug and flash firmware for the CH32V003 JSON GUI Addon Kit.

## Table of Contents

- [Overview](#overview)
- [Local Debugging (Direct OpenOCD)](#local-debugging-direct-openocd)
- [Remote Debugging (debug-probe-hub)](#remote-debugging-debug-probe-hub)
- [Flashing Firmware](#flashing-firmware)
- [Troubleshooting](#troubleshooting)

## Overview

This project supports two debugging methods:

1. **Local OpenOCD**: Direct connection to WCH-Link probe connected to your machine
2. **Remote debug-probe-hub**: Connection to a centralized debug probe server over the network

Both methods use the same GDB client and VSCode debug configurations.

## Local Debugging (Direct OpenOCD)

### Prerequisites

- WCH toolchain installed at `/opt/wch-toolchain` (run `./tool/setup_wch_toolchain.sh`)
- WCH-Link probe connected via USB
- Firmware built: `make slave` or `make master`

### Command Line

```bash
# Terminal 1: Start OpenOCD server
make debug-slave-server

# Terminal 2: Connect GDB
/opt/wch-toolchain/toolchain/RISC-V-Embedded-GCC12/bin/riscv-wch-elf-gdb build/gfx_slave.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) continue
```

### VSCode

1. Build firmware: `Ctrl+Shift+B` or `make slave`
2. Open Run and Debug panel: `Ctrl+Shift+D`
3. Select debug configuration:
   - **Debug Slave (GDB + OpenOCD)**
   - **Debug Master (GDB + OpenOCD)**
4. Press F5 to start debugging

VSCode will automatically:
- Start the OpenOCD server in the background
- Connect GDB to the target
- Load the firmware
- Stop OpenOCD when debugging ends

### Manual Flash

```bash
make flash-slave   # Flash slave firmware
make flash-master  # Flash master firmware
```

## Remote Debugging (debug-probe-hub)

debug-probe-hub is a centralized debug probe server that allows multiple developers to share debug probes over the network. This is useful for:

- Remote development (e.g., hardware in the office, developer at home)
- Sharing expensive debug probes across teams
- Avoiding USB/IP forwarding complications with VMs/containers

### Prerequisites

1. **debug-probe-hub server running** (see [debug-probe-hub setup](https://github.com/kazcomy/debug-probe-hub))
2. **SSH access** to the server
3. **Python dependencies installed**:
   ```bash
   # Using apt (recommended for Debian/Ubuntu)
   sudo apt-get install python3-requests

   # Or using pip (if available)
   pip install -r tool/debug-probe-hub-client/requirements.txt
   ```

### Configuration

Copy `.env.example` to `.env` and update:

```bash
cp .env.example .env
```

Edit `.env`:

```bash
# debug-probe-hub server URL
DEBUG_PROBE_HUB_URL=http://192.168.1.100:8080

# SSH access
DEBUG_PROBE_HUB_SSH_HOST=192.168.1.100
DEBUG_PROBE_HUB_SSH_USER=pi

# Target and probe
DEBUG_PROBE_HUB_TARGET=ch32v003
DEBUG_PROBE_HUB_PROBE=4
```

Load environment variables:

```bash
source .env
# or
export $(cat .env | xargs)
```

### Find Available Probes

```bash
# List all probes
./tool/debug-probe-hub-client/client.py list-probes

# Search for WCH-Link probes
./tool/debug-probe-hub-client/client.py search --interface wch-link

# Check server status
./tool/debug-probe-hub-client/client.py status
```

### Command Line Debugging

```bash
# Terminal 1: Start GDB tunnel (keeps running)
./tool/debug-probe-hub-client/gdb_tunnel.py \
  --ssh-host 192.168.1.100 \
  --probe 4 \
  --local-port 3333

# Terminal 2: Connect GDB
/opt/wch-toolchain/toolchain/RISC-V-Embedded-GCC12/bin/riscv-wch-elf-gdb build/gfx_slave.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) continue
```

The `gdb_tunnel.py` script:
1. Calls debug-probe-hub API to start a debug session
2. Establishes SSH tunnel forwarding `localhost:3333` to remote GDB port
3. Waits until interrupted (Ctrl+C)

### VSCode Debugging

1. **Load environment variables**:
   ```bash
   # In terminal where you launch VSCode
   source .env
   code .
   ```

2. **Build firmware**: `make slave` or `make master`

3. **Select debug configuration**:
   - **Debug Slave (Debug-Probe-Hub)**
   - **Debug Master (Debug-Probe-Hub)**

4. **Press F5** to start debugging

VSCode will:
- Run `gdb_tunnel.py` in the background (via preLaunchTask)
- Wait for SSH tunnel to be ready
- Connect GDB to localhost:3333
- Load firmware and start debugging
- Clean up SSH tunnel when debugging ends (via postDebugTask)

## Flashing Firmware

### Local Flash

```bash
make flash-slave   # Flash via local OpenOCD
make flash-master
```

### Remote Flash (debug-probe-hub)

```bash
# Via Makefile (requires DEBUG_PROBE_HUB_PROBE to be set)
export DEBUG_PROBE_HUB_PROBE=4
make flash-slave-remote
make flash-master-remote

# Via CLI tool with explicit probe ID (recommended)
./tool/debug-probe-hub-client/flash.py \
  --target ch32v003 \
  --probe 4 \
  --firmware build/gfx_slave.bin

# Auto-detect probe (requires server connection)
./tool/debug-probe-hub-client/flash.py --firmware build/gfx_slave.bin
```

**Note**: If you don't specify `--probe`, the script will try to auto-detect a compatible probe by querying the debug-probe-hub server. This requires:
- The server to be running and accessible
- `DEBUG_PROBE_HUB_URL` to be correctly set

If the server is not accessible, you'll see:
```
Error: No compatible probe found. Please specify --probe.
```

In this case, always use `--probe` or set `DEBUG_PROBE_HUB_PROBE` environment variable.

### VSCode Tasks

You can also flash firmware using VSCode tasks:

1. Press `Ctrl+Shift+P`
2. Type "Tasks: Run Task"
3. Select:
   - "Flash Slave via Debug-Probe-Hub"
   - "Flash Master via Debug-Probe-Hub"

## Troubleshooting

### Local OpenOCD Issues

**Problem**: `Error: libusb_open() failed with LIBUSB_ERROR_ACCESS`

**Solution**: Add udev rules for WCH-Link:
```bash
sudo tee /etc/udev/rules.d/99-wch-link.rules << 'EOF'
# WCH-Link (RISC-V)
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="8012", MODE="0666"
# WCH-Link (ARM)
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="8010", MODE="0666"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**Problem**: OpenOCD fails to connect

**Solution**: Check WCH-Link is connected and recognized:
```bash
lsusb | grep 1a86
```

### Remote debug-probe-hub Issues

**Problem**: Cannot connect to debug-probe-hub / "No compatible probe found"

```
Error: No compatible probe found. Please specify --probe.
Warning: Failed to auto-detect probe: ... Connection refused
```

**Cause**:
- debug-probe-hub server is not running or not accessible
- `DEBUG_PROBE_HUB_URL` is incorrect or not set
- Network connectivity issue

**Solution**:
1. **Specify probe ID explicitly** (recommended):
   ```bash
   export DEBUG_PROBE_HUB_PROBE=4
   ./tool/debug-probe-hub-client/flash.py --firmware build/gfx_slave.bin
   ```

2. **Verify server is accessible**:
   ```bash
   # Check URL is correct
   echo $DEBUG_PROBE_HUB_URL

   # Test connection
   curl http://192.168.1.100:8080/status
   ```

3. **Check debug-probe-hub server is running**:
   ```bash
   # On the server
   systemctl status debug-probe-hub
   # or
   docker ps | grep debug-probe-hub
   ```

**Problem**: SSH tunnel fails

**Solution**:
- Test SSH: `ssh user@host`
- Set up SSH key authentication (avoid password prompts)
- Check `DEBUG_PROBE_HUB_SSH_HOST` and `DEBUG_PROBE_HUB_SSH_USER`

**Problem**: Probe is busy

**Solution**:
- Another user/session is using the probe
- Wait for the session to finish
- List available probes: `./tool/debug-probe-hub-client/client.py list-probes`
- Use a different probe: `--probe 5`

**Problem**: VSCode can't start debug-probe-hub tunnel

**Solution**:
- Ensure environment variables are set before launching VSCode:
  ```bash
  source .env
  code .
  ```
- Check VSCode's integrated terminal has the variables:
  ```bash
  echo $DEBUG_PROBE_HUB_PROBE
  ```

### GDB Issues

**Problem**: `Remote communication error.  Target disconnected.`

**Solution**:
- Check OpenOCD/tunnel is running
- Verify port 3333 is not already in use: `lsof -i :3333`

**Problem**: `Error finishing flash operation`

**Solution**:
- Flash chip may be write-protected
- Try erasing first: `monitor reset halt` then `load`

## Additional Resources

- [WCH CH32V003 Datasheet](https://www.wch-ic.com/products/CH32V003.html)
- [debug-probe-hub Documentation](https://github.com/kazcomy/debug-probe-hub)
- [OpenOCD Documentation](http://openocd.org/documentation/)
- [GDB Documentation](https://sourceware.org/gdb/documentation/)

## Build System Reference

### Makefile Targets

```bash
# Build
make slave               # Build slave firmware (default)
make master              # Build master firmware
make debug-slave         # Build with debug symbols
make debug-master        # Build master with debug symbols

# Flash (Local)
make flash-slave         # Flash via local OpenOCD
make flash-master        # Flash via local OpenOCD

# Flash (Remote)
make flash-slave-remote  # Flash via debug-probe-hub
make flash-master-remote # Flash via debug-probe-hub

# Debug Servers
make debug-slave-server  # Start local OpenOCD on port 3333
make debug-master-server # Start local OpenOCD on port 3333

# Analysis
make size-analysis       # Show binary size breakdown
make analyze-stack       # Generate stack usage reports

# Clean
make clean               # Remove build artifacts
```

### Environment Variables

```bash
# WCH Toolchain
WCH_TOOLCHAIN=/opt/wch-toolchain  # Toolchain installation path

# debug-probe-hub
DEBUG_PROBE_HUB_URL=http://remoteprogrammer.local.lan:8080     # Server URL
DEBUG_PROBE_HUB_SSH_HOST=localhost            # SSH hostname
DEBUG_PROBE_HUB_SSH_USER=$(whoami)            # SSH user
DEBUG_PROBE_HUB_TARGET=ch32v003               # Target device
DEBUG_PROBE_HUB_PROBE=4                       # Probe ID
```
