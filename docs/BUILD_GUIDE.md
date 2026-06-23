# Metis Antique Marketplace Plus -- Build Guide

**Version 1.2.50**

C++20 / CMake / Ninja -- Windows, Linux, macOS

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Windows Build (CLion / MinGW-w64)](#2-windows-build-clion--mingw-w64)
3. [Linux Build](#3-linux-build)
4. [macOS Build](#4-macos-build)
5. [CMake Options](#5-cmake-options)
6. [Build Outputs](#6-build-outputs)
7. [TLS Certificate Generation](#7-tls-certificate-generation)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Prerequisites

| Platform | Toolchain                                                         |
|----------|-------------------------------------------------------------------|
| Windows  | MinGW-w64 GCC 13+, CMake 3.20+, Ninja, CLion 2026.1 recommended |
| Linux    | GCC 13+ or Clang 15+, CMake 3.20+, Ninja, libssl-dev            |
| macOS    | Xcode Command Line Tools or LLVM 15+, CMake 3.20+, Ninja, OpenSSL via Homebrew |

OpenSSL 4.0.0 static libraries for Windows are bundled in
`openssl-prebuilt/windows/`. No separate OpenSSL installation is required on
Windows.

No other runtime dependencies. SQLite 3.53.1 and bcrypt are compiled in from
source in `third_party/`.

---

## 2. Windows Build (CLion / MinGW-w64)

### CLion (recommended)

1. Extract the project zip into a working directory.
2. Open the directory in CLion -- `CMakeLists.txt` is detected automatically.
3. CLion will prompt to configure the CMake project. Select **Release**.
4. Select the MinGW-w64 toolchain (GCC 13).
5. **Build > Build Project** (Ctrl+F9).

Build output: `cmake-build-release\server\metis_antique_plus.exe`

CMake post-build steps automatically copy:
- `config.pson` to the build output directory
- `certs/` to the build output directory
- `web/` to `cmake-build-release\server\web\`
- `trust_cert.cmd` to the build output directory

### Command line (MinGW)

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build -j4
```

---

## 3. Linux Build

### Install dependencies (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libssl-dev
```

### Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Build output: `build/server/metis_antique_plus`

### GCC version check

```bash
g++ --version   # must be 13+
```

If your distribution ships an older GCC, install GCC 13 via the toolchain PPA
or build from source, then pass `-DCMAKE_CXX_COMPILER=g++-13`.

---

## 4. macOS Build

### Install dependencies (Homebrew)

```bash
brew install cmake ninja openssl@3
```

### Build

```bash
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

Build output: `build/server/metis_antique_plus`

### Apple Silicon (M-series)

The build is architecture-transparent. CMake selects the native architecture
automatically. No special flags needed.

---

## 5. CMake Options

| Variable             | Default         | Description                                |
|----------------------|-----------------|--------------------------------------------|
| `CMAKE_BUILD_TYPE`   | Release         | Release / Debug / RelWithDebInfo           |
| `OPENSSL_ROOT_DIR`   | auto-detected   | Override OpenSSL installation path         |

TLS is automatically enabled when OpenSSL is found and disabled otherwise.
The CMake output reports: `TLS enabled: OpenSSL <version>` or `TLS disabled`.

---

## 6. Build Outputs

After a successful build, the output directory contains:

```
metis_antique_plus[.exe]   Executable
config.pson                Operational configuration (copied from project root)
web/                       Browser client (copied from project web/)
certs/                     TLS certificate and key (auto-generated if missing)
  server.crt
  server.key
  san.cfg
trust_cert.cmd             Windows: double-click as Administrator to trust cert
```

Runtime directories created on first launch:
```
data/antique.db            SQLite database
logs/                      Log files
photos/                    Uploaded item photos
backups/                   Scheduled database backups
```

---

## 7. TLS Certificate Generation

CMake auto-generates a self-signed certificate with Subject Alternative Name
(SAN) fields during the configure step if `certs/server.crt` does not exist.

The certificate covers:
- `DNS:localhost`
- `IP:127.0.0.1`

Certificate validity: 825 days (Apple/Chrome maximum for self-signed certs).

### Trusting the certificate (Windows)

Run `trust_cert.cmd` as Administrator once:

```
Right-click trust_cert.cmd -> Run as administrator
```

This adds the certificate to the Windows Root CA store. Edge and Chrome will
then show the green padlock on `https://localhost:8481`.

### Trusting the certificate (Linux)

```bash
sudo cp certs/server.crt /usr/local/share/ca-certificates/metis_antique.crt
sudo update-ca-certificates
```

### Trusting the certificate (macOS)

```bash
sudo security add-trusted-cert -d -r trustRoot \
     -k /Library/Keychains/System.keychain certs/server.crt
```

---

## 8. Troubleshooting

### Windows: bcrypt.h conflict

If you see errors about `BCryptBuffer` or `BCRYPT_KEY_HANDLE`:
- Ensure `WIN32_LEAN_AND_MEAN` is set (it is via `server/CMakeLists.txt`).
- Verify the MinGW-w64 toolchain version is GCC 13 or later.

### TLS disabled unexpectedly

Run CMake with verbose output:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release --log-level=VERBOSE
```
Look for `TLS disabled: OpenSSL not found`. On Windows, verify
`openssl-prebuilt/windows/lib/libssl.a` exists.

### Certificate not trusted in browser

The `Not secure` warning appears if the certificate lacks a SAN field or has
not been added to the OS trust store. Delete `certs/server.crt` and
`certs/server.key` and re-run CMake to regenerate with SAN.

### Port already in use

Change `server.port` and/or `server.tls_port` in `config.pson`.

### Permission denied on Linux/macOS port < 1024

Ports below 1024 require root. Use the default ports (8480 / 8481) or set
`CAP_NET_BIND_SERVICE` on the executable:
```bash
sudo setcap 'cap_net_bind_service=+ep' build/server/metis_antique_plus
```
