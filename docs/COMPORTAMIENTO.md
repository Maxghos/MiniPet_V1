# Comportamiento y ajustes

## Prioridades

1. Actualización Wi-Fi autorizada/en curso: código, progreso y reinicio; sin juegos ni intervenciones.
2. Configuración Wi-Fi o menú: sin gestos ni mensajes autónomos.
3. Modo estudio activo, pausado o esperando: etapa, ojos y cuenta atrás; sin gestos ni iniciativas que lo interrumpan.
4. Juego iniciado voluntariamente: el ultrasónico controla la partida; sin consulta por mantener la mano ni iniciativas autónomas.
5. Contenido solicitado desde la app: texto/media; sensores y red continúan, iniciativas visuales pausadas.
6. Consulta solicitada: posible comentario breve, clima, reacción de ~2 s, reloj y retorno.
7. Intervención autónoma: mensaje paginado o reacción ambiental y retorno a los ojos.
8. Pantalla habitual y animación.

El modo estudio se inicia explícitamente desde la app o **Ajustes → Modo estudio**. Pausar no lo cierra. No depende de la conexión ni de NTP; los avisos son visuales y no utilizan destellos. Para recuperar las funciones habituales, termina la sesión. Véase [ESTUDIO.md](ESTUDIO.md).

Los recordatorios de agenda se muestran cuando MiniPet queda libre; funcionan también con el módulo asistente deshabilitado, y respetan No molestar. No interrumpen estudio ni un gesto en curso. Las cuatro pequeñas actividades espontáneas comparten el presupuesto de intervenciones, necesitan el asistente despierto y se pueden desactivar desde Día → Resumen. Los detalles, límites y Telegram están en [FUNCIONES_ADICIONALES.md](FUNCIONES_ADICIONALES.md).

Los hábitos, las rutinas y la memoria se describen en [EXPANSION_3_3.md](EXPANSION_3_3.md). Los saludos, celebraciones e historias esperan a que el asistente esté despierto, con Mensajes activados, fuera de No molestar y dentro del descanso compartido entre iniciativas; además tienen su propia separación y opciones. Con humor 0 no se muestran esas intervenciones de compañía. Las rutinas y los juegos son solicitudes explícitas: no empiezan espontáneamente por el PIR ni sustituyen una sesión de estudio activa.

La conexión de la app, los mensajes de sesión y guardar archivos no cuentan como atención ni cambian la pantalla. Mostrar contenido/aplicar ajustes sí es una interacción. Al terminar, detener o perder la sesión durante ~15 s, MiniPet recupera su pantalla habitual y respeta una pausa antes de nuevas iniciativas. El menú físico interrumpe el contenido externo. La reproducción fijada que expiró no reaparece al reconectar. Ver [APP_ANDROID.md](APP_ANDROID.md).

Con asistente habilitado, los ojos son la pantalla habitual. Si se habilitan solo clima y reloj, se alternan automáticamente. Un único módulo permanece en su vista. Las funciones deshabilitadas no aparecen en las consultas, ni siquiera por humor alto. Si solo está habilitado el asistente, el gesto se interpreta como interacción y despierta una reacción de curiosidad/contento.

## Gesto y presencia

El HC-SR05 se muestrea cada 100 ms. Se toma una mediana de cinco medidas válidas; un eco aislado o fuera de 2–400 cm no se acepta. Para presencia hay 600 ms de confirmación de entrada, 2 s de tolerancia de salida e histéresis de 8 cm. El valor por defecto de presencia es 130 cm.

El gesto empieza dentro de la franja configurada y permite mover la mano dentro de ella. Una vez iniciado, hay 3 cm de histéresis en los bordes. Una lectura perdida o incoherente pausa el progreso; no borra lo acumulado salvo que transcurran más de 350 ms sin lecturas coherentes o se confirme la salida de la zona. Solo se suman intervalos con lecturas válidas consecutivas. Se contrasta distancia nueva y mediana para evitar que el retraso del filtro complete un gesto después de retirar la mano. Después de activar una consulta es necesario volver a salir durante al menos 1 s con distancias válidas; la falta de eco no rearma el gesto. Al arrancar se exige igualmente liberar la zona: una pared que ya estaba ahí no solicita información de inmediato.

La presencia sostenida mantiene la compañía aunque no haya movimiento. La falta de interacción se mide aparte: una mascota expresiva puede pedir atención incluso si sigues cerca. Un acercamiento brusco a menos de 45 cm puede provocar sorpresa, con al menos tres segundos entre sorpresas y sin intervenir durante menús, consultas, contenido externo o estudio. El PIR produce la secuencia alerta con fade → atento; filtra pulsos de menos de 80 ms, espera su calentamiento inicial y no identifica de qué lado viene alguien.

## Humor

Las frases aleatorias, las reacciones ambientales y los recordatorios comparten intervalos elegidos aleatoriamente dentro de estas bandas. La espera inicial adicional se aplica a los recordatorios que buscan atención:

| Humor | Intervalo normal entre intervenciones |
|---|---|
| 0–19 % | 20–40 min |
| 20–49 % | 8–15 min |
| 50–84 % | 1–3 min |
| 85–100 % | 20–45 s |

`Frecuencia = Pausada` duplica los intervalos; `Frecuente` los reduce a la mitad. Cada tres intervenciones autónomas se aplica un descanso mínimo de tres minutos. El tiempo de lectura se suma a la pausa. Dormida, dentro de No molestar o durante consulta/configuración, la mascota no inicia intervenciones. Con Mensajes desactivados se suprimen los textos; las reacciones visuales tienen su propio interruptor. Con humor bajo y un tiempo de sueño corto puede dormirse antes del primer recordatorio.

Con humor >=70 %, ignorar varios avisos permite una expresión molesta. Al interactuar se reinicia la insistencia; al volver después de una ausencia, aparece curiosidad o alegría. El humor también cambia la frecuencia de miradas y de variantes faciales. La fluidez de animación tiene la misma cadencia con cualquier porcentaje.

En consultas, a partir de 40 % pueden aparecer comentarios irónicos de 1,6 s, con probabilidad proporcional al humor y separación mínima de un minuto. Se eligen según el clima actual cuando hay datos recientes; para una consulta sin clima actual se conserva un banco genérico. Respetan Mensajes, No molestar y el módulo asistente. Se muestra la información después de la broma.

Las 100 frases aleatorias conservan el contenido del archivo aportado y evitan las tres últimas selecciones. Se centran horizontal y verticalmente por página, con 21 columnas y hasta cuatro líneas de fuente 6×8. Si una frase ocupa varias páginas, se reparten sus líneas equilibradamente y se dan entre 3 y 7 s por página. La última página queda visible hasta terminar la lectura y no vuelve al principio.

## Clima y reacciones

Con asistente y clima habilitados, `Reacción al clima` añade ~2 s entre la vista meteorológica actual/detalles y el reloj. La vista de tres días omite esta reacción para no atribuir una condición actual a todo el pronóstico. También se omite con No molestar o datos anteriores. Temperatura y código WMO permiten combinar frío con lluvia o calor con nubes. Con la misma opción activada aparecen reacciones breves ambientales mientras la mascota está despierta, dentro del presupuesto común de intervenciones.

`Avisos del clima` compara únicamente respuestas nuevas de Open-Meteo. El primer dato establece una referencia. Se anuncia el paso de seco (despejado, nubes o niebla) a lluvia, llovizna o tormenta cuando ambas actualizaciones son recientes y sus observaciones tienen horas diferentes. Una misma respuesta releída, datos anteriores, reconexiones después de más de una hora y el retorno de No molestar no generan avisos acumulados. Los avisos pendientes de menos de cinco minutos pueden esperar a terminar la consulta/menú; después se descartan.

El texto es «El clima ahora indica lluvia»: representa la estimación de Open-Meteo y puede conocerse con retraso de hasta el intervalo de actualización. El dispositivo no tiene un sensor de lluvia. Ninguno de estos comportamientos aumenta la frecuencia de consultas de red.

## Menú

| Opción | Valor inicial / rango |
|---|---|
| Asistente, reloj, clima | Los tres habilitados; al menos uno debe permanecer activo |
| Humor | 50 %, pasos de 10 hasta 100 y vuelta a 0 |
| Mensajes | Sí |
| Frecuencia | Normal; Pausada / Normal / Frecuente |
| Espera inicial | 90 s; 30–600 s |
| Dormir tras | 15 min sin compañía; 5–60 min |
| Gesto desde / hasta | 50 / 80 cm; debe existir una franja mínima de 10 cm |
| Mantener gesto | 5 s; 2–10 s |
| Presencia hasta | 130 cm; 30–350 cm |
| Tiempo pantalla | 5 s por página; 3–15 s |
| Brillo | 120; rango útil 10–250 aproximadamente |
| No molestar | Desactivado; usa el horario configurado |
| Desde / hasta hora | 22:00–08:00; ambas iguales equivalen a todo el día |
| Vista clima | Alternar: principal → detalles → tres días en consultas sucesivas |
| Frases random | Activadas; se pueden desactivar independientemente |
| Reacción al clima | Activada; consulta y ambiente |
| Avisos del clima | Activados; requieren también Mensajes |
| Destello tormenta | Desactivado; inversión breve opcional de hasta ~66 ms |
| Modo estudio | Submenú de tiempos, automatización y controles; 25/5/15 min y descanso largo cada cuatro bloques |
| Vincular app | Muestra PIN temporal; Wi-Fi local o BLE; sin router abre la red de MiniPet |
| Vincular Telegram | PIN físico temporal; requiere bot configurado por BLE e internet; solo admite un usuario privado |
| Configurar WiFi | Abre una red temporal protegida durante un máximo de 5 min |
| Restablecer | Prepara los valores iniciales; requiere Guardar y salir; conserva Wi-Fi |
| Guardar y salir | Guarda la configuración en NVS |

Editar modifica un borrador. Mantener y soltar vuelve a la lista; una pulsación de ~3 s también deja la edición. Desde la lista, ~3 s descarta el borrador y sale. Solo `Guardar y salir` persiste los ajustes. Si la hora aún no está sincronizada, el horario de No molestar no puede calcularse; desactiva Mensajes y Reacción al clima para desactivar todas las iniciativas independientemente de la hora. Al actualizar de V1 a V2 se conservan los ajustes existentes y se inicializan las cuatro opciones nuevas; no se borra Wi-Fi.

## Red y hora

Ubicación: coordenadas aproximadas de Puente Alto, `-33.6117, -70.5758`. Son una referencia meteorológica de la comuna, no una medición de tu casa. Cambiar de ciudad requiere actualizar la URL y las etiquetas del código en esta versión.

El firmware utiliza `America/Santiago` para la respuesta de Open-Meteo y la regla POSIX `CLT4CLST3,M9.1.0/0,M4.1.0/0` para la hora local. Las comparaciones de 2026 contra IANA forman parte de las pruebas. Si Chile modifica legalmente sus transiciones, habrá que actualizar las reglas y repetir la prueba para el año correspondiente. NTP se solicita al conectar y cada seis horas; el clima, cada 30 min.

La red funciona en una tarea independiente. El eco se captura por interrupciones y el dibujo se programa cada 33 ms, evitando esperas largas de conexión o medición en la animación. El servidor de configuración se atiende mientras estás en ajustes, con las animaciones habituales suspendidas.
