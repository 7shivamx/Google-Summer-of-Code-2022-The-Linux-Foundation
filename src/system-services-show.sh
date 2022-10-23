#!/bin/bash

set -e

gcc -Wno-format -o _system-services-show-bin `cups-config --cflags` system-services-show.c cupsapi.c -Wno-deprecated-declarations -Wno-format-security -lm `cups-config --libs` `pkg-config --cflags --libs gtk+-3.0 avahi-client avahi-glib avahi-core` -export-dynamic

# gcc -g -Wno-format -o _system-services-show-bin `cups-config --cflags` system-services-show.c cupsapi.c -Wno-deprecated-declarations -Wno-format-security -lm `cups-config --libs` `pkg-config --cflags --libs gtk+-3.0 avahi-client avahi-glib avahi-core` -export-dynamic
# G_DEBUG=fatal-criticals
./_system-services-show-bin
