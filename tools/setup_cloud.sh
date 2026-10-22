#!/usr/bin/env bash
set -euo pipefail
cd /workspace/MiniPet_V1
python3 -m venv /workspace/.venvs/minipet
/workspace/.venvs/minipet/bin/pip install -r tools/requirements.txt
source tools/cloud-env.sh
pio pkg install -e esp32s3
pio run -e esp32s3 -e esp32s3-button -j 4
bash tools/test_host.sh
