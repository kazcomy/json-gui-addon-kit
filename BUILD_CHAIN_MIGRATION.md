# Build Chain Migration: PlatformIO → WCH Official Toolchain

## Migration Complete

This project has been migrated from PlatformIO-based build system to WCH official toolchain only.

### Removed Dependencies
- ❌ PlatformIO
- ❌ ch32v003fun framework (from PlatformIO packages)
- ❌ minichlink

### New Dependencies
- ✅ WCH official RISC-V toolchain (from wch-toolchain-mirror)
- ✅ WCH official OpenOCD
- ✅ Custom startup code (startup/startup_ch32v00x.S)
- ✅ Custom system initialization (startup/system_ch32v00x.c)

## Setup

```bash
# Set your private wch-toolchain-mirror URL
export WCH_TOOLCHAIN_URL=https://your-private-repo/wch-toolchain.tar.gz

# Install WCH toolchain
./tool/setup_wch_toolchain.sh

# Build firmware
make slave
make master
```

## Build Results

- Slave firmware: ~16KB (fits in 16KB FLASH)
- Master firmware: ~3KB

## References

- Old PlatformIO-based documentation: See git history
- New Makefile-based build: See README.md
