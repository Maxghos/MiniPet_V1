# Validación de esta versión

Comprobaciones ejecutadas en el entorno de desarrollo, el 22 de octubre de 2026:

| Comprobación | Resultado |
|---|---|
| PlatformIO / ESP32-S3 N16R8 / TTP223 | Compilación correcta |
| Variante con pulsador mecánico | Compilación correcta |
| Tamaño del firmware TTP223 | Aproximadamente 975 kB de flash; 49 kB de RAM estática |
| Pruebas C++ de host | 9 suites, 1.986 comprobaciones correctas |
| AddressSanitizer y UndefinedBehaviorSanitizer | Sin errores en las pruebas ejecutadas |
| Hora local 2026 | 8.760 comparaciones horarias correctas con IANA America/Santiago |
| Open-Meteo, consulta real de Puente Alto | Respuesta recibida y validada por el decodificador C++ |
| Datos incompletos o inválidos del clima | Rechazados; se conserva la última respuesta válida |
| Animación generada por el motor C++ | Ocho expresiones revisadas en framebuffer de 128×32 |
| JavaScript del visor HTML | Ocho expresiones, 801 cuadros reproducidos en prueba con canvas simulado |
| Script `tools/setup_cloud.sh` | Ejecutado correctamente después de preparar el entorno; reutiliza dependencias |

Los escenarios incluyen fondo en la zona al arrancar, gesto sostenido, retirada y rearme, ecos inválidos, pérdida de muestras, desbordamiento de `millis()`, mediana de distancia, compañía sin movimiento, rebotes y pulsaciones largas, edición/guardado, combinaciones de módulos, orden de consulta, No molestar, frecuencia de recordatorios y variedad de mensajes.

**Pendiente en hardware:** carga y arranque del ESP32 real, memoria flash/PSRAM del clon, OLED/I²C, niveles eléctricos, polaridad del TTP223, rango y ecos del ultrasónico, PIR, portal Wi-Fi desde un teléfono, NTP/HTTPS desde la red del dispositivo y fluidez física de las animaciones. No había un dispositivo USB conectado al entorno de desarrollo. Seguir [CONEXIONES.md](CONEXIONES.md) para realizar estas comprobaciones.

La consulta meteorológica se probó desde el entorno de desarrollo. La cadena TLS de la conexión del ESP32 y su CA incorporada necesitan comprobarse en la placa. La CI está configurada en el repositorio; sus ejecuciones posteriores al push son independientes de estas pruebas locales.
