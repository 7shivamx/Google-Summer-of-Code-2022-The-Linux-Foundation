#!/bin/bash

set -e

gcc -Wno-format -o p1-bin p1.c -Wno-deprecated-declarations -Wno-format-security -lm `pkg-config --cflags --libs gtk+-3.0` -export-dynamic
./p1-bin