# Cambios V2 Adiciones

## Ojos y personalidad

- Se pasa de ocho estados a **11**, añadiendo irónico, ofendido y entusiasmado.
- **19 estilos geométricos diferenciados**: cuatro tranquilos, dos contentos abiertos, contento con ojos cerrados en arco, curioso, atento, tres sorpresas, cansado, dormido, irónico de la referencia azul, entrecerrado de la violeta, pícaro de la naranja, enfado con ojos cerrados y puchero.
- Cada estilo cambia contornos, pupilas, cejas, pestañas o cierres. Se mantienen interpolación, parpadeos y miradas; la selección sigue el contexto y el humor.
- Símbolo de enfado en la variante cerrada con humor alto. Las referencias de color se interpretan en el único color de la SSD1306.

## Frases

- Se incorporan las **100 frases** aportadas en `Frases Random de vez en cuando.txt`, conservando su texto; los encabezados de categorías no se muestran.
- Frases aleatorias, recordatorios y reacciones ambientales usan un presupuesto común de intervenciones según el humor. Hay un descanso mínimo de tres minutos después de tres intervenciones autónomas.
- Los mensajes están **centrados horizontal y verticalmente**. Se muestran acentos, ñ, ¿ y ¡. Los textos largos pasan completos por páginas de hasta cuatro líneas, repartidas de forma equilibrada, durante 3–7 s por página según su longitud. Las palabras muy largas se separan en líneas sin perder caracteres.
- Se evitan las tres últimas frases del banco correspondiente. El menú permite desactivar las aleatorias sin desactivar los demás mensajes.

## Reacción al clima: tres niveles

1. **Consulta:** broma contextual de 1,6 s cuando corresponde por humor; clima; reacción de ~2 s; reloj. Se omite la reacción para pronóstico de varios días, datos anteriores, asistente desactivado o No molestar.
2. **Ambiente:** apariciones de ~2 s mientras la mascota está despierta, compartiendo pausas con frases y recordatorios. Calor: párpados bajos y sudor; frío: temblor de un píxel; lluvia: mirada hacia arriba y gotas; nieve: copos; tormenta: sorpresa y relámpago. Sol agradable: ojos contentos; nubes/niebla: ojos cansados. Temperatura y condición pueden combinarse.
3. **Cambio:** aviso «El clima ahora indica lluvia» al comparar dos actualizaciones nuevas, recientes y válidas que pasan de seco a lluvia/llovizna/tormenta. El primer dato establece referencia. Releer el mismo dato, reiniciar, reconectar tras datos viejos o salir de No molestar no genera un aviso acumulado.

Se usan los mismos datos y la misma consulta de Open-Meteo cada 30 min. El cambio puede conocerse con retraso y es una estimación del proveedor. No equivale a detectar lluvia con un sensor físico.

## Ajustes y compatibilidad

- Cuatro opciones nuevas: **Frases random**, **Reacción al clima**, **Avisos del clima**, **Destello tormenta**. Las tres primeras comienzan activadas; el destello, desactivado.
- La inversión opcional de tormenta dura como máximo unos 66 ms. Se realiza sobre el cuadro dibujado, así que no deja invertida la pantalla si abres ajustes o llega otra pantalla.
- Se migran los valores persistentes de V1; Wi-Fi usa el mismo almacenamiento. No requiere cambiar pines, componentes ni paquetes de PlatformIO.
- Las consultas intencionales y el menú interrumpen los mensajes ambientales. La mascota dormida y No molestar suprimen las iniciativas; los cambios de clima no se acumulan para mostrarse después.

## Comprobaciones

Pruebas de memoria, gestos y menú ampliadas con migración V1→V2, las 100 frases completas, centrado, paginación, orden de consulta, presupuesto compartido, datos viejos/repetidos, avisos y los 19 estilos. `docs/caras.html` incluye **38 secuencias** producidas por el motor nativo: 11 estados, 19 estilos y ocho casos meteorológicos. Ver resultados y límites en [VALIDACION.md](VALIDACION.md).

Para actualizar, descarga/actualiza el proyecto completo, abre su carpeta en VS Code y usa **PlatformIO → PROJECT TASKS → esp32s3 → General → Build**, después **Upload**. Usa `esp32s3-button` solo si tienes un pulsador mecánico. No hace falta borrar memoria ni volver a introducir Wi-Fi.
