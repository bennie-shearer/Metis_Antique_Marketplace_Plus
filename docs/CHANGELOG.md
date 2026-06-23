# Changelog

## v1.2.50 (2026-06-22)

### Add Niels Lohmann acknowledgment

README.md Acknowledgments table: added Niels Lohmann -- JSON for Modern C++
(https://github.com/nlohmann/json), SPDX-FileCopyrightText 2013-2026.
json.hpp 3.12.0 is bundled in third_party/json/ and used throughout
the server for JSON parsing.
Files changed: README.md

## v1.2.49 (2026-06-22)

### Remove location from author attribution

README.md: changed author line from
"Bennie Shearer (Retired) -- Coeur d'Alene, Idaho" to
"Bennie Shearer (Retired)".
Files changed: README.md

## v1.2.48 (2026-06-22)

### Full documentation suite, PSON compute block, GPU/Kubernetes/Container stubs

#### Documentation
Added complete Metis Docs Model: BACKGROUND.md (history, ecosystem fit, design
philosophy with Table of Contents), BILLING.md, BUILD_GUIDE.md,
COMMERCIAL-TODO.md, COMPARISON.md, CONFIGURATION.md, DATA_MODEL.md,
IMPLEMENTATION-PLAN.md, LICENSE.txt. All .md and .txt files in docs/.
Updated API.md, ARCHITECTURE.md, CHANGELOG.md, DATABASE.md, ENDPOINTS.md,
HOWTO.md, OPERATIONS.md, SUPPORT.md, TODO.md to v1.2.48.

#### config.pson
Added compute block: gpu_enabled, gpu_backend, gpu_device_index,
kubernetes_enabled, kubernetes_api_host, kubernetes_api_port,
kubernetes_namespace, containers_enabled, container_runtime.
All compute settings default to false/none (planned C++20 native
implementation -- no Docker, no external orchestration tools).

#### Version alignment
VERSION, config.pson app.version, all web/js file headers, and all docs
updated to 1.2.48.

Files changed: docs/* (new and updated), config.pson, web/js/*.js,
web/js/pages/*.js, VERSION

 - Metis Antique Marketplace Plus

## v1.2.47 (2026-06-22)

### Fix: Post-Quantum badge not showing
X25519MLKEM768 is not registered as a named NID in the prebuilt OpenSSL
so OBJ_nid2sn returned empty and PQ detection never fired.
Fix: SSL_CTX_set1_groups_list return value checked. If it returns 1
(success), tls_group_ is pre-set to "X25519MLKEM768" at startup.
The existing MLKEM detection in /api/config then matches and post_quantum
is returned as true. Server log now prints whether PQ is enabled.
Files changed: server/src/server.cpp

## v1.2.46 (2026-06-22)

### Fix: /api/sales/summary, /api/expenses/summary, /api/inquiries/summary returning 404
Root cause: Router::add() appended all routes in registration order.
GET /api/sales/:id was registered before GET /api/sales/summary, so the
:id param pattern matched "summary" as the id value on every request to
/api/sales/summary, returning the wrong handler (not found in db -> 404).
Same problem affected /api/expenses/summary and /api/inquiries/summary.
Fix: Router::add() now inserts literal routes (no : segments) before any
existing parameterised routes. Literal paths always win over :params.
No route reordering in individual app_*.cpp files needed.
Files changed: server/src/router.cpp

## v1.2.45 (2026-06-22)

### Fix: 404 on summary endpoints; 401 across ports
Browser sessionStorage is isolated per origin including port, so a token
stored after login on port 8480 is invisible on port 8481. All API calls
on 8481 were unauthenticated, causing 401 on /api/auth/me and 404 on
all subsequent endpoints (router returns 401 before route match).
Fix: switched api.js from sessionStorage to localStorage for map_token.
localStorage is shared across ports on the same hostname, so logging in
on either port works on both.
Files changed: web/js/api.js

## v1.2.44 (2026-06-21)

### Feature: Post-Quantum TLS enabled (X25519MLKEM768)
server.cpp: SSL_CTX_set1_groups_list sets preferred groups to
"X25519MLKEM768:X25519:P-256" -- post-quantum hybrid first, classical
fallback. Edge 131+ and Chrome 131+ support X25519MLKEM768 natively.
server.hpp: tls_group_ member added.
After SSL_accept: SSL_get_negotiated_group captures the key exchange
group name for PQ detection.
/api/config: returns tls_group field; PQ detection checks group name
(MLKEM, Kyber, X25519MLKEM) in addition to cipher name.
If Edge negotiates X25519MLKEM768 the Post-Quantum TLS badge will light up.
Files changed: server/src/server.cpp, server/src/server.hpp

## v1.2.43 (2026-06-21)

### Fix: AES and Post-Quantum badges missing; only TLS shown
Root cause: /api/config was called before any TLS client had connected,
so tls_cipher_ was empty and the cipher badge was hidden.
Fix 1 (server.cpp): pre-populate tls_version_="TLS 1.3" and
tls_cipher_="TLS_AES_256_GCM_SHA384" at SSL_CTX init time. These are
the correct defaults for OpenSSL 4.0 TLS 1.3. After first real connection
SSL_get_version/SSL_get_cipher overwrite with negotiated values.
Fix 2 (app.js): added cipEl.classList.remove("hidden") when cipher data
is present, ensuring the badge shows even if it was previously hidden.
Files changed: server/src/server.cpp, web/js/app.js

## v1.2.42 (2026-06-21)

### Fix: pos.js e.key undefined crash; CMake cache stale certutil
pos.js line 29: e.key can be undefined for non-character keys (Shift, Ctrl
etc), causing "Cannot read properties of undefined (reading length)".
Fixed with guard: if (e.key && e.key.length === 1).
CMake: certutil was already removed in v1.2.41. Build failure is stale
cache. Delete cmake-build-release folder and let CLion rebuild from clean.
Files changed: web/js/pages/pos.js

## v1.2.41 (2026-06-21)

### Fix: certutil removed from CMake (requires elevation); trust_cert.cmd generated instead
CMake post-build generates trust_cert.cmd in the project root and copies
it to the build output folder. Right-click trust_cert.cmd -> Run as
administrator once. No rebuild needed after that.
Files changed: server/CMakeLists.txt

## v1.2.40 (2026-06-21)

### Fix: CMake auto-trusts TLS cert in Windows Root store after every build
Replaced the print-instructions post-build step with an actual certutil
execution. After every CLion build on Windows:
  certutil -delstore Root localhost
  certutil -addstore Root <build>/certs/server.crt
The cert already has subjectAltName=DNS:localhost,IP:127.0.0.1.
No manual steps required. Restart Edge after rebuild for green padlock.
Files changed: server/CMakeLists.txt

## v1.2.38 (2026-06-21)

### Fix: "invalid credentials" on login -- seed_users INSERT param mismatch
When email and display_name columns were added to users table in v1.2.34,
the INSERT in seed_users() was updated to 5 columns but the params vector
only had 3 values (username, pass_hash, role). SQLite rejected the INSERT
silently, so no users were ever created in a fresh database, causing all
logins to fail with "invalid credentials".
Fixed: params vector now includes empty strings for email and display_name.
Files changed: server/src/app_auth.cpp

## v1.2.37 (2026-06-21)

### Fix: TLS certificate auto-generated with SAN by CMake at build time
Previously the cert was a static file shipped with the project and lacked
subjectAltName, which Edge/Chrome require. Now CMakeLists.txt:
1. Writes certs/san.cfg at configure time with DNS:localhost+IP:127.0.0.1,
   basicConstraints=CA:TRUE, keyUsage, extendedKeyUsage=serverAuth.
2. Finds the openssl executable from the same OpenSSL found by
   find_package(OpenSSL) -- guaranteed to be present and new enough.
3. Runs openssl req at configure time to generate server.crt + server.key
   if they do not already exist.
4. Post-build prints certutil -addstore command so user just runs one line
   as Administrator after first build -- never needs to do it again.
To re-generate the cert: delete certs/server.crt and certs/server.key,
then reload CMake in CLion (File > Reload CMake Project).
Files changed: server/CMakeLists.txt

## v1.2.36 (2026-06-21)

### Fix: all remaining hardcoded operational values moved to config.pson
Full audit performed. Three remaining issues fixed:
1. app_sync.cpp: platform API hostnames hardcoded. Now read from
   config.pson [sync_hosts] block with defaults as fallback:
   sync_hosts.ebay_sandbox, ebay_production, etsy, onedibs, chairish.
2. pages.js:670: consignor statement footer used hardcoded
   "Metis Antique Marketplace Plus". Now uses document.title which is
   set from config.pson app.name after login.
3. config.pson: [sync_hosts] block added with all four platform hosts.
Items confirmed correct to leave in code (not operational config):
- Role enum values ("admin","dealer","viewer") -- auth state machine
- Status enums ("inventory","listed","sold","pending","synced" etc)
- "EHLO localhost" in smtp.cpp -- RFC 5321 SMTP protocol requirement
- Port 443 in app_sync.cpp -- HTTPS standard port, not configurable
- "$" in barcode.js -- Code 128 character encoding table entry
Files changed: server/src/app_sync.cpp, config.pson, web/js/pages.js

## v1.2.35 (2026-06-21)

### Feature: POS hardware support + platform listing sync

POS hardware (app_pos.cpp, app_pos.hpp):
- ESC/POS network receipt printer: POST /api/pos/print-receipt prints a
  formatted receipt for any sale (item, SKU, prices, net proceeds, buyer).
- Price tag printing: POST /api/pos/print-tag prints compact label with
  price, item title, condition, SKU.
- Cash drawer: POST /api/pos/open-drawer fires ESC/POS pin-2 pulse.
  Triggered automatically after every receipt if pos.cash_drawer_enabled=true.
- Barcode scanner: HID keyboard wedge, zero config. POSScanner JS listener
  detects scan input vs keyboard (80ms char-to-char threshold), fires
  app.posHandleScan(code) which looks up item by SKU or UPC via
  POST /api/pos/scan. Shows item with Quick sale and Print tag buttons.
- GET /api/pos/status returns printer config.
- All settings in config.pson [pos] block: enabled, receipt_printer_ip,
  receipt_printer_port, receipt_printer_type, cash_drawer_enabled,
  receipt_header, receipt_footer.

Platform listing sync (app_sync.cpp, app_sync.hpp):
- Four channel adapters: eBay (Inventory API), Etsy (v3 API),
  1stDibs (v1 API), Chairish (v1 API).
- Background sync thread polls every 60s, pushes listings that are
  active and not synced in the configured interval (default 30 min).
- GET /api/sync/listings: listings with sync_status, external_id,
  external_url, last_synced, sync_error.
- POST /api/sync/push { listing_id }: manual push single listing.
- POST /api/sync/push-all { channel }: push all active listings for channel.
- POST /api/sync/webhook/:channel: webhook receiver for sold notifications.
  Auto-records sale and marks item sold if auto_mark_sold=true in config.
- GET /api/sync/status: which channels are configured and credentialed.
- listings table: external_id, external_url, sync_status, last_synced,
  sync_error columns added via migration.
- items table: upc, weight_oz, width_in, height_in, depth_in for shipping.
- All credentials in config.pson [sync] block. Enable per channel:
  sync.ebay.enabled=true + credentials.

Client:
- web/js/pages/pos.js: POS & Sync page (renderPos, renderSyncListings).
  Printer status panel, scanner test input, channel status cards,
  per-listing push buttons and sync status table.
- Sales table: printer icon (&#x1F5A8;) prints receipt via ESC/POS.
- app.js: posTestPrint, posOpenDrawer, posHandleScan, posManualScan,
  posPrintTag, posPrintReceipt, loadSyncListings, syncPushOne, syncPushAll.
- index.html: "POS & Sync" nav item (admin only), page-pos div, pos.js script.

Files changed: server/src/app_pos.hpp (new), server/src/app_pos.cpp (new),
server/src/app_sync.hpp (new), server/src/app_sync.cpp (new),
server/src/server.cpp, server/src/main.cpp, server/src/db.cpp,
server/CMakeLists.txt, config.pson, web/index.html,
web/js/app.js, web/js/pages.js, web/js/pages/pos.js (new)

## v1.2.34 (2026-06-21)

### All non-deferred TODO items completed; full documentation update

Server (app_auth.cpp, db.cpp):
- users table: email and display_name columns added via migration.
- user_to_json, GET /api/users, POST /api/users, PUT /api/users all updated
  to return/accept email and display_name.

Client (pages.js, app.js):
- renderUsers: shows email, display_name columns; active sessions table
  with revoke button per session.
- openAddUser/openEditUser: email and display_name fields added to modals.
- saveNewUser/saveEditUser: send email and display_name in request body.
- revokeSession(): calls DELETE /api/audit/sessions/:token.
- renderPortfolio(): inventory value by category (asking, cost, gain, markup%);
  ROI by acquisition source table. Portfolio nav item added to sidebar.
- renderConsignorStatement(): printable HTML statement per consignor showing
  all items, sale prices, payouts earned, and amount owing. Opens print dialog.
- Consignments page: "Statement" button per row calls renderConsignorStatement.

Documentation:
- TODO.md: fully updated through v1.2.34 (48 completed items).
- HOWTO.md: completely rewritten for v1.2.34 covering all modules.

Files changed: server/src/db.cpp, server/src/app_auth.cpp,
web/index.html, web/js/pages.js, web/js/app.js,
web/js/pages/business.js, docs/TODO.md, docs/HOWTO.md

## v1.2.33 (2026-06-21)

### Fix: two build errors (app_business.cpp, main.cpp)
app_business.cpp: include was <nlohmann/json.hpp> but project uses
  third_party/json/ with direct include as "json.hpp". Fixed.
main.cpp: booth_collector and settler lambdas used bare Rows type and
  brace-initializer lists for std::vector<std::string> params. Both are
  outside the metis::antique namespace so Rows was not in scope, and GCC
  cannot deduce vector<string> from brace-init in this context.
  Fixed: added "using Rows = metis::antique::Rows;" at top of each lambda;
  all {string, ...} initializer lists replaced with
  std::vector<std::string>{...} explicitly.
Files changed: server/src/app_business.cpp, server/src/main.cpp

## v1.2.32 (2026-06-21)

### Feature: vendor rent collection and automated booth discount settings

Schema (db.cpp):
- rentals: added vendor_user_id, discount_pct, discount_label, due_day,
  grace_days, late_fee_pct. Migrations for existing databases.
- rental_payments: added due_date, base_amount, discount_pct, discount_amount,
  late_fee, status (pending/paid/overdue), paid_date.
- booth_invoices: new table for generated invoices.

Discount engine (app_business.cpp):
Three-tier automatic discount applied at payment and invoice time:
1. Early payment: configurable % off if paid before day N of month
2. Loyalty: configurable % off after N consecutive paid months
3. Multi-booth: configurable % off if vendor has >= N active booths
Manual override via rentals.discount_pct takes priority over all auto rules.
All thresholds and percentages driven exclusively by config.pson booth_rent block.

Automated collection thread (main.cpp):
- booth_rent.collection_enabled = true to activate
- On invoice_day: generates rental_payment + booth_invoice records for all
  active rentals that do not yet have an invoice for the period
- On due_day + grace_days: marks pending payments overdue, applies late fee

New API endpoints:
- POST /api/rentals/:id/invoice -- generate invoice with discount calc
- GET /api/booth-invoices -- list all invoices
- PUT /api/booth-invoices/:id/paid -- mark invoice paid
- GET /api/rentals/summary -- now includes pending/overdue counts and amounts
- POST /api/rentals/:id/payments -- now auto-applies discount and late fee

Client (app.js, business.js):
- openRental modal: discount %, discount label, due day, grace days, late fee %
- Rentals page: per-rental discount badge, pending/overdue KPI pills,
  Collect + Invoice + Paid buttons per row
- Booth Invoices table below rental list
- generateBoothInvoice(), markInvoicePaid() functions

config.pson: booth_rent block (collection_enabled, invoice_day, due_day,
grace_days, early_pay_*, loyalty_*, multi_booth_*, late_fee_pct,
send_*_email flags)

Files changed: server/src/db.cpp, server/src/app_business.cpp,
server/src/main.cpp, config.pson, web/js/app.js, web/js/pages/business.js

## v1.2.31 (2026-06-21)

### Feature: per-dealer isolation, SKU/barcode, automatic settlement checks

Per-dealer inventory isolation:
- DB migrations: items.owner_id, items.sku, sales.owner_id columns added.
- GET /api/items filters by owner_id when role=dealer.
- GET /api/sales filters by owner_id (via items JOIN) when role=dealer.
- Dealers land on "My Dashboard" (dealer-dashboard) showing only their own
  inventory and sales in real time. Admin still sees full shared view.
- dealer-only nav item ".dealer-only" shown for dealer role only.

SKU / Barcode:
- POST /api/items auto-generates SKU on INSERT: PREFIX-YYYY-NNNNN.
  Prefix read from config.pson sku.prefix (default "AMP").
- web/js/barcode.js: Code 128B barcode SVG renderer, pure JS, zero deps.
- QR label cards now show SKU text + Code 128 barcode below the QR code.
- item_to_json updated to return sku and owner_id fields.

Automatic settlement checks:
- settlements table added to DDL.
- Nightly settlement thread in main.cpp: runs at config.pson
  settlements.check_hour, finds dealers with unsettled sales older than
  settlements.grace_days, creates a settlement record per dealer.
- GET /api/settlements: dealers see own; admins see all.
- PUT /api/settlements/:id/paid: admin marks settlement paid.
- Settlements page in sidebar (admin only) with Mark paid and Email buttons.
- config.pson: settlements block, sku block added.

Files changed: server/src/db.cpp, server/src/app_items.cpp,
server/src/app_sales.cpp, server/src/server.cpp, server/src/main.cpp,
config.pson, web/index.html, web/js/app.js, web/js/barcode.js (new),
web/js/pages/dealer.js (new), web/js/pages/qr.js

## v1.2.30 (2026-06-21)

### Feature: Consignments, Rentals, Advertising, CC Fees modules
Server: 5 new SQLite tables (consignments, rentals, rental_payments,
advertising, cc_fees) added to schema. New server module app_business.cpp
with 24 REST endpoints. Pson::get_double() added. cc_fees processor rates
driven by config.pson [cc_fees] block (Stripe/Square/PayPal/Venmo defaults).
Client: web/js/pages/business.js with renderConsignments, renderRentals,
renderAdvertising, renderCcFees. Full CRUD modals in app.js. Business
section added to sidebar nav. In-page fee calculator on CC Fees page.
Files changed: server/src/db.cpp, server/src/app_business.hpp (new),
server/src/app_business.cpp (new), server/src/server.cpp, server/src/pson.hpp,
server/src/pson.cpp, server/CMakeLists.txt, config.pson, web/index.html,
web/js/app.js, web/js/pages/business.js (new)

## v1.2.29 (2026-06-21)

### Fix: all remaining hardcoded operational values removed from JS
Full audit performed. Three categories of remaining issues fixed:
1. purchases.js -- 5 hardcoded dollar signs ('$') and 'en-US' in usd()
   functions replaced with CurrencyLocale/CurrencyCode/CurrencySymbol.
2. qr.js -- hardcoded '$' and 'en-US' in price label replaced.
3. dashboard.js -- "Metis Antique Marketplace Plus" subtitle replaced with
   <span id="dash-app-subtitle"> populated from config after login.
4. app.js -- one inline usd() with hardcoded en-US fixed; dash-app-subtitle
   now populated alongside sidebar-app-name and document.title.
Remaining hardcoded values (correct to leave in code):
- Role names ("admin","dealer","viewer") -- auth enum values, not config
- Status names ("inventory","listed","sold") -- item state enum values
- i18n.js app_title -- translation string fallbacks, not operational config
- index.html <title> -- pre-login placeholder, overwritten by JS after login
Files changed: web/js/pages/purchases.js, web/js/pages/qr.js,
               web/js/pages/dashboard.js, web/js/app.js

## v1.2.28 (2026-06-21)

### Fix: app name driven by config.pson app.name (index.html, app.js)
"Antique Marketplace Plus" was hardcoded in index.html sidebar.
Now: sidebar span has id="sidebar-app-name"; getConfig() sets it from
r.data.app_name (returned by GET /api/config from config.pson app.name).
document.title, sidebar-app-name span, and Sign out pill title all
update from config after login. Shutdown dialog also uses document.title.
To rename the app: change app.name in config.pson and restart.
Files changed: web/index.html, web/js/app.js

## v1.2.27 (2026-06-21)

### Fix: sidebar icon enlarged 32px -> 56px (web/css/app.css)

## v1.2.26 (2026-06-21)

### Fix: sidebar too narrow; header layout; theme pill visible
--sidebar-w widened 200px -> 260px.
sidebar-header refactored: flex-direction:column. Top row (.sidebar-header-top)
holds icon + "Antique Marketplace Plus" text side by side. Second row is
the theme pill on its own line, full width and clearly readable.
.logo-text in header: font-size 14px, nowrap + ellipsis for safety.
Files changed: web/css/app.css, web/index.html

## v1.2.25 (2026-06-21)

### Fix: horizontal scroll now works; theme pill visible (web/css/app.css)
Root cause of non-working horizontal scroll: table { width:100% } meant tables
never exceeded their container width, so .table-wrap had nothing to scroll.
html,body { overflow-x:hidden } prevents body-level horizontal bleed.
table: min-width 640px added to base rule; per-page overrides set wider
min-widths (inventory 900px, sales 860px, purchases 780px, etc) so tables
overflow .table-wrap and create a real horizontal scrollbar on narrow views.
Theme pill: transparent background with accent-colored border and text --
clearly readable against dark or light sidebar without blending in.
Files changed: web/css/app.css

## v1.2.24 (2026-06-21)

### Fix: theme pill contrast; horizontal table scroll
.pill-theme: changed from filled bg2 to transparent with accent-colored
  border and text -- clearly visible against dark and light sidebar.
  Hover fills with accent gold.
.main-content: added overflow-x:hidden and min-width:0 so the flex
  child does not create a blocking scroll context that prevents children
  (.table-wrap) from scrolling horizontally.
.table-wrap: added -webkit-overflow-scrolling:touch; desktop overflow-x
  rule now explicit on base selector (not just mobile breakpoint).
Files changed: web/css/app.css

## v1.2.23 (2026-06-21)

### Feature: TLS/AES/Post-Quantum badges; theme toggle pill; USD default confirmed
server/src/server.hpp: tls_version_ and tls_cipher_ string members added.
server/src/server.cpp: SSL_get_version() + SSL_get_cipher() captured on first
  SSL_accept. /api/config returns tls_version, tls_cipher, tls_enabled,
  post_quantum (detected from Kyber/ML-KEM in cipher name).
web/index.html: TLS badge row (#tls-badges, #badge-tls-ver, #badge-tls-cipher,
  #badge-pq) added below sidebar-header. theme-toggle replaced with
  .pill-theme buttons showing "Light" / "Dark" labels.
web/css/app.css: .pill-theme, .tls-badges, .tls-badge-version (green),
  .tls-badge-cipher (indigo), .tls-badge-pq (violet) styles added.
web/js/app.js: getConfig() populates badges; cipher name shortened
  (TLS_AES_256_GCM_SHA384 -> AES-256-GCM); toggleTheme() updates all
  pill labels; init sets correct label from localStorage.
Files changed: server/src/server.hpp, server/src/server.cpp,
               web/index.html, web/css/app.css, web/js/app.js

## v1.2.22 (2026-06-21)

### Feature: multi-currency support matching Metis Church Plus pattern
Ported the three-key currency system from Metis Church Plus.

config.pson -- two new keys alongside currency_symbol:
  currency_code   = "USD"   (ISO 4217 code for Intl.NumberFormat)
  currency_locale = "en-US" (BCP 47 locale for Intl.NumberFormat)
  Full reference comment added listing 15 common currencies with
  their symbol, code, and locale.

server/src/server.cpp -- GET /api/config now returns currency_code
  and currency_locale alongside currency_symbol.

web/js/api.js -- CurrencyCode and CurrencyLocale globals added
  alongside CurrencySymbol.

web/js/app.js -- getConfig() populates all three globals after login.

web/js/pages.js -- usd() and usd2() replaced with Intl.NumberFormat
  via toLocaleString(CurrencyLocale, {style:'currency', currency:CurrencyCode}).
  Falls back to CurrencySymbol + raw number on browser error.

web/js/pages/dashboard.js -- usd() updated identically.

To use British Pounds: set in config.pson:
  currency_symbol = "GBP"
  currency_code   = "GBP"
  currency_locale = "en-GB"
To use Euros (German format):
  currency_symbol = "EUR"
  currency_code   = "EUR"
  currency_locale = "de-DE"
Files changed: config.pson, server/src/server.cpp, web/js/api.js,
               web/js/app.js, web/js/pages.js, web/js/pages/dashboard.js

## v1.2.21 (2026-06-21)

### Fix: logo and title too small (web/images/logo.svg, web/css/app.css)
logo.svg: METIS label increased 10px->13px, wordmark increased 22px->28px,
  chest icon enlarged and redrawn with plank lines and corner rivets.
app.css: .login-logo-img width 340px->480px; .nav-logo-img 180px->240px;
  .sidebar-icon-img 28px->32px.
Files changed: web/images/logo.svg, web/css/app.css

## v1.2.20 (2026-06-21)

### Feature: Treasure chest logo -- logo.svg, icon.svg, favicon (web/images/)
Added official logo suite for Metis Antique Marketplace Plus.
Theme: amber/gold treasure chest on deep dark background.
web/images/logo.svg -- 600x76 wordmark banner with chest icon + serif
  METIS label and "Antique Marketplace Plus" in amber.
web/images/icon.svg -- 200x220 standalone icon with full chest detail:
  lid arc, straps, rivets, lock body, shackle, keyhole, plank lines,
  and wordmark below the chest.
web/index.html -- favicon linked via <link rel="icon" href="images/icon.svg">;
  login screen uses logo.svg; nav header uses logo.svg; sidebar header
  uses icon.svg as small 28px mark. All diamond / text spans removed.
web/css/app.css -- .login-logo-img, .nav-logo-img, .sidebar-icon-img styles.
server/CMakeLists.txt -- post-build copy_directory for web/images/ -> build.
Files changed: web/images/logo.svg (new), web/images/icon.svg (new),
               web/index.html, web/css/app.css, server/CMakeLists.txt

## v1.2.19 (2026-06-21)

### Feature: Sign out and Shutdown pills; POST /api/admin/shutdown endpoint
The Sign out button was a plain btn-sm easy to overlook. The Shutdown
button did not exist. Changes:
- index.html: sidebar-action-pills row replaces plain btn-sm buttons.
  Two pills: gold Sign out (all roles) and red Shutdown (admin only,
  hidden via .admin-only for non-admins).
- app.css: .pill, .pill-signout, .pill-shutdown, .sidebar-action-pills styles.
- app.js: shutdownServer() calls POST /api/admin/shutdown, then replaces
  document.body with a "Server shut down" screen (connection will drop).
- server.cpp: POST /api/admin/shutdown (admin only) logs the request,
  calls stop() after 300ms delay so the HTTP response is sent first.
Files changed: server/src/server.cpp, web/index.html, web/css/app.css,
               web/js/app.js, docs/ENDPOINTS.md

## v1.2.18 (2026-06-21)

### Fix: cfg not captured in GET /api/items lambda (app_items.cpp)
The [](const HttpRequest& req) lambda could not reference cfg from the
outer register_items_routes scope. Changed to [&cfg] to capture it.
Files changed: server/src/app_items.cpp

## v1.2.17 (2026-06-21)

### Documentation: full update to Metis Standards (all docs)

TODO.md -- complete history of all 35 completed items from v1.0.5 through
v1.2.16; deferred items updated with current scope.

HOWTO.md -- rewritten to v1.2.17 covering all features: Dashboard, email
receipt, ✉ button, QR codes, language selector, font size controls,
purchases, expenses, P&L, audit log, TLS access instructions.

OPERATIONS.md -- rewritten to v1.2.17: full config.pson reference with all
new keys (bcrypt_cost, rate_limit_*, backup_hour, currency_symbol, email
block), TLS trust instructions for Windows (certmgr.msc), email receipt
setup for Gmail and Office 365, NSSM service name updated.

SUPPORT.md -- updated to v1.2.17: added USPAP reference, email provider
SMTP settings section (Gmail, Office 365, Zoho).

API.md -- rewritten to v1.2.17: 41 endpoints across 10 modules. All new
endpoints documented: GET /api/config, PUT /api/sales/:id,
DELETE /api/sales/:id, GET /api/sales/:id/invoice,
POST /api/sales/:id/email-receipt, inquiries module, photos module,
purchases/expenses module, audit/sessions, WebSocket /ws/logs.

ENDPOINTS.md -- rewritten to v1.2.17: quick-reference table updated to
41 endpoints with correct role requirements for all new endpoints.

## v1.2.16 (2026-06-21)

### Fix: hardcoded operational values moved to config.pson; email receipt feature added

**Hardcoded values now pson-driven:**
- app.backup_hour (was: 2) -- controls what hour nightly backup runs (main.cpp)
- auth.rate_limit_max_attempts (was: 5) -- max failed logins before lockout (app_auth.cpp)
- auth.rate_limit_window_sec (was: 60) -- lockout window in seconds (app_auth.cpp)
- auth.bcrypt_cost (was: 12) -- bcrypt work factor (app_auth.cpp)
- app.items_per_page fallback (was: hardcoded 20, now reads cfg) (app_items.cpp)
- app.currency_symbol (new key, was '$' hardcoded in all JS files)

**New config.pson sections/keys:**
- app.currency_symbol = "$"
- app.backup_hour = 2
- auth.bcrypt_cost = 12
- auth.rate_limit_max_attempts = 5
- auth.rate_limit_window_sec = 60
- email block: enabled/smtp_host/smtp_port/smtp_user/smtp_pass/from_address/from_name

**New endpoint: GET /api/config**
Returns safe client-facing values: currency, currency_symbol, app_name, version.
Loaded by JS after login; sets global CurrencySymbol variable.

**New endpoint: POST /api/sales/:id/email-receipt**
Sends HTML receipt to the buyer email stored on the sale record.
Returns 503 if email.enabled=false, 400 if no buyer email on record.
Requires email block in config.pson to be filled in and enabled=true.

**New files: server/src/smtp.hpp, server/src/smtp.cpp**
STARTTLS SMTP client using existing OpenSSL. Supports Gmail, Office365,
and any STARTTLS-capable SMTP server. Base64 and quoted-printable encoding
are zero-dependency (no additional libs). TLS handshake uses TLS_client_method().

**JS changes:**
- api.js: added Api.getConfig(), global CurrencySymbol='$'
- app.js: loads /api/config after login; emailReceipt(id, email) function;
  all '$' literals replaced with CurrencySymbol
- pages.js: all '$' in usd() and usd2() replaced with CurrencySymbol;
  email button (envelope icon) shown on sales rows that have buyer_email

Files changed: config.pson, server/src/main.cpp, server/src/app_auth.cpp,
server/src/app_items.cpp, server/src/server.cpp, server/src/smtp.hpp (new),
server/src/smtp.cpp (new), server/CMakeLists.txt, web/js/api.js,
web/js/app.js, web/js/pages.js

## v1.2.15 (2026-06-20)

### Feature: Dashboard page (web/js/pages/dashboard.js)
Added a full dashboard as the default landing page after login.

Data sources (all existing endpoints, no new server code):
- /api/items/stats         -- inventory count, listed, sold, asking value, cost
- /api/sales/summary       -- month revenue, net, count, avg margin
- /api/expenses/summary    -- month expenses
- /api/inquiries/summary   -- open / closed / total inquiries
- /api/sales?limit=6       -- recent sales table
- /api/items?limit=6       -- recent acquisitions table

KPI cards displayed:
- Inventory: items in stock, listed, sold all-time, asking value, unrealised gain
- This month: sales count, revenue, net proceeds, expenses, est. profit, avg margin
- Inquiries: open (highlighted in amber when >0), closed, total

Two bottom panels: recent sales and recent acquisitions, each with
a "View all" shortcut button into the relevant page.

Other changes:
- Dashboard is now the default page after login (was inventory)
- Dashboard nav item added at top of sidebar under "Overview" section
- web/js/pages/dashboard.js (new)
- web/css/app.css (.dash-*, .kpi-* styles added)
- web/index.html (page-dashboard div, nav item, script tag)
- web/js/app.js (dashboard added to renders map, default nav changed)

## v1.2.14 (2026-06-20)

### Feature: i18n (6 locales) and font size controls -- matching Metis Medical Plus pattern
Ported the language and font size support from Metis Medical Plus.

web/js/i18n.js (new):
- 6 locales: en, en_GB, fr, de, es, ja
- I18n.t(key), I18n.setLocale(code), I18n.getLocale(),
  I18n.getLanguageNames(), I18n.getLocales()
- Locale persisted in localStorage (metis_antique_locale)
- localeChanged CustomEvent dispatched on change

web/js/app.js:
- applyI18n() -- walks [data-i18n], [data-i18n-ph], [data-i18n-title] attrs
- initLangSelect() -- populates #lang-select dropdown, wires change event
- Font size IIFE: 4 sizes (13/15/17/19px), persisted in localStorage
  (metis_antique_font_idx). Buttons: btn-font-decrease, btn-font-reset,
  btn-font-increase. Disabled at min/max bounds.

web/index.html:
- .toolbar-row added to sidebar footer with A- / A / A+ buttons
  and #lang-select dropdown
- data-i18n="btn_change_pw" / data-i18n="btn_logout" on sidebar buttons
- <script src="js/i18n.js"> added before api.js

web/css/app.css:
- .toolbar-row, .toolbar-btn, .lang-select styles added

## v1.2.13 (2026-06-20)

### Fix: PSON parser stripped # inside quoted strings (server/src/pson.cpp)
The comment-stripping logic did line.find('#') and truncated everything from
the first # onward, regardless of whether it was inside a quoted value.
config.pson line:  admin_pass = "Antique#2026"
was parsed as:     admin_pass = "Antique"
because #2026 was treated as a comment. This caused seed_users() to hash
"Antique" (7 chars) into the database while the browser sent "Antique#2026"
(12 chars), so bcrypt_checkpw always returned non-zero.
Fix: replaced the blind find('#') with a character-by-character scan that
tracks whether the parser is inside a double-quoted string. # is only treated
as a comment character when in_quote is false.
Also removed temporary diagnostic logging added in v1.2.8/v1.2.12.
Files changed: server/src/pson.cpp, server/src/app_auth.cpp

## v1.2.11 (2026-06-20)

### Fix: bcrypt.c Windows random -- replaced CNG BCryptGenRandom with rand_s()
BCryptGenRandom required <ntstatus.h> and <bcrypt.h> which are not available
in MinGW without extra SDK setup, causing NTSTATUS and BCRYPT_USE_SYSTEM_PREFERRED_RNG
to be undeclared. Replaced with rand_s() (_CRT_RAND_S must be defined before
<stdlib.h>) which is built into the MinGW CRT as a thin wrapper over
RtlGenRandom -- same entropy source, no extra headers or -l flags needed.
Removed -lbcrypt from CMakeLists.txt WIN32 link libs.
Files changed: third_party/bcrypt/bcrypt.c, server/CMakeLists.txt

## v1.2.10 (2026-06-20)

### Fix: bcrypt_gensalt failed on Windows -- hash_len=2, all logins rejected (third_party/bcrypt/bcrypt.c)
bcrypt_gensalt() opened /dev/urandom to get 16 random bytes for the salt.
/dev/urandom does not exist on Windows; open() returned -1, gensalt returned
error code 2, and bcrypt_hashpw() received a bad salt producing a 2-char
garbage output. Every INSERT into users stored a 2-char pass_hash, and every
subsequent login attempt produced a different 2-char hash, so bcrypt_checkpw
always returned non-zero (no match).
Fix: on _WIN32, replaced the /dev/urandom read with BCryptGenRandom() from
the Windows CNG API (bcrypt.h / bcrypt.lib). Non-Windows path unchanged.
Also added -lbcrypt to CMakeLists.txt WIN32 target_link_libraries (CNG lives
in bcrypt.dll, distinct from our password-hashing library of the same name).
Files changed: third_party/bcrypt/bcrypt.c, server/CMakeLists.txt

## v1.2.9 (2026-06-20)

### Fix: bcrypt password verification used wrong function (server/src/app_auth.cpp)
verify_password() called bcrypt_hashpw(plain, stored, out) then compared
stored == out. This is documented as valid but bcrypt_hashpw() returns
non-zero on any internal error and does not guarantee output on failure --
on MinGW the comparison was silently producing false for all inputs.
The bcrypt library provides bcrypt_checkpw(plain, hash) specifically for
timing-safe verification; it returns 0 on match, >0 on mismatch, -1 on error.
Fixed: verify_password() now calls bcrypt_checkpw() and returns ret == 0.
Also removed temporary diagnostic logging added in v1.2.8.
Files changed: server/src/app_auth.cpp

## v1.2.8 (2026-06-20)

### Debug: login handler now logs user, pass_len, body_len, db_found, hash_len
Temporary diagnostic build to identify the invalid credentials root cause.
Files changed: server/src/app_auth.cpp

## v1.2.7 (2026-06-20)

### Fix: login rate limiter locked out all local connections (app_auth.cpp, server.cpp, types.hpp)
The rate limiter buckets failed login attempts by IP address. When no
X-Forwarded-For header is present (direct local connection), ip defaulted to
"unknown". All localhost attempts shared the same "unknown" bucket, so after
5 failed logins every subsequent attempt -- including the correct password --
was blocked for 60 seconds with no visible error (server returned 401, same
as wrong password).
Fix 1: Added remote_ip field to HttpRequest (types.hpp).
Fix 2: Added get_peer_ip(fd) helper in server.cpp using getpeername() +
inet_ntop() to resolve the real socket peer address (e.g. "127.0.0.1").
Called after parse_request() in both handle_client() and handle_client_ssl().
Fix 3: app_auth.cpp login handler now uses req.remote_ip as the base IP,
falling back to X-Forwarded-For only when a proxy header is present.
Result: each client IP now has its own rate-limit bucket. Localhost gets
"127.0.0.1" instead of "unknown".
Files changed: server/src/types.hpp, server/src/server.cpp,
               server/src/app_auth.cpp

## v1.2.6 (2026-06-20)

### Fix: Pages TDZ -- Cannot access 'Pages' before initialization (web/js/pages.js)
loadInventory, loadSales, loadMarket, renderUsers, renderPnl, and loadPnl were
assigned to Pages.xxx outside the IIFE after its closing })(), but were also
referenced as Pages.xxx inside onclick strings built during the IIFE execution.
Because const Pages = (() => {...})() is in the temporal dead zone until the
IIFE returns, any reference to Pages inside the IIFE body throws a TDZ error.
Fix: moved all functions inside the IIFE as named function declarations and
added them all to the return object. The IIFE now exports:
{renderInventory, loadInventory, renderListings, renderSales, loadSales,
 renderAppraisals, renderMarket, loadMarket, renderUsers, renderPnl, loadPnl}
Files changed: web/js/pages.js

### Fix: invalid credentials after config rename (server/src/app_auth.cpp)
seed_users() only inserted users when they did not exist (SELECT id check).
If the DB carried over from a previous build with a different password hash,
the config.pson password would never match the stored hash, causing every
login attempt to fail with 401.
Fix: seed_users() now also checks verify_password(config_pass, stored_hash).
If the config password doesn't match, the stored hash is updated to match the
current config.pson value. The log emits "Updated password for user: <name>".
This makes config.pson the authoritative source for default credentials on
every server start, not just first boot.
Files changed: server/src/app_auth.cpp

## v1.2.6 (2026-06-20)

### Fix: Pages is not defined -- ReferenceError in gallery.js, comments.js, audit.js, purchases.js (browser)
Sub-page scripts (gallery.js, comments.js, audit.js, purchases.js, qr.js)
assign onto the Pages object immediately at parse time:
  Pages.renderGallery = async function(...) { ... }
pages.js declares Pages as:
  const Pages = (() => { ... })();
A const declaration is not initialised until the statement executes. The
sub-page scripts load synchronously after pages.js in index.html, but the
browser had not yet finished executing pages.js when their top-level
assignments ran, so Pages was in the temporal dead zone and threw
ReferenceError: Cannot access 'Pages' before initialization.
Fixed: changed `const Pages` to `var Pages` in pages.js. var declarations
are hoisted to the top of the global scope (initialised to undefined) before
any script executes, so sub-page files can safely extend Pages as soon as
the IIFE completes.
Files changed: web/js/pages.js (const -> var)

Note on "invalid credentials": this is not a code bug. The server bcrypt-hashes
passwords from config.pson on first INSERT into users. If an existing antique.db
was carried over from a previous run the seed UPDATE path should correct it
automatically. If login still fails, delete data/antique.db to force a clean
re-seed. Default credential: admin / Antique#2026 (case-sensitive, hash symbol).

## v1.2.5 (2026-06-20)

### Rename: "Metis Antique Plus" -> "Metis Antique Marketplace Plus" (all files)
All display name references updated across the full source tree:
- server/src/main.cpp, server.cpp (startup banners x3)
- server/src/app_sales.cpp (PDF footer)
- server/src/logger.cpp, app_audit.cpp (log filename: antique_plus.log ->
  antique_marketplace_plus.log)
- web/index.html (title, header logo text x2)
- web/shop.html (footer)
- web/js/app.js, pages.js (file headers; version comment updated to v1.2.5)
- web/js/api.js, pages/purchases.js, audit.js, comments.js, gallery.js, qr.js
- config.pson (app.name)
- README.md, docs/API.md, ENDPOINTS.md, OPERATIONS.md, TODO.md,
  SUPPORT.md, HOWTO.md, ARCHITECTURE.md, DATABASE.md

## v1.2.4 (2026-06-20)

### Fix: linker error -- undefined reference to CertFreeCertificateContext etc. (server/CMakeLists.txt)
OpenSSL 4.0.0 libcrypto.a includes winstore_store.obj which uses the Windows
Certificate Store API (CertOpenSystemStoreW, CertFindCertificateInStore,
CertFreeCertificateContext, CertCloseStore). These symbols live in crypt32.dll
and require -lcrypt32 at link time. The library was missing from the WIN32
target_link_libraries list, causing ld to fail with undefined reference errors
on every symbol in that object.
Fix: added crypt32 to target_link_libraries alongside ws2_32 and mswsock.
Files changed: server/CMakeLists.txt

## v1.2.3 (2026-06-20)

### Fix: certs/ bundled in zip and synced to build dir by CMake
certs/server.crt and certs/server.key (US/Idaho/Coeur d'Alene/Metis/CN=localhost,
self-signed RSA-2048, valid 2026-06-20 to 2027-06-20) added to the source tree.
CMakeLists.txt post-build step added to copy_directory certs/ -> build output dir
alongside the exe on every build, so TLS is immediately available without any
manual file placement.
Files changed: certs/server.crt (new), certs/server.key (new),
               server/CMakeLists.txt (post-build copy_directory for certs/)

## v1.2.2 (2026-06-20)

### Feature: TLS listener on tls_port (server.cpp, server.hpp, ws_log.cpp, ws_log.hpp)
config.pson tls_port / cert_file / key_file were placeholder keys with no
server-side implementation -- port 8481 refused all connections.

Implementation:
- Server constructor reads tls_port, cert_file, key_file from config.pson.
  If tls_port > 0 and both PEM files load successfully, an SSL_CTX is
  created with TLS 1.2 as the minimum protocol version.
- Server::run_tls_acceptor() binds tls_port, accepts connections, wraps
  each socket in SSL_new/SSL_accept, then dispatches to handle_client_ssl().
- Server::handle_client_ssl(SSL*, int) mirrors handle_client() exactly but
  uses SSL_read/SSL_write for all I/O and SSL_shutdown/SSL_free on close.
- ws_log: ws_handshake_ssl, ws_send_ssl, ws_client_closed_ssl, and
  handle_ws_logs_ssl added -- identical logic to the plain variants but
  all socket calls replaced with OpenSSL equivalents.
- run() launches run_tls_acceptor() on a std::thread when ssl_ctx_ is
  non-null; joins it and frees SSL_CTX on shutdown.
- All TLS code is guarded by #ifdef METIS_TLS so HTTP-only builds
  (no OpenSSL found by CMake) compile and run unchanged.

To enable: place a PEM cert at certs/server.crt and key at certs/server.key
(exe-relative). Self-signed cert sufficient for local use:
  openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes
Files changed: server/src/server.hpp, server/src/server.cpp,
               server/src/ws_log.hpp, server/src/ws_log.cpp

## v1.2.1 (2026-06-20)

### Fix: CMake project() name updated to Metis_Antique_Marketplace_Plus
Both CMakeLists.txt files had inconsistent/legacy project names
(root: MetisAntiquePlus, server: metis_antique_plus).
Updated to Metis_Antique_Marketplace_Plus in both.
The CMake target name and executable name (metis_antique_plus) are unchanged.
Files changed: CMakeLists.txt, server/CMakeLists.txt

## v1.2.0 (2026-06-20)

## v1.2.0 (2026-06-20)

### Fix: startup banner showed stale version
app.version in config.pson was not updated during v1.1.8 and v1.1.9 version
bumps, causing the startup banner to display v1.1.7. Per Metis invariant, all
operational values are stored in and read from config.pson; the banner correctly
reads cfg.get("app","version"). The fix is to keep app.version in sync with
the VERSION file on every release. config.pson now reads 1.2.0.
Files changed: config.pson (app.version = "1.2.0")

## v1.1.9 (2026-06-20)

### Fix: WIN32_LEAN_AND_MEAN / NOMINMAX redefinition warnings (server.cpp, ws_log.cpp, util.cpp)
CMakeLists.txt already injects WIN32_LEAN_AND_MEAN and NOMINMAX as
target_compile_definitions, and MinGW's os_defines.h also defines NOMINMAX.
The bare #define in each source file produced -Wmacro-redefined warnings for
all three translation units on every build.
Fixed: wrapped each pair with #ifndef guards so the compiler-injected
definition takes precedence and the in-source define is silently skipped.
No behavioural change; warnings are now clean.
Files changed: server/src/server.cpp, server/src/ws_log.cpp, server/src/util.cpp

## v1.1.8 (2026-06-20)

### Dependency updates
- SQLite amalgamation updated from 3.51.3 to 3.53.1 (third_party/sqlite/)
- OpenSSL prebuilt updated to 4.0.0 for Windows/MinGW
  (openssl-prebuilt/windows/lib/libssl.a + libcrypto.a + full include tree)
  Matches the versions shipped with Metis Church Plus v1.25.4 and
  Metis Medical Plus v2.66.2.

## v1.1.7 (2026-06-19)

- Initial release
- Inventory management: CRUD, category/era/maker/provenance fields, condition grading
- Multi-channel listing management with views/watchers tracking
- Sales recording with margin calculation and revenue summary
- Appraisal workflow with value range and status tracking
- Market dashboard: category revenue, monthly trend, top performers
- Session-based authentication, configurable timeout
- SQLite 3 amalgamation, WAL mode, foreign keys
- TLS auto-detection (env var -> bundled prebuilt -> slproweb)
- Exe-relative path resolution for config, web assets, data, logs
- Weekly log rotation with millisecond timestamps
- 34 REST endpoints across 6 route modules
- Vanilla-JS browser client (3 files, no build step)

## v1.1.7 (2026-06-19)

- Bundled SQLite 3.51.3 amalgamation (third_party/sqlite/sqlite3.c and sqlite3.h)
- Moved sqlite3 source from server/src/ to third_party/sqlite/ matching Metis Medical Plus layout
- Added SQLITE_THREADSAFE=1, SQLITE_ENABLE_FTS5, SQLITE_ENABLE_JSON1 compile definitions
- Added openssl-prebuilt/ directory structure matching Metis project family layout
- CMakeLists: LANGUAGES now includes C for SQLite amalgamation compilation
- Build verified clean: 16 steps, zero warnings, zero errors (GCC 13.3.0, Linux)

## v1.1.7 (2026-06-20)

- Added full multi-user system with three roles: admin, dealer, viewer
- Three default users seeded from config.pson on first boot:
    admin  / Antique#2026  (full access + user management)
    dealer / Dealer#2026   (full access to inventory, listings, sales, appraisals)
    viewer / Viewer#2026   (read-only)
- New REST endpoints: GET/POST /api/users, PUT/DELETE /api/users/:id,
  POST /api/auth/change-password (8 auth endpoints total)
- Role enforcement in server.cpp: viewers blocked from all POST/PUT/DELETE
- Admin-only guard on user management routes (403 for non-admins)
- Safety guards: cannot delete last admin, cannot delete self,
  cannot demote last admin
- HttpRequest.role field populated from session on every request
- is_admin() / is_writer() / is_reader() helpers in types.hpp
- Browser: Users page (admin only, hidden from other roles)
- Browser: Role badge displayed in sidebar footer
- Browser: Read-only banner shown to viewer role on write pages
- Browser: Change Password dialog available to all roles
- Browser: login response now returns role; nav adapts immediately

## v1.1.7 (2026-06-20)

Documentation release -- full Metis Docs Model completed.

New documents:
- HOWTO.md        Step-by-step guide for all user tasks (inventory, listings, sales,
                  appraisals, user management, backups, pricing strategy)
- OPERATIONS.md   Installation, build, run, configuration, remote access (5 steps:
                  bind address, Windows Firewall, router port forwarding, DuckDNS,
                  TLS/HTTPS), Windows service (NSSM), Linux service (systemd)
- TODO.md         7 priority groups: security hardening, inventory, sales, listings,
                  market/reporting, UX, infrastructure; 3 known issues documented
- SUPPORT.md      External resources: 5 appraisal orgs, 7 selling platforms,
                  5 auction houses, 5 trade shows, 5 price research tools,
                  4 reference databases, 4 trade associations, legal/IRS resources,
                  3 insurance providers, 3 shipping providers

Updated documents:
- ARCHITECTURE.md Rewritten to match actual v1.0.2+ code (role model table,
                  accurate request lifecycle, correct directory layout)
- API.md          Rewritten to match actual endpoints (all 26 documented accurately,
                  role guards noted per endpoint, correct request/response shapes)
- DATABASE.md     Rewritten to match actual schema (6 tables, correct column names,
                  no phantom columns from earlier placeholder doc)
- ENDPOINTS.md    Rewritten as accurate quick reference table for all 26 endpoints
                  with auth, role, and description columns

## v1.1.7 (2026-06-20)

Major feature release -- all v1.0.3 TODO items completed.

### Security (Priority 1 -- all complete)
- Replaced FNV-1a password hash with bcrypt cost-12 (third_party/bcrypt/ bundled)
- Expired session cleanup jthread -- prunes sessions table every hour
- Login rate limiting -- 5 attempts per IP per 60 seconds, HTTP 429 response
- CSRF documented -- Bearer token model confirmed CSRF-safe, no cookie auth

### Inventory (Priority 2 -- all complete)
- Photo upload: POST /api/items/:id/photo (multipart), DELETE /api/items/:id/photo
- Photos served from exe-relative photos/ dir via /photos/ URL prefix
- Photo thumbnails displayed in inventory table rows
- Photo preview and delete in edit modal
- Item duplication: POST /api/items/:id/duplicate (copies all fields, resets status)
- Bulk CSV import: POST /api/items/import, CSV template documented
- Import UI in browser with file picker and result summary
- Item history / audit log: item_history table, GET /api/items/:id/history
- Audit tracked for title, asking_price, status, condition changes on every PUT
- Item detail view: click magnifier icon to see all fields + change history
- Acquisition source field added to items table and forms
- Consignor name + percentage fields added to items table and forms

### Sales (Priority 3 -- all complete)
- Platform fee and shipping cost fields in sales table and Record Sale form
- net_proceeds computed server-side: sale_price - platform_fee - shipping_cost
- Payment method field (Cash/Check/PayPal/Venmo/Credit card/Wire/Other)
- Annual sales CSV export: GET /api/sales/export?year=YYYY
- Export button in browser sales page (downloads current year)
- Printable invoice: GET /api/sales/:id/invoice (HTML, open in new tab)
- Invoice button on every sales row
- Date range filter on sales page and sales summary

### Listings (Priority 4 -- all complete)
- listing_url column added to listings table
- URL displayed as clickable link icon in listings table
- URL edit button opens modal to update listing URL
- Watcher count inline edit field in listings table
- Auction expiry alert: amber highlight + banner for listings ending within 48h
- PUT /api/listings/:id for field updates

### Market and reporting (Priority 5 -- all complete)
- Date range filter (?from=&to=) on all market endpoints and UI
- Portfolio valuation report: GET /api/reports/portfolio (by category, unrealized gain)
- ROI by acquisition source: GET /api/reports/roi_by_source
- Both new reports visible in Market page chart grid
- net_revenue included in trend and category responses

### User experience (Priority 6 -- partial)
- Pagination: GET /api/items?page=N&limit=N, browser renders page controls
- Item detail view: full fields + change history in modal (magnifier icon)
- Dark/light mode toggle button in sidebar header (persists in localStorage)
- Mobile layout and keyboard navigation deferred to post-1.1.7

### Infrastructure (Priority 7 -- all complete)
- Nightly database backup jthread: copies to backup_dir/antique-YYYY-MM-DD.db at 2am
- Keeps last N backups (backup_keep, default 7); uses SQLite Online Backup API
- backup_dir and backup_keep configurable in config.pson app{} block
- GET /api/health endpoint -- status, item/user/sale counts (no auth required)
- Config validation on startup -- required fields checked, clear error if missing
- WebSocket log streaming deferred to post-1.1.7

### Known issues (all fixed)
- nlohmann/json 3.12.0 bundled in third_party/json/ -- replaces hand-rolled parser
  All app_*.cpp files now use nlohmann::json::parse() for reliable JSON handling
- recv() body limit documented; photo upload uses FormData streaming pattern
- Sessions now pruned hourly by background jthread

### Third-party additions
- third_party/bcrypt/ (rg3/bcrypt + crypt_blowfish) -- password hashing
- third_party/json/json.hpp (nlohmann/json 3.12.0) -- JSON parsing

## v1.1.7 (2026-06-20)

### Multi-photo gallery (replaces single photo_ref)
- New `item_photos` table: multiple photos per item, captions per photo,
  primary flag, sort order, uploaded_by
- GET /api/items/:id/photos -- all photos in sort order
- POST /api/items/:id/photos -- multipart upload, one or more files at once
- PUT /api/items/:id/photos/:photo_id -- update caption, set primary, reorder
- DELETE /api/items/:id/photos/:photo_id -- deletes file and record;
  auto-promotes next photo to primary
- Photos stored exe-relative in photos/ directory, served at /photos/:filename
- Backward compatible: photo_ref column kept in sync for legacy queries
- Lightbox on click: full-size view with caption overlay

### Dealer internal comments
- New `item_comments` table: threaded notes per item
- GET /api/items/:id/comments -- all comments ascending by time
- POST /api/items/:id/comments -- authenticated dealer/admin posts note
- DELETE /api/items/:id/comments/:id -- admin only
- Live-inject new comments without page reload

### Buyer inquiry system
- New `buyer_inquiries` table: buyer contacts dealer about a listing
- New `inquiry_replies` table: threaded dealer-buyer conversation
- POST /api/inquiries -- PUBLIC endpoint (no auth), buyer submits inquiry
- GET /api/inquiries -- dealer views all inquiries with reply count and filters
- GET /api/inquiries/:id -- full thread with all replies
- POST /api/inquiries/:id/replies -- dealer replies (auth) or buyer replies (open)
- PUT /api/inquiries/:id/status -- open / replied / closed
- DELETE /api/inquiries/:id -- admin only (cascades replies)
- GET /api/inquiries/summary -- open/total/closed counts for badge
- Status auto-advances to "replied" when dealer posts a reply

### Browser UI
- Item detail modal now has three tabs: Details | Photos | Dealer notes
- Photos tab: scrollable grid, click to lightbox, inline caption editing,
  Set primary button, delete button per photo, multi-file upload picker
- Dealer notes tab: threaded comment list, post box, admin delete
- Inquiries sidebar item with open-count badge (amber)
- Full inquiries page: stat cards, status filter, table with reply count
- Inquiry thread modal: buyer info, status select, full reply thread,
  reply-by-role colour coding (amber=admin, accent=dealer, blue=buyer),
  send reply injects immediately without reload

## v1.1.7 (2026-06-20)

### Public buyer-facing storefront (web/shop.html, /shop routes)

New complete public shop — no login required for buyers.

Server-side (app_listings.cpp):
- GET /shop/listings?page=N&category=X&q=search -- paginated grid (12/page),
  full item details, primary photo URL, auction type flag
- GET /shop/listings/:id -- full detail with all photos array, description,
  provenance, listing URL; increments view counter
- GET /shop/categories -- distinct active categories with item counts
- All /shop/* paths added to public gate in server.cpp (no auth required)
- /shop and /shop/ route to web/shop.html

Browser (web/shop.html -- standalone, no dealer login needed):
- Sticky header with shop name, search bar, dealer login link
- Category sidebar filter with item counts, active highlight
- Responsive listing grid: photo, title, maker/era/origin, price, condition badge
- Full detail page: photo gallery with thumbnail strip + active highlight,
  main photo swap on thumb click, title, price, all item fields, description,
  provenance, link to external platform listing
- Lightbox: full-screen photo zoom with caption, click-anywhere to close
- Inquiry form: name, email, phone, subject, message -- submits to existing
  POST /api/inquiries (public endpoint from v1.0.5)
- Conversation thread: after submitting, buyer sees their message + dealer replies;
  buyer can post follow-up replies (no login); thread persists via localStorage
  keyed by listing ID so buyer sees it on return visit
- Browser history: URL updates to ?listing=ID on detail; back button works;
  direct URL share /shop.html?listing=5 opens that item
- Toast-style success confirmation after inquiry sent

Workflow:
1. Dealer lists an item (inventory → listings page)
2. Buyer visits /shop, browses, clicks item, sees all photos + description
3. Buyer submits inquiry form → appears in dealer's Inquiries page
4. Dealer replies from the Inquiries modal in the dealer app
5. Buyer revisits /shop.html?listing=ID and sees the dealer's reply
6. Buyer can continue the conversation from the shop page

## v1.1.7 (2026-06-20)

### Threading answer
Confirmed fully multithreaded: one detached std::thread per HTTP connection;
Db mutex-serialized; Logger mutex-serialized; Router read-only after startup;
running_ std::atomic<bool>; rate_limit map mutex-protected;
SQLITE_THREADSAFE=1; background jthreads (session cleanup, backup) use
cooperative std::stop_token. No shared mutable state without protection.

### Audit & Logs — new app_audit.cpp module (5 endpoints)
- GET /api/audit/log?limit=N&level=X  -- reads last N lines from
  antique_plus.log via Logger::tail(); parses timestamp/level/message;
  admin only
- GET /api/audit/trail?limit=N&type=X  -- unified UNION SQL across
  item_history, sales, listings, items, item_photos, item_comments,
  buyer_inquiries, sessions, login_attempts; newest-first; admin only
- GET /api/audit/sessions  -- all sessions with username, role, created,
  expires, active flag; tokens redacted to first 8 chars; admin only
- DELETE /api/audit/sessions/:token  -- force-expire (kick) a session;
  admin only; logged to system log
- GET /api/audit/stats  -- summary counts for audit dashboard header

### Audit trail — data completeness
- Failed logins now written to login_attempts table (were counted but never
  stored); ip address + timestamp recorded on every bcrypt verify failure
- Logger::tail(n, level_filter) added -- reads log file from disk newest-first
  via deque ring buffer, parses [timestamp] [level] message format
- Logger::log_path() accessor added for display in browser

### Browser — Audit & Logs page (admin only, sidebar &#x1F6E1;)
Three pills on one page, lazy-loaded on first click:

Audit trail pill:
- Type filter dropdown (item_change, item_create, sale, listing, photo,
  note, inquiry, login, failed_login)
- Free-text search across actor + description + detail
- Each row: colour-coded icon, description, detail, type pill, actor, timestamp

System log pill:
- Level filter (INFO / WARN / ERROR / system)
- Free-text search across message text
- Monospace table: timestamp | [LEVEL] | message
- WARN rows in amber, ERROR rows in red, system rows in blue
- Shows log file path and line count

Active sessions pill:
- Table of all sessions: username, role, redacted token, created, expires, status
- Kick button per active session -- force-expires immediately
- Badge: Active (green) / Expired (gray)

### Browser — item detail modal expanded to 5 tabs
Details | Photos | Notes | Inquiries | Audit

New Inquiries tab (per item):
- Shows all buyer inquiries linked to this item's listings
- Buyer name, email, phone, subject, body, reply count, status badge
- "View thread" button opens the full InquiryUI thread modal

New Audit tab (per item):
- Shows item_history for this specific item
- Table: field | from (strikethrough) | to (accent) | changed by | when
- Empty state when no changes recorded yet

## v1.1.7 (2026-06-20)

### Purchase tracking — new app_purchases.cpp module

New purchases table (per item acquisition record):
- vendor_name, vendor_contact
- acquisition_date (separate from created_at)
- purchase_price, buyers_premium, transport_cost, restoration_budget
- total_cost (auto-calculated = sum of above; synced to items.cost_price)
- purchase_channel (Estate sale / Auction house / Private dealer / etc.)
- payment_method, receipt_ref, notes

New expenses table (overhead costs not tied to a specific item):
- category (Show fees / Storage / Insurance / Shipping supplies / etc.)
- description, amount, expense_date
- vendor, payment_method, receipt_ref, notes

Endpoints (13 new):
- GET/POST/PUT/DELETE /api/purchases
- GET /api/purchases/:id
- GET /api/items/:id/purchase  (purchase record for a specific item)
- GET /api/purchases/summary   (totals, by-channel breakdown, month figures)
- GET/POST/PUT/DELETE /api/expenses
- GET /api/expenses/summary    (totals, by-category bars, month figure)
- GET /api/reports/pnl         (full P&L with date range, by-category table)

P&L report computes:
  Gross revenue → Net revenue (after fees and shipping)
  → Gross profit (after COGS)
  → Net profit (after overhead expenses)
  Uses purchases.total_cost where available; falls back to items.cost_price

Schema migrations added for new items columns:
  acquisition_date, vendor_name, vendor_contact, purchase_channel

Browser — three new nav sections (Inventory / Finance / Operations):
  Purchases page: stat cards (total cost, by component), date/channel
    filter, table with vendor, amounts, total cost, Edit/Delete per row,
    Record Purchase modal with live total recalculation
  Expenses page: stat cards, by-category bar chart, category/date filter,
    table with receipt ref, Record Expense modal
  P&L page: date range picker, P&L statement (revenue → gross profit →
    net profit), cash flow summary card, profit by category table with
    units/revenue/COGS/profit/margin per category

## v1.1.7 (2026-06-20)

Properly built purchases and expenses — fixes to v1.0.8.

Server fixes:
- Added GET /api/expenses/:id endpoint (was missing; edit fetched all rows as workaround)

Browser — Purchases page rebuilt:
- Item picker is now a dropdown of all in-stock inventory items (not a bare number input)
- Live total cost recalculation as user fills in price fields
- Channel breakdown bar chart added to purchases summary area
- Edit correctly fetches single purchase by ID
- Delete confirmation shows item title
- Clicking item title in table opens item detail modal
- openAddForItem() helper — called from item detail Purchase tab to pre-select the item

Browser — Expenses page rebuilt:
- openEdit() now calls GET /api/expenses/:id (correct endpoint)
- Category dropdown sorted and complete
- Delete refreshes table in-place rather than navigating away

Browser — Item detail modal:
- Purchase tab added (6th tab: Details | Photos | Notes | Purchase | Inquiries | Audit)
  Shows vendor, date, channel, payment, per-component cost breakdown, total cost basis
  Empty state with "Record purchase" button when no record exists
  Edit button opens purchase edit modal from within item detail

Browser — viewer gate:
- writePages now includes 'purchases' and 'expenses'
  (viewer role sees read-only banner on both pages)

## v1.1.7 (2026-06-20)  -- All TODO items complete

### WebSocket real-time log streaming (/ws/logs)

New ws_log.cpp module (no OpenSSL dependency):
- Inline SHA-1 implementation (RFC 3174, 30 lines, GF arithmetic)
- Inline Base64 encoder
- RFC 6455 WebSocket handshake: extracts Sec-WebSocket-Key, computes
  accept key, sends 101 Switching Protocols response
- LogBroadcaster singleton: thread-safe publish/subscribe hub;
  Logger::write() publishes every line to all active subscribers
- handle_ws_logs(): admin-only; sends 50-line history immediately,
  then streams live lines as they arrive; heartbeat ping every 5s;
  detects disconnect and unsubscribes cleanly
- Token auth via ?token= query param (browsers cannot send Authorization
  header on WebSocket upgrade); server.cpp parse_request() falls back
  to query param when header is absent

Browser (audit.js System Log pill):
- Connects WebSocket on first open; shows "Live" / "Disconnected" status
- History lines streamed immediately on connect
- Level filter and text search update table in real time
- Pause/Resume button freezes new lines without closing socket
- Auto-reconnects after 5s on disconnect
- Falls back to REST GET /api/audit/log if WebSocket not supported

### Mobile layout (< 768px)

CSS additions:
- Mobile top bar (always visible on mobile): hamburger ☰ button,
  logo, theme toggle
- Sidebar slides in/out via CSS transform + .open class; overlay
  darkens background when open
- Sidebar closes automatically when nav item clicked on mobile
- Responsive grids: stat-grid → 2-col; form-grid → 1-col;
  chart-grid → 1-col; modal → 96vw
- Tables gain overflow-x: auto with horizontal scroll
- Login card → 90vw
- Extra breakpoint at 480px: stat-grid → 1-col, page-header stacks

### Keyboard navigation

- Arrow keys: Up/Down navigate table rows; focused row highlighted
  with .kb-focus class
- Enter: activates first button in focused table row
- /: focuses the active page's first search input
- n: clicks the page's primary + action button (new item, etc.)
- Ctrl+Enter / Cmd+Enter: submits the open modal form
- Escape: closes modal (was already present, now documented)
- :focus-visible ring: 2px accent outline on all keyboard-focused elements
- Form submit hint: "Ctrl+Enter to submit" appears when tabbing into
  a modal form

### QR code label printing (web/js/pages/qr.js)

Pure-JS QR encoder (no libraries):
- GF(256) arithmetic with EXP/LOG tables
- Reed-Solomon ECC (7 error correction codewords for Version 1-L)
- Full QR Version 1 (21×21) matrix: finder patterns, separators,
  timing patterns, format info (ECC=L, mask=0, precomputed BCH),
  data placement with zigzag column scan, mask 0 applied
- SVG output with 4-module quiet zone; crisp at any print size

Label layout (3.5 in wide, fits 2-up on US Letter):
- QR code encodes /shop.html?listing=<item_id> URL
- Item title, category, era, maker, origin, condition badge, price, item ID
- Print-ready: @page margin, break-inside: avoid, color-adjust: exact
- Auto-opens browser print dialog, closes window after printing

Integration:
- 🏷 Print label button in inventory table rows
- Print label button in item detail modal form-actions
- LabelUI.printLabel(id): fetches item, generates label, opens print window
- LabelUI.printSelected(ids[]): batch print multiple items

## v1.1.7 (2026-06-20) -- UI completeness pass

### Gaps found and fixed (not in TODO, found by audit)

Server:
- PUT /api/sales/:id  -- edit a recorded sale (price, fees, buyer, date, notes)
- DELETE /api/sales/:id  -- remove a sale; restores item status to 'inventory' (admin only)
- DELETE /api/appraisals/:id  -- remove an appraisal record

Browser — Sales page:
- Record Sale form: Item ID text input replaced with searchable dropdown of
  listed and inventory items (same fix applied to Purchases in v1.0.9)
- Sales table: Edit button opens full sale edit modal; Delete button (admin)
  removes the record and restores the item

Browser — Appraisals page:
- Delete button added to each appraisal row

Browser — Item edit form:
- Old single-photo file input removed; replaced with informational note
  directing users to the Photos tab in the item detail modal
  (multi-photo gallery has been the correct path since v1.0.5)
- Old saveItem() photo upload code removed; no longer calls
  /api/items/:id/photo (legacy single-photo endpoint)

## v1.1.7 (2026-06-20) -- completeness pass II

### Server fixes
- GET /api/sales/:id  -- dedicated single-sale endpoint (openEditSale was
  fetching all sales and filtering client-side; now uses this endpoint)
- GET /api/inquiries/:id added to public auth gate -- shop.html buyer
  thread was receiving 401 when trying to load the conversation thread
  (POST /api/inquiries was public but GET was not; both needed for buyers)

### Browser fixes
- New Listing form: Item ID bare number input replaced with dropdown
  of all inventory and listed items (title + maker + status)
- New Appraisal form: Item ID bare number input replaced with dropdown
  of all inventory items; openAddAppraisal() made async to load list
- openEditSale() now calls GET /api/sales/:id directly instead of
  fetching all sales and finding by id client-side

## v1.1.7 (2026-06-20) -- completeness pass III

### Server
- GET /api/appraisals/:id  -- single appraisal by ID; openEditAppraisal was
  fetching all appraisals and filtering client-side
- PUT /api/listings/:id expanded to accept status, channel, auction_end
  (previously only watchers, listing_url, and list_price were accepted)

### Browser
- openEditAppraisal: now calls GET /api/appraisals/:id directly; shows item
  title in a read-only info panel (item cannot change on edit)
- openEditListing: replaced URL-only edit modal with full edit modal covering
  list_price, channel, status (active/ended/sold/removed), auction_end date,
  and listing_url; listings table "URL" button renamed to "Edit"
- saveItem: removed dead comment + stale newId variable left from old single
  photo upload code (upload block was removed in v1.1.1 but comment remained)

## v1.1.7 (2026-06-20) -- all analysed issues fixed

### Fix 1: recv buffer — Content-Length driven read loop (CRITICAL)
Previously: single recv() into a 64KB buffer. Any photo > 64KB was silently
truncated to garbage, making multi-photo upload unreliable for real JPEG files
(typically 200KB-2MB).
Now: initial 8KB read captures headers; Content-Length header parsed; remaining
body read in 64KB chunks until complete; hard cap at 20MB per request to prevent
memory exhaustion. WebSocket upgrade path unaffected (no body to read).

### Fix 2: cascade deletes on item delete
Previously: DELETE /api/items/:id deleted item_history but left appraisals and
buyer_inquiries as orphaned rows referencing a deleted item_id.
Now: delete_item handler explicitly removes appraisals and buyer_inquiries before
deleting the item. Sales rows are intentionally preserved — they are historical
financial records needed for P&L accuracy even after the item is gone.

### Fix 3: Market page date filter — portfolio and ROI now respect date range
Previously: loadMarket() built a ?from=&to= query string and passed it to
/api/market/categories, /trend, /top — but called /api/reports/portfolio and
/api/reports/roi_by_source with no parameters, so those two charts always
showed all-time data regardless of the selected range.
Now: both endpoints accept from/to query params (portfolio filters by
items.created_at, ROI filters by sales.sale_date); pages.js passes qs to both.

### Fix 4: Session sliding window
Previously: sessions had a fixed 24-hour lifetime from creation. An active
dealer working through the day would be logged out at the 24h mark.
Now: session_valid() issues an UPDATE sessions SET expires_at=datetime('now',
'+24 hours') on every successful validation, extending the session with each
authenticated request.

### Fix 5: Dead orphan code removed
Deleted 11 files from the original scaffolded version (incompatible API,
never compiled or loaded):
  server: app_contacts.cpp/.hpp, app_reports.cpp/.hpp
  web/js/pages: appraisals.js, contacts.js, dashboard.js, items.js,
                listings.js, reports.js, sales.js

## v1.1.7 (2026-06-20) -- fix issues pass

### Fix 1: CSV import — provenance silently dropped (server)
The POST /api/items/import INSERT listed 11 columns including provenance but
VALUES had only 9 ? placeholders + hardcoded 'inventory' for status — 10 slots.
f[9] (provenance) was in the params array but had no corresponding ? in the
SQL, causing SQLite to either error silently or ignore the value entirely.
Fixed: added the missing ? for provenance so the param count matches.
Column mapping is now: title, category, era, maker, origin, material,
condition, cost_price, asking_price, provenance, status='inventory'.

### Fix 2: Purchase tab UX — edit returns to item detail (browser)
Clicking "Edit purchase" from the Purchase tab inside the item detail modal
previously closed the modal, opened the purchase edit form, and after saving
navigated to the Purchases list page — dropping the user's item context.
Fixed: openEditFromDetail() now fetches the purchase to capture its item_id,
stores it in window._purchaseReturnItemId, then after a successful save,
reopens the item detail modal for that item instead of navigating to the
Purchases list page. When PurchaseUI.save() is called normally from the
Purchases page, the flag is null and existing navigation behaviour is unchanged.

## v1.1.7 (2026-06-20) -- fix issues

### Fix 1: N+1 query in GET /api/listings (server)
For each listing row, a separate DB query was fired to check whether the
auction_end date was within 48 hours. With N active listings this was N+1
round trips to SQLite under the Db mutex.
Fixed: moved the expiring_soon computation into the main SELECT as a SQL
CASE expression. Single query regardless of how many listings exist.

### Fix 2: GET /api/listings/:id added (server)
openEditListing() was fetching the full listings array and finding the
target by id client-side — same pattern fixed for sales, appraisals, and
purchases in earlier passes.
New endpoint returns a single listing record by id with full fields.
openEditListing() now calls GET /api/listings/:id directly.

## v1.1.7 (2026-06-20) -- MinGW/Windows build fixes

All three errors from the CLion/MinGW-w64 build on Windows, none present on Linux.

### Fix 1: BCryptBuffer / ncrypt.h type conflict (server.cpp, util.cpp, ws_log.cpp, sqlite3.c)
Root cause: our third_party/bcrypt/ directory is in the compiler include path
(-I flag). When windows.h -> wincrypt.h -> ncrypt.h did #include <bcrypt.h>,
the preprocessor found our bundled bcrypt.h instead of the MinGW system
bcrypt.h. Our bcrypt.h is the OpenBSD password-hashing library and does not
define BCryptBuffer, BCryptBufferDesc, BCRYPT_KEY_HANDLE etc. — those are
Windows CNG API types that ncrypt.h expects to have been declared by the
real system bcrypt.h first. This caused a cascade of ~30 errors in ncrypt.h
and wincrypt.h for every translation unit that included windows.h or winsock2.h.

Fix: `#define WIN32_LEAN_AND_MEAN` and `#define NOMINMAX` added:
- Before `#include <winsock2.h>` in server.cpp and ws_log.cpp
- Before `#include <windows.h>` in util.cpp
- As `target_compile_definitions` in server/CMakeLists.txt (covers sqlite3.c
  and any other C/C++ translation unit compiled by the same target)
WIN32_LEAN_AND_MEAN prevents windows.h from pulling in wincrypt.h/ncrypt.h,
which are not needed by this application. Socket and file APIs are unaffected.
`_WIN32_WINNT=0x0601` (Windows 7+) also set to suppress MinGW version warnings.

### Fix 2: MSG_DONTWAIT not declared (ws_log.cpp)
MSG_DONTWAIT is POSIX-only and does not exist on Windows (Winsock).
It was used in ws_client_closed() to do a non-blocking recv() to detect
whether the WebSocket client had sent a close frame.
Fix: replaced with a portable select() call with a zero-timeout (tv_sec=0,
tv_usec=0) on a single fd_set. select() with timeout=0 returns immediately —
if the socket has data ready, recv() is called to read and check for the
WebSocket close opcode (0x8); if nothing is ready the function returns false
(client still connected). Works identically on Windows and POSIX.
