#!/usr/bin/sh

if [ $(id -u) -ne 0 ]; then
  echo "[lann] in order to install lann, root permissions are needed"
  exit 1
fi

echo "[lann] creating lann directories /usr/share/lann and /usr/bin..."
mkdir -p /usr/share/lann /usr/bin

echo "[lann] copying executable to /usr/bin..."
cp lann /usr/bin/

echo "[lann] copying repl.ln to /usr/share/lann..."
cp repl.ln /usr/share/lann/

echo "[lann] done!"
