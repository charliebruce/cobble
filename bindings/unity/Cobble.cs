//Cobble presents Bluetooth LE devices to Unity, both in Editor and in builds
//Note that when making changes to the plugin, you will need to close and re-open the plugin - Unity loads the library the first time it is used
//and does not release it after exiting play mode.

//TODO: Do we need __cdecl / CallingConvention = CallingConvention.Cdecl / [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
using System;
using System.Runtime.InteropServices;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using AOT;
using System.Collections.Concurrent;
#if UNITY_EDITOR
using UnityEditor;
#endif
#if UNITY_ANDROID
using UnityEngine.Android;
#endif

public class Cobble : MonoBehaviour {

#if UNITY_IOS && !UNITY_EDITOR
    private const string PLUGIN_NAME = "__Internal";
#else
    private const string PLUGIN_NAME = "Cobble";
#endif

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_init();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_deinit();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_scan_start(string service_uuids);
    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_scan_stop();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_loop();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_connect(string identifier);
    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_disconnect();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_characteristics_get();

    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_subscribe(string char_uuid);
    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_read();
    [DllImport(PLUGIN_NAME)]
    private static extern void cobble_write(string char_uid, IntPtr data, int len);


    /*
     * Callback function signatures / registration
     */

    private delegate void scanresult_cb(string name, int rssi, string identifier);
    [DllImport(PLUGIN_NAME)]
    private static extern void register_scanresult_cb(scanresult_cb p);

    private delegate void characteristicdiscovered_cb(string svc_uuid, string char_uuid);
    [DllImport(PLUGIN_NAME)]
    private static extern void register_characteristicdiscovered_cb(characteristicdiscovered_cb p);

    private delegate void updatevalue_cb(string characteristic_uuid, IntPtr data, int len);
    [DllImport(PLUGIN_NAME)]
    private static extern void register_updatevalue_cb(updatevalue_cb p);




    private static void cobble_event_characteristicdiscovered(string svc_uuid, string char_uuid)
    {
        Debug.Log("On Characteristic Discovered: " + svc_uuid +", " + char_uuid);

        if(char_uuid.Equals("C5D70003-C45D-4F12-8693-7EF838E96446"))
        {
            cobble_subscribe("C5D70003-C45D-4F12-8693-7EF838E96446");
        }
    }
    //private static void cobble_event_connectionstatus(string identifier, int status)
    //{
    //    Debug.Log("On Connection Status: " + identifier);
    //}
    //private static void cobble_event_servicediscovered(string uuid)
    //{
    //    Debug.Log("On Service Discovered: " + uuid);
    //}
    private static void cobble_event_updatevalue(string characteristic_uuid, IntPtr data, int len)
    {
        Debug.Log("Update Value: " + characteristic_uuid);
    }




    private static void cobble_event_scanresult(string name, int rssi, string identifier) {

        //Debug.Log("Found " + name + ", RSSI " + rssi + ", identifier " + identifier);
        ScanResult sr = new ScanResult
        {
            name = name,
            rssi = rssi,
            identifier = identifier
        };
        scanResults.Enqueue(sr); //Allow events to be processed on main thread in Update


    }

    /*
     * Application Specific
     */


    static bool connecting = false;

    class ScanResult
    {
        public string name;
        public int rssi;
        public string identifier;
    }

    private static ConcurrentQueue<ScanResult> scanResults;




    private bool loaded = false;

    private int frames = 0;

    public void Awake()
    {
        scanResults = new ConcurrentQueue<ScanResult>();
    }

    public void RegisterCallbacks()
    {
        register_scanresult_cb(cobble_event_scanresult);
        register_characteristicdiscovered_cb(cobble_event_characteristicdiscovered);
        register_updatevalue_cb(cobble_event_updatevalue);
    }

    public void UnregisterCallbacks()
    {
        register_scanresult_cb(null);
        register_characteristicdiscovered_cb(null);
        register_updatevalue_cb(null);
    }

    public void Update()
    {
        frames++;

        if(!loaded)
        {
            cobble_init();
            Debug.Log("Cobble initialised"); 
            RegisterCallbacks();
            loaded = true;
        }

        if(frames == 600) //TODO: Await cobble_status switching to Initialised (or Error)
        { 
            cobble_scan_start();
        }

        ScanResult res;
        while(scanResults.TryDequeue(out res))
        {
            //Debug.Log("MainThread found " + res.name);

            if(!connecting && res.name == "BeelineMoto E310")
            {
                connecting = true;
                Debug.Log("Connecting to moto");
                cobble_connect(res.identifier);
            }
        }

    }

    public void OnDestroy()
    {
        cobble_scan_stop();
        cobble_deinit();
    }

#if UNITY_EDITOR

    void OnBeforeAssemblyReload()
    {
        Debug.LogWarning("Assemblies are being reloaded - plugin is unregistering callbacks to prevent a crash. Bluetooth may no longer work!");
        UnregisterCallbacks();
    }

    private void OnEnable()
    {
        AssemblyReloadEvents.beforeAssemblyReload += OnBeforeAssemblyReload;
    }

    private void OnDisable()
    {
        AssemblyReloadEvents.beforeAssemblyReload -= OnBeforeAssemblyReload;
    }

#endif
}