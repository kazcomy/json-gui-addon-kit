#!/bin/bash

# 設定
REMOTE_HOST="remotewchlink1.local.lan"
REMOTE_BUSID="2-1"

# --- 後処理関数（終了時に必ず呼ばれる） ---
cleanup() {
    echo "Stopping OpenOCD and detaching device..."
    
    # 現在アタッチされているポート番号を探す (例: Port 00)
    # "usbip port" コマンドの結果から、接続先ホスト名を含む行を探してポート番号を抽出
    IMPORTED_PORT=$(usbip port | grep -B 2 "$REMOTE_HOST" | grep "Port" | awk '{print $2}' | tr -d ':')
    
    if [ -n "$IMPORTED_PORT" ]; then
        usbip detach -p "$IMPORTED_PORT"
        echo "Device detached (Port $IMPORTED_PORT)."
    else
        echo "Device was strictly not attached or already detached."
    fi
}

# スクリプト終了時(EXIT)や中断時(SIGINT)に cleanup を実行する予約
trap cleanup EXIT

# --- メイン処理 ---

echo "Attaching remote J-Link ($REMOTE_BUSID from $REMOTE_HOST)..."
sudo usbip attach -r "$REMOTE_HOST" -b "$REMOTE_BUSID"

if [ $? -ne 0 ]; then
    echo "Error: Failed to attach. Is the device busy?"
    # attachに失敗した場合は cleanup を呼ばずに終了したい場合はここで exit
    exit 1
fi

echo "Starting OpenOCD..."
# 引数 ("$@") をそのまま OpenOCD に渡す
openocd "$@"

# OpenOCDが終了すると、自動的に trap された cleanup が走り、detach されます