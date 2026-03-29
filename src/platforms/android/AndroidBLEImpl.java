package com.cjb248.cobble;

import android.annotation.TargetApi;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.ParcelUuid;
import android.util.Log;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

@TargetApi(26)
public class AndroidBLEImpl {

    private static enum GattOperationType {
        SubscribeCharacteristic,
        ReadCharacteristic,
        WriteCharacteristic
    }

    private static class QueuedGattOperation {
        BluetoothGattCharacteristic c;
        byte[] data;
        GattOperationType operation;
        public QueuedGattOperation(BluetoothGattCharacteristic characteristic, byte[] characteristicData, GattOperationType operationType) {
            c = characteristic;
            data = characteristicData;
            operation = operationType;
        }
    }

    private static BluetoothAdapter mBluetoothAdapter;

    private static boolean initialised = false;

    // No special meaning, just a request code that we provide
    private static final int REQUEST_ENABLE_BT = 1;

    private static BluetoothLeScanner mLEScanner;
    private static ScanSettings scanSettings;
    private static List<ScanFilter> scanFilters;
    private static BluetoothGatt mGatt;

    private static HashMap<String, BluetoothDevice> deviceCache = new HashMap<String, BluetoothDevice>();
    private static HashMap<String, BluetoothGattCharacteristic> characteristicCache = new HashMap<String, BluetoothGattCharacteristic>();
    private static String currentDeviceIdentifier;
    private static BlockingQueue<QueuedGattOperation> queuedOperations = new ArrayBlockingQueue<>(1024); //Allocate memory in advance
    private static boolean operationInProgress = false;

    protected static final UUID CHARACTERISTIC_UPDATE_NOTIFICATION_DESCRIPTOR_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private static native void scanresult(String name, int RSSI, String identifier);
    private static native void characteristicupdate(String uuid, byte[] packet, String identifier);
    private static native void characteristicdiscovered(String svc_uuid, String char_uuid);

    private static native void Connected(String name);
    private static native void Disconnected(String name);
    private static native void ConnectError(String name);

    //Interface our status to the native code.
    private static native void SetStatus(int status);
    private static native void SetError(int error);

    private static final int Status_Uninitialised = 0;
    private static final int Status_Initialised = 1;
    private static final int Status_Scanning = 2;
    private static final int Status_Connecting = 3;
    private static final int Status_Connected = 4;

    private static final int Error_NoError = 0;
    private static final int Error_HardwareUnsupported = 1;
    private static final int Error_HardwareTurnedOff = 2;
    private static final int Error_PermissionsNotGranted = 3;
    
    private static Activity currentActivity;
    
    private static Activity getCurrentActivity() {
        
        if(currentActivity == null) {
            
            try {
            
                Class player = Class.forName("com.unity3d.player.UnityPlayer");
                Field field = player.getDeclaredField("currentActivity");
                currentActivity = (Activity)field.get(null); //Getting a static field, no need to give an object reference
                
            } catch (ClassNotFoundException ex) {
                Log.e("BLEImpl", "Could not find 'com.unity3d.player.UnityPlayer'");
            } catch (NoSuchFieldException ex) {
                Log.e("BLEImpl", "Could not find field 'currentActivity' in 'com.unity3d.player.UnityPlayer'");
            } catch (IllegalAccessException ex) {
                Log.e("BLEImpl", "Not allowed to access field 'currentActivity' in 'com.unity3d.player.UnityPlayer'");
            }
            
        }
        
        return currentActivity;
    }

    private static void cobble_init() {

        if (initialised)
            return; // Prevent double initialisation
 
        Log.i("BLEImpl", "Initialising Bluetooth plugin...");

        if (!getCurrentActivity().getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Log.e("BLEImpl", "BLE Not Supported");
            SetError(Error_HardwareUnsupported);
            return;
        }

        getCurrentActivity().registerReceiver(mReceiver, new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED), 4); // 4: RECEIVER_NOT_EXPORTED

        final BluetoothManager bluetoothManager = (BluetoothManager) getCurrentActivity().getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = bluetoothManager.getAdapter();

        scanSettings = new ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                    .build();
        scanFilters = new ArrayList<ScanFilter>();

        if (mBluetoothAdapter == null || !mBluetoothAdapter.isEnabled()) {
            Log.e("BLEImpl", "BLE adapter is disabled!");
            SetError(Error_HardwareTurnedOff);
            return;
        }

        Log.i("BLEImpl", "Creating BLE scanner...");
        mLEScanner = mBluetoothAdapter.getBluetoothLeScanner();

        SetStatus(Status_Initialised);
        initialised = true;

    }

    private static void cobble_deinit() {

        if(!initialised)
            return; // Already in deinitialised state, no need to do anything

        cobble_disconnect();
        cobble_scan_stop();
        mBluetoothAdapter = null;
        scanSettings = null;
        scanFilters = null;
        mLEScanner = null;
        deviceCache.clear();
        getCurrentActivity().unregisterReceiver(mReceiver);
        initialised = false;
        SetStatus(Status_Uninitialised);
        
    }

    private static void cobble_connect(String identifier) {

        Log.d("BLEImpl", "Connecting to " + identifier);
        BluetoothDevice dev = deviceCache.get(identifier);

        if(dev == null) {
            Log.e("BLEImpl", "Cannot connect to device " + identifier + " - not found in cache?");
            return;
        }

        if (mGatt != null) {
            Log.e("BLEImpl", "Cannot connect to a new device, we already have mGatt");
            return;
        }

        currentDeviceIdentifier = identifier;
        SetStatus(Status_Connecting);
        mGatt = dev.connectGatt(getCurrentActivity(), false, gattCallback, BluetoothDevice.TRANSPORT_LE);
    
    }

    private static void cobble_disconnect() {

        if(mGatt == null) {
            Log.e("BLEImpl", "mGatt is null, cannot disconnect again.");
            return;
        }

        Log.i("BLEImpl", "Disconnecting...");
        mGatt.disconnect();

        // We do NOT clean up mGatt or anything else here, cleanup will be handled in the onConnectionStateChange callback
        // If we did, the onConnectionStateChange callback would also removed, and so Cobble (and by extension the app) 
        // would never be informed of the disconnection.

    }

    //BLE library is asynchronous, and read operations can fail if another read or write is pending
    //The internal flag "mDeviceBusy" is not exposed directly though, we just have to try and fail,
    //or attempt to track its state by setting a flag on read/write and clearing it in callback
    //when the operation completes.

    //NOTE: This is a single flag, shared between 4 types of operations: both read and write, of both characteristics and descriptors

    private static void addToQueueAndProcess(QueuedGattOperation op) {

        try {
            if(!queuedOperations.offer(op, 0, TimeUnit.MILLISECONDS)) {
                Log.e("BLEImpl", "Failed to add the given operation to the queue");
            }
        } catch (InterruptedException e) {
            Log.e("BLEImpl", "Interrupted whilst queueing the write");
        }        

        processQueuedOperations();

    }

    private static void cobble_write(String characteristicUuidString, byte[] data) {

        BluetoothGattCharacteristic c = characteristicCache.get(characteristicUuidString.toUpperCase());

        if(c == null) {
            Log.e("BLEImpl", "Failed to find " + characteristicUuidString + " in cache when trying to write.");
            return;
        }

        QueuedGattOperation writeOp = new QueuedGattOperation(c, data, GattOperationType.WriteCharacteristic);
        addToQueueAndProcess(writeOp);

    }

    private static void processQueuedOperations() {

        //If a write is already in process, don't need to start another - the next item will be handled on callback when the previous one completes
        if(operationInProgress)
            return;

        //Take a single item from the queue
        QueuedGattOperation op;
        try {
            op = queuedOperations.poll(0, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Log.e("BLEImpl", "Interrupted whilst trying to process queued writes");
            return;
        }

        //If the queue is empty, nothing to do
        if(op == null)
            return;

        boolean success = false;
        operationInProgress = true;

        // Process the queued operation according to its type
        switch(op.operation) {
            case WriteCharacteristic:
            {
                //Tell the OS to perform the write
                op.c.setValue(op.data);
                success = mGatt.writeCharacteristic(op.c);
                break;
            }
            case ReadCharacteristic:
            {
                success = mGatt.readCharacteristic(op.c);
                break;
            }
            case SubscribeCharacteristic:
            {
                // FIXME: Allow unsubscription
                boolean enable = true;

                //Set up notifications locally. Despite the name, this also works with indications
                mGatt.setCharacteristicNotification(op.c, enable);

                //Also tell the remote device that we want notifications or indications
                //See https://stackoverflow.com/a/32184537

                BluetoothGattDescriptor descriptor = op.c.getDescriptor(CHARACTERISTIC_UPDATE_NOTIFICATION_DESCRIPTOR_UUID);

                // Default to notification, if the characteristic supports both
                if((op.c.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0) {
                    descriptor.setValue(enable ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE : BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE);
                } else {
                    descriptor.setValue(enable ? BluetoothGattDescriptor.ENABLE_INDICATION_VALUE : BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE);
                }
                
                success = mGatt.writeDescriptor(descriptor);
                break;
            }
        }

        if(!success) {
            Log.e("BLEImpl", "Failed to prepare an operation of type " + op.operation.name() + " on characteristic " + op.c.getUuid().toString() + " - permissions wrong or another operation pending?");
            //Could also be bad device, service, permissions, etc

            // Process the next operation in the queue
            operationInProgress = false;
            processQueuedOperations();
        } else {
            Log.i("BLEImpl", "Succeeded in preparing operation of type " + op.operation.name() + " on characteristic " + op.c.getUuid().toString());
        }

    }

    private static void cobble_read(String characteristicUuidString) {

        BluetoothGattCharacteristic c = characteristicCache.get(characteristicUuidString.toUpperCase());   

        if(c == null) {
            Log.e("BLEImpl", "Failed to find " + characteristicUuidString + " in cache when trying to read.");
            return;
        }

        QueuedGattOperation readOp = new QueuedGattOperation(c, null, GattOperationType.ReadCharacteristic);
        addToQueueAndProcess(readOp);

    }

    protected static void cleanupConnection() {

        if (mGatt != null) {
            mGatt.close();
            mGatt = null;
        }
        
        currentDeviceIdentifier = null;
        characteristicCache.clear();
        queuedOperations.clear();
        operationInProgress = false;

    }

    private static void cobble_scan_start(String service_uuids) {
        Log.d("BLEImpl", "cobble_scan_start");
        
        if(mLEScanner == null) {
            Log.e("BLEImpl", "Cannot start scanning, mLEScanner is null");
            return;
        }
        scanFilters.clear();
        if(service_uuids != null && !service_uuids.equals("")) {
            String[] uuid_strings = service_uuids.split(",");
            for(String s: uuid_strings) {
                scanFilters.add(new ScanFilter.Builder().setServiceUuid(ParcelUuid.fromString(s)).build());
            }
        }

        SetStatus(Status_Scanning);
        mLEScanner.startScan(scanFilters, scanSettings, scanCallback);
    }

    private static void cobble_scan_stop() {
        Log.d("BLEImpl", "cobble_scan_stop");

        if(mLEScanner == null) {
            Log.e("BLEImpl", "Cannot stop scanning, mLEScanner is null");
            return;
        }

        SetStatus(Status_Initialised);
        mLEScanner.stopScan(scanCallback);
    }

    private static ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            String devName = result.getScanRecord().getDeviceName();
            int RSSI = result.getRssi();
            String identifier = result.getDevice().getAddress();
            if (devName != null) { //TODO: Handle nameless devices correctly here...
                // Log.i("BLEImpl", "Scan result: " + devName);
                //Log.i("BLEImpl", "callbackType " + String.valueOf(callbackType));
                //Log.i("BLEImpl", "result " + result.toString());
                scanresult(devName, RSSI, identifier);
                deviceCache.put(identifier, result.getDevice());
            }
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            for (ScanResult sr : results) {
                Log.i("BLEImpl", "BatchScanResults: " + sr.toString());
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            Log.e("BLEImpl", "Scan Failed, error: " + errorCode);
        }
    };

    public static void cobble_subscribe(String characteristicUuidString) {

        BluetoothGattCharacteristic c = characteristicCache.get(characteristicUuidString.toUpperCase());

        if(c == null) {
            Log.e("BLEImpl", "Failed to find " + characteristicUuidString + " in cache when trying to subscribe.");
            return;
        }

        QueuedGattOperation subscribeOp = new QueuedGattOperation(c, null, GattOperationType.SubscribeCharacteristic);
        addToQueueAndProcess(subscribeOp);

    }

    /*
     * Callbacks from the BLE stack
     */

    private static final BroadcastReceiver mReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive (Context context, Intent intent) {
            String action = intent.getAction();

            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {
                switch(intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)) {
                    case BluetoothAdapter.STATE_TURNING_OFF:
                    case BluetoothAdapter.STATE_OFF:
                        Log.e("BLEImpl", "BluetoothAdapter transitioned to STATE_TURNING_OFF or STATE_OFF unexpectedly");
                        if(currentDeviceIdentifier != null) {
                            ConnectError(currentDeviceIdentifier);
                            cleanupConnection();
                        }
                        SetError(Error_HardwareTurnedOff);
                        break;
                    case BluetoothAdapter.STATE_TURNING_ON:
                        Log.i("BLEImpl", "BluetoothAdapter transitioned to STATE_TURNING_ON");
                        break;
                    case BluetoothAdapter.STATE_ON:
                        Log.i("BLEImpl", "BluetoothAdapter transitioned to STATE_ON");
                        break;
                }
            }
        }
    };

    private static final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {

            switch (newState) {

                case BluetoothProfile.STATE_CONNECTED:
                    Log.i("BLEImpl", "GATT Callback: Connected");
                    SetStatus(Status_Connected);
                    Connected(currentDeviceIdentifier);
                    gatt.discoverServices();
                    break;

                case BluetoothProfile.STATE_DISCONNECTED:
                    Log.i("BLEImpl", "GATT Callback: Disconnected");

                    if(status == BluetoothGatt.GATT_SUCCESS) {
                        SetStatus(Status_Initialised); // Clean disconnection
                        Disconnected(currentDeviceIdentifier);
                    } else {
                        //Timeout or other GATT errors may result in status=133
                        Log.e("BLEImpl", "GATT Connection State error: " + status);

                        // Put the Bluetooth stack in to a known state
                        cobble_scan_stop();
                        SetStatus(Status_Initialised);

                        // Report the error to the app
                        ConnectError(currentDeviceIdentifier);
                    }
                    cleanupConnection();
                    break;

                default: //Connecting / Disconnecting
                    Log.e("BLEImpl", "Unhandled GATT Callback state: " + newState);

            }

        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            
            List<BluetoothGattService> services = gatt.getServices();
            Log.i("BLEImpl", "Discovered " + services.size() + " services, now caching and reporting characteristics.");
            
            for(BluetoothGattService s : services) {
                for(BluetoothGattCharacteristic c : s.getCharacteristics()) {
                    characteristicCache.put(c.getUuid().toString().toUpperCase(), c);
                    characteristicdiscovered(s.getUuid().toString(), c.getUuid().toString());
                }
            }

        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            
            Log.i("BLEImpl", "onCharacteristicWrite " + characteristic.toString() + " " + characteristic.getUuid());

            //Handle next event in queue if required
            operationInProgress = false;
            processQueuedOperations();

        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            
            Log.i("BLEImpl", "onCharacteristicRead " + characteristic.toString() + " " + characteristic.getUuid());

            characteristicupdate(characteristic.getUuid().toString(), characteristic.getValue(), gatt.getDevice().getAddress());

            //Handle next event in queue if required
            operationInProgress = false;
            processQueuedOperations();

        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            
            Log.i("BLEImpl", "onCharacteristicChanged " + characteristic.getUuid());

            characteristicupdate(characteristic.getUuid().toString(), characteristic.getValue(), gatt.getDevice().getAddress());

        }

        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {

            Log.i("BLEImpl", "onDescriptorWrite");

            //Handle next event in queue if required
            operationInProgress = false;
            processQueuedOperations();

        }
    };

}
