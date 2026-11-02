# Firmware 3.1.1 · sensores y ajustes menores

Esta actualización conserva los módulos, caras, humor, Pomodoro, configuración y cableado. **Solo hay que actualizar el firmware** con Build → Upload. La APK 3.1.0 publicada sigue siendo compatible; no necesitas reinstalarla.

## Una espera de cinco segundos más cómoda

- Puedes mover la mano dentro de la franja configurada; ya no hay que mantener una profundidad casi fija respecto al primer punto detectado.
- El gesto empieza dentro de la zona, por defecto 50–80 cm. Una vez iniciado, hay un margen de 3 cm para evitar que pequeñas oscilaciones en los bordes lo corten.
- Una lectura perdida o un pico aislado puede pausar el indicador sin borrar todo el progreso. Si pasan más de 350 ms sin lecturas coherentes, el progreso se reinicia.
- Solo cuentan los intervalos confirmados: si hubo alguna lectura perdida, completar el gesto puede tardar ligeramente más de cinco segundos.
- La salida confirmada de la zona sigue cancelando el gesto. Después de una consulta se requiere al menos un segundo de distancia válida fuera de la zona para solicitar otra. La falta de eco no se interpreta como una mano retirada.
- Se contrasta la medida nueva con la mediana: una mediana antigua dentro de la zona no puede completar por sí sola el gesto cuando la mano ya salió.

## Lecturas y reacciones más estables

La captura del HC-SR05 ignora picos eléctricos muy cortos y sigue esperando el eco dentro de la misma ventana de 28 ms. Se rechazan los ecos tardíos y las medidas que llevaban demasiado tiempo pendientes de procesar. El cierre de la medición se hace dentro de una sola sección crítica.

Si ECHO ya está alto antes de medir, se informa una lectura no válida y se vuelve a intentar después; no se bloquea el bucle ni se fuerza el pin ECHO como salida. Se recupera la medición cuando ECHO vuelve a bajar. Un fallo físico que mantenga la señal permanentemente alta puede necesitar reiniciar la alimentación del sensor.

El PIR filtra pulsos cortos, espera sus 60 s iniciales de estabilización y evita interpretar un HIGH que ya existía al acabar el calentamiento como un movimiento nuevo. El calentamiento no se repite al desbordarse `millis()`.

Las sorpresas por acercamiento tienen al menos tres segundos de separación. No se acumulan durante menús, contenido externo o estudio. La confirmación de presencia tampoco puede completarse aprovechando un hueco largo sin muestras.

## Pequeñas optimizaciones y correcciones

- El texto con desplazamiento se convierte a la fuente de la OLED una sola vez por contenido, en vez de repetir esa conversión cada fotograma.
- El bucle actualiza y obtiene el estado de estudio con una sola adquisición del mutex.
- Si falla NVS, consultar el estado no fuerza reintentos de guardado en cada consulta.
- La reproducción externa toma la hora después de adquirir su mutex. Se evita que un mensaje recién recibido parezca haber vencido porque el bucle conservaba un instante anterior.
- Cambiar la franja o duración del gesto desde la app descarta el gesto en curso; no se aprovecha el progreso anterior con unos límites distintos.
- Se refresca el instante usado por los sensores después del trabajo de red/menús, y se descartan los avisos temporales ya finalizados.

## Comprobarlo en tu montaje

1. Actualiza la placa; al arrancar debe indicar **MiniPet V3.1.1**.
2. Deja una distancia válida fuera de la zona durante un segundo. Mantén la palma a unos 60–70 cm, moviéndola suavemente dentro de la zona: el indicador debe progresar sin exigir una mano inmóvil.
3. Prueba pequeñas oscilaciones cerca del borde. Un corte breve de eco debe pausar el progreso; retirar claramente la mano debe cancelarlo.
4. Tras ver clima/hora, mantener la mano ahí no debe repetir la consulta. Retírala, espera un segundo y prueba otra vez.
5. Comprueba una consulta desde la app y una sesión de estudio; al salir, vuelve a probar el gesto.

El HC-SR05 mide distancia a objetos: no distingue una mano de un mueble ni de otras personas. Conviene ajustar la zona para que el fondo no coincida con ella. Para revisar alimentación, masa común y divisor de ECHO, consulta [CONEXIONES.md](CONEXIONES.md). Las pruebas de software y los límites de validación están en [VALIDACION.md](VALIDACION.md).
