# Inventario del dispositivo de prueba

Datos obtenidos por ADB el 2026-11-12 (sin IMEI, número de serie ni cuentas). Todas las cifras son **medidas** salvo que se indique "estimada".

## Identificación y sistema

| Campo | Valor | Fuente |
|---|---|---|
| Modelo | Xiaomi Redmi Note 10S, `M2101K7BL` | `getprop ro.product.model` |
| Nombre de dispositivo | `maltose` (producto `maltose_global`) | `getprop ro.product.device` |
| Android | 13 (API 33) | `ro.build.version.release`, `ro.build.version.sdk` |
| Compilación | `V14.0.11.0.TKLMIXM` (MIUI 14, interfaz `V140`), build `TP1A.220624.014` | `ro.build.version.incremental`, `ro.miui.ui.version.name` |
| Parche de seguridad | 2024-07-01 | `ro.build.version.security_patch` |
| Kernel | 4.14.186 | `uname -r` |
| ABI | `arm64-v8a` | `ro.product.cpu.abi` |

## SoC y CPU

| Campo | Valor | Fuente |
|---|---|---|
| Plataforma | `mt6785` (MediaTek Helio G95, `MT6785V/CD`) | `ro.board.platform`, `ro.soc.model` |
| Núcleos | 8: 6 x Cortex-A55 (part `0xd05`, CPU 0-5, máx. 2,00 GHz) + 2 x Cortex-A76 (part `0xd0b`, CPU 6-7, máx. 2,05 GHz) | `/proc/cpuinfo`, `cpuinfo_max_freq` |
| Extensiones | `fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp` | `/proc/cpuinfo` (Features) |
| NEON (ASIMD) | Sí (`asimd`) | |
| dotprod | Sí (`asimddp`) | |
| FP16 | Sí (`fphp`, `asimdhp`) | |
| i8mm / SVE | No (ausentes en Features) | |

Implicación: los núcleos rápidos son los **CPU 6-7**. Con `-t 2` el planificador no garantiza que los hilos caigan en los A76; por eso se mide 2/4/6 hilos con `llama-bench`.

## Memoria y almacenamiento

| Campo | Valor | Fuente |
|---|---|---|
| RAM física (MemTotal) | 5 725 984 kB (~5,46 GiB) | `/proc/meminfo` |
| MemAvailable en reposo (con PC conectado, apps en segundo plano) | 2 272 220 kB (~2,17 GiB) | `/proc/meminfo` |
| Swap (incluye "RAM virtual" de MIUI + zram) | 4 194 300 kB total; **no cuenta para el presupuesto** | `/proc/meminfo` |
| /data | 108 G total, 43 G libres (61 % usado) | `df -h /data` |
| Almacenamiento | UFS 2.2 (ficha técnica; no verificable por ADB) | estimada |

Presupuesto de memoria adoptado: pico por etapa <= 1,7 GB (máximo 2,0 GB). Ver `docs/decisiones.md`.

## Batería y temperatura (al iniciar)

| Campo | Valor |
|---|---|
| Nivel | 92 %, cargando por AC |
| Temperatura de batería | 34,8 °C (`dumpsys battery`, décimas de grado) |
| Zonas térmicas `/sys/class/thermal` | No legibles desde `adb shell` sin root |

## Estado de depuración

| Ajuste | Estado observado |
|---|---|
| Depuración USB | Activa (dispositivo `device`, autorizado) |
| Depuración USB (ajustes de seguridad) | **Inactiva** al inicio: `input` y `settings put` fallan con `SecurityException` (`INJECT_EVENTS`, `WRITE_SECURE_SETTINGS`); `svc power stayon usb` muere con código 137 |
| Termux / Termux:API | No instalados al inicio |
