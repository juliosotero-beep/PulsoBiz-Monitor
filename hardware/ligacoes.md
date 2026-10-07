# Ligações do Hardware

## ESP32 DevKit V1

Microcontrolador principal do PulsoBiz Monitor.

## DHT22

| DHT22 | ESP32 |
|---|---|
| SDA/DATA | GPIO 4 |
| VCC | 3V3 |
| GND | GND |

## Sensor PIR

| PIR | ESP32 |
|---|---|
| OUT | GPIO 27 |
| VCC | 5V |
| GND | GND |

## Módulo Relé

| Relé | ESP32 |
|---|---|
| IN | GPIO 26 |
| VCC | 5V |
| GND | GND |

## Display OLED SSD1306

| OLED | ESP32 |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3V3 |
| GND | GND |

## Endereço I2C

O display OLED utiliza o endereço:

```text
0x3C
