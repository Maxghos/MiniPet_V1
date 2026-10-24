# Validación de esta versión

Comprobaciones de **V2** ejecutadas en el entorno de desarrollo, el 24 de octubre de 2026. El usuario confirmó el funcionamiento físico de V1; las adiciones de V2 todavía necesitan esa comprobación en su placa.

| Comprobación | Resultado |
|---|---|
| PlatformIO / ESP32-S3 N16R8 / TTP223 | Compilación correcta |
| Variante con pulsador mecánico | Compilación correcta |
| Tamaño del firmware TTP223 | Aproximadamente 1 MB de flash; 49,4 kB de RAM estática |
| Pruebas C++ de host | 16 suites, 7.087 comprobaciones correctas |
| AddressSanitizer y UndefinedBehaviorSanitizer | Sin errores en las pruebas ejecutadas |
| Hora local 2026 | 8.760 comparaciones horarias correctas con IANA America/Santiago |
| Open-Meteo, consulta real de Puente Alto | Respuesta recibida y validada por el decodificador C++ |
| Datos incompletos o inválidos del clima | Rechazados; se conserva la última respuesta válida |
| Animación generada por el motor C++ | 11 estados, 19 estilos distintos y ocho ejemplos de clima revisados en 128×32 |
| JavaScript del visor HTML | Las 38 secuencias coinciden píxel a píxel con capturas del motor nativo; pausa comprobada |
| Frases aleatorias | Las 100 conservan su contenido completo y se centran por página en ambos ejes |
| Migración de ajustes | V1→V2 conserva los valores previos e inicializa las cuatro opciones nuevas |
| Avisos meteorológicos | Se prueban datos repetidos/viejos, reconexión, No molestar y comparación de nuevas actualizaciones |
| Script `tools/setup_cloud.sh` | Ejecutado correctamente después de preparar el entorno; reutiliza dependencias |

Los escenarios incluyen fondo en la zona al arrancar, gesto sostenido, retirada y rearme, ecos inválidos, pérdida de muestras, desbordamiento de `millis()`, mediana de distancia, compañía sin movimiento, rebotes y pulsaciones largas, edición/guardado, combinaciones de módulos, orden clima→reacción→hora, No molestar, presupuesto compartido de intervenciones, variedad de mensajes, texto largo sin pérdida de caracteres, español, centrado, páginas equilibradas, fechas meteorológicas y restauración de la inversión opcional de tormenta.

**Pendiente en hardware V2:** actualización y migración en la placa real, nuevos estilos y efectos, lectura de las frases, temporización percibida, avisos con red real y ausencia de regresiones en los sensores/configuración ya probados por el usuario en V1. No hay un dispositivo USB conectado al entorno de desarrollo. Seguir [CONEXIONES.md](CONEXIONES.md) para realizar estas comprobaciones.

La consulta meteorológica se probó desde el entorno de desarrollo. La cadena TLS de la conexión del ESP32 y su CA incorporada necesitan comprobarse en la placa. La CI está configurada en el repositorio; sus ejecuciones posteriores al push son independientes de estas pruebas locales.
