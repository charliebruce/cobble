#include <dbus/dbus.h>

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <unistd.h>

using namespace std;

DBusError dbus_error;
DBusConnection * dbus_conn = nullptr;
DBusMessage * dbus_msg = nullptr;
DBusMessage * dbus_reply = nullptr;

void dbus_cleanup(bool err) {
    
    if(dbus_reply != nullptr)
        dbus_message_unref(dbus_reply);
    
    if(dbus_msg != nullptr)
        dbus_message_unref(dbus_msg);
    
    /*
     * Applications must not close shared connections -
     * see dbus_connection_close() docs. This is a bug in the application.
     */
    //::dbus_connection_close(dbus_conn);

    if(dbus_conn != nullptr)
        dbus_connection_unref(dbus_conn);
        
    if(err) {
        perror(dbus_error.name);
        perror(dbus_error.message);
    }
}

bool dict_open(DBusMessageIter *iter, DBusMessageIter *iter_dict) {
    if (!iter || !iter_dict)
        return false;
 
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY || dbus_message_iter_get_element_type(iter) != DBUS_TYPE_DICT_ENTRY)
        return false;

    dbus_message_iter_recurse(iter, iter_dict);
    return true;
}

bool dict_has_entry(DBusMessageIter *iter_dict) {
    if (!iter_dict)
        return false;
    
    return dbus_message_iter_get_arg_type(iter_dict) == DBUS_TYPE_DICT_ENTRY;
}

int main(int argc, char * argv[]) {

    (void)argc;
    (void)argv;
    
    //const char * dbus_result = nullptr;

    // Initialize D-Bus error
    dbus_error_init(&dbus_error);

    // Connect to D-Bus
    if ( nullptr == (dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) ) {
        dbus_cleanup(true);
        return -1;
    }
    
    // TODO Connect to signals of interest
    //dbus_bus_add_match(dbus_conn, "type='signal',interface='test.signal.Type'", &err);
    
    // Compose remote procedure call
    if ( nullptr == (dbus_msg = dbus_message_new_method_call("org.bluez", "/org/bluez/hci0", "org.bluez.Adapter1", "StartDiscovery")) ) {
        dbus_cleanup(true);
        ::perror("ERROR: ::dbus_message_new_method_call - Unable to allocate memory for the message!");
        return -2;
    }
        
    // Invoke remote procedure call, block for response
    if ( nullptr == (dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &dbus_error)) ) {
        dbus_cleanup(true);
        return -3;
    }

    // Parse response
    if ( !dbus_message_get_args(dbus_reply, &dbus_error, DBUS_TYPE_INVALID) ) {
        dbus_cleanup(true);
        return -4;
    }
    
    // TODO: Inspect the reply for errors

    // Work with the results of the remote procedure call
    cout << "Connected to D-Bus as \"" << dbus_bus_get_unique_name(dbus_conn) << "\"." << endl;
    
    // We're done with the first message and the reply
    dbus_message_unref(dbus_msg);
    dbus_message_unref(dbus_reply);

    // Adapter is scanning. Wait 5s for results to arrive.
    sleep(5);
    
    // Request from org.bluez with a name matching the expected format
    // /org/bluez/hci0/dev_(MAC)
    // Print the list for debug.
    // Use org.freedesktop.DBus.ObjectManager to discover everything.
    // This returns a complex dict structure, which can only be iterated over (not read in one go)
    // We only need the keys of the parent to list devices.
    
    // Compose remote procedure call
    if ( nullptr == (dbus_msg = dbus_message_new_method_call("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects")) ) {
        dbus_cleanup(true);
        perror("ERROR: dbus_message_new_method_call - Unable to allocate memory for the second message!");
        return -5;
    }
    
    // Invoke remote procedure call, block for response
    if ( nullptr == (dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &dbus_error)) ) {
        dbus_cleanup(true);
        return -6;
    }
    
    // We get a dictionary back.
    // This is serialised as an array of Dict Entries, which themselves consist of a key/value pair
    // For convenience, we can wrap these up in dict-specific code.
    DBusMessageIter iter;
    DBusMessageIter iter_dict;
    dbus_message_iter_init(dbus_reply, &iter);
    
    if(!dict_open(&iter, &iter_dict))
    {
        dbus_cleanup(true);
        return -7;
    }
    
    int entries=0;
    while(dict_has_entry(&iter_dict)) {
        entries++;
        
        DBusMessageIter iter_dict_entry;
        dbus_message_iter_recurse(&iter_dict, &iter_dict_entry);
        char* key;
        dbus_message_iter_get_basic(&iter_dict_entry, &key);
        cout<<key<<endl;
        dbus_message_iter_next(&iter_dict_entry);
        // Should now be in the Value - a VARIANT
        
        //we have variant now, do something with contents?
        dbus_message_iter_next(&iter_dict);
    }
    
    cout << "Got a total of " << entries << endl;
    
    dbus_cleanup(false);
    return 0;
}


