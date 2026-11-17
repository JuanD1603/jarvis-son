#!/usr/bin/env bash
# Restablece el túnel USB (adb forward) hacia sshd de Termux y relanza sshd si no responde.
set -u
cd "$(dirname "$0")/.." || exit 1
source scripts/env_pc.sh
adb wait-for-device
adb forward tcp:8022 tcp:8022 >/dev/null
if ssh -o ConnectTimeout=5 -o BatchMode=yes jarvis-phone true 2>/dev/null; then
  echo "conexion OK"; exit 0
fi
echo "sshd no responde: relanzando Termux y sshd"
adb shell monkey -p com.termux -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
sleep 3
# .bashrc arranca sshd al abrir sesión; como respaldo se escribe el comando en la ventana.
adb shell input text 'sshd' && adb shell input keyevent 66
sleep 2
ssh -o ConnectTimeout=5 -o BatchMode=yes jarvis-phone true && echo "conexion OK" || { echo "ERROR: no se pudo conectar"; exit 1; }
