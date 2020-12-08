# LibPAX

A library to estimate passenger (PAX) count based on Wi-Fi & Bluetooth signals based on the ESP32 chip.
This is a library meant to be used for enabling pax-counting in other projects.
For an application facilitating pax-counting, see [ESP32-Paxcounter](https://github.com/cyberman54/ESP32-Paxcounter).
The library was extracted from the ESP32-Paxcounter project to enhance reusability.

Current version: **0.0.1**

A **1.0** should follow shortly, after which API is considered stable.

## Usage

For using the library please refer to `libpax_api.h`.
All functions outside the `libpax_api.h` are considered internal and might change between releases.

### Compile time options

You must define one of the following compile time constants to select the framework you are working with:
```
LIBPAX_ARDUINO
LIBPAX_ESPIDF
```

To select the supported counting method you may use:
```
LIBPAX_WIFI 
LIBPAX_BLE
```

Select the size for storing mac addresses in RAM:
```
LIBPAX_MAX_SIZE int [default: 15000] - Number of remembered devices, RAM usage: LIBPAX_MAX_SIZE * 2 Byte
```

## Examples

The `/examples` folder contains an

## Tests

The library contains unit tests, which can be run via:

```
pio test -e espidf -e arduino
```

Please note: The tests are executed on an esp32 which must be connected via USB.

## Changelog

Please refer to our separate [CHANGELOG.md](CHANGELOG.md) file for differences between releases.