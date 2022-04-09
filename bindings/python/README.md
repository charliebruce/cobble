# Cobble Bindings for Python

Could be implemented in 2 ways:

* CDLL
* Extension Module

For now we will use CDLL


## macOS

On macOS, a event loop (NSRunLoop) is required for interating with CoreBluetooth - work must not be done on the main thread.


## Install

```
python3 setup.py
```
