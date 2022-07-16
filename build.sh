#!/usr/bin/sh

# gcc $(find . -name "*.c") -Iinclude -o lann -Og -g -fno-omit-frame-pointer
gcc $(find . -name "*.c") -Iinclude -o lann -Ofast -s
