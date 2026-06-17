#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/workspaces/akira-workspace/AkiraOS"
WORKSPACE_DIR="/workspaces/akira-workspace"

cd "$WORKSPACE_DIR"

if [[ ! -d "$APP_DIR/.git" ]]; then
    echo "AkiraOS checkout not found at $APP_DIR" >&2
    exit 1
fi

git -C "$APP_DIR" submodule update --init --recursive

if [[ ! -d "$WORKSPACE_DIR/.west" ]]; then
    west init -l "$APP_DIR"
fi

west update
west zephyr-export

if [[ -f "$WORKSPACE_DIR/zephyr/scripts/requirements.txt" ]]; then
    python3 -m pip install --user -r "$WORKSPACE_DIR/zephyr/scripts/requirements.txt"
fi

west blobs fetch hal_espressif

cat <<'EOF'

AkiraOS devcontainer is ready.

Common commands:
  ./build.sh -b native_sim
  ./build.sh -b esp32s3_devkitm_esp32s3_procpu
  ./build.sh -b esp32s3_super_mini_esp32s3_procpu
  ./build.sh -b esp32s3_devkitm_esp32s3_procpu -r a -p /dev/ttyACM0

EOF
