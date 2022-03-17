// This library can be used in two ways: Callback or Polling
// Here, the Callback method is shown

//This client app demonstrates the following:
// * Receive scan results, including RSSI
// * Select a device by name
// * Connect to the device
// * TODO: More

#include "cobble.h"
#include "cobble_events.h"

#if defined(_WIN32) || defined (_WIN64)
#include <windows.h>
#else
#include <pthread.h> 
#endif
#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#ifdef WIN32
void usleep(unsigned int usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * (__int64)usec);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#else
// Provides usleep
#include <unistd.h>
#endif

//Services and characteristics are presented in upper-case form, with hyphens for full IDs
#define beelineService "C5D70001-C45D-4F12-8693-7EF838E96446"
#define beelineWrCharacteristic "C5D70002-C45D-4F12-8693-7EF838E96446"
#define beelineRdCharacteristic "C5D70003-C45D-4F12-8693-7EF838E96446"


volatile bool connecting = false;

//When we find the device of interest, connect to it. 
//Once connected, the library will automatically stop scanning and attempt to discover all services and characteristics
void on_scanresult(const char* name, int RSSI, const char* identifier) {
   
   if(!name)
        return;

    if(!connecting && (strcmp("BeelineMoto DF90", name) == 0)) {
        
        connecting = true;

        printf("\n\n --- Identified a Beeline, RSSI: %i!\n\n", RSSI);
        
        cobble_connect(identifier);

    }
}

//Once the characteristic of interest has been discovered, we can subscribe to it
void on_characteristicdiscovered(const char* svc_uuid, const char* char_uuid) {

    if(!svc_uuid || !char_uuid)
        return;

    if(strcmp(char_uuid, beelineRdCharacteristic) == 0) {
        printf("Found the Read characteristic - subscribing to it...\n");
        cobble_subscribe(char_uuid);
    }

}

void on_updatevalue(const char* id, const uint8_t* data, int len) {

    printf("Received data from device with length %i\n", len);
}

void mainthread(void) {

    printf("Initialising Bluetooth now. If the application crashes, you will need to grant your Terminal application permission to use Bluetooth, under System Preferences -> Security & Privacy -> Privacy -> Bluetooth\n");

    //Start the initialisation process
    cobble_init();

    //Wait until the initialisation process is complete
    //Other work could be done now / this could be done asynchronously or on a callback
    while(cobble_status() != Initialised) {
        if(cobble_status() == CobbleError) {
            printf("An error was encountered whilst starting.\n");
            return;
        }
    }
    printf("Initialised\n");

    //Register callbacks for all the events we are interested in
    register_scanresult_cb(&on_scanresult);
    register_characteristicdiscovered_cb(&on_characteristicdiscovered);
    register_updatevalue_cb(&on_updatevalue);

    cobble_scan_start(beelineService);

    while(cobble_status() != Scanning) {
        if(cobble_status() == CobbleError) {
            printf("An error was encountered whilst scanning.\n");
            return;
        }
    }
    printf("Scanning started...\n");

    int timeout = 30 * 1000000;
    while(timeout > 0 && cobble_status() == Scanning) {
#ifndef __APPLE__
        cobble_queue_process();
#endif
        usleep(10000); //10ms
        timeout -= 10000;
    }

    printf("No longer scanning...\n");

    int time = 0;
    while(cobble_status() == Connecting || cobble_status() == Connected) {
#ifndef __APPLE__
        cobble_queue_process();
#endif
        usleep(10000); //10ms

        time++;

        if(time > 100) {
            printf("Updating\n");
            time = 0;
                uint8_t uiGoPage = 13;
    uint8_t data[] = {uiGoPage, 0};
    cobble_write(beelineWrCharacteristic, data, sizeof(data));
        }

    }



    printf("No longer connecting/ed...\n");


    return;
}

#ifdef WIN32
DWORD WINAPI runner(void* data) {
#else
void *runner(void* vargp) {
#endif
    
    (void) vargp; // Unused parameter

    mainthread();
//    cobble_shutdown();    
    return NULL;
}


//Test process
int main(void) {

    printf("Starting BLE Scan demo...\n"); 

    //Launch the main logic on another thread
#ifdef WIN32
    HANDLE thread = CreateThread(NULL, 0, runner, NULL, 0, NULL);
#else
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, runner, NULL);
#endif

    //Handle delegate callbacks on main thread
#ifdef __APPLE__
    cobble_loop();
#endif
    //cobble_queue_process();

#ifdef WIN32
    WaitForSingleObject(thread, INFINITE);
#else
    pthread_join(thread_id, NULL); 
#endif
    printf("BLE Scan demo complete...\n"); 
    
}
