#include <dbus/dbus.h>

#include <cstddef>
#include <cstdio>
#include <iostream>

DBusError dbus_error;
DBusConnection * dbus_conn = nullptr;
DBusMessage * dbus_msg = nullptr;
DBusMessage * dbus_reply = nullptr;

void dbus_cleanup(bool err) {
    
    if(dbus_reply != nullptr)
        ::dbus_message_unref(dbus_reply);
    
    if(dbus_msg != nullptr)
        ::dbus_message_unref(dbus_msg);
    
    /*
     * Applications must not close shared connections -
     * see dbus_connection_close() docs. This is a bug in the application.
     */
    //::dbus_connection_close(dbus_conn);

    if(dbus_conn != nullptr)
        ::dbus_connection_unref(dbus_conn);
        
    if(err) {
        ::perror(dbus_error.name);
        ::perror(dbus_error.message);
    }
}

int main(int argc, char * argv[]) {

    (void)argc;
    (void)argv;
    
    const char * dbus_result = nullptr;

    // Initialize D-Bus error
    ::dbus_error_init(&dbus_error);

    // Connect to D-Bus
    if ( nullptr == (dbus_conn = ::dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)) ) {
        dbus_cleanup(true);
        return -1;
    }

    // Compose remote procedure call
    if ( nullptr == (dbus_msg = ::dbus_message_new_method_call("org.bluez", "/org/bluez/hci0", "org.freedesktop.DBus.Introspectable", "Introspect")) ) {
        dbus_cleanup(true);
        ::perror("ERROR: ::dbus_message_new_method_call - Unable to allocate memory for the message!");
        return -2;
    }
        
    // Invoke remote procedure call, block for response
    if ( nullptr == (dbus_reply = ::dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &dbus_error)) ) {
        dbus_cleanup(true);
        return -3;
    }

    // Parse response
    if ( !::dbus_message_get_args(dbus_reply, &dbus_error, DBUS_TYPE_STRING, &dbus_result, DBUS_TYPE_INVALID) ) {
        dbus_cleanup(true);
        return -4;
    }

    // Work with the results of the remote procedure call
    std::cout << "Connected to D-Bus as \"" << ::dbus_bus_get_unique_name(dbus_conn) << "\"." << std::endl;
    std::cout << "Introspection Result:" << std::endl;
    std::cout << std::endl << dbus_result << std::endl << std::endl;

    dbus_cleanup(false);
    return 0;
}
