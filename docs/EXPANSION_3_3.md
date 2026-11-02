# Expansión MiniPet 3.3

Esta versión añade hábitos y metas semanales, rutinas, memoria de la mascota, minijuegos con el ultrasónico y actualización por Wi-Fi. Usa el mismo ESP32-S3 N16R8, OLED 128×32, PIR, HC-SR05 y botón.

## Instalar por primera vez

1. Descarga el repositorio completo y abre su raíz en VS Code/PlatformIO.
2. Ejecuta **Build → Upload** con `esp32s3` para TTP223 o `esp32s3-button` para pulsador mecánico. Esta primera carga de 3.3 debe hacerse por USB: el firmware anterior todavía no dispone del actualizador nuevo.
3. Instala la [APK 3.3.0](../releases/android/MiniPet-3.3.0-debug.apk?raw=true) encima de la anterior. Mantiene paquete y firma; no hace falta desinstalarla.
4. Conecta la app por Wi-Fi o BLE. **Día** tiene dos filas de pestañas: Actividades/Avisos/Resumen/Telegram y Hábitos/Rutinas/Compañía/Juegos. Cada fila puede desplazarse si usas una fuente grande. La actualización de firmware se encuentra en **Ajustes** y utiliza Wi-Fi.

No borres la flash para actualizar. Los ajustes, el Wi-Fi, el bot y la agenda se conservan; el registro de agenda acepta los datos anteriores y añade los campos nuevos. Un fallo físico de almacenamiento puede impedir recuperar cambios aún no guardados.

## Hábitos y metas semanales

Un hábito se repite; una actividad de la agenda pertenece a una fecha concreta. Usa, por ejemplo, la actividad «Preparar examen del viernes» y el hábito «Leer» con una meta de cuatro días por semana.

En **Día → Hábitos**, escribe el título, elige una meta entre **1 y 7 días por semana** y crea el hábito. Hay hasta **ocho hábitos**. Marca el día como hecho cuando corresponda; desmarcarlo corrige un toque accidental. Marcarlo dos veces no duplica el progreso. Eliminar un hábito pide confirmación.

El calendario utiliza la fecha de **Santiago**. La semana va de lunes a domingo y el progreso cuenta días distintos, no la cantidad de veces que pulses. Las rachas y el avance se calculan con un historial acotado; no se mide si realmente hiciste la actividad ni se vigila el teléfono u ordenador. Necesitas una fecha sincronizada para registrar un día.

La racha cuenta días consecutivos completados. Si todavía no hiciste el hábito hoy, se conserva la racha de ayer para que puedas continuarlo. La meta semanal y la racha diaria son medidas distintas: cumplir cuatro días de siete puede alcanzar tu meta aunque no formen una racha. Se conservan hasta 56 fechas por hábito y se compacta la continuidad anterior para mantener rachas más largas; no se aplican castigos ni puntos negativos por un día sin marcar.

La lista es la misma en la app y en tu bot vinculado. Consultarla no despierta intervenciones ni cuenta como atención a la mascota. Los hábitos complementan la lista de actividades: no ocupan sus 24 espacios.

## Rutinas editables

Una rutina prepara varias cosas mediante una sola orden. Puedes guardar hasta **seis rutinas**, cada una con título y hasta **seis actividades** para hoy, y elegir estas acciones:

| Opción | Resultado |
|---|---|
| Iniciar Pomodoro guardado | Inicia el programa de Pomodoro que ya guardaste en MiniPet |
| Mostrar clima y hora | Muestra clima y hora para esta consulta, sin cambiar los módulos de la pantalla habitual |
| Actividades | Añade los títulos a hoy; reutiliza actividades con igual título y fecha |

Configúralas desde **Día → Rutinas**. Puedes crear, editar, ejecutar o eliminar; la eliminación pide confirmación. Una rutina no modifica permanentemente el humor, los horarios de No molestar ni el programa de estudio.

Iniciar Pomodoro guardado y Mostrar clima y hora son alternativas: elige una o ninguna. Las rutinas iniciales son **Estudiar** y **Comenzar el día**; se pueden editar o eliminar como las demás.

Para una rutina de concentración puedes preparar «Abrir apuntes», «Repasar álgebra» y «Resolver ejercicios» y activar Empezar estudio. Para comenzar el día puedes preparar una lista breve y activar Mostrar panorama. El estudio conserva prioridad sobre las pantallas informativas.

Una sesión activa debe terminar antes de ejecutar otra rutina que entre en conflicto con ella. El menú físico y la actualización de firmware también tienen prioridad. Si falta hora válida o espacio para las actividades, corrige el problema y vuelve a ejecutar; no se usa el inicio de una rutina para inventar una fecha o completar trabajo automáticamente.

## Nombre y memoria de la mascota

En **Día → Compañía** puedes cambiar el nombre de MiniPet y el apodo con que se dirige a ti, y activar saludos, celebraciones e historias por separado. Nombre y apodo admiten hasta 24 bytes UTF-8; el apodo puede quedar vacío. Son preferencias del dispositivo: no se publican en GitHub ni se mandan a un servicio de IA.

La mascota conserva avances y pequeños recuerdos para variar sus saludos, reconocer logros y continuar historias breves entre días. Son textos y reglas locales del proyecto; no es una conversación generada por una IA en internet.

Al actualizar desde una versión anterior se aprovechan las actividades que todavía figuran como hechas y los bloques conservados en los últimos 14 registros de día. No se reconstruye un historial completo de versiones anteriores: los recuerdos nuevos y los días acompañados empiezan a registrarse con esta función.

Los días acompañados cuentan fechas distintas en que MiniPet estuvo encendido con hora válida, no todos los días transcurridos desde la instalación ni una detección de tu presencia. El saludo se limita a uno por día entre las 06:00 y las 23:00. Una meta de hábito se celebra como máximo una vez en su semana; también hay hitos de avances acumulados. Las historias tienen seis capítulos y como máximo uno por día, desde el mediodía y con humor de al menos 40 %. Las intervenciones esperan al menos 45 s tras iniciar el servicio y mantienen pausas mínimas según el humor: 15 min con 1–39 %, 6 min con 40–69 % y 3 min con 70–100 %. Con humor 0 no hay intervenciones de compañía.

Las intervenciones autónomas respetan el humor, Mensajes, No molestar, el sueño y las pantallas prioritarias. No se muestran encima de Pomodoro, un juego, una consulta, un menú, contenido externo o una actualización. Un porcentaje alto aporta más iniciativa; no elimina los descansos entre intervenciones. Las opciones de compañía permiten reducirla sin borrar tus hábitos.

## Minijuegos con la mano

Los juegos se inician voluntariamente desde **Día → Juegos** o desde sus opciones en el menú físico. El TTP223/pulsador sigue reservado al menú: durante una partida sirve para salir y volver a los ajustes. No se inicia un juego por un movimiento casual del PIR.

Coloca la mano abierta frente al HC-SR05, aproximadamente entre **15 y 80 cm**, con espacio libre alrededor. El sensor mide una distancia; no identifica dedos ni la dirección lateral de la mano. Moverte a izquierda/derecha sin variar la distancia no equivale a un control distinto.

| Juego | Cómo jugar |
|---|---|
| **Puntería** | Acerca o aleja la mano hasta la distancia objetivo y sostenla para sumar; después aparece otro objetivo |
| **Esquivar** | Acerca o aleja la mano para mover el personaje en vertical y evitar los obstáculos |

La partida incluye una cuenta inicial de **3 s**, **45 s de juego con lecturas válidas** y un resultado de **6 s**. Puntería suma un punto al sostener la distancia objetivo durante 800 ms dentro de un margen de ±5 cm; el siguiente objetivo cambia. Esquivar tiene tres vidas y suma un punto por pared superada. Mano más cerca mueve el personaje hacia abajo; más lejos, hacia arriba.

Las lecturas inválidas no suman puntos ni deben convertirse en posiciones extremas. Si se pierde la mano, se pausa el avance; **más de cinco segundos sin lectura válida termina la partida** con una indicación. **Terminar juego** en la app también devuelve el control a la mascota. No se inicia una partida encima de una sesión de estudio activa.

El comportamiento de los ecos depende del sensor, la alimentación y la forma de la mano. Empieza con movimientos lentos; no cambies el divisor de tensión de ECHO ni el cableado para usar esta función. Los detalles eléctricos están en [CONEXIONES.md](CONEXIONES.md).

## Actualizar por Wi-Fi

Necesitas haber instalado 3.3 una primera vez por USB. A partir de ahí, una actualización compatible puede cargarse desde Android sin abrir PlatformIO.

1. Descarga el paquete que corresponda: [TTP223](../releases/firmware/MiniPet-3.3.0-touch.mpfw?raw=true) o [pulsador mecánico](../releases/firmware/MiniPet-3.3.0-button.mpfw?raw=true). Conserva la extensión **`.mpfw`**. No elijas el ZIP del repositorio, una APK ni un `bootloader.bin`.
2. Conecta MiniPet y la app a la misma Wi-Fi. La transferencia necesita una conexión Wi-Fi real de la app; seleccionar Buscar Wi-Fi mientras sigues conectado por BLE no cambia el transporte activo.
3. Termina cualquier sesión de Pomodoro. En la mascota abre **Ajustes → Actualizar WiFi** y mantén aproximadamente un segundo. La OLED muestra un código temporal de seis cifras; la autorización dura dos minutos.
4. En **Ajustes** de la app abre la actualización de firmware, selecciona el `.mpfw`, introduce ese código y confirma la carga. La credencial de vinculación de la app también se exige; conocer la IP no basta.
5. Mantén la alimentación y la conexión durante la transferencia. La pantalla muestra el progreso. Después de una carga válida, MiniPet confirma la operación y se reinicia al cabo de unos segundos.
6. Espera a que arranque y vuelve a conectar la app. Comprueba la versión y que tus ajustes, agenda y biblioteca estén disponibles.

Los paquetes llevan destino, versión, tamaño y SHA-256 de la imagen. Tanto el cliente como el firmware comprueban el formato; la placa verifica compatibilidad e integridad antes de elegir la nueva partición de arranque. TTP223 y pulsador son destinos diferentes: un paquete para el otro botón se rechaza.

La transferencia escribe en la partición de aplicación inactiva. Un archivo incompleto o rechazado no se activa; puedes volver a abrir la autorización y reintentar. Esto **no equivale a recuperación automática si un firmware válido arranca pero tiene un fallo funcional**: no se promete rollback de arranque. Conserva el cable USB como vía de recuperación. No desconectes la alimentación durante una actualización.

Los SHA-256 detectan corrupción y permiten comparar el archivo publicado; los paquetes no tienen una firma criptográfica de editor. La autorización física y la credencial protegen el acceso al actualizador. El servicio es HTTP en la LAN de confianza; no abras el puerto 8080 a internet. OTA no cambia bootloader, particiones, NVS ni LittleFS y no utiliza un servidor de actualización externo.

### Crear un paquete propio

Después de compilar el proyecto:

```sh
python3 tools/package_firmware.py --image .pio/build/esp32s3/firmware.bin --target minipet-s3-n16r8-touch --version 3.3.0 --output releases/firmware/MiniPet-3.3.0-touch.mpfw
python3 tools/package_firmware.py --image .pio/build/esp32s3-button/firmware.bin --target minipet-s3-n16r8-button --version 3.3.0 --output releases/firmware/MiniPet-3.3.0-button.mpfw
python3 tools/package_firmware.py --check releases/firmware/MiniPet-3.3.0-touch.mpfw
```

Se genera también `<paquete>.sha256`. Para una primera carga se utiliza PlatformIO/USB; el paquete contiene solo la imagen de aplicación y su cabecera.

## Comprobación en tus dispositivos

Prueba un hábito y corrígelo, confirma que el bot ve el cambio, ejecuta una rutina pequeña y revisa que repetirla no añade las mismas actividades otra vez. Configura humor bajo y No molestar y comprueba que saludos/historias esperan su turno. Juega acercando y retirando la mano y verifica que no se puntúa sin eco.

Para OTA, empieza con el paquete de la variante correcta y una alimentación estable. Confirma que un código incorrecto y el paquete del otro botón se rechazan, después prueba una actualización válida y reconecta. Verifica que se conserva el estado. La prueba de interrupción requiere poder recuperar por USB; no es necesario cortar la alimentación de tu montaje para instalar esta versión.

Las pruebas automáticas verifican lógica, contratos y compilaciones; no sustituyen los ecos reales, la radio, los cortes físicos de flash ni el arranque de una imagen recibida por OTA. Los resultados y los límites de verificación están en [VALIDACION.md](VALIDACION.md).
