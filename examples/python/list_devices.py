#!/usr/bin/env python3

# Scan for BLE devices, print them when found
from cobble import cobble
from time import sleep
from datetime import datetime, timedelta

def main():

    cobble.init()
    cobble.start_scan()

    print("Scanning for devices, press Ctrl-C to quit")


    # Results can be dispatched more than once for the same peripheral, but that isn't important here.
    starttime = datetime.now()

    while((datetime.now() - starttime) < timedelta(seconds=20)):
        
        result = cobble.get_scanresult()
        
        if result:
            name, rssi, identifier = result
            print(f"Found device: {name}, RSSI {rssi}, identifier {identifier} ")
            if False:
                cobble.connect(identifier)
                break

        sleep(0.01)

    # TODO: Await connection event rather than just sleeping
    sleep(3)

    print("Characteristics: ")
    print(repr(cobble.characteristics)) # Consists of all discovered (service, characteristic) pairs as strings


    while True:

        while (notif := cobble.get_updatevalue()) != None:
            print("Received a notification...")
            

        sleep(0.01)




# This pattern is necessary for use on macOS - CoreBluetooth requires a RunLoop
# On other platforms, this won't cause any problems
cobble.run_with(main)