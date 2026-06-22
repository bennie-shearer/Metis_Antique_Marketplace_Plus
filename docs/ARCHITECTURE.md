# Metis Antique Marketplace Plus -- Architecture

**Version 1.2.48**

## Overview

Metis Antique Marketplace Plus is a zero-external-dependency C++20 HTTP server with a
vanilla-JS single-page browser client. It follows the standard Metis Suite
architecture: one executable, one SQLite database, one web directory, one
PSON config file.

## Technology stack

| Layer       | Technology                                              |
|-------------|---------------------------------------------------------|
| Language    | C++20                                                   |
| Compiler    | MinGW-w64 GCC 13 (Windows primary), GCC 13 (Linux), clang (macOS) |
| Build       | CMake 3.20+ with Ninja                                  |
| HTTP        | Hand-written TCP socket server (no external HTTP lib)   |
| Database    | SQLite 3.53.1 amalgamation (bundled, zero dependency)   |
| TLS         | OpenSSL 4.0.0, auto-detected at CMake time                |
| Config      | PSON (Metis Property Object Notation)                   |
| Logging     | Custom: INFO / WARN / ERROR / system, ms timestamps, rotation |
| Client      | Plain HTML5 + CSS3 + vanilla ES2020, no build step      |

## Directory layout

```
metis_antique_plus/
  CMakeLists.txt              Root build file (delegates to server/)
  VERSION                     Single source of truth: MAJOR.MINOR.PATCH
  config.pson                 All operational configuration
  README.md
  server/
    CMakeLists.txt            Executable target, source list, asset sync
    src/
      main.cpp                Entry point: config -> logger -> DB -> server
      types.hpp               HttpRequest, HttpResponse, Route, Rows, role helpers
      server.hpp / server.cpp TCP accept loop, request parse, static serve, auth gate
      router.hpp / router.cpp Pattern-matching dispatch with :param extraction
      db.hpp / db.cpp         SQLite WAL wrapper: exec(), query(), last_insert_id()
      pson.hpp / pson.cpp     PSON section/key/value parser
      util.hpp / util.cpp     exe_dir(), resolve_exe_relative(), json_escape(), now_iso()
      logger.hpp / logger.cpp Thread-safe logger with millisecond timestamps and rotation
      app_auth.hpp/cpp        Login, logout, session, user management, role seeding
      app_items.hpp/cpp       Inventory CRUD + stats aggregate (5 endpoints)
      app_listings.hpp/cpp    Marketplace listing management (4 endpoints)
      app_sales.hpp/cpp       Sale recording + revenue summary (3 endpoints)
      app_appraisals.hpp/cpp  Appraisal workflow (3 endpoints)
      app_market.hpp/cpp      Category revenue, trend, top-performers (3 endpoints)
  third_party/
    sqlite/
      sqlite3.h               SQLite 3.53.1 amalgamation header
      sqlite3.c               SQLite 3.53.1 amalgamation source
  openssl-prebuilt/
    windows/                  Copy libssl.a, libcrypto.a, include/ from Metis Logistics Plus
    linux/                    (optional, system OpenSSL preferred on Linux)
    macos/                    (optional, system OpenSSL preferred on macOS)
  web/
    index.html                SPA shell: login screen, sidebar, page divs, modal
    css/app.css               All styles in one file
    js/
      api.js                  Fetch wrapper with Bearer token injection
      pages.js                Page renderers (inventory, listings, sales, appraisals, market, users)
      app.js                  Application controller: nav, modals, forms, role-aware UI
  data/                       Created at runtime -- antique.db lives here
  logs/                       Created at runtime -- antique_marketplace_plus.log lives here
  docs/                       Documentation (this directory)
```

## Request lifecycle

1. `Server::run()` accepts TCP connections on configured port, spawns detached thread per client
2. Raw HTTP bytes read into buffer; `Server::parse_request()` extracts method, path, headers, body
3. `Authorization: Bearer <token>` header extracted; `session_valid()` queries sessions table
4. `user_role()` fetches role from users table; both stored on `HttpRequest`
5. Static file requests (path does not start with `/api/`) served from `web/` directory
6. Auth gate: unauthenticated `/api/` calls (except `/api/auth/login`) return HTTP 401
7. Viewer role gate: POST/PUT/DELETE by viewer accounts (outside `/api/auth/`) return HTTP 403
8. `Router::dispatch()` pattern-matches path, extracts `:param` segments, calls handler
9. Handler returns `HttpResponse`; `Server::build_response()` serializes and sends

## Role model

Three roles enforced at two layers:

| Role    | Inventory / Listings / Sales / Appraisals | Market | User management |
|---------|-------------------------------------------|--------|-----------------|
| admin   | Full read/write                           | Read   | Full            |
| dealer  | Full read/write                           | Read   | None            |
| viewer  | Read only (writes return HTTP 403)        | Read   | None            |

Layer 1 -- server.cpp: viewer POST/PUT/DELETE blocked before reaching any handler.
Layer 2 -- app_auth.cpp: user management endpoints return 403 unless role == "admin".

## Database

Six tables: users, items, listings, sales, appraisals, sessions.
WAL journal mode and foreign keys enabled on every open.
Timestamps are UTC via SQLite datetime('now') defaults.
All paths are exe-relative via util::resolve_exe_relative().

## Client architecture

Three JavaScript files, no framework, no build step:

- `api.js` -- IIFE exposing Api.get/post/put/del, token stored in sessionStorage
- `pages.js` -- IIFE exposing Pages.render* functions, one per nav section
- `app.js` -- IIFE exposing app.* handlers wired to onclick attributes in HTML

Role-aware behaviour: login response includes role; admin-only nav items
shown/hidden immediately; viewer sees read-only banner on write pages.
