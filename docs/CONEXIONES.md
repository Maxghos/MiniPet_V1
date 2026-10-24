# Conexión y primera prueba

## Componentes

- ESP32-S3 N16R8 con USB y alimentación estable.
- OLED Adafruit SSD1306 de 128×32, I²C, dirección `0x3C`.
- HC-SR501, HC-SR05 y TTP223 momentáneo activo HIGH.
- Divisor de nivel para ECHO: resistencias de **10 kΩ y 15 kΩ**, o un adaptador de nivel adecuado.

## Cableado propuesto

Los números son **GPIO**, no posiciones de las patillas. Desconecta la alimentación antes de cambiar cables y verifica las etiquetas de tu módulo, especialmente el ultrasónico de cinco pines.

| Componente | Conexión | ESP32-S3 |
|---|---|---|
| OLED | VIN/VCC | 3V3 (comprobar especificación del módulo) |
| OLED | GND | GND |
| OLED | SDA | GPIO 8 |
| OLED | SCL | GPIO 9 |
| HC-SR501 | VCC | 5 V |
| HC-SR501 | GND | GND |
| HC-SR501 | OUT | GPIO 4; salida habitual de 3,3 V, comprobar módulo |
| HC-SR05 | VCC | 5 V |
| HC-SR05 | GND | GND |
| HC-SR05 | TRIG | GPIO 5 |
| HC-SR05 | ECHO | GPIO 6 **a través del divisor** |
| TTP223 | VCC | 3V3 |
| TTP223 | GND | GND |
| TTP223 | OUT | GPIO 7 |

El GPIO del ESP32 **no tolera una salida ECHO de 5 V**. El divisor propuesto entrega aproximadamente 3,0 V:

```text
ECHO ── 10 kΩ ──┬── GPIO 6
                │
               15 kΩ
                │
               GND
```

Todas las tierras deben estar unidas. No unas el pin de 5 V a 3V3. El código usa TRIG/ECHO separados: un pin adicional de modo/OUT del HC-SR05 debe configurarse según la documentación de esa variante; no lo conectes por su posición ni lo confundas con ECHO. Confirma también que esa variante reconoce un TRIG de 3,3 V; si requiere más, usa un adaptador apropiado.

Los GPIO propuestos evitan los pines de arranque, el USB nativo y los GPIO 35–37 utilizados por variantes con PSRAM octal. Los clones pueden diferir: si cambias el cableado, modifica `include/BoardConfig.h` y vuelve a compilar.

## TTP223 o pulsador

Alimenta el TTP223 a 3,3 V y selecciona **momentáneo, activo HIGH**, según sus puentes A/B. No uses el modo enclavado/toggle. Deja libre la superficie táctil durante su calibración inicial. Si no tienes el módulo, usa un pulsador entre GPIO 7 y GND con el entorno `esp32s3-button`; se habilita la resistencia pull-up interna.

## Primera carga

1. Compila y conecta la placa por USB con un cable de datos. Para los mensajes de `Serial`, esta configuración usa el USB nativo del S3. La carga también puede hacerse por el puente USB-UART de la placa, seleccionando su puerto.
2. Ejecuta `pio run -e esp32s3 -t upload --upload-port PUERTO`. Si la carga no inicia, mantén BOOT, pulsa y suelta RESET, suelta BOOT y vuelve a intentarlo. No es necesario mantener el monitor abierto.
3. La OLED debe mostrar el saludo y después los ojos. Si está vacía, revisa 128×32, `0x3C`, alimentación y SDA/SCL. Algunos módulos usan `0x3D`: adapta `BoardConfig.h`.
4. El PIR tiene un período inicial de estabilización de **60 s**. Ajusta su retardo físico al mínimo razonable; el software responde a su flanco de activación y limita reacciones repetidas.
5. Abre ajustes con el TTP223 y configura Wi-Fi. En el teléfono, permanece conectado a la red MiniPet aunque no tenga internet y abre `http://192.168.4.1`. La clave temporal aparece únicamente en la OLED.
6. Vuelve a tu Wi-Fi habitual. MiniPet debe obtener hora y clima. Los destinos son `pool.ntp.org`/`time.google.com` por NTP (UDP 123) y `api.open-meteo.com` por HTTPS (443).

## Comprobación física

- Comprueba que las pulsaciones cortas, largas y de salida solo actúan **al soltar** el sensor y que un contacto sostenido no repite opciones.
- Con los tres módulos habilitados, deja libre la zona de gesto durante al menos 1 s. Acerca la mano a 50–80 cm durante 5 s: debe completarse el indicador y aparecer clima → hora → ojos. Mantenerla ahí no debe repetir la consulta.
- Retírala y repite. En `Vista clima = Alternar`, las consultas sucesivas muestran principal, detalles y próximos días, siempre antes del reloj. Si prefieres una sola vista, selecciónala en ajustes.
- En los ajustes de distancias, al editar se muestra la medición actual. Elige una franja que no coincida con tu posición habitual, paredes o muebles; todos pueden reflejar ultrasonidos.
- Quédate quieto delante de la zona de presencia: no debe interpretarse como ausencia solo porque el PIR deje de activarse.
- Prueba humor 0 y 100, mensajes desactivados y horario de No molestar. Las bromas no deben impedir una consulta; el menú no debe ser interrumpido.
- Apaga y enciende para comprobar persistencia de ajustes y Wi-Fi. Comprueba recuperación tras perder la red y la indicación de datos anteriores.
- En V2, comprueba que tus ajustes de V1 se conservaron. Las nuevas opciones son Frases random, Reacción al clima, Avisos del clima y Destello tormenta. Tras clima actual/detalles debe aparecer una reacción de unos 2 s antes de la hora, con asistente habilitado y datos recientes.
- Lee una frase larga completa: las páginas deben estar centradas y avanzar sin cortar el final. Prueba que una consulta o abrir ajustes la interrumpe de inmediato. El destello de tormenta empieza desactivado.
- Revisa físicamente los parpadeos, el fade y la legibilidad. La meta de refresco es ~30 fps; la velocidad real depende del bus, cableado y módulo.

El sensor mide distancia a objetos, no identifica personas ni determina su dirección. El equipo no tiene respaldo horario por batería. Las credenciales Wi-Fi se guardan en NVS del dispositivo; esta configuración no activa cifrado de flash/NVS ni OTA.

## Confianza HTTPS

La conexión meteorológica valida certificados con ISRG Root X1, incluida en `certs/isrgrootx1.pem` desde el repositorio oficial de Let's Encrypt. Su huella SHA-256 es `96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6`. Si el proveedor cambia su cadena, actualiza la CA desde una fuente oficial y recompila. No desactives la verificación TLS.
