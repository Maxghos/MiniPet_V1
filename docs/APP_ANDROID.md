# MiniPet 3.2: app Android, agenda, Telegram y estudio

## 1. Actualizar MiniPet

Descarga el proyecto completo actualizado, abre su raíz en VS Code/PlatformIO y ejecuta **esp32s3 → General → Build**, después **Upload**. Para un pulsador mecánico usa `esp32s3-button`. El saludo dice **MiniPet V3.2.1**; el menú incluye **Vincular app**, **Modo estudio** y **Vincular Telegram**. Conserva los ajustes de sensores de 3.1.1.

El cableado es el mismo. La configuración y el Wi-Fi de V1/V2 se conservan. No necesitas borrar la flash. En el primer arranque se inicializa la partición de archivos que las versiones anteriores de MiniPet no utilizaban; allí se guarda la nueva biblioteca, separada de los ajustes NVS.

## 2. Instalar la APK

**[Descarga la APK MiniPet 3.2.1](../releases/android/MiniPet-3.2.1-debug.apk?raw=true)** y ábrela en tu teléfono Android 8.0 o superior. Autoriza la instalación desde tu navegador/gestor de archivos si Android lo solicita. No necesitas Android Studio ni Expo. Puedes instalarla sobre las APK anteriores publicadas: mantiene paquete y firma. Esta APK es una compilación de prueba firmada, disponible en el repositorio; no se publica en una tienda. Su SHA-256 está en la misma carpeta `releases/android`.

## 3. Vincular por Wi-Fi (recomendado)

1. Pon el teléfono en la misma red que MiniPet.
2. En la mascota, toca el TTP223, avanza hasta **Vincular app**, mantén ~1 s y suelta.
3. La OLED muestra un **PIN de seis cifras**, válido dos minutos, y su IP.
4. En la app, escribe el PIN y pulsa **Buscar Wi-Fi**. Elige tu MiniPet. Si no aparece, introduce la IP de la OLED y pulsa **Conectar por Wi-Fi**. El puerto es `8080` y se añade automáticamente si lo omites.
5. Al vincularse, la mascota vuelve a funcionar normalmente. Abrir la app o mantenerla conectada no cambia su pantalla ni cuenta como prestarle atención.

En conexiones posteriores no hace falta PIN mientras la vinculación siga vigente. Una nueva vinculación reemplaza la anterior. Si borraste la memoria del ESP32 o reinstalaste la app, vuelve a vincular físicamente.

### Sin router

Si MiniPet no está conectado a una red, **Vincular app** abre su propia Wi-Fi. La OLED muestra SSID, contraseña y PIN. Conecta el teléfono a esa red, mantén la conexión aunque Android avise que no tiene internet y vuelve a la app. La dirección es `192.168.4.1:8080`.

Después de vincularte, esa conexión no mantiene la OLED en una pantalla de configuración. El punto de acceso se conserva mientras haya comunicación autenticada y termina después de unos cinco minutos de inactividad. La falta de internet afecta las actualizaciones meteorológicas/NTP, pero no el contenido local ni la mascota.

## 4. Vincular por Bluetooth BLE

1. Abre **Vincular app** en la OLED.
2. En Android activa Bluetooth, concede Dispositivos cercanos y pulsa **Buscar BLE**.
3. Introduce el PIN de la OLED en la app y selecciona MiniPet. Android puede pedir el mismo PIN para vincular Bluetooth.
4. En Android 11 y anteriores también se requieren permiso de ubicación y ubicación activada para buscar BLE. La aplicación no lee coordenadas.

Wi-Fi es más rápido para clips. BLE permite las mismas órdenes y transferencias por bloques, con más espera. Si Android conserva una vinculación Bluetooth vieja tras borrar el ESP32, olvida esa vinculación en los ajustes de Bluetooth y repite el proceso.

## 5. Mostrar o guardar

- **Estudio:** concentración y descansos, programas personalizados, pausa/reanudación y resumen; sigue sin conexión. [Guía completa de Pomodoro](ESTUDIO.md).
- **Día:** actividades, recordatorios, resumen diario y configuración de Telegram. [Guía de funciones adicionales y BotFather](FUNCIONES_ADICIONALES.md).
- **Enviar → Texto:** escribe el mensaje; elige páginas centradas o desplazamiento y revisa la vista previa. «Mostrar mensaje» lo presenta completo; puedes fijarlo mientras la app siga comunicándose.
- **Enviar → Imagen / GIF / vídeo:** elige el archivo. Ajusta encuadre, zoom, giro, contraste y tramado. Pulsa **Preparar vista previa** después de cambiar valores; solo se envía el resultado preparado.
- **Mostrar ahora:** transfiere y reproduce el contenido.
- **Guardar en MiniPet sin mostrar:** transfiere a la biblioteca sin interrumpir la mascota. El teléfono conserva una copia convertida para reenviarla.
- **Enviar → Biblioteca:** permite volver a mostrar o eliminar archivos de cada dispositivo por separado.
- **Volver a MiniPet:** detiene el contenido externo. El botón físico también puede interrumpirlo para abrir ajustes.

Mientras hay contenido externo se mantienen las lecturas de sensores, pero se pausan sus consultas visuales y las iniciativas para no taparlo. El menú físico tiene prioridad: ciérralo antes de mostrar contenido o aplicar ajustes desde la app. Guardar archivos y conectarse no generan atención artificial.

Con firmware anterior a 3.1, la app envía señales de sesión cada cinco segundos mientras está en primer plano. Si deja de comunicarse, el contenido externo termina en unos **15 s** y MiniPet recupera su funcionamiento autónomo. Un contenido fijado que expiró no reaparece al reconectar: hay que pedir «Mostrar» otra vez.

Con firmware 3.1 se consulta el estudio cada dos segundos y esa consulta también mantiene la comunicación. El vencimiento del contenido externo no se aplica a Pomodoro: continúa al cerrar la app. Durante estudio puedes preparar/guardar archivos; termina la sesión antes de mostrar texto/media. El botón de volver a la mascota en las herramientas de contenido no termina una sesión de estudio.

## Límites de esta entrega

| Función | Límite |
|---|---|
| Pantalla | 128×32, un bit por píxel, proporción 4:1 |
| Vídeo/GIF convertido | Hasta 30 s y 15 fps, 450 cuadros |
| Archivo convertido | 230.424 bytes como máximo |
| Biblioteca del ESP32 | Ocho archivos y presupuesto de 2 MiB; requiere almacenamiento disponible |
| Biblioteca del teléfono | 100 archivos convertidos |
| Texto | Hasta 4.096 bytes UTF-8; español y fuente clásica; otros símbolos pueden aparecer como `?` |
| Audio | No se transmite/reproduce |
| Red | Misma LAN, Wi-Fi propia del ESP32 o BLE; sin control remoto por internet |
| Formatos originales | Los que decodifica Android; GIF de hasta 8 MiB y dimensiones acotadas, vídeo según los códecs del teléfono |

Las imágenes transparentes se componen sobre negro. Las fotografías pierden detalle; iconos, dibujos y ojos suelen ser más legibles. El tramado simula tonos. La biblioteca guarda el contenido convertido, no el vídeo original. El rendimiento de radio y pantalla necesita comprobarse en tus dispositivos.

## Comprobación física recomendada

1. Conecta la app sin enviar nada: deben seguir funcionando ojos, humor, PIR, gesto, reloj y clima.
2. Envía un texto con acentos y uno largo; revisa centrado, páginas y desplazamiento.
3. Prepara una imagen, guárdala sin mostrar y comprueba que no cambia la pantalla. Después muéstrala desde la biblioteca.
4. Prueba un GIF/vídeo corto primero por Wi-Fi, después BLE; cancela una carga y vuelve a intentarla.
5. Fija un contenido y desconecta/cierra la app: debe regresar a la mascota y no reaparecer al reconectar.
6. Cambia humor/brillo, cierra la app y reinicia MiniPet: deben conservarse. Confirma que el botón físico sigue reservado a ajustes.

Las pruebas automáticas y sus límites se documentan en [VALIDACION.md](VALIDACION.md).
