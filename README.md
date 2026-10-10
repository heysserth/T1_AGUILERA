# Pines provisionales

Esta tabla es una propuesta inicial basada en el mapa de pines de referencia
de la placa ESP32-S3. Los GPIO elegidos deben confirmarse con el modelo exacto
de la placa antes de conectar los componentes.

| Elemento | GPIO | Uso |
|---|---:|---|
|LED|GPIO 5|Entre 3,3V y el pin|
| LED externo | GPIO5 | La placa lo controla como salida para encenderlo y apagarlo. |
| Pulsador | GPIO4 | La placa lo lee como entrada para detectar si se presiona. |
| I2C - SDA | GPIO8 | Línea de datos del bus I2C. |
| I2C - SCL | GPIO9 | Línea de reloj del bus I2C. |

El LED externo debe conectarse con una resistencia en serie. El mapa también
muestra un LED RGB integrado en GPIO48; no es el LED externo de esta propuesta.