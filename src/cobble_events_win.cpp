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

#include <queue>
#include <string>
using namespace std;

// The maximum size of a Bluetooth LE characteristic value update
#define MAX_LENGTH 256

// Cobble can either call back instantly when an event occurs, or queue and defer until cobble_queue_process() is called
// This queued approach allows events to be handled on a specific thread - this seems to be required when interacting with Unity
//#define COBBLE_CALLBACK_REALTIME
#define COBBLE_CALLBACK_DEFERRED


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

#if defined(COBBLE_CALLBACK_DEFERRED)

class scandata {
public:
    string name;
    int rssi;
    string mac;
};

queue<scandata>scanQueue;

class connectionstatus {
public:
    string identifier;
    int status;
};

queue<connectionstatus> connectionStatusQueue;

class characteristicdiscovery {
public:
    string service;
    string characteristic;
};

queue<characteristicdiscovery> characteristicDiscoveryQueue;

class valueupdate {
public:
    string characteristic;
    uint8_t data[MAX_LENGTH];
    int length;
};

queue<valueupdate> valueUpdateQueue;

#endif

/*
 * Function handlers including default behaviour
 */

void cobble_event_scanresult(const char* name, int rssi, const char* identifier) {

#if defined(COBBLE_CALLBACK_REALTIME)

    if(scanresult_cb != NULL) {
        scanresult_cb(name, rssi, identifier);
        return;
    }

#elif defined(COBBLE_CALLBACK_DEFERRED)

    scandata d;
    d.name = string(name);
    d.rssi = rssi;
    d.mac = string(identifier);

    scanQueue.push(d);

#else

    printf("No handler for scan result: %s with RSSI %i and identifer %s\n", name, rssi, identifier);

#endif

}


void cobble_event_connectionstatus(const char* identifier, int status) {

#if defined(COBBLE_CALLBACK_REALTIME)

    if(connectionstatus_cb != NULL) {
        connectionstatus_cb(identifier, status);
        return;
    }

#elif defined(COBBLE_CALLBACK_DEFERRED)

    connectionstatus st;
    st.identifier = string(identifier);
    st.status = status;

    connectionStatusQueue.push(st);

#else

    printf("No handler for connection status: %s",
        status == ConnectionStatus_DidConnect ? "Did connect" :
        status == ConnectionStatus_DidDisconnect ? "Did disconnect" :
        status == ConnectionStatus_DidConnectFailed ? "Connection failed":
        "Unknown status"
    );

#endif

}

void cobble_event_servicediscovered(const char* uuid) {
    printf("Found service %s\n", uuid);
}

void cobble_event_characteristicdiscovered(const char* svc_uuid, const char* char_uuid) {

#if defined(COBBLE_CALLBACK_REALTIME)

    if(characteristicdiscovered_cb != NULL) {
        characteristicdiscovered_cb(svc_uuid, char_uuid);
        return;
    }

#elif defined(COBBLE_CALLBACK_DEFERRED)

    characteristicdiscovery d;
    d.service = string(svc_uuid);
    d.characteristic = string(char_uuid);

    characteristicDiscoveryQueue.push(d);

#else

    printf("Default handler for characteristic discovery: Service %s, characteristic %s\n", svc_uuid, char_uuid);

#endif
}

void cobble_event_updatevalue(const char* characteristic_uuid, const uint8_t* data, int len) {

#if defined(COBBLE_CALLBACK_REALTIME)

    if(updatevalue_cb != NULL) {
        updatevalue_cb(characteristic_uuid, data, len);
        return;
    }

#elif defined(COBBLE_CALLBACK_DEFERRED)

    if (len > MAX_LENGTH) {
        printf("Warning: Data length %i is larger than the maximum allowed by cobble (%i) - data will be truncated", len, MAX_LENGTH);
    }

    valueupdate v;
    v.characteristic = string(characteristic_uuid);
    v.length = min(MAX_LENGTH, len);

    for (int i = 0; i < v.length; i++) {
        v.data[i] = data[i];
    }

    valueUpdateQueue.push(v);

#else

    printf("Default handler for updated charactistic %s with %i bytes of data, first byte is 0x%02x\n", characteristic_uuid, len, data[0]);

#endif

}

EXPORTED void cobble_queue_process(void) {

#if defined(COBBLE_CALLBACK_DEFERRED)

    while (!scanQueue.empty()) {
        auto r = scanQueue.front();
        scanQueue.pop();
        if (scanresult_cb != nullptr) {
            scanresult_cb(r.name.c_str(), r.rssi, r.mac.c_str());
        }
    }

    while (!connectionStatusQueue.empty()) {
        auto c = connectionStatusQueue.front();
        connectionStatusQueue.pop();
        if (connectionstatus_cb != nullptr) {
            connectionstatus_cb(c.identifier.c_str(), c.status);
        }
    }

    while (!characteristicDiscoveryQueue.empty()) {
        auto d = characteristicDiscoveryQueue.front();
        characteristicDiscoveryQueue.pop();
        if (characteristicdiscovered_cb != nullptr) {
            characteristicdiscovered_cb(d.service.c_str(), d.characteristic.c_str());
        }
    }

    while (!valueUpdateQueue.empty()) {
        auto v = valueUpdateQueue.front();
        valueUpdateQueue.pop();
        if (updatevalue_cb != nullptr) {
            updatevalue_cb(v.characteristic.c_str(), v.data, v.length);
        }
    }

#endif

}