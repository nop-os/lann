#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -o lann -Os -s -DLN_HANDLE
