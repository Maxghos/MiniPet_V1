#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
json_dir=".pio/libdeps/esp32s3/ArduinoJson/src"
if [[ ! -d "$json_dir" ]]; then
  echo "Falta ArduinoJson. Ejecuta primero: pio pkg install -e esp32s3" >&2
  exit 1
fi
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Iinclude -I"$json_dir" src/PetCore.cpp src/FaceRenderer.cpp src/TextLayout.cpp \
  src/WeatherBehavior.cpp src/WeatherEffects.cpp src/AppProtocol.cpp src/StudyTimer.cpp src/StudyCodec.cpp src/Agenda.cpp src/TelegramProtocol.cpp src/LittleActivities.cpp src/MiniGames.cpp src/OtaPackage.cpp \
  test/core/test_main.cpp test/core/test_v2.cpp test/core/test_app.cpp test/core/test_study.cpp test/core/test_sensor_behavior.cpp test/core/test_additional.cpp test/core/test_companion.cpp test/core/test_games.cpp test/core/test_ota.cpp \
  -o build/test_core
./build/test_core "$@"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Itest/sensors -Iinclude src/Sensors.cpp src/PetCore.cpp test/sensors/test_sensors.cpp -o build/test_sensors
./build/test_sensors
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Itest/services -Iinclude -I"$json_dir" src/Agenda.cpp src/AgendaService.cpp src/PetCore.cpp \
  test/services/test_agenda_service.cpp -o build/test_agenda_service
./build/test_agenda_service
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Itest/services -Iinclude -I"$json_dir" src/Agenda.cpp src/AgendaService.cpp src/PetCore.cpp \
  test/services/test_companion_service.cpp -o build/test_companion_service
./build/test_companion_service
python3 tools/test_timezone.py
python3 tools/generate_phrases.py --check
python3 tools/generate_telegram_trust.py --check
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Iinclude -I"$json_dir" \
  src/AppProtocol.cpp src/TextLayout.cpp src/StudyTimer.cpp src/StudyCodec.cpp src/Agenda.cpp src/TelegramProtocol.cpp tools/export_app_fixtures.cpp -o build/export_app_fixtures
./build/export_app_fixtures --check
