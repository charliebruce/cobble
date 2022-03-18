mkdir -p build

# Test executable (default architecture)
g++ \
cobble_events.c \
cobble_scan_example.c \
platforms/linux/linux_bluez.cpp \
-std=c++0x $(pkg-config dbus-1 --cflags) -ldbus-1 -lpthread -Werror -Wall -Wextra \
-o build/cobble_linux

# Library (default architecture)
g++ -shared \
cobble_events.c \
platforms/linux/linux_bluez.cpp \
-std=c++0x $(pkg-config dbus-1 --cflags) -ldbus-1 -lpthread -Werror -Wall -Wextra \
-o build/libcobble.so
