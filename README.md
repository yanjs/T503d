# T503d

A driver for 10moons T503 tablet on Linux platform.

## Requirements

libevdev 1.12.1

libusb 1.0.26

## Configuration

You can edit `src/T503_key_settings.inc` and re`make` to customize the key bindings for all 6 buttons.

## Build

To build the driver using CMake, run the following commands:

```console
cmake -S . -B ./build
cmake --build ./build
```

Alternatively, you can use MakeFile:

```console
make
```

## Usage

Running in the foreground (administrator privilege required).

```console
sudo ./build/T503d
```

## Issues

If you find any bugs or memory leaks, feel free to leave an issue / PR.
