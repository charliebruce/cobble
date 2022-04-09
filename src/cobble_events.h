
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Compatibility with Windows
#if defined(_WIN32) || defined(_WIN64)
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif


typedef void (*scanresult_funcptr)(const char*, int, const char*);
EXPORTED void register_scanresult_cb(scanresult_funcptr p);

typedef void (*characteristicdiscovered_funcptr)(const char*, const char*);
EXPORTED void register_characteristicdiscovered_cb(characteristicdiscovered_funcptr p);

typedef void (*updatevalue_funcptr)(const char*, const uint8_t*, int);
EXPORTED void register_updatevalue_cb(updatevalue_funcptr p);

typedef void (*connectionstatus_funcptr)(const char*, int);
EXPORTED void register_connectionstatus_cb(connectionstatus_funcptr p);

typedef enum {
    ConnectionStatus_DidDisconnect,
    ConnectionStatus_DidConnect,
    ConnectionStatus_DidConnectFailed,
} ConnectionStatus;

//Called by the platform-specific implementations
void cobble_event_scanresult(const char* name, int rssi, const char* identifier);
void cobble_event_characteristicdiscovered(const char* svc_uuid, const char* char_uuid);
void cobble_event_connectionstatus(const char* identifier, int status);
void cobble_event_servicediscovered(const char* uuid);
void cobble_event_updatevalue(const char* characteristic_uuid, const uint8_t* data, int len);

#ifdef __cplusplus
}
#endif