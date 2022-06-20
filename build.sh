#!/usr/bin/sh

gcc -c lann.c -Iinclude -o lann.o -Os
gcc repl.c lann.o -Iinclude -o lann -Os -s -lm
