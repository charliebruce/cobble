// Handle asynchronous events sent by the respective Bluetooth stacks
// These events could be handled in one of two ways:
// * Added to a queue and pulled by the client (queue must be thread-safe to ensure reliable operation on all platforms)
// * Pushed through a callback

//Depending on your use case, either approach may be preferred. 
//Eg Unity does suppot delegates/callbacks
//We could default to queues/polling, but if a function pointer is provided, events could be dealt with in real time instead.
//If doing so, you would need to be careful about being thread-safe in the calling app.
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "cobble.h"
#include "cobble_events.h"

/*
 * Callback function pointers and registration functions
 */

scanresult_funcptr scanresult_cb = NULL;
characteristicdiscovered_funcptr characteristicdiscovered_cb = NULL;
updatevalue_funcptr updatevalue_cb = NULL;
connectionstatus_funcptr connectionstatus_cb = NULL;

EXPORTED void register_scanresult_cb(scanresult_funcptr p) {
    scanresult_cb = p;
}

EXPORTED void register_characteristicdiscovered_cb(characteristicdiscovered_funcptr p) {
    characteristicdiscovered_cb = p;
}

EXPORTED void register_updatevalue_cb(updatevalue_funcptr p) {
    updatevalue_cb = p;
}

EXPORTED void register_connectionstatus_cb(connectionstatus_funcptr p) {
    connectionstatus_cb = p;
}

/*
 * Function handlers including default behaviour
 */

void cobble_event_scanresult(const char* name, int rssi, const char* identifier) {

    if(scanresult_cb != NULL) {
        scanresult_cb(name, rssi, identifier);
        return;
    } 

    printf("Default handler for scan result: %s with RSSI %i and identifer %s\n", name, rssi, identifier);
    
}


void cobble_event_connectionstatus(const char* identifier, int status) {

    if(connectionstatus_cb != NULL) {
        connectionstatus_cb(identifier, status);
        return;
    }

    switch(status) {
        case ConnectionStatus_DidConnect:
        printf("Did connect!\n");
        break;
        case ConnectionStatus_DidDisconnect:
        printf("Did disconnect!\n");
        break;
        case ConnectionStatus_DidConnectFailed:
        printf("Did connect failed!\n");
        break;

    }
}

void cobble_event_servicediscovered(const char* uuid) {
    printf("Found service %s\n", uuid);
}

void cobble_event_characteristicdiscovered(const char* svc_uuid, const char* char_uuid) {

    if(characteristicdiscovered_cb != NULL) {
        characteristicdiscovered_cb(svc_uuid, char_uuid);
        return;
    } 

    printf("Default handler for characteristic discovery: Service %s, characteristic %s\n", svc_uuid, char_uuid);

}

void cobble_event_updatevalue(const char* characteristic_uuid, const uint8_t* data, int len) {

    if(updatevalue_cb != NULL) {
        updatevalue_cb(characteristic_uuid, data, len);
        return;
    }

    printf("Default handler for updated charactistic %s with %i bytes of data, first byte is 0x%02x\n", characteristic_uuid, len, data[0]);
}