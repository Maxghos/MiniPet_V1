# Protocolo local MiniPet 1

Se comparte entre firmware V3 y la aplicación Android. La conexión por sí sola es pasiva respecto de la mascota. Los ajustes solo se modifican con una orden explícita; reproducir texto/media establece una sesión con retorno automático.

## Transportes

- Wi-Fi: `POST http://IP:8080/api/v1/command`, `Content-Type: application/json`. `GET /api/v1/info` expone nombre, versión de protocolo y límites, sin credenciales. mDNS anuncia `_minipet._tcp` en el puerto 8080.
- BLE: servicio `899a1000-48a0-4d42-8bd6-602c08a43c7e`, escritura `899a1001-48a0-4d42-8bd6-602c08a43c7e`, respuestas `899a1002-48a0-4d42-8bd6-602c08a43c7e` por indicaciones confirmadas. El cliente activa CCCD `0x0002`. Las escrituras/respuestas usan conexión cifrada.
- BLE fragmenta JSON UTF-8 delimitado por `\n`, ajustándose al MTU. El cliente envía como máximo 160 bytes por escritura; el servidor adapta sus fragmentos hasta 180 bytes y espera confirmación. Se permite una orden en curso por cliente. Se reinicia el ensamblado al desconectar y se descartan respuestas de una conexión anterior.
- Tamaño máximo de orden JSON: 12.288 bytes. Las respuestas incluyen `ok`, `req` y, al fallar, `error`. `req` correlaciona solicitudes; la aplicación comprueba que coincide.

## Vinculación y órdenes

En la OLED se abre físicamente una ventana de dos minutos, PIN de seis cifras, hasta cinco intentos. `{"op":"pair","req":1,"code":"123456"}` devuelve una credencial aleatoria de 64 caracteres hexadecimales. El ejemplo no es un PIN fijo. Una nueva vinculación reemplaza la credencial anterior.

Las demás órdenes requieren `Authorization: Bearer CREDENCIAL` en HTTP o el campo `token` en JSON. La app admite ambos y almacena la credencial cifrada con Android Keystore. Wi-Fi es HTTP local, no TLS: utiliza una red de confianza y no publiques este servicio en internet. BLE añade el emparejamiento cifrado del sistema con el PIN de la OLED.

| `op` | Datos / resultado |
|---|---|
| `ping` | Renueva la sesión de comunicación; no cambia la pantalla ni cuenta como atención |
| `status`, `settings.get` | Ajustes, biblioteca disponible, IP, versión de firmware y reproducción activa |
| `study.get` | Estado, programa y resumen del temporizador; no cambia la pantalla ni cuenta como atención |
| `study.start`, `study.configure` | `config` opcional con programa; solo con sesión inactiva. Empezar también guarda el programa |
| `study.pause`, `study.resume` | Pausa o continúa la etapa pendiente |
| `study.skip`, `study.stop` | Salta etapa o termina sesión; conserva el resumen hasta el siguiente inicio |
| `settings.set` | Objeto `settings` con campos enteros de los ajustes existentes; se validan rangos y franja de gesto; el bucle principal aplica/persiste |
| `text` | `text` UTF-8, `durationMs` (-1 automático, 0 fijado, positivo hasta 3.600.000), `scroll`, `speed` (10–80 px/s) |
| `stop` | Finaliza el contenido externo |
| `library` | Lista de `id`, `title`, `bytes`, `frames`, `fps` |
| `play` | `id`, `durationMs`, `loop`; comprueba archivo y CRC antes de cargarlo |
| `delete` | Borra `id`; si estaba reproduciéndose, lo detiene |
| `upload.begin` | `id`, `title` (hasta 96 bytes), `bytes`; inicia staging y verifica cupo/espacio |
| `upload.chunk` | `id`, `offset`, `data` Base64; el offset debe coincidir; responde próximo offset |
| `upload.commit` | `id`; requiere longitud completa, cabecera y CRC válidos, luego renombra el archivo temporal |
| `upload.cancel` | Descarta la carga parcial sin borrar el archivo previo |

`id` acepta únicamente 1–24 caracteres ASCII alfanuméricos, `_` y `-`. No se aceptan rutas. La app usa bloques binarios de 3.072 bytes en Wi-Fi y 120 en BLE. Hay una sola carga pendiente y se exige actividad al menos cada dos minutos. Las imágenes/vídeos no se muestran hasta ejecutar `play`.

## Temporizador de estudio (firmware 3.1)

`status`/`settings.get` incluyen `study` a partir de 3.1; la app detecta ese campo para habilitar la función. Las respuestas a órdenes `study.*` incluyen el estado confirmado en `study`:

- `config`: `focusMinutes` (1–180), `shortBreakMinutes` (1–60), `longBreakMinutes` (1–120), `blocksBeforeLong` (2–12), `autoBreak` y `autoFocus` (booleanos JSON). Los valores iniciales son 25/5/15/4/true/false.
- `phase`: `idle`, `focus`, `shortBreak`, `longBreak`; `state`: `idle`, `running`, `paused`, `waiting`.
- `remainingMs`, `durationMs`, `completedBlocks`, `round`, `focusMs`, `breakMs`: tiempos y resumen. Solo una etapa corriendo acumula tiempo.
- `revision` y `eventId`: contadores uint32; `event` y `eventAgeMs` identifican el último cambio. `persisted` informa si pudo guardarse en NVS.

La app adjunta `expectedRevision` a cada orden que modifica estudio. Si el estado cambió —por ejemplo, el bloque terminó mientras se enviaba Saltar— se rechaza la orden y se devuelve el estado actual; el cliente sincroniza antes de permitir otro intento. No se reintentan automáticamente acciones que podrían afectar a otra etapa. `study.get` es una consulta pasiva autenticada y también renueva la comunicación.

Las órdenes pasan por un mutex del servicio de estudio, compartido con controles locales. El instante monotónico se toma **después de adquirirlo** para evitar retroceder entre tareas. El temporizador se guarda separadamente en `minipet-study`, registro JSON versión 1, al cambiar/controlar etapas y cada minuto en marcha; al reiniciar se recupera pausado o esperando confirmación. No usa la hora de internet ni depende de la sesión de reproducción de 15 s.

Mientras estudio esté activo, se rechazan `text`/`play`; se permite guardar archivos. El menú físico conserva prioridad: bloquea iniciar, reanudar, configurar o saltar desde la app. Pausar y terminar siguen disponibles. El mando `stop` de media no equivale a `study.stop`.

## Archivo MPET

Enteros little-endian. Cabecera de **24 bytes**, seguida de `frames × 512` bytes.

| Offset | Bytes | Valor |
|---|---|---|
| 0 | 4 | ASCII `MPET` |
| 4 | 1 | Versión `1` |
| 5 | 1 | FPS, 1–15 |
| 6 | 2 | Ancho, 128 |
| 8 | 2 | Alto, 32 |
| 10 | 2 | Cantidad de cuadros, 1–450 |
| 12 | 4 | Tamaño del payload, `frames × 512` |
| 16 | 4 | CRC-32/ISO-HDLC del payload (polinomio reflejado `0xedb88320`) |
| 20 | 4 | Reservado, cero |

Cada cuadro usa el formato de páginas SSD1306: byte `x + (y / 8) × 128`, bit `y % 8`. Blanco=1. Las imágenes tienen un cuadro y se mantienen durante el tiempo elegido; los clips pueden repetirse o conservar su último cuadro hasta completar ese tiempo. Con duración automática se termina al completar una reproducción.

## Sesión, almacenamiento y prioridades

Una orden autenticada renueva la comunicación; la aplicación consulta `study.get` cada dos segundos en primer plano con firmware 3.1 o envía `ping` cada cinco segundos con firmware anterior. El contenido externo expira sin comunicación durante 15 s. Al expirar, se descarta el estado de reproducción para impedir que reaparezca un contenido fijado al reconectar. El menú físico tiene prioridad; las órdenes de mostrar/aplicar ajustes se rechazan mientras esté abierto. Se mantienen sensores y tareas de red durante la reproducción, y se pausan las iniciativas visuales de la mascota. El temporizador de estudio conserva su autonomía al desconectar.

La biblioteca usa LittleFS en la partición `spiffs` existente, hasta ocho archivos/2 MiB. Se monta sin formatear una biblioteca ya inicializada; solo el primer uso puede inicializar la partición antes no usada por MiniPet. El contenido activo se carga en PSRAM y se reproduce desde RAM. La carga es temporal hasta verificar tamaño/CRC y renombrarla; guardar no sustituye la pantalla activa.

## Fixtures compartidos

`protocol/fixtures/frame_clip.mpet`, `text_layout.json` y `study_cycle.json` se generan mediante `tools/export_app_fixtures.cpp`. Las pruebas nativas verifican que los fixtures no cambian, y los tests Kotlin los usan para comprobar la codificación de media, páginas de texto y estados de un ciclo Pomodoro real del C++. Para regenerarlos después de cambiar deliberadamente el protocolo:

```sh
g++ -std=c++17 -Iinclude -I.pio/libdeps/esp32s3/ArduinoJson/src src/AppProtocol.cpp src/TextLayout.cpp src/StudyTimer.cpp src/StudyCodec.cpp tools/export_app_fixtures.cpp -o build/export_app_fixtures
./build/export_app_fixtures
```

Las pruebas cubren el formato/contrato, no sustituyen la validación física de Wi-Fi/BLE, flash/PSRAM o tiempos de la OLED.
