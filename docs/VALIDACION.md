# Validación de esta versión

## V3.2: Funciones adicionales

Comprobaciones locales del **2 de noviembre de 2026**, fecha de Santiago. No se ha configurado un token personal ni se han enviado mensajes a Telegram durante el desarrollo.

| Comprobación | Resultado |
|---|---|
| C++ con AddressSanitizer/UndefinedBehaviorSanitizer | 39 suites, 8.084 comprobaciones correctas |
| Controlador de sensores real con GPIO/reloj simulados | Cinco suites, 388 comprobaciones correctas |
| Agenda | Crear, completar/desmarcar sin sumar dos veces, pasar a hoy, eliminar, límites, fechas válidas y persistencia/corrupción |
| Recordatorios/resumen | Avisos puntuales/diarios, siguiente día, años bisiestos, No molestar, descarte de atrasos y una emisión por día |
| Estadísticas | Acumulación entre sesiones, separación de estudio/descanso, cambio de fecha y recuperación de registros |
| Telegram, núcleo portable | PIN, cinco intentos, caducidad, usuario privado autorizado, grupos/reenviados/otras cuentas rechazados, IDs repetidos y comandos permitidos |
| Botones de Telegram | Datos con revisión; confirmación antes de saltar/terminar; rechazo de parámetros fuera de rango |
| Privacidad | Estado público sin token/PIN/IDs; cliente Android impide enviar token por HTTP y limpia la entrada al pasar al fondo |
| Microescenas | Cuatro actividades con cuadros distintos y dibujo acotado a 128×32; lectura sin triángulo de alerta; presupuesto de humor respetado |
| Android | 29 pruebas JUnit/Robolectric/Compose, ninguna omitida; Lint sin incidencias |
| Contrato de datos | `additional.json` generado por C++ y consumido por Kotlin; actividades, avisos, resumen y estado público del bot |
| UI | Navegación de las cinco pestañas, Texto/Media/Biblioteca y cuatro vistas de Día renderizadas y revisadas |
| Firmware N16R8, TTP223 y pulsador | Ambas compilaciones correctas; aproximadamente 1,43 MB de flash y 59,3 kB de RAM estática |
| APK 3.2.0 | Código de versión 5, firma y alineación verificadas; mismo certificado de las APK anteriores publicadas |
| Confianza HTTPS | Bundle de 121 CA públicas, generado desde wheel oficial de certifi con huellas fijadas; estructura/huella comprobadas sin usar las CA privadas del entorno |
| Regresiones | 8.760 comparaciones de Santiago, 100 frases, protocolo de reproducción y ciclo Pomodoro correctos |

**Pendiente con tus dispositivos y bot:** handshake TLS real a Telegram, entrega de avisos, operación de getUpdates/teclados con la API real, almacenamiento LittleFS/NVS tras cortes, Bluetooth/Wi-Fi y convivencia prolongada de red/sensores/pantalla. Las pruebas de autenticación y comandos son locales con datos sintéticos; no se presentan como una conversación real con Telegram. No hay un ESP32 ni teléfono USB conectados al entorno. Seguir [FUNCIONES_ADICIONALES.md](FUNCIONES_ADICIONALES.md).

La fuente de las CA se obtiene de un wheel de PyPI verificado, no del almacén de certificados del host. El diseño serializa las conexiones TLS de clima y bot en tareas de red, pero el consumo dinámico de RAM y la cadencia percibida requieren medición en la placa. Las estadísticas se recogen aproximadamente cada segundo y se guardan por intervalos de hasta un minuto; no tienen precisión de auditoría ni miden atención. Los resultados de CI posteriores al push son independientes de estas pruebas locales.

## V3.1.1: mantenimiento de sensores y optimizaciones

| Comprobación | Resultado |
|---|---|
| Pruebas C++ del comportamiento, con sanitizadores | 33 suites, 7.926 comprobaciones correctas |
| Controlador real `src/Sensors.cpp` con GPIO/reloj simulados | Cinco suites, 388 comprobaciones correctas; AddressSanitizer/UndefinedBehaviorSanitizer |
| Gesto | Mano moviéndose dentro de la zona, bordes con histéresis, lectura perdida, ausencia prolongada de eco, salida con mediana antigua y rearme confirmado |
| Captura del HC-SR05 | Pico corto seguido de eco válido, eco tardío, timeout, ECHO inicialmente alto, lectura demasiado antigua y 50 ciclos alternando aciertos/pérdidas |
| Simulación integrada controlador→filtro→gesto | Una mano variable, una lectura perdida y un valor atípico completan una sola consulta; los intervalos interrumpidos no adelantan el disparo |
| PIR y presencia | Pulsos breves, calentamiento con HIGH inicial, rearme, huecos entre muestras y separación entre sorpresas |
| Relojes | Se ejercitan los desbordamientos de `micros()` y `millis()` en captura y calentamiento |
| Firmware N16R8 / TTP223 y pulsador | Ambas compilaciones correctas; aproximadamente 1,35 MB de flash y 58,9 kB de RAM estática |
| Regresiones | 8.760 comparaciones de zona horaria, 100 frases y fixtures de media/texto/Pomodoro correctos |

La app Android y su APK 3.1.0 no cambian en esta actualización. Se conserva el protocolo y el formato de ajustes; la versión que informa el firmware pasa a 3.1.1. Las correcciones de adquisición de mutex/reloj se revisaron y compilaron; no se afirma una prueba de concurrencia en un ESP32 físico.

**Pendiente en tu montaje:** sensibilidad con tu sensor/clon, alimentación y entorno, comodidad de la espera de cinco segundos y ausencia de activaciones no deseadas. El simulador ejecuta el controlador C++ real, pero no reproduce la electrónica, interferencias reales ni el planificador del ESP32. La comprobación física está en [AJUSTES_3_1_1.md](AJUSTES_3_1_1.md).

## V3.1: modo estudio / Pomodoro

Comprobaciones del **1 de noviembre de 2026**, fecha local de Santiago. Las pruebas del temporizador usan avance simulado y el mismo motor/codec C++ del firmware. No hay un ESP32 ni un teléfono físico conectados al entorno.

| Comprobación | Resultado |
|---|---|
| Firmware N16R8 con TTP223 y con pulsador mecánico | Ambas compilaciones correctas; aproximadamente 1,35 MB de flash y 58,9 kB de RAM estática |
| Pruebas C++ con AddressSanitizer/UndefinedBehaviorSanitizer | 28 suites, 7.358 comprobaciones correctas |
| Ciclo completo 25/5/15 | Cuatro bloques, descansos cortos, descanso largo y vuelta al primer bloque; confirmación manual por defecto |
| Controles y tiempos | Pausa/reanudación, saltos sin acreditar bloques, resumen, opciones de inicio automático y espera sin consumir tiempo |
| Límites y recuperación | Rangos/tipos inválidos, registros incompletos/corruptos, reinicio pausado, retrasos de ejecución y desbordamiento de `millis()` |
| Órdenes concurrentes | Revisión de estado impide aplicar una orden antigua a otra etapa; validación del protocolo compartido |
| Android | 23 pruebas JUnit/Robolectric/Compose, ninguna omitida; Lint sin incidencias |
| Contrato Android/C++ | Nueve estados en `study_cycle.json` producidos por C++; cuenta atrás, resumen, pausa y recuperación leídos por Kotlin |
| Interfaz | Navegación real entre las cinco pestañas, dos vistas de Enviar, controles de estudio/descanso/pausa/espera, confirmaciones y bloqueo de acciones sin estado reciente |
| Capturas | Vistas recompuestas y dibujadas con gráficos nativos; cinco pestañas distintas y estados de estudio revisados |
| APK 3.1.0 | Firma y alineación verificadas; mismo certificado que la APK 3.0.0, código de versión aumentado a 4 |
| Regresiones | 8.760 comprobaciones de zona horaria y 100 frases; fixtures de texto/media conservados |

**Pendiente en hardware:** temporización percibida y legibilidad en la OLED real, transferencias y control por Wi-Fi/BLE en un teléfono, guardado/recuperación NVS al cortar alimentación y convivencia con los sensores durante sesiones largas. No se han probado radios ni flash física desde el host. La app no incluye alarmas de teléfono en segundo plano; al cerrarla, el temporizador y los avisos visuales siguen en el ESP32. La prueba rápida está al final de [ESTUDIO.md](ESTUDIO.md).

La nueva versión mantiene intacto el formato de ajustes de mascota V2 y usa un namespace NVS independiente para estudio. La firma de actualización se comprueba contra la APK anterior publicada; esto no sustituye instalar ambas consecutivamente en el teléfono. Los resultados de CI posteriores al push son independientes de las pruebas locales.

## V3: Funciones + apk

Comprobaciones ejecutadas el **30 de octubre de 2026** en el entorno de desarrollo. No hay un ESP32 ni un teléfono conectados a este entorno; las pruebas de software no sustituyen la prueba conjunta en tus dispositivos.

| Comprobación | Resultado |
|---|---|
| Firmware ESP32-S3 N16R8 / TTP223 y pulsador mecánico | Ambas compilaciones correctas |
| Tamaño del firmware | Aproximadamente 1,33 MB de flash y 58,7 kB de RAM estática; memoria dinámica adicional para radio y reproducción |
| Pruebas C++ con sanitizadores | 20 suites, 7.151 comprobaciones correctas |
| Regresiones de hora y frases | 8.760 comparaciones con America/Santiago (2026); 100 frases verificadas |
| Protocolo de reproducción | Cabecera, dimensiones, CRC, límites, identificadores, temporizadores, fin de sesión y reproducción sin bucle comprobados |
| Compatibilidad Android/firmware | Android consume un clip MPET y páginas de texto generados por el C++ del firmware |
| Pruebas Android | 16 pruebas correctas, ninguna omitida; JUnit y Robolectric |
| HTTP de la app | Peticiones reales contra un servidor local de prueba: vinculación, autenticación y estado |
| Transferencias | Bloques, orden de offsets, finalización y cancelación comprobados con transporte de prueba |
| Imágenes y biblioteca del teléfono | Decodificación PNG de Android, conversión y guardado/lectura comprobados con Robolectric |
| Interfaz Android | Arranque y captura con gráficos nativos de Robolectric; V3.1 añade verificación de navegación y controles con Compose |
| Android Lint | Sin incidencias |
| APK Android 8.0 o superior | Compilación correcta; firma APK v2 y alineación verificadas con las herramientas de Android |
| Preparación del entorno | Scripts de instalación y construcción Android ejecutados; clave de prueba fuera del repositorio |

**Pendiente en hardware:** conexión BLE, emparejamiento con PIN, descubrimiento Wi-Fi, red propia del ESP32 en Android, rendimiento de transferencias y reproducción, decodificación de GIF/vídeo con los códecs del teléfono, persistencia tras reiniciar la placa y convivencia con sensores, clima y reloj. La ruta HTTP real del ESP32 y las radios no se han ejercitado desde un teléfono. Seguir la lista de [APP_ANDROID.md](APP_ANDROID.md#comprobación-física-recomendada).

La APK entregada es de prueba, firmada para instalación directa. Desde el 1 de noviembre de 2026, la misma APK verificada se incluye en `releases/android` junto con su SHA-256, a petición del usuario. Las claves de firma permanecen fuera de Git. La CI compila la app, ejecuta sus pruebas y verifica el SHA-256 de la APK publicada; sus resultados posteriores al push son independientes de estas comprobaciones locales.

## Antecedentes de V2

Comprobaciones de **V2** ejecutadas en el entorno de desarrollo, el 24 de octubre de 2026. Se conserva este registro de la versión anterior; el usuario confirmó el funcionamiento físico de V1.

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
