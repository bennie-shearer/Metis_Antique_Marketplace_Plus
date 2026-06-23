# Metis Antique Marketplace Plus -- Operations Guide

**Version 1.2.50**

Installation, startup, configuration, remote access, and email setup.

---

## Requirements

| Platform | Requirement                                                  |
|----------|--------------------------------------------------------------|
| Windows  | MinGW-w64 GCC 13, CMake 3.20+, Ninja, CLion (recommended)   |
| Linux    | GCC 13+, CMake 3.20+, Ninja, libssl-dev                      |
| macOS    | Xcode CLT or LLVM 15+, CMake 3.20+, Ninja, OpenSSL via brew |

No runtime dependencies. SQLite 3.53.1 and OpenSSL 4.0.0 are bundled
(Windows prebuilt). All paths are exe-relative.

---

## Build

### Windows (CLion / MinGW)

1. Extract the project zip into a CLion project folder
2. Open in CLion -- CMakeLists.txt is detected automatically
3. Select **Release** configuration
4. Build > Build Project (Ctrl+F9)
5. Executable: `cmake-build-release\server\metis_antique_plus.exe`

CMake post-build steps automatically copy:
- `config.pson` → build output directory
- `certs/` → build output directory
- `web/` → build output directory (via sync_web_assets target)

### Linux / macOS (command line)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

Executable: `build/server/metis_antique_plus`

With system OpenSSL (Linux):
```bash
sudo apt install libssl-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

---

## Run

### Windows

```
cd cmake-build-release\server
metis_antique_plus.exe
```

Working directory must be the folder containing the executable so that
exe-relative paths resolve correctly.

### Linux / macOS

```bash
cd build/server
./metis_antique_plus
```

---

## First boot sequence

On first startup the server:

1. Reads `config.pson` from the executable directory
2. Validates required config fields (exits with error if missing)
3. Creates `logs/` directory and opens `antique_marketplace_plus.log`
4. Opens `data/antique.db` (creates if not present)
5. Runs `CREATE TABLE IF NOT EXISTS` for all tables
6. Seeds default users from `config.pson` credentials (safe to run repeatedly)
7. Initialises TLS context if `tls_port > 0` and cert/key files exist
8. Begins listening on `server.port` (HTTP) and `server.tls_port` (HTTPS)

Open `https://localhost:8481` and log in as `admin` / `Antique#2026`.

---

## Configuration (config.pson)

All operational values are in `config.pson`. No values are hardcoded in source.

```pson
server {
    host       = "0.0.0.0"       # bind address
    port       = 8480             # HTTP port
    tls_port   = 8481             # HTTPS port (requires cert/key)
    cert_file  = "certs/server.crt"
    key_file   = "certs/server.key"
    web_root   = "web"            # static files directory (exe-relative)
    log_dir    = "logs"           # log directory (exe-relative)
    log_max_mb = 10               # rotate log at this size in MB
}

auth {
    enabled                 = true   # false disables auth (dev only)
    session_hours           = 8      # session lifetime in hours
    bcrypt_cost             = 12     # bcrypt work factor (10-14 recommended)
    rate_limit_max_attempts = 5      # failed logins before lockout
    rate_limit_window_sec   = 60     # lockout window in seconds

    admin_user  = "admin"
    admin_pass  = "Antique#2026"     # change before deployment

    dealer_user = "dealer"
    dealer_pass = "Dealer#2026"

    viewer_user = "viewer"
    viewer_pass = "Viewer#2026"
}

database {
    path = "data/antique.db"    # exe-relative path
}

app {
    name            = "Metis Antique Marketplace Plus"
    version         = "1.2.48"
    currency        = "USD"
    currency_symbol = "$"        # displayed in all monetary fields
    items_per_page  = 20         # default page size for inventory
    photos_dir      = "photos"   # uploaded item photos (exe-relative)
    backup_dir      = "backups"  # nightly backup destination
    backup_keep     = 7          # number of backup files to retain
    backup_hour     = 2          # hour (0-23) to run nightly backup
}

email {
    enabled      = false              # set true to enable email receipts
    smtp_host    = "smtp.gmail.com"
    smtp_port    = 587                # STARTTLS port
    smtp_user    = "you@gmail.com"
    smtp_pass    = ""                 # Gmail: use an App Password
    from_address = "you@gmail.com"
    from_name    = "Metis Antique Marketplace"
}
```

After editing `config.pson`, restart the server for changes to take effect.
The `#` character is safe to use inside quoted string values.

---

## Default credentials

Change all passwords before exposing the server to any network.

| Username | Default password | Role   |
|----------|------------------|--------|
| admin    | Antique#2026     | admin  |
| dealer   | Dealer#2026      | dealer |
| viewer   | Viewer#2026      | viewer |

Passwords can be changed in the browser (Users page or Change password
dialog). To reset from config.pson: edit the password, delete
`data/antique.db`, and restart -- the server re-seeds all users.

---

## TLS / HTTPS

TLS is enabled automatically when:
1. `server.tls_port` is set to a non-zero value in `config.pson`
2. `server.cert_file` and `server.key_file` both exist and load cleanly

The startup log confirms: `TLS: context ready, will bind port 8481`

### Generate a self-signed certificate (local use)

```bash
cd cmake-build-release\server\certs
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt ^
  -days 365 -nodes -subj "/C=US/ST=Idaho/L=Coeur d'Alene/O=Metis/CN=localhost"
```

### Trust the certificate in Windows (remove "Not secure" warning)

1. Open `certmgr.msc`
2. Go to **Trusted Root Certification Authorities → Certificates**
3. Right-click → **All Tasks → Import**
4. Browse to `certs\server.crt`
5. Place in **Trusted Root Certification Authorities**
6. Restart Chrome

### Production certificate (Let's Encrypt)

Use win-acme on Windows or certbot on Linux to obtain a free certificate
for your domain. Point `cert_file` and `key_file` in `config.pson` to
the issued files. TLS 1.2 is the minimum protocol version enforced.

---

## Email receipts

Email receipts are sent via SMTP STARTTLS using the OpenSSL already
linked into the binary. No additional libraries required.

### Gmail setup

1. Enable 2-Step Verification on the Gmail account
2. Go to `myaccount.google.com/apppasswords`
3. Create an App Password for "Mail"
4. Set `smtp_pass` to the 16-character app password in `config.pson`
5. Set `enabled = true`
6. Restart the server

### Office 365 setup

```pson
smtp_host = "smtp.office365.com"
smtp_port = 587
smtp_user = "you@yourdomain.com"
smtp_pass = "your-password"
```

### Testing

After configuration, record a sale with a buyer email and click the ✉ button.
The server log records `Receipt emailed to buyer@example.com for sale N` on
success, or an error detail on failure.

---

## Logging

Log file: `logs/antique_marketplace_plus.log` (exe-relative).
Format: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message`
Levels: `INFO`, `WARN`, `ERROR`, `system`

Log rotation: when the log exceeds `log_max_mb`, it is renamed to
`antique_marketplace_plus.log.1` and a new file is opened. One backup kept.

Real-time log viewing is available in the browser at **Audit & Logs → Live log**
(admin only, WebSocket stream on `/ws/logs`).

---

## Remote access

### Step 1 -- bind to all interfaces

Confirm `host = "0.0.0.0"` in `config.pson` (the default).

### Step 2 -- Windows Firewall

```
netsh advfirewall firewall add rule name="Metis Antique HTTP" ^
  dir=in action=allow protocol=TCP localport=8480
netsh advfirewall firewall add rule name="Metis Antique HTTPS" ^
  dir=in action=allow protocol=TCP localport=8481
```

### Step 3 -- router port forwarding

Add port forwarding rules for TCP 8480 and 8481 pointing to the machine's
local IP (find with `ipconfig` on Windows).

### Step 4 -- dynamic DNS (optional)

Use DuckDNS (free) to assign a stable hostname to your changing public IP.
Create a subdomain at `duckdns.org` and install the update client.

---

## Running as a service

### Windows (NSSM)

```
nssm install MetisAntiqueMarketplace "C:\path\to\metis_antique_plus.exe"
nssm set MetisAntiqueMarketplace AppDirectory "C:\path\to\"
nssm set MetisAntiqueMarketplace DisplayName "Metis Antique Marketplace Plus"
nssm start MetisAntiqueMarketplace
```

### Linux (systemd)

Create `/etc/systemd/system/metis-antique-marketplace.service`:
```ini
[Unit]
Description=Metis Antique Marketplace Plus
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/metis-antique-marketplace
ExecStart=/opt/metis-antique-marketplace/metis_antique_plus
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
systemctl daemon-reload
systemctl enable metis-antique-marketplace
systemctl start metis-antique-marketplace
systemctl status metis-antique-marketplace
```

---

## Stopping the server

Send SIGINT (Ctrl+C in terminal) or SIGTERM. The server sets `running_ = false`,
the accept loop exits cleanly, and the log records "Server stopped".
