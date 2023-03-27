# T503d

A driver for 10moons T503 tablet on Linux platform.

## Requirements

libevdev 1.12.1

libusb 1.0.26

## Configuration

You can edit `src/T503_key_settings.inc` and re`make` to customize the key bindings for all 6 buttons.

## Usage

Make and running in the foreground (administrator privilege required).

```console
cmake CMakeLists.txt
make
```

## Issues

If you find any bugs or memory leaks, feel free to leave an issue / PR.
