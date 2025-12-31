#!/bin/sh
#Reference setup script for PC that is connected to target board via WCH-LINK
sudo apt install python3.13-venv build-essential libnewlib-dev gcc-riscv64-unknown-elf libusb-1.0-0-dev libudev-dev gdb-multiarch
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
git clone https://github.com/cnlohr/ch32fun.git
cd ch32fun/minichlink && make
~/.platformio/penv/bin/pio pkg install --global --tool "community-ch32v/tool-openocd-riscv-wch@^2.1100.240729"
