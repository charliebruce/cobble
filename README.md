# Common Bindings for Bluetooth LE

This project aims to abstract out platform-specific differences, and make Bluetooth LE consistent to use across:

* Windows (WinRT only)
* Linux
* macOS
* iOS
* Android

It will eventually provide bindings for, at least:

* C / C++
* Unity
* Python 3

It will achieve this by channeling the platform-specific calls / functionality through a C middleware, compiled into a library. Unity, Python etc will then call the native C functions within the library.

It will follow a similar approach on each platform to the [Chromium](https://chromium.googlesource.com/chromium/src/+/master/device/bluetooth/) and [Qt](https://github.com/qt/qtconnectivity/tree/dev/src/bluetooth) Bluetooth layers, but will be much lighter and easier to understand.

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


## Getting Started

### Building

Run the relevant build script, copy out the produced DLL/SO/DYLIB.

### C/C++

See C header file `src/cobble.h`.

### Unity

TODO: Describe DLL structure and binding interface script.

### Python

TODO: This.



## Status

This entire project is a work-in-progress.

## Platform-specific limitations

* iOS / macOS don't tell you whether a characteristic value has been obtained as a result of a notification or a read.
* macOS Monterey doesn't support scanning unless an advertised service UUID is known - using a blank service filter gives no scan results (rather than all scan results). iOS appears unaffected. See [#10](https://github.com/charliebruce/cobble/issues/10) for more information. 

