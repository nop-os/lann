#!/usr/bin/sh

# gcc $(find . -name "*.c") -Iinclude -o lann -O0 -g -fno-omit-frame-pointer
gcc $(find . -name "*.c") -Iinclude -o lann -Ofast -s
