# Common Bindings for Bluetooth LE

This project aims to abstract out platform-specific differences, and make Bluetooth LE consistent to use across:

* Windows (WinRT)
* Linux (BlueZ)
* macOS
* iOS
* Android

It will eventually provide bindings for, at least:

* C / C++
* Unity
* Python 3

It will achieve this by channeling the platform-specific calls / functionality through a C middleware, compiled into a library. Unity, Python etc will then call the native C functions within the library.

## Potential Alternatives

* [btleplug](https://github.com/deviceplug/btleplug) might be a more sensible choice, if you need robust Linux support and/or are happy using Rust (or can write suitable bindings). (It's possible that I'll turn this project into Python/Unity bindings for btleplug in the future).
* [bleak](https://pypi.org/project/bleak/) for Python.

## Uses

It is intended for:

* Interfacing an application or game with a custom BLE peripheral
* Datalogging
* Automatic testing

It is NOT currently intended to support:

* Multiple BLE adaptors
* Windows versions before 10 (no scanning functionality available in the legacy BLE libraries)
* Any device profiles, even standard ones (Device Information Service, Battery Service etc). Interpreting the received data is up to the app or another library.
* Beacons or advertising
* Classic Bluetooth devices
* Central role
* Connecting to multiple devices simultaneously (assumption of single device is hard-coded in several places)
* Ultra high performance applications (connection process retrives all characteristics of all services)
* Integration into large or complex projects - these may be better suited to integrating Chromium
* Anything safety-critical or high-reliability

## Status

This entire project should be considered a work-in-progress with an unstable interface, however the Android, iOS and macOS implementations are well-tested and robust. WinRT is working but largely untested. Linux (BlueZ) is in development and not ready for use.

The bindings are rough examples only, you should use them as a starting point rather than a full reference implementation.

## Getting Started

### Building

* Android, macOS, iOS: Run the relevant build script within `src/`.
* Windows: Open `src/build-environments/Windows/cobble.sln` in Visual Studio and build it.
 
Binaries are created within `src/build`.
 
### C/C++

See C header file `src/cobble.h`.

### Unity

An example binding script can be found within `bindings/unity`. You should build and import the libraries for each platform you intend to support, making sure you configure the architectures / platforms correctly for each library.

### Python

An example binding is found within `bindings/python`.


## Platform-specific limitations

* iOS / macOS don't tell you whether a characteristic value has been obtained as a result of a notification or a read.
* macOS Monterey doesn't support scanning unless an advertised service UUID is known - using a blank service filter gives no scan results (rather than all scan results). iOS appears unaffected.

