#!/usr/bin/env bash
# Entorno del PC (Git Bash): añade platform-tools al PATH de la sesión.
PT="${PT:-$LOCALAPPDATA/Microsoft/WinGet/Packages/Google.PlatformTools_Microsoft.Winget.Source_8wekyb3d8bbwe/platform-tools}"
export PATH="$(cygpath -u "$PT" 2>/dev/null || echo "$PT"):$PATH"
export MSYS_NO_PATHCONV=1  # evita que Git Bash convierta rutas /data, /sdcard en rutas de Windows
