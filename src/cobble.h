// Common Bindings for Bluetooth LE
#include <stdint.h>
#include <stdbool.h>


// Compatibility with Windows
#if defined(_WIN32) || defined(_WIN64)
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

// UUIDs are used to identify services and characteristics in BLE
// We pass these around as strings for simplicity
#ifdef __cplusplus
extern "C" {
#endif

EXPORTED void cobble_init(void);
EXPORTED void cobble_deinit(void);


// service_uuids should be a service UUID, or comma-separated list of service UUIDs
// Scanning will return only results advertising one or more of the given service UUID(s).
// This seems to be required for scanning on macOS Monterey (12.0, 12.1) due to a macOS bug.
// This bug should be fixed in later versions of macOS Monterey (since 12.3 beta 21E5212f).
// Other platforms will accept null/empty strings, and will return all scan results.
// In fact, the Windows version of the plugin will totally ignore the filter for now.
EXPORTED void cobble_scan_start(const char* service_uuids);
EXPORTED void cobble_scan_stop(void);


EXPORTED void cobble_loop(void);

EXPORTED void cobble_connect(const char* identifier);
EXPORTED void cobble_disconnect(void);

EXPORTED void cobble_characteristics_get(void);

// Ask to be notified when a characteristic's value changes.
// This can be done either as a Notification, or an Indication
// - Notification does not require confirmation, and so is fast but could occasionally get lost.
// - Indication requires confirmation, and so is slower but more reliable.
// If a characteristic supports both notifications and indications:
// - Linux BlueZ favours notifications: https://github.com/bluez/bluez/blob/7c3ca2a6b940d36c553fabe38066fabc66530dc9/src/shared/gatt-client.c#L1594-L1602
// - macOS/iOS CoreBluetooth does not document this behaviour, so should be considered undefined (application has no choice)
// - Currently, the Android implementation only supports Notifications (application can choose)
// - Windows allows the application to choose - Cobble chooses Notifications when possible
EXPORTED void cobble_subscribe(const char* char_uuid);

EXPORTED void cobble_read(const char* char_uuid);
EXPORTED void cobble_write(const char* char_uid, uint8_t* data, int len);

EXPORTED int cobble_max_writesize_get(bool withResponse);

typedef enum {
    Uninitialised = 0,
    Initialised,
    Scanning,
    Connecting,
    Connected,
    CobbleError
} CobbleStatus;

EXPORTED CobbleStatus cobble_status(void);


typedef enum {
    NoError = 0,
    HardwareUnsupported,
    HardwareTurnedOff,      // Bluetooth adapter disabled. Can occur at initialisation or at scan start, depending on platform.
    PermissionsNotGranted,
    UnknownError = 0xFF
} CobbleErrorCode;

EXPORTED CobbleErrorCode cobble_error_get(void);


//Threading and delegate handling
void cobble_shutdown(void);
void cobble_loop(void);

//This function can be used, along with a Cobble build that has defined COBBLE_CALLBACK_DEFERRED, to process events at a specific time on a specific thread.
//This is used on Windows with Unity (otherwise we see lockups and crashes)
EXPORTED void cobble_queue_process(void);

#ifdef __cplusplus
}
#endif
