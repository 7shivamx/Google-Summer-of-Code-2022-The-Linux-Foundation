#!/bin/bash

set -e

gcc -o cupsapi_old-bin `cups-config --cflags` cupsapi_old.c dnssdbrowse.c `cups-config --libs` -lavahi-client -lavahi-common
./cupsapi_old-bin
