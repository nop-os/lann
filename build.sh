#!/usr/bin/sh

gcc $(find . -name "*.c") -Iinclude -o lann -Og -g -DLN_HANDLE -DLN_EVAL_DEBUG
