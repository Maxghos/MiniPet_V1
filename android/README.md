# MiniPet para Android

Aplicación nativa en Kotlin y Jetpack Compose, Android **8.0/API 26 o posterior**. Wi-Fi local y BLE cifrado comparten el protocolo documentado en [PROTOCOLO_APP.md](../docs/PROTOCOLO_APP.md).

**[Descargar APK MiniPet 3.0.0](../releases/android/MiniPet-3.0.0-debug.apk?raw=true)**. La copia verificada está en `releases/android` junto con su SHA-256. **Las demás APK/AAB, las claves de firma y los directorios de compilación están excluidos de Git.** Es una compilación de prueba firmada, paquete `cl.maxghos.minipet`; no es una publicación en Google Play.

## Compilar en Android Studio

1. Descarga/clona el repositorio completo y abre **la carpeta `android`** en Android Studio.
2. Usa JDK 17 o 21, SDK Android 35 y acepta la instalación de las herramientas que propone Gradle.
3. Sincroniza el proyecto. El wrapper fija Gradle 8.9 con su SHA-256; AGP es 8.7.3 y Kotlin 2.0.21.
4. Ejecuta `app` sobre tu teléfono o **Build APK(s)** para crear una APK de prueba.

CLI, con JDK/SDK configurados:

```sh
cd android
./gradlew testDebugUnitTest lintDebug assembleDebug
```

En Windows usa `gradlew.bat`. La APK aparece en `app/build/outputs/apk/debug/app-debug.apk`, ignorada por Git. Una compilación propia puede tener otra clave de firma y no actualizar directamente una APK firmada con otra clave; para distribución permanente configura tu propia firma y consérvala fuera de Git.

## Pantallas

- **Conectar:** buscar por Wi-Fi/mDNS, escribir una IP local o buscar por BLE; vincular con el PIN de la OLED.
- **Texto:** páginas centradas o desplazamiento, velocidad, vista previa con la misma fuente y opción de mantener mientras haya sesión.
- **Media:** importar imagen, GIF o vídeo; encuadre, zoom, giro, contraste, umbral, inversión y tramado. Preparación en el teléfono, hasta 30 s/15 fps.
- **Biblioteca:** archivos del teléfono y los ocho espacios de MiniPet; mostrar, guardar sin mostrar y eliminar.
- **Ajustes:** leer/aplicar módulos, humor, brillo, mensajes, No molestar, clima, tiempos y distancias. Se conservan en el ESP32.

La biblioteca local guarda únicamente los archivos convertidos que eliges conservar/enviar. Los documentos originales se leen a través del selector de Android. La app no envía contenido a servidores externos, no solicita acceso a todas tus fotos y no usa ubicación real; Android 11 y anteriores requieren el permiso de ubicación para buscar BLE.

Las credenciales de vinculación se guardan cifradas con una clave de Android Keystore; las copias de seguridad de datos están deshabilitadas. El transporte Wi-Fi usa HTTP autenticado **solo hacia direcciones locales**; úsalo en tu red de confianza, sin exponer el puerto del ESP32 a internet. BLE requiere vinculación cifrada y el mismo PIN de la OLED. Hay una vinculación de app activa en el dispositivo: una nueva reemplaza la credencial anterior.

## Pruebas

`testDebugUnitTest` incluye conversión a monocromo, encuadre/giro/tramado, formato binario/CRC, comparación con fixtures producidos por el C++ del firmware, paginación/acentos, transferencia por bloques, cancelación y solicitudes HTTP reales contra un servidor local de prueba. Robolectric también inicia y dibuja las cinco pantallas y prueba importar un PNG con el decodificador Android y guardarlo/leerlo de la biblioteca.

Esto no comprueba la radio física, el selector de cada fabricante, los códecs reales de vídeo/GIF del teléfono ni la comunicación con un ESP32 conectado. Esas pruebas se realizan siguiendo [APP_ANDROID.md](../docs/APP_ANDROID.md).

## Entorno de desarrollo en la nube

```sh
python3 tools/setup_android_cloud.py
python3 tools/build_android_cloud.py
```

Esos comandos se ejecutan desde la raíz del repositorio y preparan herramientas únicamente bajo `/workspace`. Reutilizan los módulos del JDK instalado, verifican las descargas, ejecutan pruebas/Lint/compilación y exportan la APK a `/workspace/artifacts`, fuera del checkout. En un equipo personal utiliza Android Studio o el wrapper normal.
