#!/usr/bin/sh

mkdir -p build
cp lann/repl.ln build/

gcc $(find ./lann -name "*.c") -ldl -Ilann/include -o build/lann_debug -Og -g -fno-omit-frame-pointer -fsanitize=address
gcc $(find ./lann -name "*.c") -ldl -Ilann/include -o build/lann -Ofast -s
