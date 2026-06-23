# Metis Antique Marketplace Plus

**Version 1.2.50**

Professional antique inventory, marketplace, and operations platform.
Zero-dependency C++20 HTTP server with a vanilla-JS browser client.
Runs on Windows, Linux, and macOS. Stores all data locally in SQLite.

---

## Overview

Metis Antique Marketplace Plus is a self-hosted management system for antique
dealers, collectors, consignment shops, and antique mall operators. It provides
inventory tracking, multi-channel listing records, sales and appraisal workflow,
POS hardware integration, booth rent management, and full business analytics --
all from a single executable with no cloud dependencies.

---

## Features

- **Inventory** -- Rich item records: era, maker, origin, material, condition,
  provenance, cost basis, asking price, SKU (auto-generated), barcode labels
- **Multi-photo gallery** -- Upload and manage multiple photos per item
- **Listings** -- Record items listed on 1stDibs, eBay, Etsy, Chairish, Ruby Lane
- **Sales** -- Buyer details, platform fees, shipping, net proceeds, email receipt
- **Appraisals** -- Appraiser tracking, valuation ranges, status workflow
- **Consignment** -- Consignor splits, printable consignor statements
- **Purchases and Expenses** -- Acquisition cost ledger and operating expense tracking
- **P&L Report** -- Date-range profit and loss with per-category breakdown
- **Portfolio Analytics** -- Inventory value by category, ROI by acquisition source
- **POS** -- Barcode scanner (HID wedge), ESC/POS receipt printer, cash drawer
- **Booth Rent** -- Monthly invoicing, three-tier discount engine, late fees, email reminders
- **Dealer Settlements** -- Per-dealer payout calculation for mall operators
- **Market Analytics** -- Category revenue, trend charts, top performers
- **Dashboard** -- KPI summary: inventory count, revenue, recent sales, acquisitions
- **Dealer Dashboard** -- Personal real-time view for each dealer
- **Audit Trail** -- Full change log with user, role, action, and IP for every write
- **WebSocket Logs** -- Real-time log streaming on `/ws/logs`
- **QR + Code 128 Labels** -- Pure-JS SVG barcode generator, browser-print
- **Multi-currency** -- `Intl.NumberFormat` via `app.currency*` in config.pson
- **i18n** -- Six locales included
- **TLS 1.3** -- OpenSSL 4.0.0 with post-quantum hybrid (X25519MLKEM768)
- **Three-role auth** -- admin / dealer / viewer with session timeout
- **Platform sync** -- eBay, Etsy, Chairish, 1stDibs (config-ready, credentials in PSON)
- **GPU / Kubernetes / Containers** -- Native C++20 implementation planned; stubs in config.pson

---

## Quick Start

### Windows (CLion / MinGW-w64)

```
1. Open project folder in CLion
2. Select Release configuration
3. Build > Build Project (Ctrl+F9)
4. Run cmake-build-release\server\metis_antique_plus.exe
5. Open https://localhost:8481
6. Log in: admin / Antique#2026
```

### Linux / macOS

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/server/metis_antique_plus
```

Open `https://localhost:8481` (TLS) or `http://localhost:8480`.

Default credentials: `admin` / `Antique#2026`
**Change all passwords immediately after first login.**

---

## REST API

121 endpoints across 16 modules. See [docs/API.md](docs/API.md) and
[docs/ENDPOINTS.md](docs/ENDPOINTS.md) for full documentation.

Key endpoint groups: auth, items, listings, sales, appraisals, market,
photos, comments, purchases, expenses, business, audit, POS, sync, settlements,
and WebSocket log streaming.

---

## Configuration

All operational parameters are in `config.pson`. Nothing is hardcoded in source.

```pson
server { port = 8480  tls_port = 8481  web_root = "web" }
auth   { admin_user = "admin"  admin_pass = "Antique#2026" }
app    { name = "Metis Antique Marketplace Plus"  currency = "USD" }
email  { enabled = false  smtp_host = "smtp.gmail.com" }
pos    { enabled = false  receipt_printer_ip = "192.168.1.100" }
sync   { ebay { enabled = false } etsy { enabled = false } }
compute { gpu_enabled = false  kubernetes_enabled = false  containers_enabled = false }
```

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for all parameters.

---

## Directory Structure

```
Metis.Antique.Plus.v1.2.48/
  CMakeLists.txt             Root build file
  VERSION                    Single source of truth: 1.2.48
  config.pson                All operational configuration
  README.md
  server/
    CMakeLists.txt           Executable target, sources, asset sync, cert generation
    src/                     C++20 source (main, server, router, db, logger, pson,
                             util, smtp, app_auth, app_items, app_listings, app_sales,
                             app_appraisals, app_market, app_photos, app_comments,
                             app_audit, app_purchases, app_business, app_pos,
                             app_sync, ws_log, types)
  web/
    index.html               SPA shell (login, sidebar, page containers, modal)
    shop.html                Public read-only buyer storefront
    css/app.css              All styles
    js/
      api.js                 Fetch wrapper with Bearer token
      app.js                 Application controller
      pages.js               Page renderers (inventory, listings, sales, appraisals, market, users)
      barcode.js             Code 128 SVG barcode renderer
      i18n.js                Locale strings
      pages/                 Modular page renderers (audit, business, comments, dashboard,
                             dealer, gallery, pos, purchases, qr)
  third_party/
    sqlite/                  SQLite 3.53.1 amalgamation
    json/                    json.hpp
    bcrypt/                  bcrypt + crypt_blowfish
  openssl-prebuilt/
    windows/                 OpenSSL 4.0.0 static libs for MinGW-w64
  certs/                     TLS certificate (auto-generated by CMake with SAN)
  docs/                      All documentation
  data/                      Created at runtime: antique.db
  logs/                      Created at runtime: log files
  photos/                    Created at runtime: uploaded item photos
  backups/                   Created at runtime: nightly SQLite backups
```

---

## Technology Stack

| Layer    | Technology                                                          |
|----------|---------------------------------------------------------------------|
| Language | C++20                                                               |
| Compiler | MinGW-w64 GCC 13 (Windows), GCC 13+ (Linux), Clang 15+ (macOS)    |
| Build    | CMake 3.20+ with Ninja                                              |
| Server   | Hand-written TCP socket server (zero external HTTP library)         |
| Database | SQLite 3.53.1 amalgamation (bundled)                                |
| TLS      | OpenSSL 4.0.0 (bundled Windows prebuilt; system on Linux/macOS)    |
| Config   | PSON (Metis Property Object Notation)                               |
| Client   | HTML5 + CSS3 + vanilla ES2020, no framework, no build step         |

---

## Documentation

| Document                                       | Contents                                          |
|------------------------------------------------|---------------------------------------------------|
| [docs/API.md](docs/API.md)                     | Full REST API reference                           |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)   | System architecture and design                    |
| [docs/BACKGROUND.md](docs/BACKGROUND.md)       | History, ecosystem context, design philosophy     |
| [docs/BILLING.md](docs/BILLING.md)             | Fee calculations, P&L, consignment, booth rent    |
| [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)     | Platform-specific build instructions              |
| [docs/CHANGELOG.md](docs/CHANGELOG.md)         | Version history                                   |
| [docs/COMMERCIAL-TODO.md](docs/COMMERCIAL-TODO.md) | Future commercial feature roadmap             |
| [docs/COMPARISON.md](docs/COMPARISON.md)       | vs. SaaS, spreadsheets, and alternatives          |
| [docs/CONFIGURATION.md](docs/CONFIGURATION.md) | All config.pson parameters                        |
| [docs/DATA_MODEL.md](docs/DATA_MODEL.md)       | All 18 database tables with columns               |
| [docs/HOWTO.md](docs/HOWTO.md)                 | Step-by-step operational guide                    |
| [docs/IMPLEMENTATION-PLAN.md](docs/IMPLEMENTATION-PLAN.md) | Phased roadmap (Phases 1-8)        |
| [docs/LICENSE.txt](docs/LICENSE.txt)           | MIT License                                       |
| [docs/OPERATIONS.md](docs/OPERATIONS.md)       | Installation, startup, remote access              |
| [docs/SUPPORT.md](docs/SUPPORT.md)             | Professional organizations and resources          |
| [docs/TODO.md](docs/TODO.md)                   | Completed and deferred feature list               |

---

## License

MIT License -- see [docs/LICENSE.txt](docs/LICENSE.txt)

Copyright (c) 2026 Bennie Shearer (Retired)

---

## Author

Bennie Shearer (Retired)

## Acknowledgments

| Organization                  | Website                              |
|-------------------------------|--------------------------------------|
| CLion by JetBrains s.r.o.     | https://www.jetbrains.com/clion/     |
| WebStorm by JetBrains s.r.o.  | https://www.jetbrains.com/webstorm/  |
| Claude by Anthropic PBC       | https://www.anthropic.com/           |
| OpenSSL Project               | https://www.openssl.org/             |
| SQLite                        | https://sqlite.org/                  |
| Niels Lohmann -- JSON for Modern C++ | https://github.com/nlohmann/json |

---

*Part of the Metis Suite -- zero-dependency C++20 server applications*
