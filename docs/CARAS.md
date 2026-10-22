# Adaptación de las referencias

Las 13 referencias aportadas se reinterpretan con geometría propia en el espacio nativo de **128×32 píxeles, un bit por píxel**. Párpados, pupilas, brillo y líneas inferiores se componen por separado para permitir animación y transiciones. El trazado de los ojos tranquilo y atento toma como base el ejemplo Arduino aportado.

| Referencias | Adaptación |
|---|---|
| `Tranquilo_Ojos_Dinamico_1` a `_4` | Cuatro variantes con diferentes aperturas, pupilas y cejas; parpadeo irregular y mirada lateral suave |
| `Contento_Ojos_Dinamico_1` y `_2` | Pupilas grandes con brillo y dos variantes de tamaño |
| `Curioso_Ojos` | Asimetría de párpados/cejas y mirada que cambia de objetivo |
| `Impactado_Cara_Dinamico_1` | Ojo afilado para atento/molesto, conservando su contorno triangular |
| `Impactado_Cara_Dinamico_2`, `Impactado_Ojos_Dinamico_1` y `_2` | Ojos abiertos con contorno amplio y pupilas pequeñas para sorpresa |
| `Cansado_Ojos_Parpadeo_Lento_pls` | Párpados bajos y parpadeo de 600 ms, frente a unos 200 ms en los demás estados |
| `Durmiendo_Ojos_ojala_iterar_con_pantalla_con_ZZZZZ` | Ojos cerrados, movimiento suave de descanso y alternancia con `Zzz` |

Se simplifican sombreado, pestañas y detalles finos para conservar la legibilidad. Las variantes comparten anclajes: cambian gradualmente de forma con interpolación exponencial. Las miradas son animadas, no seguimiento direccional de la persona. No se incluyen los archivos de imagen originales ni sus marcas de agua en el firmware.

`tools/preview.sh` compila y ejecuta el **mismo `FaceRenderer.cpp`** utilizado por el ESP32. Produce `docs/caras.html` con ocho secuencias reproducibles, ampliadas sin suavizado. Su visor reproduce cuadros muestreados a ~15 fps; el objetivo del firmware es ~30 fps. Las variaciones más espaciadas se aprecian al mantener encendido el equipo.

Se ha comprobado el framebuffer con pruebas de memoria y un visor de desarrollo; la comparación final de contraste, ritmo y expresividad necesita la OLED física.
