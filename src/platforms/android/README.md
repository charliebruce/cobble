# Android Implementation

This uses JNI to interface between the native code and the Android Bluetooth LE stack.

We use a String containing the full MAC address as our device identifier, as you can use BluetoothAdapter#getRemoteDevice(String) to create a BluetoothDevice object. 
