#!/usr/bin/sh

gcc repl.c lann.c -Iinclude -o lann -Os -s -DLN_HANDLE
