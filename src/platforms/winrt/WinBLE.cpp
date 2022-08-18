
#include <winrt/windows.foundation.h>
#include <winrt/windows.storage.streams.h>
#include <winrt/windows.devices.enumeration.h> // needed otherwise there are weird compile issues relating to un-defined functions (only forward declared): https://devblogs.microsoft.com/oldnewthing/20190529-00/?p=102527
#include <winrt/windows.devices.bluetooth.h>
#include <winrt/windows.devices.bluetooth.advertisement.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>

#include <winrt/windows.foundation.collections.h>

extern "C" {
#include "../../cobble.h"
#include "../../cobble_events.h"
}

using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;

#pragma comment(lib, "windowsapp")

#include <iostream>
#include <winerror.h>
using namespace Windows::Storage::Streams;


CobbleStatus status = Uninitialised;
CobbleErrorCode error = NoError;

// Bluetooth stack state we track
std::list<GattDeviceService> serviceCache;
std::list<GattCharacteristic> characteristicCache;
BluetoothLEDevice currentDevice { nullptr };
BluetoothLEAdvertisementWatcher advWatcher { nullptr };
GattSession sess { nullptr };

__declspec(dllexport) CobbleStatus cobble_status(void) {
	return status;
}

__declspec(dllexport)CobbleErrorCode cobble_error_get(void) {
	return error;
}

void advertisementHandler(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs args) {

	// TODO: Verify this works with Unicode strings
	char short_name[256];
	snprintf(short_name, sizeof(short_name), "%ws", args.Advertisement().LocalName().c_str());

	// Convert from uint64 to upper-case string representation of MAC
	char short_id[256];
	uint8_t mac_bytes[6];
	mac_bytes[0] = (uint8_t)(args.BluetoothAddress() >> (8 * 5));
	mac_bytes[1] = (uint8_t)(args.BluetoothAddress() >> (8 * 4));
	mac_bytes[2] = (uint8_t)(args.BluetoothAddress() >> (8 * 3));
	mac_bytes[3] = (uint8_t)(args.BluetoothAddress() >> (8 * 2));
	mac_bytes[4] = (uint8_t)(args.BluetoothAddress() >> (8 * 1));
	mac_bytes[5] = (uint8_t)(args.BluetoothAddress() >> (8 * 0));
	snprintf(short_id, sizeof(short_id), "%02X:%02X:%02X:%02X:%02X:%02X", mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);

	// Fire the event
	cout << "Adv: Device has name " << short_name << " and address " << short_id << std::endl;
	cobble_event_scanresult(short_name, args.RawSignalStrengthInDBm(), short_id);

}

// Used only if GattSession is used to preserve/resume connections
void sessionStatusChangedHandler(GattSession session, GattSessionStatusChangedEventArgs args) {
	
	cout << "Session status changed to " << (int)args.Status() << std::endl;

	cobble_event_connectionstatus("DEVICE IDENTIFIER GOES HERE", (args.Status() == GattSessionStatus::Active) ? ConnectionStatus_DidConnect : ConnectionStatus_DidDisconnect);
	
	//Active = 1 //The GATT session is active.
	//Closed = 0 //The GATT session is closed.
}

void connectionStatusChangedHandler(BluetoothLEDevice dev, IInspectable unused) {

	int cobble_status_callback = 0;

	switch (dev.ConnectionStatus()) {
	case BluetoothConnectionStatus::Connected:
		cobble_status_callback = ConnectionStatus_DidConnect;
		break;
	case BluetoothConnectionStatus::Disconnected:
		cobble_status_callback = ConnectionStatus_DidDisconnect;
		break;

	}

	// Convert from uint64 to upper-case string representation of MAC
	char short_id[256];
	uint8_t mac_bytes[6];
	mac_bytes[0] = (uint8_t)(dev.BluetoothAddress() >> (8 * 5));
	mac_bytes[1] = (uint8_t)(dev.BluetoothAddress() >> (8 * 4));
	mac_bytes[2] = (uint8_t)(dev.BluetoothAddress() >> (8 * 3));
	mac_bytes[3] = (uint8_t)(dev.BluetoothAddress() >> (8 * 2));
	mac_bytes[4] = (uint8_t)(dev.BluetoothAddress() >> (8 * 1));
	mac_bytes[5] = (uint8_t)(dev.BluetoothAddress() >> (8 * 0));
	snprintf(short_id, sizeof(short_id), "%02X:%02X:%02X:%02X:%02X:%02X", mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);

	cout << "Connection status for device " << short_id << " changed to " << ((cobble_status_callback == ConnectionStatus_DidConnect) ? "Connected" : "Disconnected") << std::endl;

	cobble_event_connectionstatus(short_id, cobble_status_callback);

}



EXPORTED void cobble_init(void)
{
	//winrt::init_apartment();

	std::cout << "Cobble initialised\n";

	status = Initialised;

}

EXPORTED void cobble_deinit(void)
{

	cobble_disconnect();
	cobble_scan_stop();

	std::cout << "Cobble deinitialised\n";

	status = Uninitialised;

}



std::string ToString(winrt::guid guid) {

	char guid_string[37]; // 32 hex chars + 4 hyphens + null terminator
	
	snprintf(
		guid_string, sizeof(guid_string),
		"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2],
		guid.Data4[3], guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);

	return guid_string;
}

std::ostream& operator<<(std::ostream& os, winrt::guid guid) {
	os << ToString(guid);
	return os;
}

// Called in a callback when the service has been discovered.
void discover_characteristics(GattDeviceService s) {
	//std::cout << "Discovering c for s " << s.Uuid() << std::endl;

	IAsyncOperation < GattCharacteristicsResult> res = s.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);

	res.Completed([s](IAsyncOperation< GattCharacteristicsResult> as_async, AsyncStatus as_status) {
		auto res = as_async.GetResults();
		//std::cout << "Service " << s.Uuid() << " has " << res.Characteristics().Size() << " results: " ;

		for (auto c : res.Characteristics()) {
			//std::cout << "CharUUID found " << c.Uuid() << ", ";
			characteristicCache.push_front(c);
			cobble_event_characteristicdiscovered(ToString(s.Uuid()).c_str(), ToString(c.Uuid()).c_str());
			
		}
		//std::cout << std::endl;
	});
}


void DiscoverServices(BluetoothLEDevice dev);

EXPORTED void cobble_connect(const char* identifier) {
	// TODO: It seems that Windows doesn't simply support just connecting to devices? It automatically opens a connection when you interact with a characteristic.
	// It's unclear when this is triggered again - some kind of GC when the number of connections falls to zero across all apps?
	// "Bluetooth LE Explorer" seems to have some degree of control over connections. Clicking on the device opens up a connection (albeit, only if you've recently been connected. If you leave for a while you need to notify.).
	// Pressing Back disconnects after a couple of seconds.
	// May be something to do with a handle being open until it's Closed as part of its IClosable spec?

	uint32_t address[6] = { 0 };

	if (sscanf_s(identifier, "%02X:%02X:%02X:%02X:%02X:%02X", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5]) != 6) {
		std::cout << "Matching address failed: identifier " << identifier << " does not look like a MAC address" << std::endl;
		return;
	}

	uint64_t addr_full = ((uint64_t)address[0] << (8 * 5)) | ((uint64_t)address[1] << (8 * 4)) | ((uint64_t)address[2] << (8 * 3)) | ((uint64_t)address[3] << (8 * 2)) | ((uint64_t)address[4] << (8 * 1)) | ((uint64_t)address[5] << (8 * 0));


	// Create the BluetoothLE device
	std::cout << "CONNECTING TO " << addr_full << std::endl;

	// We must run everything asynchronously - we cannot use get() here because this trips a protection in check_sta_blocking_wait()
	// This is to prevent deadlocks. It is only an issue when running from Unity (as this is on the UI thread).
	IAsyncOperation<BluetoothLEDevice> ao = BluetoothLEDevice::FromBluetoothAddressAsync(addr_full);
	ao.Completed([](IAsyncOperation<BluetoothLEDevice> iao, AsyncStatus as_status) {
		BluetoothLEDevice dev = iao.GetResults();
		BluetoothDeviceId id = dev.BluetoothDeviceId();

		// Discover services for the device
		DiscoverServices(dev);

		// Monitor for connection status changes
		dev.ConnectionStatusChanged(connectionStatusChangedHandler);

#if 1
		// Also create a GattSession
		IAsyncOperation<GattSession> sessionCreation = GattSession::FromDeviceIdAsync(id);
		sessionCreation.Completed([dev](IAsyncOperation<GattSession> sss, AsyncStatus as_status2) {

			sess = sss.GetResults();
			sess.MaintainConnection(true);
			sess.SessionStatusChanged(sessionStatusChangedHandler);

			// Discover services for the device
			//DiscoverServices(dev);

			});
#endif
		}
	);


	cobble_scan_stop(); //TODO: Don't stop scanning until connected?
	status = Connecting;
}

void DiscoverServices(BluetoothLEDevice dev) {

	// Convert from uint64 to upper-case string representation of MAC
	char short_id[256];
	uint8_t mac_bytes[6];
	mac_bytes[0] = (uint8_t)(dev.BluetoothAddress() >> (8 * 5));
	mac_bytes[1] = (uint8_t)(dev.BluetoothAddress() >> (8 * 4));
	mac_bytes[2] = (uint8_t)(dev.BluetoothAddress() >> (8 * 3));
	mac_bytes[3] = (uint8_t)(dev.BluetoothAddress() >> (8 * 2));
	mac_bytes[4] = (uint8_t)(dev.BluetoothAddress() >> (8 * 1));
	mac_bytes[5] = (uint8_t)(dev.BluetoothAddress() >> (8 * 0));
	snprintf(short_id, sizeof(short_id), "%02X:%02X:%02X:%02X:%02X:%02X", mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);


	currentDevice = dev;
	status = Connected;
	cobble_event_connectionstatus(short_id, ConnectionStatus_DidConnect);
	
	std::wcout << "Getting services for device " << dev.Name().c_str() << std::endl;

	// Returns GattDeviceServicesResult
	IAsyncOperation<GattDeviceServicesResult> ao = dev.GetGattServicesAsync(BluetoothCacheMode::Uncached);

	ao.Completed([](IAsyncOperation<GattDeviceServicesResult> as_async, AsyncStatus as_status) {
		auto res = as_async.GetResults();
		std::cout << "Service discovery complete with " << res.Services().Size() << " services discovered." << std::endl;

		for (auto s : res.Services()) {
			serviceCache.push_front(s);
			//std::cout << "Got service " << s.Uuid() << std::endl;
			cobble_event_servicediscovered(ToString(s.Uuid()).c_str());
			discover_characteristics(s);
		}

	});
}


EXPORTED void cobble_disconnect(void) {

	if (sess != nullptr)
		sess.Close();

	if (currentDevice != nullptr)
		currentDevice.Close();

	for (auto s : serviceCache)
		s.Close();

	characteristicCache.clear();
	serviceCache.clear();

}


EXPORTED void cobble_scan_stop() {
	if (advWatcher == nullptr)
		return;
	advWatcher.Stop();
}

EXPORTED void cobble_scan_start(const char* svc_uuids) {

	// TODO: Use svc_uuids
	
	// Listen for actual BLE advertisement packets being sent over the air.
	// Unlike DeviceInformation, it can be used to scan indefinitely.

	advWatcher = BluetoothLEAdvertisementWatcher();
	advWatcher.Received(advertisementHandler);
	try {
		advWatcher.Start();
	}
	catch (hresult_error& ex) {

		if (ex.to_abi() == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE)) {
			cout << "Cannot start scanning - device is not available." << endl;
			status = CobbleError;
			error = HardwareTurnedOff;
		}
		else {
			cout << "Error whilst starting scan: " << ex.code() << endl;
			status = CobbleError;
			error = UnknownError;
		}

		return;
	}

	status = Scanning;

}

int cobble_max_writesize_get(bool withResponse) {
	if (sess != nullptr)
		return sess.MaxPduSize()-3;
	else
		return 20; // Safe but slow
}

void onValueChange(GattCharacteristic const& charateristic, GattValueChangedEventArgs const& args)
{
		//std::wcout << std::hex << "\t\tNotified GattCharacteristic - Guid: [" << ToString(charateristic.Uuid()).c_str() << "]" << std::endl;
		cobble_event_updatevalue(ToString(charateristic.Uuid()).c_str(), args.CharacteristicValue().data(), args.CharacteristicValue().Length());
}

EXPORTED void cobble_subscribe(const char* characteristic) {

	//std::cout << "Subscribing to characteristic " << characteristic << " when there are " << characteristicCache.size() << " items" << std::endl;

	// Get the Characteristic object
	for (auto cc : characteristicCache) {
		if (ToString(cc.Uuid()) == characteristic) {
			//std::cout << "MATCHED " << ToString(cc.Uuid()).c_str() << std::endl;
			
			cc.ValueChanged(onValueChange);

			GattClientCharacteristicConfigurationDescriptorValue dv;

			// If notifications are available, use them. Otherwise, use indications.
			if (((cc.CharacteristicProperties()) & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None) {
				dv = GattClientCharacteristicConfigurationDescriptorValue::Notify;
			}
			else {
				dv = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
			}

			cc.WriteClientCharacteristicConfigurationDescriptorAsync(dv);

			return;
		}
	}

	std::cout << "No match in the cache for characteristic" << characteristic << " when trying to subscribe!" << std::endl;
	return;

}


__declspec(dllexport) void cobble_write(const char* characteristic, uint8_t* data, int len) {

	//std::cout << "Writing " << len << " bytes to characteristic " << characteristic << " when there are " << characteristicCache.size() << " items" << std::endl;

	// Get the Characteristic object
	for (auto cc : characteristicCache) {
		if (ToString(cc.Uuid()) == characteristic) {
			
			// Create an IBuffer around our given data. TODO: Profile this to see if we need something more efficient.
			DataWriter writer;
			array_view av((const uint8_t*)data, (const uint8_t*)(data + len));
			writer.WriteBytes(av);
			IBuffer b = writer.DetachBuffer();
			
			cc.WriteValueAsync(b); // TODO: Something with the asynchronous status result from this op
			return;
		}
	}

	std::cout << "No match in the cache for characteristic " << characteristic << " when trying to write " << len << " bytes!" << std::endl;
	return;

}

EXPORTED void cobble_read(const char* characteristic) {

	//std::cout << "Reading from characteristic " << characteristic << " when there are " << characteristicCache.size() << " items" << std::endl;

	// Get the Characteristic object
	for (auto cc : characteristicCache) {
		if (ToString(cc.Uuid()) == characteristic) {
			IAsyncOperation<GattReadResult> ao = cc.ReadValueAsync();
			ao.Completed([cc](IAsyncOperation<GattReadResult> iao, AsyncStatus as_status) {
				GattReadResult result = iao.GetResults();
				std::cout << "Result: got some bytes " << (result.Value().Length()) << std::endl;
				cobble_event_updatevalue(ToString(cc.Uuid()).c_str(), result.Value().data(), result.Value().Length());
				}
			);
			return;
		}
	}

	std::cout << "No match in the cache for characteristic " << characteristic << " when trying to read!" << std::endl;
	return;

}

