#!/usr/bin/env python3
import argparse
import dbus
import dbus.mainloop.glib
import dbus.service
import json
from time import sleep

from threading import Thread

try:
    from gi.repository import GObject  # python3
except ImportError:
    import gobject as GObject  # python2

DBUS_OM_IFACE = 'org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE = 'org.freedesktop.DBus.Properties'

BLUEZ_SERVICE_NAME = 'org.bluez'

ADV_MONITOR_APP_BASE_PATH = '/org/bluez/example/adv_monitor_app'

DEVICE_IFACE = 'org.bluez.Device1'


def find_adapter(bus):
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'),
                                DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()

    adapter = None # "org/bluez/hci0" in testing
    adapter_props = None

    for o, props in objects.items():
        if "org.bluez.Adapter1" in props:
            adapter = o
            break

    if adapter:
        # Turn on the bluetooth adapter.
        adapter_props = dbus.Interface(
                                bus.get_object(BLUEZ_SERVICE_NAME, adapter),
                                DBUS_PROP_IFACE)
        adapter_props.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(1))

    return adapter, adapter_props

from pprint import pprint

def test(bus, mainloop):

    if not True:
        print('Something went wrong.')
        mainloop.quit()
        exit(-1)
        
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'), DBUS_OM_IFACE)
    

    discoveredDevices = []
    while True:
        sleep(1)
        objects = remote_om.GetManagedObjects()
        currentDevices = [o for o,props in objects.items() if DEVICE_IFACE in props]
        #print("Current devices: ")
        #pprint(currentDevices)
        newDevices = [x for x in currentDevices if not x in discoveredDevices]
        for d in newDevices:
            print("NEW")
            pprint(d)
        discoveredDevices.extend(newDevices)


    mainloop.quit()


def main(app_id):
    # Initialize threads in gobject/dbus-glib before creating local threads.
    GObject.threads_init()
    dbus.mainloop.glib.threads_init()

    # Arrange for the GLib main loop to be the default.
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    bus = dbus.SystemBus()
    mainloop = GObject.MainLoop()

    # Find bluetooth adapter and power it on.
    adapter, adapter_props = find_adapter(bus)
    if not adapter or not adapter_props:
        print('Bluetooth adapter not found.')
        exit(-1)

    Thread(target=test, args=(bus, mainloop)).start()

    mainloop.run() # blocks until mainloop.quit() is called


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--app_id', default=0, type=int, help='use this App-ID '
                        'for creating dbus objects (default: 0)')
    args = parser.parse_args()

    main(args.app_id)


