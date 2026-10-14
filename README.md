# Pines provisionales

Esta tabla es una propuesta inicial basada en el mapa de pines de referencia
de la placa ESP32-S3. Los GPIO elegidos deben confirmarse con el modelo exacto
de la placa antes de conectar los componentes.

| Elemento | GPIO | Uso |
|---|---:|---|
|LED|GPIO 5|Entre el pin y masa, con su resistencia en serie|
| LED externo | GPIO5 | La placa lo controla como salida para encenderlo y apagarlo. |
| Pulsador | GPIO4 | La placa lo lee como entrada para detectar si se presiona. |
| I2C - SDA | GPIO8 | Línea de datos del bus I2C. |
| I2C - SCL | GPIO9 | Línea de reloj del bus I2C. |

El LED externo debe conectarse con una resistencia en serie. El mapa también
muestra un LED RGB integrado en GPIO48; no es el LED externo de esta propuesta.

## Ejercicio 3: sensor BME280

Conecta el sensor con la placa apagada:

| BME280 | ESP32-S3 |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

No conectes VIN a 5V. La dirección I2C comprobada en la placa es `0x76`.
El ejercicio usa la API oficial Bosch para comprobar la identidad del sensor
y leer sus datos de calibración. Después configura las tres magnitudes con
sobremuestreo 1x y muestra lecturas compensadas cada dos segundos:
[BME280 Sensor API](https://github.com/BoschSensortec/BME280_SensorAPI),
licencia BSD-3-Clause. Los archivos originales y sus avisos de licencia están
en `components/bme280`.