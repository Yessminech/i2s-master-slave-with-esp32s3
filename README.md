# I²S Master/Slave on ESP32-S3

Bit-banged I²S master and slave implementations for the ESP32-S3, used to read three strain
sensors in parallel and get their samples off the board reliably.

## Why bit-banging

The ESP32-S3's hardware I²S peripheral is built around audio use cases, which constrains the
frame formats and clocking it will produce. Driving the clock and data lines directly gave
control over the timing needed for this sensor setup, at the cost of doing the protocol by hand.

## Repository layout

```
i2s-bit-banging-master/   master implementation — generates clock, reads sensor data
i2s-bit-banging-slave/    slave implementation — clocked externally
.vscode/                  ESP-IDF build and debug configuration
```

Each subproject is a standalone ESP-IDF application.

## Building

Requires ESP-IDF (v5.x) with an ESP32-S3 target.

```bash
cd i2s-bit-banging-master
idf.py set-target esp32s3
idf.py build flash monitor
```

Same for the slave, on a second board.

## Result

Reliable parallel acquisition from three strain sensors, verified against the sensors' expected
output.
