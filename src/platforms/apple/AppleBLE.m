// Common bindings for Bluetooth LE
// CoreBluetooth (macOS/iOS)
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../cobble.h"
#include "../../cobble_events.h"

// State exposed to the calling app
CobbleStatus status = Uninitialised;
CobbleErrorCode error_code = NoError;

@interface CoreBluetoothBackend : NSObject
@end

CoreBluetoothBackend* appleBackend = NULL;

@interface CoreBluetoothBackend () <CBCentralManagerDelegate, CBPeripheralDelegate>

    @property (nonatomic, strong) CBCentralManager *centralManager;
    @property (nonatomic, strong) CBPeripheral *currentPeripheral;

    //Cache of characteristics - we can't get characteristics from UUIDs without this
    @property NSMutableDictionary *characteristicCache;

@end

@implementation CoreBluetoothBackend {
}

- (id)init {

    _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil options:nil];
    _characteristicCache = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)pauseScan {

    [self.centralManager stopScan];
    status = Initialised;

}

- (void)resumeScan:(NSArray*) scanFilter {

    //Allow duplicates to get updated RSSI readings on each packet. Note that this will impact power consumption.
    [self.centralManager scanForPeripheralsWithServices:scanFilter options: @{CBCentralManagerScanOptionAllowDuplicatesKey: @true }];
    status = Scanning;

}

- (void)read:(CBCharacteristic*) characteristic {
    [_currentPeripheral readValueForCharacteristic:characteristic];
}

- (void)write:(CBCharacteristic*) characteristic length:(uint8_t) len dataPtr:(uint8_t*) data {
    if([characteristic properties] & CBCharacteristicPropertyWriteWithoutResponse) {
        [_currentPeripheral writeValue:[NSData dataWithBytes:data length:len] forCharacteristic:characteristic type:CBCharacteristicWriteWithoutResponse];
    } else {
        [_currentPeripheral writeValue:[NSData dataWithBytes:data length:len] forCharacteristic:characteristic type:CBCharacteristicWriteWithResponse];
    }
}

- (void)cleanup {

    [self disconnect];

    [_centralManager release];
    [_characteristicCache release];

    _centralManager = NULL;
    [self cleanupOnDisconnect];

}

- (void)cleanupOnDisconnect {
    _currentPeripheral = NULL;
    // [_characteristicCache removeAllObjects];
}

- (void) connect:(NSString*) identifier {

    //Try to find the peripheral device matching the given identifier
    NSUUID* iduuid = [[NSUUID UUID] initWithUUIDString:identifier];
    NSArray<CBPeripheral*> * matching = [self.centralManager retrievePeripheralsWithIdentifiers:@[iduuid]];

    // If no matching device is found, we can't do anything more
    if ([matching count] == 0) {
        NSLog(@"ERROR: can't find peripherial with identifier %@", identifier);
        return;
    }

    // Try to initiate a connection to the device
    CBPeripheral* peripheral = [matching firstObject];
    self.currentPeripheral = peripheral;
    [self.centralManager connectPeripheral:self.currentPeripheral options:nil];
    status = Connecting;

}

- (void)disconnect {
    if (self.currentPeripheral != NULL && _centralManager != NULL) {
        [_centralManager cancelPeripheralConnection:self.currentPeripheral];
    }
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    
    NSLog(@"centralManagerDidUpdateState");
    NSString *state = @"";

    switch ([central state])
    {
          case CBManagerStateUnsupported:
                state = @"Bluetooth Low Energy not supported.";
                error_code = HardwareUnsupported;
                status = CobbleError;
                break;
          case CBManagerStateUnauthorized:
                state = @"Not authorized to use Bluetooth Low Energy.";
                error_code = PermissionsNotGranted;
                status = CobbleError;
                break;
          case CBManagerStatePoweredOff:
                state = @"Bluetooth on this device is powered off.";
                error_code = HardwareTurnedOff;
                status = CobbleError;
                break;
          case CBManagerStateResetting:
                state = @"BLE is resetting, state update pending.";
                break;
          case CBManagerStatePoweredOn:
                state = @"Bluetooth LE is turned on and ready for communication.";
                status = Initialised;
                break;
          case CBManagerStateUnknown:
                state = @"BLE Manager status unknown.";
                error_code = UnknownError;
                status = CobbleError;
                break;
          default:
                state = @"BLE Manager status unknown.";
                error_code = UnknownError;
                status = CobbleError;
                break;
    }

    NSLog(@"%@", state);
    
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary *)advertisementData RSSI:(NSNumber *)RSSI {

    NSString *peripheralName = [advertisementData objectForKey:@"kCBAdvDataLocalName"];
    NSString *peripheralUUID = peripheral.identifier.UUIDString;

    cobble_event_scanresult([peripheralName UTF8String], [RSSI intValue], [peripheralUUID UTF8String]);

}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {

    if(status == Connected) //TODO: Unclear why this is needed, but without it we get duplicate events.
        return;
    status = Connected;

    // TODO: Should the app control scanning behaviour instead?
    [self.centralManager stopScan];

    NSString *peripheralUUID = peripheral.identifier.UUIDString;
    cobble_event_connectionstatus([peripheralUUID UTF8String], ConnectionStatus_DidConnect);

    //Register ourself as the delegate for CBPeripheral events
    self.currentPeripheral.delegate = self;

    // Discover all services on the device.
    // Discovering all services is slightly less energy-efficient than only obtaining the results we care about
    // but the tiny energy saving is not worth the extra code complexity
    [peripheral discoverServices:nil];

}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {

    if([self.centralManager isScanning])
        status = Scanning;
    else
        status = Initialised;

    NSString *peripheralUUID = peripheral.identifier.UUIDString;
    cobble_event_connectionstatus([peripheralUUID UTF8String], ConnectionStatus_DidConnectFailed);

}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {

    status = Initialised;

    NSString *peripheralUUID = peripheral.identifier.UUIDString;
    cobble_event_connectionstatus([peripheralUUID UTF8String], ConnectionStatus_DidDisconnect);

    [self cleanupOnDisconnect];
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {

    for (CBService *service in peripheral.services) {

        NSString *serviceId = [service.UUID UUIDString];
        if([serviceId length] == 4) { // Short UUID - extend it for consistency with other platforms
            serviceId = [NSString stringWithFormat:@"0000%@-0000-1000-8000-00805F9B34FB", serviceId];
        }

        cobble_event_servicediscovered([serviceId UTF8String]);
        [peripheral discoverCharacteristics:nil forService:service];
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    for (CBCharacteristic *characteristic in service.characteristics) {

        NSString *serviceId = [service.UUID UUIDString];
        if([serviceId length] == 4) { // Short UUID - extend it for consistency with other platforms
            serviceId = [NSString stringWithFormat:@"0000%@-0000-1000-8000-00805F9B34FB", serviceId];
        }

        NSString *characteristicId = [characteristic.UUID UUIDString];
        if([characteristicId length] == 4) { // Short UUID - extend it for consistency with other platforms
            characteristicId = [NSString stringWithFormat:@"0000%@-0000-1000-8000-00805F9B34FB", characteristicId];
        }

        //Cache for easy access to characteristics by UUID
        [_characteristicCache setValue: characteristic forKey: characteristicId];

        cobble_event_characteristicdiscovered([serviceId UTF8String], [characteristicId UTF8String]);

    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {

    if (error) {
          NSLog(@"Error updating notification state: %@", [error localizedDescription]);
          return;
    }

    // No need to do anything if the operation succeeded

}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    if (error) {
          NSLog(@"Error changing notification state: %@", [error localizedDescription]);
          return;
    }

    NSData *dataBytes = characteristic.value;
    NSString *characteristicId = [characteristic.UUID UUIDString];

    if([characteristicId length] == 4) { // Short UUID - extend it for consistency with other platforms
        characteristicId = [NSString stringWithFormat:@"0000%@-0000-1000-8000-00805F9B34FB", characteristicId];
    }

    cobble_event_updatevalue([characteristicId UTF8String], [dataBytes bytes], [dataBytes length]);

}

@end

void cobble_init(void) {

    NSLog(@"Cobble initialising...");
    appleBackend = [[CoreBluetoothBackend alloc] init ];

}

void cobble_deinit(void) {

    NSLog(@"Cobble deinitialising...");
    
    if (appleBackend != NULL) {
        [appleBackend cleanup];
        [appleBackend release];
    }
    
    appleBackend = NULL;

}

void cobble_read(const char* characteristic_uuid) {

    //Find the characteristic object with the given UUID in the cache
    NSString *characteristic_uuid_str = [NSString stringWithUTF8String:characteristic_uuid];
    CBCharacteristic *characteristic = [appleBackend characteristicCache][characteristic_uuid_str];

    if(characteristic == nil) {
        NSLog(@"Could not find the characteristic %@ in the cache.", characteristic_uuid_str);
        return;
    }

   [appleBackend read: characteristic];
}

void cobble_subscribe(const char* characteristic_uuid) {

    //Find the characteristic object with the given UUID in the cache
    NSString *characteristic_uuid_str = [NSString stringWithUTF8String:characteristic_uuid];
    CBCharacteristic *characteristic = [appleBackend characteristicCache][characteristic_uuid_str];

    if(characteristic == nil) {
        NSLog(@"Could not find the characteristic %@ in the cache.", characteristic_uuid_str);
        return;
    }

    //Subscribe to it
    [appleBackend.currentPeripheral setNotifyValue:true forCharacteristic:characteristic];
}

void cobble_write(const char* characteristic_uuid, uint8_t *data, int len) {

    //Find the characteristic object with the given UUID in the cache
    NSString *characteristic_uuid_str = [NSString stringWithUTF8String:characteristic_uuid];
    CBCharacteristic *characteristic = [appleBackend characteristicCache][characteristic_uuid_str];

    if(characteristic == nil) {
        NSLog(@"Could not find the characteristic %@ in the cache.", characteristic_uuid_str);
        return;
    }

    [appleBackend write: characteristic length:len dataPtr: data];
}

void cobble_connect(const char* identifier) {
    [appleBackend connect:[NSString stringWithUTF8String:identifier]];
}

void cobble_disconnect(void) {
   [appleBackend disconnect];
}

void cobble_scan_start(const char* service_uuids) {

    NSMutableArray *arrayOfCbuuids = nil;

    // Create our list of Service UUIDs to scan for
    if(service_uuids == NULL) {
        // Monterey (macOS 12) introduced a bug with scan filters, whereby if you scan with no filters, no scan results will be provided.
        // This was at odds with the documented behaviour (which state that no scan filter results in all scan results being provided)
        // This behaviour was fixed in macOS Monterey Beta 21E5212f, and released in 12.3.
        NSLog(@"Scanning with no service_uuids filter will return no results on certain versions of Monterey! If you get no scan results, this might be why. For maximum compatibility you should specify service UUIDs.");
    } else {
        // Split comma-separated values into strings
        NSArray *arrayOfStrings = [[NSString stringWithUTF8String:service_uuids] componentsSeparatedByString:@","];

        // Convert to CBUUID array
        arrayOfCbuuids = [NSMutableArray arrayWithCapacity: [arrayOfStrings count]];
        for (id obj in arrayOfStrings) {
            @try {
                [arrayOfCbuuids addObject: [CBUUID UUIDWithString:obj]];
            }
            @catch (NSException *exception) {
                NSLog(@"Could not create Service UUID from \"%@\" to add to filter", obj);
            }
        }

    }

    [appleBackend resumeScan:arrayOfCbuuids]; //TODO: Report an error if we're not in the Initialised state

}

void cobble_scan_stop(void) {
    [appleBackend pauseScan];
}

// Determine the largest value that can be written safely to the current peripheral
int cobble_max_writesize_get(bool withResponse) {

    if(withResponse)
        return [[appleBackend currentPeripheral] maximumWriteValueLengthForType:CBCharacteristicWriteWithResponse];

    return [[appleBackend currentPeripheral] maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse];

}

// Run loop handling is required for console apps / some other specific use cases
bool cobble_shutdown_requested = false;

void cobble_shutdown(void) {
    cobble_shutdown_requested = true;
}

//Run a loop on the main thread until terminated
void cobble_loop(void) {

    NSRunLoop *runLoop = NSRunLoop.currentRunLoop;
    
    //Loop runs about once per second (otherwise we'd be stuck until another delegate event happens, potentially forever!)
    while (!cobble_shutdown_requested && [runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:1]])
        ;

}

CobbleStatus cobble_status(void) {
    return status;
}

CobbleErrorCode cobble_error_get(void) {
    return error_code;
}

void cobble_queue_process(void) {
    static bool warningShown = false;
    if(!warningShown) {
        warningShown = true;
        printf("cobble_queue_process has no effect on this platform, callbacks will be delivered when the events are fired.");
    }
}
