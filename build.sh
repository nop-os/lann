#!/usr/bin/sh

mkdir -p build
cp lann/repl.ln build/

gcc $(find ./lann -name "*.c") -ldl -Ilann/include -o build/lann_debug -rdynamic -Og -g -fno-omit-frame-pointer -fsanitize=address
gcc $(find ./lann -name "*.c") -ldl -Ilann/include -o build/lann -rdynamic -Ofast -s
