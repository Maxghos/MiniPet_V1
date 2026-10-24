# Comportamiento y ajustes

## Prioridades

1. Configuración Wi-Fi o menú: sin gestos ni mensajes autónomos.
2. Consulta solicitada: posible comentario breve, clima, reacción de ~2 s, reloj y retorno.
3. Intervención autónoma: mensaje paginado o reacción ambiental y retorno a los ojos.
4. Pantalla habitual y animación.

Con asistente habilitado, los ojos son la pantalla habitual. Si se habilitan solo clima y reloj, se alternan automáticamente. Un único módulo permanece en su vista. Las funciones deshabilitadas no aparecen en las consultas, ni siquiera por humor alto. Si solo está habilitado el asistente, el gesto se interpreta como interacción y despierta una reacción de curiosidad/contento.

## Gesto y presencia

El HC-SR05 se muestrea cada 100 ms. Se toma una mediana de cinco medidas válidas; un eco aislado o fuera de 2–400 cm no se acepta. Para presencia hay 600 ms de confirmación de entrada, 2 s de tolerancia de salida e histéresis de 8 cm. El valor por defecto de presencia es 130 cm.

El gesto exige muestras continuas en la franja configurada y tolera variaciones de hasta 10 cm respecto al inicio de la espera. Salir de la franja, perder el eco o dejar de recibir muestras reinicia la espera. Después de activar una consulta es necesario volver a salir durante al menos 1 s. Al arrancar se exige igualmente liberar la zona: una pared que ya estaba ahí no solicita información de inmediato.

La presencia sostenida mantiene la compañía aunque no haya movimiento. La falta de interacción se mide aparte: una mascota expresiva puede pedir atención incluso si sigues cerca. Un acercamiento brusco a menos de 45 cm puede provocar sorpresa. El PIR produce la secuencia alerta con fade → atento; no identifica de qué lado viene alguien.

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
| Configurar WiFi | Abre una red temporal protegida durante un máximo de 5 min |
| Restablecer | Prepara los valores iniciales; requiere Guardar y salir; conserva Wi-Fi |
| Guardar y salir | Guarda la configuración en NVS |

Editar modifica un borrador. Mantener y soltar vuelve a la lista; una pulsación de ~3 s también deja la edición. Desde la lista, ~3 s descarta el borrador y sale. Solo `Guardar y salir` persiste los ajustes. Si la hora aún no está sincronizada, el horario de No molestar no puede calcularse; desactiva Mensajes y Reacción al clima para desactivar todas las iniciativas independientemente de la hora. Al actualizar de V1 a V2 se conservan los ajustes existentes y se inicializan las cuatro opciones nuevas; no se borra Wi-Fi.

## Red y hora

Ubicación: coordenadas aproximadas de Puente Alto, `-33.6117, -70.5758`. Son una referencia meteorológica de la comuna, no una medición de tu casa. Cambiar de ciudad requiere actualizar la URL y las etiquetas del código en esta versión.

El firmware utiliza `America/Santiago` para la respuesta de Open-Meteo y la regla POSIX `CLT4CLST3,M9.1.0/0,M4.1.0/0` para la hora local. Las comparaciones de 2026 contra IANA forman parte de las pruebas. Si Chile modifica legalmente sus transiciones, habrá que actualizar las reglas y repetir la prueba para el año correspondiente. NTP se solicita al conectar y cada seis horas; el clima, cada 30 min.

La red funciona en una tarea independiente. El eco se captura por interrupciones y el dibujo se programa cada 33 ms, evitando esperas largas de conexión o medición en la animación. El servidor de configuración se atiende mientras estás en ajustes, con las animaciones habituales suspendidas.
