# CA públicas

`isrgrootx1.pem` mantiene la CA de Open-Meteo usada por las versiones anteriores.

`telegram_roots.bin` es el formato indexado de CA que utiliza WiFiClientSecure de Arduino ESP32 2.0.17. Contiene **121 raíces públicas** del conjunto Mozilla distribuido por **certifi 2026.7.22**, obtenido de su wheel oficial de PyPI y comprobado mediante SHA-256. Solo se incorporan nombres y claves públicas, ordenados para la búsqueda del SDK. No se usa el almacén del sistema ni el del entorno de desarrollo, que pueden contener CA privadas de un proxy.

La procedencia, URL y huellas están en `telegram_roots.json`; la licencia de certifi/Mozilla está en [CERTIFI_LICENSE.txt](CERTIFI_LICENSE.txt). El archivo no contiene credenciales de Telegram ni certificados personales.

Comprobar el binario versionado, sin red ni dependencias externas:

```sh
python3 tools/generate_telegram_trust.py --check
```

Regenerarlo, desde la raíz del repositorio, con Python y `cryptography` instalado:

```sh
python3 tools/generate_telegram_trust.py
# También admite --wheel ruta/certifi-2026.7.22-py3-none-any.whl
```

La descarga y el PEM se verifican contra huellas fijadas en el script. Para actualizar el conjunto de CA hay que revisar y cambiar deliberadamente la versión/huellas, regenerar y probar en la placa. Las conexiones de Telegram requieren hora válida y verifican certificado y nombre del servidor. Las peticiones HTTPS de clima y Telegram se serializan en tareas de red para acotar el uso de RAM y mantener el dibujo/sensores fuera de esas esperas.
