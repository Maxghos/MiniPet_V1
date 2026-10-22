#!/usr/bin/env bash
# Source this only inside the Codex cloud workspace.
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-/workspace/.platformio}"
export PLATFORMIO_SETTING_ENABLE_TELEMETRY=no
export PATH="/workspace/.venvs/minipet/bin:$PATH"
