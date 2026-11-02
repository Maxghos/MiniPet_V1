# Modo estudio / Pomodoro · MiniPet 3.2

MiniPet organiza bloques de concentración y descansos. El temporizador vive en el **ESP32**: sigue funcionando sin la app, sin Bluetooth y sin internet mientras la placa esté encendida. No necesita componentes nuevos.

## Empezar desde la app

1. Actualiza el firmware con PlatformIO: **Build → Upload**, igual que antes. El saludo indica **MiniPet V3.2**. Se conservan Wi-Fi y ajustes previos.
2. Instala la [APK 3.2.0](../releases/android/MiniPet-3.2.0-debug.apk?raw=true) encima de la anterior. Conserva el mismo paquete y la misma firma de las APK anteriores publicadas; no hace falta desinstalar ni volver a vincular si la vinculación sigue vigente.
3. Conecta MiniPet por Wi-Fi o BLE y abre **Estudio**.
4. Elige un programa, ajusta los tiempos y pulsa **Guardar y empezar a estudiar**. También puedes guardar el programa sin iniciarlo.

| Programa | Estudio | Descanso corto | Descanso largo | Cada cuántos bloques |
|---|---|---|---|---|
| Clásico, inicial | 25 min | 5 min | 15 min | 4 |
| Suave | 15 min | 5 min | 15 min | 4 |
| Profundo | 50 min | 10 min | 20 min | 4 |

Puedes personalizar estudio de **1–180 min**, descanso corto de **1–60 min**, descanso largo de **1–120 min** y un ciclo de **2–12 bloques**. El programa queda fijo durante una sesión; termina esa sesión antes de cambiarlo.

## El ritmo lo eliges tú

Por defecto, al completar un bloque empieza el descanso automáticamente. Al terminar el descanso, MiniPet muestra **Listo para estudiar** y espera tu confirmación. Ese tiempo de espera no consume el siguiente bloque ni cuenta como estudio.

- **Empezar descansos automáticamente:** si lo desactivas, también confirmas el inicio de cada descanso.
- **Volver a estudiar automáticamente:** si lo activas, comienza el siguiente bloque al terminar el descanso.
- Tras cada cuatro bloques completados —o la cantidad configurada— corresponde el descanso largo. Después empieza un ciclo nuevo. Los ciclos se repiten hasta que termines la sesión.

La pantalla muestra etapa, bloque del ciclo, ojos animados, tiempo restante y progreso. Cada cambio de etapa tiene un aviso visual breve; no se repite constantemente. El humor puede cambiar una felicitación, pero no los tiempos ni la intensidad de las interrupciones. No hay sonidos ni destellos de pantalla en este modo.

## Pausar, continuar, saltar y terminar

- **Pausar:** congela la etapa y deja de sumar tiempo. **Reanudar** continúa el tiempo pendiente.
- **Empezar estudio/descanso:** confirma una etapa que estaba esperando. No reinicia la sesión completa.
- **Saltar etapa:** requiere confirmación. Un bloque saltado **no cuenta como completado**, pero conserva los minutos trabajados. Saltar un descanso lleva al siguiente bloque según el inicio automático elegido.
- **Terminar sesión:** requiere confirmación; devuelve la mascota a su comportamiento habitual. El resumen conserva bloques completados, tiempo de estudio y descanso. Los tiempos de pausa/espera quedan excluidos. Empezar una nueva sesión reinicia ese resumen.

Los números de **Estudio** son de la sesión actual o la última terminada. El historial de varias sesiones está en **Día → Resumen**, descrito en [Funciones adicionales](FUNCIONES_ADICIONALES.md). Ninguno mide tu atención. Durante una sesión, la app muestra una cuenta atrás basada en el último estado recibido y vuelve a consultar al ESP32 cada dos segundos mientras esté en primer plano. Si no hay una confirmación reciente, indica que hay que reconectar/sincronizar; no inventa etapas nuevas.

## Usarlo sin teléfono

El TTP223 sigue reservado al menú de configuración:

1. Abre **Ajustes → Modo estudio** y mantén aproximadamente un segundo para entrar.
2. La primera opción permite **Empezar**, **Pausar** o **Continuar**, según el estado.
3. Un toque corto avanza; mantener un segundo elige o entra/sale de edición. Mientras editas, cada toque corto incrementa el valor; al llegar al máximo vuelve al mínimo.
4. **Saltar etapa** y **Terminar sesión** piden dos pulsaciones largas: elegir y confirmar. Un toque corto cancela la confirmación.
5. **Guardar estudio** guarda el programa sin iniciar. **Empezar estudio** también aplica el programa que acabas de editar. Una pulsación de unos tres segundos vuelve al menú anterior.

Si ya hay una sesión activa, al abrir Ajustes queda seleccionada **Modo estudio** para llegar rápido a Pausar/Continuar. Abrir un menú no pausa automáticamente el temporizador. La configuración general de la mascota y el programa de estudio se guardan por separado.

## Convivencia con la mascota y la app

Mientras hay una sesión activa, también pausada o esperando confirmación, la OLED permanece dedicada al estudio. Se suspenden consultas por gesto, bromas aleatorias, solicitudes de atención y avisos meteorológicos que puedan tapar el tiempo. Sensores, red, hora y actualización del clima siguen funcionando. La sesión también funciona si deshabilitaste el módulo asistente.

Los menús físicos tienen prioridad visual. Puedes preparar y guardar archivos desde la app sin interrumpir el temporizador; para mostrar texto/media primero termina la sesión. **Volver a la mascota** en las herramientas de contenido detiene ese contenido externo, no termina Pomodoro. Usa **Terminar sesión** en Estudio.

El vencimiento de 15 s de una reproducción externa **no se aplica** al temporizador. Cerrar la app o desconectarla no lo pausa ni lo termina. Los avisos siguen en la OLED; la app muestra cambios de etapa mientras está abierta y conectada. Con tu bot vinculado, los avisos de fin de estudio y descanso llegan también a Telegram aunque nuestra app esté cerrada; requieren internet y respetan No molestar. La app MiniPet no genera alarmas propias en segundo plano.

## Reinicios y guardado

El programa, la etapa y el resumen se guardan en NVS al operar/cambiar de etapa y cada minuto de temporizador en marcha. Tras un reinicio, una etapa que estaba corriendo vuelve **pausada**; una que esperaba confirmación sigue esperando. No se cuenta el tiempo que la placa estuvo apagada. Puede recuperarse hasta un minuto menos de avance si se interrumpió la alimentación entre guardados. Si el almacenamiento falla, la app lo indica y esa recuperación no está garantizada.

No se usa NTP para contar intervalos: ajustes de hora, cambio de horario de verano o pérdida de internet no alteran el temporizador. Un retraso de ejecución avanza como máximo una etapa y empieza la siguiente con su duración completa; no se acumulan bloques ficticios.

## Prueba rápida en tus dispositivos

Configura 1 min de estudio, 1 min de descanso corto, 2 min de descanso largo y dos bloques por ciclo. Comprueba estudio → descanso → espera; pulsa Empezar estudio y verifica el descanso largo al completar el segundo bloque. Prueba Pausar, Saltar, cerrar la app y reiniciar la placa. Termina la sesión y confirma que vuelven ojos, gestos, clima y reloj habituales. Las pruebas de software se detallan en [VALIDACION.md](VALIDACION.md).
