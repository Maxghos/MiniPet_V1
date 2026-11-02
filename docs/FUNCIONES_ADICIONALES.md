# Funciones adicionales · MiniPet 3.2

Esta versión añade Telegram personal, actividades del día, recordatorios, resumen diario y cuatro pequeñas actividades animadas. Conserva Pomodoro, las caras, la app de contenido y los ajustes de sensores de 3.1.1. No requiere componentes nuevos ni un servidor propio.

En la versión actual se añaden hábitos, rutinas, memoria, juegos y OTA. Esta guía sigue describiendo agenda y bot; las nuevas herramientas están en [EXPANSION_3_3.md](EXPANSION_3_3.md).

## Instalar

1. Descarga el proyecto actualizado y abre su raíz en VS Code/PlatformIO.
2. Ejecuta **Build → Upload** en `esp32s3`, o `esp32s3-button` si usas pulsador mecánico. El saludo muestra **MiniPet V3.3**.
3. Instala la [APK 3.3.0](../releases/android/MiniPet-3.3.0-debug.apk?raw=true) encima de la anterior. Mantiene paquete y certificado de firma; no hace falta desinstalar. El cableado, Wi-Fi y ajustes existentes se conservan.
4. En la app, conecta MiniPet y abre **Día**. Allí están **Actividades, Avisos, Resumen y Telegram**. Texto, imágenes y biblioteca ahora están juntos en **Enviar**.

## Actividades del día

Añade un título de hasta 96 bytes UTF-8 y deja la fecha vacía para usar el día actual de Santiago. Para otra fecha usa ocho números: `20261105` significa 5 de noviembre de 2026. Marca actividades como hechas, desmárcalas, elimínalas o pasa pendientes anteriores a hoy. El mismo título en la misma fecha se reutiliza para evitar duplicados al reintentar una solicitud.

Hay **24 actividades guardadas en total**, incluidas otras fechas. La vista inicial muestra hoy y pendientes anteriores; **Ver también otras fechas** permite gestionar el resto. No se borran tareas pendientes al cambiar de día. Las actividades no son hábitos que se reinicien automáticamente: cada una pertenece a una fecha.

La lista vive en el ESP32 y es común para la app y Telegram. El número ID identifica cada actividad en los comandos del bot.

## Recordatorios

En **Día → Avisos** escribe el título, la hora `HH:MM` de Santiago y, opcionalmente, una fecha. Si dejas la fecha vacía, se elige la próxima llegada de esa hora: hoy o mañana. Puedes repetir el aviso cada día a partir de la fecha indicada.

Hay **12 recordatorios guardados**, incluidos los puntuales que ya cumplieron su fecha. Elimínalos para liberar espacio. No se duplica una programación activa idéntica al reintentar guardarla.

Los avisos se muestran paginados en la OLED cuando no interrumpen ajustes, una consulta, contenido externo, un gesto o estudio. Pueden esperar hasta 15 minutos; después se descartan. En Telegram pueden llegar mientras la app está cerrada. La pérdida de red no detiene la agenda local.

**No molestar suprime los avisos automáticos**: no se acumulan para enviarlos al terminar el horario. Un recordatorio cuya hora pasó hace más de 15 minutos tampoco genera una ráfaga al volver a encender. El estado finalizado indica que su programación puntual ya cumplió su fecha, no que el teléfono haya confirmado una entrega.

## Resumen diario

**Día → Resumen** muestra estudio, descanso, bloques y actividades completadas, con hasta **14 días** de estadísticas. Reúne varias sesiones de Pomodoro: empezar otra sesión no borra el progreso del día. Los intervalos de pausa y espera no suman; el tiempo sin una fecha sincronizada no se asigna al calendario.

El resumen automático está activado inicialmente a las **20:30**, configurable. Refleja lo acumulado hasta ese momento; puedes consultar el resumen actualizado después. Se prepara una vez por día, con un aviso corto en la OLED y el detalle en Telegram si lo habilitaste. No molestar también lo suprime. Si se enciende más de una hora después de la hora prevista, no se envía un resumen automático atrasado. `/resumen` y la app siguen disponibles.

El tiempo se contabiliza por intervalos de aproximadamente un segundo. Las estadísticas se guardan periódicamente, hasta una vez por minuto mientras cambian, y al modificar la agenda. Un corte de alimentación puede perder el avance posterior al último guardado. Se mide tiempo del temporizador, no tu atención ni el uso del ordenador.

En 3.2.1, la agenda se separa de la biblioteca y se importa la guardada en 3.2.0. Si falla una escritura, se reintenta cada cinco segundos; se conserva la última copia válida y puede recuperarse un temporal completo tras un reinicio. La app avisa cuando se recuperó una copia o no se pudieron leer los datos. Una copia anterior puede perder los cambios más recientes. Guardar opciones sin cambios no vuelve a escribir la flash.

## Vincular tu propio Telegram

El código y la APK son genéricos: no contienen un token, una cuenta personal ni una vinculación preconfigurada.

1. En Telegram abre **[@BotFather](https://t.me/BotFather)** y usa `/newbot`. Elige nombre y usuario para tu bot personal.
2. Copia su token únicamente en **Día → Telegram → Token de BotFather** de nuestra app.
3. Para **Guardar token por BLE**, conecta MiniPet por Bluetooth BLE mediante el PIN de vinculación habitual. La app bloquea el envío por Wi-Fi y el firmware también rechaza esa configuración por HTTP.
4. MiniPet debe estar conectado a su Wi-Fi de internet, con la hora sincronizada. Pulsa **Sincronizar día y Telegram** hasta ver **Listo para vincular desde la OLED**. Puede tardar unos segundos mientras valida el bot.
5. En la mascota, abre **Ajustes → Vincular Telegram** y mantén aproximadamente un segundo. La OLED muestra un PIN temporal.
6. Abre tu bot desde el botón de la app o su enlace `t.me/NombreDeTuBot`. Pulsa Iniciar si Telegram lo pide y envíale **`/vincular PIN`**, sustituyendo PIN por las seis cifras de la OLED.
7. El bot confirma la vinculación y la mascota vuelve a su pantalla habitual. A partir de entonces usa `/ayuda` o `/estudio`.

Antes de vincular, el bot no ejecuta otros comandos. La ventana de PIN dura dos minutos y permite cinco intentos. No se permite vinculación desde grupos, mensajes reenviados ni otros bots. Una nueva vinculación física reemplaza la cuenta anterior; conocer el enlace público no da acceso a la mascota.

El token y la cuenta autorizada se guardan en NVS, fuera del repositorio. Las consultas de estado nunca devuelven token, PIN ni identificadores de cuenta. El campo de token se vacía tras enviarlo y al dejar la app en segundo plano. Como en el resto de la configuración actual, NVS no está cifrada contra lectura física de la flash. El tráfico hacia Telegram usa HTTPS y un conjunto de CA públicas verificable; no se usa `setInsecure`.

Puedes desactivar Telegram, elegir avisos de Pomodoro/recordatorios/resumen o **Borrar token y vinculación** desde la app. Borrar esos datos de MiniPet no elimina el bot en Telegram. Si el token se filtró, revócalo desde BotFather y configura el nuevo; reemplazarlo obliga a repetir el PIN físico.

## Comandos del bot

| Comando | Acción |
|---|---|
| `/ayuda` o `/start` | Lista de comandos, una vez vinculado |
| `/estudio` o `/estado` | Etapa, cuenta atrás y botones actualizados |
| `/pomodoro` | Empieza el programa guardado en MiniPet |
| `/pausa` / `/continuar` | Pausa o reanuda la etapa |
| `/saltar` / `/terminar` | Solicita confirmación con botones |
| `/actividad Repasar álgebra` | Añade una actividad para hoy |
| `/actividades` o `/hoy` | Lista actividades de hoy y anteriores con sus ID |
| `/hecho 3` / `/pendiente 3` | Marca o desmarca la actividad 3 |
| `/hoy 3` | Pasa una actividad pendiente a hoy |
| `/recordar 18:30 Leer` | Crea un aviso puntual para la próxima llegada de esa hora |
| `/diario 09:00 Preparar el día` | Crea un aviso diario |
| `/avisos` / `/borraraviso 2` | Lista o elimina recordatorios |
| `/resumen` | Consulta el resumen de hoy |
| `/habitos` | Hábitos, ID, progreso semanal y racha |
| `/habito 4 Leer` | Crea el hábito Leer con meta de cuatro días por semana |
| `/cumpli 3` / `/desmarcarhabito 3` | Marca o corrige el hábito 3 para hoy |
| `/rutinas` / `/rutina 2` | Lista rutinas o ejecuta la rutina 2 |
| `/estudiar` / `/dia` | Ejecuta los accesos a las rutinas iniciales 1 y 2, si siguen existiendo |
| `/compania` | Consulta nombre y memoria pública de la mascota |
| `/jugar punteria` / `/jugar esquivar` | Inicia voluntariamente uno de los juegos |
| `/salirjuego` | Termina la partida |

Los botones de Pomodoro llevan la revisión de estado: si la etapa cambió mientras decidías, se rechaza la acción antigua y puedes pedir `/estudio` otra vez. No se duplican temporizadores entre Telegram y la app. Los menús físicos mantienen prioridad: desde Telegram se puede pausar o terminar, pero hay que cerrarlos para empezar, continuar o saltar.

El bot funciona directamente desde el ESP32 mediante consultas HTTPS en segundo plano. Al reiniciar, cambiar la vinculación o detectar la reconexión de Wi-Fi **se descartan las órdenes pendientes anteriores**; espera a que MiniPet esté disponible y reenvía la orden. Un error transitorio de la API mantiene la posición de lectura: las órdenes de texto siguen necesitando menos de dos minutos de antigüedad. Las solicitudes repetidas no se ejecutan dos veces por el mismo identificador; se admite el reinicio de secuencia que Telegram permite tras una semana sin actualizaciones. Las esperas por 429 se aplican a todas las peticiones del bot, incluidos envíos y respuestas a botones.

Los avisos automáticos tienen cola acotada, vencimiento y un máximo de tres intentos: Pomodoro vence a los dos minutos, recordatorios a los 15 minutos y resumen a la hora. Se vuelven a comprobar silencio, preferencias y caducidad antes de enviarlos. La red no garantiza entrega exacta ni inmediata; un fallo después de aceptar un mensaje puede producir una repetición al reintentar.

## Pequeñas actividades espontáneas

MiniPet puede hojear un libro, respirar suavemente, estirarse o mirar una estrella. Son escenas originales de unos 2,2 segundos, dibujadas por el motor nativo para 128×32. Comparten el presupuesto de intervenciones existente: humor bajo las hace ocasionales; humor alto permite más variedad. Respetan No molestar, el sueño, los módulos habilitados y los gestos. No aparecen durante Pomodoro, menús o contenido externo.

Se pueden desactivar en **Día → Resumen → Pequeñas actividades espontáneas → Guardar opciones del día**. No requieren Mensajes activados porque son animaciones visuales.

## Solución de problemas y prueba física

- **Telegram no está listo:** comprueba Wi-Fi con internet y la hora. La red temporal de MiniPet sin router permite configurar, pero no tiene internet por sí misma.
- **Código 401:** token incorrecto o revocado; vuelve a guardarlo por BLE.
- **Código 409:** otro programa o un webhook está usando ese bot. Dedícale un bot a MiniPet y detén el otro consumidor; el firmware no elimina webhooks ajenos automáticamente.
- **Código 429:** Telegram pidió esperar. Se respeta la espera; evita repetir órdenes continuamente.
- **No responde al PIN:** sincroniza el estado, vuelve a abrir Vincular Telegram y envía el nuevo PIN desde un chat privado. Los mensajes antiguos o reenviados se ignoran.
- **Hora no disponible:** agenda y resumen necesitan una fecha válida. Pomodoro conserva su temporizador independiente de internet.

Prueba una actividad desde la app, márcala por Telegram y sincroniza. Programa un aviso para dentro de dos minutos y una sesión de estudio corta; cierra nuestra app y comprueba las notificaciones de Telegram. Ensaya No molestar, reinicio, una cuenta no vinculada y un botón de una etapa anterior. Verifica que los sensores y los gestos siguen funcionando. Las pruebas locales y sus límites figuran en [VALIDACION.md](VALIDACION.md).
