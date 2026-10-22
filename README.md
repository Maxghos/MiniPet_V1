# MiniPet · ESP32-S3

Mascota de escritorio con ojos animados en una OLED **SSD1306 I²C de 128×32**, sensores PIR y ultrasónico, reloj de Santiago y clima de Puente Alto.

El **TTP223 se reserva para ajustes**. Una presencia sostenida en la zona del HC-SR05 solicita **clima → hora → pantalla habitual**. El humor regula la iniciativa, las expresiones y los comentarios de la mascota; las consultas siempre se completan.

## Qué incluye

- Ocho estados: tranquilo, atento, curioso, contento, impactado, molesto, cansado y dormido. Variantes de ojos, parpadeos, miradas suaves, alerta con fade por tramado y descanso con `Zzz`.
- Humor **0–100 %**, mensajes variados sin repetir los tres últimos y descansos entre rachas de insistencia.
- Presencia filtrada, tolerancia a ecos perdidos y gesto de **50–80 cm durante 5 s**, configurable. Necesita salir de la zona durante 1 s para rearmarse; un fondo inmóvil en esa zona al encender no activa consultas.
- Módulos independientes: asistente, reloj y clima; cualquier combinación que deje al menos uno habilitado.
- Clima principal con icono, detalles y máximas/mínimas de tres días. Consulta gratuita de Open-Meteo para uso no comercial, conforme a sus condiciones.
- Hora por NTP y reglas de Santiago con horario de verano. Tras sincronizar, el reloj sigue funcionando sin Wi-Fi mientras el equipo esté encendido.
- Menú de 21 opciones, ajustes persistentes y configuración Wi-Fi desde el teléfono. No requiere una clave de API meteorológica.

**[Guía de conexión y primera carga](docs/CONEXIONES.md)** · **[Comportamiento y ajustes](docs/COMPORTAMIENTO.md)** · **[Ver animaciones](docs/caras.html)**

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
| `src/Screen.cpp` | Pantallas, iconos y composición en 128×32 |
| `include/Messages.h` | Frases adaptadas del archivo de referencia y comentarios de la mascota |
| `src/main.cpp` | Integración y prioridades |

El clima se consulta cada 30 minutos y se reintenta al cabo de un minuto si falla. La última respuesta válida se conserva en RAM; se marca como anterior al desconectarse o al superar una hora. Un reinicio sin internet deja el reloj y el clima pendientes de sincronización; el asistente y los ajustes siguen disponibles.

Datos meteorológicos: [Open-Meteo](https://open-meteo.com/), [licencia de datos CC BY 4.0](https://open-meteo.com/en/license) y [condiciones del servicio](https://open-meteo.com/en/terms). Los iconos y el trazado de ojos son código del proyecto basado en las referencias aportadas; consulta su adaptación en [CARAS.md](docs/CARAS.md).
