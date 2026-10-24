# Adaptación de las referencias

Las 13 referencias originales y las cuatro nuevas se reinterpretan con geometría propia en **128×32 píxeles, un bit por píxel**. V2 contiene **19 estilos comprobados como diferentes en el framebuffer**, distribuidos en 11 estados. Párpados, pupilas, brillo, cejas y pestañas se componen por separado. Se conserva el trazado clásico y la secuencia de alerta del ejemplo Arduino aportado.

| Referencias | Adaptación |
|---|---|
| `Tranquilo_Ojos_Dinamico_1` a `_4` | Cuatro contornos: clásico, caído, con pestañas y amplio; distintos párpados, anchuras, pupilas y contorno inferior |
| `Contento_Ojos_Dinamico_1` y `_2` | Contento redondo y brillante; además, variante con ojos cerrados en arco |
| `Curioso_Ojos` | Asimetría de párpados/cejas y mirada que cambia de objetivo |
| `Impactado_Cara_Dinamico_1` | Ojo afilado para atento/molesto, conservando su contorno triangular |
| `Impactado_Cara_Dinamico_2`, `Impactado_Ojos_Dinamico_1` y `_2` | Tres formas de sorpresa: redonda, rectangular y anime, con diferentes contornos y pupilas |
| `Cansado_Ojos_Parpadeo_Lento_pls` | Párpados bajos y parpadeo de 600 ms, frente a unos 200 ms en los demás estados |
| `Durmiendo_Ojos_ojala_iterar_con_pantalla_con_ZZZZZ` | Ojos cerrados, movimiento suave de descanso y alternancia con `Zzz` |
| Nueva referencia de ojos azules | Irónico: párpado casi horizontal, cejas inclinadas y mirada asimétrica |
| Nueva referencia de ojos violetas | Entrecerrado: iris anchos, pestañas y cejas de gesto seco |
| Nueva referencia de ojos naranjas | Pícaro: contorno oblicuo, pupilas más grandes y cejas inclinadas |
| Nueva referencia de enfado con ojos cerrados | Ojos cerrados, cejas en V y símbolo de enfado que aparece con humor alto |
| Expresión de puchero | Ofendido: apertura pequeña, cejas y asimetría propia |

Se simplifican sombreado, pestañas y detalles finos para conservar la legibilidad. Las variantes comparten anclajes: cambian gradualmente de forma con interpolación exponencial. Las miradas son animadas, no seguimiento direccional de la persona. No se incluyen los archivos de imagen originales ni sus marcas de agua en el firmware.

`tools/preview.sh` compila y ejecuta los **mismos motores C++** usados por el ESP32. Produce `docs/caras.html` con **38 secuencias**: 11 estados, 19 estilos individuales y ocho reacciones al clima. El visor muestrea cuadros a ~15 fps; el objetivo del firmware es ~30 fps. `node tools/test_preview.js` comprueba que los píxeles decodificados por el HTML coinciden con las capturas del motor nativo.

El frío desplaza un píxel el cuadro; el calor añade una gota; lluvia/nieve añaden gotas/copos y mirada hacia arriba; la tormenta usa sorpresa y un relámpago. La inversión opcional se aplica al framebuffer solo al inicio de la reacción; el siguiente dibujo la restituye automáticamente. Las animaciones ambientales se regulan por humor y se omiten con datos anteriores o No molestar.

Se ha comprobado el framebuffer con pruebas de memoria y un visor de desarrollo; la comparación final de contraste, ritmo y expresividad necesita la OLED física.
