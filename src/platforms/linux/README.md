# Linux interface for Cobble

Cobble uses BlueZ via DBus.

## How to talk to DBus

This is a pain. There are loads of bindings, with similar names. See [here](https://www.freedesktop.org/wiki/Software/DBusBindings/) for more details.

We need something that can compile cleanly and simply into a library. We want minimal dependencies.

For testing, first writing a Python implementation (using dbus-python) and using `d-feet` to inspect the DBus interface exposed by BlueZ

With modern BlueZ it looks like we need to:
* Identify the right adapter "/org/bluez/hci0"
* Register to be notified of creation of org.bluez.Device1 proxy objects (HOW?)
* Begin Discovery using its interface "org.bluez.Adapter1", call method "StartDiscovery()"

Notes:

* Discovery automatically stops
* Objects disappear after a while (after discovery?)
* RSSI seems to be RSSI at time of discovery. Need to use Proximity Monitor if RSSI desired

In Python do we need to do something like:

```
        self._adapter = dbus.Interface(dbus_obj, _INTERFACE)
        self._props = dbus.Interface(dbus_obj, 'org.freedesktop.DBus.Properties')
        self._props.connect_to_signal('PropertiesChanged', self._prop_changed)
```
in order to receive callbacks rather than polling?

Let's just jump into C/C++ world. discovered [this](https://github.com/makercrew/dbus-sample) guide!


