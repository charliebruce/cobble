# Wrapper around Cobble native DLL
# TODO: Test on Windows (may need to use stdcall)
from ctypes import *
from enum import Enum, unique
import platform
import sys
import os
from queue import Queue, Empty
import signal
from enum import IntEnum

# Required for runloop
if platform.system() == 'Darwin':
    from PyObjCTools import AppHelper

from threading import Thread

plugin_name = {
    'Darwin': 'cobble_mac.dylib',
    'Windows': 'Release/Windows/x64/Cobble.dll',
    'Linux': 'cobble.so'
}
if platform.system() not in plugin_name.keys():
    print("Platform {} does not have a corresponding Cobble library!")
    sys.exit(-1)

plugin = cdll.LoadLibrary(os.path.abspath(os.path.dirname(os.path.abspath(__file__)) + "/../../../src/build/" + plugin_name[platform.system()]))

c_float_p = POINTER(c_float)
c_byte_p = POINTER(c_byte)

plugin.cobble_init.restype = None
plugin.cobble_deinit.restype = None
plugin.cobble_scan_start.restype = None
plugin.cobble_scan_stop.restype = None
plugin.register_scanresult_cb.restype = None

plugin.cobble_status.restype = c_int

class CobbleStatus(IntEnum):
    Uninitialised = 0
    Initialised = 1
    Scanning = 2
    Connecting = 3
    Connected = 4
    CobbleError = 5

class ConnectionEvent(IntEnum):
    DidDisconnect = 0
    DidConnect = 1
    DidConnectFailed = 2

plugin.cobble_connect.restype = None
plugin.cobble_connect.argtypes = [c_char_p]
plugin.cobble_subscribe.restype = None
plugin.cobble_subscribe.argtypes = [c_char_p]
plugin.cobble_write.restype = None
plugin.cobble_write.argtypes = [c_char_p, c_char_p, c_int]
plugin.cobble_max_writesize_get.restype = c_int
plugin.cobble_max_writesize_get.argtypes = [c_bool]

# Windows only
plugin.cobble_queue_process.restype = None


#typedef void (*scanresult_funcptr)(const char*, int, const char*);

scanresults = Queue()
updatevalues = Queue()
characteristics = []

# Scan results from the library are sent via this callback
# For simplicity of use, we simply add to a queue
# Note that this means that results can be stale.
@CFUNCTYPE(None, c_char_p, c_int, c_char_p)
def scanresult_cb(name, rssi, identifier):
    if name != None:
        name = str(name, 'utf-8')
    else:
        name = "(none)"
    identifier = str(identifier, "utf-8")
    #print(f"Python received a scan result with name {name}, rssi {rssi}, identifier {identifier}")
    scanresults.put((name, rssi, identifier))
plugin.register_scanresult_cb(scanresult_cb)

# Discovered characteristics are sent by the library via this callback
# For simplicity of use, we simply add to a list
# Note that this means that results can be stale.
@CFUNCTYPE(None, c_char_p, c_char_p)
def characteristicdiscovered_cb(service_uuid, characteristic_uuid):
    service_uuid = str(service_uuid, 'utf-8')
    characteristic_uuid = str(characteristic_uuid, 'utf-8')
    print(f"Characteristic discovered: {service_uuid}, {characteristic_uuid}")
    characteristics.append((service_uuid, characteristic_uuid,))
plugin.register_characteristicdiscovered_cb(characteristicdiscovered_cb)

# Characteristic value update notifications are sent by the library via this callback
@CFUNCTYPE(None, c_char_p, POINTER(c_char), c_int)
def updatevalue_cb(characteristic_uuid, data, length):
    characteristic_uuid = str(characteristic_uuid, 'utf-8')
    buf = bytes(b''.join([data[i] for i in range(length)]))
    # print(f"Data received on {characteristic_uuid} is size {len(buf)}, value is " + repr(buf))
    updatevalues.put((characteristic_uuid, buf))
plugin.register_updatevalue_cb(updatevalue_cb)


@CFUNCTYPE(None, c_char_p, c_int)
def connectionstatus_cb(identifier, e):
    print(f"Got a connection status change event for device {identifier}: {e}")
    if ConnectionEvent(e) == ConnectionEvent.DidDisconnect:
        characteristics = [] # Clear the cache
        # Preserve queued updates though, we might have a backlog
plugin.register_connectionstatus_cb(connectionstatus_cb)

def init():
    print("Cobble init")
    plugin.cobble_init()

    # Await either completion or failure
    while(CobbleStatus(plugin.cobble_status()) not in [CobbleStatus.Initialised, CobbleStatus.CobbleError]):
        pass
    
    pass


def start_scan():
    print("Cobble start scan")
    plugin.cobble_scan_start()
    pass

def stop_scan():
    plugin.cobble_scan_stop()

def connect(name):
    plugin.cobble_connect(name.encode('utf-8'))
    pass

def get_scanresult():
    try:
        return scanresults.get(block=False)
    except Empty:
        return None

def get_updatevalue():
    try:
        return updatevalues.get(block=False)
    except Empty:
        return None

def subscribe(characteristic_uuid):
    plugin.cobble_subscribe(characteristic_uuid.encode('utf-8'))

def write(characteristic_uuid, data):
    assert isinstance(data, (bytearray, bytes))
    data_converted = (c_char * len(data))(*data)
    plugin.cobble_write(characteristic_uuid.encode('utf-8'), data_converted, len(data))
    pass

def main_wrap(main_func):
    try:
        return_code = main_func()
        if platform.system() == 'Darwin':
            AppHelper.callAfter(lambda: sys.exit(return_code if return_code else 0))
    except Exception as ex:
        import traceback
        print(traceback.format_exc())
        if platform.system() == 'Darwin':
            AppHelper.callAfter(lambda: sys.exit(-1))

def run_with(main_func):

    print("Running main")
    t = Thread(target=main_wrap, args=(main_func, ))
    t.daemon = True
    t.start()

    print("Running event loop on main thread...")
    if platform.system() == 'Darwin':
        try:
            AppHelper.runConsoleEventLoop(installInterrupt=True)
        except KeyboardInterrupt:
            AppHelper.stopEventLoop()
    else:
        try:
            while(t.is_alive()):
                plugin.cobble_queue_process()
        except KeyboardInterrupt:
            pass

    print("Cobble completed.")
    sys.exit(0)