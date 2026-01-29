# ベースイメージ（Ubuntu 24.04などを指定）
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

# 必要なツールをインストール
# linux-tools-generic: usbipの実体が含まれる
# picolibc-riscv64-unknown-elf: RISC-V用の軽量Cライブラリ (newlib互換)
RUN apt-get update && apt-get install -y \
    linux-tools-generic \
    hwdata \
    usbutils \
    openocd \
    python3-venv \
    build-essential \
    gcc-riscv64-unknown-elf \
    picolibc-riscv64-unknown-elf \
    libusb-1.0-0-dev \
    libudev-dev \
    gdb-multiarch \
    && rm -rf /var/lib/apt/lists/*

# 【重要】カーネルバージョンチェックの回避策
# 1. コンテナ内にインストールされた linux-tools の中から、実体の usbip を探す
# 2. それを /usr/local/bin/usbip にシンボリックリンクする
#    (/usr/local/bin は /usr/bin より優先されるため、ラッパーが無視される)
RUN REAL_USBIP=$(ls /usr/lib/linux-tools/*/usbip | head -n 1) && \
    if [ -f "$REAL_USBIP" ]; then \
        ln -sf "$REAL_USBIP" /usr/local/bin/usbip && \
        echo "Fixed usbip to use $REAL_USBIP"; \
    else \
        echo "Error: usbip binary not found!"; exit 1; \
    fi