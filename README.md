# MiniPet V3.2 · ESP32-S3 + Android + Telegram

Mascota de escritorio con ojos animados en una OLED **SSD1306 I²C de 128×32**, sensores PIR y ultrasónico, reloj de Santiago y clima de Puente Alto.

**[Funciones adicionales](docs/FUNCIONES_ADICIONALES.md):** Telegram conectado a Pomodoro, actividades del día, recordatorios puntuales/diarios, resumen de estudio y cuatro pequeñas actividades espontáneas. Todo se guarda y ejecuta en el ESP32; la app incorpora **Día**. El bot se configura con tu propio token por BLE cifrado y se vincula a tu cuenta mediante un PIN físico. El repositorio y las APK no incluyen datos personales ni un token preconfigurado.

Se conservan los **[ajustes de sensores de 3.1.1](docs/AJUSTES_3_1_1.md)**: gesto tolerante a pequeños movimientos y lecturas perdidas, captura de ecos más robusta y filtro del PIR.

**[Modo estudio / Pomodoro](docs/ESTUDIO.md).** Concentración y descansos configurables, pausa/reanudación, descanso largo por ciclos y resumen. Sigue en marcha sin la app ni internet; puedes controlarlo también desde el menú físico.

La app Android nativa amplía MiniPet por Wi-Fi local o BLE, con mensajes, imágenes/GIF/vídeos adaptados, biblioteca, estudio y ajustes. Conectarse no cambia su comportamiento. **[Instalación y uso de la app](docs/APP_ANDROID.md)** · **[Proyecto Android](android/README.md)** · **[Protocolo](docs/PROTOCOLO_APP.md)**.

**[Descargar APK MiniPet 3.2.0](releases/android/MiniPet-3.2.0-debug.apk?raw=true)** para Android 8.0 o superior. Actualiza primero el firmware. Se instala encima de las APK anteriores publicadas: conserva paquete y firma. Es una compilación de prueba firmada, guardada en `releases/android` junto con su SHA-256. Las claves de firma y los archivos generados por las compilaciones siguen excluidos.

El **TTP223 se reserva para ajustes**. Fuera del modo estudio, una presencia sostenida en la zona del HC-SR05 solicita **clima → reacción de la mascota → hora → pantalla habitual**. El humor regula la iniciativa, las expresiones y los comentarios de la mascota; las consultas siempre se completan. La reacción se omite con el asistente desactivado, No molestar, datos anteriores o pronóstico de próximos días.

## Qué incluye

- **19 estilos de ojos y 11 estados**: se añaden ironía, puchero y entusiasmo, con variantes diferenciadas de las referencias originales y las cuatro nuevas. Parpadeos, miradas suaves, alerta con fade y descanso con `Zzz`.
- **100 frases aleatorias** del archivo aportado, ocasionales según el humor. Texto centrado horizontal y verticalmente, acentos y signos españoles, páginas equilibradas y tiempo de lectura completo.
- Reacciones al calor, frío, sol, nubes, niebla, lluvia, nieve y tormenta; bromas según el clima, animaciones ambientales y un aviso cuando nuevas actualizaciones pasan de seco a lluvia. **Sin peticiones adicionales**.
- Humor **0–100 %**, mensajes variados sin repetir los tres últimos y descansos entre rachas de insistencia.
- Presencia filtrada, tolerancia a ecos perdidos y gesto de **50–80 cm durante 5 s**, configurable. Necesita salir de la zona durante 1 s para rearmarse; un fondo inmóvil en esa zona al encender no activa consultas.
- Módulos independientes: asistente, reloj y clima; cualquier combinación que deje al menos uno habilitado.
- Clima principal con icono, detalles y máximas/mínimas de tres días. Consulta gratuita de Open-Meteo para uso no comercial, conforme a sus condiciones.
- Hora por NTP y reglas de Santiago con horario de verano. Tras sincronizar, el reloj sigue funcionando sin Wi-Fi mientras el equipo esté encendido.
- Menú de **28 opciones**, con «Modo estudio», «Vincular app», «Vincular Telegram», controles independientes para frases random, reacciones y avisos del clima y destello de tormenta (apagado inicialmente). Se conservan los ajustes guardados en V1/V2/V3 y el Wi-Fi. No requiere una clave de API meteorológica.
- App Android 8.0+: previsualización exacta de 128×32, texto centrado/desplazado, recorte/zoom/giro/contraste/tramado y clips de hasta 30 s/15 fps. Bibliotecas separadas en teléfono y ESP32; guardar no cambia la pantalla.
- Pomodoro 25/5/15 por defecto, descanso largo cada cuatro bloques, programas Clásico/Suave/Profundo y tiempos personalizados. Espera tu confirmación para volver al estudio; inicio automático opcional. Sin bromas ni consultas visuales durante la sesión, con recuperación pausada tras reiniciar.
- Agenda compartida entre app y bot: 24 actividades, 12 recordatorios y estadísticas de 14 días. Resumen automático a las 20:30, configurable, con horarios de Santiago y No molestar.
- Telegram personal sin servidor propio: notificaciones con la app cerrada, controles de Pomodoro con confirmación y acceso exclusivo a la cuenta vinculada. Requiere Wi-Fi con internet. El temporizador sigue funcionando localmente si se pierde la red.
- Cuatro microescenas propias: leer un librito, respirar, estirarse y mirar una estrella; reguladas por humor y desactivables.

**[Guía de conexión y primera carga](docs/CONEXIONES.md)** · **[Comportamiento y ajustes](docs/COMPORTAMIENTO.md)** · **[Ver animaciones](docs/caras.html)**

Los cambios de V2 están en **[CAMBIOS_V2.md](docs/CAMBIOS_V2.md)**; las nuevas funciones y límites, en **[APP_ANDROID.md](docs/APP_ANDROID.md)**. Para actualizar desde V1/V2, abre el proyecto completo actualizado en VS Code/PlatformIO y ejecuta `esp32s3 → General → Build` y después `Upload`; no borres la memoria del dispositivo. El cableado es el mismo. Después instala la APK y abre «Vincular app» en la OLED.

Para la vista interactiva, descarga `docs/caras.html` y ábrelo en el navegador. Contiene cuadros producidos por el mismo motor C++ del firmware; GitHub muestra el código HTML en su visor.

## Compilar y cargar

Usa PlatformIO Core **6.1.18** o PlatformIO en VS Code, Python 3 y un ESP32-S3 **N16R8** (16 MB flash, 8 MB PSRAM OPI).

```sh
python -m venv .venv
# Linux/macOS; en Windows: .venv\Scripts\activate
source .venv/bin/activate
pip install -r tools/requirements.txt
pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor -b 115200
```

La configuración de memoria está en `boards/minipet-s3-n16r8.json` y los GPIO en `include/BoardConfig.h`. Comprueba la serigrafía de tu placa antes de cablear. Para un pulsador entre GPIO 7 y GND, compila/carga con `-e esp32s3-button`.

Al iniciar, toca el botón para abrir ajustes. Avanza hasta **Configurar WiFi**, mantén aproximadamente 1 s y suelta. La OLED muestra el nombre y la clave temporal de su red; conéctate y abre `http://192.168.4.1`. Introduce tu Wi-Fi de **2,4 GHz**. La configuración se cierra con el botón o tras cinco minutos.

## Controles

| Contexto | Toque corto | Mantener ~1 s y soltar | Mantener ~3 s y soltar |
|---|---|---|---|
| Pantalla habitual | Abrir ajustes | Abrir ajustes | Abrir ajustes |
| Lista de ajustes | Siguiente opción | Editar/elegir | Salir sin guardar |
| Editando un valor | Cambiar valor | Volver a la lista | Volver a la lista |
| Configuración Wi-Fi | Cerrar configuración | Cerrar configuración | Cerrar configuración |

Elige **Guardar y salir** para conservar los cambios. Los gestos y mensajes quedan suspendidos mientras configuras. El botón no recorre pantallas informativas.

## Verificación de software

```sh
pio pkg install -e esp32s3
bash tools/test_host.sh
pio run -e esp32s3 -e esp32s3-button
bash tools/preview.sh
# Opcional: comprobar también el visor generado (requiere Node.js 18+)
node tools/test_preview.js
```

Las pruebas de host necesitan Linux/macOS con `g++`, Python 3.9+ y la base IANA de zonas horarias. Ejecutan escenarios de sensores, gestos, rebotes, menús, prioridades, módulos, mensajes y respuestas meteorológicas, con AddressSanitizer y UndefinedBehaviorSanitizer. Comparan las reglas horarias del firmware con `America/Santiago` durante cada hora de 2026. La CI reproduce las pruebas y las dos compilaciones.

La compilación y estas simulaciones **no sustituyen las pruebas físicas**: hay que comprobar el pinout del clon, polaridad del TTP223, distancias, alimentación, PSRAM, Wi-Fi, TLS y legibilidad/fluidez de la OLED real. El procedimiento está en la guía de conexión. No se ha cargado este firmware en una placa durante el desarrollo en la nube.

## Organización

| Archivo | Responsabilidad |
|---|---|
| `src/PetCore.cpp` | Gesto, presencia, personalidad, consulta y menú; portable al ordenador |
| `src/FaceRenderer.cpp` | Geometría y animación de ojos, framebuffer nativo de 512 bytes |
| `src/Sensors.cpp` | Eco por interrupciones, PIR y botón |
| `src/NetworkService.cpp` | Tarea de red, NTP, HTTPS y configuración Wi-Fi |
| `src/AppService.cpp` | API Wi-Fi/BLE, vinculación, transferencias y biblioteca LittleFS |
| `src/AppProtocol.cpp` | Formato MPET/CRC y reproducción con sesión temporal |
| `src/Agenda.cpp`, `src/AgendaService.cpp` | Actividades, recordatorios, estadísticas, persistencia y avisos |
| `src/TelegramProtocol.cpp`, `src/TelegramService.cpp` | Vinculación física, comandos autorizados y comunicación HTTPS con el bot |
| `src/LittleActivities.cpp` | Cuatro pequeñas actividades dibujadas por el motor nativo |
| `src/Screen.cpp` | Pantallas, iconos y composición en 128×32 |
| `src/TextLayout.cpp` | Conversión de español a la fuente de pantalla, ajuste de líneas, centrado y paginación |
| `src/WeatherBehavior.cpp` | Clasificación del clima, comparación de actualizaciones y pausas compartidas |
| `src/WeatherEffects.cpp` | Sudor, gotas, nieve, temblor y tormenta |
| `include/Messages.h` | Frases adaptadas del archivo de referencia y comentarios de la mascota |
| `assets/frases_random_es.txt` | Las 100 frases originales; `tools/generate_phrases.py` regenera su banco C++ |
| `src/main.cpp` | Integración y prioridades |
| `android/` | App Kotlin/Jetpack Compose y pruebas Android |
| `protocol/fixtures/` | Contratos binarios y de texto comprobados entre C++ y Kotlin |

El clima se consulta cada 30 minutos y se reintenta al cabo de un minuto si falla. La última respuesta válida se conserva en RAM; se marca como anterior al desconectarse, al superar una hora desde la consulta o si la hora de la observación/modelo supera 90 minutos respecto a la descarga. Un reinicio sin internet deja el reloj y el clima pendientes de sincronización; el asistente y los ajustes siguen disponibles. Los avisos describen información de Open-Meteo, no mediciones físicas de lluvia en tu casa.

Datos meteorológicos: [Open-Meteo](https://open-meteo.com/), [licencia de datos CC BY 4.0](https://open-meteo.com/en/license) y [condiciones del servicio](https://open-meteo.com/en/terms). Los iconos y el trazado de ojos son código del proyecto basado en las referencias aportadas; consulta su adaptación en [CARAS.md](docs/CARAS.md).
